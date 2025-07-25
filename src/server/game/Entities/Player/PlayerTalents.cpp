/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "Player.h"
#include "AccountMgr.h"
#include "AchievementMgr.h"
#include "ArenaTeam.h"
#include "ArenaTeamMgr.h"
#include "Bag.h"
#include "Battlefield.h"
#include "BattlefieldMgr.h"
#include "Battleground.h"
#include "BattlegroundMgr.h"
#include "BattlegroundScore.h"
#include "CellImpl.h"
#include "Channel.h"
#include "ChannelMgr.h"
#include "CharacterCache.h"
#include "CharacterDatabaseCleaner.h"
#include "Chat.h"
#include "CinematicMgr.h"
#include "CombatLogPackets.h"
#include "CombatPackets.h"
#include "Common.h"
#include "ConditionMgr.h"
#include "Containers.h"
#include "CreatureAI.h"
#include "DBCEnums.h"
#include "DatabaseEnv.h"
#include "DisableMgr.h"
#include "Formulas.h"
#include "GameClient.h"
#include "GameEventMgr.h"
#include "GameObjectAI.h"
#include "GameTime.h"
#include "GitRevision.h"
#include "GossipDef.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "GuardMgr.h"
#include "Group.h"
#include "GroupMgr.h"
#include "Guild.h"
#include "GuildMgr.h"
#include "InstanceSaveMgr.h"
#include "InstanceScript.h"
#include "Item.h"
#include "KillRewarder.h"
#include "Language.h"
#include "LFGMgr.h"
#include "Log.h"
#include "LootItemStorage.h"
#include "LootMgr.h"
#include "Mail.h"
#include "MailPackets.h"
#include "MapManager.h"
#include "MiscPackets.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Opcodes.h"
#include "OutdoorPvP.h"
#include "OutdoorPvPMgr.h"
#include "Pet.h"
#include "PetitionMgr.h"
#include "PoolMgr.h"
#include "QueryHolder.h"
#include "QuestDef.h"
#include "QuestPools.h"
#include "Realm.h"
#include "ReputationMgr.h"
#include "SharedDefines.h"
#include "SkillDiscovery.h"
#include "SocialMgr.h"
#include "Spell.h"
#include "SpellAuraEffects.h"
#include "SpellAuras.h"
#include "SpellHistory.h"
#include "SpellMgr.h"
#include "SpellPackets.h"
#include "StringConvert.h"
#include "TalentPackets.h"
#include "TicketMgr.h"
#include "TradeData.h"
#include "Trainer.h"
// @tswow-begin (Using Rochet2/Transmog)
#include "Transmogrification.h"
// @tswow-end
#include "Transport.h"
#include "UpdateData.h"
#include "UpdateFieldFlags.h"
#include "UpdateMask.h"
#include "Util.h"
#include "Vehicle.h"
#include "Weather.h"
#include "WeatherMgr.h"
#include "World.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "WorldStatePackets.h"
// @tswow-begin
#include "TSProfile.h"
#include "TSEvents.h"
#include "TSFactionTemplate.h"
#include "TSQuest.h"
#include "TSPlayer.h"
#include "TSCreature.h"
#include "TSItem.h"
#include "TSGameObject.h"
#include "TSCorpse.h"
// @tswow-end
// @epoch-begin
#include "AnticheatMgr.h"
// @epoch-end

void Player::InitTalentForLevel()
{
    uint8 level = GetLevel();
    // talents base at level diff (talents = level - 9 but some can be used already)
    if (level < 10)
    {
        // Remove all talent points
        if (m_usedTalentCount > 0)                           // Free any used talents
        {
            ResetTalents(true);
            SetFreeTalentPoints(0);
        }
    }
    else
    {
        if (level < sWorld->getIntConfig(CONFIG_MIN_DUALSPEC_LEVEL) || m_specsCount == 0)
        {
            m_specsCount = 1;
            m_activeSpec = 0;
        }

        uint32 talentPointsForLevel = CalculateTalentsPoints();

        // if used more that have then reset
        if (m_usedTalentCount > talentPointsForLevel)
        {
            if (!GetSession()->HasPermission(rbac::RBAC_PERM_SKIP_CHECK_MORE_TALENTS_THAN_ALLOWED))
                ResetTalents(true);
            else
                SetFreeTalentPoints(0);
        }
        // else update amount of free points
        else
            SetFreeTalentPoints(talentPointsForLevel - m_usedTalentCount);
    }

    if (!GetSession()->PlayerLoading())
        SendTalentsInfoData(false);                         // update at client
}

bool Player::AddTalent(uint32 spellId, uint8 spec, bool learning)
{
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
    if (!spellInfo)
    {
        // do character spell book cleanup (all characters)
        if (!IsInWorld() && !learning)                       // spell load case
        {
            TC_LOG_ERROR("spells", "Player::AddTalent: Spell (ID: {}) does not exist. Deleting for all characters in `character_spell` and `character_talent`.", spellId);

            DeleteSpellFromAllPlayers(spellId);
        }
        else
            TC_LOG_ERROR("spells", "Player::AddTalent: Spell (ID: {}) does not exist", spellId);

        return false;
    }

    if (!SpellMgr::IsSpellValid(spellInfo, this, false))
    {
        // do character spell book cleanup (all characters)
        if (!IsInWorld() && !learning)                       // spell load case
        {
            TC_LOG_ERROR("spells", "Player::AddTalent: Spell (ID: {}) is invalid. Deleting for all characters in `character_spell` and `character_talent`.", spellId);

            DeleteSpellFromAllPlayers(spellId);
        }
        else
            TC_LOG_ERROR("spells", "Player::AddTalent: Spell (ID: {}) is invalid", spellId);

        return false;
    }

    PlayerTalentMap::iterator itr = m_talents[spec]->find(spellId);
    if (itr != m_talents[spec]->end())
        itr->second->state = PLAYERSPELL_UNCHANGED;
    else if (TalentSpellPos const* talentPos = GetTalentSpellPos(spellId))
    {
        if (TalentEntry const* talentInfo = sTalentStore.LookupEntry(talentPos->talent_id))
        {
            for (uint8 rank = 0; rank < MAX_TALENT_RANK; ++rank)
            {
                // skip learning spell and no rank spell case
                uint32 rankSpellId = talentInfo->SpellRank[rank];
                if (!rankSpellId || rankSpellId == spellId)
                    continue;

                itr = m_talents[spec]->find(rankSpellId);
                if (itr != m_talents[spec]->end())
                    itr->second->state = PLAYERSPELL_REMOVED;
            }
        }

        PlayerTalent* newtalent = new PlayerTalent();

        newtalent->state = learning ? PLAYERSPELL_NEW : PLAYERSPELL_UNCHANGED;
        newtalent->spec = spec;

        (*m_talents[spec])[spellId] = newtalent;
        return true;
    }
    return false;
}

uint32 Player::ResetTalentsCost() const
{
    if (sWorld->getBoolConfig(CONFIG_NO_RESET_TALENT_COST))
        return 0;

    // The first time reset costs 1 gold
    if (m_resetTalentsCost < 1*GOLD)
        return 1*GOLD;
    // then 5 gold
    else if (m_resetTalentsCost < 5*GOLD)
        return 5*GOLD;
    // After that it increases in increments of 5 gold
    else if (m_resetTalentsCost < 10*GOLD)
        return 10*GOLD;
    else
    {
        uint64 months = (GameTime::GetGameTime() - m_resetTalentsTime)/MONTH;
        if (months > 0)
        {
            // This cost will be reduced by a rate of 5 gold per month
            int32 new_cost = int32(m_resetTalentsCost - 5*GOLD*months);
            // to a minimum of 10 gold.
            return (new_cost < 10*GOLD ? 10*GOLD : new_cost);
        }
        else
        {
            // After that it increases in increments of 5 gold
            int32 new_cost = m_resetTalentsCost + 5*GOLD;
            // until it hits a cap of 50 gold.
            if (new_cost > 50*GOLD)
                new_cost = 50*GOLD;
            return new_cost;
        }
    }
}

void Player::IncreaseResetTalentsCostAndCounters(uint32 lastResetTalentsCost)
{

    if (lastResetTalentsCost > 0) // We don't want to reset the accumulated talent reset cost if we decide to temporarily enable CONFIG_NO_RESET_TALENT_COST
        m_resetTalentsCost = lastResetTalentsCost;

    m_resetTalentsTime = GameTime::GetGameTime();

    UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_GOLD_SPENT_FOR_TALENTS, lastResetTalentsCost);
    UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_NUMBER_OF_TALENT_RESETS, 1);
}

bool Player::ResetTalents(bool involuntarily /*= false*/)
{
        // @tswow-begin
    FIRE(
          Player,OnTalentsResetEarly
        , TSPlayer(this)
        , TSMutable<bool,bool>(&involuntarily)
    );
    // @tswow-end
    sScriptMgr->OnPlayerTalentsReset(this, involuntarily);

    // not need after this call
    if (HasAtLoginFlag(AT_LOGIN_RESET_TALENTS))
        RemoveAtLoginFlag(AT_LOGIN_RESET_TALENTS, true);

    uint32 talentPointsForLevel = CalculateTalentsPoints();

    if (m_usedTalentCount == 0)
    {
        SetFreeTalentPoints(talentPointsForLevel);
        return false;
    }

    RemovePet(nullptr, PET_SAVE_NOT_IN_SLOT, true);

    for (uint32 talentId = 0; talentId < sTalentStore.GetNumRows(); ++talentId)
    {
        TalentEntry const* talentInfo = sTalentStore.LookupEntry(talentId);

        if (!talentInfo)
            continue;

        TalentTabEntry const* talentTabInfo = sTalentTabStore.LookupEntry(talentInfo->TabID);

        if (!talentTabInfo)
            continue;

        // unlearn only talents for character class
        // some spell learned by one class as normal spells or know at creation but another class learn it as talent,
        // to prevent unexpected lost normal learned spell skip another class talents
        if ((GetClassMask() & talentTabInfo->ClassMask) == 0)
            continue;

        for (int8 rank = MAX_TALENT_RANK - 1; rank >= 0; --rank)
        {
            // skip non-existing talent ranks
            if (talentInfo->SpellRank[rank] == 0)
                continue;
            SpellInfo const* _spellEntry = sSpellMgr->GetSpellInfo(talentInfo->SpellRank[rank]);
            if (!_spellEntry)
                continue;

            // @tswow-begin
            FIRE_ID(_spellEntry->events.id, Spell, OnUnlearnTalent, TSSpellInfo(_spellEntry), TSPlayer(this), talentTabInfo->OrderIndex, talentInfo->TierID, talentInfo->ColumnIndex, rank, true);
            // @tswow-end
            RemoveSpell(talentInfo->SpellRank[rank], true);

            // search for spells that the talent teaches and unlearn them
            for (SpellEffectInfo const& spellEffectInfo : _spellEntry->GetEffects())
                if (spellEffectInfo.IsEffect(SPELL_EFFECT_LEARN_SPELL) && spellEffectInfo.TriggerSpell > 0)
                // @tswow-begin
                {
                    FIRE_ID(_spellEntry->events.id, Spell, OnUnlearnTalent, TSSpellInfo(_spellEntry), TSPlayer(this), talentTabInfo->OrderIndex, talentInfo->TierID, talentInfo->ColumnIndex, rank, false);
                    RemoveSpell(spellEffectInfo.TriggerSpell, true);
                }
                // @tswow-end
            // if this talent rank can be found in the PlayerTalentMap, mark the talent as removed so it gets deleted
            PlayerTalentMap::iterator plrTalent = m_talents[m_activeSpec]->find(talentInfo->SpellRank[rank]);
            if (plrTalent != m_talents[m_activeSpec]->end())
                plrTalent->second->state = PLAYERSPELL_REMOVED;
        }
    }

    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
    _SaveTalents(trans);
    _SaveSpells(trans);
    CharacterDatabase.CommitTransaction(trans);

    SetFreeTalentPoints(talentPointsForLevel);

    if (involuntarily)
        SendDirectMessage(WorldPackets::Talents::InvoluntarilyReset(false).Write());

    // @tswow-begin
    FIRE(
          Player,OnTalentsResetLate
        , TSPlayer(this)
        , involuntarily
    );
    // @tswow-end

    return true;
}

void Player::SetFreeTalentPoints(uint32 points)
{
    sScriptMgr->OnPlayerFreeTalentPointsChanged(this, points);
    SetUInt32Value(PLAYER_CHARACTER_POINTS1, points);
}

bool Player::HasTalent(uint32 spell, uint8 spec) const
{
    PlayerTalentMap::const_iterator itr = m_talents[spec]->find(spell);
    return (itr != m_talents[spec]->end() && itr->second->state != PLAYERSPELL_REMOVED);
}

// @tswow-begin change layout to attach event
uint32 Player::CalculateTalentsPoints() const
{
    uint32 base_talent = GetLevel() < 10 ? 0 : GetLevel() - 9;
    uint32 out_talent;

    if (GetClass() != CLASS_DEATH_KNIGHT || GetMapId() != 609)
    {
        out_talent = uint32((base_talent + m_questRewardPermTalentCount) * sWorld->getRate(RATE_TALENT));
    }
    else
    {
        uint32 talentPointsForLevel = GetLevel() < 56 ? 0 : GetLevel() - 55;
        talentPointsForLevel += m_questRewardTalentCount;

        if (talentPointsForLevel > base_talent)
            talentPointsForLevel = base_talent;

        out_talent = uint32((talentPointsForLevel + m_questRewardPermTalentCount) * sWorld->getRate(RATE_TALENT));
    }
    FIRE(
          Player,OnCalcTalentPoints
        , TSPlayer(const_cast<Player*>(this))
        , TSMutableNumber<uint32>(&out_talent)
    )
        return out_talent;
}
// @tswow-end

// @tswow-begin
uint32 Player::GetTalentPointsInTree(uint32 tabId)
{
    uint32 spentPoints = 0;
    for (uint32 i = 0; i < sTalentStore.GetNumRows(); i++)          // Loop through all talents.
        if (TalentEntry const* tmpTalent = sTalentStore.LookupEntry(i))                                  // the way talents are tracked
            if (tmpTalent->TabID == tabId)
                for (uint8 rank = 0; rank < MAX_TALENT_RANK; rank++)
                    if (tmpTalent->SpellRank[rank] != 0)
                        if (HasSpell(tmpTalent->SpellRank[rank]))
                            spentPoints += (rank + 1);
    return spentPoints;
}
// @tswow-end

void Player::LearnTalent(uint32 talentId, uint32 talentRank)
{
    uint32 CurTalentPoints = GetFreeTalentPoints();

    if (CurTalentPoints == 0)
        return;

    if (talentRank >= MAX_TALENT_RANK)
        return;

    TalentEntry const* talentInfo = sTalentStore.LookupEntry(talentId);

    if (!talentInfo)
        return;

    TalentTabEntry const* talentTabInfo = sTalentTabStore.LookupEntry(talentInfo->TabID);

    if (!talentTabInfo)
        return;

    // prevent learn talent for different class (cheating)
    if ((GetClassMask() & talentTabInfo->ClassMask) == 0)
        return;

    // find current max talent rank (0~5)
    uint8 curtalent_maxrank = 0; // 0 = not learned any rank
    for (int8 rank = MAX_TALENT_RANK-1; rank >= 0; --rank)
    {
        if (talentInfo->SpellRank[rank] && HasSpell(talentInfo->SpellRank[rank]))
        {
            curtalent_maxrank = (rank + 1);
            break;
        }
    }

    // we already have same or higher talent rank learned
    if (curtalent_maxrank >= (talentRank + 1))
        return;

    // check if we have enough talent points
    if (CurTalentPoints < (talentRank - curtalent_maxrank + 1))
        return;

    // Check if it requires another talent
    if (talentInfo->PrereqTalent > 0)
    {
        if (TalentEntry const* depTalentInfo = sTalentStore.LookupEntry(talentInfo->PrereqTalent))
        {
            bool hasEnoughRank = false;
            for (uint8 rank = talentInfo->PrereqRank; rank < MAX_TALENT_RANK; rank++)
            {
                if (depTalentInfo->SpellRank[rank] != 0)
                    if (HasSpell(depTalentInfo->SpellRank[rank]))
                        hasEnoughRank = true;
            }
            if (!hasEnoughRank)
                return;
        }
    }

    // Find out how many points we have in this field
    // @tswow-begin move to function
    // not have required min points spent in talent tree
    if (talentInfo->TierID > 0 && GetTalentPointsInTree(talentInfo->TabID) < (talentInfo->TierID * MAX_TALENT_RANK))
    // @tswow-end
        return;

    // spell not set in talent.dbc
    uint32 spellid = talentInfo->SpellRank[talentRank];
    if (spellid == 0)
    {
        TC_LOG_ERROR("entities.player", "Player::LearnTalent: Talent.dbc has no spellInfo for talent: {} (spell id = 0)", talentId);
        return;
    }

    // already known
    if (HasSpell(spellid))
        return;

    // @tswow-begin
    bool cancel = false;
    FIRE(Player,OnLearnTalent, TSPlayer(this), talentInfo->TabID, talentId, talentRank, spellid, TSMutable<bool,bool>(&cancel));
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellid);
    if (spellInfo)
    {
        FIRE_ID(spellInfo->events.id,Spell,OnLearnTalent, TSSpellInfo(spellInfo), TSPlayer(this), talentInfo->TabID, talentId, talentRank, spellid, TSMutable<bool, bool>(&cancel));
    }
    if (cancel)
    {
        return;
    }
    // @tswow-end

    // learn! (other talent ranks will unlearned at learning)
    LearnSpell(spellid, false);
    AddTalent(spellid, m_activeSpec, true);

    TC_LOG_DEBUG("misc", "Player::LearnTalent: TalentID: {} Spell: {} Group: {}\n", talentId, spellid, uint32(m_activeSpec));

    // update free talent points
    SetFreeTalentPoints(CurTalentPoints - (talentRank - curtalent_maxrank + 1));
}

void Player::LearnPetTalent(ObjectGuid petGuid, uint32 talentId, uint32 talentRank)
{
    Pet* pet = GetPet();

    if (!pet)
        return;

    if (petGuid != pet->GetGUID())
        return;

    uint32 CurTalentPoints = pet->GetFreeTalentPoints();

    if (CurTalentPoints == 0)
        return;

    if (talentRank >= MAX_PET_TALENT_RANK)
        return;

    TalentEntry const* talentInfo = sTalentStore.LookupEntry(talentId);

    if (!talentInfo)
        return;

    TalentTabEntry const* talentTabInfo = sTalentTabStore.LookupEntry(talentInfo->TabID);

    if (!talentTabInfo)
        return;

    CreatureTemplate const* ci = pet->GetCreatureTemplate();

    if (!ci)
        return;

    CreatureFamilyEntry const* pet_family = sCreatureFamilyStore.LookupEntry(ci->family);

    if (!pet_family)
        return;

    if (pet_family->PetTalentType < 0)                       // not hunter pet
        return;

    // prevent learn talent for different family (cheating)
    if (!((1 << pet_family->PetTalentType) & talentTabInfo->PetTalentMask))
        return;

    // find current max talent rank (0~5)
    uint8 curtalent_maxrank = 0; // 0 = not learned any rank
    for (int8 rank = MAX_TALENT_RANK-1; rank >= 0; --rank)
    {
        if (talentInfo->SpellRank[rank] && pet->HasSpell(talentInfo->SpellRank[rank]))
        {
            curtalent_maxrank = (rank + 1);
            break;
        }
    }

    // we already have same or higher talent rank learned
    if (curtalent_maxrank >= (talentRank + 1))
        return;

    // check if we have enough talent points
    if (CurTalentPoints < (talentRank - curtalent_maxrank + 1))
        return;

    // Check if it requires another talent
    if (talentInfo->PrereqTalent > 0)
    {
        if (TalentEntry const* depTalentInfo = sTalentStore.LookupEntry(talentInfo->PrereqTalent))
        {
            bool hasEnoughRank = false;
            for (uint8 rank = talentInfo->PrereqRank; rank < MAX_TALENT_RANK; rank++)
            {
                if (depTalentInfo->SpellRank[rank] != 0)
                    if (pet->HasSpell(depTalentInfo->SpellRank[rank]))
                        hasEnoughRank = true;
            }
            if (!hasEnoughRank)
                return;
        }
    }

    // Find out how many points we have in this field
    uint32 spentPoints = 0;

    uint32 tTab = talentInfo->TabID;
    if (talentInfo->TierID > 0)
    {
        uint32 numRows = sTalentStore.GetNumRows();
        for (uint32 i = 0; i < numRows; ++i)          // Loop through all talents.
        {
            // Someday, someone needs to revamp
            TalentEntry const* tmpTalent = sTalentStore.LookupEntry(i);
            if (tmpTalent)                                  // the way talents are tracked
            {
                if (tmpTalent->TabID == tTab)
                {
                    for (uint8 rank = 0; rank < MAX_TALENT_RANK; rank++)
                    {
                        if (tmpTalent->SpellRank[rank] != 0)
                        {
                            if (pet->HasSpell(tmpTalent->SpellRank[rank]))
                            {
                                spentPoints += (rank + 1);
                            }
                        }
                    }
                }
            }
        }
    }

    // not have required min points spent in talent tree
    if (spentPoints < (talentInfo->TierID * MAX_PET_TALENT_RANK))
        return;

    // spell not set in talent.dbc
    uint32 spellid = talentInfo->SpellRank[talentRank];
    if (spellid == 0)
    {
        TC_LOG_ERROR("entities.player", "Talent.dbc contains talent: {} Rank: {} spell id = 0", talentId, talentRank);
        return;
    }

    // already known
    if (pet->HasSpell(spellid))
        return;

    // learn! (other talent ranks will unlearned at learning)
    pet->learnSpell(spellid);
    TC_LOG_DEBUG("entities.player", "PetTalentID: {} Rank: {} Spell: {}\n", talentId, talentRank, spellid);

    // update free talent points
    pet->SetFreeTalentPoints(CurTalentPoints - (talentRank - curtalent_maxrank + 1));
}

void Player::BuildPlayerTalentsInfoData(WorldPacket* data)
{
    *data << uint32(GetFreeTalentPoints());                 // unspentTalentPoints
    *data << uint8(m_specsCount);                           // talent group count (0, 1 or 2)
    *data << uint8(m_activeSpec);                           // talent group index (0 or 1)

    if (m_specsCount)
    {
        if (m_specsCount > MAX_TALENT_SPECS)
            m_specsCount = MAX_TALENT_SPECS;

        // loop through all specs (only 1 for now)
        for (uint32 specIdx = 0; specIdx < m_specsCount; ++specIdx)
        {
            uint8 talentIdCount = 0;
            size_t pos = data->wpos();
            *data << uint8(talentIdCount);                  // [PH], talentIdCount

            // find class talent tabs (all players have 3 talent tabs)
            uint32 const* talentTabIds = GetTalentTabPages(GetClass());

            for (uint8 i = 0; i < MAX_TALENT_TABS; ++i)
            {
                uint32 talentTabId = talentTabIds[i];

                for (uint32 talentId = 0; talentId < sTalentStore.GetNumRows(); ++talentId)
                {
                    TalentEntry const* talentInfo = sTalentStore.LookupEntry(talentId);
                    if (!talentInfo)
                        continue;

                    // skip another tab talents
                    if (talentInfo->TabID != talentTabId)
                        continue;

                    // find max talent rank (0~4)
                    int8 curtalent_maxrank = -1;
                    for (int8 rank = MAX_TALENT_RANK-1; rank >= 0; --rank)
                    {
                        if (talentInfo->SpellRank[rank] && HasTalent(talentInfo->SpellRank[rank], specIdx))
                        {
                            curtalent_maxrank = rank;
                            break;
                        }
                    }

                    // not learned talent
                    if (curtalent_maxrank < 0)
                        continue;

                    *data << uint32(talentInfo->ID);  // Talent.dbc
                    *data << uint8(curtalent_maxrank);      // talentMaxRank (0-4)

                    ++talentIdCount;
                }
            }

            data->put<uint8>(pos, talentIdCount);           // put real count

            *data << uint8(MAX_GLYPH_SLOT_INDEX);           // glyphs count

            for (uint8 i = 0; i < MAX_GLYPH_SLOT_INDEX; ++i)
                *data << uint16(m_Glyphs[specIdx][i]);               // GlyphProperties.dbc
        }
    }
}

void Player::BuildPetTalentsInfoData(WorldPacket* data)
{
    ZoneScopedNC("Player::BuildPetTalentsInfoData", WORLD_UPDATE_COLOR)

    uint32 unspentTalentPoints = 0;
    size_t pointsPos = data->wpos();
    *data << uint32(unspentTalentPoints);                   // [PH], unspentTalentPoints

    uint8 talentIdCount = 0;
    size_t countPos = data->wpos();
    *data << uint8(talentIdCount);                          // [PH], talentIdCount

    Pet* pet = GetPet();
    if (!pet)
        return;

    unspentTalentPoints = pet->GetFreeTalentPoints();

    data->put<uint32>(pointsPos, unspentTalentPoints);      // put real points

    CreatureTemplate const* ci = pet->GetCreatureTemplate();
    if (!ci)
        return;

    CreatureFamilyEntry const* pet_family = sCreatureFamilyStore.LookupEntry(ci->family);
    if (!pet_family || pet_family->PetTalentType < 0)
        return;

    for (uint32 talentTabId = 1; talentTabId < sTalentTabStore.GetNumRows(); ++talentTabId)
    {
        TalentTabEntry const* talentTabInfo = sTalentTabStore.LookupEntry(talentTabId);
        if (!talentTabInfo)
            continue;

        if (!((1 << pet_family->PetTalentType) & talentTabInfo->PetTalentMask))
            continue;

        for (uint32 talentId = 0; talentId < sTalentStore.GetNumRows(); ++talentId)
        {
            TalentEntry const* talentInfo = sTalentStore.LookupEntry(talentId);
            if (!talentInfo)
                continue;

            // skip another tab talents
            if (talentInfo->TabID != talentTabId)
                continue;

            // find max talent rank (0~4)
            int8 curtalent_maxrank = -1;
            for (int8 rank = MAX_TALENT_RANK-1; rank >= 0; --rank)
            {
                if (talentInfo->SpellRank[rank] && pet->HasSpell(talentInfo->SpellRank[rank]))
                {
                    curtalent_maxrank = rank;
                    break;
                }
            }

            // not learned talent
            if (curtalent_maxrank < 0)
                continue;

            *data << uint32(talentInfo->ID);          // Talent.dbc
            *data << uint8(curtalent_maxrank);              // talentMaxRank (0-4)

            ++talentIdCount;
        }

        data->put<uint8>(countPos, talentIdCount);          // put real count

        break;
    }
}

void Player::SendTalentsInfoData(bool pet)
{
    ZoneScopedNC("Player::SendTalentsInfoData", WORLD_UPDATE_COLOR)

    WorldPacket data(SMSG_TALENTS_INFO, 50);
    data << uint8(pet ? 1 : 0);
    if (pet)
        BuildPetTalentsInfoData(&data);
    else
        BuildPlayerTalentsInfoData(&data);
    SendDirectMessage(&data);
}

void Player::UpdateSpecCount(uint8 count)
{
    uint32 curCount = GetSpecsCount();
    if (curCount == count)
        return;

    if (m_activeSpec >= count)
        ActivateSpec(0);

    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
    CharacterDatabasePreparedStatement* stmt;

    // Copy spec data
    if (count > curCount)
    {
        _SaveActions(trans); // make sure the button list is cleaned up
        for (ActionButtonList::iterator itr = m_actionButtons.begin(); itr != m_actionButtons.end(); ++itr)
        {
            stmt = CharacterDatabase.GetPreparedStatement(CHAR_INS_CHAR_ACTION);
            stmt->setUInt32(0, GetGUID().GetCounter());
            stmt->setUInt8(1, 1);
            stmt->setUInt8(2, itr->first);
            stmt->setUInt32(3, itr->second.GetAction());
            stmt->setUInt8(4, uint8(itr->second.GetType()));
            trans->Append(stmt);
        }
    }
    // Delete spec data for removed spec.
    else if (count < curCount)
    {
        _SaveActions(trans);

        stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHAR_ACTION_EXCEPT_SPEC);
        stmt->setUInt8(0, m_activeSpec);
        stmt->setUInt32(1, GetGUID().GetCounter());
        trans->Append(stmt);

        m_activeSpec = 0;
    }

    CharacterDatabase.CommitTransaction(trans);

    SetSpecsCount(count);

    SendTalentsInfoData(false);
}

void Player::ActivateSpec(uint8 spec)
{
    if (GetActiveSpec() == spec)
        return;

    if (spec > GetSpecsCount())
        return;

    if (IsNonMeleeSpellCast(false))
        InterruptNonMeleeSpells(false);

    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
    _SaveActions(trans);
    CharacterDatabase.CommitTransaction(trans);

    // TO-DO: We need more research to know what happens with warlock's reagent
    if (Pet* pet = GetPet())
        RemovePet(pet, PET_SAVE_NOT_IN_SLOT);

    ClearAllReactives();
    UnsummonAllTotems();
    ExitVehicle();
    RemoveAllControlled();

    // remove single target auras at other targets
    AuraList& scAuras = GetSingleCastAuras();
    for (AuraList::iterator iter = scAuras.begin(); iter != scAuras.end();)
    {
        Aura* aura = *iter;
        if (aura->GetUnitOwner() != this)
        {
            aura->Remove();
            iter = scAuras.begin();
        }
        else
            ++iter;
    }
    /*RemoveAllAurasOnDeath();
    if (GetPet())
        GetPet()->RemoveAllAurasOnDeath();*/

    //RemoveAllAuras(GetGUID(), nullptr, false, true); // removes too many auras
    //ExitVehicle(); // should be impossible to switch specs from inside a vehicle..

    // Let client clear his current Actions
    SendActionButtons(2);
    // m_actionButtons.clear() is called in the next _LoadActionButtons
    for (uint32 talentId = 0; talentId < sTalentStore.GetNumRows(); ++talentId)
    {
        TalentEntry const* talentInfo = sTalentStore.LookupEntry(talentId);

        if (!talentInfo)
            continue;

        TalentTabEntry const* talentTabInfo = sTalentTabStore.LookupEntry(talentInfo->TabID);

        if (!talentTabInfo)
            continue;

        // unlearn only talents for character class
        // some spell learned by one class as normal spells or know at creation but another class learn it as talent,
        // to prevent unexpected lost normal learned spell skip another class talents
        if ((GetClassMask() & talentTabInfo->ClassMask) == 0)
            continue;

        for (int8 rank = MAX_TALENT_RANK-1; rank >= 0; --rank)
        {
            // skip non-existing talent ranks
            if (talentInfo->SpellRank[rank] == 0)
                continue;
            RemoveSpell(talentInfo->SpellRank[rank], true); // removes the talent, and all dependant, learned, and chained spells..
            if (SpellInfo const* _spellEntry = sSpellMgr->GetSpellInfo(talentInfo->SpellRank[rank]))
                for (SpellEffectInfo const& spellEffectInfo : _spellEntry->GetEffects())                  // search through the SpellInfo for valid trigger spells
                    if (spellEffectInfo.IsEffect(SPELL_EFFECT_LEARN_SPELL) && spellEffectInfo.TriggerSpell > 0)
                        RemoveSpell(spellEffectInfo.TriggerSpell, true); // and remove any spells that the talent teaches
            // if this talent rank can be found in the PlayerTalentMap, mark the talent as removed so it gets deleted
            //PlayerTalentMap::iterator plrTalent = m_talents[m_activeSpec]->find(talentInfo->RankID[rank]);
            //if (plrTalent != m_talents[m_activeSpec]->end())
            //    plrTalent->second->state = PLAYERSPELL_REMOVED;
        }
    }

    // set glyphs
    for (uint8 slot = 0; slot < MAX_GLYPH_SLOT_INDEX; ++slot)
        // remove secondary glyph
        if (uint32 oldglyph = m_Glyphs[m_activeSpec][slot])
            if (GlyphPropertiesEntry const* old_gp = sGlyphPropertiesStore.LookupEntry(oldglyph))
                RemoveAurasDueToSpell(old_gp->SpellID);

    SetActiveSpec(spec);
    uint32 spentTalents = 0;

    for (uint32 talentId = 0; talentId < sTalentStore.GetNumRows(); ++talentId)
    {
        TalentEntry const* talentInfo = sTalentStore.LookupEntry(talentId);

        if (!talentInfo)
            continue;

        TalentTabEntry const* talentTabInfo = sTalentTabStore.LookupEntry(talentInfo->TabID);

        if (!talentTabInfo)
            continue;

        // learn only talents for character class
        if ((GetClassMask() & talentTabInfo->ClassMask) == 0)
            continue;

        // learn highest talent rank that exists in newly activated spec
        for (int8 rank = MAX_TALENT_RANK-1; rank >= 0; --rank)
        {
            // skip non-existing talent ranks
            if (talentInfo->SpellRank[rank] == 0)
                continue;
            // if the talent can be found in the newly activated PlayerTalentMap
            if (HasTalent(talentInfo->SpellRank[rank], m_activeSpec))
            {
                LearnSpell(talentInfo->SpellRank[rank], false); // add the talent to the PlayerSpellMap
                spentTalents += (rank + 1);                  // increment the spentTalents count
            }
        }
    }

    // set glyphs
    for (uint8 slot = 0; slot < MAX_GLYPH_SLOT_INDEX; ++slot)
    {
        uint32 glyph = m_Glyphs[m_activeSpec][slot];

        // apply primary glyph
        if (glyph)
            if (GlyphPropertiesEntry const* gp = sGlyphPropertiesStore.LookupEntry(glyph))
                CastSpell(this, gp->SpellID, true);

        SetGlyph(slot, glyph);
    }

    m_usedTalentCount = spentTalents;
    InitTalentForLevel();

    // load them asynchronously
    {
        CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_ACTIONS_SPEC);
        stmt->setUInt32(0, GetGUID().GetCounter());
        stmt->setUInt8(1, m_activeSpec);

        WorldSession* mySess = GetSession();
        mySess->GetQueryProcessor().AddCallback(CharacterDatabase.AsyncQuery(stmt)
            .WithPreparedCallback([mySess](PreparedQueryResult result)
        {
            // safe callback, we can't pass this pointer directly
            // in case player logs out before db response (player would be deleted in that case)
            if (Player* thisPlayer = mySess->GetPlayer())
                thisPlayer->LoadActions(result);
        }));
    }

    Powers pw = GetPowerType();
    if (pw != POWER_MANA)
        SetPower(POWER_MANA, 0); // Mana must be 0 even if it isn't the active power type.

    SetPower(pw, 0);

    Unit::AuraEffectList const& shapeshiftAuras = GetAuraEffectsByType(SPELL_AURA_MOD_SHAPESHIFT);
    for (AuraEffect* aurEff : shapeshiftAuras)
    {
        aurEff->HandleShapeshiftBoosts(this, false);
        aurEff->HandleShapeshiftBoosts(this, true);
    }
}

void Player::SendTalentWipeConfirm(ObjectGuid trainerGuid) const
{
    SendDirectMessage(WorldPackets::Talents::RespecWipeConfirm(trainerGuid, ResetTalentsCost()).Write());
}

void Player::ResetPetTalents()
{
    // This needs another gossip option + NPC text as a confirmation.
    // The confirmation gossip listid has the text: "Yes, please do."
    Pet* pet = GetPet();

    if (!pet || pet->getPetType() != HUNTER_PET || pet->m_usedTalentCount == 0)
        return;

    CharmInfo* charmInfo = pet->GetCharmInfo();
    if (!charmInfo)
    {
        TC_LOG_ERROR("entities.player", "Object {} is considered pet-like, but doesn't have charm info!", pet->GetGUID().ToString());
        return;
    }
    pet->resetTalents();
    SendTalentsInfoData(true);
}

void Player::SetGlyph(uint8 slot, uint32 glyph)
{
    m_Glyphs[m_activeSpec][slot] = glyph;
    SetUInt32Value(PLAYER_FIELD_GLYPHS_1 + slot, glyph);
}

void Player::InitGlyphsForLevel()
{
    for (uint32 i = 0; i < sGlyphSlotStore.GetNumRows(); ++i)
        if (GlyphSlotEntry const* gs = sGlyphSlotStore.LookupEntry(i))
            if (gs->Tooltip)
                SetGlyphSlot(gs->Tooltip - 1, gs->ID);

    uint8 level = GetLevel();
    uint32 value = 0;

    // 0x3F = 0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 for 80 level
    if (level >= 15)
        value |= (0x01 | 0x02);
    if (level >= 30)
        value |= 0x08;
    if (level >= 50)
        value |= 0x04;
    if (level >= 70)
        value |= 0x10;
    if (level >= 80)
        value |= 0x20;

    // @tswow-begin
    FIRE(Player,OnGlyphInitForLevel
        , TSPlayer(this)
        , TSMutableNumber<uint32>(&value)
    );
    // @tswow-end

    SetUInt32Value(PLAYER_GLYPHS_ENABLED, value);
}