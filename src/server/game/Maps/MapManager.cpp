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

#include "MapManager.h"
#include "DisableMgr.h"
#include "InstanceSaveMgr.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "ObjectAccessor.h"
#include "Transport.h"
#include "GridDefines.h"
#include "MapInstanced.h"
#include "MapPartitioned.h"
#include "InstanceScript.h"
#include "Config.h"
#include "World.h"
#include "Corpse.h"
#include "ObjectMgr.h"
#include "WorldPacket.h"
#include "Group.h"
#include "Player.h"
#include "WorldSession.h"
#include "Opcodes.h"
#include "TSProfile.h"
#include "ScriptMgr.h"
#include "VMapFactory.h"
#include "VMapManager2.h"
#include "MMapFactory.h"
#include <numeric>

MapManager::MapManager()
    : _nextInstanceId(0), _scheduledScripts(0)
{
    i_timer.SetInterval(sWorld->getIntConfig(CONFIG_INTERVAL_MAPUPDATE));
}

MapManager::~MapManager() { }

void MapManager::Initialize()
{
    int num_threads(sWorld->getIntConfig(CONFIG_NUMTHREADS));
    // Start mtmaps if needed.
    if (num_threads > 0)
        m_updater.activate(num_threads);
}

void MapManager::InitializeVisibilityDistanceInfo()
{
    for (MapMapType::iterator iter = i_maps.begin(); iter != i_maps.end(); ++iter)
        (*iter).second->InitVisibilityDistance();
}

MapManager* MapManager::instance()
{
    static MapManager instance;
    return &instance;
}

Map* MapManager::CreateBaseMap(uint32 id)
{
    ZoneScopedNC("Map* MapManager::CreateBaseMap", WORLD_UPDATE_COLOR)

    // BaseMaps are 'maps' that manage other maps.
    // MapInstanced manages its instances, and MapPartitioned manages its partitions.
    Map* map = FindBaseMap(id);

    if (map == nullptr)
    {
        std::lock_guard<std::mutex> lock(_mapsLock);

        MapEntry const* entry = sMapStore.LookupEntry(id);
        ASSERT(entry);

        if (entry->Instanceable())
            map = new MapInstanced(id);
        else
            map = new MapPartitioned(id);

        Trinity::unique_trackable_ptr<Map>& ptr = i_maps[id];
        ptr.reset(map);
        map->SetWeakPtr(ptr);

        sScriptMgr->OnCreateMap(map);
    }

    ASSERT(map);
    return map;
}

Map* MapManager::FindPartitionMap(uint32 mapId, float x, float y) const
{
    Map* map = FindBaseMap(mapId);
    if (!map || !map->IsWorldMap())
        return nullptr;

    MapPartitioned* mapPartitioned = map->ToMapPartitioned();
    if (!mapPartitioned)
        return nullptr;

    uint32 partitionId = mapPartitioned->CalculatePartitionId(x, y);
    return mapPartitioned->FindPartition(partitionId);
}

// Players are the primary instigator of map creation
Map* MapManager::CreateMap(uint32 id, Player* player, uint32 loginInstanceId)
{
    ZoneScopedNC("Map* MapManager::CreateMap", WORLD_UPDATE_COLOR)

    Map* map = CreateBaseMap(id);
    if (!map)
        return nullptr;

    if (map->Instanceable())
    {
        MapInstanced* mapInstanced = map->ToMapInstanced();
        if (!mapInstanced)
            return nullptr;

        // Additional Logic to check for existing instance
        return mapInstanced->CreateInstanceForPlayer(id, player, loginInstanceId);
    }
    else
    {
        MapPartitioned* mapPartitioned = map->ToMapPartitioned();
        if (!mapPartitioned)
            return nullptr;

        uint32 partitionId = mapPartitioned->CalculatePartitionId(player->GetPositionX(), player->GetPositionY());

        Map* partition = mapPartitioned->FindPartition(partitionId);
        if (partition)
            return partition;

        return mapPartitioned->CreatePartition(id, partitionId);
    }

    return nullptr;
}

Map* MapManager::FindMap(uint32 mapid, uint32 instanceId, float x, float y) const
{
    Map* map = FindBaseMap(mapid);
    if (!map)
        return nullptr;

    if (map->Instanceable())
    {
        MapInstanced* mapInstanced = map->ToMapInstanced();
        if (!mapInstanced)
            return nullptr;

        return mapInstanced->FindInstanceMap(instanceId);
    }
    else if (map->IsWorldMap())
    {
        MapPartitioned* mapPartitioned = map->ToMapPartitioned();
        if (!mapPartitioned)
            return nullptr;

        uint32 partitionId = mapPartitioned->CalculatePartitionId(x, y);

        return mapPartitioned->FindPartition(partitionId);
    }

    return nullptr;
}

Map::EnterState MapManager::PlayerCannotEnter(uint32 mapid, Player* player, bool loginCheck)
{
    MapEntry const* entry = sMapStore.LookupEntry(mapid);
    if (!entry)
        return Map::CANNOT_ENTER_NO_ENTRY;

    if (!entry->IsDungeon())
        return Map::CAN_ENTER;

    InstanceTemplate const* instance = sObjectMgr->GetInstanceTemplate(mapid);
    if (!instance)
        return Map::CANNOT_ENTER_UNINSTANCED_DUNGEON;

    Difficulty targetDifficulty, requestedDifficulty;
    targetDifficulty = requestedDifficulty = player->GetDifficulty(entry->IsRaid());
    // Get the highest available difficulty if current setting is higher than the instance allows
    MapDifficulty const* mapDiff = GetDownscaledMapDifficultyData(entry->ID, targetDifficulty);
    if (!mapDiff)
        return Map::CANNOT_ENTER_DIFFICULTY_UNAVAILABLE;

    //Bypass checks for GMs
    if (player->IsGameMaster())
        return Map::CAN_ENTER;

    char const* mapName = entry->MapName[player->GetSession()->GetSessionDbcLocale()];

    Group* group = player->GetGroup();
    if (entry->IsRaid()) // can only enter in a raid group
        if ((!group || !group->isRaidGroup()) && !sWorld->getBoolConfig(CONFIG_INSTANCE_IGNORE_RAID))
            return Map::CANNOT_ENTER_NOT_IN_RAID;

    if (!player->IsAlive())
    {
        if (player->HasCorpse())
        {
            // let enter in ghost mode in instance that connected to inner instance with corpse
            uint32 corpseMap = player->GetCorpseLocation().GetMapId();
            do
            {
                if (corpseMap == mapid)
                    break;

                InstanceTemplate const* corpseInstance = sObjectMgr->GetInstanceTemplate(corpseMap);
                corpseMap = corpseInstance ? corpseInstance->Parent : 0;
            } while (corpseMap);

            if (!corpseMap)
                return Map::CANNOT_ENTER_CORPSE_IN_DIFFERENT_INSTANCE;

            TC_LOG_DEBUG("maps", "MAP: Player '{}' has corpse in instance '{}' and can enter.", player->GetName(), mapName);
        }
        else
            TC_LOG_DEBUG("maps", "Map::CanPlayerEnter - player '{}' is dead but does not have a corpse!", player->GetName());
    }

    //Get instance where player's group is bound & its map
    if (!loginCheck && group)
    {
        InstanceGroupBind* boundInstance = group->GetBoundInstance(entry);
        if (boundInstance && boundInstance->save)
            if (Map* boundMap = sMapMgr->FindMap(mapid, boundInstance->save->GetInstanceId()))
                if (Map::EnterState denyReason = boundMap->CannotEnter(player))
                    return denyReason;
    }

    // players are only allowed to enter 5 instances per hour
    if (entry->IsDungeon() && (!player->GetGroup() || (player->GetGroup() && !player->GetGroup()->isLFGGroup())))
    {
        uint32 instanceIdToCheck = 0;
        if (InstanceSave* save = player->GetInstanceSave(mapid, entry->IsRaid()))
            instanceIdToCheck = save->GetInstanceId();

        // instanceId can never be 0 - will not be found
        if (!player->GetSession()->UpdateAndCheckInstanceCount(instanceIdToCheck) && !player->isDead())
            return Map::CANNOT_ENTER_TOO_MANY_INSTANCES;
    }

    //Other requirements
    if (!player->Satisfy(sObjectMgr->GetAccessRequirement(mapid, targetDifficulty), mapid, true))
        return Map::CANNOT_ENTER_UNSPECIFIED_REASON;

    return Map::CAN_ENTER;
}

void MapManager::Update(uint32 diff)
{
    i_timer.Update(diff);
    if (!i_timer.Passed())
        return;

    MapMapType::iterator iter = i_maps.begin();
    for (; iter != i_maps.end(); ++iter)
    {
        if (m_updater.activated())
            m_updater.schedule_update(*iter->second, uint32(i_timer.GetCurrent()));
        else
            iter->second->Update(uint32(i_timer.GetCurrent()));
    }
    if (m_updater.activated())
        m_updater.wait();

    for (iter = i_maps.begin(); iter != i_maps.end(); ++iter)
        iter->second->DelayedUpdate(uint32(i_timer.GetCurrent()));

    i_timer.SetCurrent(0);
}

void MapManager::DoDelayedMovesAndRemoves() { }

char const* MapManager::GetMapName(uint32 mapid)
{
    MapEntry const* entry = sMapStore.LookupEntry(mapid);
    return entry ? entry->MapName[sWorld->GetDefaultDbcLocale()] : "UNNAMEDMAP\x0";
}

bool MapManager::ExistMapAndVMap(uint32 mapid, float x, float y)
{
    GridCoord p = Trinity::ComputeGridCoord(x, y);

    int gx = (MAX_NUMBER_OF_GRIDS - 1) - p.x_coord;
    int gy = (MAX_NUMBER_OF_GRIDS - 1) - p.y_coord;

    return MapManager::ExistMap(mapid, gx, gy) && MapManager::ExistVMap(mapid, gx, gy);
}

bool MapManager::ExistMap(uint32 mapid, int gx, int gy)
{
    std::string fileName = Trinity::StringFormat("{}maps/{:03}{:02}{:02}.map", sWorld->GetDataPath(), mapid, gx, gy);

    bool ret = false;
    FILE* pf = fopen(fileName.c_str(), "rb");

    if (!pf)
    {
        TC_LOG_ERROR("maps", "Map file '{}' does not exist!", fileName);
        TC_LOG_ERROR("maps", "Please place MAP-files (*.map) in the appropriate directory ({}), or correct the DataDir setting in your worldserver.conf file.", (sWorld->GetDataPath()+"maps/"));
    }
    else
    {
        map_fileheader header;
        if (fread(&header, sizeof(header), 1, pf) == 1)
        {
            if (header.mapMagic.asUInt != MapMagic.asUInt || header.versionMagic != MapVersionMagic)
                TC_LOG_ERROR("maps", "Map file '{}' is from an incompatible map version (%.*s v{}), %.*s v{} is expected. Please pull your source, recompile tools and recreate maps using the updated mapextractor, then replace your old map files with new files. If you still have problems search on forum for error TCE00018.",
                    fileName, 4, header.mapMagic.asChar, header.versionMagic, 4, MapMagic.asChar, MapVersionMagic);
            else
                ret = true;
        }
        fclose(pf);
    }

    return ret;
}

bool MapManager::ExistVMap(uint32 mapid, int gx, int gy)
{
    if (VMAP::IVMapManager* vmgr = VMAP::VMapFactory::createOrGetVMapManager())
    {
        if (vmgr->isMapLoadingEnabled())
        {
            VMAP::LoadResult result = vmgr->existsMap((sWorld->GetDataPath() + "vmaps").c_str(), mapid, gx, gy);
            std::string name = vmgr->getDirFileName(mapid, gx, gy);
            switch (result)
            {
                case VMAP::LoadResult::Success:
                    break;
                case VMAP::LoadResult::FileNotFound:
                    TC_LOG_ERROR("maps", "VMap file '{}' does not exist", (sWorld->GetDataPath() + "vmaps/" + name));
                    TC_LOG_ERROR("maps", "Please place VMAP files (*.vmtree and *.vmtile) in the vmap directory ({}), or correct the DataDir setting in your worldserver.conf file.", (sWorld->GetDataPath() + "vmaps/"));
                    return false;
                case VMAP::LoadResult::VersionMismatch:
                    TC_LOG_ERROR("maps", "VMap file '{}' couldn't be loaded", (sWorld->GetDataPath() + "vmaps/" + name));
                    TC_LOG_ERROR("maps", "This is because the version of the VMap file and the version of this module are different, please re-extract the maps with the tools compiled with this module.");
                    return false;
            }
        }
    }

    return true;
}

bool MapManager::IsValidMAP(uint32 mapid, bool startUp)
{
    MapEntry const* mEntry = sMapStore.LookupEntry(mapid);

    if (startUp)
        return mEntry ? true : false;
    else
        return mEntry && (!mEntry->IsDungeon() || sObjectMgr->GetInstanceTemplate(mapid));

    /// @todo add check for battleground template
}

void MapManager::UnloadAll()
{
    // first unload maps
    for (auto iter = i_maps.begin(); iter != i_maps.end(); ++iter)
    {
        iter->second->UnloadAll();

        sScriptMgr->OnDestroyMap(iter->second.get());

        UnloadGridMaps(iter->first);
    }

    // then delete them
    i_maps.clear();

    if (m_updater.activated())
        m_updater.deactivate();
}

uint32 MapManager::GetNumInstances()
{
    std::lock_guard<std::mutex> lock(_mapsLock);

    uint32 ret = 0;
    for (auto const& [_, map] : i_maps)
    {
        MapInstanced* mapInstanced = map->ToMapInstanced();
        if (!mapInstanced)
            continue;
        ret += mapInstanced->GetInstancedMaps().size();
    }
    return ret;
}

uint32 MapManager::GetNumPlayersInInstances()
{
    std::lock_guard<std::mutex> lock(_mapsLock);

    uint32 ret = 0;
    for (auto& [_, map] : i_maps)
    {
        MapInstanced* mapInstanced = map->ToMapInstanced();
        if (!mapInstanced)
            continue;
        MapInstanced::InstancedMaps& maps = mapInstanced->GetInstancedMaps();
        ret += std::accumulate(maps.begin(), maps.end(), 0u, [](uint32 total, MapInstanced::InstancedMaps::value_type const& value) { return total + value.second->GetPlayers().getSize(); });
    }
    return ret;
}

void MapManager::InitInstanceIds()
{
    _nextInstanceId = 1;

    QueryResult result = CharacterDatabase.Query("SELECT MAX(id) FROM instance");
    if (result)
    {
        uint32 maxId = (*result)[0].GetUInt32();

        // resize to maxId + 1, if we have instance with id n, there are n+1 elements
        _instanceIds.resize(maxId + 1);
    }
}

void MapManager::RegisterInstanceId(uint32 instanceId)
{
    // Allocation was done in InitInstanceIds()
    _instanceIds[instanceId] = true;

    // Instances are pulled in ascending order from db and _nextInstanceId is initialized with 1,
    // so if the instance id is used, increment
    if (_nextInstanceId == instanceId)
        ++_nextInstanceId;
}

uint32 MapManager::GenerateInstanceId()
{
    uint32 newInstanceId = _nextInstanceId;

    // find the lowest available id starting from the current _nextInstanceId
    while (_nextInstanceId < 0xFFFFFFFF && ++_nextInstanceId < _instanceIds.size() && _instanceIds[_nextInstanceId]);

    if (_nextInstanceId == 0xFFFFFFFF)
    {
        TC_LOG_ERROR("maps", "Instance ID overflow!! Can't continue, shutting down server. ");
        World::StopNow(ERROR_EXIT_CODE);
    }

    return newInstanceId;
}

GridMap* MapManager::GetGridMap(uint32 mapId, int gx, int gy)
{
    auto& grid = _mapGrids[mapId]; // Will create if not present
    if (grid[gx][gy])
        return grid[gx][gy];

    std::string fileName = Trinity::StringFormat("{}maps/{:03}{:02}{:02}.map", sWorld->GetDataPath(), mapId, gx, gy);
    grid[gx][gy] = new GridMap();
    if (grid[gx][gy]->loadData(fileName.c_str()))
    {
        sScriptMgr->OnLoadGridMap(this, grid[gx][gy], gx, gy);
        LoadVMap(mapId, gx, gy);
        LoadMMap(mapId, gx, gy);
    }
    else
    {
        TC_LOG_ERROR("maps", "Error loading map file: \n {}\n", fileName);
    }
}

void MapManager::LoadVMap(uint32 mapId, int gx, int gy)
{
    if (!VMAP::VMapFactory::createOrGetVMapManager()->isMapLoadingEnabled())
        return;
                                                            // x and y are swapped !!
    int vmapLoadResult = VMAP::VMapFactory::createOrGetVMapManager()->loadMap((sWorld->GetDataPath()+ "vmaps").c_str(),  mapId, gx, gy);
    switch (vmapLoadResult)
    {
        case VMAP::VMAP_LOAD_RESULT_OK:
            TC_LOG_DEBUG("maps", "VMAP loaded name:{}, id:{}, x:{}, y:{} (vmap rep.: x:{}, y:{})", GetMapName(mapId), mapId, gx, gy, gx, gy);
            break;
        case VMAP::VMAP_LOAD_RESULT_ERROR:
            TC_LOG_ERROR("maps", "Could not load VMAP name:{}, id:{}, x:{}, y:{} (vmap rep.: x:{}, y:{})", GetMapName(mapId), mapId, gx, gy, gx, gy);
            break;
        case VMAP::VMAP_LOAD_RESULT_IGNORED:
            TC_LOG_DEBUG("maps", "Ignored VMAP name:{}, id:{}, x:{}, y:{} (vmap rep.: x:{}, y:{})", GetMapName(mapId), mapId, gx, gy, gx, gy);
            break;
    }
}

void MapManager::LoadMMap(uint32 mapId, int gx, int gy)
{
    if (!DisableMgr::IsPathfindingEnabled(mapId))
        return;

    bool mmapLoadResult = MMAP::MMapFactory::createOrGetMMapManager()->loadMap(sWorld->GetDataPath(), mapId, gx, gy);

    if (mmapLoadResult)
        TC_LOG_DEBUG("mmaps.tiles", "MMAP loaded name:{}, id:{}, x:{}, y:{} (mmap rep.: x:{}, y:{})", GetMapName(mapId), mapId, gx, gy, gx, gy);
    else
        TC_LOG_WARN("mmaps.tiles", "Could not load MMAP name:{}, id:{}, x:{}, y:{} (mmap rep.: x:{}, y:{})", GetMapName(mapId), mapId, gx, gy, gx, gy);
}

void MapManager::UnloadGridMaps(uint32 mapId)
{
    auto it = _mapGrids.find(mapId);
    if (it != _mapGrids.end())
    {
        auto& grid = it->second;
        for (int gx = 0; gx < MAX_NUMBER_OF_GRIDS; ++gx)
        {
            for (int gy = 0; gy < MAX_NUMBER_OF_GRIDS; ++gy)
            {
                if (grid[gx][gy])
                {
                    grid[gx][gy]->unloadData();
                    delete grid[gx][gy];
                    grid[gx][gy] = nullptr;
                    VMAP::VMapFactory::createOrGetVMapManager()->unloadMap(mapId, gx, gy);
                    MMAP::MMapFactory::createOrGetMMapManager()->unloadMap(mapId, gx, gy);
                }
            }
        }
        _mapGrids.erase(it);
    }
}