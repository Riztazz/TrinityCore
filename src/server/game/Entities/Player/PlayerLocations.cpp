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

void Player::ResetMap()
{
    // this may be called during Map::Update
    // after decrement+unlink, ++m_mapRefIter will continue correctly
    // when the first element of the list is being removed
    // nocheck_prev will return the padding element of the RefManager
    // instead of nullptr in the case of prev
    GetMap()->UpdateIteratorBack(this);
    Unit::ResetMap();
    GetMapRef().unlink();
}

void Player::SetMap(Map* map)
{
    Unit::SetMap(map);
    m_mapRef.link(map, this);
}

void Player::AddToWorld()
{
    // TODO should we add this check?
    //if (IsInWorld())
    //    return;

    ///- Do not add/remove the player from the object storage
    ///- It will crash when updating the ObjectAccessor
    ///- The player should only be added when logging in
    Unit::AddToWorld();

    for (uint8 i = PLAYER_SLOT_START; i < PLAYER_SLOT_END; ++i)
        if (m_items[i])
            m_items[i]->AddToWorld();
}

void Player::RemoveFromWorld()
{
    // TODO should we add this check?
    //if (!IsInWorld())
    //    return;

    // cleanup
    if (IsInWorld())
    {
        ///- Release charmed creatures, unsummon totems and remove pets/guardians
        StopCastingCharm();
        StopCastingBindSight();
        UnsummonPetTemporaryIfAny();
        ClearComboPoints();
        ClearComboPointHolders();
        ObjectGuid lootGuid = GetLootGUID();
        if (!lootGuid.IsEmpty())
            m_session->DoLootRelease(lootGuid);
        sOutdoorPvPMgr->HandlePlayerLeaveZone(this, m_zoneUpdateId);
        sBattlefieldMgr->HandlePlayerLeaveZone(this, m_zoneUpdateId);
    }

    // Remove items from world before self - player must be found in Item::RemoveFromObjectUpdate
    for (uint8 i = PLAYER_SLOT_START; i < PLAYER_SLOT_END; ++i)
    {
        if (m_items[i])
            m_items[i]->RemoveFromWorld();
    }

    ///- Do not add/remove the player from the object storage
    ///- It will crash when updating the ObjectAccessor
    ///- The player should only be removed when logging out
    Unit::RemoveFromWorld();

    for (ItemMap::iterator iter = mMitems.begin(); iter != mMitems.end(); ++iter)
        iter->second->RemoveFromWorld();

    if (m_uint32Values)
    {
        if (WorldObject* viewpoint = GetViewpoint())
        {
            TC_LOG_ERROR("entities.player", "Player::RemoveFromWorld: Player '{}' ({}) has viewpoint (Entry:{}, Type: {}) when removed from world",
                GetName(), GetGUID().ToString(), viewpoint->GetEntry(), viewpoint->GetTypeId());
            SetViewpoint(viewpoint, false);
        }
    }
}

void Player::AddToPartition()
{
    if (IsInWorld())
        return;

    ///- Do not add/remove the player from the object storage
    ///- It will crash when updating the ObjectAccessor
    ///- The player should only be added when logging in
    Unit::AddToPartition();

    for (uint8 i = PLAYER_SLOT_START; i < PLAYER_SLOT_END; ++i)
        if (m_items[i])
            m_items[i]->AddToWorld();
}

void Player::RemoveFromPartition()
{
    if (!IsInWorld())
        return;

    ///- Release charmed creatures, unsummon totems and remove pets/guardians
    //StopCastingCharm();
    Unit* charmed = GetCharmed();
    if (charmed && !charmed->IsVehicle())
        StopCastingCharm();
    StopCastingBindSight();
    UnsummonPetTemporaryIfAny();
    ClearComboPoints();
    ClearComboPointHolders();
    ObjectGuid lootGuid = GetLootGUID();
    if (!lootGuid.IsEmpty())
        m_session->DoLootRelease(lootGuid);
    sOutdoorPvPMgr->HandlePlayerLeaveZone(this, m_zoneUpdateId);
    sBattlefieldMgr->HandlePlayerLeaveZone(this, m_zoneUpdateId);

    // Remove items from world before self - player must be found in Item::RemoveFromObjectUpdate
    for (uint8 i = PLAYER_SLOT_START; i < PLAYER_SLOT_END; ++i)
    {
        if (m_items[i])
            m_items[i]->RemoveFromWorld();
    }

    ///- Do not add/remove the player from the object storage
    ///- It will crash when updating the ObjectAccessor
    ///- The player should only be removed when logging out
    Unit::RemoveFromPartition();

    for (ItemMap::iterator iter = mMitems.begin(); iter != mMitems.end(); ++iter)
        iter->second->RemoveFromWorld();

    // if (m_uint32Values)
    // {
    //     if (WorldObject* viewpoint = GetViewpoint())
    //     {
    //         TC_LOG_ERROR("entities.player", "Player::RemoveFromPartition: Player '{}' ({}) has viewpoint (Entry:{}, Type: {}) when removed from world",
    //             GetName(), GetGUID().ToString(), viewpoint->GetEntry(), viewpoint->GetTypeId());
    //         SetViewpoint(viewpoint, false);
    //     }
    // }
}

// Only call from Map Delayed Update (map thread safety)
void Player::UpdateMapPartition(Map* forcedMap)
{
    // When players are in a vehicle or on transport these entities are responsible for updating partition
    if ((m_vehicle || m_transport) && !forcedMap)
        return;

    Map* currentMap = IsInWorld() ? GetMap() : nullptr;
    // We only ever change partitions if we are currently in a world map
    if (!currentMap || !currentMap->IsWorldMap())
        return;

    Map* newMap = forcedMap ? forcedMap : sMapMgr->CreateMap(currentMap->GetId(), GetPosition(), this);
    // We don't change partitions if already in the correct partition
    if (!newMap || newMap == currentMap)
        return;

    // Experiment with all of the things we should set off when we cross partitions, these are taken from teleport
    DuelComplete(DUEL_FLED);
    SetSelection(ObjectGuid::Empty);
    CombatStop();
    ResetContestedPvP();

    if (IsNonMeleeSpellCast(true))
        InterruptNonMeleeSpells(true);

    // TODO do we need to remove these?
    RemoveAurasWithInterruptFlags(AURA_INTERRUPT_FLAG_CHANGE_MAP | AURA_INTERRUPT_FLAG_MOVE | AURA_INTERRUPT_FLAG_TURNING);

    currentMap->RemovePlayerFromPartition(this);

    // Delete all existing visible objects, we don't have an existing function that does this
    // since usually we send teleport packets for changing maps
    UpdateData deleteData;
    for (auto it = m_clientGUIDs.begin(); it != m_clientGUIDs.end(); ++it)
    {
        if (m_vehicle)
        {
            // Don't delete the vehicle
            if (m_vehicle->GetBase()->GetGUID() == *it)
                continue;

            // Don't delete passengers
            bool passenger = false;
            for (auto const& [_, seat] : m_vehicle->Seats)
            {
                if (seat.Passenger.Guid == *it)
                {
                    passenger = true;
                    break;
                }
            }
            if (passenger)
                continue;
        }

        // Transport static passengers get new guids each time, and other passengers will eventually appear, this
        // isn't a priority to not clear visibility

        deleteData.AddOutOfRangeGUID(*it);
    }
    if (deleteData.HasData())
    {
        WorldPacket packet;
        deleteData.BuildPacket(&packet);
        SendDirectMessage(&packet);
    }

    // Set the new map
    ResetMap();
    SetMap(newMap);

    newMap->AddPlayerToPartition(this);

    if (!m_vehicle)
    {
        ResummonPetTemporaryUnSummonedIfAny();
    }
    // Trying to move the controlled units is a challenge, for now we will just resummon pets
    // for (ControlList::iterator itr = m_Controlled.begin(); itr != m_Controlled.end(); ++itr)
    // {
    //     // controlled players always need to move with their controller (if on boat etc)
    //     if (auto player = (*itr)->ToPlayer())
    //     {
    //         player->UpdateMapPartition(newMap);
    //     }
    //     // controlled creatures as well, except for vehicles, I hope 'controlled' vehicle are always driven and will update their
    //     // passengers, but we can test for edge cases here
    //     else if (auto creature = (*itr)->ToCreature())
    //     {
    //         if (!creature->IsVehicle())
    //         {
    //             creature->UpdateMapPartition(newMap);
    //         }
    //     }
    // }
}

bool Player::TeleportTo(uint32 mapid, float x, float y, float z, float orientation, uint32 options)
{
    if (!MapManager::IsValidMapCoord(mapid, x, y, z, orientation))
    {
        TC_LOG_ERROR("maps", "Player::TeleportTo: Invalid map ({}) or invalid coordinates (X: {}, Y: {}, Z: {}, O: {}) given when teleporting player '{}' ({}, MapID: {}, X: {}, Y: {}, Z: {}, O: {}).",
            mapid, x, y, z, orientation, GetGUID().ToString(), GetName(), GetMapId(), GetPositionX(), GetPositionY(), GetPositionZ(), GetOrientation());
        return false;
    }

    if (!GetSession()->HasPermission(rbac::RBAC_PERM_SKIP_CHECK_DISABLE_MAP) && DisableMgr::IsDisabledFor(DISABLE_TYPE_MAP, mapid, this))
    {
        TC_LOG_ERROR("entities.player.cheat", "Player::TeleportTo: Player '{}' ({}) tried to enter a forbidden map (MapID: {})", GetGUID().ToString(), GetName(), mapid);
        SendTransferAborted(mapid, TRANSFER_ABORT_MAP_NOT_ALLOWED);
        return false;
    }

    // preparing unsummon pet if lost (we must get pet before teleportation or will not find it later)
    Pet* pet = GetPet();

    MapEntry const* mEntry = sMapStore.LookupEntry(mapid);

    // don't let enter battlegrounds without assigned battleground id (for example through areatrigger)...
    // don't let gm level > 1 either
    if (!InBattleground() && mEntry->IsBattlegroundOrArena())
        return false;

    // client without expansion support
    if (GetSession()->Expansion() < mEntry->Expansion())
    {
        TC_LOG_DEBUG("maps", "Player '{}' ({}) using client without required expansion tried teleport to non accessible map (MapID: {})",
            GetName(), GetGUID().ToString(), mapid);

        if (GenericTransport* transport = GetTransport())
        {
            transport->RemovePassenger(this);
            RepopAtGraveyard();                             // teleport to near graveyard if on transport, looks blizz like :)
        }

        SendTransferAborted(mapid, TRANSFER_ABORT_INSUF_EXPAN_LVL, mEntry->Expansion());

        return false;                                       // normal client can't teleport to this map...
    }
    else
        TC_LOG_DEBUG("maps", "Player {} ({}) is being teleported to map (MapID: {})", GetName(), GetGUID().ToString(), mapid);

    if (m_vehicle)
        ExitVehicle();

    // reset movement flags at teleport, because player will continue move with these flags after teleport
    SetUnitMovementFlags(GetUnitMovementFlags() & MOVEMENTFLAG_MASK_HAS_PLAYER_STATUS_OPCODE);
    DisableSpline();
    GetMotionMaster()->Remove(EFFECT_MOTION_TYPE);

    if (GenericTransport* transport = GetTransport())
    {
        if (options & TELE_TO_NOT_LEAVE_TRANSPORT)
            AddUnitMovementFlag(MOVEMENTFLAG_ONTRANSPORT);
        else
            transport->RemovePassenger(this);
    }

    // The player was ported to another map and loses the duel immediately.
    // We have to perform this check before the teleport, otherwise the
    // ObjectAccessor won't find the flag.
    if (duel && GetMapId() != mapid && GetMap()->GetGameObject(GetGuidValue(PLAYER_DUEL_ARBITER)))
        DuelComplete(DUEL_FLED);

    if (GetMapId() == mapid)
    {
        //lets reset far teleport flag if it wasn't reset during chained teleport
        SetSemaphoreTeleportFar(false);
        //setup delayed teleport flag
        SetDelayedTeleportFlag(IsCanDelayTeleport());
        //if teleport spell is cast in Unit::Update() func
        //then we need to delay it until update process will be finished
        if (IsHasDelayedTeleport())
        {
            SetSemaphoreTeleportNear(true);
            //lets save teleport destination for player
            m_teleport_dest = WorldLocation(mapid, x, y, z, orientation);
            m_teleport_options = options;
            return true;
        }

        //same map, only remove pet if out of range for new position
        if (pet && !pet->IsWithinDist3d(x, y, z, 50.0f))
            UnsummonPetTemporaryIfAny();

        if (!IsAlive() && options & TELE_REVIVE_AT_TELEPORT)
            ResurrectPlayer(0.5f);

        if (!(options & TELE_TO_NOT_LEAVE_COMBAT))
            CombatStop();

        // this will be used instead of the current location in SaveToDB
        m_teleport_dest = WorldLocation(mapid, x, y, z, orientation);
        SetFallInformation(0, GetPositionZ());

        // code for finish transfer called in WorldSession::HandleMovementOpcodes()
        // at client packet MSG_MOVE_TELEPORT_ACK
        SetSemaphoreTeleportNear(true);
        // near teleport, triggering send MSG_MOVE_TELEPORT_ACK from client at landing
        // @epoch-begin
        if (!GetSession()->PlayerLogout())
        {
            SetCanTeleport(true);
            SendTeleportPacket(m_teleport_dest, (options & TELE_TO_TRANSPORT_TELEPORT) != 0);
        }
        // @epoch-end
    }
    else
    {
        if (GetClass() == CLASS_DEATH_KNIGHT && GetMapId() == 609 && !IsGameMaster() && !HasSpell(50977))
        {
            SendTransferAborted(mapid, TRANSFER_ABORT_UNIQUE_MESSAGE, 1);
            return false;
        }

        // far teleport to another map
        Map* oldmap = IsInWorld() ? GetMap() : nullptr;
        // check if we can enter before stopping combat / removing pet / totems / interrupting spells

        // Check enter rights before map getting to avoid creating instance copy for player
        // this check not dependent from map instance copy and same for all instance copies of selected map
        if (sMapMgr->PlayerCannotEnter(mapid, this, false))
            return false;

        //I think this always returns true. Correct me if I am wrong.
        // If the map is not created, assume it is possible to enter it.
        // It will be created in the WorldPortAck.
        //Map* map = sMapMgr->FindBaseNonInstanceMap(mapid);
        //if (!map || map->CanEnter(this))
        {
            //lets reset near teleport flag if it wasn't reset during chained teleports
            SetSemaphoreTeleportNear(false);
            //setup delayed teleport flag
            SetDelayedTeleportFlag(IsCanDelayTeleport());
            //if teleport spell is cast in Unit::Update() func
            //then we need to delay it until update process will be finished
            if (IsHasDelayedTeleport())
            {
                SetSemaphoreTeleportFar(true);
                //lets save teleport destination for player
                m_teleport_dest = WorldLocation(mapid, x, y, z, orientation);
                m_teleport_options = options;
                return true;
            }

            SetSelection(ObjectGuid::Empty);

            CombatStop();

            ResetContestedPvP();

            // remove player from battleground on far teleport (when changing maps)
            if (Battleground const* bg = GetBattleground())
            {
                // Note: at battleground join battleground id set before teleport
                // and we already will found "current" battleground
                // just need check that this is targeted map or leave
                if (bg->GetMapId() != mapid)
                    LeaveBattleground(false);                   // don't teleport to entry point
            }

            // remove arena spell coldowns/buffs now to also remove pet's cooldowns before it's temporarily unsummoned
            if (mEntry->IsBattleArena() && !IsGameMaster())
            {
                RemoveArenaSpellCooldowns(true);
                RemoveArenaAuras();
                if (pet)
                    pet->RemoveArenaAuras();
            }

            // remove pet on map change
            if (pet)
                UnsummonPetTemporaryIfAny();

            // remove all dyn objects
            RemoveAllDynObjects();

            // stop spellcasting
            // not attempt interrupt teleportation spell at caster teleport
            if (!(options & TELE_TO_SPELL))
                if (IsNonMeleeSpellCast(true))
                    InterruptNonMeleeSpells(true);

            //remove auras before removing from map...
            RemoveAurasWithInterruptFlags(AURA_INTERRUPT_FLAG_CHANGE_MAP | AURA_INTERRUPT_FLAG_MOVE | AURA_INTERRUPT_FLAG_TURNING);

            if (!GetSession()->PlayerLogout())
            {
                // send transfer packets
                WorldPacket data(SMSG_TRANSFER_PENDING, 4 + 4 + 4);
                data << uint32(mapid);
                if (GenericTransport* transport = GetTransport())
                    data << transport->GetEntry() << GetMapId();

                SendDirectMessage(&data);
            }

            // remove from old map now
            if (oldmap)
                oldmap->RemovePlayerFromMap(this, false);

            // players on mount will be dismounted. the speed and height change should not require an ACK and should be applied directly
            PurgeAndApplyPendingMovementChanges(false);

            m_teleport_dest = WorldLocation(mapid, x, y, z, orientation);
            m_teleport_options = options;
            SetFallInformation(0, GetPositionZ());
            // if the player is saved before worldportack (at logout for example)
            // this will be used instead of the current location in SaveToDB

            if (!GetSession()->PlayerLogout())
            {
                // @epoch-begin
                SetCanTeleport(true);
                // @epoch-end
                WorldPacket data(SMSG_NEW_WORLD, 4 + 4 + 4 + 4 + 4);
                data << uint32(mapid);
                if (GetTransport())
                    data << m_movementInfo.transport.pos.PositionXYZOStream();
                else
                    data << m_teleport_dest.PositionXYZOStream();

                SendDirectMessage(&data);
                SendSavedInstances();
            }

            // move packet sent by client always after far teleport
            // code for finish transfer to new map called in WorldSession::HandleMoveWorldportAckOpcode at client packet
            SetSemaphoreTeleportFar(true);
        }
        //else
        //    return false;
    }
    return true;
}

bool Player::TeleportTo(WorldLocation const& loc, uint32 options /*= 0*/)
{
    return TeleportTo(loc.GetMapId(), loc.GetPositionX(), loc.GetPositionY(), loc.GetPositionZ(), loc.GetOrientation(), options);
}

bool Player::TeleportToBGEntryPoint()
{
    if (m_bgData.joinPos.m_mapId == MAPID_INVALID)
        return false;

    ScheduleDelayedOperation(DELAYED_BG_MOUNT_RESTORE);
    ScheduleDelayedOperation(DELAYED_BG_TAXI_RESTORE);
    return TeleportTo(m_bgData.joinPos);
}

// @epoch-start
bool Player::TeleportToInstanceId(uint32 mapid, float x, float y, float z, float orientation, uint32 instanceId, uint32 options)
{
    if (!MapManager::IsValidMapCoord(mapid, x, y, z, orientation))
    {
        TC_LOG_ERROR("maps", "Player::TeleportToInstanceId: Invalid map ({}) or invalid coordinates (X: {}, Y: {}, Z: {}, O: {}) given when teleporting player '{}' ({}, MapID: {}, X: {}, Y: {}, Z: {}, O: {}).",
            mapid, x, y, z, orientation, GetGUID().ToString(), GetName(), GetMapId(), GetPositionX(), GetPositionY(), GetPositionZ(), GetOrientation());
        return false;
    }

    if (!GetSession()->HasPermission(rbac::RBAC_PERM_SKIP_CHECK_DISABLE_MAP) && DisableMgr::IsDisabledFor(DISABLE_TYPE_MAP, mapid, this))
    {
        TC_LOG_ERROR("entities.player.cheat", "Player::TeleportToInstanceId: Player '{}' ({}) tried to enter a forbidden map (MapID: {})", GetGUID().ToString(), GetName(), mapid);
        SendTransferAborted(mapid, TRANSFER_ABORT_MAP_NOT_ALLOWED);
        return false;
    }

    // preparing unsummon pet if lost (we must get pet before teleportation or will not find it later)
    Pet* pet = GetPet();

    MapEntry const* mEntry = sMapStore.LookupEntry(mapid);

    // don't let enter battlegrounds without assigned battleground id (for example through areatrigger)...
    // don't let gm level > 1 either
    if (!InBattleground() && mEntry->IsBattlegroundOrArena())
        return false;

    // client without expansion support
    if (GetSession()->Expansion() < mEntry->Expansion())
    {
        TC_LOG_DEBUG("maps", "Player '{}' ({}) using client without required expansion tried teleport to non accessible map (MapID: {})",
            GetName(), GetGUID().ToString(), mapid);

        if (GenericTransport* transport = GetTransport())
        {
            transport->RemovePassenger(this);
            RepopAtGraveyard();                             // teleport to near graveyard if on transport, looks blizz like :)
        }

        SendTransferAborted(mapid, TRANSFER_ABORT_INSUF_EXPAN_LVL, mEntry->Expansion());

        return false;                                       // normal client can't teleport to this map...
    }
    else
        TC_LOG_DEBUG("maps", "Player {} ({}) is being teleported to map (MapID: {})", GetName(), GetGUID().ToString(), mapid);

    if (m_vehicle)
        ExitVehicle();

    // reset movement flags at teleport, because player will continue move with these flags after teleport
    SetUnitMovementFlags(GetUnitMovementFlags() & MOVEMENTFLAG_MASK_HAS_PLAYER_STATUS_OPCODE);
    DisableSpline();
    GetMotionMaster()->Remove(EFFECT_MOTION_TYPE);

    if (GenericTransport* transport = GetTransport())
    {
        if (options & TELE_TO_NOT_LEAVE_TRANSPORT)
            AddUnitMovementFlag(MOVEMENTFLAG_ONTRANSPORT);
        else
            transport->RemovePassenger(this);
    }

    // The player was ported to another map and loses the duel immediately.
    // We have to perform this check before the teleport, otherwise the
    // ObjectAccessor won't find the flag.
    if (duel && GetMapId() != mapid && GetMap()->GetGameObject(GetGuidValue(PLAYER_DUEL_ARBITER)))
        DuelComplete(DUEL_FLED);

    if (GetMapId() == mapid && GetInstanceId() == instanceId)
    {
        //lets reset far teleport flag if it wasn't reset during chained teleport
        SetSemaphoreTeleportFar(false);
        //setup delayed teleport flag
        SetDelayedTeleportFlag(IsCanDelayTeleport());
        //if teleport spell is cast in Unit::Update() func
        //then we need to delay it until update process will be finished
        if (IsHasDelayedTeleport())
        {
            SetSemaphoreTeleportNear(true);
            //lets save teleport destination for player
            m_teleport_dest = WorldLocation(mapid, x, y, z, orientation);
            m_teleport_options = options;
            return true;
        }

        if (!(options & TELE_TO_NOT_UNSUMMON_PET))
        {
            //same map, only remove pet if out of range for new position
            if (pet && !pet->IsWithinDist3d(x, y, z, GetMap()->GetVisibilityRange()))
                UnsummonPetTemporaryIfAny();
        }

        if (!IsAlive() && options & TELE_REVIVE_AT_TELEPORT)
            ResurrectPlayer(0.5f);

        if (!(options & TELE_TO_NOT_LEAVE_COMBAT))
            CombatStop();

        // this will be used instead of the current location in SaveToDB
        m_teleport_dest = WorldLocation(mapid, x, y, z, orientation);
        SetFallInformation(0, GetPositionZ());

        // code for finish transfer called in WorldSession::HandleMovementOpcodes()
        // at client packet MSG_MOVE_TELEPORT_ACK
        SetSemaphoreTeleportNear(true);
        // near teleport, triggering send MSG_MOVE_TELEPORT_ACK from client at landing
        if (!GetSession()->PlayerLogout())
            SendTeleportPacket(m_teleport_dest, (options & TELE_TO_TRANSPORT_TELEPORT) != 0);
    }
    else
    {
        if (GetMapId() != mapid && GetClass() == CLASS_DEATH_KNIGHT && GetMapId() == 609 && !IsGameMaster() && !HasSpell(50977))
        {
            SendTransferAborted(mapid, TRANSFER_ABORT_UNIQUE_MESSAGE, 1);
            return false;
        }

        if (GetInstanceId() != instanceId)
            SetLocationInstanceId(instanceId);

        // far teleport to another map
        Map* oldmap = IsInWorld() ? GetMap() : nullptr;
        // check if we can enter before stopping combat / removing pet / totems / interrupting spells

        // Check enter rights before map getting to avoid creating instance copy for player
        // this check not dependent from map instance copy and same for all instance copies of selected map
        if (sMapMgr->PlayerCannotEnter(mapid, this, false))
            return false;

        //I think this always returns true. Correct me if I am wrong.
        // If the map is not created, assume it is possible to enter it.
        // It will be created in the WorldPortAck.
        //Map* map = sMapMgr->FindBaseNonInstanceMap(mapid);
        //if (!map || map->CanEnter(this))
        {
            //lets reset near teleport flag if it wasn't reset during chained teleports
            SetSemaphoreTeleportNear(false);
            //setup delayed teleport flag
            SetDelayedTeleportFlag(IsCanDelayTeleport());
            //if teleport spell is cast in Unit::Update() func
            //then we need to delay it until update process will be finished
            if (IsHasDelayedTeleport())
            {
                SetSemaphoreTeleportFar(true);
                //lets save teleport destination for player
                m_teleport_dest = WorldLocation(mapid, x, y, z, orientation);
                m_teleport_options = options;
                return true;
            }

            SetSelection(ObjectGuid::Empty);

            CombatStop();

            ResetContestedPvP();

            // remove player from battleground on far teleport (when changing maps)
            if (Battleground const* bg = GetBattleground())
            {
                // Note: at battleground join battleground id set before teleport
                // and we already will found "current" battleground
                // just need check that this is targeted map or leave
                if (bg->GetMapId() != mapid)
                    LeaveBattleground(false);                   // don't teleport to entry point
            }

            // remove arena spell coldowns/buffs now to also remove pet's cooldowns before it's temporarily unsummoned
            if (mEntry->IsBattleArena() && !IsGameMaster())
            {
                RemoveArenaSpellCooldowns(true);
                RemoveArenaAuras();
                if (pet)
                    pet->RemoveArenaAuras();
            }

            // remove pet on map change
            if (pet)
                UnsummonPetTemporaryIfAny();

            // remove all dyn objects
            RemoveAllDynObjects();

            // stop spellcasting
            // not attempt interrupt teleportation spell at caster teleport
            if (!(options & TELE_TO_SPELL))
                if (IsNonMeleeSpellCast(true))
                    InterruptNonMeleeSpells(true);

            //remove auras before removing from map...
            RemoveAurasWithInterruptFlags(AURA_INTERRUPT_FLAG_CHANGE_MAP | AURA_INTERRUPT_FLAG_MOVE | AURA_INTERRUPT_FLAG_TURNING);

            if (!GetSession()->PlayerLogout())
            {
                // send transfer packets
                WorldPacket data(SMSG_TRANSFER_PENDING, 4 + 4 + 4);
                data << uint32(mapid);
                if (GenericTransport* transport = GetTransport())
                    data << transport->GetEntry() << GetMapId();

                SendDirectMessage(&data);
            }

            // remove from old map now
            if (oldmap)
                oldmap->RemovePlayerFromMap(this, false);

            // players on mount will be dismounted. the speed and height change should not require an ACK and should be applied directly
            PurgeAndApplyPendingMovementChanges(false);

            m_teleport_dest = WorldLocation(mapid, x, y, z, orientation);
            m_teleport_options = options;
            SetFallInformation(0, GetPositionZ());
            // if the player is saved before worldportack (at logout for example)
            // this will be used instead of the current location in SaveToDB

            if (!GetSession()->PlayerLogout())
            {
                WorldPacket data(SMSG_NEW_WORLD, 4 + 4 + 4 + 4 + 4);
                data << uint32(mapid);
                if (GetTransport())
                    data << m_movementInfo.transport.pos.PositionXYZOStream();
                else
                    data << m_teleport_dest.PositionXYZOStream();

                SendDirectMessage(&data);
                SendSavedInstances();
            }

            // move packet sent by client always after far teleport
            // code for finish transfer to new map called in WorldSession::HandleMoveWorldportAckOpcode at client packet
            SetSemaphoreTeleportFar(true);
        }
        //else
        //    return false;
    }
    return true;
}
// @epoch-end

/*
- called on every successful teleportation to a map
*/
void Player::SendSavedInstances()
{
    bool hasBeenSaved = false;
    WorldPacket data;

    for (uint8 i = 0; i < MAX_DIFFICULTY; ++i)
    {
        for (BoundInstancesMap::iterator itr = m_boundInstances[i].begin(); itr != m_boundInstances[i].end(); ++itr)
        {
            if (itr->second.perm)                               // only permanent binds are sent
            {
                hasBeenSaved = true;
                break;
            }
        }
    }

    //Send opcode 811. true or false means, whether you have current raid/heroic instances
    data.Initialize(SMSG_UPDATE_INSTANCE_OWNERSHIP);
    data << uint32(hasBeenSaved);
    SendDirectMessage(&data);

    if (!hasBeenSaved)
        return;

    for (uint8 i = 0; i < MAX_DIFFICULTY; ++i)
    {
        for (BoundInstancesMap::iterator itr = m_boundInstances[i].begin(); itr != m_boundInstances[i].end(); ++itr)
        {
            if (itr->second.perm)
            {
                data.Initialize(SMSG_UPDATE_LAST_INSTANCE);
                data << uint32(itr->second.save->GetMapId());
                SendDirectMessage(&data);
            }
        }
    }
}

bool Player::TeleportToInstanceId(WorldLocation const& loc, uint32 instanceId, uint32 options /*= 0*/)
{
    return TeleportToInstanceId(loc.GetMapId(), loc.GetPositionX(), loc.GetPositionY(), loc.GetPositionZ(), loc.GetOrientation(), instanceId, options);
}

void Player::SetMovement(PlayerMovementType pType)
{
    WorldPacket data;
    switch (pType)
    {
        case MOVE_ROOT:       data.Initialize(SMSG_FORCE_MOVE_ROOT,   GetPackGUID().size()+4); break;
        case MOVE_UNROOT:     data.Initialize(SMSG_FORCE_MOVE_UNROOT, GetPackGUID().size()+4); break;
        case MOVE_WATER_WALK: data.Initialize(SMSG_MOVE_WATER_WALK,   GetPackGUID().size()+4); break;
        case MOVE_LAND_WALK:  data.Initialize(SMSG_MOVE_LAND_WALK,    GetPackGUID().size()+4); break;
        default:
            TC_LOG_ERROR("entities.player", "Player::SetMovement: Unsupported move type ({}), data not sent to client.", pType);
            return;
    }
    data << GetPackGUID();
    data << uint32(0);
    SendDirectMessage(&data);
}

bool Player::UpdatePosition(float x, float y, float z, float orientation, bool teleport)
{
    if (!Unit::UpdatePosition(x, y, z, orientation, teleport))
        return false;

    //if (movementInfo.flags & MOVEMENTFLAG_MOVING)
    //    mover->RemoveAurasWithInterruptFlags(AURA_INTERRUPT_FLAG_MOVE);
    //if (movementInfo.flags & MOVEMENTFLAG_TURNING)
    //    mover->RemoveAurasWithInterruptFlags(AURA_INTERRUPT_FLAG_TURNING);
    //AURA_INTERRUPT_FLAG_JUMP not sure

    // Update player zone if needed
    if (m_needsZoneUpdate)
    {
        uint32 newZone, newArea;
        GetZoneAndAreaId(newZone, newArea);
        UpdateZone(newZone, newArea);
        m_needsZoneUpdate = false;
    }

    // group update
    if (GetGroup())
        SetGroupUpdateFlag(GROUP_UPDATE_FLAG_POSITION);

    CheckAreaExploreAndOutdoor();

    return true;
}

void Player::CheckAreaExploreAndOutdoor()
{
    if (!IsAlive())
        return;

    if (IsInFlight())
        return;

    if (sWorld->getBoolConfig(CONFIG_VMAP_INDOOR_CHECK) && !IsOutdoors())
        RemoveAurasWithAttribute(SPELL_ATTR0_OUTDOORS_ONLY);

    uint32 const areaId = GetAreaId();
    if (!areaId)
        return;

    AreaTableEntry const* areaEntry = sAreaTableStore.LookupEntry(areaId);
    if (!areaEntry)
    {
        TC_LOG_ERROR("entities.player", "Player '{}' ({}) discovered unknown area (x: {} y: {} z: {} map: {})",
            GetName(), GetGUID().ToString(), GetPositionX(), GetPositionY(), GetPositionZ(), GetMapId());
        return;
    }

    uint32 offset = areaEntry->AreaBit / 32;

    if (offset >= PLAYER_EXPLORED_ZONES_SIZE)
    {
        TC_LOG_ERROR("entities.player", "Player::CheckAreaExploreAndOutdoor: Wrong area flag {} in map data for (X: {} Y: {}) point to field PLAYER_EXPLORED_ZONES_1 + {} ( {} must be < {} ).",
            areaEntry->AreaBit, GetPositionX(), GetPositionY(), offset, offset, PLAYER_EXPLORED_ZONES_SIZE);
        return;
    }

    uint32 val = (uint32)(1 << (areaEntry->AreaBit % 32));
    uint32 currFields = GetUInt32Value(PLAYER_EXPLORED_ZONES_1 + offset);

    if (!(currFields & val))
    {
        SetUInt32Value(PLAYER_EXPLORED_ZONES_1 + offset, (uint32)(currFields | val));

        UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_EXPLORE_AREA, GetAreaId());

        if (areaEntry->ExplorationLevel > 0)
        {
            if (IsMaxLevel())
            {
                SendExplorationExperience(areaId, 0);
            }
            else
            {
                int32 diff = int32(GetLevel()) - areaEntry->ExplorationLevel;
                uint32 XP;
                if (diff < -5)
                {
                    XP = uint32(sObjectMgr->GetBaseXP(GetLevel()+5)*sWorld->getRate(RATE_XP_EXPLORE));
                }
                else if (diff > 5)
                {
                    int32 exploration_percent = 100 - ((diff - 5) * 5);
                    if (exploration_percent < 0)
                        exploration_percent = 0;

                    XP = uint32(sObjectMgr->GetBaseXP(areaEntry->ExplorationLevel)*exploration_percent/100*sWorld->getRate(RATE_XP_EXPLORE));
                }
                else
                {
                    XP = uint32(sObjectMgr->GetBaseXP(areaEntry->ExplorationLevel)*sWorld->getRate(RATE_XP_EXPLORE));
                }

                if (sWorld->getIntConfig(CONFIG_MIN_DISCOVERED_SCALED_XP_RATIO))
                {
                    uint32 minScaledXP = uint32(sObjectMgr->GetBaseXP(areaEntry->ExplorationLevel)*sWorld->getRate(RATE_XP_EXPLORE)) * sWorld->getIntConfig(CONFIG_MIN_DISCOVERED_SCALED_XP_RATIO) / 100;
                    XP = std::max(minScaledXP, XP);
                }

                GiveXP(XP, nullptr);
                SendExplorationExperience(areaId, XP);
            }
            TC_LOG_DEBUG("entities.player", "Player '{}' ({}) discovered a new area: {}", GetName(),GetGUID().ToString(), areaId);
        }
    }
}

uint32 Player::GetZoneIdFromDB(ObjectGuid guid)
{
    ObjectGuid::LowType guidLow = guid.GetCounter();
    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHAR_ZONE);
    stmt->setUInt32(0, guidLow);
    PreparedQueryResult result = CharacterDatabase.Query(stmt);

    if (!result)
        return 0;
    Field* fields = result->Fetch();
    uint32 zone = fields[0].GetUInt16();

    if (!zone)
    {
        // stored zone is zero, use generic and slow zone detection
        stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHAR_POSITION_XYZ);
        stmt->setUInt32(0, guidLow);
        result = CharacterDatabase.Query(stmt);

        if (!result)
            return 0;
        fields = result->Fetch();
        uint32 map = fields[0].GetUInt16();
        float posx = fields[1].GetFloat();
        float posy = fields[2].GetFloat();
        float posz = fields[3].GetFloat();

        if (!sMapStore.LookupEntry(map))
            return 0;

        zone = sMapMgr->GetZoneId(PHASEMASK_NORMAL, map, posx, posy, posz);

        if (zone > 0)
        {
            stmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_ZONE);

            stmt->setUInt16(0, uint16(zone));
            stmt->setUInt32(1, guidLow);

            CharacterDatabase.Execute(stmt);
        }
    }

    return zone;
}

void Player::UpdateArea(uint32 newArea)
{
    // FFA_PVP flags are area and not zone id dependent
    // so apply them accordingly
    m_areaUpdateId = newArea;

    AreaTableEntry const* area = sAreaTableStore.LookupEntry(newArea);
    bool oldFFAPvPArea = pvpInfo.IsInFFAPvPArea;
    pvpInfo.IsInFFAPvPArea = area && (area->Flags & AREA_FLAG_FREE_FOR_ALL_PVP);
    UpdatePvPState(true);

    // check if we were in ffa arena and we left
    if (oldFFAPvPArea && !pvpInfo.IsInFFAPvPArea)
        ValidateAttackersAndOwnTarget();

    UpdateAreaDependentAuras(newArea);

    // previously this was in UpdateZone (but after UpdateArea) so nothing will break
    pvpInfo.IsInNoPvPArea = false;
    if (area && area->IsSanctuary())    // in sanctuary
    {
        SetPvpFlag(UNIT_BYTE2_FLAG_SANCTUARY);
        pvpInfo.IsInNoPvPArea = true;
        if (!duel && GetCombatManager().HasPvPCombat())
            CombatStopWithPets();
    }
    else
        RemovePvpFlag(UNIT_BYTE2_FLAG_SANCTUARY);

    uint32 const areaRestFlag = (GetTeam() == ALLIANCE) ? AREA_FLAG_ALLIANCE_RESTING : AREA_FLAG_HORDE_RESTING;
    if (area && area->Flags & areaRestFlag)
        SetRestFlag(REST_FLAG_IN_FACTION_AREA);
    else
        RemoveRestFlag(REST_FLAG_IN_FACTION_AREA);
}

void Player::UpdateZone(uint32 newZone, uint32 newArea)
{
    if (!IsInWorld())
        return;

    uint32 const oldZone = m_zoneUpdateId;
    m_zoneUpdateId = newZone;
    m_zoneUpdateTimer = ZONE_UPDATE_INTERVAL;

    GetMap()->UpdatePlayerZoneStats(oldZone, newZone);

    // call leave script hooks immedately (before updating flags)
    if (oldZone != newZone)
    {
        sOutdoorPvPMgr->HandlePlayerLeaveZone(this, oldZone);
        sBattlefieldMgr->HandlePlayerLeaveZone(this, oldZone);
    }

    // group update
    if (GetGroup())
        SetGroupUpdateFlag(GROUP_UPDATE_FULL);

    // zone changed, so area changed as well, update it
    UpdateArea(newArea);

    AreaTableEntry const* zone = sAreaTableStore.LookupEntry(newZone);
    if (!zone)
        return;

    if (sWorld->getBoolConfig(CONFIG_WEATHER))
        GetMap()->GetOrGenerateZoneDefaultWeather(newZone);

    GetMap()->SendZoneDynamicInfo(newZone, this);

    // in PvP, any not controlled zone (except zone->FactionGroupMask == 6, default case)
    // in PvE, only opposition team capital
    // @epoch-start
    bool pvparea = false;
    FIRE(
        Player
        , OnCheckAreaIsPvP, TSPlayer(this)
        , TSMutable<bool, bool>(&pvparea)
    );
    switch (zone->FactionGroupMask)
    {
        case AREATEAM_ALLY:
            pvpInfo.IsInHostileArea = GetTeam() != ALLIANCE && (sWorld->IsPvPRealm() || zone->Flags & AREA_FLAG_LINKED_CHAT);
            break;
        case AREATEAM_HORDE:
            pvpInfo.IsInHostileArea = GetTeam() != HORDE && (sWorld->IsPvPRealm() || zone->Flags & AREA_FLAG_LINKED_CHAT);
            break;
        case AREATEAM_NONE:
            // overwrite for battlegrounds, maybe batter some zone flags but current known not 100% fit to this
            pvpInfo.IsInHostileArea = sWorld->IsPvPRealm() || InBattleground() || zone->Flags & AREA_FLAG_COMBAT_ZONE;
        // @epoch-end
            break;
        default:                                            // 6 in fact
            pvpInfo.IsInHostileArea = false;
            break;
    }

    // Treat players having a quest flagging for PvP as always in hostile area
    pvpInfo.IsHostile = pvpInfo.IsInHostileArea || HasPvPForcingQuest() || pvparea;

    if (zone->Flags & AREA_FLAG_LINKED_CHAT)                     // Is in a capital city
    {
        /** @epoch-start */
        if (!pvpInfo.IsInHostileArea || zone->IsSanctuary())
        /** @epoch-end */
            SetRestFlag(REST_FLAG_IN_CITY);

        pvpInfo.IsInNoPvPArea = true;
    }
    else
        RemoveRestFlag(REST_FLAG_IN_CITY); // Recently left a capital city

    UpdatePvPState();

    // remove items with area/map limitations (delete only for alive player to allow back in ghost mode)
    // if player resurrected at teleport this will be applied in resurrect code
    if (IsAlive())
        DestroyZoneLimitedItem(true, newZone);

    // check some item equip limitations (in result lost CanTitanGrip at talent reset, for example)
    AutoUnequipOffhandIfNeed();

    // recent client version not send leave/join channel packets for built-in local channels
    UpdateLocalChannels(newZone);

    UpdateZoneDependentAuras(newZone);

    // call enter script hooks after everyting else has processed
    sScriptMgr->OnPlayerUpdateZone(this, newZone, newArea);
    if (oldZone != newZone)
    {
        sOutdoorPvPMgr->HandlePlayerEnterZone(this, newZone);
        sBattlefieldMgr->HandlePlayerEnterZone(this, newZone);
        SendInitWorldStates(newZone, newArea);              // only if really enters to new zone, not just area change, works strange...
        if (Guild* guild = GetGuild())
            guild->UpdateMemberData(this, GUILD_MEMBER_DATA_ZONEID, newZone);
    }
}

void Player::UpdateZoneDependentAuras(uint32 newZone)
{
    // Some spells applied at enter into zone (with subzones), aura removed in UpdateAreaDependentAuras that called always at zone->area update
    SpellAreaForAreaMapBounds saBounds = sSpellMgr->GetSpellAreaForAreaMapBounds(newZone);
    for (SpellAreaForAreaMap::const_iterator itr = saBounds.first; itr != saBounds.second; ++itr)
        if (itr->second->autocast && itr->second->IsFitToRequirements(this, newZone, 0))
            if (!HasAura(itr->second->spellId))
                CastSpell(this, itr->second->spellId, true);
}

void Player::UpdateAreaDependentAuras(uint32 newArea)
{
    // remove auras from spells with area limitations
    for (AuraMap::iterator iter = m_ownedAuras.begin(); iter != m_ownedAuras.end();)
    {
        // use m_zoneUpdateId for speed: UpdateArea called from UpdateZone or instead UpdateZone in both cases m_zoneUpdateId up-to-date
        if (iter->second->GetSpellInfo()->CheckLocation(GetMapId(), m_zoneUpdateId, newArea, this, false) != SPELL_CAST_OK)
            RemoveOwnedAura(iter);
        else
            ++iter;
    }

    // some auras applied at subzone enter
    SpellAreaForAreaMapBounds saBounds = sSpellMgr->GetSpellAreaForAreaMapBounds(newArea);
    for (SpellAreaForAreaMap::const_iterator itr = saBounds.first; itr != saBounds.second; ++itr)
        if (itr->second->autocast && itr->second->IsFitToRequirements(this, m_zoneUpdateId, newArea))
            if (!HasAura(itr->second->spellId))
                CastSpell(this, itr->second->spellId, true);
}

void Player::SendInitialPacketsBeforeAddToMap()
{
    ZoneScopedNC("Player::SendInitialPacketsBeforeAddToMap", WORLD_UPDATE_COLOR)

    /// Pass 'this' as argument because we're not stored in ObjectAccessor yet
    /// SMSG_CONTACT_LIST
    GetSocial()->SendSocialList(this, SOCIAL_FLAG_ALL);

    // guild bank list wtf?

    /// SMSG_BINDPOINTUPDATE
    SendBindPointUpdate();

    // SMSG_SET_PROFICIENCY
    // SMSG_SET_PCT_SPELL_MODIFIER
    // SMSG_SET_FLAT_SPELL_MODIFIER
    // SMSG_UPDATE_AURA_DURATION

    /// SMSG_TALENTS_INFO
    SendTalentsInfoData(false);

    /// SMSG_INSTANCE_DIFFICULTY
    WorldPacket data(SMSG_INSTANCE_DIFFICULTY, 4+4);
    data << uint32(GetMap()->GetDifficulty());
    data << uint32(GetMap()->GetEntry()->IsDynamicDifficultyMap() && GetMap()->IsHeroic()); // Raid dynamic difficulty
    SendDirectMessage(&data);

    /// SMSG_INITIAL_SPELLS
    SendInitialSpells();
    /// SMSG_SEND_UNLEARN_SPELLS
    SendUnlearnSpells();

    /// SMSG_ACTION_BUTTONS
    SendInitialActionButtons();

    /// SMSG_INITIALIZE_FACTIONS
    m_reputationMgr->SendInitialReputations();

    /// SMSG_ALL_ACHIEVEMENT_DATA
    m_achievementMgr->SendAllAchievementData();

    /// SMSG_EQUIPMENT_SET_LIST
    SendEquipmentSetList();

    /// SMSG_LOGIN_SET_TIME_SPEED
    static float const TimeSpeed = 0.01666667f;
    WorldPackets::Misc::LoginSetTimeSpeed loginSetTimeSpeed;
    loginSetTimeSpeed.NewSpeed = TimeSpeed;
    loginSetTimeSpeed.GameTime = *GameTime::GetWowTime();
    loginSetTimeSpeed.GameTimeHolidayOffset = 0; /// @todo
    SendDirectMessage(loginSetTimeSpeed.Write());

    /// SMSG_SET_FORCED_REACTIONS
    GetReputationMgr().SendForceReactions();

    // SMSG_TALENTS_INFO x 2 for pet (unspent points and talents in separate packets...)
    // SMSG_PET_GUIDS
    // SMSG_UPDATE_WORLD_STATE
    // SMSG_POWER_UPDATE

    /// SMSG_RESYNC_RUNES
    ResyncRunes();

    GetGameClient()->SetMovedUnit(this, true);
}

void Player::SendBindPointUpdate()
{
    WorldPackets::Misc::BindPointUpdate packet;
    packet.BindPosition = Position(m_homebindX, m_homebindY, m_homebindZ);
    packet.BindMapID = m_homebindMapId;
    packet.BindAreaID = m_homebindAreaId;
    SendDirectMessage(packet.Write());
}

void Player::SendInitialPacketsAfterAddToMap()
{
    ZoneScopedNC("Player::SendInitialPacketsAfterAddToMap", WORLD_UPDATE_COLOR)

    UpdateVisibilityForPlayer();

    // update zone
    uint32 newzone, newarea;
    GetZoneAndAreaId(newzone, newarea);
    UpdateZone(newzone, newarea);                            // also call SendInitWorldStates();

    GetSession()->ResetTimeSync();
    GetSession()->SendTimeSync();

    CastSpell(this, 836, true);                             // LOGINEFFECT

    // set some aura effects that send packet to player client after add player to map
    // SendMessageToSet not send it to player not it map, only for aura that not changed anything at re-apply
    // same auras state lost at far teleport, send it one more time in this case also
    static const AuraType auratypes[] =
    {
        SPELL_AURA_MOD_FEAR,     SPELL_AURA_TRANSFORM,                 SPELL_AURA_WATER_WALK,
        SPELL_AURA_FEATHER_FALL, SPELL_AURA_HOVER,                     SPELL_AURA_SAFE_FALL,
        SPELL_AURA_FLY,          SPELL_AURA_MOD_INCREASE_MOUNTED_FLIGHT_SPEED, SPELL_AURA_NONE
    };
    for (AuraType const* itr = &auratypes[0]; itr && itr[0] != SPELL_AURA_NONE; ++itr)
    {
        Unit::AuraEffectList const& auraList = GetAuraEffectsByType(*itr);
        if (!auraList.empty())
            auraList.front()->HandleEffect(this, AURA_EFFECT_HANDLE_SEND_FOR_CLIENT, true);
    }

    if (HasAuraType(SPELL_AURA_MOD_STUN))
        SetMovement(MOVE_ROOT);

    WorldPacket setCompoundState(SMSG_MULTIPLE_MOVES, 100);
    setCompoundState << uint32(0); // size placeholder

    // manual send package (have code in HandleEffect(this, AURA_EFFECT_HANDLE_SEND_FOR_CLIENT, true); that must not be re-applied.
    if (HasAuraType(SPELL_AURA_MOD_ROOT))
    {
        setCompoundState << uint8(2 + GetPackGUID().size() + 4);
        setCompoundState << uint16(SMSG_FORCE_MOVE_ROOT);
        setCompoundState << GetPackGUID();
        setCompoundState << uint32(0);          //! movement counter
    }

    if (HasAuraType(SPELL_AURA_FEATHER_FALL))
    {
        setCompoundState << uint8(2 + GetPackGUID().size() + 4);
        setCompoundState << uint16(SMSG_MOVE_FEATHER_FALL);
        setCompoundState << GetPackGUID();
        setCompoundState << uint32(0);          //! movement counter0
    }

    if (HasAuraType(SPELL_AURA_WATER_WALK))
    {
        setCompoundState << uint8(2 + GetPackGUID().size() + 4);
        setCompoundState << uint16(SMSG_MOVE_WATER_WALK);
        setCompoundState << GetPackGUID();
        setCompoundState << uint32(0);          //! movement counter0
    }

    if (HasAuraType(SPELL_AURA_HOVER))
    {
        setCompoundState << uint8(2 + GetPackGUID().size() + 4);
        setCompoundState << uint16(SMSG_MOVE_SET_HOVER);
        setCompoundState << GetPackGUID();
        setCompoundState << uint32(0);          //! movement counter0
    }

    if (setCompoundState.size() > 4)
    {
        setCompoundState.put<uint32>(0, setCompoundState.size() - 4);
        SendDirectMessage(&setCompoundState);
    }

    SendEnchantmentDurations();                             // must be after add to map
    SendItemDurations();                                    // must be after add to map
    SendQuestGiverStatusMultiple();
    SendTaxiNodeStatusMultiple();

    // raid downscaling - send difficulty to player
    if (GetMap()->IsRaid())
    {
        if (GetMap()->GetDifficulty() != GetRaidDifficulty())
        {
            StoreRaidMapDifficulty();
            SendRaidDifficulty(GetGroup() != nullptr, GetStoredRaidDifficulty());
        }
    }
    else if (GetRaidDifficulty() != GetStoredRaidDifficulty())
        SendRaidDifficulty(GetGroup() != nullptr);

    if (GetPlayerSharingQuest())
    {
        if (Quest const* quest = sObjectMgr->GetQuestTemplate(GetSharedQuestID()))
            PlayerTalkClass->SendQuestGiverQuestDetails(quest, GetGUID(), true);
        else
            ClearQuestSharingInfo();
    }
}

std::string Player::GetMapAreaAndZoneString() const
{
    uint32 areaId = GetAreaId();
    std::string areaName = "Unknown";
    std::string zoneName = "Unknown";
    if (AreaTableEntry const* area = sAreaTableStore.LookupEntry(areaId))
    {
        int locale = GetSession()->GetSessionDbcLocale();
        areaName = area->AreaName[locale];
        if (AreaTableEntry const* zone = sAreaTableStore.LookupEntry(area->ParentAreaID))
            zoneName = zone->AreaName[locale];
    }

    std::ostringstream str;
    str << "Map: " << GetMapId() << " (" << (FindMap() ? FindMap()->GetMapName() : "Unknown") << ") Area: " << areaId << " (" << areaName.c_str() << ") Zone: " << zoneName.c_str();
    return str.str();
}

// @epoch-start
std::string Player::GetAreaString() const
{
    uint32 areaId = GetAreaId();
    std::string areaName = "Unknown";
    if (AreaTableEntry const* area = sAreaTableStore.LookupEntry(areaId))
    {
        int locale = GetSession()->GetSessionDbcLocale();
        areaName = area->AreaName[locale];
    }

    std::ostringstream str;
    str << areaName.c_str();
    return str.str();
}
// @epoch-end

std::string Player::GetCoordsMapAreaAndZoneString() const
{
    std::ostringstream str;
    str << Position::ToString() << " " << GetMapAreaAndZoneString();
    return str.str();
}