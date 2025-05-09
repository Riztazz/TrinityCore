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

#ifndef TRINITY_MAP_PARTITIONED_H
#define TRINITY_MAP_PARTITIONED_H

#include "Map.h"
#include "UniqueTrackablePtr.h"

class TC_GAME_API MapPartitioned : public Map
{
    friend class MapManager;
    public:
        typedef std::vector<std::pair<float, float>> PartitionPolygon;
        typedef std::unordered_map<uint32, PartitionPolygon> PartitionBounds;
        typedef std::unordered_map<uint32, Trinity::unique_trackable_ptr<PartitionMap>> PartitionedMaps;

        MapPartitioned(uint32 id);
        ~MapPartitioned() { }

        // functions overwrite Map versions
        void Update(uint32 diff) override;
        void DelayedUpdate(uint32 diff) override;
        virtual void InitVisibilityDistance() override;
        //void RelocationNotify();
        void UnloadAll() override;

        uint32 CalculatePartitionId(float x, float y) const;
        PartitionMap* CreatePartition(uint32 mapId, uint32 partitionId);
        PartitionMap* FindPartition(uint32 partitionId) const
        {
            PartitionedMaps::const_iterator i = _partitionedMaps.find(partitionId);
            return(i == _partitionedMaps.end() ? nullptr : i->second.get());
        }
        bool DestroyPartition(PartitionedMaps::iterator &itr);

        PartitionedMaps &GetPartitionedMaps() { return _partitionedMaps; }
        PartitionBounds &GetPartitionBounds() { return _partitionBounds; }
    private:
        static bool IsPointInPolygon(float x, float y, const PartitionPolygon& polygon);

        PartitionedMaps _partitionedMaps;
        PartitionBounds _partitionBounds; // partitionId -> polygon
};

#endif // TRINITY_MAP_PARTITIONED_H
