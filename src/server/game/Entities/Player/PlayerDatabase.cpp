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

enum CharacterFlags
{
    CHARACTER_FLAG_NONE                 = 0x00000000,
    CHARACTER_FLAG_UNK1                 = 0x00000001,
    CHARACTER_FLAG_UNK2                 = 0x00000002,
    CHARACTER_LOCKED_FOR_TRANSFER       = 0x00000004,
    CHARACTER_FLAG_UNK4                 = 0x00000008,
    CHARACTER_FLAG_UNK5                 = 0x00000010,
    CHARACTER_FLAG_UNK6                 = 0x00000020,
    CHARACTER_FLAG_UNK7                 = 0x00000040,
    CHARACTER_FLAG_UNK8                 = 0x00000080,
    CHARACTER_FLAG_UNK9                 = 0x00000100,
    CHARACTER_FLAG_UNK10                = 0x00000200,
    CHARACTER_FLAG_HIDE_HELM            = 0x00000400,
    CHARACTER_FLAG_HIDE_CLOAK           = 0x00000800,
    CHARACTER_FLAG_UNK13                = 0x00001000,
    CHARACTER_FLAG_GHOST                = 0x00002000,
    CHARACTER_FLAG_RENAME               = 0x00004000,
    CHARACTER_FLAG_UNK16                = 0x00008000,
    CHARACTER_FLAG_UNK17                = 0x00010000,
    CHARACTER_FLAG_UNK18                = 0x00020000,
    CHARACTER_FLAG_UNK19                = 0x00040000,
    CHARACTER_FLAG_UNK20                = 0x00080000,
    CHARACTER_FLAG_UNK21                = 0x00100000,
    CHARACTER_FLAG_UNK22                = 0x00200000,
    CHARACTER_FLAG_UNK23                = 0x00400000,
    CHARACTER_FLAG_UNK24                = 0x00800000,
    CHARACTER_FLAG_LOCKED_BY_BILLING    = 0x01000000,
    CHARACTER_FLAG_DECLINED             = 0x02000000,
    CHARACTER_FLAG_UNK27                = 0x04000000,
    CHARACTER_FLAG_UNK28                = 0x08000000,
    CHARACTER_FLAG_UNK29                = 0x10000000,
    CHARACTER_FLAG_UNK30                = 0x20000000,
    CHARACTER_FLAG_UNK31                = 0x40000000,
    CHARACTER_FLAG_UNK32                = 0x80000000
};

enum CharacterCustomizeFlags
{
    CHAR_CUSTOMIZE_FLAG_NONE            = 0x00000000,
    CHAR_CUSTOMIZE_FLAG_CUSTOMIZE       = 0x00000001,       // name, gender, etc...
    CHAR_CUSTOMIZE_FLAG_FACTION         = 0x00010000,       // name, gender, faction, etc...
    CHAR_CUSTOMIZE_FLAG_RACE            = 0x00100000        // name, gender, race, etc...
};

bool Player::LoadFromDB(ObjectGuid guid, CharacterDatabaseQueryHolder const& holder)
{
    ZoneScopedNC("Player::LoadFromDB", WORLD_UPDATE_COLOR)

    //                                                       0     1        2     3     4      5       6      7   8      9     10    11         12         13           14         15         16
    //QueryResult* result = CharacterDatabase.PQuery("SELECT guid, account, name, race, class, gender, level, xp, money, skin, face, hairStyle, hairColor, facialStyle, bankSlots, restState, playerFlags, "
    // 17          18          19          20   21           22        23         24         25         26          27           28                 29
    //"position_x, position_y, position_z, map, orientation, taximask, cinematic, totaltime, leveltime, rest_bonus, logout_time, is_logout_resting, resettalents_cost, "
    // 30                 31       32       33       34       35         36           37            38        39    40      41                 42         43
    //"resettalents_time, trans_x, trans_y, trans_z, trans_o, transguid, extra_flags, stable_slots, at_login, zone, online, death_expire_time, taxi_path, instance_mode_mask, "
    // 44           45                46                47                    48          49          50              51           52               53              54
    //"arenaPoints, totalHonorPoints, todayHonorPoints, yesterdayHonorPoints, totalKills, todayKills, yesterdayKills, chosenTitle, knownCurrencies, watchedFaction, drunk, "
    // 55      56      57      58      59      60      61      62      63           64                 65                 66             67              68      69           70          71               72
    //"health, power1, power2, power3, power4, power5, power6, power7, instance_id, talentGroupsCount, activeTalentGroup, exploredZones, equipmentCache, ammoId, knownTitles, actionBars, grantableLevels, fishing_steps FROM characters WHERE guid = '{}'", guid);
    PreparedQueryResult result = holder.GetPreparedResult(PLAYER_LOGIN_QUERY_LOAD_FROM);
    if (!result)
    {
        std::string name = "<unknown>";
        sCharacterCache->GetCharacterNameByGuid(guid, name);
        TC_LOG_ERROR("entities.player.loading", "Player::LoadFromDB: Player '{}' ({}) not found in table `characters`, can't load. ", name, guid.ToString());
        return false;
    }

    Field* fields = result->Fetch();

    uint32 dbAccountId = fields[1].GetUInt32();

    // check if the character's account in the db and the logged in account match.
    // player should be able to load/delete character only with correct account!
    if (dbAccountId != GetSession()->GetAccountId())
    {
        TC_LOG_ERROR("entities.player.loading", "Player::LoadFromDB: Player ({}) loading from wrong account (is: {}, should be: {})", guid.ToString(), GetSession()->GetAccountId(), dbAccountId);
        return false;
    }

    if (holder.GetPreparedResult(PLAYER_LOGIN_QUERY_LOAD_BANNED))
    {
        TC_LOG_ERROR("entities.player.loading", "Player::LoadFromDB: Player ({}) is banned, can't load.", guid.ToString());
        return false;
    }

    Object::_Create(guid.GetCounter(), 0, HighGuid::Player);

    m_name = fields[2].GetString();

    /** @epoch-start */
    // check name limitations
    if (ObjectMgr::CheckPlayerName(m_name, GetSession()->GetSessionDbcLocale()) != CHAR_NAME_SUCCESS ||
        (!GetSession()->HasPermission(rbac::RBAC_PERM_SKIP_CHECK_CHARACTER_CREATION_RESERVEDNAME) && sObjectMgr->IsReservedName(m_name)))
    {
        CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_ADD_AT_LOGIN_FLAG);
        stmt->setUInt16(0, uint16(AT_LOGIN_RENAME));
        stmt->setUInt32(1, guid.GetCounter());
        CharacterDatabase.Execute(stmt);
        return false;
    }
    /** @epoch-end */

    Gender gender = Gender(fields[5].GetUInt8());
    if (!IsValidGender(gender))
    {
        TC_LOG_ERROR("entities.player.loading", "Player::LoadFromDB: Player ({}) has wrong gender ({}), can't load.", guid.ToString(), uint32(gender));
        return false;
    }

    SetRace(fields[3].GetUInt8());
    SetClass(fields[4].GetUInt8());
    SetGender(gender);

    // check if race/class combination is valid
    PlayerInfo const* info = sObjectMgr->GetPlayerInfo(GetRace(), GetClass());
    if (!info)
    {
        TC_LOG_ERROR("entities.player.loading", "Player::LoadFromDB: Player ({}) has wrong race/class ({}/{}), can't load.", guid.ToString(), GetRace(), GetClass());
        return false;
    }

    SetLevel(fields[6].GetUInt8(), false);
    SetXP(fields[7].GetUInt32());

    if (!_LoadIntoDataField(fields[66].GetString(), PLAYER_EXPLORED_ZONES_1, PLAYER_EXPLORED_ZONES_SIZE))
        TC_LOG_WARN("entities.player.loading", "Player::LoadFromDB: Player ({}) has invalid exploredzones data ({}). Forcing partial load.", guid.ToString(), fields[66].GetCString());

    if (!_LoadIntoDataField(fields[69].GetString(), PLAYER__FIELD_KNOWN_TITLES, KNOWN_TITLES_SIZE * 2))
        TC_LOG_WARN("entities.player.loading", "Player::LoadFromDB: Player ({}) has invalid knowntitles mask ({}). Forcing partial load.", guid.ToString(), fields[69].GetCString());

    SetObjectScale(1.0f);
    SetHoverHeight(1.0f);

    // load achievements before anything else to prevent multiple gains for the same achievement/criteria on every loading (as loading does call UpdateAchievementCriteria)
    m_achievementMgr->LoadFromDB(holder.GetPreparedResult(PLAYER_LOGIN_QUERY_LOAD_ACHIEVEMENTS), holder.GetPreparedResult(PLAYER_LOGIN_QUERY_LOAD_CRITERIA_PROGRESS));

    uint32 money = fields[8].GetUInt32();
    if (money > MAX_MONEY_AMOUNT)
        money = MAX_MONEY_AMOUNT;
    SetMoney(money);

    SetSkinId(fields[9].GetUInt8());
    SetFaceId(fields[10].GetUInt8());
    SetHairStyleId(fields[11].GetUInt8());
    SetHairColorId(fields[12].GetUInt8());
    SetFacialStyle(fields[13].GetUInt8());
    SetBankBagSlotCount(fields[14].GetUInt8());
    SetRestState(fields[15].GetUInt8());
    SetNativeGender(Gender(fields[5].GetUInt8()));
    SetByteValue(PLAYER_BYTES_3, PLAYER_BYTES_3_OFFSET_INEBRIATION, fields[54].GetUInt8());

    if (!ValidateAppearance(
        fields[3].GetUInt8(), // race
        fields[4].GetUInt8(), // class
        gender, GetHairStyleId(), GetHairColorId(), GetFaceId(), GetFacialStyle(), GetSkinId()))
    {
        TC_LOG_ERROR("entities.player.loading", "Player::LoadFromDB: Player ({}) has wrong Appearance values (Hair/Skin/Color), can't load.", guid.ToString());
        return false;
    }

    SetUInt32Value(PLAYER_FLAGS, fields[16].GetUInt32());
    SetInt32Value(PLAYER_FIELD_WATCHED_FACTION_INDEX, fields[53].GetUInt32());

    SetUInt64Value(PLAYER_FIELD_KNOWN_CURRENCIES, fields[52].GetUInt64());

    SetUInt32Value(PLAYER_AMMO_ID, fields[68].GetUInt32());

    // set which actionbars the client has active - DO NOT REMOVE EVER AGAIN (can be changed though, if it does change fieldwise)
    SetByteValue(PLAYER_FIELD_BYTES, PLAYER_FIELD_BYTES_OFFSET_ACTION_BAR_TOGGLES, fields[70].GetUInt8());

    m_fishingSteps = fields[72].GetUInt8();

    InitDisplayIds();

    // cleanup inventory related item value fields (it will be filled correctly in _LoadInventory)
    for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
    {
        SetGuidValue(PLAYER_FIELD_INV_SLOT_HEAD + (slot * 2), ObjectGuid::Empty);
        SetVisibleItemSlot(slot, nullptr);

        delete m_items[slot];
        m_items[slot] = nullptr;
    }

    TC_LOG_DEBUG("entities.player.loading", "Player::LoadFromDB: Load Basic value of player '{}' is: ", m_name);
    outDebugValues();

    //Need to call it to initialize m_team (m_team can be calculated from race)
    //Other way is to saves m_team into characters table.
    SetFactionForRace(GetRace());

    // load home bind and check in same time class/race pair, it used later for restore broken positions
    if (!_LoadHomeBind(holder.GetPreparedResult(PLAYER_LOGIN_QUERY_LOAD_HOME_BIND)))
        return false;

    InitPrimaryProfessions();                               // to max set before any spell loaded

    // init saved position, and fix it later if problematic
    ObjectGuid::LowType transLowGUID = fields[35].GetUInt32();
    Relocate(fields[17].GetFloat(), fields[18].GetFloat(), fields[19].GetFloat(), fields[21].GetFloat());
    uint32 mapId = fields[20].GetUInt16();
    uint32 instanceId = fields[63].GetUInt32();

    uint32 dungeonDiff = fields[43].GetUInt8() & 0x0F;
    if (dungeonDiff >= MAX_DUNGEON_DIFFICULTY)
        dungeonDiff = DUNGEON_DIFFICULTY_NORMAL;
    uint32 raidDiff = (fields[43].GetUInt8() >> 4) & 0x0F;
    if (raidDiff >= MAX_RAID_DIFFICULTY)
        raidDiff = RAID_DIFFICULTY_10MAN_NORMAL;
    SetDungeonDifficulty(Difficulty(dungeonDiff));          // may be changed in _LoadGroup
    SetRaidDifficulty(Difficulty(raidDiff));                // may be changed in _LoadGroup

    std::string taxi_nodes = fields[42].GetString();

    auto RelocateToHomebind = [this, &mapId, &instanceId]() { mapId = m_homebindMapId; instanceId = 0; Relocate(m_homebindX, m_homebindY, m_homebindZ); };

    _LoadGroup(holder.GetPreparedResult(PLAYER_LOGIN_QUERY_LOAD_GROUP));

    _LoadArenaTeamInfo(holder.GetPreparedResult(PLAYER_LOGIN_QUERY_LOAD_ARENA_INFO));

    SetArenaPoints(fields[44].GetUInt32());

    // check arena teams integrity
    for (uint32 arena_slot = 0; arena_slot < MAX_ARENA_SLOT; ++arena_slot)
    {
        uint32 arena_team_id = GetArenaTeamId(arena_slot);
        if (!arena_team_id)
            continue;

        if (ArenaTeam* at = sArenaTeamMgr->GetArenaTeamById(arena_team_id))
            if (at->IsMember(GetGUID()))
                continue;

        // arena team not exist or not member, cleanup fields
        for (int j = 0; j < 6; ++j)
            SetArenaTeamInfoField(arena_slot, ArenaTeamInfoType(j), 0);
    }

    SetHonorPoints(fields[45].GetUInt32());
    SetUInt32Value(PLAYER_FIELD_TODAY_CONTRIBUTION, fields[46].GetUInt32());
    SetUInt32Value(PLAYER_FIELD_YESTERDAY_CONTRIBUTION, fields[47].GetUInt32());
    SetUInt32Value(PLAYER_FIELD_LIFETIME_HONORABLE_KILLS, fields[48].GetUInt32());
    SetUInt16Value(PLAYER_FIELD_KILLS, 0, fields[49].GetUInt16());
    SetUInt16Value(PLAYER_FIELD_KILLS, 1, fields[50].GetUInt16());

    _LoadBoundInstances(holder.GetPreparedResult(PLAYER_LOGIN_QUERY_LOAD_BOUND_INSTANCES));

    _LoadBGData(holder.GetPreparedResult(PLAYER_LOGIN_QUERY_LOAD_BG_DATA));

    GetSession()->SetPlayer(this);
    MapEntry const* mapEntry = sMapStore.LookupEntry(mapId);
    Map* map = nullptr;
    bool player_at_bg = false;
    if (!mapEntry || !IsPositionValid())
    {
        TC_LOG_ERROR("entities.player.loading", "Player::LoadFromDB: Player ({}) has invalid coordinates (MapId: {} X: {} Y: {} Z: {} O: {}). Teleport to default race/class locations.",
            guid.ToString(), mapId, GetPositionX(), GetPositionY(), GetPositionZ(), GetOrientation());
        RelocateToHomebind();
    }
    // Player was saved in Arena or Bg
    else if (mapEntry->IsBattlegroundOrArena())
    {
        Battleground* currentBg = nullptr;
        if (m_bgData.bgInstanceID)                                                //saved in Battleground
            currentBg = sBattlegroundMgr->GetBattleground(m_bgData.bgInstanceID, BATTLEGROUND_TYPE_NONE);

        player_at_bg = currentBg && currentBg->IsPlayerInBattleground(GetGUID());

        if (player_at_bg && currentBg->GetStatus() != STATUS_WAIT_LEAVE)
        {
            map = currentBg->GetBgMap();

            BattlegroundQueueTypeId bgQueueTypeId = sBattlegroundMgr->BGQueueTypeId(currentBg->GetTypeID(), currentBg->GetArenaType());
            AddBattlegroundQueueId(bgQueueTypeId);

            m_bgData.bgTypeID = currentBg->GetTypeID();

            //join player to battleground group
            currentBg->EventPlayerLoggedIn(this);

            SetInviteForBattlegroundQueueType(bgQueueTypeId, currentBg->GetInstanceID());
        }
        // Bg was not found - go to Entry Point
        else
        {
            // leave bg
            if (player_at_bg)
            {
                player_at_bg = false;
                currentBg->RemovePlayerAtLeave(GetGUID(), false, true);
            }

            // Do not look for instance if bg not found
            WorldLocation const& _loc = GetBattlegroundEntryPoint();
            mapId = _loc.GetMapId();
            instanceId = 0;

            // Db field type is type int16, so it can never be MAPID_INVALID
            //if (mapId == MAPID_INVALID) -- code kept for reference
            if (int16(mapId) == int16(-1)) // Battleground Entry Point not found (???)
            {
                TC_LOG_ERROR("entities.player.loading", "Player::LoadFromDB: Player ({}) was in BG in database, but BG was not found and entry point was invalid! Teleport to default race/class locations.",
                    guid.ToString());
                RelocateToHomebind();
            }
            else
                Relocate(&_loc);

            // We are not in BG anymore
            m_bgData.bgInstanceID = 0;
        }
    }
    // currently we do not support transport in bg
    else if (transLowGUID)
    {
        ObjectGuid transGUID(HighGuid::Mo_Transport, transLowGUID);

        GenericTransport* transport = nullptr;
        if (Transport* go = HashMapHolder<Transport>::Find(transGUID))
            transport = go;

        if (transport)
        {
            float x = fields[31].GetFloat(), y = fields[32].GetFloat(), z = fields[33].GetFloat(), o = fields[34].GetFloat();
            m_movementInfo.transport.pos.Relocate(x, y, z, o);
            transport->CalculatePassengerPosition(x, y, z, &o);

            if (!Trinity::IsValidMapCoord(x, y, z, o) ||
                // transport size limited
                std::fabs(m_movementInfo.transport.pos.GetPositionX()) > 250.0f ||
                std::fabs(m_movementInfo.transport.pos.GetPositionY()) > 250.0f ||
                std::fabs(m_movementInfo.transport.pos.GetPositionZ()) > 250.0f)
            {
                TC_LOG_ERROR("entities.player.loading", "Player::LoadFromDB: Player ({}) has invalid transport coordinates (X: {} Y: {} Z: {} O: {}). Teleport to bind location.",
                    guid.ToString(), x, y, z, o);

                m_movementInfo.transport.Reset();

                RelocateToHomebind();
            }
            else
            {
                Relocate(x, y, z, o);
                mapId = transport->GetMapId();

                transport->AddPassenger(this);
            }
        }
        else
        {
            TC_LOG_ERROR("entities.player.loading", "Player::LoadFromDB: Player ({}) has problems with transport guid ({}). Teleport to bind location.",
                guid.ToString(), transLowGUID);

            RelocateToHomebind();
        }
    }
    // currently we do not support taxi in instance
    else if (!taxi_nodes.empty())
    {
        instanceId = 0;

        // Not finish taxi flight path
        if (m_bgData.HasTaxiPath())
        {
            for (int i = 0; i < 2; ++i)
                m_taxi.AddTaxiDestination(m_bgData.taxiPath[i]);
        }
        else if (!m_taxi.LoadTaxiDestinationsFromString(taxi_nodes, GetTeam()))
        {
            // problems with taxi path loading
            TaxiNodesEntry const* nodeEntry = nullptr;
            if (uint32 node_id = m_taxi.GetTaxiSource())
                nodeEntry = sTaxiNodesStore.LookupEntry(node_id);

            if (!nodeEntry)                                      // don't know taxi start node, teleport to homebind
            {
                TC_LOG_ERROR("entities.player.loading", "Player::LoadFromDB: Player ({}) has wrong data in taxi destination list ({}), teleport to homebind.", GetGUID().ToString(), taxi_nodes);
                RelocateToHomebind();
            }
            else                                                // has start node, teleport to it
            {
                TC_LOG_ERROR("entities.player.loading", "Player::LoadFromDB: Player ({}) has too short taxi destination list ({}), teleport to original node.", GetGUID().ToString(), taxi_nodes);
                mapId = nodeEntry->ContinentID;
                Relocate(nodeEntry->Pos.X, nodeEntry->Pos.Y, nodeEntry->Pos.Z, 0.0f);
            }
            m_taxi.ClearTaxiDestinations();
        }

        if (uint32 node_id = m_taxi.GetTaxiSource())
        {
            // save source node as recall coord to prevent recall and fall from sky
            TaxiNodesEntry const* nodeEntry = sTaxiNodesStore.LookupEntry(node_id);
            if (nodeEntry && nodeEntry->ContinentID == GetMapId())
            {
                ASSERT(nodeEntry);                                  // checked in m_taxi.LoadTaxiDestinationsFromString
                mapId = nodeEntry->ContinentID;
                Relocate(nodeEntry->Pos.X, nodeEntry->Pos.Y, nodeEntry->Pos.Z, 0.0f);
            }

            // flight will started later
        }
    }

    // Map could be changed before
    mapEntry = sMapStore.LookupEntry(mapId);
    // client without expansion support
    if (mapEntry)
    {
        if (GetSession()->Expansion() < mapEntry->Expansion())
        {
            TC_LOG_DEBUG("entities.player.loading", "Player::LoadFromDB: Player '{}' ({}) using client without required expansion tried login at non accessible map {}",
                GetName(), GetGUID().ToString(), mapId);
            RelocateToHomebind();
        }

        // fix crash (because of if (Map* map = _FindMap(instanceId)) in MapInstanced::CreateInstance)
        if (instanceId)
            if (InstanceSave* save = GetInstanceSave(mapId, mapEntry->IsRaid()))
                if (save->GetInstanceId() != instanceId)
                    instanceId = 0;
    }

    // NOW player must have valid map
    // load the player's map here if it's not already loaded
    if (!map)
        map = sMapMgr->CreateMap(mapId, GetPosition(), this, instanceId);

    AreaTrigger const* areaTrigger = nullptr;
    bool check = false;

    if (!map)
    {
        areaTrigger = sObjectMgr->GetGoBackTrigger(mapId);
        check = true;
    }
    else if (map->IsDungeon()) // if map is dungeon...
    {
        if (Map::EnterState denyReason = ((InstanceMap*)map)->CannotEnter(this)) // ... and can't enter map, then look for entry point.
        {
            switch (denyReason)
            {
                case Map::CANNOT_ENTER_DIFFICULTY_UNAVAILABLE:
                    SendTransferAborted(map->GetId(), TRANSFER_ABORT_DIFFICULTY, map->GetDifficulty());
                    break;
                case Map::CANNOT_ENTER_INSTANCE_BIND_MISMATCH:
                    ChatHandler(GetSession()).PSendSysMessage(GetSession()->GetTrinityString(LANG_INSTANCE_BIND_MISMATCH), map->GetMapName());
                    break;
                case Map::CANNOT_ENTER_TOO_MANY_INSTANCES:
                    SendTransferAborted(map->GetId(), TRANSFER_ABORT_TOO_MANY_INSTANCES);
                    break;
                case Map::CANNOT_ENTER_MAX_PLAYERS:
                    SendTransferAborted(map->GetId(), TRANSFER_ABORT_MAX_PLAYERS);
                    break;
                case Map::CANNOT_ENTER_ZONE_IN_COMBAT:
                    SendTransferAborted(map->GetId(), TRANSFER_ABORT_ZONE_IN_COMBAT);
                    break;
                default:
                    break;
            }
            areaTrigger = sObjectMgr->GetGoBackTrigger(mapId);
            check = true;
        }
        else if (instanceId && !sInstanceSaveMgr->GetInstanceSave(instanceId)) // ... and instance is reseted then look for entrance.
        {
            areaTrigger = sObjectMgr->GetMapEntranceTrigger(mapId);
            check = true;
        }
    }

    if (check) // in case of special event when creating map...
    {
        if (areaTrigger) // ... if we have an areatrigger, then relocate to new map/coordinates.
        {
            Relocate(areaTrigger->target_X, areaTrigger->target_Y, areaTrigger->target_Z, GetOrientation());
            if (mapId != areaTrigger->target_mapId)
            {
                mapId = areaTrigger->target_mapId;
                map = sMapMgr->CreateMap(mapId, GetPosition(), this);
            }
        }
        else
        {
            TC_LOG_ERROR("entities.player.loading", "Player::LoadFromDB: Player '{}' ({}) Map: {}, X: {}, Y: {}, Z: {}, O: {}. Areatrigger not found.",
                m_name, guid.ToString(), mapId, GetPositionX(), GetPositionY(), GetPositionZ(), GetOrientation());
            RelocateToHomebind();
            map = nullptr;
        }
    }

    if (!map)
    {
        mapId = info->mapId;
        Relocate(info->positionX, info->positionY, info->positionZ, 0.0f);
        map = sMapMgr->CreateMap(mapId, GetPosition(), this);
        if (!map)
        {
            TC_LOG_ERROR("entities.player.loading", "Player::LoadFromDB: Player '{}' ({}) Map: {}, X: {}, Y: {}, Z: {}, O: {}. Invalid default map coordinates or instance couldn't be created.",
                m_name, guid.ToString(), mapId, GetPositionX(), GetPositionY(), GetPositionZ(), GetOrientation());
            return false;
        }
    }

    SetMap(map);
    StoreRaidMapDifficulty();
    UpdatePositionData();

    // now that map position is determined, check instance validity
    if (!CheckInstanceValidity(true) && !IsInstanceLoginGameMasterException())
        m_InstanceValid = false;

    if (player_at_bg)
        map->ToBattlegroundMap()->GetBG()->AddPlayer(this);

    // randomize first save time in range [CONFIG_INTERVAL_SAVE] around [CONFIG_INTERVAL_SAVE]
    // this must help in case next save after mass player load after server startup
    m_nextSave = urand(m_nextSave / 2, m_nextSave * 3 / 2);

    SaveRecallPosition();

    time_t now = GameTime::GetGameTime();
    time_t logoutTime = time_t(fields[27].GetUInt32());

    // since last logout (in seconds)
    uint32 time_diff = uint32(now - logoutTime); //uint64 is excessive for a time_diff in seconds.. uint32 allows for 136~ year difference.

    // set value, including drunk invisibility detection
    // calculate sobering. after 15 minutes logged out, the player will be sober again
    if (time_diff < uint32(GetDrunkValue()) * 9)
        SetDrunkValue(GetDrunkValue() - time_diff / 9);
    else
        SetDrunkValue(0);

    m_cinematic = fields[23].GetUInt8();
    m_Played_time[PLAYED_TIME_TOTAL] = fields[24].GetUInt32();
    m_Played_time[PLAYED_TIME_LEVEL] = fields[25].GetUInt32();

    m_resetTalentsCost = fields[29].GetUInt32();
    m_resetTalentsTime = time_t(fields[30].GetUInt32());

    if (!m_taxi.LoadTaxiMask(fields[22].GetString()))                // must be before InitTaxiNodesForLevel
        TC_LOG_WARN("entities.player.loading", "Player::LoadFromDB: Player ({}) has invalid taximask ({}) in DB. Forced partial load.", GetGUID().ToString(), fields[22].GetString());

    uint32 extraflags = fields[36].GetUInt16();

    _LoadPetStable(fields[37].GetUInt8(), holder.GetPreparedResult(PLAYER_LOGIN_QUERY_LOAD_PET_SLOTS));

    m_atLoginFlags = fields[38].GetUInt16();

    if (HasAtLoginFlag(AT_LOGIN_RENAME))
    {
        TC_LOG_ERROR("entities.player.cheat", "Player::LoadFromDB: Player ({}) tried to login while forced to rename, can't load.'", GetGUID().ToString());
        return false;
    }

    // Honor system
    // Update Honor kills data
    m_lastHonorUpdateTime = logoutTime;
    UpdateHonorFields();

    m_deathExpireTime = time_t(fields[41].GetUInt32());

    if (m_deathExpireTime > now + MAX_DEATH_COUNT * DEATH_EXPIRE_STEP)
        m_deathExpireTime = now + MAX_DEATH_COUNT * DEATH_EXPIRE_STEP - 1;

    // clear channel spell data (if saved at channel spell casting)
    SetChannelObjectGuid(ObjectGuid::Empty);
    SetChannelSpellId(0);

    // clear charm/summon related fields
    SetOwnerGUID(ObjectGuid::Empty);
    SetGuidValue(UNIT_FIELD_CHARMEDBY, ObjectGuid::Empty);
    SetGuidValue(UNIT_FIELD_CHARM, ObjectGuid::Empty);
    SetGuidValue(UNIT_FIELD_SUMMON, ObjectGuid::Empty);
    SetGuidValue(PLAYER_FARSIGHT, ObjectGuid::Empty);
    SetCreatorGUID(ObjectGuid::Empty);

    RemoveUnitFlag2(UNIT_FLAG2_FORCE_MOVEMENT);

    // reset some aura modifiers before aura apply
    SetUInt32Value(PLAYER_TRACK_CREATURES, 0);
    SetUInt32Value(PLAYER_TRACK_RESOURCES, 0);

    // make sure the unit is considered out of combat for proper loading
    ClearInCombat();

    // make sure the unit is considered not in duel for proper loading
    SetGuidValue(PLAYER_DUEL_ARBITER, ObjectGuid::Empty);
    SetUInt32Value(PLAYER_DUEL_TEAM, 0);

    // reset stats before loading any modifiers
    InitStatsForLevel();
    InitGlyphsForLevel();
    InitTaxiNodesForLevel();
    InitRunes();

    // rest bonus can only be calculated after InitStatsForLevel()
    m_rest_bonus = fields[26].GetFloat();

    if (time_diff > 0)
    {
        //speed collect rest bonus in offline, in logout, far from tavern, city (section/in hour)
        float bubble0 = 0.031f;
        //speed collect rest bonus in offline, in logout, in tavern, city (section/in hour)
        float bubble1 = 0.125f;
        float bubble = fields[28].GetUInt8() > 0
            ? bubble1*sWorld->getRate(RATE_REST_OFFLINE_IN_TAVERN_OR_CITY)
            : bubble0*sWorld->getRate(RATE_REST_OFFLINE_IN_WILDERNESS);

        SetRestBonus(GetRestBonus() + time_diff*((float)GetUInt32Value(PLAYER_NEXT_LEVEL_XP) / 72000)*bubble);
    }

    // load skills after InitStatsForLevel because it triggering aura apply also
    _LoadSkills(holder.GetPreparedResult(PLAYER_LOGIN_QUERY_LOAD_SKILLS));
    UpdateSkillsForLevel(); //update skills after load, to make sure they are correctly update at player load

    // apply original stats mods before spell loading or item equipment that call before equip _RemoveStatsMods()

    //mails are loaded only when needed ;-) - when player in game click on mailbox.
    //_LoadMail();

    m_specsCount = fields[64].GetUInt8();
    m_activeSpec = fields[65].GetUInt8();

    // sanity check
    if (m_specsCount > MAX_TALENT_SPECS || m_activeSpec > MAX_TALENT_SPEC || m_specsCount < MIN_TALENT_SPECS)
    {
        TC_LOG_ERROR("entities.player.loading", "Player::LoadFromDB: Player {} ({}) has invalid SpecCount = {} and/or invalid ActiveSpec = {}.",
            GetName(), GetGUID().ToString(), uint32(m_specsCount), uint32(m_activeSpec));
        m_activeSpec = 0;
    }

    UpdateDisplayPower();
    _LoadTalents(holder.GetPreparedResult(PLAYER_LOGIN_QUERY_LOAD_TALENTS));
    _LoadSpells(holder.GetPreparedResult(PLAYER_LOGIN_QUERY_LOAD_SPELLS));

    _LoadGlyphs(holder.GetPreparedResult(PLAYER_LOGIN_QUERY_LOAD_GLYPHS));
    _LoadAuras(holder.GetPreparedResult(PLAYER_LOGIN_QUERY_LOAD_AURAS), time_diff);
    _LoadGlyphAuras();
    // add ghost flag (must be after aura load: PLAYER_FLAGS_GHOST set in aura)
    if (HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_GHOST))
        m_deathState = DEAD;

    // after spell load, learn rewarded spell if need also
    _LoadQuestStatus(holder.GetPreparedResult(PLAYER_LOGIN_QUERY_LOAD_QUEST_STATUS));
    _LoadQuestStatusRewarded(holder.GetPreparedResult(PLAYER_LOGIN_QUERY_LOAD_QUEST_STATUS_REW));
    _LoadDailyQuestStatus(holder.GetPreparedResult(PLAYER_LOGIN_QUERY_LOAD_DAILY_QUEST_STATUS));
    _LoadWeeklyQuestStatus(holder.GetPreparedResult(PLAYER_LOGIN_QUERY_LOAD_WEEKLY_QUEST_STATUS));
    _LoadSeasonalQuestStatus(holder.GetPreparedResult(PLAYER_LOGIN_QUERY_LOAD_SEASONAL_QUEST_STATUS));
    _LoadMonthlyQuestStatus(holder.GetPreparedResult(PLAYER_LOGIN_QUERY_LOAD_MONTHLY_QUEST_STATUS));
    _LoadRandomBGStatus(holder.GetPreparedResult(PLAYER_LOGIN_QUERY_LOAD_RANDOM_BG));

    // after spell and quest load
    InitTalentForLevel();
    LearnDefaultSkills();
    LearnCustomSpells();

    // must be before inventory (some items required reputation check)
    m_reputationMgr->LoadFromDB(holder.GetPreparedResult(PLAYER_LOGIN_QUERY_LOAD_REPUTATION));

    _LoadInventory(holder.GetPreparedResult(PLAYER_LOGIN_QUERY_LOAD_INVENTORY), time_diff);

    // update items with duration and realtime
    UpdateItemDuration(time_diff, true);

    _LoadActions(holder.GetPreparedResult(PLAYER_LOGIN_QUERY_LOAD_ACTIONS));

    // unread mails and next delivery time, actual mails not loaded
    _LoadMail(holder.GetPreparedResult(PLAYER_LOGIN_QUERY_LOAD_MAILS), holder.GetPreparedResult(PLAYER_LOGIN_QUERY_LOAD_MAIL_ITEMS));

    m_social = sSocialMgr->LoadFromDB(holder.GetPreparedResult(PLAYER_LOGIN_QUERY_LOAD_SOCIAL_LIST), GetGUID());

    // check PLAYER_CHOSEN_TITLE compatibility with PLAYER__FIELD_KNOWN_TITLES
    // note: PLAYER__FIELD_KNOWN_TITLES updated at quest status loaded
    uint32 curTitle = fields[51].GetUInt32();
    if (curTitle && !HasTitle(curTitle))
        curTitle = 0;

    SetUInt32Value(PLAYER_CHOSEN_TITLE, curTitle);

    // has to be called after last Relocate() in Player::LoadFromDB
    SetFallInformation(0, GetPositionZ());

    GetSpellHistory()->LoadFromDB<Player>(holder.GetPreparedResult(PLAYER_LOGIN_QUERY_LOAD_SPELL_COOLDOWNS));

    uint32 savedHealth = fields[55].GetUInt32();
    if (!savedHealth)
        m_deathState = CORPSE;

    // Spell code allow apply any auras to dead character in load time in aura/spell/item loading
    // Do now before stats re-calculation cleanup for ghost state unexpected auras
    if (!IsAlive())
        RemoveAllAurasOnDeath();
    else
        RemoveAllAurasRequiringDeadTarget();

    //apply all stat bonuses from items and auras
    SetCanModifyStats(true);
    UpdateAllStats();

    // restore remembered power/health values (but not more max values)
    SetHealth(savedHealth);
    for (uint8 i = 0; i < MAX_POWERS; ++i)
    {
        uint32 savedPower = fields[56 + i].GetUInt32();
        SetPower(static_cast<Powers>(i), savedPower);
    }

    TC_LOG_DEBUG("entities.player.loading", "Player::LoadFromDB: The value of player '{}' after load item and aura is: ", m_name);
    outDebugValues();

    // GM state
    if (GetSession()->HasPermission(rbac::RBAC_PERM_RESTORE_SAVED_GM_STATE))
    {
        switch (sWorld->getIntConfig(CONFIG_GM_LOGIN_STATE))
        {
            default:
            case 0:                      break;             // disable
            case 1: SetGameMaster(true); break;             // enable
            case 2:                                         // save state
                if (extraflags & PLAYER_EXTRA_GM_ON)
                    SetGameMaster(true);
                break;
        }

        switch (sWorld->getIntConfig(CONFIG_GM_VISIBLE_STATE))
        {
            default:
            case 0: SetGMVisible(false); break;             // invisible
            case 1:                      break;             // visible
            case 2:                                         // save state
                if (extraflags & PLAYER_EXTRA_GM_INVISIBLE)
                    SetGMVisible(false);
                break;
        }

        switch (sWorld->getIntConfig(CONFIG_GM_CHAT))
        {
            default:
            case 0:                  break;                 // disable
            case 1: SetGMChat(true); break;                 // enable
            case 2:                                         // save state
                if (extraflags & PLAYER_EXTRA_GM_CHAT)
                    SetGMChat(true);
                break;
        }

        switch (sWorld->getIntConfig(CONFIG_GM_WHISPERING_TO))
        {
            default:
            case 0:                          break;         // disable
            case 1: SetAcceptWhispers(true); break;         // enable
            case 2:                                         // save state
                if (extraflags & PLAYER_EXTRA_ACCEPT_WHISPERS)
                    SetAcceptWhispers(true);
                break;
        }
    }

    InitPvP();

    // RaF stuff.
    m_grantableLevels = fields[71].GetUInt8();
    if (GetSession()->IsARecruiter() || (GetSession()->GetRecruiterId() != 0))
        SetDynamicFlag(UNIT_DYNFLAG_REFER_A_FRIEND);

    if (m_grantableLevels > 0)
        SetByteValue(PLAYER_FIELD_BYTES, PLAYER_FIELD_BYTES_OFFSET_RAF_GRANTABLE_LEVEL, 0x01);

    _LoadDeclinedNames(holder.GetPreparedResult(PLAYER_LOGIN_QUERY_LOAD_DECLINED_NAMES));

    m_achievementMgr->CheckAllAchievementCriteria();

    _LoadEquipmentSets(holder.GetPreparedResult(PLAYER_LOGIN_QUERY_LOAD_EQUIPMENT_SETS));

    /** @epoch-begin */
    // @tswow-begin
    // m_db_json = TSDBJson(DBJsonEntityType::PLAYER, guid);
    // m_db_json.Load();
    // @tswow-end
    /** @epoch-end */

    // @epoch-start
    m_canSeeTransmog = true;
    // @epoch-end

    return true;
}

bool Player::_LoadHomeBind(PreparedQueryResult result)
{
    PlayerInfo const* info = sObjectMgr->GetPlayerInfo(GetRace(), GetClass());
    if (!info)
    {
        TC_LOG_ERROR("entities.player", "Player::_LoadHomeBind: Player '{}' ({}) has incorrect race/class ({}/{}) pair. Can't load.",
            GetGUID().ToString(), GetName(), uint32(GetRace()), uint32(GetClass()));
        return false;
    }

    bool ok = false;
    // SELECT mapId, zoneId, posX, posY, posZ FROM character_homebind WHERE guid = ?
    if (result)
    {
        Field* fields = result->Fetch();

        m_homebindMapId = fields[0].GetUInt16();
        m_homebindAreaId = fields[1].GetUInt16();
        m_homebindX = fields[2].GetFloat();
        m_homebindY = fields[3].GetFloat();
        m_homebindZ = fields[4].GetFloat();

        MapEntry const* bindMapEntry = sMapStore.LookupEntry(m_homebindMapId);

        // accept saved data only for valid position (and non instanceable), and accessable
        if (MapManager::IsValidMapCoord(m_homebindMapId, m_homebindX, m_homebindY, m_homebindZ) &&
            !bindMapEntry->Instanceable() && GetSession()->Expansion() >= bindMapEntry->Expansion())
            ok = true;
        else
        {
            CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_PLAYER_HOMEBIND);
            stmt->setUInt32(0, GetGUID().GetCounter());
            CharacterDatabase.Execute(stmt);
        }
    }

    if (!ok)
    {
        m_homebindMapId = info->mapId;
        m_homebindAreaId = info->areaId;
        m_homebindX = info->positionX;
        m_homebindY = info->positionY;
        m_homebindZ = info->positionZ;

        CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_INS_PLAYER_HOMEBIND);
        stmt->setUInt32(0, GetGUID().GetCounter());
        stmt->setUInt16(1, m_homebindMapId);
        stmt->setUInt16(2, m_homebindAreaId);
        stmt->setFloat (3, m_homebindX);
        stmt->setFloat (4, m_homebindY);
        stmt->setFloat (5, m_homebindZ);
        CharacterDatabase.Execute(stmt);
    }

    TC_LOG_DEBUG("entities.player", "Player::_LoadHomeBind: Setting home position (MapID: {}, AreaID: {}, X: {}, Y: {}, Z: {}) of player '{}' ({})",
        m_homebindMapId, m_homebindAreaId, m_homebindX, m_homebindY, m_homebindZ, GetName(), GetGUID().ToString());

    return true;
}

void Player::_LoadGroup(PreparedQueryResult result)
{
    //QueryResult* result = CharacterDatabase.PQuery("SELECT guid FROM group_member WHERE memberGuid={}", GetGUID().GetCounter());
    if (result)
    {
        if (Group* group = sGroupMgr->GetGroupByDbStoreId((*result)[0].GetUInt32()))
        {
            if (group->IsLeader(GetGUID()))
                SetFlag(PLAYER_FLAGS, PLAYER_FLAGS_GROUP_LEADER);

            uint8 subgroup = group->GetMemberGroup(GetGUID());
            SetGroup(group, subgroup);

            // Make sure the player's difficulty settings are always aligned with the group's settings in order to avoid issues when checking access requirements
            SetDungeonDifficulty(group->GetDungeonDifficulty());
            SetRaidDifficulty(group->GetRaidDifficulty());
        }
    }

    if (!GetGroup() || !GetGroup()->IsLeader(GetGUID()))
        RemoveFlag(PLAYER_FLAGS, PLAYER_FLAGS_GROUP_LEADER);
}

void Player::_LoadArenaTeamInfo(PreparedQueryResult result)
{
    // arenateamid, played_week, played_season, personal_rating
    memset((void*)&m_uint32Values[PLAYER_FIELD_ARENA_TEAM_INFO_1_1], 0, sizeof(uint32) * MAX_ARENA_SLOT * ARENA_TEAM_END);

    uint16 personalRatingCache[] = {0, 0, 0};

    if (result)
    {
        do
        {
            Field* fields = result->Fetch();

            uint32 arenaTeamId = fields[0].GetUInt32();

            ArenaTeam* arenaTeam = sArenaTeamMgr->GetArenaTeamById(arenaTeamId);
            if (!arenaTeam)
            {
                TC_LOG_ERROR("entities.player.loading", "Player::_LoadArenaTeamInfo: couldn't load arenateam {}", arenaTeamId);
                continue;
            }

            uint8 arenaSlot = arenaTeam->GetSlot();
            ASSERT(arenaSlot < 3);

            personalRatingCache[arenaSlot] = fields[4].GetUInt16();

            SetArenaTeamInfoField(arenaSlot, ARENA_TEAM_ID, arenaTeamId);
            SetArenaTeamInfoField(arenaSlot, ARENA_TEAM_TYPE, arenaTeam->GetType());
            SetArenaTeamInfoField(arenaSlot, ARENA_TEAM_MEMBER, (arenaTeam->GetCaptain() == GetGUID()) ? 0 : 1);
            SetArenaTeamInfoField(arenaSlot, ARENA_TEAM_GAMES_WEEK, uint32(fields[1].GetUInt16()));
            SetArenaTeamInfoField(arenaSlot, ARENA_TEAM_GAMES_SEASON, uint32(fields[2].GetUInt16()));
            SetArenaTeamInfoField(arenaSlot, ARENA_TEAM_WINS_SEASON, uint32(fields[3].GetUInt16()));
        }
        while (result->NextRow());
    }

    for (uint8 slot = 0; slot <= 2; ++slot)
    {
        SetArenaTeamInfoField(slot, ARENA_TEAM_PERSONAL_RATING, uint32(personalRatingCache[slot]));
    }
}

void Player::_LoadBoundInstances(PreparedQueryResult result)
{
    for (uint8 i = 0; i < MAX_DIFFICULTY; ++i)
        m_boundInstances[i].clear();

    Group* group = GetGroup();

    //         0          1    2           3            4          5
    // SELECT id, permanent, map, difficulty, extendState, resettime FROM character_instance LEFT JOIN instance ON instance = id WHERE guid = ?
    if (result)
    {
        do
        {
            Field* fields = result->Fetch();

            bool perm = fields[1].GetBool();
            uint32 mapId = fields[2].GetUInt16();
            uint32 instanceId = fields[0].GetUInt32();
            uint8 difficulty = fields[3].GetUInt8();
            BindExtensionState extendState = BindExtensionState(fields[4].GetUInt8());

            time_t resetTime = time_t(fields[5].GetUInt64());
            // the resettime for normal instances is only saved when the InstanceSave is unloaded
            // so the value read from the DB may be wrong here but only if the InstanceSave is loaded
            // and in that case it is not used

            bool deleteInstance = false;

            MapEntry const* mapEntry = sMapStore.LookupEntry(mapId);
            std::string mapname = mapEntry ? mapEntry->MapName[sWorld->GetDefaultDbcLocale()] : "Unknown";

            if (!mapEntry || !mapEntry->IsDungeon())
            {
                TC_LOG_ERROR("entities.player", "Player::_LoadBoundInstances: Player '{}' ({}) has bind to not existed or not dungeon map {} ({})",
                    GetName(), GetGUID().ToString(), mapId, mapname);
                deleteInstance = true;
            }
            else if (difficulty >= MAX_DIFFICULTY)
            {
                TC_LOG_ERROR("entities.player", "Player::_LoadBoundInstances: player '{}' ({}) has bind to not existed difficulty {} instance for map {} ({})",
                    GetName(), GetGUID().ToString(), difficulty, mapId, mapname);
                deleteInstance = true;
            }
            else
            {
                MapDifficulty const* mapDiff = GetMapDifficultyData(mapId, Difficulty(difficulty));
                if (!mapDiff)
                {
                    TC_LOG_ERROR("entities.player", "Player::_LoadBoundInstances: player '{}' ({}) has bind to not existed difficulty {} instance for map {} ({})",
                        GetName(), GetGUID().ToString(), difficulty, mapId, mapname);
                    deleteInstance = true;
                }
                else if (!perm && group)
                {
                    TC_LOG_ERROR("entities.player", "Player::_LoadBoundInstances: player '{}' ({}) is in group {} but has a non-permanent character bind to map {} ({}), {}, {}",
                        GetName(), GetGUID().ToString(), group->GetGUID().ToString(), mapId, mapname, instanceId, difficulty);
                    deleteInstance = true;
                }
            }

            if (deleteInstance)
            {
                CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHAR_INSTANCE_BY_INSTANCE_GUID);

                stmt->setUInt32(0, GetGUID().GetCounter());
                stmt->setUInt32(1, instanceId);

                CharacterDatabase.Execute(stmt);

                continue;
            }

            // since non permanent binds are always solo bind, they can always be reset
            if (InstanceSave* save = sInstanceSaveMgr->AddInstanceSave(mapId, instanceId, Difficulty(difficulty), resetTime, !perm, true))
               BindToInstance(save, perm, extendState, true);
        }
        while (result->NextRow());
    }
}

void Player::_LoadBGData(PreparedQueryResult result)
{
    if (!result)
        return;

    Field* fields = result->Fetch();
    // Expecting only one row
    //        0           1     2      3      4      5      6          7          8        9
    // SELECT instanceId, team, joinX, joinY, joinZ, joinO, joinMapId, taxiStart, taxiEnd, mountSpell FROM character_battleground_data WHERE guid = ?

    m_bgData.bgInstanceID = fields[0].GetUInt32();
    m_bgData.bgTeam       = fields[1].GetUInt16();
    m_bgData.joinPos      = WorldLocation(fields[6].GetUInt16(),    // Map
                                          fields[2].GetFloat(),     // X
                                          fields[3].GetFloat(),     // Y
                                          fields[4].GetFloat(),     // Z
                                          fields[5].GetFloat());    // Orientation
    m_bgData.taxiPath[0]  = fields[7].GetUInt32();
    m_bgData.taxiPath[1]  = fields[8].GetUInt32();
    m_bgData.mountSpell   = fields[9].GetUInt32();
}

void Player::_LoadPetStable(uint8 petStableSlots, PreparedQueryResult result)
{
    if (!petStableSlots && !result)
        return;

    m_petStable = std::make_unique<PetStable>();
    m_petStable->MaxStabledPets = petStableSlots;
    if (m_petStable->MaxStabledPets > MAX_PET_STABLES)
    {
        TC_LOG_ERROR("entities.player", "Player::LoadFromDB: Player ({}) can't have more stable slots than {}, but has {} in DB",
            GetGUID().ToString(), MAX_PET_STABLES, m_petStable->MaxStabledPets);
        m_petStable->MaxStabledPets = MAX_PET_STABLES;
    }

    //         0      1        2      3    4           5     6     7        8          9       10            11      12        13              14       15
    // SELECT id, entry, modelid, level, exp, Reactstate, slot, name, renamed, curhealth, curmana, curhappiness, abdata, savetime, CreatedBySpell, PetType FROM character_pet WHERE owner = ?
    if (result)
    {
        do
        {
            Field* fields = result->Fetch();
            PetStable::PetInfo petInfo;
            petInfo.PetNumber = fields[0].GetUInt32();
            petInfo.CreatureId = fields[1].GetUInt32();
            petInfo.DisplayId = fields[2].GetUInt32();
            petInfo.Level = fields[3].GetUInt16();
            petInfo.Experience = fields[4].GetUInt32();
            petInfo.ReactState = ReactStates(fields[5].GetUInt8());
            PetSaveMode slot = PetSaveMode(fields[6].GetUInt8());
            petInfo.Name = fields[7].GetString();
            petInfo.WasRenamed = fields[8].GetBool();
            petInfo.Health = fields[9].GetUInt32();
            petInfo.Mana = fields[10].GetUInt32();
            petInfo.Happiness = fields[11].GetUInt32();
            petInfo.ActionBar = fields[12].GetString();
            petInfo.LastSaveTime = fields[13].GetUInt32();
            petInfo.CreatedBySpellId = fields[14].GetUInt32();
            petInfo.Type = PetType(fields[15].GetUInt8());
            if (slot == PET_SAVE_AS_CURRENT)
                m_petStable->CurrentPet = std::move(petInfo);
            else if (slot >= PET_SAVE_FIRST_STABLE_SLOT && slot <= PET_SAVE_LAST_STABLE_SLOT)
                m_petStable->StabledPets[slot - 1] = std::move(petInfo);
            else if (slot == PET_SAVE_NOT_IN_SLOT)
                m_petStable->UnslottedPets.push_back(std::move(petInfo));

        } while (result->NextRow());
    }
}

void Player::_LoadSkills(PreparedQueryResult result)
{
    //                                                           0      1      2
    // SetPQuery(PLAYER_LOGIN_QUERY_LOADSKILLS,          "SELECT skill, value, max FROM character_skills WHERE guid = '{}'", GUID_LOPART(m_guid));

    uint32 count = 0;
    std::unordered_map<uint32, uint32> loadedSkillValues;
    if (result)
    {
        do
        {
            Field* fields = result->Fetch();
            uint16 skill    = fields[0].GetUInt16();
            uint16 value    = fields[1].GetUInt16();
            uint16 max      = fields[2].GetUInt16();

            SkillRaceClassInfoEntry const* rcEntry = GetSkillRaceClassInfo(skill, GetRace(), GetClass());
            if (!rcEntry)
            {
                TC_LOG_ERROR("entities.player", "Player::_LoadSkills: Player '{}' ({}, Race: {}, Class: {}) has forbidden skill {} for his race/class combination",
                    GetName(), GetGUID().ToString(), uint32(GetRace()), uint32(GetClass()), skill);

                mSkillStatus.insert(SkillStatusMap::value_type(skill, SkillStatusData(0, SKILL_DELETED)));
                continue;
            }

            // set fixed skill ranges
            switch (GetSkillRangeType(rcEntry))
            {
                case SKILL_RANGE_LANGUAGE:                      // 300..300
                    value = max = 300;
                    break;
                case SKILL_RANGE_MONO:                          // 1..1, grey monolite bar
                    value = max = 1;
                    break;
                case SKILL_RANGE_LEVEL:
                    max = GetMaxSkillValueForLevel();
                    break;
                default:
                    break;
            }

            if (value == 0)
            {
                TC_LOG_ERROR("entities.player", "Player::_LoadSkills: Player '{}' ({}) has skill {} with value 0, deleted.",
                    GetName(), GetGUID().ToString(), skill);

                CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHARACTER_SKILL);

                stmt->setUInt32(0, GetGUID().GetCounter());
                stmt->setUInt16(1, skill);

                CharacterDatabase.Execute(stmt);

                continue;
            }

            uint16 skillStep = 0;
            if (SkillTiersEntry const* skillTier = sSkillTiersStore.LookupEntry(rcEntry->SkillTierID))
            {
                for (uint32 i = 0; i < MAX_SKILL_STEP; ++i)
                {
                    if (skillTier->Value[skillStep] == max)
                    {
                        skillStep = i + 1;
                        break;
                    }
                }
            }

            SetUInt32Value(PLAYER_SKILL_INDEX(count), MAKE_PAIR32(skill, skillStep));

            SetUInt32Value(PLAYER_SKILL_VALUE_INDEX(count), MAKE_SKILL_VALUE(value, max));
            SetUInt32Value(PLAYER_SKILL_BONUS_INDEX(count), 0);

            mSkillStatus.insert(SkillStatusMap::value_type(skill, SkillStatusData(count, SKILL_UNCHANGED)));
            loadedSkillValues[skill] = value;

            ++count;

            if (count >= PLAYER_MAX_SKILLS)                      // client limit
            {
                TC_LOG_ERROR("entities.player", "Player::_LoadSkills: Player '{}' ({}) has more than {} skills.",
                    GetName(), GetGUID().ToString(), PLAYER_MAX_SKILLS);
                break;
            }
        }
        while (result->NextRow());
    }

    // Learn skill rewarded spells after all skills have been loaded to prevent learning a skill from them before its loaded with proper value from DB
    for (auto& skill : loadedSkillValues)
        LearnSkillRewardedSpells(skill.first, skill.second);

    for (; count < PLAYER_MAX_SKILLS; ++count)
    {
        SetUInt32Value(PLAYER_SKILL_INDEX(count), 0);
        SetUInt32Value(PLAYER_SKILL_VALUE_INDEX(count), 0);
        SetUInt32Value(PLAYER_SKILL_BONUS_INDEX(count), 0);
    }

    if (HasSkill(SKILL_FIST_WEAPONS))
        SetSkill(SKILL_FIST_WEAPONS, 0, GetSkillValue(SKILL_UNARMED), GetMaxSkillValueForLevel());
}

void Player::_LoadTalents(PreparedQueryResult result)
{
    // SetPQuery(PLAYER_LOGIN_QUERY_LOADTALENTS, "SELECT spell, talentGroup FROM character_talent WHERE guid = '{}'", GUID_LOPART(m_guid));
    if (result)
    {
        do
            AddTalent((*result)[0].GetUInt32(), (*result)[1].GetUInt8(), false);
        while (result->NextRow());
    }
}

void Player::_LoadSpells(PreparedQueryResult result)
{
    //QueryResult* result = CharacterDatabase.PQuery("SELECT spell, active, disabled FROM character_spell WHERE guid = '{}'", GetGUID().GetCounter());

    if (result)
    {
        do
            AddSpell((*result)[0].GetUInt32(), (*result)[1].GetBool(), false, false, (*result)[2].GetBool(), true);
        while (result->NextRow());
    }
}

void Player::_LoadGlyphs(PreparedQueryResult result)
{
    // SELECT talentGroup, glyph1, glyph2, glyph3, glyph4, glyph5, glyph6 from character_glyphs WHERE guid = '%u'
    if (!result)
        return;

    do
    {
        Field* fields = result->Fetch();

        uint8 spec = fields[0].GetUInt8();
        if (spec >= m_specsCount)
            continue;

        m_Glyphs[spec][0] = fields[1].GetUInt16();
        m_Glyphs[spec][1] = fields[2].GetUInt16();
        m_Glyphs[spec][2] = fields[3].GetUInt16();
        m_Glyphs[spec][3] = fields[4].GetUInt16();
        m_Glyphs[spec][4] = fields[5].GetUInt16();
        m_Glyphs[spec][5] = fields[6].GetUInt16();
    }
    while (result->NextRow());
}

void Player::_LoadAuras(PreparedQueryResult result, uint32 timediff)
{
    TC_LOG_DEBUG("entities.player.loading", "Player::_LoadAuras: Loading auras for {}", GetGUID().ToString());

    /*                                                     0           1         2      3           4                5           6        7        8        9             10            11
    QueryResult* result = CharacterDatabase.PQuery("SELECT casterGuid, itemGuid, spell, effectMask, recalculateMask, stackCount, amount0, amount1, amount2, base_amount0, base_amount1, base_amount2,
                                                    12           13          14             15          16
                                                    maxDuration, remainTime, remainCharges, critChance, applyResilience FROM character_aura WHERE guid = '{}'", GetGUID().GetCounter());
    */

    if (result)
    {
        do
        {
            Field* fields = result->Fetch();
            int32 damage[3];
            int32 baseDamage[3];
            ObjectGuid caster_guid(fields[0].GetUInt64());
            ObjectGuid itemGuid(fields[1].GetUInt64());
            uint32 spellid = fields[2].GetUInt32();
            uint8 effmask = fields[3].GetUInt8();
            uint8 recalculatemask = fields[4].GetUInt8();
            uint8 stackcount = fields[5].GetUInt8();
            damage[0] = fields[6].GetInt32();
            damage[1] = fields[7].GetInt32();
            damage[2] = fields[8].GetInt32();
            baseDamage[0] = fields[9].GetInt32();
            baseDamage[1] = fields[10].GetInt32();
            baseDamage[2] = fields[11].GetInt32();
            int32 maxduration = fields[12].GetInt32();
            int32 remaintime = fields[13].GetInt32();
            uint8 remaincharges = fields[14].GetUInt8();
            float critChance = fields[15].GetFloat();
            bool applyResilience = fields[16].GetBool();

            SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellid);
            if (!spellInfo)
            {
                TC_LOG_ERROR("entities.player", "Player::_LoadAuras: Player '{}' ({}) has an invalid aura (SpellID: {}), ignoring.",
                    GetName(), GetGUID().ToString(), spellid);
                continue;
            }

            ChrRacesEntry const* raceEntry = sChrRacesStore.AssertEntry(GetRace());

            // negative effects should continue counting down after logout
            if (remaintime != -1 && ((!spellInfo->IsPositive() && spellInfo->Id != raceEntry->ResSicknessSpellID) || spellInfo->HasAttribute(SPELL_ATTR4_FADES_WHILE_LOGGED_OUT))) // Resurrection sickness should not fade while logged out
            {
                if (remaintime/IN_MILLISECONDS <= int32(timediff))
                    continue;

                remaintime -= timediff*IN_MILLISECONDS;
            }

            // prevent wrong values of remaincharges
            if (spellInfo->ProcCharges)
            {
                // we have no control over the order of applying auras and modifiers allow auras
                // to have more charges than value in SpellInfo
                if (remaincharges <= 0/* || remaincharges > spellproto->procCharges*/)
                    remaincharges = spellInfo->ProcCharges;
            }
            else
                remaincharges = 0;

            AuraCreateInfo createInfo(spellInfo, effmask, this);
            createInfo
                .SetCasterGUID(caster_guid)
                .SetCastItemGUID(itemGuid)
                .SetBaseAmount(baseDamage);

            if (Aura* aura = Aura::TryCreate(createInfo))
            {
                if (!aura->CanBeSaved())
                {
                    aura->Remove();
                    continue;
                }

                aura->SetLoadedState(maxduration, remaintime, remaincharges, stackcount, recalculatemask, critChance, applyResilience, &damage[0]);
                aura->ApplyForTargets();
                TC_LOG_DEBUG("entities.player", "Player::_LoadAuras: Added aura (SpellID: {}, EffectMask: {}) to player '{} ({})",
                    spellInfo->Id, effmask, GetName(), GetGUID().ToString());
            }
        }
        while (result->NextRow());
    }
}

void Player::_LoadGlyphAuras()
{
    for (uint8 i = 0; i < MAX_GLYPH_SLOT_INDEX; ++i)
    {
        if (uint32 glyph = GetGlyph(i))
        {
            if (GlyphPropertiesEntry const* gp = sGlyphPropertiesStore.LookupEntry(glyph))
            {
                if (GlyphSlotEntry const* gs = sGlyphSlotStore.LookupEntry(GetGlyphSlot(i)))
                {
                    if (gp->GlyphSlotFlags == gs->Type)
                    {
                        CastSpell(this, gp->SpellID, true);
                        continue;
                    }
                    else
                        TC_LOG_ERROR("entities.player", "Player::_LoadGlyphAuras: Player '{}' ({}) has glyph with typeflags {} in slot with typeflags {}, removing.", GetName(), GetGUID().ToString(), gp->GlyphSlotFlags, gs->Type);
                }
                else
                    TC_LOG_ERROR("entities.player", "Player::_LoadGlyphAuras: Player '{}' ({}) has not existing glyph slot entry {} on index {}", GetName(), GetGUID().ToString(), GetGlyphSlot(i), i);
            }
            else
                TC_LOG_ERROR("entities.player", "Player::_LoadGlyphAuras: Player '{}' ({}) has not existing glyph entry {} on index {}", GetName(), GetGUID().ToString(), glyph, i);

            // On any error remove glyph
            SetGlyph(i, 0);
        }
    }
}

void Player::_LoadQuestStatus(PreparedQueryResult result)
{
    uint16 slot = 0;

    ////                                                       0      1       2        3        4           5          6         7           8           9           10
    //QueryResult* result = CharacterDatabase.PQuery("SELECT quest, status, explored, timer, mobcount1, mobcount2, mobcount3, mobcount4, itemcount1, itemcount2, itemcount3,
    //                                                    11          12          13           14
    //                                                itemcount4, itemcount5, itemcount6, playercount FROM character_queststatus WHERE guid = '{}'", GetGUID().GetCounter());

    if (result)
    {
        do
        {
            Field* fields = result->Fetch();

            uint32 quest_id = fields[0].GetUInt32();
                                                            // used to be new, no delete?
            Quest const* quest = sObjectMgr->GetQuestTemplate(quest_id);
            if (quest)
            {
                // find or create
                QuestStatusData& questStatusData = m_QuestStatus[quest_id];

                uint8 qstatus = fields[1].GetUInt8();
                if (qstatus < MAX_QUEST_STATUS)
                    questStatusData.Status = QuestStatus(qstatus);
                else
                {
                    questStatusData.Status = QUEST_STATUS_INCOMPLETE;
                    TC_LOG_ERROR("entities.player", "Player::_LoadQuestStatus: Player '{}' ({}) has invalid quest {} status ({}), replaced by QUEST_STATUS_INCOMPLETE(3).",
                        GetName(), GetGUID().ToString(), quest_id, qstatus);
                }

                questStatusData.Explored = (fields[2].GetUInt8() > 0);

                time_t quest_time = time_t(fields[3].GetUInt32());

                if (quest->HasSpecialFlag(QUEST_SPECIAL_FLAGS_TIMED) && !GetQuestRewardStatus(quest_id))
                {
                    AddTimedQuest(quest_id);

                    if (quest_time <= GameTime::GetGameTime())
                        questStatusData.Timer = 1;
                    else
                        questStatusData.Timer = uint32((quest_time - GameTime::GetGameTime()) * IN_MILLISECONDS);
                }
                else
                    quest_time = 0;

                for (uint32 i = 0; i < QUEST_OBJECTIVES_COUNT; ++i)
                    questStatusData.CreatureOrGOCount[i] = fields[4 + i].GetUInt16();

                for (uint32 i = 0; i < QUEST_ITEM_OBJECTIVES_COUNT; ++i)
                    questStatusData.ItemCount[i] = fields[8 + i].GetUInt16();

                questStatusData.PlayerCount = fields[14].GetUInt16();

                // add to quest log
                if (slot < MAX_QUEST_LOG_SIZE && questStatusData.Status != QUEST_STATUS_NONE)
                {
                    SetQuestSlot(slot, quest_id, uint32(quest_time)); // cast can't be helped

                    if (questStatusData.Status == QUEST_STATUS_COMPLETE)
                        SetQuestSlotState(slot, QUEST_STATE_COMPLETE);
                    else if (questStatusData.Status == QUEST_STATUS_FAILED)
                        SetQuestSlotState(slot, QUEST_STATE_FAIL);

                    for (uint8 idx = 0; idx < QUEST_OBJECTIVES_COUNT; ++idx)
                        if (questStatusData.CreatureOrGOCount[idx])
                            SetQuestSlotCounter(slot, idx, questStatusData.CreatureOrGOCount[idx]);

                    if (questStatusData.PlayerCount)
                        SetQuestSlotCounter(slot, QUEST_PVP_KILL_SLOT, questStatusData.PlayerCount);

                    ++slot;
                }

                TC_LOG_DEBUG("entities.player.loading", "Player::_LoadQuestStatus: Quest status is {{}} for quest {{}} for player ({})", questStatusData.Status, quest_id, GetGUID().ToString());
            }
        }
        while (result->NextRow());
    }

    // clear quest log tail
    for (uint16 i = slot; i < MAX_QUEST_LOG_SIZE; ++i)
        SetQuestSlot(i, 0);
}

void Player::_LoadQuestStatusRewarded(PreparedQueryResult result)
{
    // SELECT quest FROM character_queststatus_rewarded WHERE guid = ?

    if (result)
    {
        do
        {
            Field* fields = result->Fetch();

            uint32 quest_id = fields[0].GetUInt32();
                                                            // used to be new, no delete?
            Quest const* quest = sObjectMgr->GetQuestTemplate(quest_id);
            if (quest)
            {
                // learn rewarded spell if unknown
                LearnQuestRewardedSpells(quest);

                // set rewarded title if any
                if (quest->GetCharTitleId())
                {
                    if (CharTitlesEntry const* titleEntry = sCharTitlesStore.LookupEntry(quest->GetCharTitleId()))
                        SetTitle(titleEntry);
                }

                if (quest->GetBonusTalents())
                    m_questRewardTalentCount += quest->GetBonusTalents();

                // @tswow-begin
                if (quest->GetBonusTalentsPerm())
                    m_questRewardPermTalentCount += quest->GetBonusTalentsPerm();
                // @tswow-end

                if (quest->CanIncreaseRewardedQuestCounters())
                    m_RewardedQuests.insert(quest_id);
            }
        }
        while (result->NextRow());
    }
}

void Player::_LoadDailyQuestStatus(PreparedQueryResult result)
{
    for (uint32 quest_daily_idx = 0; quest_daily_idx < PLAYER_MAX_DAILY_QUESTS; ++quest_daily_idx)
        SetUInt32Value(PLAYER_FIELD_DAILY_QUESTS_1+quest_daily_idx, 0);

    m_DFQuests.clear();

    //QueryResult* result = CharacterDatabase.PQuery("SELECT quest, time FROM character_queststatus_daily WHERE guid = '{}'", GetGUID().GetCounter());

    if (result)
    {
        uint32 quest_daily_idx = 0;

        do
        {
            Field* fields = result->Fetch();
            if (Quest const* qQuest = sObjectMgr->GetQuestTemplate(fields[0].GetUInt32()))
            {
                if (qQuest->IsDFQuest())
                {
                    m_DFQuests.insert(qQuest->GetQuestId());
                    m_lastDailyQuestTime = time_t(fields[1].GetUInt32());
                    continue;
                }
            }

            if (quest_daily_idx >= PLAYER_MAX_DAILY_QUESTS)  // max amount with exist data in query
            {
                TC_LOG_ERROR("entities.player", "Player {} has more than 25 daily quest records in `charcter_queststatus_daily`", GetGUID().ToString());
                break;
            }

            uint32 quest_id = fields[0].GetUInt32();

            // save _any_ from daily quest times (it must be after last reset anyway)
            m_lastDailyQuestTime = time_t(fields[1].GetUInt32());

            Quest const* quest = sObjectMgr->GetQuestTemplate(quest_id);
            if (!quest)
                continue;

            SetUInt32Value(PLAYER_FIELD_DAILY_QUESTS_1+quest_daily_idx, quest_id);
            ++quest_daily_idx;

            TC_LOG_DEBUG("entities.player.loading", "Player::_LoadDailyQuestStatus: Loaded daily quest cooldown (QuestID: {}) for player '{}' ({})",
                quest_id, GetName(), GetGUID().ToString());
        }
        while (result->NextRow());
    }

    m_DailyQuestChanged = false;
}

void Player::_LoadWeeklyQuestStatus(PreparedQueryResult result)
{
    m_weeklyquests.clear();

    if (result)
    {
        do
        {
            Field* fields = result->Fetch();
            uint32 quest_id = fields[0].GetUInt32();
            Quest const* quest = sObjectMgr->GetQuestTemplate(quest_id);
            if (!quest)
                continue;

            m_weeklyquests.insert(quest_id);

            TC_LOG_DEBUG("entities.player.loading", "Player::_LoadWeeklyQuestStatus: Loaded weekly quest cooldown (QuestID: {}) for player '{}' ({})",
                quest_id, GetName(), GetGUID().ToString());
        }
        while (result->NextRow());
    }

    m_WeeklyQuestChanged = false;
}

void Player::_LoadSeasonalQuestStatus(PreparedQueryResult result)
{
    m_seasonalquests.clear();

    if (result)
    {
        do
        {
            Field* fields = result->Fetch();
            uint32 quest_id = fields[0].GetUInt32();
            uint32 event_id = fields[1].GetUInt32();
            Quest const* quest = sObjectMgr->GetQuestTemplate(quest_id);
            if (!quest)
                continue;

            m_seasonalquests[event_id].insert(quest_id);
            TC_LOG_DEBUG("entities.player.loading", "Player::_LoadSeasonalQuestStatus: Loaded seasonal quest cooldown (QuestID: {}) for player '{}' ({})",
                quest_id, GetName(), GetGUID().ToString());
        }
        while (result->NextRow());
    }

    m_SeasonalQuestChanged = false;
}

void Player::_LoadMonthlyQuestStatus(PreparedQueryResult result)
{
    m_monthlyquests.clear();

    if (result)
    {
        do
        {
            Field* fields = result->Fetch();
            uint32 quest_id = fields[0].GetUInt32();
            Quest const* quest = sObjectMgr->GetQuestTemplate(quest_id);
            if (!quest)
                continue;

            m_monthlyquests.insert(quest_id);
            TC_LOG_DEBUG("entities.player.loading", "Player::_LoadMonthlyQuestStatus: Loaded monthly quest cooldown (QuestID: {}) for player '{}' ({})",
                quest_id, GetName(), GetGUID().ToString());
        }
        while (result->NextRow());
    }

    m_MonthlyQuestChanged = false;
}

void Player::_LoadRandomBGStatus(PreparedQueryResult result)
{
    //QueryResult result = CharacterDatabase.PQuery("SELECT guid FROM character_battleground_random WHERE guid = '{}'", GetGUID().GetCounter());

    if (result)
        m_IsBGRandomWinner = true;
}

void Player::_LoadInventory(PreparedQueryResult result, uint32 timeDiff)
{
    //QueryResult* result = CharacterDatabase.PQuery("SELECT data, text, bag, slot, item, item_template FROM character_inventory JOIN item_instance ON character_inventory.item = item_instance.guid WHERE character_inventory.guid = '{}' ORDER BY bag, slot", GetGUID().GetCounter());
    //NOTE: the "order by `bag`" is important because it makes sure
    //the bagMap is filled before items in the bags are loaded
    //NOTE2: the "order by `slot`" is needed because mainhand weapons are (wrongly?)
    //expected to be equipped before offhand items (@todo fixme)

    if (result)
    {
        uint32 zoneId = GetZoneId();

        std::map<ObjectGuid::LowType, Bag*> bagMap;                  // fast guid lookup for bags
        std::map<ObjectGuid::LowType, Item*> invalidBagMap;          // fast guid lookup for bags
        std::list<Item*> problematicItems;
        CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();

        // Prevent items from being added to the queue while loading
        m_itemUpdateQueueBlocked = true;
        do
        {
            Field* fields = result->Fetch();
            if (Item* item = _LoadItem(trans, zoneId, timeDiff, fields))
            {
                ObjectGuid::LowType bagGuid = fields[11].GetUInt32();
                uint8  slot     = fields[12].GetUInt8();

                InventoryResult err = EQUIP_ERR_OK;
                // Item is not in bag
                if (!bagGuid)
                {
                    item->SetContainer(nullptr);
                    item->SetSlot(slot);

                    if (IsInventoryPos(INVENTORY_SLOT_BAG_0, slot))
                    {
                        ItemPosCountVec dest;
                        err = CanStoreItem(INVENTORY_SLOT_BAG_0, slot, dest, item, false);
                        if (err == EQUIP_ERR_OK)
                            item = StoreItem(dest, item, true);
                    }
                    else if (IsEquipmentPos(INVENTORY_SLOT_BAG_0, slot))
                    {
                        uint16 dest;
                        err = CanEquipItem(slot, dest, item, false, false);
                        if (err == EQUIP_ERR_OK)
                            QuickEquipItem(dest, item);
                    }
                    else if (IsBankPos(INVENTORY_SLOT_BAG_0, slot))
                    {
                        ItemPosCountVec dest;
                        err = CanBankItem(INVENTORY_SLOT_BAG_0, slot, dest, item, false, false);
                        if (err == EQUIP_ERR_OK)
                            item = BankItem(dest, item, true);
                    }

                    // Remember bags that may contain items in them
                    if (err == EQUIP_ERR_OK)
                    {
                        if (IsBagPos(item->GetPos()))
                            if (Bag* pBag = item->ToBag())
                                bagMap[item->GetGUID().GetCounter()] = pBag;
                    }
                    else
                        if (IsBagPos(item->GetPos()))
                            if (item->IsBag())
                                invalidBagMap[item->GetGUID().GetCounter()] = item;
                }
                else
                {
                    item->SetSlot(NULL_SLOT);
                    // Item is in the bag, find the bag
                    std::map<ObjectGuid::LowType, Bag*>::iterator itr = bagMap.find(bagGuid);
                    if (itr != bagMap.end())
                    {
                        ItemPosCountVec dest;
                        err = CanStoreItem(itr->second->GetSlot(), slot, dest, item);
                        if (err == EQUIP_ERR_OK)
                            item = StoreItem(dest, item, true);
                    }
                    else if (invalidBagMap.find(bagGuid) != invalidBagMap.end())
                    {
                        std::map<ObjectGuid::LowType, Item*>::iterator invalidBagItr = invalidBagMap.find(bagGuid);
                        if (std::find(problematicItems.begin(), problematicItems.end(), invalidBagItr->second) != problematicItems.end())
                            err = EQUIP_ERR_INT_BAG_ERROR;
                    }
                    else
                    {
                        TC_LOG_ERROR("entities.player", "Player::_LoadInventory: Player '{}' ({}) has item ({}, entry: {}) which doesnt have a valid bag (Bag {}, slot: {}). Possible cheat?",
                            GetName(), GetGUID().ToString(), item->GetGUID().ToString(), item->GetEntry(), bagGuid, slot);
                        item->DeleteFromInventoryDB(trans);
                        delete item;
                        continue;
                    }

                }

                // Item's state may have changed after storing
                if (err == EQUIP_ERR_OK)
                    item->SetState(ITEM_UNCHANGED, this);
                else
                {
                    TC_LOG_ERROR("entities.player", "Player::_LoadInventory: Player '{}' ({}) has item ({}, entry: {}) which can't be loaded into inventory (Bag {}, slot: {}) by reason {}. Item will be sent by mail.",
                        GetName(), GetGUID().ToString(), item->GetGUID().ToString(), item->GetEntry(), bagGuid, slot, uint32(err));
                    item->DeleteFromInventoryDB(trans);
                    problematicItems.push_back(item);
                }
            }
        } while (result->NextRow());

        m_itemUpdateQueueBlocked = false;

        // Send problematic items by mail
        while (!problematicItems.empty())
        {
            std::string subject = GetSession()->GetTrinityString(LANG_NOT_EQUIPPED_ITEM);

            MailDraft draft(subject, "There were problems with equipping item(s).");
            for (uint8 i = 0; !problematicItems.empty() && i < MAX_MAIL_ITEMS; ++i)
            {
                draft.AddItem(problematicItems.front());
                problematicItems.pop_front();
            }
            draft.SendMailTo(trans, this, MailSender(this, MAIL_STATIONERY_GM), MAIL_CHECK_MASK_COPIED);
        }
        CharacterDatabase.CommitTransaction(trans);
    }
    //if (IsAlive())
    _ApplyAllItemMods();
}

Item* Player::_LoadItem(CharacterDatabaseTransaction trans, uint32 zoneId, uint32 timeDiff, Field* fields)
{
    Item* item = nullptr;
    ObjectGuid::LowType itemGuid  = fields[13].GetUInt32();
    uint32 itemEntry = fields[14].GetUInt32();
    if (ItemTemplate const* proto = sObjectMgr->GetItemTemplate(itemEntry))
    {
        bool remove = false;
        item = NewItemOrBag(proto);
        if (item->LoadFromDB(itemGuid, GetGUID(), fields, itemEntry))
        {
            CharacterDatabasePreparedStatement* stmt;

            // Do not allow to have item limited to another map/zone in alive state
            if (IsAlive() && item->IsLimitedToAnotherMapOrZone(GetMapId(), zoneId))
            {
                TC_LOG_DEBUG("entities.player.loading", "Player::_LoadInventory: player ({}, name: '{}', map: {}) has item ({}) limited to another map ({}). Deleting item.",
                    GetGUID().ToString(), GetName(), GetMapId(), item->GetGUID().ToString(), zoneId);
                remove = true;
            }
            // "Conjured items disappear if you are logged out for more than 15 minutes"
            else if (timeDiff > 15 * MINUTE && proto->HasFlag(ITEM_FLAG_CONJURED))
            {
                TC_LOG_DEBUG("entities.player.loading", "Player::_LoadInventory: player ({}, name: '{}', diff: {}) has conjured item ({}) with expired lifetime (15 minutes). Deleting item.",
                    GetGUID().ToString(), GetName(), timeDiff, item->GetGUID().ToString());
                remove = true;
            }
            else if (item->IsRefundable())
            {
                if (item->GetPlayedTime() > (2 * HOUR))
                {
                    TC_LOG_DEBUG("entities.player.loading", "Player::_LoadInventory: player ({}, name: '{}') has item ({}) with expired refund time ({}). Deleting refund data and removing refundable flag.",
                        GetGUID().ToString(), GetName(), item->GetGUID().ToString(), item->GetPlayedTime());

                    stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_ITEM_REFUND_INSTANCE);
                    stmt->setUInt32(0, item->GetGUID().GetCounter());
                    trans->Append(stmt);

                    item->RemoveFlag(ITEM_FIELD_FLAGS, ITEM_FIELD_FLAG_REFUNDABLE);
                }
                else
                {
                    stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_ITEM_REFUNDS);
                    stmt->setUInt32(0, item->GetGUID().GetCounter());
                    stmt->setUInt32(1, GetGUID().GetCounter());
                    if (PreparedQueryResult result = CharacterDatabase.Query(stmt))
                    {
                        item->SetRefundRecipient((*result)[0].GetUInt32());
                        item->SetPaidMoney((*result)[1].GetUInt32());
                        item->SetPaidExtendedCost((*result)[2].GetUInt16());
                        AddRefundReference(item->GetGUID());
                    }
                    else
                    {
                        TC_LOG_WARN("entities.player.loading", "Player::_LoadInventory: player ({}, name: '{}') has item ({}) with refundable flags, but without data in item_refund_instance. Removing flag.",
                            GetGUID().ToString(), GetName(), item->GetGUID().ToString());
                        item->RemoveFlag(ITEM_FIELD_FLAGS, ITEM_FIELD_FLAG_REFUNDABLE);
                    }
                }
            }
            else if (item->IsBOPTradeable())
            {
                stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_ITEM_BOP_TRADE);
                stmt->setUInt32(0, item->GetGUID().GetCounter());
                if (PreparedQueryResult result = CharacterDatabase.Query(stmt))
                {
                    GuidSet looters;
                    for (std::string_view guidStr : Trinity::Tokenize((*result)[0].GetStringView(), ' ', false))
                    {
                        if (Optional<ObjectGuid::LowType> guid = Trinity::StringTo<ObjectGuid::LowType>(guidStr))
                            looters.insert(ObjectGuid::Create<HighGuid::Player>(*guid));
                        else
                            TC_LOG_WARN("entities.player.loading", "Player::_LoadInventory: invalid item_soulbound_trade_data GUID '{}' for item {}. Skipped.", std::string(guidStr), item->GetGUID().ToString());
                    }

                    if (looters.size() > 1 && item->GetTemplate()->GetMaxStackSize() == 1 && item->IsSoulBound())
                    {
                        item->SetSoulboundTradeable(looters);
                        AddTradeableItem(item);
                    }
                    else
                        item->ClearSoulboundTradeable(this);
                }
                else
                {
                    TC_LOG_WARN("entities.player.loading", "Player::_LoadInventory: player ({}, name: '{}') has item ({}) with ITEM_FLAG_BOP_TRADEABLE flag, but without data in item_soulbound_trade_data. Removing flag.",
                        GetGUID().ToString(), GetName(), item->GetGUID().ToString());
                    item->RemoveFlag(ITEM_FIELD_FLAGS, ITEM_FIELD_FLAG_BOP_TRADEABLE);
                }
            }
            else if (proto->HolidayId)
            {
                remove = true;
                GameEventMgr::GameEventDataMap const& events = sGameEventMgr->GetEventMap();
                GameEventMgr::ActiveEvents const& activeEventsList = sGameEventMgr->GetActiveEventList();
                for (GameEventMgr::ActiveEvents::const_iterator itr = activeEventsList.begin(); itr != activeEventsList.end(); ++itr)
                {
                    if (uint32(events[*itr].holiday_id) == proto->HolidayId)
                    {
                        remove = false;
                        break;
                    }
                }
            }
        }
        else
        {
            TC_LOG_ERROR("entities.player", "Player::_LoadInventory: player ({}, name: '{}') has a broken item (GUID: {}, entry: {}) in inventory. Deleting item.",
                GetGUID().ToString(), GetName(), itemGuid, itemEntry);
            remove = true;
        }
        // Remove item from inventory if necessary
        if (remove)
        {
            Item::DeleteFromInventoryDB(trans, itemGuid);
            item->FSetState(ITEM_REMOVED);
            item->SaveToDB(trans);                           // it also deletes item object!
            item = nullptr;
        }
    }
    else
    {
        TC_LOG_ERROR("entities.player", "Player::_LoadInventory: player ({}, name: '{}') has an unknown item (entry: {}) in inventory. Deleting item.",
            GetGUID().ToString(), GetName(), itemEntry);
        Item::DeleteFromInventoryDB(trans, itemGuid);
        Item::DeleteFromDB(trans, itemGuid);
    }
    return item;
}

void Player::_LoadActions(PreparedQueryResult result)
{
    m_actionButtons.clear();

    if (result)
    {
        do
        {
            Field* fields = result->Fetch();
            uint8 button = fields[0].GetUInt8();
            uint32 action = fields[1].GetUInt32();
            uint8 type = fields[2].GetUInt8();

            if (ActionButton* ab = addActionButton(button, action, type))
                ab->uState = ACTIONBUTTON_UNCHANGED;
            else
            {
                TC_LOG_DEBUG("entities.player", "Player::_LoadActions: Player '{}' ({}) has an invalid action button (Button: {}, Action: {}, Type: {}). It will be deleted at next save. This can be due to a player changing their talents.",
                    GetName(), GetGUID().ToString(), button, action, type);

                // Will be deleted in DB at next save (it can create data until save but marked as deleted).
                m_actionButtons[button].uState = ACTIONBUTTON_DELETED;
            }
        } while (result->NextRow());
    }
}

void Player::_LoadMail(PreparedQueryResult mailsResult, PreparedQueryResult mailItemsResult)
{
    m_mail.clear();

    std::unordered_map<uint32, Mail*> mailById;

    if (mailsResult)
    {
        do
        {
            Field* fields = mailsResult->Fetch();
            Mail* m = new Mail;

            m->messageID      = fields[0].GetUInt32();
            m->messageType    = fields[1].GetUInt8();
            m->sender         = fields[2].GetUInt32();
            m->receiver       = fields[3].GetUInt32();
            m->subject        = fields[4].GetString();
            m->body           = fields[5].GetString();
            m->expire_time    = time_t(fields[6].GetUInt32());
            m->deliver_time   = time_t(fields[7].GetUInt32());
            m->money          = fields[8].GetUInt32();
            m->COD            = fields[9].GetUInt32();
            m->checked        = fields[10].GetUInt8();
            m->stationery     = fields[11].GetUInt8();
            m->mailTemplateId = fields[12].GetInt16();

            if (m->mailTemplateId && !sMailTemplateStore.LookupEntry(m->mailTemplateId))
            {
                TC_LOG_ERROR("entities.player", "Player::_LoadMail: Mail ({}) has nonexistent MailTemplateId ({}), remove at load", m->messageID, m->mailTemplateId);
                m->mailTemplateId = 0;
            }

            m->state = MAIL_STATE_UNCHANGED;

            m_mail.push_back(m);
            mailById[m->messageID] = m;
        }
        while (mailsResult->NextRow());
    }

    if (mailItemsResult)
    {
        do
        {
            Field* fields = mailItemsResult->Fetch();
            uint32 mailId = fields[14].GetUInt32();
            _LoadMailedItem(GetGUID(), this, mailId, mailById[mailId], fields);
        } while (mailItemsResult->NextRow());
    }

    UpdateNextMailTimeAndUnreads();
}

// load mailed item which should receive current player
Item* Player::_LoadMailedItem(ObjectGuid const& playerGuid, Player* player, uint32 mailId, Mail* mail, Field* fields)
{
    ObjectGuid::LowType itemGuid = fields[11].GetUInt32();
    uint32 itemEntry = fields[12].GetUInt32();

    ItemTemplate const* proto = sObjectMgr->GetItemTemplate(itemEntry);
    if (!proto)
    {
        TC_LOG_ERROR("entities.player", "Player '{}' ({}) has unknown item in mailed items (GUID: {}, Entry: {}) in mail ({}), deleted.",
            player ? player->GetName() : "<unknown>", playerGuid.ToString(), itemGuid, itemEntry, mailId);

        CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();

        CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_INVALID_MAIL_ITEM);
        stmt->setUInt32(0, itemGuid);
        trans->Append(stmt);

        Item::DeleteFromDB(trans, itemGuid);

        CharacterDatabase.CommitTransaction(trans);
        return nullptr;
    }

    Item* item = NewItemOrBag(proto);

    ObjectGuid ownerGuid = fields[13].GetUInt32() ? ObjectGuid::Create<HighGuid::Player>(fields[13].GetUInt32()) : ObjectGuid::Empty;
    if (!item->LoadFromDB(itemGuid, ownerGuid, fields, itemEntry))
    {
        TC_LOG_ERROR("entities.player", "Player::_LoadMailedItems: Item (GUID: {}) in mail ({}) doesn't exist, deleted from mail.", itemGuid, mailId);

        CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_MAIL_ITEM);
        stmt->setUInt32(0, itemGuid);
        CharacterDatabase.Execute(stmt);

        item->FSetState(ITEM_REMOVED);

        CharacterDatabaseTransaction temp = CharacterDatabaseTransaction(nullptr);
        item->SaveToDB(temp);                               // it also deletes item object !
        return nullptr;
    }

    if (mail)
        mail->AddItem(itemGuid, itemEntry);

    if (player)
        player->AddMItem(item);

    return item;
}

void Player::_LoadDeclinedNames(PreparedQueryResult result)
{
    if (!result)
        return;

    delete m_declinedname;
    m_declinedname = new DeclinedName;
    for (uint8 i = 0; i < MAX_DECLINED_NAME_CASES; ++i)
        m_declinedname->name[i] = (*result)[i].GetString();
}

void Player::_LoadEquipmentSets(PreparedQueryResult result)
{
    // SetPQuery(PLAYER_LOGIN_QUERY_LOADEQUIPMENTSETS,   "SELECT setguid, setindex, name, iconname, item0, item1, item2, item3, item4, item5, item6, item7, item8, item9, item10, item11, item12, item13, item14, item15, item16, item17, item18 FROM character_equipmentsets WHERE guid = '{}' ORDER BY setindex", GUID_LOPART(m_guid));
    if (!result)
        return;

    do
    {
        Field* fields = result->Fetch();
        EquipmentSetInfo eqSet;

        eqSet.Data.Guid       = fields[0].GetUInt64();
        eqSet.Data.SetID      = fields[1].GetUInt8();
        eqSet.Data.SetName    = fields[2].GetString();
        eqSet.Data.SetIcon    = fields[3].GetString();
        eqSet.Data.IgnoreMask = fields[4].GetUInt32();
        eqSet.State           = EQUIPMENT_SET_UNCHANGED;

        for (uint32 i = 0; i < EQUIPMENT_SLOT_END; ++i)
            if (ObjectGuid::LowType guid = fields[5 + i].GetUInt32())
                eqSet.Data.Pieces[i] = ObjectGuid::Create<HighGuid::Item>(guid);

        if (eqSet.Data.SetID >= MAX_EQUIPMENT_SET_INDEX)    // client limit
            continue;

        _equipmentSets[eqSet.Data.Guid] = eqSet;
    } while (result->NextRow());
}

// Called on login after LoadFromDB
void Player::LoadCorpse(PreparedQueryResult result)
{
    if (IsAlive() || HasAtLoginFlag(AT_LOGIN_RESURRECT))
        SpawnCorpseBones(false);

    if (!IsAlive())
    {
        if (HasAtLoginFlag(AT_LOGIN_RESURRECT))
            ResurrectPlayer(0.5f);
        else if (result)
        {
            Field* fields = result->Fetch();
            _corpseLocation.WorldRelocate(fields[0].GetUInt16(), fields[1].GetFloat(), fields[2].GetFloat(), fields[3].GetFloat(), fields[4].GetFloat());
            ApplyModByteFlag(PLAYER_FIELD_BYTES, PLAYER_FIELD_BYTES_OFFSET_FLAGS, PLAYER_FIELD_BYTE_RELEASE_TIMER, !sMapStore.LookupEntry(_corpseLocation.GetMapId())->Instanceable());
        }
    }

    RemoveAtLoginFlag(AT_LOGIN_RESURRECT);
}

// Called on login after LoadFromDB
void Player::LoadPet()
{
    //fixme: the pet should still be loaded if the player is not in world
    // just not added to the map
    if (m_petStable && IsInWorld())
    {
        Pet* pet = new Pet(this);
        if (!pet->LoadPetFromDB(this, 0, 0, true))
            delete pet;
    }
}

void Player::SaveToDB(bool create /*=false*/)
{
    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();

    SaveToDB(trans, create);

    CharacterDatabase.CommitTransaction(trans);
}

void Player::SaveToDB(CharacterDatabaseTransaction trans, bool create /* = false */)
{
    // delay auto save at any saves (manual, in code, or autosave)
    m_nextSave = sWorld->getIntConfig(CONFIG_INTERVAL_SAVE);

    //lets allow only players in world to be saved
    if (IsBeingTeleportedFar())
    {
        ScheduleDelayedOperation(DELAYED_SAVE_PLAYER);
        return;
    }

    // first save/honor gain after midnight will also update the player's honor fields
    UpdateHonorFields();

    TC_LOG_DEBUG("entities.unit", "Player::SaveToDB: The value of player {} at save: ", m_name);
    outDebugValues();

    if (!create)
        sScriptMgr->OnPlayerSave(this);

    CharacterDatabasePreparedStatement* stmt = nullptr;
    uint8 index = 0;

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHAR_FISHINGSTEPS);
    stmt->setUInt32(0, GetGUID().GetCounter());
    trans->Append(stmt);

    auto finiteAlways = [](float f) { return std::isfinite(f) ? f : 0.0f; };

    if (create)
    {
        //! Insert query
        /// @todo: Filter out more redundant fields that can take their default value at player create
        stmt = CharacterDatabase.GetPreparedStatement(CHAR_INS_CHARACTER);
        stmt->setUInt32(index++, GetGUID().GetCounter());
        stmt->setUInt32(index++, GetSession()->GetAccountId());
        stmt->setString(index++, GetName());
        stmt->setUInt8(index++, GetRace());
        stmt->setUInt8(index++, GetClass());
        stmt->setUInt8(index++, GetNativeGender());   // save gender from PLAYER_BYTES_3, UNIT_BYTES_0 changes with every transform effect
        stmt->setUInt8(index++, GetLevel());
        stmt->setUInt32(index++, GetXP());
        stmt->setUInt32(index++, GetMoney());
        stmt->setUInt8(index++, GetSkinId());
        stmt->setUInt8(index++, GetFaceId());
        stmt->setUInt8(index++, GetHairStyleId());
        stmt->setUInt8(index++, GetHairColorId());
        stmt->setUInt8(index++, GetFacialStyle());
        stmt->setUInt8(index++, GetBankBagSlotCount());
        stmt->setUInt8(index++, GetRestState());
        stmt->setUInt32(index++, GetUInt32Value(PLAYER_FLAGS));
        stmt->setUInt16(index++, (uint16)GetMapId());
        stmt->setUInt32(index++, (uint32)GetInstanceId());
        stmt->setUInt8(index++, (uint8(GetDungeonDifficulty()) | uint8(GetRaidDifficulty()) << 4));
        stmt->setFloat(index++, finiteAlways(GetPositionX()));
        stmt->setFloat(index++, finiteAlways(GetPositionY()));
        stmt->setFloat(index++, finiteAlways(GetPositionZ()));
        stmt->setFloat(index++, finiteAlways(GetOrientation()));
        stmt->setFloat(index++, finiteAlways(GetTransOffsetX()));
        stmt->setFloat(index++, finiteAlways(GetTransOffsetY()));
        stmt->setFloat(index++, finiteAlways(GetTransOffsetZ()));
        stmt->setFloat(index++, finiteAlways(GetTransOffsetO()));
        ObjectGuid::LowType transLowGUID = 0;
        if (GetTransport() && GetTransport()->GetGOInfo()->type == GAMEOBJECT_TYPE_MO_TRANSPORT) // TODO Type 11 Login
            transLowGUID = GetTransport()->GetGUID().GetCounter();
        stmt->setUInt32(index++, transLowGUID);

        std::ostringstream ss;
        ss << m_taxi;
        stmt->setString(index++, ss.str());
        stmt->setUInt8(index++, m_cinematic);
        stmt->setUInt32(index++, m_Played_time[PLAYED_TIME_TOTAL]);
        stmt->setUInt32(index++, m_Played_time[PLAYED_TIME_LEVEL]);
        stmt->setFloat(index++, finiteAlways(m_rest_bonus));
        stmt->setUInt32(index++, uint32(GameTime::GetGameTime()));
        stmt->setUInt8(index++,  (HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_RESTING) ? 1 : 0));
        //save, far from tavern/city
        //save, but in tavern/city
        stmt->setUInt32(index++, m_resetTalentsCost);
        stmt->setUInt32(index++, uint32(m_resetTalentsTime));
        stmt->setUInt16(index++, (uint16)m_ExtraFlags);
        stmt->setUInt8(index++,  m_petStable ? m_petStable->MaxStabledPets : 0);
        stmt->setUInt16(index++, (uint16)m_atLoginFlags);
        stmt->setUInt16(index++, GetZoneId());
        stmt->setUInt32(index++, uint32(m_deathExpireTime));

        ss.str("");
        ss << m_taxi.SaveTaxiDestinationsToString();

        stmt->setString(index++, ss.str());
        stmt->setUInt32(index++, GetArenaPoints());
        stmt->setUInt32(index++, GetHonorPoints());
        stmt->setUInt32(index++, GetUInt32Value(PLAYER_FIELD_TODAY_CONTRIBUTION));
        stmt->setUInt32(index++, GetUInt32Value(PLAYER_FIELD_YESTERDAY_CONTRIBUTION));
        stmt->setUInt32(index++, GetUInt32Value(PLAYER_FIELD_LIFETIME_HONORABLE_KILLS));
        stmt->setUInt16(index++, GetUInt16Value(PLAYER_FIELD_KILLS, 0));
        stmt->setUInt16(index++, GetUInt16Value(PLAYER_FIELD_KILLS, 1));
        stmt->setUInt32(index++, GetUInt32Value(PLAYER_CHOSEN_TITLE));
        stmt->setUInt64(index++, GetUInt64Value(PLAYER_FIELD_KNOWN_CURRENCIES));
        stmt->setUInt32(index++, GetUInt32Value(PLAYER_FIELD_WATCHED_FACTION_INDEX));
        stmt->setUInt8(index++, GetDrunkValue());
        stmt->setUInt32(index++, GetHealth());

        for (uint32 i = 0; i < MAX_POWERS; ++i)
            stmt->setUInt32(index++, GetPower(Powers(i)));

        stmt->setUInt32(index++, GetSession()->GetLatency());

        stmt->setUInt8(index++, m_specsCount);
        stmt->setUInt8(index++, m_activeSpec);

        ss.str("");
        for (uint32 i = 0; i < PLAYER_EXPLORED_ZONES_SIZE; ++i)
            ss << GetUInt32Value(PLAYER_EXPLORED_ZONES_1 + i) << ' ';
        stmt->setString(index++, ss.str());

        ss.str("");
        // cache equipment...
        for (uint32 i = 0; i < EQUIPMENT_SLOT_END * 2; ++i)
            ss << GetUInt32Value(PLAYER_VISIBLE_ITEM_1_ENTRYID + i) << ' ';

        // ...and bags for enum opcode
        for (uint32 i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; ++i)
        {
            if (Item* item = GetItemByPos(INVENTORY_SLOT_BAG_0, i))
                ss << item->GetEntry();
            else
                ss << '0';
            ss << " 0 ";
        }

        stmt->setString(index++, ss.str());
        stmt->setUInt32(index++, GetUInt32Value(PLAYER_AMMO_ID));

        ss.str("");
        for (uint32 i = 0; i < KNOWN_TITLES_SIZE*2; ++i)
            ss << GetUInt32Value(PLAYER__FIELD_KNOWN_TITLES + i) << ' ';

        stmt->setString(index++, ss.str());
        stmt->setUInt8(index++, GetByteValue(PLAYER_FIELD_BYTES, PLAYER_FIELD_BYTES_OFFSET_ACTION_BAR_TOGGLES));
        stmt->setUInt32(index++, m_grantableLevels);
    }
    else
    {
        // Update query
        stmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_CHARACTER);
        stmt->setString(index++, GetName());
        stmt->setUInt8(index++, GetRace());
        stmt->setUInt8(index++, GetClass());
        stmt->setUInt8(index++, GetNativeGender());   // save gender from PLAYER_BYTES_3, UNIT_BYTES_0 changes with every transform effect
        stmt->setUInt8(index++, GetLevel());
        stmt->setUInt32(index++, GetXP());
        stmt->setUInt32(index++, GetMoney());
        stmt->setUInt8(index++, GetSkinId());
        stmt->setUInt8(index++, GetFaceId());
        stmt->setUInt8(index++, GetHairStyleId());
        stmt->setUInt8(index++, GetHairColorId());
        stmt->setUInt8(index++, GetFacialStyle());
        stmt->setUInt8(index++, GetBankBagSlotCount());
        stmt->setUInt8(index++, GetRestState());
        stmt->setUInt32(index++, GetUInt32Value(PLAYER_FLAGS));

        if (!IsBeingTeleported())
        {
            stmt->setUInt16(index++, (uint16)GetMapId());
            stmt->setUInt32(index++, (uint32)GetInstanceId());
            stmt->setUInt8(index++, (uint8(GetDungeonDifficulty()) | uint8(GetRaidDifficulty()) << 4));
            stmt->setFloat(index++, finiteAlways(GetPositionX()));
            stmt->setFloat(index++, finiteAlways(GetPositionY()));
            stmt->setFloat(index++, finiteAlways(GetPositionZ()));
            stmt->setFloat(index++, finiteAlways(GetOrientation()));
        }
        else
        {
            stmt->setUInt16(index++, (uint16)GetTeleportDest().GetMapId());
            stmt->setUInt32(index++, (uint32)0);
            stmt->setUInt8(index++, (uint8(GetDungeonDifficulty()) | uint8(GetRaidDifficulty()) << 4));
            stmt->setFloat(index++, finiteAlways(GetTeleportDest().GetPositionX()));
            stmt->setFloat(index++, finiteAlways(GetTeleportDest().GetPositionY()));
            stmt->setFloat(index++, finiteAlways(GetTeleportDest().GetPositionZ()));
            stmt->setFloat(index++, finiteAlways(GetTeleportDest().GetOrientation()));
        }

        stmt->setFloat(index++, finiteAlways(GetTransOffsetX()));
        stmt->setFloat(index++, finiteAlways(GetTransOffsetY()));
        stmt->setFloat(index++, finiteAlways(GetTransOffsetZ()));
        stmt->setFloat(index++, finiteAlways(GetTransOffsetO()));
        ObjectGuid::LowType transLowGUID = 0;
        if (GetTransport() && GetTransport()->GetGOInfo()->type == GAMEOBJECT_TYPE_MO_TRANSPORT) // TODO Type 11 Login
            transLowGUID = GetTransport()->GetGUID().GetCounter();
        stmt->setUInt32(index++, transLowGUID);

        std::ostringstream ss;
        ss << m_taxi;
        stmt->setString(index++, ss.str());
        stmt->setUInt8(index++, m_cinematic);
        stmt->setUInt32(index++, m_Played_time[PLAYED_TIME_TOTAL]);
        stmt->setUInt32(index++, m_Played_time[PLAYED_TIME_LEVEL]);
        stmt->setFloat(index++, finiteAlways(m_rest_bonus));
        stmt->setUInt32(index++, uint32(GameTime::GetGameTime()));
        stmt->setUInt8(index++,  (HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_RESTING) ? 1 : 0));
        //save, far from tavern/city
        //save, but in tavern/city
        stmt->setUInt32(index++, m_resetTalentsCost);
        stmt->setUInt32(index++, uint32(m_resetTalentsTime));
        stmt->setUInt16(index++, (uint16)m_ExtraFlags);
        stmt->setUInt8(index++,  m_petStable ? m_petStable->MaxStabledPets : 0);
        stmt->setUInt16(index++, (uint16)m_atLoginFlags);
        stmt->setUInt16(index++, GetZoneId());
        stmt->setUInt32(index++, uint32(m_deathExpireTime));

        ss.str("");
        ss << m_taxi.SaveTaxiDestinationsToString();

        stmt->setString(index++, ss.str());
        stmt->setUInt32(index++, GetArenaPoints());
        stmt->setUInt32(index++, GetHonorPoints());
        stmt->setUInt32(index++, GetUInt32Value(PLAYER_FIELD_TODAY_CONTRIBUTION));
        stmt->setUInt32(index++, GetUInt32Value(PLAYER_FIELD_YESTERDAY_CONTRIBUTION));
        stmt->setUInt32(index++, GetUInt32Value(PLAYER_FIELD_LIFETIME_HONORABLE_KILLS));
        stmt->setUInt16(index++, GetUInt16Value(PLAYER_FIELD_KILLS, 0));
        stmt->setUInt16(index++, GetUInt16Value(PLAYER_FIELD_KILLS, 1));
        stmt->setUInt32(index++, GetUInt32Value(PLAYER_CHOSEN_TITLE));
        stmt->setUInt64(index++, GetUInt64Value(PLAYER_FIELD_KNOWN_CURRENCIES));
        stmt->setUInt32(index++, GetUInt32Value(PLAYER_FIELD_WATCHED_FACTION_INDEX));
        stmt->setUInt8(index++, GetDrunkValue());
        stmt->setUInt32(index++, GetHealth());

        for (uint32 i = 0; i < MAX_POWERS; ++i)
            stmt->setUInt32(index++, GetPower(Powers(i)));

        stmt->setUInt32(index++, GetSession()->GetLatency());

        stmt->setUInt8(index++, m_specsCount);
        stmt->setUInt8(index++, m_activeSpec);

        ss.str("");
        for (uint32 i = 0; i < PLAYER_EXPLORED_ZONES_SIZE; ++i)
            ss << GetUInt32Value(PLAYER_EXPLORED_ZONES_1 + i) << ' ';
        stmt->setString(index++, ss.str());

        ss.str("");
        // cache equipment...
        for (uint32 i = 0; i < EQUIPMENT_SLOT_END * 2; ++i)
            ss << GetUInt32Value(PLAYER_VISIBLE_ITEM_1_ENTRYID + i) << ' ';

        // ...and bags for enum opcode
        for (uint32 i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; ++i)
        {
            if (Item* item = GetItemByPos(INVENTORY_SLOT_BAG_0, i))
                ss << item->GetEntry();
            else
                ss << '0';
            ss << " 0 ";
        }

        stmt->setString(index++, ss.str());
        stmt->setUInt32(index++, GetUInt32Value(PLAYER_AMMO_ID));

        ss.str("");
        for (uint32 i = 0; i < KNOWN_TITLES_SIZE*2; ++i)
            ss << GetUInt32Value(PLAYER__FIELD_KNOWN_TITLES + i) << ' ';

        stmt->setString(index++, ss.str());
        stmt->setUInt8(index++, GetByteValue(PLAYER_FIELD_BYTES, PLAYER_FIELD_BYTES_OFFSET_ACTION_BAR_TOGGLES));
        stmt->setUInt32(index++, m_grantableLevels);

        stmt->setUInt8(index++, IsInWorld() && !GetSession()->PlayerLogout() ? 1 : 0);
        // Index
        stmt->setUInt32(index++, GetGUID().GetCounter());
    }

    trans->Append(stmt);

    if (m_fishingSteps != 0)
    {
        stmt = CharacterDatabase.GetPreparedStatement(CHAR_INS_CHAR_FISHINGSTEPS);
        index = 0;
        stmt->setUInt32(index++, GetGUID().GetCounter());
        stmt->setUInt32(index++, m_fishingSteps);
        trans->Append(stmt);
    }

    if (m_mailsUpdated)                                     //save mails only when needed
        _SaveMail(trans);

    _SaveBGData(trans);
    _SaveInventory(trans);
    _SaveQuestStatus(trans);
    _SaveDailyQuestStatus(trans);
    _SaveWeeklyQuestStatus(trans);
    _SaveSeasonalQuestStatus(trans);
    _SaveMonthlyQuestStatus(trans);
    _SaveTalents(trans);
    _SaveSpells(trans);
    GetSpellHistory()->SaveToDB<Player>(trans);
    _SaveActions(trans);
    _SaveAuras(trans);
    _SaveSkills(trans);
    m_achievementMgr->SaveToDB(trans);
    m_reputationMgr->SaveToDB(trans);
    _SaveEquipmentSets(trans);
    GetSession()->SaveTutorialsData(trans);                 // changed only while character in game
    _SaveGlyphs(trans);
    // @tswow-begin TODO: fix transaction
    m_db_json.Save();
    // @tswow-end
    GetSession()->SaveInstanceTimeRestrictions(trans);

    // check if stats should only be saved on logout
    // save stats can be out of transaction
    if (m_session->isLogingOut() || !sWorld->getBoolConfig(CONFIG_STATS_SAVE_ONLY_ON_LOGOUT))
        _SaveStats(trans);

    // @epoch-begin
    // we save the data here to prevent spamming
    sAnticheatMgr->SavePlayerData(this);
    // @epoch-end

    // save pet (hunter pet level and experience and all type pets health/mana).
    if (Pet* pet = GetPet())
        pet->SavePetToDB(PET_SAVE_AS_CURRENT);
}

void Player::outDebugValues() const
{
    if (!sLog->ShouldLog("entities.unit", LOG_LEVEL_DEBUG))
        return;

    TC_LOG_DEBUG("entities.unit", "HP is: \t\t\t{}\t\tMP is: \t\t\t{}", GetMaxHealth(), GetMaxPower(POWER_MANA));
    TC_LOG_DEBUG("entities.unit", "AGILITY is: \t\t{}\t\tSTRENGTH is: \t\t{}", GetStat(STAT_AGILITY), GetStat(STAT_STRENGTH));
    TC_LOG_DEBUG("entities.unit", "INTELLECT is: \t\t{}\t\tSPIRIT is: \t\t{}", GetStat(STAT_INTELLECT), GetStat(STAT_SPIRIT));
    TC_LOG_DEBUG("entities.unit", "STAMINA is: \t\t{}", GetStat(STAT_STAMINA));
    TC_LOG_DEBUG("entities.unit", "Armor is: \t\t{}\t\tBlock is: \t\t{}", GetArmor(), GetFloatValue(PLAYER_BLOCK_PERCENTAGE));
    TC_LOG_DEBUG("entities.unit", "HolyRes is: \t\t{}\t\tFireRes is: \t\t{}", GetResistance(SPELL_SCHOOL_HOLY), GetResistance(SPELL_SCHOOL_FIRE));
    TC_LOG_DEBUG("entities.unit", "NatureRes is: \t\t{}\t\tFrostRes is: \t\t{}", GetResistance(SPELL_SCHOOL_NATURE), GetResistance(SPELL_SCHOOL_FROST));
    TC_LOG_DEBUG("entities.unit", "ShadowRes is: \t\t{}\t\tArcaneRes is: \t\t{}", GetResistance(SPELL_SCHOOL_SHADOW), GetResistance(SPELL_SCHOOL_ARCANE));
    TC_LOG_DEBUG("entities.unit", "MIN_DAMAGE is: \t\t{}\tMAX_DAMAGE is: \t\t{}", GetFloatValue(UNIT_FIELD_MINDAMAGE), GetFloatValue(UNIT_FIELD_MAXDAMAGE));
    TC_LOG_DEBUG("entities.unit", "MIN_OFFHAND_DAMAGE is: \t{}\tMAX_OFFHAND_DAMAGE is: \t{}", GetFloatValue(UNIT_FIELD_MINOFFHANDDAMAGE), GetFloatValue(UNIT_FIELD_MAXOFFHANDDAMAGE));
    TC_LOG_DEBUG("entities.unit", "MIN_RANGED_DAMAGE is: \t{}\tMAX_RANGED_DAMAGE is: \t{}", GetFloatValue(UNIT_FIELD_MINRANGEDDAMAGE), GetFloatValue(UNIT_FIELD_MAXRANGEDDAMAGE));
    TC_LOG_DEBUG("entities.unit", "ATTACK_TIME is: \t{}\t\tRANGE_ATTACK_TIME is: \t{}", GetAttackTime(BASE_ATTACK), GetAttackTime(RANGED_ATTACK));
}

void Player::_SaveMail(CharacterDatabaseTransaction trans)
{
    CharacterDatabasePreparedStatement* stmt;

    for (PlayerMails::iterator itr = m_mail.begin(); itr != m_mail.end(); ++itr)
    {
        Mail* m = (*itr);
        if (m->state == MAIL_STATE_CHANGED)
        {
            stmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_MAIL);
            stmt->setUInt8(0, uint8(m->HasItems() ? 1 : 0));
            stmt->setUInt32(1, uint32(m->expire_time));
            stmt->setUInt32(2, uint32(m->deliver_time));
            stmt->setUInt32(3, m->money);
            stmt->setUInt32(4, m->COD);
            stmt->setUInt8(5, uint8(m->checked));
            stmt->setUInt32(6, m->messageID);

            trans->Append(stmt);

            if (!m->removedItems.empty())
            {
                for (std::vector<uint32>::iterator itr2 = m->removedItems.begin(); itr2 != m->removedItems.end(); ++itr2)
                {
                    stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_MAIL_ITEM);
                    stmt->setUInt32(0, *itr2);
                    trans->Append(stmt);
                }
                m->removedItems.clear();
            }
            m->state = MAIL_STATE_UNCHANGED;
        }
        else if (m->state == MAIL_STATE_DELETED)
        {
            if (m->HasItems())
            {
                for (MailItemInfoVec::iterator itr2 = m->items.begin(); itr2 != m->items.end(); ++itr2)
                {
                    stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_ITEM_INSTANCE);
                    stmt->setUInt32(0, itr2->item_guid);
                    trans->Append(stmt);
                }
            }
            stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_MAIL_BY_ID);
            stmt->setUInt32(0, m->messageID);
            trans->Append(stmt);

            stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_MAIL_ITEM_BY_ID);
            stmt->setUInt32(0, m->messageID);
            trans->Append(stmt);
        }
    }

    //deallocate deleted mails...
    for (PlayerMails::iterator itr = m_mail.begin(); itr != m_mail.end();)
    {
        if ((*itr)->state == MAIL_STATE_DELETED)
        {
            Mail* m = *itr;
            m_mail.erase(itr);
            delete m;
            itr = m_mail.begin();
        }
        else
            ++itr;
    }

    m_mailsUpdated = false;
}

void Player::_SaveBGData(CharacterDatabaseTransaction trans)
{
    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_PLAYER_BGDATA);
    stmt->setUInt32(0, GetGUID().GetCounter());
    trans->Append(stmt);
    /* guid, bgInstanceID, bgTeam, x, y, z, o, map, taxi[0], taxi[1], mountSpell */
    stmt = CharacterDatabase.GetPreparedStatement(CHAR_INS_PLAYER_BGDATA);
    stmt->setUInt32(0, GetGUID().GetCounter());
    stmt->setUInt32(1, m_bgData.bgInstanceID);
    stmt->setUInt16(2, m_bgData.bgTeam);
    stmt->setFloat (3, m_bgData.joinPos.GetPositionX());
    stmt->setFloat (4, m_bgData.joinPos.GetPositionY());
    stmt->setFloat (5, m_bgData.joinPos.GetPositionZ());
    stmt->setFloat (6, m_bgData.joinPos.GetOrientation());
    stmt->setUInt16(7, m_bgData.joinPos.GetMapId());
    stmt->setUInt32(8, m_bgData.taxiPath[0]);
    stmt->setUInt32(9, m_bgData.taxiPath[1]);
    stmt->setUInt32(10, m_bgData.mountSpell);
    trans->Append(stmt);
}

void Player::_SaveInventory(CharacterDatabaseTransaction trans)
{
    CharacterDatabasePreparedStatement* stmt;
    // force items in buyback slots to new state
    // and remove those that aren't already
    for (uint8 i = BUYBACK_SLOT_START; i < BUYBACK_SLOT_END; ++i)
    {
        Item* item = m_items[i];
        if (!item)
            continue;

        if (item->GetState() == ITEM_NEW)
        {
            if (ItemTemplate const* itemTemplate = item->GetTemplate())
                if (itemTemplate->HasFlag(ITEM_FLAG_HAS_LOOT))
                    sLootItemStorage->RemoveStoredLootForContainer(item->GetGUID().GetCounter());

            continue;
        }

        stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHAR_INVENTORY_BY_ITEM);
        stmt->setUInt32(0, item->GetGUID().GetCounter());
        trans->Append(stmt);

        stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_ITEM_INSTANCE);
        stmt->setUInt32(0, item->GetGUID().GetCounter());
        trans->Append(stmt);
        m_items[i]->FSetState(ITEM_NEW);

        if (ItemTemplate const* itemTemplate = item->GetTemplate())
            if (itemTemplate->HasFlag(ITEM_FLAG_HAS_LOOT))
                sLootItemStorage->RemoveStoredLootForContainer(item->GetGUID().GetCounter());
    }

    // Updated played time for refundable items. We don't do this in Player::Update because there's simply no need for it,
    // the client auto counts down in real time after having received the initial played time on the first
    // SMSG_ITEM_REFUND_INFO_RESPONSE packet.
    // Item::UpdatePlayedTime is only called when needed, which is in DB saves, and item refund info requests.
    GuidSet::iterator i_next;
    for (GuidSet::iterator itr = m_refundableItems.begin(); itr!= m_refundableItems.end(); itr = i_next)
    {
        // use copy iterator because itr may be invalid after operations in this loop
        i_next = itr;
        ++i_next;

        Item* iPtr = GetItemByGuid(*itr);
        if (iPtr)
        {
            iPtr->UpdatePlayedTime(this);
            continue;
        }
        else
        {
            TC_LOG_ERROR("entities.player", "Player::_SaveInventory: Can't find item ({}) in refundable storage for player '{}' ({}), removing.",
                itr->ToString(), GetName(), GetGUID().ToString());
            m_refundableItems.erase(itr);
        }
    }

    // update enchantment durations
    for (EnchantDurationList::iterator itr = m_enchantDuration.begin(); itr != m_enchantDuration.end(); ++itr)
        itr->item->SetEnchantmentDuration(itr->slot, itr->leftduration, this);

    // if no changes
    if (m_itemUpdateQueue.empty())
        return;

    ObjectGuid::LowType lowGuid = GetGUID().GetCounter();
    for (size_t i = 0; i < m_itemUpdateQueue.size(); ++i)
    {
        Item* item = m_itemUpdateQueue[i];
        if (!item)
            continue;

        Bag* container = item->GetContainer();
        ObjectGuid::LowType bag_guid = container ? container->GetGUID().GetCounter() : 0;

        if (item->GetState() != ITEM_REMOVED)
        {
            Item* test = GetItemByPos(item->GetBagSlot(), item->GetSlot());
            if (test == nullptr)
            {
                ObjectGuid::LowType bagTestGUID = 0;
                if (Item* test2 = GetItemByPos(INVENTORY_SLOT_BAG_0, item->GetBagSlot()))
                    bagTestGUID = test2->GetGUID().GetCounter();

                TC_LOG_ERROR("entities.player", "Player::_SaveInventory: Player '{}' ({}) has incorrect values (Bag: {}, Slot: {}) for the item ({}, State: {}). The player doesn't have an item at that position.",
                    GetName(), GetGUID().ToString(), item->GetBagSlot(), item->GetSlot(), item->GetGUID().ToString(), (int32)item->GetState());
                // according to the test that was just performed nothing should be in this slot, delete
                stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHAR_INVENTORY_BY_BAG_SLOT);
                stmt->setUInt32(0, bagTestGUID);
                stmt->setUInt8(1, item->GetSlot());
                stmt->setUInt32(2, lowGuid);
                trans->Append(stmt);

                RemoveTradeableItem(item);
                RemoveEnchantmentDurationsReferences(item);
                RemoveItemDurations(item);

                // also THIS item should be somewhere else, cheat attempt
                item->FSetState(ITEM_REMOVED); // we are IN updateQueue right now, can't use SetState which modifies the queue
                DeleteRefundReference(item->GetGUID());
                // don't skip, let the switch delete it
                //continue;
            }
            else if (test != item)
            {
                TC_LOG_ERROR("entities.player", "Player::_SaveInventory: Player '{}' ({}) has incorrect values (Bag: {}, Slot: {}) for the item ({}). {} is there instead!",
                    GetName(), GetGUID().ToString(), item->GetBagSlot(), item->GetSlot(), item->GetGUID().ToString(), test->GetGUID().ToString());
                // save all changes to the item...
                if (item->GetState() != ITEM_NEW) // only for existing items, no duplicates
                    item->SaveToDB(trans);
                // ...but do not save position in invntory
                continue;
            }
        }

        switch (item->GetState())
        {
            case ITEM_NEW:
            case ITEM_CHANGED:
                stmt = CharacterDatabase.GetPreparedStatement(CHAR_REP_INVENTORY_ITEM);
                stmt->setUInt32(0, lowGuid);
                stmt->setUInt32(1, bag_guid);
                stmt->setUInt8 (2, item->GetSlot());
                stmt->setUInt32(3, item->GetGUID().GetCounter());
                trans->Append(stmt);
                break;
            case ITEM_REMOVED:
                stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHAR_INVENTORY_BY_ITEM);
                stmt->setUInt32(0, item->GetGUID().GetCounter());
                trans->Append(stmt);
                break;
            case ITEM_UNCHANGED:
                break;
        }

        item->SaveToDB(trans);                                   // item have unchanged inventory record and can be save standalone
    }
    m_itemUpdateQueue.clear();
}

void Player::_SaveQuestStatus(CharacterDatabaseTransaction trans)
{
    bool isTransaction = bool(trans);
    if (!isTransaction)
        trans = CharacterDatabase.BeginTransaction();

    QuestStatusSaveMap::iterator saveItr;
    QuestStatusMap::iterator statusItr;
    CharacterDatabasePreparedStatement* stmt;

    bool keepAbandoned = !(sWorld->GetCleaningFlags() & CharacterDatabaseCleaner::CLEANING_FLAG_QUESTSTATUS);

    for (saveItr = m_QuestStatusSave.begin(); saveItr != m_QuestStatusSave.end(); ++saveItr)
    {
        if (saveItr->second == QUEST_DEFAULT_SAVE_TYPE)
        {
            statusItr = m_QuestStatus.find(saveItr->first);
            if (statusItr != m_QuestStatus.end() && (keepAbandoned || statusItr->second.Status != QUEST_STATUS_NONE))
            {
                uint8 index = 0;
                stmt = CharacterDatabase.GetPreparedStatement(CHAR_REP_CHAR_QUESTSTATUS);

                stmt->setUInt32(index++, GetGUID().GetCounter());
                stmt->setUInt32(index++, statusItr->first);
                stmt->setUInt8(index++, uint8(statusItr->second.Status));
                stmt->setBool(index++, statusItr->second.Explored);
                stmt->setUInt32(index++, uint32(statusItr->second.Timer / IN_MILLISECONDS+ GameTime::GetGameTime()));

                for (uint8 i = 0; i < QUEST_OBJECTIVES_COUNT; i++)
                    stmt->setUInt16(index++, statusItr->second.CreatureOrGOCount[i]);

                for (uint8 i = 0; i < QUEST_ITEM_OBJECTIVES_COUNT; i++)
                    stmt->setUInt16(index++, statusItr->second.ItemCount[i]);

                stmt->setUInt16(index, statusItr->second.PlayerCount);
                trans->Append(stmt);
            }
        }
        else
        {
            stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHAR_QUESTSTATUS_BY_QUEST);
            stmt->setUInt32(0, GetGUID().GetCounter());
            stmt->setUInt32(1, saveItr->first);
            trans->Append(stmt);
        }
    }

    m_QuestStatusSave.clear();

    for (saveItr = m_RewardedQuestsSave.begin(); saveItr != m_RewardedQuestsSave.end(); ++saveItr)
    {
        if (saveItr->second == QUEST_DEFAULT_SAVE_TYPE)
        {
            stmt = CharacterDatabase.GetPreparedStatement(CHAR_INS_CHAR_QUESTSTATUS_REWARDED);
            stmt->setUInt32(0, GetGUID().GetCounter());
            stmt->setUInt32(1, saveItr->first);
            trans->Append(stmt);

        }
        else if (saveItr->second == QUEST_FORCE_DELETE_SAVE_TYPE || !keepAbandoned)
        {
            stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHAR_QUESTSTATUS_REWARDED_BY_QUEST);
            stmt->setUInt32(0, GetGUID().GetCounter());
            stmt->setUInt32(1, saveItr->first);
            trans->Append(stmt);
        }
    }

    m_RewardedQuestsSave.clear();

    if (!isTransaction)
        CharacterDatabase.CommitTransaction(trans);
}

void Player::_SaveDailyQuestStatus(CharacterDatabaseTransaction trans)
{
    if (!m_DailyQuestChanged)
        return;

    m_DailyQuestChanged = false;

    // save last daily quest time for all quests: we need only mostly reset time for reset check anyway

    // we don't need transactions here.
    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHARACTER_QUESTSTATUS_DAILY);
    stmt->setUInt32(0, GetGUID().GetCounter());
    trans->Append(stmt);

    for (uint32 quest_daily_idx = 0; quest_daily_idx < PLAYER_MAX_DAILY_QUESTS; ++quest_daily_idx)
    {
        if (GetUInt32Value(PLAYER_FIELD_DAILY_QUESTS_1+quest_daily_idx))
        {
            stmt = CharacterDatabase.GetPreparedStatement(CHAR_INS_CHARACTER_QUESTSTATUS_DAILY);
            stmt->setUInt32(0, GetGUID().GetCounter());
            stmt->setUInt32(1, GetUInt32Value(PLAYER_FIELD_DAILY_QUESTS_1+quest_daily_idx));
            stmt->setUInt64(2, uint64(m_lastDailyQuestTime));
            trans->Append(stmt);
        }
    }

    if (!m_DFQuests.empty())
    {
        for (DFQuestsDoneList::iterator itr = m_DFQuests.begin(); itr != m_DFQuests.end(); ++itr)
        {
            stmt = CharacterDatabase.GetPreparedStatement(CHAR_INS_CHARACTER_QUESTSTATUS_DAILY);
            stmt->setUInt32(0, GetGUID().GetCounter());
            stmt->setUInt32(1, (*itr));
            stmt->setUInt64(2, uint64(m_lastDailyQuestTime));
            trans->Append(stmt);
        }
    }
}

void Player::_SaveWeeklyQuestStatus(CharacterDatabaseTransaction trans)
{
    if (!m_WeeklyQuestChanged || m_weeklyquests.empty())
        return;

    // we don't need transactions here.
    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHARACTER_QUESTSTATUS_WEEKLY);
    stmt->setUInt32(0, GetGUID().GetCounter());
    trans->Append(stmt);

    for (QuestSet::const_iterator iter = m_weeklyquests.begin(); iter != m_weeklyquests.end(); ++iter)
    {
        uint32 questId  = *iter;

        stmt = CharacterDatabase.GetPreparedStatement(CHAR_INS_CHARACTER_QUESTSTATUS_WEEKLY);
        stmt->setUInt32(0, GetGUID().GetCounter());
        stmt->setUInt32(1, questId);
        trans->Append(stmt);
    }

    m_WeeklyQuestChanged = false;
}

void Player::_SaveSeasonalQuestStatus(CharacterDatabaseTransaction trans)
{
    if (!m_SeasonalQuestChanged)
        return;

    // we don't need transactions here.
    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHARACTER_QUESTSTATUS_SEASONAL);
    stmt->setUInt32(0, GetGUID().GetCounter());
    trans->Append(stmt);

    m_SeasonalQuestChanged = false;

    if (m_seasonalquests.empty())
        return;

    for (SeasonalEventQuestMap::const_iterator iter = m_seasonalquests.begin(); iter != m_seasonalquests.end(); ++iter)
    {
        uint16 eventId = iter->first;

        for (SeasonalQuestSet::const_iterator itr = iter->second.begin(); itr != iter->second.end(); ++itr)
        {
            uint32 questId = *itr;

            stmt = CharacterDatabase.GetPreparedStatement(CHAR_INS_CHARACTER_QUESTSTATUS_SEASONAL);
            stmt->setUInt32(0, GetGUID().GetCounter());
            stmt->setUInt32(1, questId);
            stmt->setUInt32(2, eventId);
            trans->Append(stmt);
        }
    }
}

void Player::_SaveMonthlyQuestStatus(CharacterDatabaseTransaction trans)
{
    if (!m_MonthlyQuestChanged || m_monthlyquests.empty())
        return;

    // we don't need transactions here.
    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHARACTER_QUESTSTATUS_MONTHLY);
    stmt->setUInt32(0, GetGUID().GetCounter());
    trans->Append(stmt);

    for (QuestSet::const_iterator iter = m_monthlyquests.begin(); iter != m_monthlyquests.end(); ++iter)
    {
        uint32 questId = *iter;

        stmt = CharacterDatabase.GetPreparedStatement(CHAR_INS_CHARACTER_QUESTSTATUS_MONTHLY);
        stmt->setUInt32(0, GetGUID().GetCounter());
        stmt->setUInt32(1, questId);
        trans->Append(stmt);
    }

    m_MonthlyQuestChanged = false;
}

void Player::_SaveTalents(CharacterDatabaseTransaction trans)
{
    CharacterDatabasePreparedStatement* stmt = nullptr;

    for (uint8 i = 0; i < MAX_TALENT_SPECS; ++i)
    {
        for (PlayerTalentMap::iterator itr = m_talents[i]->begin(); itr != m_talents[i]->end();)
        {
            if (itr->second->state == PLAYERSPELL_REMOVED || itr->second->state == PLAYERSPELL_CHANGED)
            {
                stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHAR_TALENT_BY_SPELL_SPEC);
                stmt->setUInt32(0, GetGUID().GetCounter());
                stmt->setUInt32(1, itr->first);
                stmt->setUInt8(2, itr->second->spec);
                trans->Append(stmt);
            }

            if (itr->second->state == PLAYERSPELL_NEW || itr->second->state == PLAYERSPELL_CHANGED)
            {
                stmt = CharacterDatabase.GetPreparedStatement(CHAR_INS_CHAR_TALENT);
                stmt->setUInt32(0, GetGUID().GetCounter());
                stmt->setUInt32(1, itr->first);
                stmt->setUInt8(2, itr->second->spec);
                trans->Append(stmt);
            }

            if (itr->second->state == PLAYERSPELL_REMOVED)
            {
                delete itr->second;
                m_talents[i]->erase(itr++);
            }
            else
            {
                itr->second->state = PLAYERSPELL_UNCHANGED;
                ++itr;
            }
        }
    }
}

void Player::_SaveSpells(CharacterDatabaseTransaction trans)
{
    CharacterDatabasePreparedStatement* stmt;

    for (PlayerSpellMap::iterator itr = m_spells.begin(); itr != m_spells.end();)
    {
        if (itr->second.state == PLAYERSPELL_REMOVED || itr->second.state == PLAYERSPELL_CHANGED)
        {
            stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHAR_SPELL_BY_SPELL);
            stmt->setUInt32(0, itr->first);
            stmt->setUInt32(1, GetGUID().GetCounter());
            trans->Append(stmt);
        }

        // add only changed/new not dependent spells
        if (!itr->second.dependent && (itr->second.state == PLAYERSPELL_NEW || itr->second.state == PLAYERSPELL_CHANGED))
        {
            stmt = CharacterDatabase.GetPreparedStatement(CHAR_INS_CHAR_SPELL);
            stmt->setUInt32(0, GetGUID().GetCounter());
            stmt->setUInt32(1, itr->first);
            stmt->setBool(2, itr->second.active);
            stmt->setBool(3, itr->second.disabled);
            trans->Append(stmt);
        }

        if (itr->second.state == PLAYERSPELL_REMOVED)
        {
            itr = m_spells.erase(itr);
            continue;
        }

        if (itr->second.state != PLAYERSPELL_TEMPORARY)
            itr->second.state = PLAYERSPELL_UNCHANGED;

        ++itr;
    }
}

void Player::_SaveActions(CharacterDatabaseTransaction trans)
{
    CharacterDatabasePreparedStatement* stmt;

    for (ActionButtonList::iterator itr = m_actionButtons.begin(); itr != m_actionButtons.end();)
    {
        switch (itr->second.uState)
        {
            case ACTIONBUTTON_NEW:
                stmt = CharacterDatabase.GetPreparedStatement(CHAR_INS_CHAR_ACTION);
                stmt->setUInt32(0, GetGUID().GetCounter());
                stmt->setUInt8(1, m_activeSpec);
                stmt->setUInt8(2, itr->first);
                stmt->setUInt32(3, itr->second.GetAction());
                stmt->setUInt8(4, uint8(itr->second.GetType()));
                trans->Append(stmt);

                itr->second.uState = ACTIONBUTTON_UNCHANGED;
                ++itr;
                break;
            case ACTIONBUTTON_CHANGED:
                stmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_CHAR_ACTION);
                stmt->setUInt32(0, itr->second.GetAction());
                stmt->setUInt8(1, uint8(itr->second.GetType()));
                stmt->setUInt32(2,  GetGUID().GetCounter());
                stmt->setUInt8(3, itr->first);
                stmt->setUInt8(4, m_activeSpec);
                trans->Append(stmt);

                itr->second.uState = ACTIONBUTTON_UNCHANGED;
                ++itr;
                break;
            case ACTIONBUTTON_DELETED:
                stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHAR_ACTION_BY_BUTTON_SPEC);
                stmt->setUInt32(0, GetGUID().GetCounter());
                stmt->setUInt8(1, itr->first);
                stmt->setUInt8(2, m_activeSpec);
                trans->Append(stmt);

                m_actionButtons.erase(itr++);
                break;
            default:
                ++itr;
                break;
        }
    }
}

void Player::_SaveAuras(CharacterDatabaseTransaction trans)
{
    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHAR_AURA);
    stmt->setUInt32(0, GetGUID().GetCounter());
    trans->Append(stmt);

    for (AuraMap::const_iterator itr = m_ownedAuras.begin(); itr != m_ownedAuras.end(); ++itr)
    {
        if (!itr->second->CanBeSaved())
            continue;

        Aura* aura = itr->second;

        int32 damage[MAX_SPELL_EFFECTS];
        int32 baseDamage[MAX_SPELL_EFFECTS];
        uint8 effMask = 0;
        uint8 recalculateMask = 0;
        for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
        {
            if (AuraEffect const* effect = aura->GetEffect(i))
            {
                baseDamage[i] = effect->GetBaseAmount();
                damage[i] = effect->GetAmount();
                effMask |= 1 << i;
                if (effect->CanBeRecalculated())
                    recalculateMask |= 1 << i;
            }
            else
            {
                baseDamage[i] = 0;
                damage[i] = 0;
            }
        }

        uint8 index = 0;
        stmt = CharacterDatabase.GetPreparedStatement(CHAR_INS_AURA);
        stmt->setUInt32(index++, GetGUID().GetCounter());
        stmt->setUInt64(index++, itr->second->GetCasterGUID().GetRawValue());
        stmt->setUInt64(index++, itr->second->GetCastItemGUID().GetRawValue());
        stmt->setUInt32(index++, itr->second->GetId());
        stmt->setUInt8(index++, effMask);
        stmt->setUInt8(index++, recalculateMask);
        stmt->setUInt8(index++, itr->second->GetStackAmount());
        stmt->setInt32(index++, damage[0]);
        stmt->setInt32(index++, damage[1]);
        stmt->setInt32(index++, damage[2]);
        stmt->setInt32(index++, baseDamage[0]);
        stmt->setInt32(index++, baseDamage[1]);
        stmt->setInt32(index++, baseDamage[2]);
        stmt->setInt32(index++, itr->second->GetMaxDuration());
        stmt->setInt32(index++, itr->second->GetDuration());
        stmt->setUInt8(index++, itr->second->GetCharges());
        stmt->setFloat(index++, itr->second->GetCritChance());
        stmt->setBool (index++, itr->second->CanApplyResilience());
        trans->Append(stmt);
    }
}

void Player::_SaveSkills(CharacterDatabaseTransaction trans)
{
    CharacterDatabasePreparedStatement* stmt;
    // we don't need transactions here.
    for (SkillStatusMap::iterator itr = mSkillStatus.begin(); itr != mSkillStatus.end();)
    {
        if (itr->second.uState == SKILL_UNCHANGED)
        {
            ++itr;
            continue;
        }

        if (itr->second.uState == SKILL_DELETED)
        {
            stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHAR_SKILL_BY_SKILL);
            stmt->setUInt32(0, GetGUID().GetCounter());
            stmt->setUInt32(1, itr->first);
            trans->Append(stmt);

            mSkillStatus.erase(itr++);
            continue;
        }

        uint32 valueData = GetUInt32Value(PLAYER_SKILL_VALUE_INDEX(itr->second.pos));
        uint16 value = SKILL_VALUE(valueData);
        uint16 max = SKILL_MAX(valueData);

        switch (itr->second.uState)
        {
            case SKILL_NEW:
                stmt = CharacterDatabase.GetPreparedStatement(CHAR_INS_CHAR_SKILLS);
                stmt->setUInt32(0, GetGUID().GetCounter());
                stmt->setUInt16(1, uint16(itr->first));
                stmt->setUInt16(2, value);
                stmt->setUInt16(3, max);
                trans->Append(stmt);

                break;
            case SKILL_CHANGED:
                stmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_CHAR_SKILLS);
                stmt->setUInt16(0, value);
                stmt->setUInt16(1, max);
                stmt->setUInt32(2, GetGUID().GetCounter());
                stmt->setUInt16(3, uint16(itr->first));
                trans->Append(stmt);

                break;
            default:
                break;
        }
        itr->second.uState = SKILL_UNCHANGED;

        ++itr;
    }
}

void Player::_SaveEquipmentSets(CharacterDatabaseTransaction trans)
{
    for (EquipmentSetContainer::iterator itr = _equipmentSets.begin(); itr != _equipmentSets.end();)
    {
        EquipmentSetInfo& eqSet = itr->second;
        CharacterDatabasePreparedStatement* stmt;
        uint8 j = 0;
        switch (eqSet.State)
        {
            case EQUIPMENT_SET_UNCHANGED:
                ++itr;
                break;                                      // nothing do
            case EQUIPMENT_SET_CHANGED:
                stmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_EQUIP_SET);
                stmt->setString(j++, eqSet.Data.SetName);
                stmt->setString(j++, eqSet.Data.SetIcon);
                stmt->setUInt32(j++, eqSet.Data.IgnoreMask);
                for (uint8 i = 0; i < EQUIPMENT_SLOT_END; ++i)
                    stmt->setUInt32(j++, eqSet.Data.Pieces[i].GetCounter());
                stmt->setUInt32(j++, GetGUID().GetCounter());
                stmt->setUInt64(j++, eqSet.Data.Guid);
                stmt->setUInt32(j, eqSet.Data.SetID);
                trans->Append(stmt);
                eqSet.State = EQUIPMENT_SET_UNCHANGED;
                ++itr;
                break;
            case EQUIPMENT_SET_NEW:
                stmt = CharacterDatabase.GetPreparedStatement(CHAR_INS_EQUIP_SET);
                stmt->setUInt32(j++, GetGUID().GetCounter());
                stmt->setUInt64(j++, eqSet.Data.Guid);
                stmt->setUInt32(j++, eqSet.Data.SetID);
                stmt->setString(j++, eqSet.Data.SetName);
                stmt->setString(j++, eqSet.Data.SetIcon);
                stmt->setUInt32(j++, eqSet.Data.IgnoreMask);
                for (uint8 i = 0; i < EQUIPMENT_SLOT_END; ++i)
                    stmt->setUInt32(j++, eqSet.Data.Pieces[i].GetCounter());
                trans->Append(stmt);
                eqSet.State = EQUIPMENT_SET_UNCHANGED;
                ++itr;
                break;
            case EQUIPMENT_SET_DELETED:
                stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_EQUIP_SET);
                stmt->setUInt64(0, eqSet.Data.Guid);
                trans->Append(stmt);
                itr = _equipmentSets.erase(itr);
                break;
        }
    }
}

void Player::_SaveGlyphs(CharacterDatabaseTransaction trans) const
{
    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHAR_GLYPHS);
    stmt->setUInt32(0, GetGUID().GetCounter());
    trans->Append(stmt);

    for (uint8 spec = 0; spec < m_specsCount; ++spec)
    {
        uint8 index = 0;

        stmt = CharacterDatabase.GetPreparedStatement(CHAR_INS_CHAR_GLYPHS);
        stmt->setUInt32(index++, GetGUID().GetCounter());

        stmt->setUInt8(index++, spec);

        for (uint8 i = 0; i < MAX_GLYPH_SLOT_INDEX; ++i)
            stmt->setUInt16(index++, uint16(m_Glyphs[spec][i]));

        trans->Append(stmt);
    }
}

// save player stats -- only for external usage
// real stats will be recalculated on player login
void Player::_SaveStats(CharacterDatabaseTransaction trans) const
{
    // check if stat saving is enabled and if char level is high enough
    if (!sWorld->getIntConfig(CONFIG_MIN_LEVEL_STAT_SAVE) || GetLevel() < sWorld->getIntConfig(CONFIG_MIN_LEVEL_STAT_SAVE))
        return;

    CharacterDatabasePreparedStatement* stmt;

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHAR_STATS);
    stmt->setUInt32(0, GetGUID().GetCounter());
    trans->Append(stmt);

    uint8 index = 0;

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_INS_CHAR_STATS);
    stmt->setUInt32(index++, GetGUID().GetCounter());
    stmt->setUInt32(index++, GetMaxHealth());

    for (uint8 i = 0; i < MAX_POWERS; ++i)
        stmt->setUInt32(index++, GetMaxPower(Powers(i)));

    for (uint8 i = 0; i < MAX_STATS; ++i)
        stmt->setUInt32(index++, GetStat(Stats(i)));

    for (int i = 0; i < MAX_SPELL_SCHOOL; ++i)
        stmt->setUInt32(index++, GetResistance(SpellSchools(i)));

    stmt->setFloat(index++, GetFloatValue(PLAYER_BLOCK_PERCENTAGE));
    stmt->setFloat(index++, GetFloatValue(PLAYER_DODGE_PERCENTAGE));
    stmt->setFloat(index++, GetFloatValue(PLAYER_PARRY_PERCENTAGE));
    stmt->setFloat(index++, GetFloatValue(PLAYER_CRIT_PERCENTAGE));
    stmt->setFloat(index++, GetFloatValue(PLAYER_RANGED_CRIT_PERCENTAGE));

    // Store the max spell crit percentage out of all the possible schools
    float spellCrit = 0.0f;
    for (int i = 0; i < MAX_SPELL_SCHOOL; ++i)
        spellCrit = std::max(spellCrit, GetFloatValue(PLAYER_SPELL_CRIT_PERCENTAGE1 + i));
    stmt->setFloat(index++, spellCrit);

    stmt->setUInt32(index++, GetUInt32Value(UNIT_FIELD_ATTACK_POWER));
    stmt->setUInt32(index++, GetUInt32Value(UNIT_FIELD_RANGED_ATTACK_POWER));
    stmt->setUInt32(index++, GetBaseSpellPowerBonus());
    stmt->setUInt32(index++, GetUInt32Value(PLAYER_FIELD_COMBAT_RATING_1 + AsUnderlyingType(CR_CRIT_TAKEN_SPELL)));

    trans->Append(stmt);
}

// fast save function for item/money cheating preventing - save only inventory and money state
void Player::SaveInventoryAndGoldToDB(CharacterDatabaseTransaction trans)
{
    _SaveInventory(trans);
    SaveGoldToDB(trans);
}

void Player::SaveGoldToDB(CharacterDatabaseTransaction trans) const
{
    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_CHAR_MONEY);
    stmt->setUInt32(0, GetMoney());
    stmt->setUInt32(1, GetGUID().GetCounter());
    trans->Append(stmt);
}

// Called from cs_anticheat and spell effects
void Player::SetHomebind(WorldLocation const& loc, uint32 areaId)
{
    loc.GetPosition(m_homebindX, m_homebindY, m_homebindZ);
    m_homebindMapId = loc.GetMapId();
    m_homebindAreaId = areaId;

    // update sql homebind
    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_PLAYER_HOMEBIND);
    stmt->setUInt16(0, m_homebindMapId);
    stmt->setUInt16(1, m_homebindAreaId);
    stmt->setFloat (2, m_homebindX);
    stmt->setFloat (3, m_homebindY);
    stmt->setFloat (4, m_homebindZ);
    stmt->setUInt32(5, GetGUID().GetCounter());
    CharacterDatabase.Execute(stmt);
}

// Called from Guild LoadFromDB
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

/**
 * Deletes a character from the database
 *
 * The way characters will be deleted is decided based on the config option.
 *
 * @see Player::DeleteOldCharacters
 *
 * @param playerguid       the low-GUID from the player which should be deleted
 * @param accountId        the account id from the player
 * @param updateRealmChars when this flag is set, the amount of characters on that realm will be updated in the realmlist
 * @param deleteFinally    if this flag is set, the config option will be ignored and the character will be permanently removed from the database
 */
void Player::DeleteFromDB(ObjectGuid playerguid, uint32 accountId, bool updateRealmChars, bool deleteFinally)
{
    // Avoid realm-update for non-existing account
    if (accountId == 0)
        updateRealmChars = false;

    // Convert guid to low GUID for CharacterNameData, but also other methods on success
    ObjectGuid::LowType guid = playerguid.GetCounter();
    uint32 charDelete_method = sWorld->getIntConfig(CONFIG_CHARDELETE_METHOD);
    CharacterCacheEntry const* characterInfo = sCharacterCache->GetCharacterCacheByGuid(playerguid);

    if (deleteFinally)
        charDelete_method = CHAR_DELETE_REMOVE;
    else if (characterInfo)    // To avoid a query, we select loaded data. If it doesn't exist, return.
    {
        // Define the required variables
        uint32 charDelete_minLvl = sWorld->getIntConfig(characterInfo->Class != CLASS_DEATH_KNIGHT ? CONFIG_CHARDELETE_MIN_LEVEL : CONFIG_CHARDELETE_DEATH_KNIGHT_MIN_LEVEL);

        // if we want to finalize the character removal or the character does not meet the level requirement of either heroic or non-heroic settings,
        // we set it to mode CHAR_DELETE_REMOVE
        if (characterInfo->Level < charDelete_minLvl)
            charDelete_method = CHAR_DELETE_REMOVE;
    }

    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
    if (ObjectGuid::LowType guildId = sCharacterCache->GetCharacterGuildIdByGuid(playerguid))
        if (Guild* guild = sGuildMgr->GetGuildById(guildId))
            guild->DeleteMember(trans, playerguid, false, false);

    // close player ticket if any
    GmTicket* ticket = sTicketMgr->GetTicketByPlayer(playerguid);
    if (ticket)
        sTicketMgr->CloseTicket(ticket->GetId(), playerguid);

    // remove from arena teams
    LeaveAllArenaTeams(playerguid);

    // the player was uninvited already on logout so just remove from group
    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_GROUP_MEMBER);
    stmt->setUInt32(0, guid);
    PreparedQueryResult resultGroup = CharacterDatabase.Query(stmt);

    if (resultGroup)
        if (Group* group = sGroupMgr->GetGroupByDbStoreId((*resultGroup)[0].GetUInt32()))
            RemoveFromGroup(group, playerguid);

    // Remove signs from petitions (also remove petitions if owner);
    RemovePetitionsAndSigns(playerguid, CHARTER_TYPE_ANY);

    switch (charDelete_method)
    {
        // Completely remove from the database
        case CHAR_DELETE_REMOVE:
        {
            stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHAR_COD_ITEM_MAIL);
            stmt->setUInt32(0, guid);
            PreparedQueryResult resultMail = CharacterDatabase.Query(stmt);

            if (resultMail)
            {
                std::unordered_map<uint32, std::vector<Item*>> itemsByMail;

                stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_MAILITEMS);
                stmt->setUInt32(0, guid);
                PreparedQueryResult resultItems = CharacterDatabase.Query(stmt);

                if (resultItems)
                {
                    do
                    {
                        Field* fields = resultItems->Fetch();
                        uint32 mailId = fields[14].GetUInt32();
                        if (Item* mailItem = _LoadMailedItem(playerguid, nullptr, mailId, nullptr, fields))
                            itemsByMail[mailId].push_back(mailItem);

                    } while (resultItems->NextRow());
                }

                do
                {
                    Field* mailFields = resultMail->Fetch();

                    uint32 mail_id       = mailFields[0].GetUInt32();
                    uint8 mailType       = mailFields[1].GetUInt8();
                    uint16 mailTemplateId= mailFields[2].GetUInt16();
                    uint32 sender        = mailFields[3].GetUInt32();
                    std::string subject  = mailFields[4].GetString();
                    std::string body     = mailFields[5].GetString();
                    uint32 money         = mailFields[6].GetUInt32();
                    bool has_items       = mailFields[7].GetBool();

                    // We can return mail now
                    // So firstly delete the old one
                    stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_MAIL_BY_ID);
                    stmt->setUInt32(0, mail_id);
                    trans->Append(stmt);

                    // Mail is not from player
                    if (mailType != MAIL_NORMAL)
                    {
                        if (has_items)
                        {
                            stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_MAIL_ITEM_BY_ID);
                            stmt->setUInt32(0, mail_id);
                            trans->Append(stmt);
                        }
                        continue;
                    }

                    MailDraft draft(subject, body);
                    if (mailTemplateId)
                        draft = MailDraft(mailTemplateId, false);    // items are already included

                    auto itemsItr = itemsByMail.find(mail_id);
                    if (itemsItr != itemsByMail.end())
                    {
                        for (Item* item : itemsItr->second)
                            draft.AddItem(item);

                        // MailDraft will take care of freeing memory
                        itemsByMail.erase(itemsItr);
                    }

                    stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_MAIL_ITEM_BY_ID);
                    stmt->setUInt32(0, mail_id);
                    trans->Append(stmt);

                    uint32 pl_account = sCharacterCache->GetCharacterAccountIdByGuid(playerguid);

                    draft.AddMoney(money).SendReturnToSender(pl_account, guid, sender, trans);
                }
                while (resultMail->NextRow());
            }

            // Unsummon and delete for pets in world is not required: player deleted from CLI or character list with not loaded pet.
            // NOW we can finally clear other DB data related to character
            stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHAR_PET_IDS);
            stmt->setUInt32(0, guid);
            PreparedQueryResult resultPets = CharacterDatabase.Query(stmt);

            if (resultPets)
            {
                do
                {
                    ObjectGuid::LowType petguidlow = (*resultPets)[0].GetUInt32();
                    Pet::DeleteFromDB(petguidlow);
                } while (resultPets->NextRow());
            }

            // Delete char from social list of online chars
            stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHAR_SOCIAL);
            stmt->setUInt32(0, guid);

            if (PreparedQueryResult resultFriends = CharacterDatabase.Query(stmt))
            {
                do
                {
                    if (Player* playerFriend = ObjectAccessor::FindPlayer(ObjectGuid(HighGuid::Player, 0, (*resultFriends)[0].GetUInt32())))
                    {
                        playerFriend->GetSocial()->RemoveFromSocialList(playerguid, SOCIAL_FLAG_ALL);
                        sSocialMgr->SendFriendStatus(playerFriend, FRIEND_REMOVED, playerguid);
                    }
                } while (resultFriends->NextRow());
            }

            stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHARACTER);
            stmt->setUInt32(0, guid);
            trans->Append(stmt);
// @tswow-begin (Using Rochet2/Transmog)
#ifdef PRESETS
            trans->PAppend("DELETE FROM `custom_transmogrification_sets` WHERE `Owner` = {}", guid);
#endif
// @tswow-end

            stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_PLAYER_ACCOUNT_DATA);
            stmt->setUInt32(0, guid);
            trans->Append(stmt);

            stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHAR_DECLINED_NAME);
            stmt->setUInt32(0, guid);
            trans->Append(stmt);

            stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHAR_ACTION);
            stmt->setUInt32(0, guid);
            trans->Append(stmt);

            stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHARACTER_ARENA_STATS);
            stmt->setUInt32(0, guid);
            trans->Append(stmt);

            stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHAR_AURA);
            stmt->setUInt32(0, guid);
            trans->Append(stmt);

            stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_PLAYER_BGDATA);
            stmt->setUInt32(0, guid);
            trans->Append(stmt);

            stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_BATTLEGROUND_RANDOM);
            stmt->setUInt32(0, guid);
            trans->Append(stmt);

            stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHAR_GIFT);
            stmt->setUInt32(0, guid);
            trans->Append(stmt);

            stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_PLAYER_HOMEBIND);
            stmt->setUInt32(0, guid);
            trans->Append(stmt);

            stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHAR_INSTANCE);
            stmt->setUInt32(0, guid);
            trans->Append(stmt);

            stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHAR_INVENTORY);
            stmt->setUInt32(0, guid);
            trans->Append(stmt);

            stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHAR_QUESTSTATUS);
            stmt->setUInt32(0, guid);
            trans->Append(stmt);

            stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHAR_QUESTSTATUS_REWARDED);
            stmt->setUInt32(0, guid);
            trans->Append(stmt);

            stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHAR_REPUTATION);
            stmt->setUInt32(0, guid);
            trans->Append(stmt);

            stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHAR_SPELL);
            stmt->setUInt32(0, guid);
            trans->Append(stmt);

            stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHAR_SPELL_COOLDOWNS);
            stmt->setUInt32(0, guid);
            trans->Append(stmt);

            if (sWorld->getBoolConfig(CONFIG_DELETE_CHARACTER_TICKET_TRACE))
            {
                stmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_PLAYER_GM_TICKETS_ON_CHAR_DELETION);
                stmt->setUInt32(0, guid);
                trans->Append(stmt);
            }
            else
            {
                stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_PLAYER_GM_TICKETS);
                stmt->setUInt32(0, guid);
                trans->Append(stmt);
            }

            stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_ITEM_INSTANCE_BY_OWNER);
            stmt->setUInt32(0, guid);
            trans->Append(stmt);

            stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHAR_SOCIAL_BY_FRIEND);
            stmt->setUInt32(0, guid);
            trans->Append(stmt);

            stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHAR_SOCIAL_BY_GUID);
            stmt->setUInt32(0, guid);
            trans->Append(stmt);

            stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_MAIL);
            stmt->setUInt32(0, guid);
            trans->Append(stmt);

            stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_MAIL_ITEMS);
            stmt->setUInt32(0, guid);
            trans->Append(stmt);

            stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHAR_PET_BY_OWNER);
            stmt->setUInt32(0, guid);
            trans->Append(stmt);

            stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHAR_PET_DECLINEDNAME_BY_OWNER);
            stmt->setUInt32(0, guid);
            trans->Append(stmt);

            stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHAR_ACHIEVEMENTS);
            stmt->setUInt32(0, guid);
            trans->Append(stmt);

            stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHAR_ACHIEVEMENT_PROGRESS);
            stmt->setUInt32(0, guid);
            trans->Append(stmt);

            stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHAR_EQUIPMENTSETS);
            stmt->setUInt32(0, guid);
            trans->Append(stmt);

            stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_GUILD_EVENTLOG_BY_PLAYER);
            stmt->setUInt32(0, guid);
            stmt->setUInt32(1, guid);
            trans->Append(stmt);

            stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_GUILD_BANK_EVENTLOG_BY_PLAYER);
            stmt->setUInt32(0, guid);
            trans->Append(stmt);

            stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHAR_GLYPHS);
            stmt->setUInt32(0, guid);
            trans->Append(stmt);

            stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHARACTER_QUESTSTATUS_DAILY);
            stmt->setUInt32(0, guid);
            trans->Append(stmt);

            stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHARACTER_QUESTSTATUS_WEEKLY);
            stmt->setUInt32(0, guid);
            trans->Append(stmt);

            stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHARACTER_QUESTSTATUS_MONTHLY);
            stmt->setUInt32(0, guid);
            trans->Append(stmt);

            stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHARACTER_QUESTSTATUS_SEASONAL);
            stmt->setUInt32(0, guid);
            trans->Append(stmt);

            stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHAR_TALENT);
            stmt->setUInt32(0, guid);
            trans->Append(stmt);

            stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHAR_SKILLS);
            stmt->setUInt32(0, guid);
            trans->Append(stmt);

            stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHAR_STATS);
            stmt->setUInt32(0, guid);
            trans->Append(stmt);

            stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHAR_FISHINGSTEPS);
            stmt->setUInt32(0, guid);
            trans->Append(stmt);

            Corpse::DeleteFromDB(playerguid, trans);
            break;
        }
        // The character gets unlinked from the account, the name gets freed up and appears as deleted ingame
        case CHAR_DELETE_UNLINK:
        {
            stmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_DELETE_INFO);
            stmt->setUInt32(0, guid);
            trans->Append(stmt);
            break;
        }
        default:
            TC_LOG_ERROR("entities.player.cheat", "Player::DeleteFromDB: Tried to delete player ({}) with unsupported delete method ({}).",
                playerguid.ToString(), charDelete_method);

            if (trans->GetSize() > 0)
                CharacterDatabase.CommitTransaction(trans);
            return;
    }

    // @tswow-begin
    stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_JSON_DATA);
    stmt->setUInt32(0, DBJsonEntityType::PLAYER);
    stmt->setUInt32(1, guid);
    trans->Append(stmt);
    // @tswow-end

    CharacterDatabase.CommitTransaction(trans);

    if (updateRealmChars)
        sWorld->UpdateRealmCharCount(accountId);

    sCharacterCache->DeleteCharacterCacheEntry(playerguid);
}

/**
 * Characters which were kept back in the database after being deleted and are now too old (see config option "CharDelete.KeepDays"), will be completely deleted.
 *
 * @see Player::DeleteFromDB
 */
void Player::DeleteOldCharacters()
{
    uint32 keepDays = sWorld->getIntConfig(CONFIG_CHARDELETE_KEEP_DAYS);
    if (!keepDays)
        return;

    Player::DeleteOldCharacters(keepDays);
}

/**
 * Characters which were kept back in the database after being deleted and are older than the specified amount of days, will be completely deleted.
 *
 * @see Player::DeleteFromDB
 *
 * @param keepDays overwrite the config option by another amount of days
 */
void Player::DeleteOldCharacters(uint32 keepDays)
{
    TC_LOG_INFO("entities.player", "Player::DeleteOldCharacters: Deleting all characters which have been deleted {} days before...", keepDays);

    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHAR_OLD_CHARS);
    stmt->setUInt32(0, static_cast<uint32>(GameTime::GetGameTime() - static_cast<time_t>(keepDays) * DAY));
    PreparedQueryResult result = CharacterDatabase.Query(stmt);

    if (result)
    {
         TC_LOG_DEBUG("entities.player", "Player::DeleteOldCharacters: Found {} character(s) to delete", result->GetRowCount());
         do
         {
            Field* fields = result->Fetch();
            Player::DeleteFromDB(ObjectGuid(HighGuid::Player, fields[0].GetUInt32()), fields[1].GetUInt32(), true, true);
         }
         while (result->NextRow());
    }
}

// Builds character info for CMSG_CHAR_ENUM request (Display the character at login screen)
bool Player::BuildEnumData(PreparedQueryResult result, WorldPacket* data)
{
    //        0                1                2                3                 4                  5                6                7
    // SELECT characters.guid, characters.name, characters.race, characters.class, characters.gender, characters.skin, characters.face, characters.hairStyle,
    // 8                     9                       10                11               12              13                     14                     15
    // characters.hairColor, characters.facialStyle, characters.level, characters.zone, characters.map, characters.position_x, characters.position_y, characters.position_z,
    // 16                    17                      18                   19                   20                     21                   22
    // guild_member.guildid, characters.playerFlags, characters.at_login, character_pet.entry, character_pet.modelid, character_pet.level, characters.equipmentCache,
    // 23                     24
    // character_banned.guid, character_declinedname.genitive

    Field* fields = result->Fetch();

    ObjectGuid::LowType guid = fields[0].GetUInt32();
    uint8 plrRace = fields[2].GetUInt8();
    uint8 plrClass = fields[3].GetUInt8();
    uint8 gender = fields[4].GetUInt8();

    PlayerInfo const* info = sObjectMgr->GetPlayerInfo(plrRace, plrClass);
    if (!info)
    {
        TC_LOG_ERROR("entities.player.loading", "Player {} has incorrect race/class pair. Don't build enum.", guid);
        return false;
    }
    else if (!IsValidGender(gender))
    {
        TC_LOG_ERROR("entities.player.loading", "Player ({}) has incorrect gender ({}), don't build enum.", guid, gender);
        return false;
    }

    *data << ObjectGuid(HighGuid::Player, guid);
    *data << fields[1].GetString();                         // name
    *data << uint8(plrRace);                                // race
    *data << uint8(plrClass);                               // class
    *data << uint8(gender);                                 // gender

    uint8 skin = fields[5].GetUInt8();
    uint8 face = fields[6].GetUInt8();
    uint8 hairStyle = fields[7].GetUInt8();
    uint8 hairColor = fields[8].GetUInt8();
    uint8 facialStyle = fields[9].GetUInt8();

    uint16 atLoginFlags = fields[18].GetUInt16();

    if (!ValidateAppearance(uint8(plrRace), uint8(plrClass), gender, hairStyle, hairColor, face, facialStyle, skin))
    {
        TC_LOG_ERROR("entities.player.loading", "Player {} has wrong Appearance values (Hair/Skin/Color), forcing recustomize", guid);

        // Make sure customization always works properly - send all zeroes instead
        skin = 0, face = 0, hairStyle = 0, hairColor = 0, facialStyle = 0;

        if (!(atLoginFlags & AT_LOGIN_CUSTOMIZE))
        {
            CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_ADD_AT_LOGIN_FLAG);
            stmt->setUInt16(0, uint16(AT_LOGIN_CUSTOMIZE));
            stmt->setUInt32(1, guid);
            CharacterDatabase.Execute(stmt);
            atLoginFlags |= AT_LOGIN_CUSTOMIZE;
        }
    }

    *data << uint8(skin);
    *data << uint8(face);
    *data << uint8(hairStyle);
    *data << uint8(hairColor);
    *data << uint8(facialStyle);

    *data << uint8(fields[10].GetUInt8());                   // level
    *data << uint32(fields[11].GetUInt16());                 // zone
    *data << uint32(fields[12].GetUInt16());                 // map

    *data << fields[13].GetFloat();                         // x
    *data << fields[14].GetFloat();                         // y
    *data << fields[15].GetFloat();                         // z

    *data << uint32(fields[16].GetUInt32());                // guild id

    uint32 charFlags = 0;
    uint32 playerFlags = fields[17].GetUInt32();
    if (atLoginFlags & AT_LOGIN_RESURRECT)
        playerFlags &= ~PLAYER_FLAGS_GHOST;
    if (playerFlags & PLAYER_FLAGS_HIDE_HELM)
        charFlags |= CHARACTER_FLAG_HIDE_HELM;
    if (playerFlags & PLAYER_FLAGS_HIDE_CLOAK)
        charFlags |= CHARACTER_FLAG_HIDE_CLOAK;
    if (playerFlags & PLAYER_FLAGS_GHOST)
        charFlags |= CHARACTER_FLAG_GHOST;
    if (atLoginFlags & AT_LOGIN_RENAME)
        charFlags |= CHARACTER_FLAG_RENAME;
    if (fields[23].GetUInt32())
        charFlags |= CHARACTER_FLAG_LOCKED_BY_BILLING;
    if (sWorld->getBoolConfig(CONFIG_DECLINED_NAMES_USED))
    {
        if (!fields[24].GetString().empty())
            charFlags |= CHARACTER_FLAG_DECLINED;
    }
    else
        charFlags |= CHARACTER_FLAG_DECLINED;

    *data << uint32(charFlags);                             // character flags

    // character customize flags
    if (atLoginFlags & AT_LOGIN_CUSTOMIZE)
        *data << uint32(CHAR_CUSTOMIZE_FLAG_CUSTOMIZE);
    else if (atLoginFlags & AT_LOGIN_CHANGE_FACTION)
        *data << uint32(CHAR_CUSTOMIZE_FLAG_FACTION);
    else if (atLoginFlags & AT_LOGIN_CHANGE_RACE)
        *data << uint32(CHAR_CUSTOMIZE_FLAG_RACE);
    else
        *data << uint32(CHAR_CUSTOMIZE_FLAG_NONE);

    // First login
    *data << uint8(atLoginFlags & AT_LOGIN_FIRST ? 1 : 0);

    // Pets info
    uint32 petDisplayId = 0;
    uint32 petLevel = 0;
    CreatureFamily petFamily = CREATURE_FAMILY_NONE;

    // show pet at selection character in character list only for non-ghost character
    if (result && !(playerFlags & PLAYER_FLAGS_GHOST) && (plrClass == CLASS_WARLOCK || plrClass == CLASS_HUNTER || plrClass == CLASS_DEATH_KNIGHT))
    {
        uint32 entry = fields[19].GetUInt32();
        CreatureTemplate const* creatureInfo = sObjectMgr->GetCreatureTemplate(entry);
        if (creatureInfo)
        {
            petDisplayId = fields[20].GetUInt32();
            petLevel = fields[21].GetUInt16();
            petFamily = creatureInfo->family;
        }
    }

    *data << uint32(petDisplayId);
    *data << uint32(petLevel);
    *data << uint32(petFamily);

    std::vector<std::string_view> equipment = Trinity::Tokenize(fields[22].GetStringView(), ' ', false);
    for (uint8 slot = 0; slot < INVENTORY_SLOT_BAG_END; ++slot)
    {
        uint32 const visualBase = slot * 2;
        Optional<uint32> itemId;
        if (visualBase < equipment.size())
            itemId = Trinity::StringTo<uint32>(equipment[visualBase]);

        ItemTemplate const* proto = nullptr;
        if (itemId)
            proto = sObjectMgr->GetItemTemplate(*itemId);

        if (!proto)
        {
            if (!itemId || *itemId)
            {
                TC_LOG_WARN("entities.player.loading", "Player {} has invalid equipment '{}' in `equipmentcache` at index {}. Skipped.",
                    guid, (visualBase < equipment.size()) ? std::string(equipment[visualBase]) : "<none>", visualBase);
            }

            *data << uint32(0);
            *data << uint8(0);
            *data << uint32(0);

            continue;
        }

        SpellItemEnchantmentEntry const* enchant = nullptr;

        Optional<uint32> enchants;
        if ((visualBase+1) < equipment.size())
            enchants = Trinity::StringTo<uint32>(equipment[visualBase + 1]);
        if (!enchants)
        {
            TC_LOG_WARN("entities.player.loading", "Player {} has invalid enchantment info '{}' in `equipmentcache` at index {}. Skipped.",
                guid, ((visualBase+1) < equipment.size()) ? std::string(equipment[visualBase + 1]) : "<none>", visualBase + 1);
            enchants = 0;
        }
        for (uint8 enchantSlot = PERM_ENCHANTMENT_SLOT; enchantSlot <= TEMP_ENCHANTMENT_SLOT; ++enchantSlot)
        {
            // values stored in 2 uint16
            uint32 enchantId = 0x0000FFFF & ((*enchants) >> enchantSlot * 16);
            if (!enchantId)
                continue;

            enchant = sSpellItemEnchantmentStore.LookupEntry(enchantId);
            if (enchant)
                break;
        }

        *data << uint32(proto->DisplayInfoID);
        *data << uint8(proto->InventoryType);
        *data << uint32(enchant ? enchant->ItemVisual : 0);
    }

    return true;
}

// Called from cs_misc appear
bool Player::LoadPositionFromDB(uint32& mapid, float& x, float& y, float& z, float& o, bool& in_flight, ObjectGuid guid)
{
    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHAR_POSITION);
    stmt->setUInt32(0, guid.GetCounter());
    PreparedQueryResult result = CharacterDatabase.Query(stmt);

    if (!result)
        return false;

    Field* fields = result->Fetch();

    x = fields[0].GetFloat();
    y = fields[1].GetFloat();
    z = fields[2].GetFloat();
    o = fields[3].GetFloat();
    mapid = fields[4].GetUInt16();
    in_flight = !fields[5].GetString().empty();

    return true;
}

// Called from cs_misc summon, tele, faction or race change
void Player::SavePositionInDB(WorldLocation const& loc, uint16 zoneId, ObjectGuid guid, CharacterDatabaseTransaction trans)
{
    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_CHARACTER_POSITION);

    stmt->setFloat(0, loc.GetPositionX());
    stmt->setFloat(1, loc.GetPositionY());
    stmt->setFloat(2, loc.GetPositionZ());
    stmt->setFloat(3, loc.GetOrientation());
    stmt->setUInt16(4, uint16(loc.GetMapId()));
    stmt->setUInt16(5, zoneId);
    stmt->setUInt32(6, guid.GetCounter());

    CharacterDatabase.ExecuteOrAppend(trans, stmt);
}

// Called from char customize, faction or race change
void Player::Customize(CharacterCustomizeInfo const* customizeInfo, CharacterDatabaseTransaction trans)
{
    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_GENDER_AND_APPEARANCE);

    stmt->setUInt8(0, customizeInfo->Gender);
    stmt->setUInt8(1, customizeInfo->Skin);
    stmt->setUInt8(2, customizeInfo->Face);
    stmt->setUInt8(3, customizeInfo->HairStyle);
    stmt->setUInt8(4, customizeInfo->HairColor);
    stmt->setUInt8(5, customizeInfo->FacialHair);
    stmt->setUInt32(6, customizeInfo->Guid.GetCounter());

    CharacterDatabase.ExecuteOrAppend(trans, stmt);
}