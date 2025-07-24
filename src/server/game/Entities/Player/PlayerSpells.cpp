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

void Player::SendInitialSpells()
{
    uint16 spellCooldowns = GetSpellHistory()->GetCooldownsSizeForPacket();
    uint16 spellCount = 0;

    WorldPacket data(SMSG_INITIAL_SPELLS, (1 + 2 + 4 * m_spells.size() + 2 + spellCooldowns * (2 + 2 + 2 + 4 + 4)));
    data << uint8(0);

    size_t countPos = data.wpos();
    data << uint16(spellCount);                             // spell count placeholder

    for (PlayerSpellMap::const_iterator itr = m_spells.begin(); itr != m_spells.end(); ++itr)
    {
        if (itr->second.state == PLAYERSPELL_REMOVED)
            continue;

        if (!itr->second.active || itr->second.disabled)
            continue;

        data << uint32(itr->first);
        data << uint16(0);                                  // it's not slot id

        spellCount +=1;
    }

    data.put<uint16>(countPos, spellCount);                  // write real count value

    GetSpellHistory()->WritePacket<Player>(data);

    SendDirectMessage(&data);
}

void Player::SendUnlearnSpells()
{
    WorldPacket data(SMSG_SEND_UNLEARN_SPELLS, 4 + 4 * m_spells.size());

    uint32 spellCount = 0;
    size_t countPos = data.wpos();
    data << uint32(spellCount);                             // spell count placeholder

    for (PlayerSpellMap::const_iterator itr = m_spells.begin(); itr != m_spells.end(); ++itr)
    {
        if (itr->second.state == PLAYERSPELL_REMOVED)
            continue;

        if (itr->second.active || itr->second.disabled)
            continue;

        auto skillLineAbilities = sSpellMgr->GetSkillLineAbilityMapBounds(itr->first);
        if (skillLineAbilities.first == skillLineAbilities.second)
            continue;

        bool hasSupercededSpellInfoInClient = false;
        for (auto boundsItr = skillLineAbilities.first; boundsItr != skillLineAbilities.second; ++boundsItr)
        {
            if (boundsItr->second->SupercededBySpell)
            {
                hasSupercededSpellInfoInClient = true;
                break;
            }
        }

        if (hasSupercededSpellInfoInClient)
            continue;

        uint32 nextRank = sSpellMgr->GetNextSpellInChain(itr->first);
        if (!nextRank || !HasSpell(nextRank))
            continue;

        data << uint32(itr->first);

        ++spellCount;
    }

    data.put<uint32>(countPos, spellCount);                  // write real count value

    SendDirectMessage(&data);
}

void Player::SendTameFailure(uint8 result)
{
    WorldPacket data(SMSG_PET_TAME_FAILURE, 1);
    data << uint8(result);
    SendDirectMessage(&data);
}

void DeleteSpellFromAllPlayers(uint32 spellId)
{
    CharacterDatabaseStatements stmts[2] = {CHAR_DEL_INVALID_SPELL_SPELLS, CHAR_DEL_INVALID_SPELL_TALENTS};
    for (uint8 i = 0; i < 2; i++)
    {
        CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(stmts[i]);

        stmt->setUInt32(0, spellId);

        CharacterDatabase.Execute(stmt);
    }
}

static bool IsUnlearnSpellsPacketNeededForSpell(uint32 spellId)
{
    SpellInfo const* spellInfo = sSpellMgr->AssertSpellInfo(spellId);
    if (spellInfo->IsRanked() && !spellInfo->IsStackableWithRanks())
    {
        auto skillLineAbilities = sSpellMgr->GetSkillLineAbilityMapBounds(spellId);
        if (skillLineAbilities.first != skillLineAbilities.second)
        {
            bool hasSupercededSpellInfoInClient = false;
            for (auto boundsItr = skillLineAbilities.first; boundsItr != skillLineAbilities.second; ++boundsItr)
            {
                if (boundsItr->second->SupercededBySpell)
                {
                    hasSupercededSpellInfoInClient = true;
                    break;
                }
            }

            return !hasSupercededSpellInfoInClient;
        }
    }

    return false;
}

bool Player::AddSpell(uint32 spellId, bool active, bool learning, bool dependent, bool disabled, bool loading /*= false*/, uint32 fromSkill /*= 0*/)
{
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
    if (!spellInfo)
    {
        // do character spell book cleanup (all characters)
        if (!IsInWorld() && !learning)                       // spell load case
        {
            TC_LOG_ERROR("spells", "Player::AddSpell: Spell (ID: {}) does not exist. deleting for all characters in `character_spell` and `character_talent`.", spellId);

            DeleteSpellFromAllPlayers(spellId);
        }
        else
            TC_LOG_ERROR("spells", "Player::AddSpell: Spell (ID: {}) does not exist", spellId);

        return false;
    }

    if (!SpellMgr::IsSpellValid(spellInfo, this, false))
    {
        // do character spell book cleanup (all characters)
        if (!IsInWorld() && !learning)                       // spell load case
        {
            TC_LOG_ERROR("spells", "Player::AddSpell: Spell (ID: {}) is invalid. deleting for all characters in `character_spell` and `character_talent`.", spellId);

            DeleteSpellFromAllPlayers(spellId);
        }
        else
            TC_LOG_ERROR("spells", "Player::AddSpell: Spell (ID: {}) is invalid", spellId);

        return false;
    }

    PlayerSpellState state = learning ? PLAYERSPELL_NEW : PLAYERSPELL_UNCHANGED;

    bool dependent_set = false;
    bool disabled_case = false;
    bool superceded_old = false;

    PlayerSpellMap::iterator itr = m_spells.find(spellId);

    // Remove temporary spell if found to prevent conflicts
    if (itr != m_spells.end() && itr->second.state == PLAYERSPELL_TEMPORARY)
        RemoveTemporarySpell(spellId);
    else if (itr != m_spells.end())
    {
        uint32 next_active_spell_id = 0;
        // fix activate state for non-stackable low rank (and find next spell for !active case)
        if (!spellInfo->IsStackableWithRanks() && spellInfo->IsRanked())
        {
            if (uint32 next = sSpellMgr->GetNextSpellInChain(spellId))
            {
                if (HasSpell(next))
                {
                    // high rank already known so this must !active
                    active = false;
                    next_active_spell_id = next;
                }
            }
        }

        // not do anything if already known in expected state
        if (itr->second.state != PLAYERSPELL_REMOVED && itr->second.active == active &&
            itr->second.dependent == dependent && itr->second.disabled == disabled)
        {
            if (!IsInWorld() && !learning)                   // explicitly load from DB and then exist in it already and set correctly
                itr->second.state = PLAYERSPELL_UNCHANGED;

            return false;
        }

        // dependent spell known as not dependent, overwrite state
        if (itr->second.state != PLAYERSPELL_REMOVED && !itr->second.dependent && dependent)
        {
            itr->second.dependent = dependent;
            if (itr->second.state != PLAYERSPELL_NEW)
                itr->second.state = PLAYERSPELL_CHANGED;
            dependent_set = true;
        }

        // update active state for known spell
        if (itr->second.active != active && itr->second.state != PLAYERSPELL_REMOVED && !itr->second.disabled)
        {
            itr->second.active = active;

            if (!IsInWorld() && !learning && !dependent_set) // explicitly load from DB and then exist in it already and set correctly
                itr->second.state = PLAYERSPELL_UNCHANGED;
            else if (itr->second.state != PLAYERSPELL_NEW)
                itr->second.state = PLAYERSPELL_CHANGED;

            if (active)
            {
                if (spellInfo->IsPassive() && HandlePassiveSpellLearn(spellInfo))
                    CastSpell(this, spellId, true);
            }
            else if (IsInWorld())
            {
                if (next_active_spell_id)
                {
                    SendSupercededSpell(spellId, next_active_spell_id);
                    if (IsUnlearnSpellsPacketNeededForSpell(spellId))
                        SendUnlearnSpells();
                }
                else
                {
                    WorldPacket data(SMSG_REMOVED_SPELL, 4);
                    data << uint32(spellId);
                    SendDirectMessage(&data);
                }
            }

            return active;                                  // learn (show in spell book if active now)
        }

        if (itr->second.disabled != disabled && itr->second.state != PLAYERSPELL_REMOVED)
        {
            if (itr->second.state != PLAYERSPELL_NEW)
                itr->second.state = PLAYERSPELL_CHANGED;
            itr->second.disabled = disabled;

            if (disabled)
                return false;

            disabled_case = true;
        }
        else switch (itr->second.state)
        {
            case PLAYERSPELL_UNCHANGED:                     // known saved spell
                return false;
            case PLAYERSPELL_REMOVED:                       // re-learning removed not saved spell
            {
                m_spells.erase(itr);
                state = PLAYERSPELL_CHANGED;
                break;                                      // need re-add
            }
            default:                                        // known not saved yet spell (new or modified)
            {
                // can be in case spell loading but learned at some previous spell loading
                if (!IsInWorld() && !learning && !dependent_set)
                    itr->second.state = PLAYERSPELL_UNCHANGED;

                return false;
            }
        }
    }

    if (!disabled_case) // skip new spell adding if spell already known (disabled spells case)
    {
        // talent: unlearn all other talent ranks (high and low)
        if (TalentSpellPos const* talentPos = GetTalentSpellPos(spellId))
        {
            if (TalentEntry const* talentInfo = sTalentStore.LookupEntry(talentPos->talent_id))
            {
                for (uint8 rank = 0; rank < MAX_TALENT_RANK; ++rank)
                {
                    // skip learning spell and no rank spell case
                    uint32 rankSpellId = talentInfo->SpellRank[rank];
                    if (!rankSpellId || rankSpellId == spellId)
                        continue;

                    RemoveSpell(rankSpellId, false, false);
                }
            }
        }
        // non talent spell: learn low ranks (recursive call)
        else if (uint32 prev_spell = sSpellMgr->GetPrevSpellInChain(spellId))
        {
            if (!IsInWorld() || disabled)                    // at spells loading, no output, but allow save
                AddSpell(prev_spell, active, true, true, disabled, false, fromSkill);
            else                                            // at normal learning
                LearnSpell(prev_spell, true, fromSkill);
        }

        std::pair<PlayerSpellMap::iterator, bool> inserted = m_spells.emplace(std::piecewise_construct, std::forward_as_tuple(spellId), std::forward_as_tuple());
        PlayerSpell& newspell = inserted.first->second;
        // learning a previous rank might have given us this spell already from a skill autolearn, most likely with PLAYERSPELL_NEW state
        // we dont want to do double insert if this happened during load from db so we force state to CHANGED, just in case
        newspell.state     = inserted.second ? state : PLAYERSPELL_CHANGED;
        newspell.active    = active;
        newspell.dependent = dependent;
        newspell.disabled  = disabled;

        bool needsUnlearnSpellsPacket = false;

        // replace spells in action bars and spellbook to bigger rank if only one spell rank must be accessible
        if (newspell.active && !newspell.disabled && !spellInfo->IsStackableWithRanks() && spellInfo->IsRanked())
        {
            for (PlayerSpellMap::iterator itr2 = m_spells.begin(); itr2 != m_spells.end(); ++itr2)
            {
                if (itr2->second.state == PLAYERSPELL_REMOVED)
                    continue;

                SpellInfo const* i_spellInfo = sSpellMgr->GetSpellInfo(itr2->first);
                if (!i_spellInfo)
                    continue;

                if (spellInfo->IsDifferentRankOf(i_spellInfo))
                {
                    if (itr2->second.active)
                    {
                        if (spellInfo->IsHighRankOf(i_spellInfo))
                        {
                            if (IsInWorld())                 // not send spell (re-/over-)learn packets at loading
                            {
                                SendSupercededSpell(itr2->first, spellId);
                                needsUnlearnSpellsPacket = needsUnlearnSpellsPacket || IsUnlearnSpellsPacketNeededForSpell(itr2->first);
                            }

                            // mark old spell as disable (SMSG_SUPERCEDED_SPELL replace it in client by new)
                            itr2->second.active = false;
                            if (itr2->second.state != PLAYERSPELL_NEW)
                                itr2->second.state = PLAYERSPELL_CHANGED;
                            superceded_old = true;          // new spell replace old in action bars and spell book.
                        }
                        else
                        {
                            if (IsInWorld())                 // not send spell (re-/over-)learn packets at loading
                            {
                                SendSupercededSpell(spellId, itr2->first);
                                needsUnlearnSpellsPacket = needsUnlearnSpellsPacket || IsUnlearnSpellsPacketNeededForSpell(spellId);
                            }

                            // mark new spell as disable (not learned yet for client and will not learned)
                            newspell.active = false;
                            if (newspell.state != PLAYERSPELL_NEW)
                                newspell.state = PLAYERSPELL_CHANGED;
                        }
                    }
                }
            }
        }

        if (needsUnlearnSpellsPacket)
            SendUnlearnSpells();

        // return false if spell disabled
        if (newspell.disabled)
            return false;
    }

    uint32 talentCost = GetTalentSpellCost(spellId);

    // cast talents with SPELL_EFFECT_LEARN_SPELL (other dependent spells will learned later as not auto-learned)
    // note: all spells with SPELL_EFFECT_LEARN_SPELL isn't passive
    if (!loading && talentCost > 0 && spellInfo->HasEffect(SPELL_EFFECT_LEARN_SPELL))
    {
        // ignore stance requirement for talent learn spell (stance set for spell only for client spell description show)
        CastSpell(this, spellId, true);
    }
    // also cast passive spells (including all talents without SPELL_EFFECT_LEARN_SPELL) with additional checks
    else if (spellInfo->IsPassive())
    {
        if (HandlePassiveSpellLearn(spellInfo))
            CastSpell(this, spellId, true);
    }
    else if (spellInfo->HasEffect(SPELL_EFFECT_SKILL_STEP))
    {
        CastSpell(this, spellId, true);
        return false;
    }

    // update used talent points count
    m_usedTalentCount += talentCost;
    // update free primary prof.points (if any, can be none in case GM .learn prof. learning)
    if (uint32 freeProfs = GetFreePrimaryProfessionPoints())
    {
        if (spellInfo->IsPrimaryProfessionFirstRank())
            SetFreePrimaryProfessions(freeProfs-1);
    }

    SkillLineAbilityMapBounds skill_bounds = sSpellMgr->GetSkillLineAbilityMapBounds(spellId);

    if (SpellLearnSkillNode const* spellLearnSkill = sSpellMgr->GetSpellLearnSkill(spellId))
    {
        // add dependent skills if this spell is not learned from adding skill already
        if (spellLearnSkill->skill != fromSkill)
        {
            uint32 skill_value = GetPureSkillValue(spellLearnSkill->skill);
            uint32 skill_max_value = GetPureMaxSkillValue(spellLearnSkill->skill);

            if (skill_value < spellLearnSkill->value)
                skill_value = spellLearnSkill->value;

            uint32 new_skill_max_value = spellLearnSkill->maxvalue == 0 ? GetMaxSkillValueForLevel() : spellLearnSkill->maxvalue;

            if (skill_max_value < new_skill_max_value)
                skill_max_value = new_skill_max_value;

            if (sWorld->getBoolConfig(CONFIG_ALWAYS_MAXSKILL) && !IsProfessionOrRidingSkill(spellLearnSkill->skill))
                skill_value = skill_max_value;

            SetSkill(spellLearnSkill->skill, spellLearnSkill->step, skill_value, skill_max_value);
        }
    }
    else
    {
        // not ranked skills
        for (SkillLineAbilityMap::const_iterator _spell_idx = skill_bounds.first; _spell_idx != skill_bounds.second; ++_spell_idx)
        {
            SkillLineEntry const* pSkill = sSkillLineStore.LookupEntry(_spell_idx->second->SkillLine);
            if (!pSkill)
                continue;

            if (pSkill->ID == fromSkill)
                continue;

            ///@todo: confirm if rogues start with lockpicking skill at level 1 but only receive the spell to use it at level 16
            // Also added for runeforging. It's already confirmed this happens upon learning for Death Knights, not from character creation.
            if ((_spell_idx->second->AcquireMethod == SKILL_LINE_ABILITY_LEARNED_ON_SKILL_LEARN && !HasSkill(pSkill->ID)) || ((pSkill->ID == SKILL_LOCKPICKING || pSkill->ID == SKILL_RUNEFORGING) && _spell_idx->second->TrivialSkillLineRankHigh == 0))
                LearnDefaultSkill(pSkill->ID, 0);

            if (pSkill->ID == SKILL_MOUNTS && !Has310Flyer(false))
                for (SpellEffectInfo const& spellEffectInfo : spellInfo->GetEffects())
                    if (spellEffectInfo.ApplyAuraName == SPELL_AURA_MOD_INCREASE_MOUNTED_FLIGHT_SPEED &&
                        spellEffectInfo.CalcValue() == 310)
                        SetHas310Flyer(true);
        }
    }

    // learn dependent spells
    SpellLearnSpellMapBounds spell_bounds = sSpellMgr->GetSpellLearnSpellMapBounds(spellId);

    for (SpellLearnSpellMap::const_iterator itr2 = spell_bounds.first; itr2 != spell_bounds.second; ++itr2)
    {
        if (!itr2->second.autoLearned)
        {
            if (!IsInWorld() || !itr2->second.active)       // at spells loading, no output, but allow save
                AddSpell(itr2->second.spell, itr2->second.active, true, true, false);
            else                                            // at normal learning
                LearnSpell(itr2->second.spell, true);
        }
    }

    if (!GetSession()->PlayerLoading())
    {
        // not ranked skills
        for (SkillLineAbilityMap::const_iterator _spell_idx = skill_bounds.first; _spell_idx != skill_bounds.second; ++_spell_idx)
        {
            UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_LEARN_SKILL_LINE, _spell_idx->second->SkillLine);
            UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_LEARN_SKILLLINE_SPELLS, _spell_idx->second->SkillLine);
        }

        UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_LEARN_SPELL, spellId);
    }

    // @tswow-begin
    FIRE_ID(spellInfo->events.id, Spell, OnLearn, TSSpellInfo(spellInfo), TSPlayer(this), active, disabled, superceded_old, fromSkill);
    // @tswow-end

    // return true (for send learn packet) only if spell active (in case ranked spells) and not replace old spell
    return active && !disabled && !superceded_old;
}

void Player::AddTemporarySpell(uint32 spellId)
{
    PlayerSpellMap::iterator itr = m_spells.find(spellId);
    // spell already added - do not do anything
    if (itr != m_spells.end())
        return;
    PlayerSpell* newspell = &m_spells[spellId];
    newspell->state     = PLAYERSPELL_TEMPORARY;
    newspell->active    = true;
    newspell->dependent = false;
    newspell->disabled  = false;
}

void Player::RemoveTemporarySpell(uint32 spellId)
{
    PlayerSpellMap::iterator itr = m_spells.find(spellId);
    // spell already not in list - do not do anything
    if (itr == m_spells.end())
        return;
    // spell has other state than temporary - do not change it
    if (itr->second.state != PLAYERSPELL_TEMPORARY)
        return;
    m_spells.erase(itr);
}

bool Player::HandlePassiveSpellLearn(SpellInfo const* spellInfo)
{
    // note: form passives activated with shapeshift spells be implemented by HandleShapeshiftBoosts instead of spell_learn_spell
    // talent dependent passives activated at form apply have proper stance data
    ShapeshiftForm form = GetShapeshiftForm();
    bool need_cast = (!spellInfo->Stances || (form && (spellInfo->Stances & (UI64LIT(1) << (form - 1)))) ||
        (!form && spellInfo->HasAttribute(SPELL_ATTR2_NOT_NEED_SHAPESHIFT)));

    // Check EquippedItemClass
    // passive spells which apply aura and have an item requirement are to be added manually, instead of casted
    if (spellInfo->EquippedItemClass >= 0)
    {
        for (SpellEffectInfo const& spellEffectInfo : spellInfo->GetEffects())
        {
            if (spellEffectInfo.IsAura())
            {
                if (!HasAura(spellInfo->Id) && HasItemFitToSpellRequirements(spellInfo))
                    AddAura(spellInfo->Id, this);
                return false;
            }
        }
    }

    //Check CasterAuraStates
    return need_cast && (!spellInfo->CasterAuraState || HasAuraState(AuraStateType(spellInfo->CasterAuraState)));
}

void Player::LearnSpell(uint32 spell_id, bool dependent, uint32 fromSkill /*= 0*/)
{
    PlayerSpellMap::iterator itr = m_spells.find(spell_id);

    bool disabled = (itr != m_spells.end()) ? itr->second.disabled : false;
    bool active = disabled ? itr->second.active : true;

    bool learning = AddSpell(spell_id, active, true, dependent, false, false, fromSkill);

    // prevent duplicated entires in spell book, also not send if not in world (loading)
    if (learning && IsInWorld())
    {
        WorldPacket data(SMSG_LEARNED_SPELL, 6);
        data << uint32(spell_id);
        data << uint16(0);
        SendDirectMessage(&data);
    }

    // learn all disabled higher ranks and required spells (recursive)
    if (disabled)
    {
        if (uint32 nextSpell = sSpellMgr->GetNextSpellInChain(spell_id))
        {
            PlayerSpellMap::iterator iter = m_spells.find(nextSpell);
            if (iter != m_spells.end() && iter->second.disabled)
                LearnSpell(nextSpell, false, fromSkill);
        }

        SpellsRequiringSpellMapBounds spellsRequiringSpell = sSpellMgr->GetSpellsRequiringSpellBounds(spell_id);
        for (SpellsRequiringSpellMap::const_iterator itr2 = spellsRequiringSpell.first; itr2 != spellsRequiringSpell.second; ++itr2)
        {
            PlayerSpellMap::iterator iter2 = m_spells.find(itr2->second);
            if (iter2 != m_spells.end() && iter2->second.disabled)
                LearnSpell(itr2->second, false, fromSkill);
        }
    }
}

void Player::RemoveSpell(uint32 spell_id, bool disabled, bool learn_low_rank)
{
    PlayerSpellMap::iterator itr = m_spells.find(spell_id);
    if (itr == m_spells.end())
        return;

    if (itr->second.state == PLAYERSPELL_REMOVED || (disabled && itr->second.disabled) || itr->second.state == PLAYERSPELL_TEMPORARY)
        return;

    // unlearn non talent higher ranks (recursive)
    if (uint32 nextSpell = sSpellMgr->GetNextSpellInChain(spell_id))
    {
        if (HasSpell(nextSpell) && !GetTalentSpellPos(nextSpell))
            RemoveSpell(nextSpell, disabled, false);
    }
    //unlearn spells dependent from recently removed spells
    SpellsRequiringSpellMapBounds spellsRequiringSpell = sSpellMgr->GetSpellsRequiringSpellBounds(spell_id);
    for (SpellsRequiringSpellMap::const_iterator itr2 = spellsRequiringSpell.first; itr2 != spellsRequiringSpell.second; ++itr2)
        RemoveSpell(itr2->second, disabled);

    // re-search, it can be corrupted in prev loop
    itr = m_spells.find(spell_id);
    if (itr == m_spells.end())
        return;                                             // already unleared

    bool giveTalentPoints = disabled || !itr->second.disabled;

    bool cur_active    = itr->second.active;
    bool cur_dependent = itr->second.dependent;

    if (disabled)
    {
        itr->second.disabled = disabled;
        if (itr->second.state != PLAYERSPELL_NEW)
            itr->second.state = PLAYERSPELL_CHANGED;
    }
    else
    {
        if (itr->second.state == PLAYERSPELL_NEW)
            m_spells.erase(itr);
        else
            itr->second.state = PLAYERSPELL_REMOVED;
    }

    RemoveOwnedAura(spell_id, GetGUID());

    // remove pet auras
    for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
        if (PetAura const* petSpell = sSpellMgr->GetPetAura(spell_id, i))
            RemovePetAura(petSpell);

    // free talent points
    uint32 talentCosts = GetTalentSpellCost(spell_id);
    if (talentCosts > 0 && giveTalentPoints)
    {
        if (talentCosts < m_usedTalentCount)
            m_usedTalentCount -= talentCosts;
        else
            m_usedTalentCount = 0;
    }

    // update free primary prof.points (if not overflow setting, can be in case GM use before .learn prof. learning)
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spell_id);
    if (spellInfo && spellInfo->IsPrimaryProfessionFirstRank())
    {
        uint32 freeProfs = GetFreePrimaryProfessionPoints()+1;
        if (freeProfs <= sWorld->getIntConfig(CONFIG_MAX_PRIMARY_TRADE_SKILL))
            SetFreePrimaryProfessions(freeProfs);
    }

    // remove dependent skill
    SpellLearnSkillNode const* spellLearnSkill = sSpellMgr->GetSpellLearnSkill(spell_id);
    if (spellLearnSkill)
    {
        uint32 prev_spell = sSpellMgr->GetPrevSpellInChain(spell_id);
        if (!prev_spell)                                    // first rank, remove skill
            SetSkill(spellLearnSkill->skill, 0, 0, 0);
        else
        {
            // search prev. skill setting by spell ranks chain
            SpellLearnSkillNode const* prevSkill = sSpellMgr->GetSpellLearnSkill(prev_spell);
            while (!prevSkill && prev_spell)
            {
                prev_spell = sSpellMgr->GetPrevSpellInChain(prev_spell);
                prevSkill = sSpellMgr->GetSpellLearnSkill(sSpellMgr->GetFirstSpellInChain(prev_spell));
            }

            if (!prevSkill)                                 // not found prev skill setting, remove skill
                SetSkill(spellLearnSkill->skill, 0, 0, 0);
            else                                            // set to prev. skill setting values
            {
                uint32 skill_value = GetPureSkillValue(prevSkill->skill);
                uint32 skill_max_value = GetPureMaxSkillValue(prevSkill->skill);

                if (skill_value > prevSkill->value)
                    skill_value = prevSkill->value;

                uint32 new_skill_max_value = prevSkill->maxvalue == 0 ? GetMaxSkillValueForLevel() : prevSkill->maxvalue;

                if (skill_max_value > new_skill_max_value)
                    skill_max_value = new_skill_max_value;

                SetSkill(prevSkill->skill, prevSkill->step, skill_value, skill_max_value);
            }
        }
    }
    else
    {
        // not ranked skills
        SkillLineAbilityMapBounds bounds = sSpellMgr->GetSkillLineAbilityMapBounds(spell_id);

        // most likely will never be used, haven't heard of cases where players unlearn a mount
        if (Has310Flyer(false) && spellInfo)
        {
            for (SkillLineAbilityMap::const_iterator _spell_idx = bounds.first; _spell_idx != bounds.second; ++_spell_idx)
            {
                SkillLineEntry const* pSkill = sSkillLineStore.LookupEntry(_spell_idx->second->SkillLine);
                if (!pSkill)
                    continue;

                if (_spell_idx->second->SkillLine == SKILL_MOUNTS)
                {
                    for (SpellEffectInfo const& spellEffectInfo : spellInfo->GetEffects())
                    {
                        if (spellEffectInfo.ApplyAuraName == SPELL_AURA_MOD_INCREASE_MOUNTED_FLIGHT_SPEED &&
                            spellEffectInfo.CalcValue() == 310)
                        {
                            Has310Flyer(true, spell_id);    // with true as first argument its also used to set/remove the flag
                            break;
                        }
                    }
                }
            }
        }
    }

    // remove dependent spells
    SpellLearnSpellMapBounds spell_bounds = sSpellMgr->GetSpellLearnSpellMapBounds(spell_id);

    for (SpellLearnSpellMap::const_iterator itr2 = spell_bounds.first; itr2 != spell_bounds.second; ++itr2)
        RemoveSpell(itr2->second.spell, disabled);

    // activate lesser rank in spellbook/action bar, and cast it if need
    bool prev_activate = false;
    bool needsUnlearnSpellsPacket = false;

    if (uint32 prev_id = sSpellMgr->GetPrevSpellInChain(spell_id))
    {
        // if talent then lesser rank also talent and need learn
        if (talentCosts)
        {
            // I cannot see why mangos has these lines.
            //if (learn_low_rank)
            //    learnSpell(prev_id, false);
        }
        // if ranked non-stackable spell: need activate lesser rank and update dendence state
        /// No need to check for spellInfo != nullptr here because if cur_active is true, then that means that the spell was already in m_spells, and only valid spells can be pushed there.
        else if (cur_active && !spellInfo->IsStackableWithRanks() && spellInfo->IsRanked())
        {
            // need manually update dependence state (learn spell ignore like attempts)
            PlayerSpellMap::iterator prev_itr = m_spells.find(prev_id);
            if (prev_itr != m_spells.end())
            {
                if (prev_itr->second.dependent != cur_dependent)
                {
                    prev_itr->second.dependent = cur_dependent;
                    if (prev_itr->second.state != PLAYERSPELL_NEW)
                        prev_itr->second.state = PLAYERSPELL_CHANGED;
                }

                // now re-learn if need re-activate
                if (!prev_itr->second.active && learn_low_rank)
                {
                    if (AddSpell(prev_id, true, false, prev_itr->second.dependent, prev_itr->second.disabled))
                    {
                        // downgrade spell ranks in spellbook and action bar
                        SendSupercededSpell(spell_id, prev_id);
                        needsUnlearnSpellsPacket = IsUnlearnSpellsPacketNeededForSpell(prev_id);
                        prev_activate = true;
                    }
                }
            }
        }
    }

    if (spell_id == 46917 && m_canTitanGrip)
    {
        RemoveAurasDueToSpell(m_titanGripPenaltySpellId);
        SetCanTitanGrip(false);
    }

    if (spell_id == 674 && m_canDualWield)
        SetCanDualWield(false);

    if (sWorld->getBoolConfig(CONFIG_OFFHAND_CHECK_AT_SPELL_UNLEARN))
        AutoUnequipOffhandIfNeed();

    if (needsUnlearnSpellsPacket)
        SendUnlearnSpells();

    // remove from spell book if not replaced by lesser rank
    if (!prev_activate)
    {
        WorldPacket data(SMSG_REMOVED_SPELL, 4);
        data << uint32(spell_id);
        SendDirectMessage(&data);
    }

    // @tswow-begin
    FIRE_ID(spellInfo->events.id, Spell, OnUnlearn, TSSpellInfo(spellInfo), TSPlayer(this), disabled, learn_low_rank);
    // @tswow-end
}

bool Player::Has310Flyer(bool checkAllSpells, uint32 excludeSpellId)
{
    if (!checkAllSpells)
        return (m_ExtraFlags & PLAYER_EXTRA_HAS_310_FLYER) != 0;
    else
    {
        SetHas310Flyer(false);
        SpellInfo const* spellInfo;
        for (PlayerSpellMap::iterator itr = m_spells.begin(); itr != m_spells.end(); ++itr)
        {
            if (itr->first == excludeSpellId)
                continue;

            SkillLineAbilityMapBounds bounds = sSpellMgr->GetSkillLineAbilityMapBounds(itr->first);
            for (SkillLineAbilityMap::const_iterator _spell_idx = bounds.first; _spell_idx != bounds.second; ++_spell_idx)
            {
                if (_spell_idx->second->SkillLine != SKILL_MOUNTS)
                    break;  // We can break because mount spells belong only to one skillline (at least 310 flyers do)

                spellInfo = sSpellMgr->AssertSpellInfo(itr->first);
                for (SpellEffectInfo const& spellEffectInfo : spellInfo->GetEffects())
                {
                    if (spellEffectInfo.ApplyAuraName == SPELL_AURA_MOD_INCREASE_MOUNTED_FLIGHT_SPEED &&
                        spellEffectInfo.CalcValue() == 310)
                    {
                        SetHas310Flyer(true);
                        return true;
                    }
                }
            }
        }
    }

    return false;
}

void Player::RemoveArenaSpellCooldowns(bool removeActivePetCooldowns)
{
    // remove cooldowns on spells that have < 10 min CD
    GetSpellHistory()->ResetCooldowns([](SpellHistory::CooldownStorageType::iterator itr) -> bool
    {
        SpellInfo const* spellInfo = sSpellMgr->AssertSpellInfo(itr->first);
        int32 cooldown = -1;
        int32 categoryCooldown = -1;
        SpellHistory::GetCooldownDurations(spellInfo, itr->second.ItemId, &cooldown, nullptr, &categoryCooldown);
        return cooldown < 10 * MINUTE * IN_MILLISECONDS && categoryCooldown < 10 * MINUTE * IN_MILLISECONDS;
    }, true);

    // pet cooldowns
    if (removeActivePetCooldowns)
        if (Pet* pet = GetPet())
            pet->GetSpellHistory()->ResetAllCooldowns();
}

bool Player::HasSpell(uint32 spell) const
{
    PlayerSpellMap::const_iterator itr = m_spells.find(spell);
    return (itr != m_spells.end() && itr->second.state != PLAYERSPELL_REMOVED &&
        !itr->second.disabled);
}

bool Player::HasActiveSpell(uint32 spell) const
{
    PlayerSpellMap::const_iterator itr = m_spells.find(spell);
    return (itr != m_spells.end() && itr->second.state != PLAYERSPELL_REMOVED &&
        itr->second.active && !itr->second.disabled);
}

void Player::CastAllObtainSpells()
{
    for (uint8 slot = INVENTORY_SLOT_ITEM_START; slot < INVENTORY_SLOT_ITEM_END; ++slot)
        if (Item* item = GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
            ApplyItemObtainSpells(item, true);

    for (uint8 i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; ++i)
    {
        Bag* bag = GetBagByPos(i);
        if (!bag)
            continue;

        for (uint32 slot = 0; slot < bag->GetBagSize(); ++slot)
            if (Item* item = bag->GetItemByPos(slot))
                ApplyItemObtainSpells(item, true);
    }
}

void Player::ApplyItemObtainSpells(Item* item, bool apply)
{
    ItemTemplate const* itemTemplate = item->GetTemplate();
    for (uint8 i = 0; i < MAX_ITEM_PROTO_SPELLS; ++i)
    {
        if (itemTemplate->Spells[i].SpellTrigger != ITEM_SPELLTRIGGER_ON_NO_DELAY_USE) // On obtain trigger
            continue;

        int32 const spellId = itemTemplate->Spells[i].SpellId;
        if (spellId <= 0)
            continue;

        if (apply)
        {
            if (!HasAura(spellId))
                CastSpell(this, spellId, item);
        }
        else
            RemoveAurasDueToSpell(spellId);
    }
}

void Player::ResetSpells(bool myClassOnly)
{
    // not need after this call
    if (HasAtLoginFlag(AT_LOGIN_RESET_SPELLS))
        RemoveAtLoginFlag(AT_LOGIN_RESET_SPELLS, true);

    // make full copy of map (spells removed and marked as deleted at another spell remove
    // and we can't use original map for safe iterative with visit each spell at loop end
    PlayerSpellMap smap = GetSpellMap();

    uint32 family;

    if (myClassOnly)
    {
        ChrClassesEntry const* clsEntry = sChrClassesStore.LookupEntry(GetClass());
        if (!clsEntry)
            return;
        family = clsEntry->SpellClassSet;

        for (PlayerSpellMap::const_iterator iter = smap.begin(); iter != smap.end(); ++iter)
        {
            SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(iter->first);
            if (!spellInfo)
                continue;

            // skip server-side/triggered spells
            if (spellInfo->SpellLevel == 0)
                continue;

            // skip wrong class/race skills
            if (!IsSpellFitByClassAndRace(spellInfo->Id))
                continue;

            // skip other spell families
            if (spellInfo->SpellFamilyName != family)
                continue;

            // skip spells with first rank learned as talent (and all talents then also)
            uint32 firstRank = spellInfo->GetFirstRankSpell()->Id;
            if (GetTalentSpellCost(firstRank) > 0)
                continue;

            // skip broken spells
            if (!SpellMgr::IsSpellValid(spellInfo, this, false))
                continue;
        }
    }
    else
        for (PlayerSpellMap::const_iterator iter = smap.begin(); iter != smap.end(); ++iter)
            RemoveSpell(iter->first, false, false);           // only iter->first can be accessed, object by iter->second can be deleted already

    LearnDefaultSkills();
    LearnCustomSpells();
    LearnQuestRewardedSpells();
}

void Player::LearnCustomSpells()
{
    if (!sWorld->getBoolConfig(CONFIG_START_ALL_SPELLS))
        return;

    // learn default race/class spells
    PlayerInfo const* info = sObjectMgr->GetPlayerInfo(GetRace(), GetClass());
    ASSERT(info);
    for (PlayerCreateInfoSpells::const_iterator itr = info->customSpells.begin(); itr != info->customSpells.end(); ++itr)
    {
        uint32 tspell = *itr;
        TC_LOG_DEBUG("entities.player.loading", "Player::LearnCustomSpells: Player '{}' ({}, Class: {} Race: {}): Adding initial spell (SpellID: {})",
            GetName(), GetGUID().ToString(), uint32(GetClass()), uint32(GetRace()), tspell);
        if (!IsInWorld())                                    // will send in INITIAL_SPELLS in list anyway at map add
            AddSpell(tspell, true, true, true, false);
        else                                                // but send in normal spell in game learn case
            LearnSpell(tspell, true);
    }
}

void Player::LearnQuestRewardedSpells(Quest const* quest)
{
    int32 spell_id = quest->GetRewSpellCast();
    uint32 src_spell_id = quest->GetSrcSpell();

    // skip quests without rewarded spell
    if (!spell_id)
        return;

    // if RewSpellCast = -1 we remove aura do to SrcSpell from player.
    if (spell_id == -1 && src_spell_id)
    {
        RemoveAurasDueToSpell(src_spell_id);
        return;
    }

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spell_id);
    if (!spellInfo)
        return;

    // check learned spells state
    bool found = false;
    for (SpellEffectInfo const& spellEffectInfo : spellInfo->GetEffects())
    {
        if (spellEffectInfo.IsEffect(SPELL_EFFECT_LEARN_SPELL) && !HasSpell(spellEffectInfo.TriggerSpell))
        {
            found = true;
            break;
        }
    }

    // skip quests with not teaching spell or already known spell
    if (!found)
        return;

    uint32 learned_0 = spellInfo->GetEffect(EFFECT_0).TriggerSpell;
    if (!HasSpell(learned_0))
    {
        SpellInfo const* learnedInfo = sSpellMgr->GetSpellInfo(learned_0);
        if (!learnedInfo)
            return;

       // profession specialization can be re-learned from npc
       if (learnedInfo->GetEffect(EFFECT_0).Effect == SPELL_EFFECT_TRADE_SKILL && learnedInfo->GetEffect(EFFECT_1).Effect == 0 && !learnedInfo->SpellLevel)
           return;
    }

    CastSpell(this, spell_id, true);
}

void Player::LearnQuestRewardedSpells()
{
    // learn spells received from quest completing
    for (RewardedQuestSet::const_iterator itr = m_RewardedQuests.begin(); itr != m_RewardedQuests.end(); ++itr)
    {
        Quest const* quest = sObjectMgr->GetQuestTemplate(*itr);
        if (!quest)
            continue;

        LearnQuestRewardedSpells(quest);
    }
}

void Player::LearnSkillRewardedSpells(uint32 skillId, uint32 skillValue)
{
    uint32 raceMask  = GetRaceMask();
    uint32 classMask = GetClassMask();
    std::vector<SkillLineAbilityEntry const*> const* skillLineAbilities = GetSkillLineAbilitiesBySkill(skillId);
    if (!skillLineAbilities)
        return;

    for (SkillLineAbilityEntry const* ability : *skillLineAbilities)
    {
        if (!sSpellMgr->GetSpellInfo(ability->Spell))
            continue;

        if (ability->AcquireMethod != SKILL_LINE_ABILITY_LEARNED_ON_SKILL_VALUE && ability->AcquireMethod != SKILL_LINE_ABILITY_LEARNED_ON_SKILL_LEARN)
            continue;

        // Check race if set
        if (ability->RaceMask && !(ability->RaceMask & raceMask))
            continue;

        // Check class if set
        if (ability->ClassMask && !(ability->ClassMask & classMask))
            continue;

        // need unlearn spell
        if (skillValue < ability->MinSkillLineRank && ability->AcquireMethod == SKILL_LINE_ABILITY_LEARNED_ON_SKILL_VALUE)
            RemoveSpell(ability->Spell);
        // need learn
        else
        {
            // used to avoid double Seal of Righteousness on paladins, it's the only player spell which has both spell and forward spell in auto learn
            if (ability->AcquireMethod == SKILL_LINE_ABILITY_LEARNED_ON_SKILL_LEARN && ability->SupercededBySpell)
            {
                bool skipCurrent = false;
                auto bounds = sSpellMgr->GetSkillLineAbilityMapBounds(ability->SupercededBySpell);
                for (auto itr = bounds.first; itr != bounds.second; ++itr)
                {
                    if (itr->second->AcquireMethod == SKILL_LINE_ABILITY_LEARNED_ON_SKILL_LEARN && skillValue >= itr->second->MinSkillLineRank)
                    {
                        skipCurrent = true;
                        break;
                    }
                }

                if (skipCurrent)
                    continue;
            }

            if (!IsInWorld())
                AddSpell(ability->Spell, true, true, true, false, false, ability->SkillLine);
            else
                LearnSpell(ability->Spell, true, ability->SkillLine);
        }
    }
}

bool Player::CanExecutePendingSpellCastRequest(SpellInfo const* spellInfo, bool without_queue/* = false*/)
{
  PendingSpellCastRequest* request = GetCastRequest(spellInfo->StartRecoveryCategory);

    // already queued spell
    if (!without_queue)
    {
        if (request)
        {
            if (!request->active)
                return false;
            if (!request->spell_id)
                return false;
        }
    }

    // gcd
    if (GetSpellHistory()->GetRemainingGlobalCooldown(spellInfo) > uint32(0))
    {
        if (request && !without_queue)
            if (GetSpellHistory()->GetRemainingGlobalCooldown(spellInfo) > SPELL_QUEUE_TIME_WINDOW)
                CancelPendingCastRequest(spellInfo->StartRecoveryCategory);

        // TC_LOG_DEBUG("misc", "global cooldown for {} is {}", spellInfo->Id, GetSpellHistory()->GetGlobalCooldown(spellInfo));
        return false;
    }

    // spell cooldown
    if (GetSpellHistory()->GetRemainingCooldown(spellInfo) > (without_queue ? 0 : SPELL_QUEUE_TIME_WINDOW))
        return false;

    // spell in progress
    for (CurrentSpellTypes spellSlot : {CURRENT_MELEE_SPELL, CURRENT_GENERIC_SPELL})
        if (Spell *spell = GetCurrentSpell(spellSlot))
        {
            bool autoshot = false;

            if (request && request->spell_id)
                if (const SpellInfo* info = sSpellMgr->GetSpellInfo(request->spell_id))
                    if (info->IsAutoRepeatRangedSpell())
                        autoshot = true;

            if (IsNonMeleeSpellCast(false, true, true, autoshot))
            {
                // TC_LOG_DEBUG("misc", "CanExecutePendingSpellCastRequest false 3");
                return false;
            }
        }
    TC_LOG_DEBUG("misc", "CanExecutePendingSpellCastRequest {}) returns true", !without_queue);
    return true;
}

bool Player::IsSpellQueueEnabled() const
{
    return true;
}

void Player::RequestSpellCast(PendingSpellCastRequest castRequest, SpellInfo const* spellInfo)
{
    // We are overriding an already existing spell cast request so inform the client that the old cast is being replaced
    if (PendingSpellCastRequest* request = GetCastRequest(spellInfo))
        if (request->active)
            ClearCastRequest(spellInfo);

    m_pendingCasts[spellInfo->StartRecoveryCategory] = castRequest;
}

void Player::SetPendingCastRequest(PendingSpellCastRequest new_request)
{
    const SpellInfo* info = sSpellMgr->GetSpellInfo(new_request.spell_id);
    if (IsSpellQueueEnabled())
        CancelPendingCastRequest(info->StartRecoveryCategory);
    else
    {
        m_pendingCasts[info->StartRecoveryCategory].active             = true;
        m_pendingCasts[info->StartRecoveryCategory].request_packet     = new_request.request_packet;
        m_pendingCasts[info->StartRecoveryCategory].spell_id           = new_request.spell_id;
        m_pendingCasts[info->StartRecoveryCategory].time_requested     = new_request.time_requested;
        m_pendingCasts[info->StartRecoveryCategory].cast_count         = new_request.cast_count;
        m_pendingCasts[info->StartRecoveryCategory].cancel_in_progress = false;
    }
    return;
}

void Player::CancelPendingCastRequest(uint32 category)
{
    PendingSpellCastRequest* request = GetCastRequest(category);
    if (!request)
    {
        TC_LOG_DEBUG("misc", "CancelPendingCastRequest category {}", category);
        return;
    }
    else
    {
        WorldPacket packet = m_pendingCasts[category].request_packet;
        if (WorldSession* session = GetSession())
        {
            m_pendingCasts[category].cancel_in_progress = true;
            if (request->is_item)
                session->HandleUseItemOpcode(packet);
            else
                session->HandleCastSpellOpcode(packet);
        }
        ClearCastRequest(category);
    }
}

void Player::CancelPendingCastRequests()
{
    if (m_pendingCasts.size())
    {
        for (auto i = m_pendingCasts.begin(); i != m_pendingCasts.end(); ++i)
        {
            // if (uint32 category = i->first)
            uint32 category = i->first; // keys cannot be null, item category is 0
            CancelPendingCastRequest(category);
        }
    }
}

PendingSpellCastRequest* Player::GetCastRequest(SpellInfo const* spellInfo) const
{
    if (spellInfo)
        return GetCastRequest(spellInfo->StartRecoveryCategory);
    return nullptr;
};

PendingSpellCastRequest* Player::GetCastRequest(uint32 gcd_category) const
{
    auto itr = m_pendingCasts.find(gcd_category);
    if (itr != m_pendingCasts.end())
    {
      const PendingSpellCastRequest* request = (&itr->second);
      return (PendingSpellCastRequest *const)request;
    }
    return nullptr;
};

void Player::ClearCastRequest(SpellInfo const* info)
{
    if (info)
        ClearCastRequest(info->StartRecoveryCategory);
}

void Player::ClearCastRequest(uint32 category)
{
    auto itr = m_pendingCasts.find(category);
    if (itr != m_pendingCasts.end())
        m_pendingCasts.erase(itr->first);
}

bool Player::CanRequestSpellCast(SpellInfo const* spellInfo) const
{
    if (!IsSpellQueueEnabled())
        return false;

    if (PendingSpellCastRequest* castRequest = GetCastRequest(spellInfo))
        if (castRequest->active)
            return false;

    if (GetSpellHistory()->GetRemainingGlobalCooldown(spellInfo) > SPELL_QUEUE_TIME_WINDOW)
        return false;

    if (GetSpellHistory()->GetRemainingCooldown(spellInfo) > SPELL_QUEUE_TIME_WINDOW)
        return false;

    for (CurrentSpellTypes spellSlot : { CURRENT_MELEE_SPELL, CURRENT_GENERIC_SPELL })
        if (Spell* spell = GetCurrentSpell(spellSlot))
            if (spell->GetCastTimeRemaining(true) > SPELL_QUEUE_TIME_WINDOW)
                return false;

    return true;
}

void Player::ProcessPendingSpellCastRequest(uint32 category)
{
    PendingSpellCastRequest* request = GetCastRequest(category);
    if (!request || !request->active)
        return;
    if (!CanExecutePendingSpellCastRequest(sSpellMgr->GetSpellInfo(request->spell_id)))
        return;

    WorldPacket packet = request->request_packet;
    if (packet.empty())
    {
        CancelPendingCastRequest(category);
        return;
    }

    if (WorldSession* session = GetSession())
    {
        // AddSameTickQueueBlock(category);
        if (request->is_item)
            session->HandleUseItemOpcode(packet);
        else
            session->HandleCastSpellOpcode(packet);

        ClearCastRequest(category);
    }
}

void Player::RemoveSameTickQueueBlock(uint32 category)
{
    if (m_SameTickBlockList.size())
    {
        if (m_SameTickBlockList.find(category) != m_SameTickBlockList.end())
        {
            m_SameTickBlockList.erase(category);
            TC_LOG_DEBUG("misc", "removing same tick block from category {} at {}", category, getMSTime());
        }
    }
}

void Player::AddSameTickQueueBlock(uint32 category)
{
    TC_LOG_DEBUG("misc", "adding same tick block to category {} at {}", category, getMSTime());
    m_SameTickBlockList[category] = getMSTime();
}

bool Player::HasSameTickQueueBlock(uint32 category, bool ignore_time) const
{
    if (m_SameTickBlockList.size())
    {
        if (m_SameTickBlockList.find(category) != m_SameTickBlockList.end())
        {
            auto block = m_SameTickBlockList.find(category);
            uint32 time = block->second;
            if (ignore_time || (GetMSTimeDiffToNow(time) < 140))
                return true;
        }
    }
    return false;
}

void Player::ExecuteSortedCastRequests()
{
    std::multimap<uint32, uint32> organized_list;

    if (m_pendingCasts.size())
    {
        for (auto i = m_pendingCasts.begin(); i != m_pendingCasts.end(); ++i)
        {
                organized_list.insert({ i->second.time_requested, i->first });
        }
    }

    if (organized_list.size())
    {
        for (auto i = organized_list.begin(); i != organized_list.end(); ++i)
        {
            // PendingSpellCastRequest* request = GetCastRequest(i->second);
            // if (request)
            // {
                // TC_LOG_DEBUG("misc", "time: {}, spell: {}", i->first, request->spell_id);
            // }
            ProcessPendingSpellCastRequest(i->second);
        }
    }
}

// @tswow-begin
void Player::ApplyAutolearnSpells(uint32 fromLevel)
{
    uint32 level = GetLevel();
    if (fromLevel > level) return;

    SpellAutoLearns spells = sObjectMgr->GetSpellAutolearns();
    for (size_t i = fromLevel; i <= level; ++i)
    {
        if (spells.size() <= i) break;
        for (auto const& sal : spells[i])
        {
            if (
                (sal.classmask == 0 || this->GetClassMask() & sal.classmask)
                &&
                (sal.racemask == 0 || this->GetRaceMask() & sal.racemask)
                )
            {
                LearnSpell(sal.spell, false);
            }
        }
    }
}
// @tswow-end

void Player::ApplyItemEquipSpell(Item* item, bool apply, bool form_change)
{
    if (!item)
        return;

    ItemTemplate const* proto = item->GetTemplate();
    if (!proto)
        return;

    for (uint8 i = 0; i < MAX_ITEM_PROTO_SPELLS; ++i)
    {
        _Spell const& spellData = proto->Spells[i];

        // no spell
        if (spellData.SpellId <= 0)
            continue;

        // wrong triggering type
        if (apply && spellData.SpellTrigger != ITEM_SPELLTRIGGER_ON_EQUIP)
            continue;

        // check if it is valid spell
        SpellInfo const* spellproto = sSpellMgr->GetSpellInfo(spellData.SpellId);
        if (!spellproto)
            continue;

        ApplyEquipSpell(spellproto, item, apply, form_change);
    }
}

void Player::ApplyEquipSpell(SpellInfo const* spellInfo, Item* item, bool apply, bool form_change)
{
    if (apply)
    {
        // Cannot be used in this stance/form
        if (spellInfo->CheckShapeshift(GetShapeshiftForm()) != SPELL_CAST_OK)
            return;

        if (form_change)                                    // check aura active state from other form
        {
            AuraApplicationMapBounds range = GetAppliedAuras().equal_range(spellInfo->Id);
            for (AuraApplicationMap::const_iterator itr = range.first; itr != range.second; ++itr)
                if (!item || itr->second->GetBase()->GetCastItemGUID() == item->GetGUID())
                    return;
        }

        TC_LOG_DEBUG("entities.player", "Player::ApplyEquipSpell: Player '{}' ({}) cast {} equip spell (ID: {})",
            GetName(), GetGUID().ToString(), (item ? "item" : "itemset"), spellInfo->Id);

        CastSpell(this, spellInfo->Id, item);
    }
    else
    {
        if (form_change)                                     // check aura compatibility
        {
            // Cannot be used in this stance/form
            if (spellInfo->CheckShapeshift(GetShapeshiftForm()) == SPELL_CAST_OK)
                return;                                     // and remove only not compatible at form change
        }

        if (item)
            RemoveAurasDueToItemSpell(spellInfo->Id, item->GetGUID());  // un-apply all spells, not only at-equipped
        else
            RemoveAurasDueToSpell(spellInfo->Id);           // un-apply spell (item set case)
    }
}

void Player::UpdateEquipSpellsAtFormChange()
{
    for (uint8 i = 0; i < INVENTORY_SLOT_BAG_END; ++i)
    {
        if (m_items[i] && !m_items[i]->IsBroken() && CanUseAttackType(GetAttackBySlot(i)))
        {
            ApplyItemEquipSpell(m_items[i], false, true);     // remove spells that not fit to form
            ApplyItemEquipSpell(m_items[i], true, true);      // add spells that fit form but not active
        }
    }

    // item set bonuses not dependent from item broken state
    for (size_t setindex = 0; setindex < ItemSetEff.size(); ++setindex)
    {
        ItemSetEffect* eff = ItemSetEff[setindex];
        if (!eff)
            continue;

        for (uint32 y = 0; y < MAX_ITEM_SET_SPELLS; ++y)
        {
            SpellInfo const* spellInfo = eff->spells[y];
            if (!spellInfo)
                continue;

            ApplyEquipSpell(spellInfo, nullptr, false, true);       // remove spells that not fit to form
            ApplyEquipSpell(spellInfo, nullptr, true, true);        // add spells that fit form but not active
        }
    }
}

bool Player::IsSpellFitByClassAndRace(uint32 spell_id) const
{
    uint32 racemask  = GetRaceMask();
    uint32 classmask = GetClassMask();

    SkillLineAbilityMapBounds bounds = sSpellMgr->GetSkillLineAbilityMapBounds(spell_id);
    if (bounds.first == bounds.second)
        return true;

    for (SkillLineAbilityMap::const_iterator _spell_idx = bounds.first; _spell_idx != bounds.second; ++_spell_idx)
    {
        // skip wrong race skills
        if (_spell_idx->second->RaceMask && (_spell_idx->second->RaceMask & racemask) == 0)
            continue;

        // skip wrong class skills
        if (_spell_idx->second->ClassMask && (_spell_idx->second->ClassMask & classmask) == 0)
            continue;

        // skip wrong class and race skill saved in SkillRaceClassInfo.dbc
        if (!GetSkillRaceClassInfo(_spell_idx->second->SkillLine, GetRace(), GetClass()))
            continue;

        return true;
    }

    return false;
}

bool Player::IsAffectedBySpellmod(SpellInfo const* spellInfo, SpellModifier* mod, Spell* spell)
{
    if (!mod || !spellInfo)
        return false;

    // First time this aura applies a mod to us and is out of charges
    if (spell && mod->ownerAura->IsUsingCharges() && !mod->ownerAura->GetCharges() && !spell->m_appliedMods.count(mod->ownerAura))
        return false;

    // +duration to infinite duration spells making them limited
    if (mod->op == SPELLMOD_DURATION && spellInfo->GetDuration() == -1)
        return false;

    // mod crit to spells that can't crit
    if (mod->op == SPELLMOD_CRITICAL_CHANCE && !spellInfo->HasAttribute(SPELL_ATTR0_CU_CAN_CRIT))
        return false;

    return spellInfo->IsAffectedBySpellMod(mod);
}

template <class T>
void Player::ApplySpellMod(uint32 spellId, SpellModOp op, T& basevalue, Spell* spell /*= nullptr*/) const
{
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
    if (!spellInfo)
        return;

    float totalmul = 1.0f;
    int32 totalflat = 0;

    auto calculateSpellMod = [&](SpellModifier* mod)
    {
        switch (mod->type)
        {
            case SPELLMOD_FLAT:
                totalflat += mod->value;
                break;
            case SPELLMOD_PCT:
                // special case (skip > 10sec spell casts for instant cast setting)
                if (op == SPELLMOD_CASTING_TIME && mod->value <= -100 && basevalue >= T(10000))
                    return;
                else if (!Player::HasSpellModApplied(mod, spell))
                {
                    // Special case for Surge of Light, do not apply critical chance reduction if others mods was not applied (i.e. procs while casting another spell)
                    // (Surge of Light is the only PCT_MOD on critical chance)
                    if (op == SPELLMOD_CRITICAL_CHANCE)
                        return;
                    // Special case for Backdraft: do not apply GCD reduction if cast time reduction was not applied (i.e. when Backlash is consumed first).
                    // (Backdraft is the only PCT_MOD on global cooldown)
                    else if (op == SPELLMOD_GLOBAL_COOLDOWN)
                        return;
                }

                totalmul += CalculatePct(1.0f, mod->value);
                break;
        }

        Player::ApplyModToSpell(mod, spell);
    };

    // Drop charges for triggering spells instead of triggered ones
    if (m_spellModTakingSpell)
        spell = m_spellModTakingSpell;

    SpellModifier* chargedMod = nullptr;
    for (SpellModifier* mod : m_spellMods[op])
    {
        if (!IsAffectedBySpellmod(spellInfo, mod, spell))
            continue;

        if (mod->ownerAura->IsUsingCharges())
        {
            if (!chargedMod || (chargedMod->ownerAura->GetSpellInfo()->Priority < mod->ownerAura->GetSpellInfo()->Priority))
                chargedMod = mod;
            continue;
        }

        calculateSpellMod(mod);
    }

    if (chargedMod)
        calculateSpellMod(chargedMod);

    basevalue = T(float(basevalue + totalflat) * totalmul);
}

template TC_GAME_API void Player::ApplySpellMod(uint32 spellId, SpellModOp op, int32& basevalue, Spell* spell) const;
template TC_GAME_API void Player::ApplySpellMod(uint32 spellId, SpellModOp op, uint32& basevalue, Spell* spell) const;
template TC_GAME_API void Player::ApplySpellMod(uint32 spellId, SpellModOp op, float& basevalue, Spell* spell) const;

void Player::AddSpellMod(SpellModifier* mod, bool apply)
{
    TC_LOG_DEBUG("spells", "Player::AddSpellMod: Player '{}' ({}), SpellID: {}", GetName(), GetGUID().ToString(), mod->spellId);
    uint16 Opcode = (mod->type == SPELLMOD_FLAT) ? SMSG_SET_FLAT_SPELL_MODIFIER : SMSG_SET_PCT_SPELL_MODIFIER;

    flag96 modMask;
    for (uint8 i = 0; i < 3; ++i)
    {
        for (uint32 eff = 0; eff < 32; ++eff)
        {
            modMask[i] = uint32(1) << eff;
            if ((mod->mask & modMask))
            {
                int32 val = 0;
                for (SpellModifier* spellMod : m_spellMods[mod->op])
                {
                    if (spellMod->type == mod->type && (spellMod->mask & modMask))
                        val += spellMod->value;
                }
                val += apply ? mod->value : -(mod->value);
                WorldPacket data(Opcode, (1 + 1 + 4));
                data << uint8(eff + 32 * i);
                data << uint8(mod->op);
                data << int32(val);
                SendDirectMessage(&data);
            }
        }

        modMask[i] = 0;
    }

    if (apply)
        m_spellMods[mod->op].insert(mod);
    else
        m_spellMods[mod->op].erase(mod);
}

void Player::ApplyModToSpell(SpellModifier* mod, Spell* spell)
{
    if (!spell)
        return;

    // don't do anything with no charges
    if (mod->ownerAura->IsUsingCharges() && !mod->ownerAura->GetCharges())
        return;

    // register inside spell, proc system uses this to drop charges
    spell->m_appliedMods.insert(mod->ownerAura);
}

bool Player::HasSpellModApplied(SpellModifier* mod, Spell* spell)
{
    if (!spell)
        return false;

    return spell->m_appliedMods.count(mod->ownerAura) != 0;
}

void Player::SetSpellModTakingSpell(Spell* spell, bool apply)
{
    if (apply && m_spellModTakingSpell != nullptr)
        return;

    if (!apply && (!m_spellModTakingSpell || m_spellModTakingSpell != spell))
        return;

    m_spellModTakingSpell = apply ? spell : nullptr;
}

// send Proficiency
void Player::SendProficiency(ItemClass itemClass, uint32 itemSubclassMask) const
{
    WorldPacket data(SMSG_SET_PROFICIENCY, 1 + 4);
    data << uint8(itemClass) << uint32(itemSubclassMask);
    SendDirectMessage(&data);
}