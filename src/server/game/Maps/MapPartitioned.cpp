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
#include "ObjectMgr.h"
#include "ScriptMgr.h"
#include "TSProfile.h"

MapPartitioned::MapPartitioned(uint32 id) : Map(id)
{
    // TODO lookup partition entry data and keep parition bounds here - we do this here since we
    // since we need to determine the partition id before the map is loaded for the player
    // Create a single partition (partitionId = 1) that covers the whole map as a rectangle
    PartitionPolygon fullMapPolygon;
    fullMapPolygon.emplace_back(Position(-MAP_HALFSIZE, -MAP_HALFSIZE));
    fullMapPolygon.emplace_back(Position( 0, -MAP_HALFSIZE));
    fullMapPolygon.emplace_back(Position( 0,  MAP_HALFSIZE));
    fullMapPolygon.emplace_back(Position(-MAP_HALFSIZE,  MAP_HALFSIZE));

    _partitionBounds[1] = std::move(fullMapPolygon);
}

void MapPartitioned::InitVisibilityDistance()
{
    for (auto& [_, partition] : _partitions)
        partition->InitVisibilityDistance();

    Map::InitVisibilityDistance();
}

void MapPartitioned::Update(uint32 t)
{
    for (auto& [partitionId, partitionPtr] : _partitions)
    {
        if (sMapMgr->GetMapUpdater()->activated())
            sMapMgr->GetMapUpdater()->schedule_update(*partitionPtr, t);
        else
            partitionPtr->Update(t);
    }

    Map::Update(t);
}

void MapPartitioned::DelayedUpdate(uint32 diff)
{
    for (auto& [partitionId, partitionPtr] : _partitions)
        partitionPtr->DelayedUpdate(diff);

    Map::DelayedUpdate(diff);
}

void MapPartitioned::UnloadAll()
{
    // Clear child maps
    for (auto& [partitionId, partitionPtr] : _partitions)
        partitionPtr->UnloadAll();

    _partitions.clear();

    // Clear this base map as well
    Map::UnloadAll();

    sScriptMgr->OnDestroyMap(this);
}

bool MapPartitioned::IsPointInPolygon(Position const& pos, PartitionPolygon const& polygon)
{
    float x = pos.GetPositionX();
    float y = pos.GetPositionY();
    bool inside = false;
    size_t n = polygon.size();
    if (n < 3)
        return false;
    for (size_t i = 0, j = n - 1; i < n; j = i++) {
        float xi = polygon[i].GetPositionX(), yi = polygon[i].GetPositionY();
        float xj = polygon[j].GetPositionX(), yj = polygon[j].GetPositionY();
        bool intersect = ((yi > y) != (yj > y)) &&
                         (x < (xj - xi) * (y - yi) / (yj - yi + 1e-12f) + xi);
        if (intersect)
            inside = !inside;
    }
    return inside;
}

uint32 MapPartitioned::CalculatePartitionId(Position const& pos) const
{
    for (const auto& [partitionId, polygon] : _partitionBounds)
    {
        if (IsPointInPolygon(pos, polygon))
            return partitionId;
    }

    return 0;
}

Map* MapPartitioned::CreatePartition(uint32 mapId, uint32 partitionId)
{
    ASSERT(GetId() == mapId);

    TC_LOG_DEBUG("maps", "MapPartitioned::CreatePartition called with mapId: {} partitionId: {}", mapId, partitionId);

    // The base map will be used as fallback for all partitions, this
    // just skips searching the partitions if the id matches
    if (GetPartitionId() == partitionId)
        return this;

    Map* partition = FindPartition(partitionId);
    if (partition)
        return partition;

    ZoneScopedNC("Map* MapPartitioned::CreatePartition", WORLD_UPDATE_COLOR)

    TC_LOG_DEBUG("maps", "Partition not found, actually creating it: {} partitionId: {}", mapId, partitionId);

    // load/create a map
    std::lock_guard<std::mutex> lock(_mapLock);

    // make sure we have a valid map id
    MapEntry const* entry = sMapStore.LookupEntry(GetId());
    if (!entry)
    {
        TC_LOG_ERROR("maps", "CreatePartition: no entry for map {}", GetId());
        ABORT();
    }
    // TODO lookup partition entry
    // PartitionTemplate const* pTemplate = sObjectMgr->GetPartitionTemplate(GetId(), partitionId);
    // if (!pTemplate)
    // {
    //     TC_LOG_ERROR("maps", "CreatePartitionMap: no partition template for map {}", GetId());
    //     ABORT();
    // }

    TC_LOG_DEBUG("maps", "MapPartitioned::CreatePartition: map partition {} for {} created", partitionId, GetId());

    Map* map = new PartitionMap(GetId(), partitionId, this);
    ASSERT(map->IsWorldMap());

    map->LoadRespawnTimes();
    map->LoadCorpseData();

    Trinity::unique_trackable_ptr<Map>& ptr = _partitions[partitionId];
    ptr.reset(map);
    map->SetWeakPtr(ptr);

    sScriptMgr->OnCreateMap(map);

    return map;
}