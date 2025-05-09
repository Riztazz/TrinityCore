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

#include "MapPartitioned.h"
#include "DBCStores.h"
#include "Group.h"
#include "Log.h"
#include "MapManager.h"
#include "MMapFactory.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "VMapFactory.h"
#include "VMapManager2.h"
#include "TSProfile.h"
#include "World.h"

MapPartitioned::MapPartitioned(uint32 id) : Map(id)
{
    // TODO lookup partition entry data and keep parition bounds here - we do this here since we
    // since we need to determine the partition id before the map is loaded for the player
    // Create a single partition (partitionId = 1) that covers the whole map as a rectangle
    PartitionPolygon fullMapPolygon;
    fullMapPolygon.emplace_back(-MAP_HALFSIZE, -MAP_HALFSIZE);
    fullMapPolygon.emplace_back( MAP_HALFSIZE, -MAP_HALFSIZE);
    fullMapPolygon.emplace_back( MAP_HALFSIZE,  MAP_HALFSIZE);
    fullMapPolygon.emplace_back(-MAP_HALFSIZE,  MAP_HALFSIZE);

    _partitionBounds[1] = std::move(fullMapPolygon);
}

void MapPartitioned::InitVisibilityDistance()
{
    if (_partitionedMaps.empty())
        return;
    //initialize visibility distances for all partition maps
    for (auto& kv : _partitionedMaps)
    {
        kv.second->InitVisibilityDistance();
    }
}

void MapPartitioned::Update(uint32 t)
{
    // Dont think this is necessary
    Map::Update(t);

    for (auto& kv : _partitionedMaps)
    {
        if (sMapMgr->GetMapUpdater()->activated())
            sMapMgr->GetMapUpdater()->schedule_update(*kv.second, t);
        else
            kv.second->Update(t);
    }
}

void MapPartitioned::DelayedUpdate(uint32 diff)
{
    for (auto& kv : _partitionedMaps)
        kv.second->DelayedUpdate(diff);
}

void MapPartitioned::UnloadAll()
{
    for (auto& kv : _partitionedMaps)
    {
        kv.second->UnloadAll();
        sScriptMgr->OnDestroyMap(kv.second.get());
    }
    _partitionedMaps.clear();

    Map::UnloadAll();
}

static bool IsPointInPolygon(float x, float y, const std::vector<std::pair<float, float>>& polygon)
{
    bool inside = false;
    size_t n = polygon.size();
    if (n < 3)
        return false;
    for (size_t i = 0, j = n - 1; i < n; j = i++) {
        float xi = polygon[i].first, yi = polygon[i].second;
        float xj = polygon[j].first, yj = polygon[j].second;
        bool intersect = ((yi > y) != (yj > y)) &&
                         (x < (xj - xi) * (y - yi) / (yj - yi + 1e-12f) + xi);
        if (intersect)
            inside = !inside;
    }
    return inside;
}

uint32 MapPartitioned::GetPartitionId(float x, float y) const
{
    for (const auto& [partitionId, polygon] : _partitionBounds)
    {
        if (IsPointInPolygon(x, y, polygon))
            return partitionId;
    }
    return 0;
}

PartitionMap* MapPartitioned::CreatePartition(uint32 mapId, uint32 partitionId)
{
    ZoneScopedNC("Map* MapPartitioned::CreatePartition", WORLD_UPDATE_COLOR)

    if (GetId() != mapId)
        return nullptr;

    // load/create a map
    std::lock_guard<std::mutex> lock(_mapLock);

    // make sure we have a valid map id
    MapEntry const* entry = sMapStore.LookupEntry(GetId());
    if (!entry)
    {
        TC_LOG_ERROR("maps", "CreatePartition: no entry for map {}", GetId());
        ABORT();
    }
    // TODO lookup parition entry
    // PartitionTemplate const* pTemplate = sObjectMgr->GetPartitionTemplate(GetId(), partitionId);
    // if (!pTemplate)
    // {
    //     TC_LOG_ERROR("maps", "CreatePartitionMap: no partition template for map {}", GetId());
    //     ABORT();
    // }

    TC_LOG_DEBUG("maps", "MapPartitioned::CreatePartition: map partition {} for {} created", partitionId, GetId());

    PartitionMap* map = new PartitionMap(GetId(), partitionId, this);
    ASSERT(map->IsWorldMap());

    map->LoadRespawnTimes();
    map->LoadCorpseData();

    Trinity::unique_trackable_ptr<Map>& ptr = _partitionedMaps[partitionId];
    ptr.reset(map);
    map->SetWeakPtr(ptr);

    sScriptMgr->OnCreateMap(map);
    return map;
}