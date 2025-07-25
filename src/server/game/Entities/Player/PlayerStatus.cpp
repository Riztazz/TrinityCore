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

void Player::SendMirrorTimer(MirrorTimerType Type, uint32 MaxValue, uint32 CurrentValue, int32 Regen)
{
    if (int(MaxValue) == DISABLED_MIRROR_TIMER)
    {
        if (int(CurrentValue) != DISABLED_MIRROR_TIMER)
            StopMirrorTimer(Type);
        return;
    }

    SendDirectMessage(WorldPackets::Misc::StartMirrorTimer(Type, CurrentValue, MaxValue, Regen, false, 0).Write());
}

void Player::StopMirrorTimer(MirrorTimerType Type)
{
    m_MirrorTimer[Type] = DISABLED_MIRROR_TIMER;
    SendDirectMessage(WorldPackets::Misc::StopMirrorTimer(Type).Write());
}

int32 Player::getMaxTimer(MirrorTimerType timer) const
{
    switch (timer)
    {
        case FATIGUE_TIMER:
            return MINUTE * IN_MILLISECONDS;
        case BREATH_TIMER:
        {
            if (!IsAlive() || HasAuraType(SPELL_AURA_WATER_BREATHING) || GetSession()->GetSecurity() >= AccountTypes(sWorld->getIntConfig(CONFIG_DISABLE_BREATHING)))
                return DISABLED_MIRROR_TIMER;

            int32 UnderWaterTime = 1 * MINUTE * IN_MILLISECONDS;
            UnderWaterTime *= GetTotalAuraMultiplier(SPELL_AURA_MOD_WATER_BREATHING);
            return UnderWaterTime;
        }
        case FIRE_TIMER:
        {
            if (!IsAlive())
                return DISABLED_MIRROR_TIMER;
            return 1 * IN_MILLISECONDS;
        }
        default:
            return 0;
    }
}

void Player::UpdateMirrorTimers()
{
    // Desync flags for update on next HandleDrowning
    if (m_MirrorTimerFlags)
        m_MirrorTimerFlagsLast = ~m_MirrorTimerFlags;
}

void Player::StopMirrorTimers()
{
    StopMirrorTimer(FATIGUE_TIMER);
    StopMirrorTimer(BREATH_TIMER);
    StopMirrorTimer(FIRE_TIMER);
}

bool Player::IsMirrorTimerActive(MirrorTimerType type) const
{
    return m_MirrorTimer[type] == getMaxTimer(type);
}

void Player::HandleDrowning(uint32 time_diff)
{
    if (!m_MirrorTimerFlags)
        return;

    // In water
    if (m_MirrorTimerFlags & UNDERWATER_INWATER)
    {
        // Breath timer not activated - activate it
        if (m_MirrorTimer[BREATH_TIMER] == DISABLED_MIRROR_TIMER)
        {
            m_MirrorTimer[BREATH_TIMER] = getMaxTimer(BREATH_TIMER);
            SendMirrorTimer(BREATH_TIMER, m_MirrorTimer[BREATH_TIMER], m_MirrorTimer[BREATH_TIMER], -1);
        }
        else                                                              // If activated - do tick
        {
            m_MirrorTimer[BREATH_TIMER] -= time_diff;
            // Timer limit - need deal damage
            if (m_MirrorTimer[BREATH_TIMER] < 0)
            {
                m_MirrorTimer[BREATH_TIMER] += 1 * IN_MILLISECONDS;
                // Calculate and deal damage
                /// @todo Check this formula
                uint32 damage = GetMaxHealth() / 5 + urand(0, GetLevel() - 1);
                EnvironmentalDamage(DAMAGE_DROWNING, damage);
            }
            else if (!(m_MirrorTimerFlagsLast & UNDERWATER_INWATER))      // Update time in client if need
                SendMirrorTimer(BREATH_TIMER, getMaxTimer(BREATH_TIMER), m_MirrorTimer[BREATH_TIMER], -1);
        }
    }
    else if (m_MirrorTimer[BREATH_TIMER] != DISABLED_MIRROR_TIMER)        // Regen timer
    {
        int32 UnderWaterTime = getMaxTimer(BREATH_TIMER);
        // Need breath regen
        m_MirrorTimer[BREATH_TIMER] += 10 * time_diff;
        if (m_MirrorTimer[BREATH_TIMER] >= UnderWaterTime || !IsAlive())
            StopMirrorTimer(BREATH_TIMER);
        else if (m_MirrorTimerFlagsLast & UNDERWATER_INWATER)
            SendMirrorTimer(BREATH_TIMER, UnderWaterTime, m_MirrorTimer[BREATH_TIMER], 10);
    }

    // In dark water
    if (m_MirrorTimerFlags & UNDERWATER_INDARKWATER)
    {
        // Fatigue timer not activated - activate it
        if (m_MirrorTimer[FATIGUE_TIMER] == DISABLED_MIRROR_TIMER)
        {
            m_MirrorTimer[FATIGUE_TIMER] = getMaxTimer(FATIGUE_TIMER);
            SendMirrorTimer(FATIGUE_TIMER, m_MirrorTimer[FATIGUE_TIMER], m_MirrorTimer[FATIGUE_TIMER], -1);
        }
        else
        {
            m_MirrorTimer[FATIGUE_TIMER] -= time_diff;
            // Timer limit - need deal damage or teleport ghost to graveyard
            if (m_MirrorTimer[FATIGUE_TIMER] < 0)
            {
                m_MirrorTimer[FATIGUE_TIMER] += 1 * IN_MILLISECONDS;
                if (IsAlive())                                            // Calculate and deal damage
                {
                    uint32 damage = GetMaxHealth() / 5 + urand(0, GetLevel() - 1);
                    EnvironmentalDamage(DAMAGE_EXHAUSTED, damage);
                }
                else if (HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_GHOST))       // Teleport ghost to graveyard
                    RepopAtGraveyard();
            }
            else if (!(m_MirrorTimerFlagsLast & UNDERWATER_INDARKWATER))
                SendMirrorTimer(FATIGUE_TIMER, getMaxTimer(FATIGUE_TIMER), m_MirrorTimer[FATIGUE_TIMER], -1);
        }
    }
    else if (m_MirrorTimer[FATIGUE_TIMER] != DISABLED_MIRROR_TIMER)       // Regen timer
    {
        int32 DarkWaterTime = getMaxTimer(FATIGUE_TIMER);
        m_MirrorTimer[FATIGUE_TIMER] += 10 * time_diff;
        if (m_MirrorTimer[FATIGUE_TIMER] >= DarkWaterTime || !IsAlive())
            StopMirrorTimer(FATIGUE_TIMER);
        else if (m_MirrorTimerFlagsLast & UNDERWATER_INDARKWATER)
            SendMirrorTimer(FATIGUE_TIMER, DarkWaterTime, m_MirrorTimer[FATIGUE_TIMER], 10);
    }

    if (m_MirrorTimerFlags & (UNDERWATER_INLAVA /*| UNDERWATER_INSLIME*/) && !(_lastLiquid && _lastLiquid->SpellID))
    {
        // Breath timer not activated - activate it
        if (m_MirrorTimer[FIRE_TIMER] == DISABLED_MIRROR_TIMER)
            m_MirrorTimer[FIRE_TIMER] = getMaxTimer(FIRE_TIMER);
        else
        {
            m_MirrorTimer[FIRE_TIMER] -= time_diff;
            if (m_MirrorTimer[FIRE_TIMER] < 0)
            {
                m_MirrorTimer[FIRE_TIMER] += 1 * IN_MILLISECONDS;
                // Calculate and deal damage
                /// @todo Check this formula
                uint32 damage = urand(600, 700);
                if (m_MirrorTimerFlags & UNDERWATER_INLAVA)
                    EnvironmentalDamage(DAMAGE_LAVA, damage);
                // need to skip Slime damage in Undercity,
                // maybe someone can find better way to handle environmental damage
                //else if (m_zoneUpdateId != 1497)
                //    EnvironmentalDamage(DAMAGE_SLIME, damage);
            }
        }
    }
    else
        m_MirrorTimer[FIRE_TIMER] = DISABLED_MIRROR_TIMER;

    // Recheck timers flag
    m_MirrorTimerFlags &= ~UNDERWATER_EXIST_TIMERS;
    for (uint8 i = 0; i < MAX_TIMERS; ++i)
    {
        if (m_MirrorTimer[i] != DISABLED_MIRROR_TIMER)
        {
            m_MirrorTimerFlags |= UNDERWATER_EXIST_TIMERS;
            break;
        }
    }
    m_MirrorTimerFlagsLast = m_MirrorTimerFlags;
}

///The player sobers by 1% every 9 seconds
void Player::HandleSobering()
{
    m_drunkTimer = 0;

    uint8 currentDrunkValue = GetDrunkValue();
    uint8 drunk = currentDrunkValue ? --currentDrunkValue : 0;
    SetDrunkValue(drunk);
}

/*static*/ DrunkenState Player::GetDrunkenstateByValue(uint8 value)
{
    if (value >= 90)
        return DRUNKEN_SMASHED;
    if (value >= 50)
        return DRUNKEN_DRUNK;
    if (value)
        return DRUNKEN_TIPSY;
    return DRUNKEN_SOBER;
}

void Player::SetDrunkValue(uint8 newDrunkValue, uint32 itemId /*= 0*/)
{
    newDrunkValue = std::min<uint8>(newDrunkValue, 100);
    if (newDrunkValue == GetDrunkValue())
        return;

    uint32 oldDrunkenState = Player::GetDrunkenstateByValue(GetDrunkValue());
    uint32 newDrunkenState = Player::GetDrunkenstateByValue(newDrunkValue);

    SetByteValue(PLAYER_BYTES_3, PLAYER_BYTES_3_OFFSET_INEBRIATION, newDrunkValue);
    UpdateInvisibilityDrunkDetect();

    m_drunkTimer = 0; // reset sobering timer

    if (newDrunkenState == oldDrunkenState)
        return;

    WorldPackets::Misc::CrossedInebriationThreshold data;
    data.Guid = GetGUID();
    data.Threshold = newDrunkenState;
    data.ItemID = itemId;

    SendMessageToSet(data.Write(), true);
}

void Player::UpdateInvisibilityDrunkDetect()
{
    // select drunk percent or total SPELL_AURA_MOD_FAKE_INEBRIATE amount, whichever is higher for visibility updates
    uint8 drunkValue        = GetDrunkValue();
    int32 fakeDrunkValue    = GetFakeDrunkValue();
    int32 maxDrunkValue     = std::max<int32>(drunkValue, fakeDrunkValue);

    if (maxDrunkValue != 0)
    {
        m_invisibilityDetect.AddFlag(INVISIBILITY_DRUNK);
        m_invisibilityDetect.SetValue(INVISIBILITY_DRUNK, maxDrunkValue);
    }
    else
        m_invisibilityDetect.DelFlag(INVISIBILITY_DRUNK);

    if (IsInWorld())
        UpdateObjectVisibility();
}

void Player::ToggleAFK()
{
    ToggleFlag(PLAYER_FLAGS, PLAYER_FLAGS_AFK);

    // afk player not allowed in battleground
    if (!GetSession()->HasPermission(rbac::RBAC_PERM_CAN_AFK_ON_BATTLEGROUND) && isAFK() && InBattleground() && !InArena())
        LeaveBattleground();
}

void Player::ToggleDND()
{
    ToggleFlag(PLAYER_FLAGS, PLAYER_FLAGS_DND);
}

bool Player::IsImmunedToSpellEffect(SpellInfo const* spellInfo, SpellEffectInfo const& spellEffectInfo, WorldObject const* caster, bool requireImmunityPurgesEffectAttribute /*= false*/) const
{
    // players are immune to taunt (the aura and the spell effect)
    if (spellEffectInfo.IsAura(SPELL_AURA_MOD_TAUNT))
        return true;
    if (spellEffectInfo.IsEffect(SPELL_EFFECT_ATTACK_ME))
        return true;

    return Unit::IsImmunedToSpellEffect(spellInfo, spellEffectInfo, caster, requireImmunityPurgesEffectAttribute);
}

void Player::HandleFoodEmotes(uint32 diff)
{
    m_foodEmoteTimerCount += diff;

    // Handles the emotes for drinking and eating.
    // According to sniffs there is a background timer going on that repeats independed from the time window where the aura applies.
    // That's why we dont need to reset the timer on apply. In sniffs I have seen that the first call for the spell visual is totally random, then after
    // 5 seconds over and over again which confirms my theory that we have a independed timer.
    if (m_foodEmoteTimerCount >= 5000)
    {
        std::vector<AuraEffect*> auraList;
        AuraEffectList const& ModRegenAuras = GetAuraEffectsByType(SPELL_AURA_MOD_REGEN);
        AuraEffectList const& ModPowerRegenAuras = GetAuraEffectsByType(SPELL_AURA_MOD_POWER_REGEN);

        auraList.reserve(ModRegenAuras.size() + ModPowerRegenAuras.size());
        auraList.insert(auraList.end(), ModRegenAuras.begin(), ModRegenAuras.end());
        auraList.insert(auraList.end(), ModPowerRegenAuras.begin(), ModPowerRegenAuras.end());

        for (auto itr = auraList.begin(); itr != auraList.end(); ++itr)
        {
            // Food emote comes above drinking emote if we have to decide (mage regen food for example)
            if ((*itr)->GetBase()->HasEffectType(SPELL_AURA_MOD_REGEN) && (*itr)->GetSpellInfo()->AuraInterruptFlags & AURA_INTERRUPT_FLAG_NOT_SEATED)
            {
                SendPlaySpellVisual(SPELL_VISUAL_KIT_FOOD);
                break;
            }
            else if ((*itr)->GetBase()->HasEffectType(SPELL_AURA_MOD_POWER_REGEN) && (*itr)->GetSpellInfo()->AuraInterruptFlags & AURA_INTERRUPT_FLAG_NOT_SEATED)
            {
                SendPlaySpellVisual(SPELL_VISUAL_KIT_DRINK);
                break;
            }
        }
        m_foodEmoteTimerCount -= 5000;
    }
}

void Player::RegenerateAll(uint32 diff)
{
    // Not in combat or they have regeneration
    if (!IsInCombat() || HasAuraType(SPELL_AURA_MOD_REGEN_DURING_COMBAT) ||
            HasAuraType(SPELL_AURA_MOD_HEALTH_REGEN_IN_COMBAT))
    {
        RegenerateHealth(diff);
        if (!IsInCombat() && !HasAuraType(SPELL_AURA_INTERRUPT_REGEN))
            Regenerate(POWER_RAGE, diff);
    }

    Regenerate(POWER_ENERGY, diff);

    Regenerate(POWER_MANA, diff);

    m_regenTimer -= diff;
}

void Player::Regenerate(Powers power, uint32 diff)
{
    uint32 maxValue = GetMaxPower(power);
    if (!maxValue)
        return;

    uint32 curValue = GetPower(power);

    /// @todo possible use of miscvalueb instead of amount
    if (HasAuraTypeWithValue(SPELL_AURA_PREVENT_REGENERATE_POWER, power))
        return;

    float addvalue = 0.0f;

    switch (power)
    {
        case POWER_MANA:
        {
            bool recentCast = IsUnderLastManaUseEffect();
            float ManaIncreaseRate = sWorld->getRate(RATE_POWER_MANA);

            if (recentCast) // Trinity Updates Mana in intervals of 2s, which is correct
                addvalue += GetFloatValue(UNIT_FIELD_POWER_REGEN_INTERRUPTED_FLAT_MODIFIER) * ManaIncreaseRate * uint32(float(diff) / 1000);
            else
                addvalue += GetFloatValue(UNIT_FIELD_POWER_REGEN_FLAT_MODIFIER) * ManaIncreaseRate * uint32(float(diff) / 1000);
        }   break;
        case POWER_RAGE:                                    // Regenerate rage
        {
            float RageDecreaseRate = sWorld->getRate(RATE_POWER_RAGE_LOSS);
            addvalue += uint32(float(diff) / 200) * (-2.5 * RageDecreaseRate); // decay 2.5 rage per 2 seconds
            addvalue *= GetTotalAuraMultiplierByMiscValue(SPELL_AURA_MOD_POWER_REGEN_PERCENT, power);
        }   break;
        case POWER_ENERGY:                                  // Regenerate energy (rogue)
        {
            float EnergyRate = sWorld->getRate(RATE_POWER_ENERGY);
            addvalue = 20 * EnergyRate * GetTotalAuraMultiplierByMiscValue(SPELL_AURA_MOD_POWER_REGEN_PERCENT, power);
        }   break;
        case POWER_RUNE:
        case POWER_RUNIC_POWER:
        case POWER_FOCUS:
        case POWER_HAPPINESS:
        case POWER_HEALTH:
            return;
        default:
            break;
    }

    if (addvalue < 0.0f)
    {
        if (curValue == 0)
            return;
    }
    else if (addvalue > 0.0f)
    {
        if (curValue == maxValue)
            return;
    }
    else
        return;

    addvalue += m_powerFraction[power];
    uint32 integerValue = uint32(std::fabs(addvalue));

    if (addvalue < 0.0f)
    {
        if (curValue > integerValue)
        {
            curValue -= integerValue;
            m_powerFraction[power] = addvalue + integerValue;
        }
        else
        {
            curValue = 0;
            m_powerFraction[power] = 0;
        }
    }
    else
    {
        curValue += integerValue;

        if (curValue > maxValue)
        {
            curValue = maxValue;
            m_powerFraction[power] = 0;
        }
        else
            m_powerFraction[power] = addvalue - integerValue;
    }

    SetPower(power, curValue);
}

void Player::RegenerateHealth(uint32 diff)
{
    uint32 curValue = GetHealth();
    uint32 maxValue = GetMaxHealth();

    if (curValue >= maxValue)
        return;

    float HealthIncreaseRate = sWorld->getRate(RATE_HEALTH);

    float addValue = 0.0f;

    // normal regen case (maybe partly in combat case)
    if (!IsInCombat() || HasAuraType(SPELL_AURA_MOD_REGEN_DURING_COMBAT))
    {
        addValue = OCTRegenHPPerSpirit() * HealthIncreaseRate;

        if (! IsInCombat())
        {
            addValue *= GetTotalAuraMultiplier(SPELL_AURA_MOD_HEALTH_REGEN_PERCENT);

            // Food
            float food = 0.0f;
            AuraEffectList const& regenAuras = GetAuraEffectsByType(SPELL_AURA_MOD_REGEN);
            for (AuraEffect const* aurEff : regenAuras)
                food += (float(aurEff->GetAmount()) * 0.4f) * 0.5f;

            addValue += food;
        }
        else if (HasAuraType(SPELL_AURA_MOD_REGEN_DURING_COMBAT))
            addValue *= GetTotalAuraModifier(SPELL_AURA_MOD_REGEN_DURING_COMBAT) / 100.0f;

        if (! IsStandState())
            addValue *= 1.5f;
    }

    // always regeneration bonus (including combat)
    addValue += (GetTotalAuraModifier(SPELL_AURA_MOD_HEALTH_REGEN_IN_COMBAT) / 5.0f);

    addValue *= float(diff) / 1000;

    // Health fractions get carried to the next tick
    addValue += m_carryHealthRegen;
    m_carryHealthRegen = addValue - int32(addValue);

    if (addValue < 0.0f)
        addValue = 0.0f;

    ModifyHealth(int32(addValue));
}

void Player::ResetAllPowers()
{
    SetFullHealth();
    switch (GetPowerType())
    {
        case POWER_MANA:
            SetFullPower(POWER_MANA);
            break;
        case POWER_RAGE:
            SetPower(POWER_RAGE, 0);
            break;
        case POWER_ENERGY:
            SetFullPower(POWER_ENERGY);
            break;
        case POWER_RUNIC_POWER:
            SetPower(POWER_RUNIC_POWER, 0);
            break;
        default:
            break;
    }
}

bool Player::isTotalImmune() const
{
    AuraEffectList const& immune = GetAuraEffectsByType(SPELL_AURA_SCHOOL_IMMUNITY);

    uint32 immuneMask = 0;
    for (AuraEffectList::const_iterator itr = immune.begin(); itr != immune.end(); ++itr)
    {
        immuneMask |= (*itr)->GetMiscValue();
        if (immuneMask & SPELL_SCHOOL_MASK_ALL)            // total immunity
            return true;
    }
    return false;
}

uint32 Player::GetRuneBaseCooldown(uint8 index)
{
    uint8 rune = GetBaseRune(index);
    uint32 cooldown = RUNE_BASE_COOLDOWN;

    AuraEffectList const& regenAura = GetAuraEffectsByType(SPELL_AURA_MOD_POWER_REGEN_PERCENT);
    for (AuraEffectList::const_iterator i = regenAura.begin();i != regenAura.end(); ++i)
    {
        if ((*i)->GetMiscValue() == POWER_RUNE && (*i)->GetMiscValueB() == rune)
            cooldown = cooldown * (100 - (*i)->GetAmount()) / 100;
    }

    return cooldown;
}

void Player::SetRuneCooldown(uint8 index, uint32 cooldown, bool casted /*= false*/)
{
    uint32 gracePeriod = GetRuneTimer(index);

    if (casted && IsInCombat())
    {
        if (gracePeriod < 0xFFFFFFFF && cooldown > 0)
        {
            uint32 lessCd = std::min(uint32(2500), gracePeriod);
            cooldown = (cooldown > lessCd) ? (cooldown - lessCd) : 0;
            SetLastRuneGraceTimer(index, lessCd);
        }

        SetRuneTimer(index, 0);
    }

    m_runes->runes[index].Cooldown = cooldown;
    m_runes->SetRuneState(index, (cooldown == 0) ? true : false);

    ResyncRunes();
}

void Player::SetRuneConvertAura(uint8 index, AuraEffect const* aura)
{
    m_runes->runes[index].ConvertAuras.insert(aura);
}

void Player::RemoveRuneConvertAura(uint8 index, AuraEffect const* aura)
{
    m_runes->runes[index].ConvertAuras.erase(aura);
}

void Player::AddRuneByAuraEffect(uint8 index, RuneType newType, AuraEffect const* aura)
{
    SetRuneConvertAura(index, aura);
    ConvertRune(index, newType);
}

void Player::RemoveRunesByAuraEffect(AuraEffect const* aura)
{
    for (uint8 itr = 0; itr < MAX_RUNES; ++itr)
    {
        RemoveRuneConvertAura(itr, aura);

        if (m_runes->runes[itr].ConvertAuras.empty())
            ConvertRune(itr, GetBaseRune(itr));
    }
}

void Player::RestoreBaseRune(uint8 index)
{
    std::vector<AuraEffect const*> removeList;
    std::unordered_set<AuraEffect const*>& auras = m_runes->runes[index].ConvertAuras;

    auto criteria = [&removeList](AuraEffect const* storedAura) -> bool
    {
        // AuraEffect already gone
        if (!storedAura)
            return true;

        if (storedAura->GetSpellInfo()->HasAttribute(SPELL_ATTR0_PASSIVE))
        {
            // Don't drop passive talents providing rune conversion
            if (storedAura->GetAuraType() == SPELL_AURA_CONVERT_RUNE)
                removeList.push_back(storedAura);
            return true;
        }

        // If rune was converted by a non-passive aura that is still active we should keep it converted
        return false;
    };

    Trinity::Containers::EraseIf(auras, criteria);

    if (!auras.empty())
        return;

    ConvertRune(index, GetBaseRune(index));

    if (removeList.empty())
        return;

    // Filter auras set to be removed if they are converting any other rune index
    for (AuraEffect const* storedAura : removeList)
    {
        uint8 itr = 0;
        for (; itr < MAX_RUNES; ++itr)
        {
            if (m_runes->runes[itr].ConvertAuras.find(storedAura) != m_runes->runes[itr].ConvertAuras.end())
                break;
        }

        if (itr == MAX_RUNES)
            storedAura->GetBase()->Remove();
    }
}

void Player::ConvertRune(uint8 index, RuneType newType)
{
    SetCurrentRune(index, newType);

    WorldPacket data(SMSG_CONVERT_RUNE, 2);
    data << uint8(index);
    data << uint8(newType);
    SendDirectMessage(&data);
}

void Player::ResyncRunes() const
{
    //@tswow-begin
    if (!HasRunes())
        return;
    //@tswow-end

    WorldPackets::Spells::ResyncRunes packet;
    packet.Count = MAX_RUNES;
    for (uint32 itr = 0; itr < MAX_RUNES; ++itr)
    {
        WorldPackets::Spells::ResyncRune resyncRune;
        resyncRune.RuneType = GetCurrentRune(itr);
        resyncRune.Cooldown = uint8(255) - uint8(GetRuneCooldown(itr) * uint32(255) / uint32(RUNE_BASE_COOLDOWN)); // cooldown time (0-255)
        packet.Runes.emplace_back(resyncRune);
    }
    SendDirectMessage(packet.Write());
}

void Player::AddRunePower(uint8 index) const
{
    WorldPacket data(SMSG_ADD_RUNE_POWER, 4);
    data << uint32(1 << index);                             // mask (0x00-0x3F probably)
    SendDirectMessage(&data);
}

static RuneType runeSlotTypes[MAX_RUNES] =
{
    /*0*/ RUNE_BLOOD,
    /*1*/ RUNE_BLOOD,
    /*2*/ RUNE_UNHOLY,
    /*3*/ RUNE_UNHOLY,
    /*4*/ RUNE_FROST,
    /*5*/ RUNE_FROST
};

void Player::InitRunes()
{
    //@tswow-begin
    if (!HasRunes())
        return;
    //@tswow-end

    m_runes = new Runes;

    m_runes->runeState = 0;
    m_runes->lastUsedRune = RUNE_BLOOD;

    for (uint8 i = 0; i < MAX_RUNES; ++i)
    {
        SetBaseRune(i, runeSlotTypes[i]);                               // init base types
        SetCurrentRune(i, runeSlotTypes[i]);                            // init current types
        SetRuneCooldown(i, 0);                                          // reset cooldowns
        SetRuneTimer(i, 0xFFFFFFFF);                                    // Reset rune flags
        SetLastRuneGraceTimer(i, 0);
        SetRuneConvertAura(i, nullptr);
        m_runes->SetRuneState(i);
    }

    for (uint8 i = 0; i < NUM_RUNE_TYPES; ++i)
        SetFloatValue(PLAYER_RUNE_REGEN_1 + i, 0.1f);
}

bool Player::IsBaseRuneSlotsOnCooldown(RuneType runeType) const
{
    for (uint8 i = 0; i < MAX_RUNES; ++i)
        if (GetBaseRune(i) == runeType && GetRuneCooldown(i) == 0)
            return false;

    return true;
}

bool Player::CanFlyInZone(uint32 mapid, uint32 zone, SpellInfo const* bySpell) const
{
    // continent checked in SpellInfo::CheckLocation at cast and area update
    uint32 v_map = GetVirtualMapForMapAndZone(mapid, zone);
    if (v_map == 571 && !bySpell->HasAttribute(SPELL_ATTR7_IGNORE_COLD_WEATHER_FLYING))
        if (!HasSpell(54197)) // 54197 = Cold Weather Flying
            return false;

    return true;
}

uint32 Player::GetPhaseMaskForSpawn() const
{
    uint32 phase = PHASEMASK_NORMAL;
    if (!IsGameMaster())
        phase = GetPhaseMask();
    else
    {
        AuraEffectList const& phases = GetAuraEffectsByType(SPELL_AURA_PHASE);
        if (!phases.empty())
            phase = phases.front()->GetMiscValue();
    }

    // some aura phases include 1 normal map in addition to phase itself
    if (uint32 n_phase = phase & ~PHASEMASK_NORMAL)
        return n_phase;

    return PHASEMASK_NORMAL;
}

void Player::SetFallInformation(uint32 time, float z)
{
    m_lastFallTime = time;
    m_lastFallZ = z;
}

void Player::HandleFall(MovementInfo const& movementInfo)
{
    // calculate total z distance of the fall
    float z_diff = m_lastFallZ - movementInfo.pos.GetPositionZ();
    //TC_LOG_DEBUG("zDiff = {}", z_diff);

    //Players with low fall distance, Feather Fall or physical immunity (charges used) are ignored
    // 14.57 can be calculated by resolving damageperc formula below to 0
    if (z_diff >= 14.57f && !isDead() && !IsGameMaster() &&
        !HasAuraType(SPELL_AURA_HOVER) && !HasAuraType(SPELL_AURA_FEATHER_FALL) &&
        !HasAuraType(SPELL_AURA_FLY) && !IsImmunedToDamage(SPELL_SCHOOL_MASK_NORMAL))
    {
        //Safe fall, fall height reduction
        int32 safe_fall = GetTotalAuraModifier(SPELL_AURA_SAFE_FALL);

        float damageperc = 0.018f*(z_diff-safe_fall)-0.2426f;

        if (damageperc > 0)
        {
            uint32 damage = (uint32)(damageperc * GetMaxHealth()*sWorld->getRate(RATE_DAMAGE_FALL));

            if (GetCommandStatus(CHEAT_GOD))
                damage = 0;

            float height = movementInfo.pos.m_positionZ;
            UpdateGroundPositionZ(movementInfo.pos.m_positionX, movementInfo.pos.m_positionY, height);

            if (damage > 0)
            {
                //Prevent fall damage from being more than the player maximum health
                if (damage > GetMaxHealth())
                    damage = GetMaxHealth();

                // Gust of Wind
                if (HasAura(43621))
                    damage = GetMaxHealth()/2;

                uint32 original_health = GetHealth();
                uint32 final_damage = EnvironmentalDamage(DAMAGE_FALL, damage);

                // recheck alive, might have died of EnvironmentalDamage, avoid cases when player die in fact like Spirit of Redemption case
                if (IsAlive() && final_damage < original_health)
                    UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_FALL_WITHOUT_DYING, uint32(z_diff*100));
            }

            //Z given by moveinfo, LastZ, FallTime, WaterZ, MapZ, Damage, Safefall reduction
            TC_LOG_DEBUG("entities.player.falldamage", "FALLDAMAGE z={} sz={} pZ={} FallTime={} mZ={} damage={} SF={}\nPlayer debug info:\n{}", movementInfo.pos.GetPositionZ(), height, GetPositionZ(), movementInfo.fallTime, height, damage, safe_fall, GetDebugInfo());
        }
    }
}

void Player::UpdateFallInformationIfNeed(MovementInfo const& minfo, uint16 opcode)
{
    if (m_lastFallTime >= minfo.fallTime || m_lastFallZ <= minfo.pos.GetPositionZ() || opcode == MSG_MOVE_FALL_LAND)
        SetFallInformation(minfo.fallTime, minfo.pos.GetPositionZ());
}

bool Player::SetDisableGravity(bool disable, bool packetOnly /*= false*/, bool updateAnimTier /*= true*/)
{
    if (!packetOnly && !Unit::SetDisableGravity(disable, packetOnly, updateAnimTier))
        return false;

    WorldPacket data(disable ? SMSG_MOVE_GRAVITY_DISABLE : SMSG_MOVE_GRAVITY_ENABLE, 12);
    data << GetPackGUID();
    data << uint32(0);          //! movement counter
    SendDirectMessage(&data);

    data.Initialize(MSG_MOVE_GRAVITY_CHNG, 64);
    data << GetPackGUID();
    BuildMovementPacket(&data);
    SendMessageToSet(&data, false);
    return true;
}

bool Player::SetCanFly(bool apply, bool packetOnly /*= false*/)
{
    if (!apply)
        SetFallInformation(0, GetPositionZ());

    WorldPacket data(apply ? SMSG_MOVE_SET_CAN_FLY : SMSG_MOVE_UNSET_CAN_FLY, 12);
    data << GetPackGUID();
    data << uint32(0);          //! movement counter
    SendDirectMessage(&data);

    if (packetOnly || Unit::SetCanFly(apply))
    {
        data.Initialize(MSG_MOVE_UPDATE_CAN_FLY, 64);
        data << GetPackGUID();
        BuildMovementPacket(&data);
        SendMessageToSet(&data, false);
        return true;
    }
    else
        return false;
}

bool Player::SetHover(bool apply, bool packetOnly /*= false*/, bool updateAnimTier /*= true*/)
{
    if (!packetOnly && !Unit::SetHover(apply, packetOnly, updateAnimTier))
        return false;

    WorldPacket data(apply ? SMSG_MOVE_SET_HOVER : SMSG_MOVE_UNSET_HOVER, 12);
    data << GetPackGUID();
    data << uint32(0);          //! movement counter
    SendDirectMessage(&data);

    data.Initialize(MSG_MOVE_HOVER, 64);
    data << GetPackGUID();
    BuildMovementPacket(&data);
    SendMessageToSet(&data, false);
    return true;
}

bool Player::SetWaterWalking(bool apply, bool packetOnly /*= false*/)
{
    if (!packetOnly && !Unit::SetWaterWalking(apply))
        return false;

    WorldPacket data(apply ? SMSG_MOVE_WATER_WALK : SMSG_MOVE_LAND_WALK, 12);
    data << GetPackGUID();
    data << uint32(0);          //! movement counter
    SendDirectMessage(&data);

    data.Initialize(MSG_MOVE_WATER_WALK, 64);
    data << GetPackGUID();
    BuildMovementPacket(&data);
    SendMessageToSet(&data, false);
    return true;
}

bool Player::SetFeatherFall(bool apply, bool packetOnly /*= false*/)
{
    if (!packetOnly && !Unit::SetFeatherFall(apply))
        return false;

    WorldPacket data(apply ? SMSG_MOVE_FEATHER_FALL : SMSG_MOVE_NORMAL_FALL, 12);
    data << GetPackGUID();
    data << uint32(0);          //! movement counter
    SendDirectMessage(&data);

    data.Initialize(MSG_MOVE_FEATHER_FALL, 64);
    data << GetPackGUID();
    BuildMovementPacket(&data);
    SendMessageToSet(&data, false);
    return true;
}

void Player::SetRestFlag(RestFlag restFlag, uint32 triggerId /*= 0*/)
{
    uint32 oldRestMask = _restFlagMask;
    _restFlagMask |= restFlag;

    if (!oldRestMask && _restFlagMask) // only set flag/time on the first rest state
    {
        _restTime = GameTime::GetGameTime();
        SetFlag(PLAYER_FLAGS, PLAYER_FLAGS_RESTING);
    }

    if (triggerId)
        inn_triggerId = triggerId;
}

void Player::RemoveRestFlag(RestFlag restFlag)
{
    uint32 oldRestMask = _restFlagMask;
    _restFlagMask &= ~restFlag;

    if (oldRestMask && !_restFlagMask) // only remove flag/time on the last rest state remove
    {
        _restTime = 0;
        RemoveFlag(PLAYER_FLAGS, PLAYER_FLAGS_RESTING);
    }
}

void Player::SetCanSeeTransmog(bool on)
{
    if (m_canSeeTransmog == on)
        return;

    m_canSeeTransmog = on;

    // update own item display
    for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
        ForceValuesUpdateAtIndex(PLAYER_VISIBLE_ITEM_1_ENTRYID + (slot * 2));

    if (m_clientGUIDs.empty())
        return;

    for (auto itr = m_clientGUIDs.begin(); itr != m_clientGUIDs.end(); ++itr)
    {
        if (itr->GetTypeId() != TYPEID_PLAYER)
            continue;

        Player* pp = ObjectAccessor::FindPlayer(*itr);

        for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
            pp->ForceValuesUpdateAtIndex(PLAYER_VISIBLE_ITEM_1_ENTRYID + (slot * 2));
    }
}