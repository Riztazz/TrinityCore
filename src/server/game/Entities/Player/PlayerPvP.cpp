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

using namespace Trinity;

void Player::InitPvP()
{
    // pvp flag should stay after relog
    if (HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_IN_PVP) || HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_PVP_TIMER))
        UpdatePvP(true, true);
}

bool Player::UpdatePvP(bool state, bool _override, WorldObject const* source)
{
    if (IsCharmed())
        return false;

    if (source)
    {
        if (Unit const* unit = source->ToUnit())
        {
            if (unit->IsCharmed())
                return false;
        }
    }

    if (!state || _override)
    {
        SetPvP(state);
        pvpInfo.EndTimer = 0;
    }
    else
    {
        pvpInfo.EndTimer = GameTime::GetGameTime();
        SetPvP(state);
    }

    return true;
}

void Player::SetPvP(bool state)
{
    Unit::SetPvP(state);
    for (ControlList::iterator itr = m_Controlled.begin(); itr != m_Controlled.end(); ++itr)
        (*itr)->SetPvP(state);
}

void Player::UpdatePvPState(bool onlyFFA)
{
    /// @todo should we always synchronize UNIT_FIELD_BYTES_2, 1 of controller and controlled?
    // no, we shouldn't, those are checked for affecting player by client
    if (!pvpInfo.IsInNoPvPArea && !IsGameMaster()
        && (pvpInfo.IsInFFAPvPArea || sWorld->IsFFAPvPRealm()))
    {
        if (!IsFFAPvP())
        {
            SetPvpFlag(UNIT_BYTE2_FLAG_FFA_PVP);
            for (ControlList::iterator itr = m_Controlled.begin(); itr != m_Controlled.end(); ++itr)
                (*itr)->SetPvpFlag(UNIT_BYTE2_FLAG_FFA_PVP);
        }
    }
    else if (IsFFAPvP())
    {
        RemovePvpFlag(UNIT_BYTE2_FLAG_FFA_PVP);
        for (ControlList::iterator itr = m_Controlled.begin(); itr != m_Controlled.end(); ++itr)
            (*itr)->RemovePvpFlag(UNIT_BYTE2_FLAG_FFA_PVP);
    }

    if (onlyFFA)
        return;

    if (pvpInfo.IsHostile)                                  // in hostile area
    {
        if (!IsPvP() || pvpInfo.EndTimer)
            UpdatePvP(true, true);
    }
    else                                                    // in friendly area
    {
        if (IsPvP() && !HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_IN_PVP) && !pvpInfo.EndTimer)
            pvpInfo.EndTimer = GameTime::GetGameTime();     // start toggle-off
    }
}

void Player::UpdatePvPFlag(time_t currTime)
{
    if (!IsPvP())
        return;

    // @epoch-start
    if (!pvpInfo.EndTimer || (currTime < pvpInfo.EndTimer +900) || pvpInfo.IsHostile)
    // @epoch-end
        return;

    if (pvpInfo.EndTimer <= currTime)
    {
        pvpInfo.EndTimer = 0;
        RemoveFlag(PLAYER_FLAGS, PLAYER_FLAGS_PVP_TIMER);
    }

    UpdatePvP(false);
}

void Player::UpdateDuelFlag(time_t currTime)
{
    if (duel && duel->State == DUEL_STATE_COUNTDOWN && duel->StartTime <= currTime)
    {
        sScriptMgr->OnPlayerDuelStart(this, duel->Opponent);

        SetUInt32Value(PLAYER_DUEL_TEAM, 1);
        duel->Opponent->SetUInt32Value(PLAYER_DUEL_TEAM, 2);

        duel->State = DUEL_STATE_IN_PROGRESS;
        duel->Opponent->duel->State = DUEL_STATE_IN_PROGRESS;
    }

    if (duel && duel->State == DUEL_STATE_COMPLETED)
    {
        // Delay duel reset until UpdateDuelFlag so that extra attacks like sword spec do not kill duelist
        duel->Opponent->duel.reset(nullptr);
        duel.reset(nullptr);
    }
}

//If players are too far away from the duel flag... they lose the duel
void Player::CheckDuelDistance(time_t currTime)
{
    if (!duel)
        return;

    ObjectGuid duelFlagGUID = GetGuidValue(PLAYER_DUEL_ARBITER);
    GameObject* obj = GetMap()->GetGameObject(duelFlagGUID);
    if (!obj)
        return;

    if (!duel->OutOfBoundsTime)
    {
        if (!IsWithinDistInMap(obj, 50))
        {
            duel->OutOfBoundsTime = currTime + 10;

            WorldPacket data(SMSG_DUEL_OUTOFBOUNDS, 0);
            SendDirectMessage(&data);
        }
    }
    else
    {
        if (IsWithinDistInMap(obj, 40))
        {
            duel->OutOfBoundsTime = 0;

            WorldPacket data(SMSG_DUEL_INBOUNDS, 0);
            SendDirectMessage(&data);
        }
        else if (currTime >= duel->OutOfBoundsTime)
            DuelComplete(DUEL_FLED);
    }
}

void Player::DuelComplete(DuelCompleteType type)
{
    // duel not requested
    if (!duel)
        return;

    // Check if DuelComplete() has been called already up in the stack and in that case don't do anything else here
    if (duel->State == DUEL_STATE_COMPLETED)
        return;

    Player* opponent = duel->Opponent;
    duel->State = DUEL_STATE_COMPLETED;
    opponent->duel->State = DUEL_STATE_COMPLETED;

    TC_LOG_DEBUG("entities.unit", "Player::DuelComplete: Player '{}' ({}), Opponent: '{}' ({})",
        GetName(), GetGUID().ToString(), opponent->GetName(), opponent->GetGUID().ToString());

    WorldPacket data(SMSG_DUEL_COMPLETE, (1));
    data << uint8((type != DUEL_INTERRUPTED) ? 1 : 0);
    SendDirectMessage(&data);
    if (opponent->GetSession())
        opponent->SendDirectMessage(&data);

    if (type != DUEL_INTERRUPTED)
    {
        data.Initialize(SMSG_DUEL_WINNER, (1+20));          // we guess size
        data << uint8(type == DUEL_WON ? 0 : 1);            // 0 = just won; 1 = fled
        data << opponent->GetName();
        data << GetName();
        SendMessageToSet(&data, true);
    }

    sScriptMgr->OnPlayerDuelEnd(opponent, this, type);

    switch (type)
    {
        case DUEL_FLED:
            // if initiator and opponent are on the same team
            // or initiator and opponent are not PvP enabled, forcibly stop attacking
            if (GetTeam() == opponent->GetTeam())
            {
                AttackStop();
                opponent->AttackStop();
            }
            else
            {
                if (!IsPvP())
                    AttackStop();
                if (!opponent->IsPvP())
                    opponent->AttackStop();
            }
            break;
        case DUEL_WON:
            UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_LOSE_DUEL, 1);
            opponent->UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_WIN_DUEL, 1);

            // Credit for quest Death's Challenge
            if (GetClass() == CLASS_DEATH_KNIGHT && opponent->GetQuestStatus(12733) == QUEST_STATUS_INCOMPLETE)
                opponent->CastSpell(opponent, 52994, true);

            // Honor points after duel (the winner) - ImpConfig
            if (uint32 amount = sWorld->getIntConfig(CONFIG_HONOR_AFTER_DUEL))
                opponent->RewardHonor(nullptr, 1, amount);

            break;
        default:
            break;
    }

    // Beg emote spell
    if (type == DUEL_WON)
        CastSpell(this, 7267, true);

    // Victory emote spell
    if (type != DUEL_INTERRUPTED)
        opponent->CastSpell(opponent, 52852, true);

    //Remove Duel Flag object
    GameObject* obj = GetMap()->GetGameObject(GetGuidValue(PLAYER_DUEL_ARBITER));
    if (obj)
        duel->Initiator->RemoveGameObject(obj, true);

    /* remove auras */
    AuraApplicationMap &itsAuras = opponent->GetAppliedAuras();
    for (AuraApplicationMap::iterator i = itsAuras.begin(); i != itsAuras.end();)
    {
        Aura const* aura = i->second->GetBase();
        if (!i->second->IsPositive() && aura->GetCasterGUID() == GetGUID() && aura->GetApplyTime() >= duel->StartTime)
            opponent->RemoveAura(i);
        else
            ++i;
    }

    AuraApplicationMap &myAuras = GetAppliedAuras();
    for (AuraApplicationMap::iterator i = myAuras.begin(); i != myAuras.end();)
    {
        Aura const* aura = i->second->GetBase();
        if (!i->second->IsPositive() && aura->GetCasterGUID() == opponent->GetGUID() && aura->GetApplyTime() >= duel->StartTime)
            RemoveAura(i);
        else
            ++i;
    }

    // cleanup combo points
    if (GetComboTarget() && GetComboTarget()->GetControllingPlayer() == opponent)
        ClearComboPoints();

    if (opponent->GetComboTarget() && opponent->GetComboTarget()->GetControllingPlayer() == this)
        opponent->ClearComboPoints();

    //cleanups
    SetGuidValue(PLAYER_DUEL_ARBITER, ObjectGuid::Empty);
    SetUInt32Value(PLAYER_DUEL_TEAM, 0);
    opponent->SetGuidValue(PLAYER_DUEL_ARBITER, ObjectGuid::Empty);
    opponent->SetUInt32Value(PLAYER_DUEL_TEAM, 0);
}

void Player::SetContestedPvP(Player* attackedPlayer)
{
    if (attackedPlayer && (attackedPlayer == this || (duel && duel->Opponent == attackedPlayer)))
        return;

    SetContestedPvPTimer(30000);
    if (!HasUnitState(UNIT_STATE_ATTACK_PLAYER))
    {
        AddUnitState(UNIT_STATE_ATTACK_PLAYER);
        SetFlag(PLAYER_FLAGS, PLAYER_FLAGS_CONTESTED_PVP);
        sGuardMgr->SummonGuard(attackedPlayer, this);
    }
    for (Unit* unit : m_Controlled)
    {
        if (!unit->HasUnitState(UNIT_STATE_ATTACK_PLAYER))
        {
            unit->AddUnitState(UNIT_STATE_ATTACK_PLAYER);
            sGuardMgr->SummonGuard(attackedPlayer, unit);
        }
    }
}

void Player::UpdateContestedPvP(uint32 diff)
{
    if (!m_contestedPvPTimer || IsInCombat())
        return;

    if (m_contestedPvPTimer <= diff)
        ResetContestedPvP();
    else
        m_contestedPvPTimer -= diff;
}

void Player::ResetContestedPvP()
{
    ClearUnitState(UNIT_STATE_ATTACK_PLAYER);
    RemoveFlag(PLAYER_FLAGS, PLAYER_FLAGS_CONTESTED_PVP);
    m_contestedPvPTimer = 0;
}

bool Player::IsOutdoorPvPActive() const
{
    return IsAlive() && !HasInvisibilityAura() && !HasStealthAura() && IsPvP() && !HasUnitMovementFlag(MOVEMENTFLAG_FLYING) && !IsInFlight();
}

OutdoorPvP* Player::GetOutdoorPvP() const
{
    return sOutdoorPvPMgr->GetOutdoorPvPToZoneId(GetZoneId());
}

bool Player::CanCaptureTowerPoint() const
{
    return (!HasStealthAura() &&                            // not stealthed
            !HasInvisibilityAura() &&                       // not invisible
            IsAlive());                                     // live player
}

void Player::SendBattlefieldWorldStates() const
{
    /// Send misc stuff that needs to be sent on every login, like the battle timers.
    if (sWorld->getBoolConfig(CONFIG_WINTERGRASP_ENABLE))
    {
        if (Battlefield* wg = sBattlefieldMgr->GetBattlefieldByBattleId(BATTLEFIELD_BATTLEID_WG))
        {
            SendUpdateWorldState(WS_BATTLEFIELD_WG_ACTIVE, wg->IsWarTime() ? 0 : 1);
            uint32 timer = wg->IsWarTime() ? 0 : (wg->GetTimer() / 1000); // 0 - Time to next battle
            SendUpdateWorldState(WS_BATTLEFIELD_WG_TIME_NEXT_BATTLE, uint32(GameTime::GetGameTime() + timer));
        }
    }
}

void Player::SendBGWeekendWorldStates() const
{
    for (uint32 i = 1; i < sBattlemasterListStore.GetNumRows(); ++i)
    {
        BattlemasterListEntry const* bl = sBattlemasterListStore.LookupEntry(i);
        if (bl && bl->HolidayWorldState)
        {
            if (BattlegroundMgr::IsBGWeekend((BattlegroundTypeId)bl->ID))
                SendUpdateWorldState(bl->HolidayWorldState, 1);
            else
                SendUpdateWorldState(bl->HolidayWorldState, 0);
        }
    }
}

void Player::SetBattlegroundEntryPoint()
{
    // Taxi path store
    if (!m_taxi.empty())
    {
        m_bgData.mountSpell  = 0;
        m_bgData.taxiPath[0] = m_taxi.GetTaxiSource();
        m_bgData.taxiPath[1] = m_taxi.GetTaxiDestination();

        // On taxi we don't need check for dungeon
        m_bgData.joinPos = WorldLocation(GetMapId(), GetPositionX(), GetPositionY(), GetPositionZ(), GetOrientation());
    }
    else
    {
        m_bgData.ClearTaxiPath();

        // Mount spell id storing
        if (IsMounted())
        {
            AuraEffectList const& auras = GetAuraEffectsByType(SPELL_AURA_MOUNTED);
            if (!auras.empty())
                m_bgData.mountSpell = (*auras.begin())->GetId();
        }
        else
            m_bgData.mountSpell = 0;

        // If map is dungeon find linked graveyard
        if (GetMap()->IsDungeon())
        {
            if (WorldSafeLocsEntry const* entry = sObjectMgr->GetClosestGraveyard(GetPositionX(), GetPositionY(), GetPositionZ(), GetMapId(), GetTeam()))
                m_bgData.joinPos = WorldLocation(entry->Continent, entry->Loc.X, entry->Loc.Y, entry->Loc.Z, 0.0f);
            else
                TC_LOG_ERROR("entities.player", "Player::SetBattlegroundEntryPoint: Dungeon (MapID: {}) has no linked graveyard, setting home location as entry point.", GetMapId());
        }
        // If new entry point is not BG or arena set it
        else if (!GetMap()->IsBattlegroundOrArena())
            m_bgData.joinPos = WorldLocation(GetMapId(), GetPositionX(), GetPositionY(), GetPositionZ(), GetOrientation());
    }

    if (m_bgData.joinPos.m_mapId == MAPID_INVALID) // In error cases use homebind position
        m_bgData.joinPos = WorldLocation(m_homebindMapId, m_homebindX, m_homebindY, m_homebindZ, 0.0f);
}

void Player::SetBGTeam(uint32 team)
{
    m_bgData.bgTeam = team;
    SetArenaFaction(uint8(team == ALLIANCE ? 1 : 0));
}

uint32 Player::GetBGTeam() const
{
    return m_bgData.bgTeam ? m_bgData.bgTeam : GetTeam();
}

void Player::LeaveBattleground(bool teleportToEntryPoint /*= true*/, bool withoutDeserterDebuff /*= false*/)
{
    Battleground* bg = GetBattleground();
    if (!bg)
        return;

    bg->RemovePlayerAtLeave(GetGUID(), teleportToEntryPoint, true);

    // call after remove to be sure that player resurrected for correct cast
    if (bg->isBattleground() && sWorld->getBoolConfig(CONFIG_BATTLEGROUND_CAST_DESERTER))
    {
        if (!withoutDeserterDebuff && !GetSession()->HasPermission(rbac::RBAC_PERM_NO_BATTLEGROUND_DESERTER_DEBUFF))
        {
            if (bg->GetStatus() == STATUS_IN_PROGRESS || bg->GetStatus() == STATUS_WAIT_JOIN)
            {
                //lets check if player was teleported from BG and schedule delayed Deserter spell cast
                if (IsBeingTeleportedFar())
                {
                    ScheduleDelayedOperation(DELAYED_SPELL_CAST_DESERTER);
                    return;
                }

                CastSpell(this, 26013, true);               // Deserter
            }
        }
    }

    // track if player leaves the BG while inside it
    if (bg->isBattleground() && sWorld->getBoolConfig(CONFIG_BATTLEGROUND_TRACK_DESERTERS) &&
            (bg->GetStatus() == STATUS_IN_PROGRESS || bg->GetStatus() == STATUS_WAIT_JOIN))
    {
        CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_INS_DESERTER_TRACK);
        stmt->setUInt32(0, GetGUID().GetCounter());
        stmt->setUInt8(1, BG_DESERTION_TYPE_LEAVE_BG);
        CharacterDatabase.Execute(stmt);
    }
}

bool Player::CanJoinToBattleground(Battleground const* bg) const
{
    uint32 perm = rbac::RBAC_PERM_JOIN_NORMAL_BG;
    if (bg->isArena())
        perm = rbac::RBAC_PERM_JOIN_ARENAS;
    else if (bg->IsRandom())
        perm = rbac::RBAC_PERM_JOIN_RANDOM_BG;

    return GetSession()->HasPermission(perm);
}

bool Player::CanReportAfkDueToLimit()
{
    // a player can complain about 15 people per 5 minutes
    if (m_bgData.bgAfkReportedCount++ >= 15)
        return false;

    return true;
}

Battleground* Player::GetBattleground() const
{
    if (GetBattlegroundId() == 0)
        return nullptr;

    return sBattlegroundMgr->GetBattleground(GetBattlegroundId(), m_bgData.bgTypeID);
}

bool Player::InBattlegroundQueue(bool ignoreArena) const
{
    for (uint8 i = 0; i < PLAYER_MAX_BATTLEGROUND_QUEUES; ++i)
        if (m_bgBattlegroundQueueID[i].bgQueueTypeId != BATTLEGROUND_QUEUE_NONE && (!ignoreArena ||
            (m_bgBattlegroundQueueID[i].bgQueueTypeId != BATTLEGROUND_QUEUE_2v2 &&
            m_bgBattlegroundQueueID[i].bgQueueTypeId != BATTLEGROUND_QUEUE_3v3 &&
            m_bgBattlegroundQueueID[i].bgQueueTypeId != BATTLEGROUND_QUEUE_5v5)))
            return true;
    return false;
}

BattlegroundQueueTypeId Player::GetBattlegroundQueueTypeId(uint32 index) const
{
    return m_bgBattlegroundQueueID[index].bgQueueTypeId;
}

uint32 Player::GetBattlegroundQueueIndex(BattlegroundQueueTypeId bgQueueTypeId) const
{
    for (uint8 i = 0; i < PLAYER_MAX_BATTLEGROUND_QUEUES; ++i)
        if (m_bgBattlegroundQueueID[i].bgQueueTypeId == bgQueueTypeId)
            return i;
    return PLAYER_MAX_BATTLEGROUND_QUEUES;
}

bool Player::IsInvitedForBattlegroundQueueType(BattlegroundQueueTypeId bgQueueTypeId) const
{
    for (uint8 i = 0; i < PLAYER_MAX_BATTLEGROUND_QUEUES; ++i)
        if (m_bgBattlegroundQueueID[i].bgQueueTypeId == bgQueueTypeId)
            return m_bgBattlegroundQueueID[i].invitedToInstance != 0;
    return false;
}

bool Player::InBattlegroundQueueForBattlegroundQueueType(BattlegroundQueueTypeId bgQueueTypeId) const
{
    return GetBattlegroundQueueIndex(bgQueueTypeId) < PLAYER_MAX_BATTLEGROUND_QUEUES;
}

void Player::SetBattlegroundId(uint32 val, BattlegroundTypeId bgTypeId)
{
    m_bgData.bgInstanceID = val;
    m_bgData.bgTypeID = bgTypeId;
}

uint32 Player::AddBattlegroundQueueId(BattlegroundQueueTypeId val)
{
    for (uint8 i=0; i < PLAYER_MAX_BATTLEGROUND_QUEUES; ++i)
    {
        if (m_bgBattlegroundQueueID[i].bgQueueTypeId == BATTLEGROUND_QUEUE_NONE || m_bgBattlegroundQueueID[i].bgQueueTypeId == val)
        {
            m_bgBattlegroundQueueID[i].bgQueueTypeId = val;
            m_bgBattlegroundQueueID[i].invitedToInstance = 0;
            return i;
        }
    }
    return PLAYER_MAX_BATTLEGROUND_QUEUES;
}

bool Player::HasFreeBattlegroundQueueId() const
{
    for (uint8 i=0; i < PLAYER_MAX_BATTLEGROUND_QUEUES; ++i)
        if (m_bgBattlegroundQueueID[i].bgQueueTypeId == BATTLEGROUND_QUEUE_NONE)
            return true;
    return false;
}

void Player::RemoveBattlegroundQueueId(BattlegroundQueueTypeId val)
{
    for (uint8 i=0; i < PLAYER_MAX_BATTLEGROUND_QUEUES; ++i)
    {
        if (m_bgBattlegroundQueueID[i].bgQueueTypeId == val)
        {
            m_bgBattlegroundQueueID[i].bgQueueTypeId = BATTLEGROUND_QUEUE_NONE;
            m_bgBattlegroundQueueID[i].invitedToInstance = 0;
            return;
        }
    }
}

void Player::SetInviteForBattlegroundQueueType(BattlegroundQueueTypeId bgQueueTypeId, uint32 instanceId)
{
    for (uint8 i=0; i < PLAYER_MAX_BATTLEGROUND_QUEUES; ++i)
        if (m_bgBattlegroundQueueID[i].bgQueueTypeId == bgQueueTypeId)
            m_bgBattlegroundQueueID[i].invitedToInstance = instanceId;
}

bool Player::IsInvitedForBattlegroundInstance(uint32 instanceId) const
{
    for (uint8 i=0; i < PLAYER_MAX_BATTLEGROUND_QUEUES; ++i)
        if (m_bgBattlegroundQueueID[i].invitedToInstance == instanceId)
            return true;
    return false;
}

bool Player::GetBGAccessByLevel(BattlegroundTypeId bgTypeId) const
{
    // get a template bg instead of running one
    Battleground* bg = sBattlegroundMgr->GetBattlegroundTemplate(bgTypeId);
    if (!bg)
        return false;

    // limit check leel to dbc compatible level range
    uint32 level = GetLevel();
    if (level > DEFAULT_MAX_LEVEL)
        level = DEFAULT_MAX_LEVEL;

    if (level < bg->GetMinLevel() || level > bg->GetMaxLevel())
        return false;

    return true;
}

bool Player::CanUseBattlegroundObject(GameObject* gameobject) const
{
    // It is possible to call this method with a null pointer, only skipping faction check.
    if (gameobject)
    {
        FactionTemplateEntry const* playerFaction = GetFactionTemplateEntry();
        FactionTemplateEntry const* faction = sFactionTemplateStore.LookupEntry(gameobject->GetFaction());

        if (playerFaction && faction && !playerFaction->IsFriendlyTo(*faction))
            return false;
    }

    // BUG: sometimes when player clicks on flag in AB - client won't send gameobject_use, only gameobject_report_use packet
    // Note: Mount, stealth and invisibility will be removed when used
    return (!isTotalImmune() &&                            // Damage immune
            !HasAura(SPELL_RECENTLY_DROPPED_FLAG) &&       // Still has recently held flag debuff
            IsAlive());                                    // Alive
}

///This player has been blamed to be inactive in a battleground
void Player::ReportedAfkBy(Player* reporter)
{
    Battleground* bg = GetBattleground();
    // Battleground also must be in progress!
    if (!bg || bg != reporter->GetBattleground() || GetTeam() != reporter->GetTeam() || bg->GetStatus() != STATUS_IN_PROGRESS)
        return;

    // check if player has 'Idle' or 'Inactive' debuff
    if (m_bgData.bgAfkReporter.find(reporter->GetGUID().GetCounter()) == m_bgData.bgAfkReporter.end() && !HasAura(43680) && !HasAura(43681) && reporter->CanReportAfkDueToLimit())
    {
        m_bgData.bgAfkReporter.insert(reporter->GetGUID().GetCounter());
        // by default 3 players have to complain to apply debuff
        if (m_bgData.bgAfkReporter.size() >= sWorld->getIntConfig(CONFIG_BATTLEGROUND_REPORT_AFK))
        {
            // cast 'Idle' spell
            CastSpell(this, 43680, true);
            m_bgData.bgAfkReporter.clear();
        }
    }
}

///checks the 15 afk reports per 5 minutes limit
void Player::UpdateAfkReport(time_t currTime)
{
    if (m_bgData.bgAfkReportedTimer <= currTime)
    {
        m_bgData.bgAfkReportedCount = 0;
        m_bgData.bgAfkReportedTimer = currTime+5*MINUTE;
    }
}

bool Player::InArena() const
{
    Battleground* bg = GetBattleground();
    if (!bg || !bg->isArena())
        return false;

    return true;
}

void Player::SetInArenaTeam(uint32 ArenaTeamId, uint8 slot, uint8 type)
{
    SetArenaTeamInfoField(slot, ARENA_TEAM_ID, ArenaTeamId);
    SetArenaTeamInfoField(slot, ARENA_TEAM_TYPE, type);
}

void Player::SetArenaTeamInfoField(uint8 slot, ArenaTeamInfoType type, uint32 value)
{
    SetUInt32Value(PLAYER_FIELD_ARENA_TEAM_INFO_1_1 + (slot * ARENA_TEAM_END) + type, value);
}

void Player::LeaveAllArenaTeams(ObjectGuid guid)
{
    CharacterCacheEntry const* characterInfo = sCharacterCache->GetCharacterCacheByGuid(guid);
    if (!characterInfo)
        return;

    for (uint8 i = 0; i < MAX_ARENA_SLOT; ++i)
    {
        uint32 arenaTeamId = characterInfo->ArenaTeamId[i];
        if (arenaTeamId != 0)
        {
            ArenaTeam* arenaTeam = sArenaTeamMgr->GetArenaTeamById(arenaTeamId);
            if (arenaTeam)
                arenaTeam->DelMember(guid, true);
        }
    }
}

void Player::UpdateHonorFields()
{
    /// called when rewarding honor and at each save
    time_t now = GameTime::GetGameTime();
    time_t today = GameTime::GetGameTime() / DAY * DAY;

    if (m_lastHonorUpdateTime < today)
    {
        time_t yesterday = today - DAY;

        uint16 kills_today = PAIR32_LOPART(GetUInt32Value(PLAYER_FIELD_KILLS));

        // update yesterday's contribution
        if (m_lastHonorUpdateTime >= yesterday)
        {
            SetUInt32Value(PLAYER_FIELD_YESTERDAY_CONTRIBUTION, GetUInt32Value(PLAYER_FIELD_TODAY_CONTRIBUTION));

            // this is the first update today, reset today's contribution
            SetUInt32Value(PLAYER_FIELD_TODAY_CONTRIBUTION, 0);
            SetUInt32Value(PLAYER_FIELD_KILLS, MAKE_PAIR32(0, kills_today));
        }
        else
        {
            // no honor/kills yesterday or today, reset
            SetUInt32Value(PLAYER_FIELD_YESTERDAY_CONTRIBUTION, 0);
            SetUInt32Value(PLAYER_FIELD_KILLS, 0);
        }
    }

    m_lastHonorUpdateTime = now;
}

///Calculate the amount of honor gained based on the victim
///and the size of the group for which the honor is divided
///An exact honor value can also be given (overriding the calcs)
bool Player::RewardHonor(Unit* victim, uint32 groupsize, int32 honor, bool pvptoken)
{
    // @epoch-start
    bool cancel = false;
    FIRE(
        Player
        , OnRewardHonorEarly
        , TSMutable<bool, bool>(&cancel)
    );
    // @epoch-end

    // do not reward honor in arenas, but enable onkill spellproc
    if (cancel || InArena())
    {
        if (!victim || victim == this || victim->GetTypeId() != TYPEID_PLAYER)
            return false;

        if (GetBGTeam() == victim->ToPlayer()->GetBGTeam())
            return false;

        return true;
    }

    // 'Inactive' this aura prevents the player from gaining honor points and battleground tokens
    if (HasAura(SPELL_AURA_PLAYER_INACTIVE))
        return false;

    ObjectGuid victim_guid;
    uint32 victim_rank = 0;

    // need call before fields update to have chance move yesterday data to appropriate fields before today data change.
    UpdateHonorFields();

    // do not reward honor in arenas, but return true to enable onkill spellproc
    if (InBattleground() && GetBattleground() && GetBattleground()->isArena())
        return true;

    // Promote to float for calculations
    float honor_f = (float)honor;

    if (honor_f <= 0)
    {
        if (!victim || victim == this || victim->HasAuraType(SPELL_AURA_NO_PVP_CREDIT))
            return false;

        victim_guid = victim->GetGUID();

        if (Player* plrVictim = victim->ToPlayer())
        {
            if (GetTeam() == plrVictim->GetTeam() && !sWorld->IsFFAPvPRealm())
                return false;

            uint8 k_level = GetLevel();
            // @tswow-begin
            uint8 k_grey = Trinity::XP::GetGrayLevel(this, k_level);
            // @tswow-end
            uint8 v_level = victim->GetLevel();

            if (v_level <= k_grey)
                return false;

            // PLAYER_CHOSEN_TITLE VALUES DESCRIPTION
            //  [0]      Just name
            //  [1..14]  Alliance honor titles and player name
            //  [15..28] Horde honor titles and player name
            //  [29..38] Other title and player name
            //  [39+]    Nothing
            /** @epoch-start */
            victim_rank = victim->GetByteValue(PLAYER_FIELD_BYTES,PLAYER_FIELD_BYTES_OFFSET_LIFETIME_MAX_PVP_RANK);
                                                        // Get Killer titles, CharTitlesEntry::MaskID
            // Ranks:
            //  title[1..14]  -> rank[5..18]
            //  title[15..28] -> rank[5..18]
            //  title[other]  -> 0
            //if (victim_title == 0)
            //    victim_guid.Clear();                     // Don't show HK: <rank> message, only log.
            //else if (victim_title < 15)
            //    victim_rank = victim_title + 4;
            //else if (victim_title < 29)
            //    victim_rank = victim_title - 14 + 4;
            //else
            //    victim_guid.Clear();                     // Don't show HK: <rank> message, only log.
            /** @epoch-end */

            honor_f = std::ceil(Trinity::Honor::hk_honor_at_level_f(k_level) * (v_level - k_grey) / (k_level - k_grey));

            // count the number of playerkills in one day
            ApplyModUInt32Value(PLAYER_FIELD_KILLS, 1, true);
            // and those in a lifetime
            ApplyModUInt32Value(PLAYER_FIELD_LIFETIME_HONORABLE_KILLS, 1, true);
            UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_EARN_HONORABLE_KILL);
            UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_HK_CLASS, victim->GetClass());
            UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_HK_RACE, victim->GetRace());
            UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_HONORABLE_KILL_AT_AREA, GetAreaId());
            UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_HONORABLE_KILL, 1, 0, victim);
            UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_SPECIAL_PVP_KILL, 1, 0, victim);
        }
        else
        {
            if (!victim->ToCreature()->IsRacialLeader())
                return false;

            honor_f = 100.0f;                               // ??? need more info
            victim_rank = 19;                               // HK: Leader
        }
    }

    if (victim != nullptr)
    {
        if (groupsize > 1)
            honor_f /= groupsize;

        // apply honor multiplier from aura (not stacking-get highest)
        AddPct(honor_f, GetMaxPositiveAuraModifier(SPELL_AURA_MOD_HONOR_GAIN_PCT));
    }

    honor_f *= sWorld->getRate(RATE_HONOR);
    // Back to int now
    honor = int32(honor_f);
    // honor - for show honor points in log
    // victim_guid - for show victim name in log
    // victim_rank [1..4]  HK: <dishonored rank>
    // victim_rank [5..19] HK: <alliance\horde rank>
    // victim_rank [0, 20+] HK: <>
    WorldPacket data(SMSG_PVP_CREDIT, 4+8+4);
    data << uint32(honor);
    data << uint64(victim_guid);
    data << uint32(victim_rank);

    SendDirectMessage(&data);

    // add honor points
    ModifyHonorPoints(honor);

    ApplyModUInt32Value(PLAYER_FIELD_TODAY_CONTRIBUTION, honor, true);

    if (InBattleground() && honor > 0)
    {
        if (Battleground* bg = GetBattleground())
        {
            bg->UpdatePlayerScore(this, SCORE_BONUS_HONOR, honor, false); //false: prevent looping
        }
    }

    if (sWorld->getBoolConfig(CONFIG_PVP_TOKEN_ENABLE) && pvptoken)
    {
        if (!victim || victim == this || victim->HasAuraType(SPELL_AURA_NO_PVP_CREDIT))
            return true;

        if (victim->GetTypeId() == TYPEID_PLAYER)
        {
            // Check if allowed to receive it in current map
            uint8 MapType = sWorld->getIntConfig(CONFIG_PVP_TOKEN_MAP_TYPE);
            if ((MapType == 1 && !InBattleground() && !IsFFAPvP())
                || (MapType == 2 && !IsFFAPvP())
                || (MapType == 3 && !InBattleground()))
                return true;

            uint32 itemId = sWorld->getIntConfig(CONFIG_PVP_TOKEN_ID);
            int32 count = sWorld->getIntConfig(CONFIG_PVP_TOKEN_COUNT);

            if (AddItem(itemId, count))
                ChatHandler(GetSession()).PSendSysMessage("You have been awarded a token for slaying another player.");
        }
    }

    return true;
}

void Player::SetHonorPoints(uint32 value)
{
    if (value > sWorld->getIntConfig(CONFIG_MAX_HONOR_POINTS))
        value = sWorld->getIntConfig(CONFIG_MAX_HONOR_POINTS);
    SetUInt32Value(PLAYER_FIELD_HONOR_CURRENCY, value);
    if (value)
        AddKnownCurrency(ITEM_HONOR_POINTS_ID);
}

void Player::ModifyHonorPoints(int32 value, CharacterDatabaseTransaction trans)
{
    int32 newValue = int32(GetHonorPoints()) + value;
    if (newValue < 0)
        newValue = 0;
    SetHonorPoints(uint32(newValue));

    if (trans)
    {
        CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_CHAR_HONOR_POINTS);
        stmt->setUInt32(0, newValue);
        stmt->setUInt32(1, GetGUID().GetCounter());
        trans->Append(stmt);
    }
}

void Player::SetArenaPoints(uint32 value)
{
    if (value > sWorld->getIntConfig(CONFIG_MAX_ARENA_POINTS))
        value = sWorld->getIntConfig(CONFIG_MAX_ARENA_POINTS);
    SetUInt32Value(PLAYER_FIELD_ARENA_CURRENCY, value);
    if (value)
        AddKnownCurrency(ITEM_ARENA_POINTS_ID);
}

void Player::ModifyArenaPoints(int32 value, CharacterDatabaseTransaction trans)
{
    int32 newValue = int32(GetArenaPoints()) + value;
    if (newValue < 0)
        newValue = 0;
    SetArenaPoints(uint32(newValue));

    if (trans)
    {
        CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_CHAR_ARENA_POINTS);
        stmt->setUInt32(0, newValue);
        stmt->setUInt32(1, GetGUID().GetCounter());
        trans->Append(stmt);
    }
}