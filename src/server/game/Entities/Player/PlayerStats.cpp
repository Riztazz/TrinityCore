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

void Player::SendLogXPGain(uint32 GivenXP, Unit* victim, uint32 BonusXP, bool recruitAFriend, float /*group_rate*/) const
{
    WorldPacket data(SMSG_LOG_XPGAIN, 21); // guess size?
    data << uint64(victim ? victim->GetGUID() : ObjectGuid::Empty);
    data << uint32(GivenXP + BonusXP);                      // given experience
    data << uint8(victim ? 0 : 1);                          // 00-kill_xp type, 01-non_kill_xp type

    if (victim)
    {
        data << uint32(GivenXP);                            // experience without bonus

        // should use group_rate here but can't figure out how
        data << float(1);                                   // 1 - none 0 - 100% group bonus output
    }

    data << uint8(recruitAFriend ? 1 : 0);                  // does the GivenXP include a RaF bonus?
    SendDirectMessage(&data);
}

void Player::GiveXP(uint32 xp, Unit* victim, float group_rate)
{
    if (xp < 1)
        return;

    if (!IsAlive() && !GetBattlegroundId())
        return;

    if (HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_NO_XP_GAIN))
        return;

    if (victim && victim->GetTypeId() == TYPEID_UNIT && !victim->ToCreature()->hasLootRecipient())
        return;

    uint8 level = GetLevel();

    sScriptMgr->OnGivePlayerXP(this, xp, victim);

    // XP to money conversion processed in Player::RewardQuest
    if (IsMaxLevel())
        return;

    uint32 bonus_xp;
    bool recruitAFriend = GetsRecruitAFriendBonus(true);

    // RaF does NOT stack with rested experience
    if (recruitAFriend)
        bonus_xp = 2 * xp; // xp + bonus_xp must add up to 3 * xp for RaF; calculation for quests done client-side
    else
        bonus_xp = victim ? GetXPRestBonus(xp) : 0; // XP resting bonus

    SendLogXPGain(xp, victim, bonus_xp, recruitAFriend, group_rate);

    uint32 nextLvlXP = GetUInt32Value(PLAYER_NEXT_LEVEL_XP);
    uint32 newXP = GetXP() + xp + bonus_xp;

    while (newXP >= nextLvlXP && !IsMaxLevel())
    {
        newXP -= nextLvlXP;

        if (!IsMaxLevel())
            GiveLevel(level + 1);

        level = GetLevel();
        nextLvlXP = GetUInt32Value(PLAYER_NEXT_LEVEL_XP);
    }

    SetXP(newXP);
}

// Update player to next level
// Current player experience not update (must be update by caller)
void Player::GiveLevel(uint8 level)
{
    uint8 oldLevel = GetLevel();
    if (level == oldLevel)
        return;

    if (Guild* guild = GetGuild())
        guild->UpdateMemberData(this, GUILD_MEMBER_DATA_LEVEL, level);

    PlayerLevelInfo info;
    sObjectMgr->GetPlayerLevelInfo(GetRace(), GetClass(), level, &info);

    PlayerClassLevelInfo classInfo;
    sObjectMgr->GetPlayerClassLevelInfo(GetClass(), level, &classInfo);

    WorldPackets::Misc::LevelUpInfo packet;
    packet.Level = level;
    packet.HealthDelta = int32(classInfo.basehealth) - int32(GetCreateHealth());

    /// @todo find some better solution
    // for (int i = 0; i < MAX_POWERS; ++i)
    packet.PowerDelta[0] = int32(classInfo.basemana) - int32(GetCreateMana());
    packet.PowerDelta[1] = 0;
    packet.PowerDelta[2] = 0;
    packet.PowerDelta[3] = 0;
    packet.PowerDelta[4] = 0;
    packet.PowerDelta[5] = 0;

    for (uint8 i = STAT_STRENGTH; i < MAX_STATS; ++i)
        packet.StatDelta[i] = int32(info.stats[i]) - GetCreateStat(Stats(i));

    SendDirectMessage(packet.Write());

    SetUInt32Value(PLAYER_NEXT_LEVEL_XP, sObjectMgr->GetXPForLevel(level));

    //update level, max level of skills
    m_Played_time[PLAYED_TIME_LEVEL] = 0;                   // Level Played Time reset

    _ApplyAllLevelScaleItemMods(false);

    SetLevel(level);

    UpdateSkillsForLevel();

    // save base values (bonuses already included in stored stats
    for (uint8 i = STAT_STRENGTH; i < MAX_STATS; ++i)
        SetCreateStat(Stats(i), info.stats[i]);

    SetCreateHealth(classInfo.basehealth);
    SetCreateMana(classInfo.basemana);

    InitTalentForLevel();
    InitTaxiNodesForLevel();
    InitGlyphsForLevel();

    UpdateAllStats();

    if (sWorld->getBoolConfig(CONFIG_ALWAYS_MAXSKILL)) // Max weapon skill when leveling up
        UpdateWeaponsSkillsToMaxSkillsForLevel();

    _ApplyAllLevelScaleItemMods(true);

    // set current level health and mana/energy to maximum after applying all mods.
    SetFullHealth();
    SetFullPower(POWER_MANA);

    // update level to hunter/summon pet
    if (Pet* pet = GetPet())
        pet->SynchronizeLevelWithOwner();

    if (MailLevelReward const* mailReward = sObjectMgr->GetMailLevelReward(level, GetRaceMask()))
    {
        /// @todo Poor design of mail system
        CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
        MailDraft(mailReward->mailTemplateId).SendMailTo(trans, this, MailSender(MAIL_CREATURE, mailReward->senderEntry));
        CharacterDatabase.CommitTransaction(trans);
    }

    UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_REACH_LEVEL);

    // Refer-A-Friend
    if (GetSession()->GetRecruiterId())
        if (level < sWorld->getIntConfig(CONFIG_MAX_RECRUIT_A_FRIEND_BONUS_PLAYER_LEVEL))
            if (level % 2 == 0)
            {
                ++m_grantableLevels;

                if (!HasByteFlag(PLAYER_FIELD_BYTES, PLAYER_FIELD_BYTES_OFFSET_RAF_GRANTABLE_LEVEL, 0x01))
                    SetByteFlag(PLAYER_FIELD_BYTES, PLAYER_FIELD_BYTES_OFFSET_RAF_GRANTABLE_LEVEL, 0x01);
            }

    SendQuestGiverStatusMultiple();

    // @tswow-begin
    ApplyAutolearnSpells(oldLevel+1);
    // @tswow-end

    sScriptMgr->OnPlayerLevelChanged(this, oldLevel);
}

bool Player::IsMaxLevel() const
{
    return GetLevel() >= GetUInt32Value(PLAYER_FIELD_MAX_LEVEL);
}

void Player::InitStatsForLevel(bool reapplyMods)
{
    if (reapplyMods)                                        //reapply stats values only on .reset stats (level) command
        _RemoveAllStatBonuses();

    PlayerClassLevelInfo classInfo;
    sObjectMgr->GetPlayerClassLevelInfo(GetClass(), GetLevel(), &classInfo);

    PlayerLevelInfo info;
    sObjectMgr->GetPlayerLevelInfo(GetRace(), GetClass(), GetLevel(), &info);

    uint8 exp_max_lvl = GetMaxLevelForExpansion(GetSession()->Expansion());
    uint8 conf_max_lvl = sWorld->getIntConfig(CONFIG_MAX_PLAYER_LEVEL);
    if (exp_max_lvl == DEFAULT_MAX_LEVEL || exp_max_lvl >= conf_max_lvl)
        SetUInt32Value(PLAYER_FIELD_MAX_LEVEL, conf_max_lvl);
    else
        SetUInt32Value(PLAYER_FIELD_MAX_LEVEL, exp_max_lvl);
    SetUInt32Value(PLAYER_NEXT_LEVEL_XP, sObjectMgr->GetXPForLevel(GetLevel()));

    // reset before any aura state sources (health set/aura apply)
    SetUInt32Value(UNIT_FIELD_AURASTATE, 0);

    UpdateSkillsForLevel();

    // set default cast time multiplier
    SetModCastingSpeed(1.0f);

    // reset size before reapply auras
    SetObjectScale(1.0f);

    // save base values (bonuses already included in stored stats
    for (uint8 i = STAT_STRENGTH; i < MAX_STATS; ++i)
        SetCreateStat(Stats(i), info.stats[i]);

    for (uint8 i = STAT_STRENGTH; i < MAX_STATS; ++i)
        SetStat(Stats(i), info.stats[i]);

    SetCreateHealth(classInfo.basehealth);

    //set create powers
    SetCreateMana(classInfo.basemana);

    SetArmor(int32(m_createStats[STAT_AGILITY]*2));

    InitStatBuffMods();

    //reset rating fields values
    for (uint16 index = PLAYER_FIELD_COMBAT_RATING_1; index < PLAYER_FIELD_COMBAT_RATING_1 + MAX_COMBAT_RATING; ++index)
        SetUInt32Value(index, 0);

    SetUInt32Value(PLAYER_FIELD_MOD_HEALING_DONE_POS, 0);
    for (uint8 i = SPELL_SCHOOL_NORMAL; i < MAX_SPELL_SCHOOL; ++i)
    {
        SetInt32Value(PLAYER_FIELD_MOD_DAMAGE_DONE_NEG + i, 0);
        SetInt32Value(PLAYER_FIELD_MOD_DAMAGE_DONE_POS + i, 0);
        SetFloatValue(PLAYER_FIELD_MOD_DAMAGE_DONE_PCT + i, 1.00f);
    }

    //reset attack power, damage and attack speed fields
    for (uint8 i = BASE_ATTACK; i < MAX_ATTACK; ++i)
        SetFloatValue(UNIT_FIELD_BASEATTACKTIME + i, float(BASE_ATTACK_TIME));

    SetFloatValue(UNIT_FIELD_MINDAMAGE, 0.0f);
    SetFloatValue(UNIT_FIELD_MAXDAMAGE, 0.0f);
    SetFloatValue(UNIT_FIELD_MINOFFHANDDAMAGE, 0.0f);
    SetFloatValue(UNIT_FIELD_MAXOFFHANDDAMAGE, 0.0f);
    SetFloatValue(UNIT_FIELD_MINRANGEDDAMAGE, 0.0f);
    SetFloatValue(UNIT_FIELD_MAXRANGEDDAMAGE, 0.0f);

    SetAttackPower(0);
    SetAttackPowerModPos(0);
    SetAttackPowerModNeg(0);
    SetAttackPowerMultiplier(0.0f);
    SetRangedAttackPower(0);
    SetRangedAttackPowerModPos(0);
    SetRangedAttackPowerModNeg(0);
    SetRangedAttackPowerMultiplier(0.0f);

    // Base crit values (will be recalculated in UpdateAllStats() at loading and in _ApplyAllStatBonuses() at reset
    SetFloatValue(PLAYER_CRIT_PERCENTAGE, 0.0f);
    SetFloatValue(PLAYER_OFFHAND_CRIT_PERCENTAGE, 0.0f);
    SetFloatValue(PLAYER_RANGED_CRIT_PERCENTAGE, 0.0f);

    // Init spell schools (will be recalculated in UpdateAllStats() at loading and in _ApplyAllStatBonuses() at reset
    for (uint8 i = 0; i < 7; ++i)
        SetFloatValue(PLAYER_SPELL_CRIT_PERCENTAGE1+i, 0.0f);

    SetFloatValue(PLAYER_PARRY_PERCENTAGE, 0.0f);
    SetFloatValue(PLAYER_BLOCK_PERCENTAGE, 0.0f);
    SetUInt32Value(PLAYER_SHIELD_BLOCK, 0);

    // Dodge percentage
    SetFloatValue(PLAYER_DODGE_PERCENTAGE, 0.0f);

    // set armor (resistance 0) to original value (create_agility*2)
    SetArmor(int32(m_createStats[STAT_AGILITY]*2));
    SetFloatValue(UNIT_FIELD_RESISTANCEBUFFMODSPOSITIVE + AsUnderlyingType(SPELL_SCHOOL_NORMAL), 0.0f);
    SetFloatValue(UNIT_FIELD_RESISTANCEBUFFMODSNEGATIVE + AsUnderlyingType(SPELL_SCHOOL_NORMAL), 0.0f);
    // set other resistance to original value (0)
    for (uint8 i = SPELL_SCHOOL_HOLY; i < MAX_SPELL_SCHOOL; ++i)
    {
        SetResistance(SpellSchools(i), 0);
        SetFloatValue(UNIT_FIELD_RESISTANCEBUFFMODSPOSITIVE + i, 0.0f);
        SetFloatValue(UNIT_FIELD_RESISTANCEBUFFMODSNEGATIVE + i, 0.0f);
    }

    SetUInt32Value(PLAYER_FIELD_MOD_TARGET_RESISTANCE, 0);
    SetUInt32Value(PLAYER_FIELD_MOD_TARGET_PHYSICAL_RESISTANCE, 0);
    for (uint8 i = SPELL_SCHOOL_NORMAL; i < MAX_SPELL_SCHOOL; ++i)
    {
        SetUInt32Value(UNIT_FIELD_POWER_COST_MODIFIER + i, 0);
        SetFloatValue(UNIT_FIELD_POWER_COST_MULTIPLIER + i, 0.0f);
    }
    // Reset no reagent cost field
    for (uint8 i = 0; i < 3; ++i)
        SetUInt32Value(PLAYER_NO_REAGENT_COST_1 + i, 0);
    // Init data for form but skip reapply item mods for form
    InitDataForForm(reapplyMods);

    // save new stats
    for (uint8 i = POWER_MANA; i < MAX_POWERS; ++i)
        SetMaxPower(Powers(i), uint32(GetCreatePowerValue(Powers(i))));

    SetMaxHealth(classInfo.basehealth);                     // stamina bonus will applied later

    // cleanup mounted state (it will set correctly at aura loading if player saved at mount.
    SetMountDisplayId(0);

    // cleanup unit flags (will be re-applied if need at aura load).
    RemoveUnitFlag(
        UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_REMOVE_CLIENT_CONTROL | UNIT_FLAG_NOT_ATTACKABLE_1 |
        UNIT_FLAG_IMMUNE_TO_PC | UNIT_FLAG_IMMUNE_TO_NPC  | UNIT_FLAG_LOOTING          |
        UNIT_FLAG_PET_IN_COMBAT  | UNIT_FLAG_SILENCED     | UNIT_FLAG_PACIFIED         |
        UNIT_FLAG_STUNNED        | UNIT_FLAG_IN_COMBAT    | UNIT_FLAG_DISARMED         |
        UNIT_FLAG_CONFUSED       | UNIT_FLAG_FLEEING      | UNIT_FLAG_UNINTERACTIBLE   |
        UNIT_FLAG_SKINNABLE      | UNIT_FLAG_MOUNT        | UNIT_FLAG_ON_TAXI          );
    SetUnitFlag(UNIT_FLAG_PLAYER_CONTROLLED);   // must be set

    // cleanup player flags (will be re-applied if need at aura load), to avoid have ghost flag without ghost aura, for example.
    RemoveFlag(PLAYER_FLAGS, PLAYER_FLAGS_AFK | PLAYER_FLAGS_DND | PLAYER_FLAGS_GM | PLAYER_FLAGS_GHOST | PLAYER_ALLOW_ONLY_ABILITY);

    RemoveVisFlag(UNIT_VIS_FLAGS_ALL);                 // one form stealth modified bytes
    RemovePvpFlag(UNIT_BYTE2_FLAG_FFA_PVP | UNIT_BYTE2_FLAG_SANCTUARY);

    // restore if need some important flags
    SetUInt32Value(PLAYER_FIELD_BYTES2, 0);                 // flags empty by default

    if (reapplyMods)                                        // reapply stats values only on .reset stats (level) command
        _ApplyAllStatBonuses();

    // set current level health and mana/energy to maximum after applying all mods.
    SetFullHealth();
    SetFullPower(POWER_MANA);
    SetFullPower(POWER_ENERGY);
    if (GetPower(POWER_RAGE) > GetMaxPower(POWER_RAGE))
        SetFullPower(POWER_RAGE);
    SetFullPower(POWER_FOCUS);
    SetPower(POWER_HAPPINESS, 0);
    SetPower(POWER_RUNIC_POWER, 0);

    // update level to hunter/summon pet
    if (Pet* pet = GetPet())
        pet->SynchronizeLevelWithOwner();
}

void Player::UpdateDefense()
{
    if (UpdateSkill(SKILL_DEFENSE, sWorld->getIntConfig(CONFIG_SKILL_GAIN_DEFENSE)))
        UpdateDefenseBonusesMod(); // update dependent from defense skill part
}

void Player::HandleBaseModFlatValue(BaseModGroup modGroup, float amount, bool apply)
{
    if (modGroup >= BASEMOD_END)
    {
        TC_LOG_ERROR("spells", "Player::HandleBaseModValue: Invalid BaseModGroup/BaseModType ({}/{}) for player '{}' ({})",
            modGroup, FLAT_MOD, GetName(), GetGUID().ToString());
        return;
    }

    m_auraBaseFlatMod[modGroup] += apply ? amount : -amount;
    UpdateBaseModGroup(modGroup);
}

void Player::ApplyBaseModPctValue(BaseModGroup modGroup, float pct)
{
    if (modGroup >= BASEMOD_END)
    {
        TC_LOG_ERROR("spells", "Player::HandleBaseModValue: Invalid BaseModGroup/BaseModType ({}/{}) for player '{}' ({})",
            modGroup, FLAT_MOD, GetName(), GetGUID().ToString());
        return;
    }

    m_auraBasePctMod[modGroup] += CalculatePct(1.0f, pct);
    UpdateBaseModGroup(modGroup);
}

void Player::SetBaseModFlatValue(BaseModGroup modGroup, float val)
{
    if (m_auraBaseFlatMod[modGroup] == val)
        return;

    m_auraBaseFlatMod[modGroup] = val;
    UpdateBaseModGroup(modGroup);
}

void Player::SetBaseModPctValue(BaseModGroup modGroup, float val)
{
    if (m_auraBasePctMod[modGroup] == val)
        return;

    m_auraBasePctMod[modGroup] = val;
    UpdateBaseModGroup(modGroup);
}

void Player::UpdateDamageDoneMods(WeaponAttackType attackType, int32 skipEnchantSlot /*= -1*/)
{
    Unit::UpdateDamageDoneMods(attackType, skipEnchantSlot);

    UnitMods unitMod;
    switch (attackType)
    {
        case BASE_ATTACK:
            unitMod = UNIT_MOD_DAMAGE_MAINHAND;
            break;
        case OFF_ATTACK:
            unitMod = UNIT_MOD_DAMAGE_OFFHAND;
            break;
        case RANGED_ATTACK:
            unitMod = UNIT_MOD_DAMAGE_RANGED;
            break;
        default:
            ABORT();
            break;
    }

    float amount = 0.0f;
    Item* item = GetWeaponForDamageMods(attackType);
    if (!item)
        return;

    for (uint8 slot = 0; slot < MAX_ENCHANTMENT_SLOT; ++slot)
    {
        if (skipEnchantSlot == slot)
            continue;

        SpellItemEnchantmentEntry const* enchantmentEntry = sSpellItemEnchantmentStore.LookupEntry(item->GetEnchantmentId(EnchantmentSlot(slot)));
        if (!enchantmentEntry)
            continue;

        for (uint8 i = 0; i < MAX_ITEM_ENCHANTMENT_EFFECTS; ++i)
        {
            switch (enchantmentEntry->Effect[i])
            {
                case ITEM_ENCHANTMENT_TYPE_DAMAGE:
                    amount += enchantmentEntry->EffectPointsMin[i];
                    break;
                case ITEM_ENCHANTMENT_TYPE_TOTEM:
                    if (GetClass() == CLASS_SHAMAN)
                        amount += enchantmentEntry->EffectPointsMin[i] * item->GetTemplate()->Delay / 1000.0f; // Wrath Rockbiter increases dps by flat amount
                    break;
                default:
                    break;
            }
        }
    }

    // For +damage enchants and rockbiter we add flat mods to the primary weapon's spell school
    SpellSchools school = GetMeleeDamageSchool(attackType, 0);
    HandleDamageFlatModifier(unitMod, school, amount, true);
}

void Player::UpdateBaseModGroup(BaseModGroup modGroup)
{
    if (!CanModifyStats())
        return;

    switch (modGroup)
    {
        case CRIT_PERCENTAGE:              UpdateCritPercentage(BASE_ATTACK);                          break;
        case RANGED_CRIT_PERCENTAGE:       UpdateCritPercentage(RANGED_ATTACK);                        break;
        case OFFHAND_CRIT_PERCENTAGE:      UpdateCritPercentage(OFF_ATTACK);                           break;
        case SHIELD_BLOCK_VALUE:           UpdateShieldBlockValue();                                   break;
        default: break;
    }
}

float Player::GetBaseModValue(BaseModGroup modGroup, BaseModType modType) const
{
    if (modGroup >= BASEMOD_END || modType >= MOD_END)
    {
        TC_LOG_ERROR("spells", "Player::GetBaseModValue: Invalid BaseModGroup/BaseModType ({}/{}) for player '{}' ({})",
            modGroup, modType, GetName(), GetGUID().ToString());
        return 0.0f;
    }

    return (modType == FLAT_MOD ? m_auraBaseFlatMod[modGroup] : m_auraBasePctMod[modGroup]);
}

float Player::GetTotalBaseModValue(BaseModGroup modGroup) const
{
    if (modGroup >= BASEMOD_END)
    {
        TC_LOG_ERROR("spells", "Player::GetTotalBaseModValue: Invalid BaseModGroup ({}) for player '{}' ({})",
            modGroup, GetName(), GetGUID().ToString());
        return 0.0f;
    }

    return m_auraBaseFlatMod[modGroup] * m_auraBasePctMod[modGroup];
}

uint32 Player::GetShieldBlockValue() const
{
    /** @epoch-start */
    // float value = std::max(0.f, (m_auraBaseFlatMod[SHIELD_BLOCK_VALUE] + GetStat(STAT_STRENGTH) * 0.5f - 10) * m_auraBasePctMod[SHIELD_BLOCK_VALUE]);
    float value = std::max(0.f, (m_auraBaseFlatMod[SHIELD_BLOCK_VALUE] + GetStat(STAT_STRENGTH) / 20 - 1) * m_auraBasePctMod[SHIELD_BLOCK_VALUE]);
    /** @epoch-end */

    return uint32(value);
}

float Player::GetMeleeCritFromAgility() const
{
    uint8 level = GetLevel();
    uint32 pclass = GetClass();

    if (level > GT_MAX_LEVEL)
        level = GT_MAX_LEVEL;

    GtChanceToMeleeCritBaseEntry const* critBase  = sGtChanceToMeleeCritBaseStore.LookupEntry(pclass-1);
    GtChanceToMeleeCritEntry     const* critRatio = sGtChanceToMeleeCritStore.LookupEntry((pclass-1)*GT_MAX_LEVEL + level-1);
    if (critBase == nullptr || critRatio == nullptr)
        return 0.0f;

    float crit = critBase->Data + GetStat(STAT_AGILITY)*critRatio->Data;
    return crit*100.0f;
}

// @tswow-begin move dodge_base/crit_to_doge values to global scope and remove const
// Table for base dodge values
float dodge_base[MAX_CLASSES] =
{
     0.036640f, // Warrior
     0.034943f, // Paladin
    -0.040873f, // Hunter
     0.020957f, // Rogue
     0.034178f, // Priest
     0.036640f, // DK
     0.021080f, // Shaman
     0.036587f, // Mage
     0.024211f, // Warlock
     0.0f,      // ??
     0.056097f,  // Druid

     // default values for custom classes
    .035f,.035f,.035f,.035f,.035f,.035f,.035f,
    .035f,.035f,.035f,.035f,.035f,.035f,.035f,
    .035f,.035f,.035f,.035f,.035f,.035f,.035f,
};
// Crit/agility to dodge/agility coefficient multipliers; 3.2.0 increased required agility by 15%
float crit_to_dodge[MAX_CLASSES] =
{
    1.1f, // [1]  Warrior
    1.0f, // [2]  Paladin
    1.6f, // [3]  Hunter
    2.0f, // [4]  Rogue
    1.0f, // [5]  Priest
    1.0f, // [6]  DK
    1.0f, // [7]  Shaman
    1.0f, // [8]  Mage
    1.0f, // [9]  Warlock
    0.0f, // [10] <Unused>
    1.7f, // [11] Druid

    // default values for custom classes
    .87f,.87f,.87f,.87f,.87f,.87f,.87f,
    .87f,.87f,.87f,.87f,.87f,.87f,.87f,
    .87f,.87f,.87f,.87f,.87f,.87f,.87f,
};

float Player::GetDodgeFromAgility(float amount) const
{
// @tswow-end

    uint8 level = GetLevel();
    uint32 pclass = GetClass();

    if (level > GT_MAX_LEVEL)
        level = GT_MAX_LEVEL;

    // Dodge per agility is proportional to crit per agility, which is available from DBC files
    GtChanceToMeleeCritEntry  const* dodgeRatio = sGtChanceToMeleeCritStore.LookupEntry((pclass-1)*GT_MAX_LEVEL + level-1);
    if (dodgeRatio == nullptr || pclass > MAX_CLASSES)
        return 0.0f;

    return (100.0f * amount * dodgeRatio->Data * crit_to_dodge[pclass - 1]);
}

float Player::GetSpellCritFromIntellect() const
{
    uint8 level = GetLevel();
    uint32 pclass = GetClass();

    if (level > GT_MAX_LEVEL)
        level = GT_MAX_LEVEL;

    GtChanceToSpellCritBaseEntry const* critBase  = sGtChanceToSpellCritBaseStore.LookupEntry(pclass-1);
    GtChanceToSpellCritEntry     const* critRatio = sGtChanceToSpellCritStore.LookupEntry((pclass-1)*GT_MAX_LEVEL + level-1);
    if (critBase == nullptr || critRatio == nullptr)
        return 0.0f;

    float crit = critBase->Data + GetStat(STAT_INTELLECT) * critRatio->Data;
    return crit * 100.0f;
}

float Player::GetRatingMultiplier(CombatRating cr) const
{
    uint8 level = GetLevel();

    if (level > GT_MAX_LEVEL)
        level = GT_MAX_LEVEL;

    GtCombatRatingsEntry const* Rating = sGtCombatRatingsStore.LookupEntry(cr*GT_MAX_LEVEL+level-1);
    // gtOCTClassCombatRatingScalarStore.dbc starts with 1, CombatRating with zero, so cr+1
    GtOCTClassCombatRatingScalarEntry const* classRating = sGtOCTClassCombatRatingScalarStore.LookupEntry((GetClass()-1)*GT_MAX_RATING+cr+1);
    if (!Rating || !classRating)
        return 1.0f;                                        // By default use minimum coefficient (not must be called)

    return classRating->Data / Rating->Data;
}

float Player::GetRatingBonusValue(CombatRating cr) const
{
    return float(GetUInt32Value(PLAYER_FIELD_COMBAT_RATING_1 + AsUnderlyingType(cr))) * GetRatingMultiplier(cr);
}

float Player::GetExpertiseDodgeOrParryReduction(WeaponAttackType attType) const
{
    switch (attType)
    {
        case BASE_ATTACK:
            return GetUInt32Value(PLAYER_EXPERTISE) / 4.0f;
        case OFF_ATTACK:
            return GetUInt32Value(PLAYER_OFFHAND_EXPERTISE) / 4.0f;
        default:
            break;
    }
    return 0.0f;
}

void Player::ApplyRatingMod(CombatRating combatRating, int32 value, bool apply)
{
    float oldRating = m_baseRatingValue[combatRating];
    m_baseRatingValue[combatRating] += (apply ? value : -value);

    // explicit affected values
    float const multiplier = GetRatingMultiplier(combatRating);
    float const oldVal = oldRating * multiplier;
    float const newVal = m_baseRatingValue[combatRating] * multiplier;
    switch (combatRating)
    {
        case CR_HASTE_MELEE:
            ApplyAttackTimePercentMod(BASE_ATTACK, oldVal, false);
            ApplyAttackTimePercentMod(OFF_ATTACK, oldVal, false);
            ApplyAttackTimePercentMod(BASE_ATTACK, newVal, true);
            ApplyAttackTimePercentMod(OFF_ATTACK, newVal, true);
            break;
        case CR_HASTE_RANGED:
            ApplyAttackTimePercentMod(RANGED_ATTACK, oldVal, false);
            ApplyAttackTimePercentMod(RANGED_ATTACK, newVal, true);
            break;
        case CR_HASTE_SPELL:
            ApplyCastTimePercentMod(oldVal, false);
            ApplyCastTimePercentMod(newVal, true);
            break;
        default:
            break;
    }

    UpdateRating(combatRating);
}

void Player::UpdateRating(CombatRating cr)
{
    int32 amount = m_baseRatingValue[cr];
    // Apply bonus from SPELL_AURA_MOD_RATING_FROM_STAT
    // stat used stored in miscValueB for this aura
    AuraEffectList const& modRatingFromStat = GetAuraEffectsByType(SPELL_AURA_MOD_RATING_FROM_STAT);
    for (AuraEffect const* aurEff : modRatingFromStat)
        if (aurEff->GetMiscValue() & (1 << cr))
            amount += int32(CalculatePct(GetStat(Stats(aurEff->GetMiscValueB())), aurEff->GetAmount()));

    if (amount < 0)
        amount = 0;
    SetUInt32Value(PLAYER_FIELD_COMBAT_RATING_1 + AsUnderlyingType(cr), uint32(amount));

    bool affectStats = CanModifyStats();

    switch (cr)
    {
        case CR_WEAPON_SKILL:                               // Implemented in Unit::RollMeleeOutcomeAgainst
        case CR_DEFENSE_SKILL:
            UpdateDefenseBonusesMod();
            break;
        case CR_DODGE:
            UpdateDodgePercentage();
            break;
        case CR_PARRY:
            UpdateParryPercentage();
            break;
        case CR_BLOCK:
            UpdateBlockPercentage();
            break;
        case CR_HIT_MELEE:
            UpdateMeleeHitChances();
            break;
        case CR_HIT_RANGED:
            UpdateRangedHitChances();
            break;
        case CR_HIT_SPELL:
            UpdateSpellHitChances();
            break;
        case CR_CRIT_MELEE:
            if (affectStats)
            {
                UpdateCritPercentage(BASE_ATTACK);
                UpdateCritPercentage(OFF_ATTACK);
            }
            break;
        case CR_CRIT_RANGED:
            if (affectStats)
                UpdateCritPercentage(RANGED_ATTACK);
            break;
        case CR_CRIT_SPELL:
            if (affectStats)
                UpdateAllSpellCritChances();
            break;
        case CR_HIT_TAKEN_MELEE:                            // Implemented in Unit::MeleeMissChanceCalc
        case CR_HIT_TAKEN_RANGED:
            break;
        case CR_HIT_TAKEN_SPELL:                            // Implemented in Unit::MagicSpellHitResult
            break;
        case CR_CRIT_TAKEN_MELEE:                           // Implemented in Unit::RollMeleeOutcomeAgainst (only for chance to crit)
        case CR_CRIT_TAKEN_RANGED:
            break;
        case CR_CRIT_TAKEN_SPELL:                           // Implemented in Unit::SpellCriticalBonus (only for chance to crit)
            break;
        case CR_HASTE_MELEE:                                // Implemented in Player::ApplyRatingMod
        case CR_HASTE_RANGED:
        case CR_HASTE_SPELL:
            break;
        case CR_WEAPON_SKILL_MAINHAND:                      // Implemented in Unit::RollMeleeOutcomeAgainst
        case CR_WEAPON_SKILL_OFFHAND:
        case CR_WEAPON_SKILL_RANGED:
            break;
        case CR_EXPERTISE:
            if (affectStats)
            {
                UpdateExpertise(BASE_ATTACK);
                UpdateExpertise(OFF_ATTACK);
            }
            break;
        case CR_ARMOR_PENETRATION:
            if (affectStats)
                UpdateArmorPenetration(amount);
            break;
    }
}

void Player::UpdateAllRatings()
{
    for (uint8 cr = 0; cr < MAX_COMBAT_RATING; ++cr)
        UpdateRating(CombatRating(cr));
}

void Player::SetRegularAttackTime()
{
    for (uint8 i = 0; i < MAX_ATTACK; ++i)
    {
        Item* tmpitem = GetWeaponForAttack(WeaponAttackType(i), true);
        if (tmpitem && !tmpitem->IsBroken())
        {
            ItemTemplate const* proto = tmpitem->GetTemplate();
            if (proto->Delay)
                SetAttackTime(WeaponAttackType(i), proto->Delay);
        }
        else
            SetAttackTime(WeaponAttackType(i), BASE_ATTACK_TIME);  // If there is no weapon reset attack time to base (might have been changed from forms)
    }
}

ScalingStatDistributionEntry const* Player::GetScalingStatDistributionFor(ItemTemplate const& itemTemplate) const
{
    if (!itemTemplate.ScalingStatDistribution)
        return nullptr;

    return sScalingStatDistributionStore.LookupEntry(itemTemplate.ScalingStatDistribution);
}

ScalingStatValuesEntry const* Player::GetScalingStatValuesFor(ItemTemplate const& itemTemplate) const
{
    if (!itemTemplate.ScalingStatValue)
        return nullptr;

    ScalingStatDistributionEntry const* ssd = GetScalingStatDistributionFor(itemTemplate);
    if (!ssd)
        return nullptr;

    // req. check at equip, but allow use for extended range if range limit max level, set proper level
    uint32 const ssd_level = std::min(uint32(GetLevel()), ssd->Maxlevel);
    return sScalingStatValuesStore.LookupEntry(ssd_level);
}

void Player::_ApplyItemBonuses(ItemTemplate const* proto, uint8 slot, bool apply, bool only_level_scale /*= false*/)
{
    if (slot >= INVENTORY_SLOT_BAG_END || !proto)
        return;

    ScalingStatDistributionEntry const* ssd = GetScalingStatDistributionFor(*proto);
    ScalingStatValuesEntry const* ssv = GetScalingStatValuesFor(*proto);
    if (only_level_scale && (!ssd || !ssv))
        return;

    for (uint8 i = 0; i < MAX_ITEM_PROTO_STATS; ++i)
    {
        uint32 statType = 0;
        int32  val = 0;
        // If set ScalingStatDistribution need get stats and values from it
        if (ssd && ssv)
        {
            if (ssd->StatID[i] < 0)
                continue;
            statType = ssd->StatID[i];
            val = (ssv->getssdMultiplier(proto->ScalingStatValue) * ssd->Bonus[i]) / 10000;
        }
        else
        {
            if (i >= proto->StatsCount)
                continue;
            statType = proto->ItemStat[i].ItemStatType;
            val = proto->ItemStat[i].ItemStatValue;
        }

        if (val == 0)
            continue;

        switch (statType)
        {
            case ITEM_MOD_MANA:
                HandleStatFlatModifier(UNIT_MOD_MANA, BASE_VALUE, float(val), apply);
                break;
            case ITEM_MOD_HEALTH:                           // modify HP
                HandleStatFlatModifier(UNIT_MOD_HEALTH, BASE_VALUE, float(val), apply);
                break;
            case ITEM_MOD_AGILITY:                          // modify agility
                HandleStatFlatModifier(UNIT_MOD_STAT_AGILITY, BASE_VALUE, float(val), apply);
                UpdateStatBuffMod(STAT_AGILITY);
                break;
            case ITEM_MOD_STRENGTH:                         //modify strength
                HandleStatFlatModifier(UNIT_MOD_STAT_STRENGTH, BASE_VALUE, float(val), apply);
                UpdateStatBuffMod(STAT_STRENGTH);
                break;
            case ITEM_MOD_INTELLECT:                        //modify intellect
                HandleStatFlatModifier(UNIT_MOD_STAT_INTELLECT, BASE_VALUE, float(val), apply);
                UpdateStatBuffMod(STAT_INTELLECT);
                break;
            case ITEM_MOD_SPIRIT:                           //modify spirit
                HandleStatFlatModifier(UNIT_MOD_STAT_SPIRIT, BASE_VALUE, float(val), apply);
                UpdateStatBuffMod(STAT_SPIRIT);
                break;
            case ITEM_MOD_STAMINA:                          //modify stamina
                HandleStatFlatModifier(UNIT_MOD_STAT_STAMINA, BASE_VALUE, float(val), apply);
                UpdateStatBuffMod(STAT_STAMINA);
                break;
            case ITEM_MOD_DEFENSE_SKILL_RATING:
                ApplyRatingMod(CR_DEFENSE_SKILL, int32(val), apply);
                break;
            case ITEM_MOD_DODGE_RATING:
                ApplyRatingMod(CR_DODGE, int32(val), apply);
                break;
            case ITEM_MOD_PARRY_RATING:
                ApplyRatingMod(CR_PARRY, int32(val), apply);
                break;
            case ITEM_MOD_BLOCK_RATING:
                ApplyRatingMod(CR_BLOCK, int32(val), apply);
                break;
            case ITEM_MOD_HIT_MELEE_RATING:
                ApplyRatingMod(CR_HIT_MELEE, int32(val), apply);
                break;
            case ITEM_MOD_HIT_RANGED_RATING:
                ApplyRatingMod(CR_HIT_RANGED, int32(val), apply);
                break;
            case ITEM_MOD_HIT_SPELL_RATING:
                ApplyRatingMod(CR_HIT_SPELL, int32(val), apply);
                break;
            case ITEM_MOD_CRIT_MELEE_RATING:
                ApplyRatingMod(CR_CRIT_MELEE, int32(val), apply);
                break;
            case ITEM_MOD_CRIT_RANGED_RATING:
                ApplyRatingMod(CR_CRIT_RANGED, int32(val), apply);
                break;
            case ITEM_MOD_CRIT_SPELL_RATING:
                ApplyRatingMod(CR_CRIT_SPELL, int32(val), apply);
                break;
            case ITEM_MOD_HIT_TAKEN_MELEE_RATING:
                ApplyRatingMod(CR_HIT_TAKEN_MELEE, int32(val), apply);
                break;
            case ITEM_MOD_HIT_TAKEN_RANGED_RATING:
                ApplyRatingMod(CR_HIT_TAKEN_RANGED, int32(val), apply);
                break;
            case ITEM_MOD_HIT_TAKEN_SPELL_RATING:
                ApplyRatingMod(CR_HIT_TAKEN_SPELL, int32(val), apply);
                break;
            case ITEM_MOD_CRIT_TAKEN_MELEE_RATING:
                ApplyRatingMod(CR_CRIT_TAKEN_MELEE, int32(val), apply);
                break;
            case ITEM_MOD_CRIT_TAKEN_RANGED_RATING:
                ApplyRatingMod(CR_CRIT_TAKEN_RANGED, int32(val), apply);
                break;
            case ITEM_MOD_CRIT_TAKEN_SPELL_RATING:
                ApplyRatingMod(CR_CRIT_TAKEN_SPELL, int32(val), apply);
                break;
            case ITEM_MOD_HASTE_MELEE_RATING:
                ApplyRatingMod(CR_HASTE_MELEE, int32(val), apply);
                break;
            case ITEM_MOD_HASTE_RANGED_RATING:
                ApplyRatingMod(CR_HASTE_RANGED, int32(val), apply);
                break;
            case ITEM_MOD_HASTE_SPELL_RATING:
                ApplyRatingMod(CR_HASTE_SPELL, int32(val), apply);
                break;
            case ITEM_MOD_HIT_RATING:
                ApplyRatingMod(CR_HIT_MELEE, int32(val), apply);
                ApplyRatingMod(CR_HIT_RANGED, int32(val), apply);
                ApplyRatingMod(CR_HIT_SPELL, int32(val), apply);
                break;
            case ITEM_MOD_CRIT_RATING:
                ApplyRatingMod(CR_CRIT_MELEE, int32(val), apply);
                ApplyRatingMod(CR_CRIT_RANGED, int32(val), apply);
                ApplyRatingMod(CR_CRIT_SPELL, int32(val), apply);
                break;
            case ITEM_MOD_HIT_TAKEN_RATING:
                ApplyRatingMod(CR_HIT_TAKEN_MELEE, int32(val), apply);
                ApplyRatingMod(CR_HIT_TAKEN_RANGED, int32(val), apply);
                ApplyRatingMod(CR_HIT_TAKEN_SPELL, int32(val), apply);
                break;
            case ITEM_MOD_CRIT_TAKEN_RATING:
                ApplyRatingMod(CR_CRIT_TAKEN_MELEE, int32(val), apply);
                ApplyRatingMod(CR_CRIT_TAKEN_RANGED, int32(val), apply);
                ApplyRatingMod(CR_CRIT_TAKEN_SPELL, int32(val), apply);
                break;
            case ITEM_MOD_RESILIENCE_RATING:
                ApplyRatingMod(CR_CRIT_TAKEN_MELEE, int32(val), apply);
                ApplyRatingMod(CR_CRIT_TAKEN_RANGED, int32(val), apply);
                ApplyRatingMod(CR_CRIT_TAKEN_SPELL, int32(val), apply);
                break;
            case ITEM_MOD_HASTE_RATING:
                ApplyRatingMod(CR_HASTE_MELEE, int32(val), apply);
                ApplyRatingMod(CR_HASTE_RANGED, int32(val), apply);
                ApplyRatingMod(CR_HASTE_SPELL, int32(val), apply);
                break;
            case ITEM_MOD_EXPERTISE_RATING:
                ApplyRatingMod(CR_EXPERTISE, int32(val), apply);
                break;
            case ITEM_MOD_ATTACK_POWER:
                HandleAttackPowerModifier(MELEE_AP_MODS, (val > 0) ? AP_MOD_POSITIVE_FLAT : AP_MOD_NEGATIVE_FLAT, float(val), apply);
                HandleAttackPowerModifier(RANGED_AP_MODS, (val > 0) ? AP_MOD_POSITIVE_FLAT : AP_MOD_NEGATIVE_FLAT, float(val), apply);
                break;
            case ITEM_MOD_RANGED_ATTACK_POWER:
                HandleAttackPowerModifier(RANGED_AP_MODS, (val > 0) ? AP_MOD_POSITIVE_FLAT : AP_MOD_NEGATIVE_FLAT, float(val), apply);
                break;
//            case ITEM_MOD_FERAL_ATTACK_POWER:
//                ApplyFeralAPBonus(int32(val), apply);
//                break;
            case ITEM_MOD_MANA_REGENERATION:
                ApplyManaRegenBonus(int32(val), apply);
                break;
            case ITEM_MOD_ARMOR_PENETRATION_RATING:
                ApplyRatingMod(CR_ARMOR_PENETRATION, int32(val), apply);
                break;
            case ITEM_MOD_SPELL_POWER:
                ApplySpellPowerBonus(int32(val), apply);
                break;
            case ITEM_MOD_HEALTH_REGEN:
                ApplyHealthRegenBonus(int32(val), apply);
                break;
            case ITEM_MOD_SPELL_PENETRATION:
                ApplySpellPenetrationBonus(val, apply);
                break;
            case ITEM_MOD_BLOCK_VALUE:
                HandleBaseModFlatValue(SHIELD_BLOCK_VALUE, float(val), apply);
                break;
            // deprecated item mods
            case ITEM_MOD_SPELL_HEALING_DONE:
            case ITEM_MOD_SPELL_DAMAGE_DONE:
                break;
        }
    }

    // Apply Spell Power from ScalingStatValue if set
    if (ssv)
        if (int32 spellbonus = ssv->getSpellBonus(proto->ScalingStatValue))
            ApplySpellPowerBonus(spellbonus, apply);

    /** @epoch-start */
    // // If set ScalingStatValue armor get it or use item armor
    // uint32 armor = proto->Armor;
    // if (ssv)
    // {
    //     if (uint32 ssvarmor = ssv->getArmorMod(proto->ScalingStatValue))
    //         armor = ssvarmor;
    // }
    // else if (armor && proto->ArmorDamageModifier)
    //     armor -= uint32(proto->ArmorDamageModifier);

    // if (armor)
    // {
    //     UnitModifierFlatType modType = TOTAL_VALUE;
    //     if (proto->Class == ITEM_CLASS_ARMOR)
    //     {
    //         switch (proto->SubClass)
    //         {
    //             case ITEM_SUBCLASS_ARMOR_CLOTH:
    //             case ITEM_SUBCLASS_ARMOR_LEATHER:
    //             case ITEM_SUBCLASS_ARMOR_MAIL:
    //             case ITEM_SUBCLASS_ARMOR_PLATE:
    //             case ITEM_SUBCLASS_ARMOR_SHIELD:
    //                 modType = BASE_VALUE;
    //             break;
    //         }
    //     }
    //     HandleStatFlatModifier(UNIT_MOD_ARMOR, modType, float(armor), apply);
    // }

    // // Add armor bonus from ArmorDamageModifier if > 0
    // if (proto->ArmorDamageModifier > 0)
    //     HandleStatFlatModifier(UNIT_MOD_ARMOR, TOTAL_VALUE, float(proto->ArmorDamageModifier), apply);

    uint32 armor = proto->Armor;
    if (armor)
        HandleStatFlatModifier(UNIT_MOD_ARMOR, BASE_VALUE, float(armor), apply);
    /** @epoch-end */

    if (proto->Block)
        HandleBaseModFlatValue(SHIELD_BLOCK_VALUE, float(proto->Block), apply);

    if (proto->HolyRes)
        HandleStatFlatModifier(UNIT_MOD_RESISTANCE_HOLY, BASE_VALUE, float(proto->HolyRes), apply);

    if (proto->FireRes)
        HandleStatFlatModifier(UNIT_MOD_RESISTANCE_FIRE, BASE_VALUE, float(proto->FireRes), apply);

    if (proto->NatureRes)
        HandleStatFlatModifier(UNIT_MOD_RESISTANCE_NATURE, BASE_VALUE, float(proto->NatureRes), apply);

    if (proto->FrostRes)
        HandleStatFlatModifier(UNIT_MOD_RESISTANCE_FROST, BASE_VALUE, float(proto->FrostRes), apply);

    if (proto->ShadowRes)
        HandleStatFlatModifier(UNIT_MOD_RESISTANCE_SHADOW, BASE_VALUE, float(proto->ShadowRes), apply);

    if (proto->ArcaneRes)
        HandleStatFlatModifier(UNIT_MOD_RESISTANCE_ARCANE, BASE_VALUE, float(proto->ArcaneRes), apply);

    WeaponAttackType attType = Player::GetAttackBySlot(slot);
    if (attType != MAX_ATTACK)
        _ApplyWeaponDamage(slot, proto, apply);

    // Druids get feral AP bonus from weapon dps (also use DPS from ScalingStatValue)
    if (GetClass() == CLASS_DRUID)
    {
        int32 dpsMod = 0;
        int32 feral_bonus = 0;
        if (ssv)
        {
            dpsMod = ssv->getDPSMod(proto->ScalingStatValue);
            feral_bonus += ssv->getFeralBonus(proto->ScalingStatValue);
        }

        feral_bonus += proto->getFeralBonus(dpsMod);
        if (feral_bonus)
            ApplyFeralAPBonus(feral_bonus, apply);
    }
}

void Player::_ApplyAmmoBonuses()
{
    // check ammo
    uint32 ammo_id = GetUInt32Value(PLAYER_AMMO_ID);
    if (!ammo_id)
        return;

    float currentAmmoDPS;

    ItemTemplate const* ammo_proto = sObjectMgr->GetItemTemplate(ammo_id);
    if (!ammo_proto || ammo_proto->Class != ITEM_CLASS_PROJECTILE || !CheckAmmoCompatibility(ammo_proto))
        currentAmmoDPS = 0.0f;
    else
        currentAmmoDPS = (ammo_proto->Damage[0].DamageMin + ammo_proto->Damage[0].DamageMax) / 2;

    if (currentAmmoDPS == GetAmmoDPS())
        return;

    m_ammoDPS = currentAmmoDPS;

    if (CanModifyStats())
        UpdateDamagePhysical(RANGED_ATTACK);
}

void Player::_ApplyWeaponDamage(uint8 slot, ItemTemplate const* proto, bool apply)
{
    WeaponAttackType attType = Player::GetAttackBySlot(slot);
    if (!IsInFeralForm() && apply && !CanUseAttackType(attType))
        return;

    ScalingStatValuesEntry const* ssv = GetScalingStatValuesFor(*proto);
    for (uint8 i = 0; i < MAX_ITEM_PROTO_DAMAGES; ++i)
    {
        float minDamage = proto->Damage[i].DamageMin;
        float maxDamage = proto->Damage[i].DamageMax;

        // If set dpsMod in ScalingStatValue use it for min (70% from average), max (130% from average) damage
        if (ssv && i == 0) // scaling stats only for first damage
        {
            int32 extraDPS = ssv->getDPSMod(proto->ScalingStatValue);
            if (extraDPS)
            {
                float average = extraDPS * proto->Delay / 1000.0f;
                float mod = ssv->isTwoHand(proto->ScalingStatValue) ? 0.2f : 0.3f;

                minDamage = (1.0f - mod) * average;
                maxDamage = (1.0f + mod) * average;
            }
        }

        if (apply)
        {
            if (minDamage > 0.f)
                SetBaseWeaponDamage(attType, MINDAMAGE, minDamage, i);

            if (maxDamage > 0.f)
                SetBaseWeaponDamage(attType, MAXDAMAGE, maxDamage, i);
        }
    }

    if (!apply)
    {
        for (uint8 i = 0; i < MAX_ITEM_PROTO_DAMAGES; ++i)
        {
            SetBaseWeaponDamage(attType, MINDAMAGE, 0.f, i);
            SetBaseWeaponDamage(attType, MAXDAMAGE, 0.f, i);
        }

        if (attType == BASE_ATTACK)
        {
            SetBaseWeaponDamage(BASE_ATTACK, MINDAMAGE, BASE_MINDAMAGE);
            SetBaseWeaponDamage(BASE_ATTACK, MAXDAMAGE, BASE_MAXDAMAGE);
        }
    }

    if (proto->Delay && !IsInFeralForm())
        SetAttackTime(attType, apply ? proto->Delay : BASE_ATTACK_TIME);

    // No need to modify any physical damage for ferals as it is calculated from stats only
    if (IsInFeralForm())
        return;

    if (CanModifyStats() && (GetWeaponDamageRange(attType, MAXDAMAGE) || proto->Delay))
        UpdateDamagePhysical(attType);
}

// this one rechecks weapon auras and stores them in BaseModGroup container
// needed for things like axe specialization applying only to axe weapons in case of dual-wield
void Player::UpdateWeaponDependentCritAuras(WeaponAttackType attackType)
{
    BaseModGroup modGroup;
    switch (attackType)
    {
        case BASE_ATTACK:
            modGroup = CRIT_PERCENTAGE;
            break;
        case OFF_ATTACK:
            modGroup = OFFHAND_CRIT_PERCENTAGE;
            break;
        case RANGED_ATTACK:
            modGroup = RANGED_CRIT_PERCENTAGE;
            break;
        default:
            ABORT();
            break;
    }

    float amount = 0.0f;
    amount += GetTotalAuraModifier(SPELL_AURA_MOD_WEAPON_CRIT_PERCENT, std::bind(&Unit::CheckAttackFitToAuraRequirement, this, attackType, std::placeholders::_1));

    // these auras don't have item requirement (only Combat Expertise in 3.3.5a)
    amount += GetTotalAuraModifier(SPELL_AURA_MOD_CRIT_PCT);

    SetBaseModFlatValue(modGroup, amount);
}

void Player::UpdateAllWeaponDependentCritAuras()
{
    for (uint8 i = BASE_ATTACK; i < MAX_ATTACK; ++i)
        UpdateWeaponDependentCritAuras(WeaponAttackType(i));
}

void Player::UpdateWeaponDependentAuras(WeaponAttackType attackType)
{
    UpdateWeaponDependentCritAuras(attackType);
    UpdateDamageDoneMods(attackType);
    UpdateDamagePctDoneMods(attackType);
}

void Player::_ApplyAllLevelScaleItemMods(bool apply)
{
    for (uint8 i = 0; i < INVENTORY_SLOT_BAG_END; ++i)
    {
        if (m_items[i])
        {
            if (m_items[i]->IsBroken() || !CanUseAttackType(GetAttackBySlot(i)))
                continue;

            ItemTemplate const* proto = m_items[i]->GetTemplate();
            if (!proto)
                continue;

            _ApplyItemBonuses(proto, i, apply, true);
        }
    }
}