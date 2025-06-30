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

#ifndef TRINITY_NGRID_H
#define TRINITY_NGRID_H

/** NGrid is nothing more than a wrapper of the Grid with an NxN cells
 */

#include "Grid.h"
#include "GridReference.h"
#include "Timer.h"
#include "Util.h"
#include "Random.h"

#define DEFAULT_VISIBILITY_NOTIFY_PERIOD      1000

typedef enum
{
    GRID_STATE_INACTIVE = 0,
    GRID_STATE_ACTIVE = 1,
    MAX_GRID_STATE = 2
} grid_state_t;

template
<
uint32 N,
class ACTIVE_OBJECT,
class WORLD_OBJECT_TYPES,
class GRID_OBJECT_TYPES
>
class NGrid
{
    public:
        typedef Grid<ACTIVE_OBJECT, WORLD_OBJECT_TYPES, GRID_OBJECT_TYPES> GridType;
        NGrid(uint32 id, int32 x, int32 y) :
            i_gridId(id), i_x(x), i_y(y),
            i_cellstate(GRID_STATE_INACTIVE), i_GridObjectDataLoaded(false), vis_Update(0, irand(0, DEFAULT_VISIBILITY_NOTIFY_PERIOD))
        { }

        GridType& GetGridType(const uint32 x, const uint32 y)
        {
            ASSERT(x < N && y < N);
            return i_cells[x][y];
        }

        GridType const& GetGridType(const uint32 x, const uint32 y) const
        {
            ASSERT(x < N && y < N);
            return i_cells[x][y];
        }

        uint32 GetGridId(void) const { return i_gridId; }
        grid_state_t GetGridState(void) const { return i_cellstate; }
        void SetGridState(grid_state_t s) { i_cellstate = s; }
        int32 getX() const { return i_x; }
        int32 getY() const { return i_y; }

        void link(GridRefManager<NGrid<N, ACTIVE_OBJECT, WORLD_OBJECT_TYPES, GRID_OBJECT_TYPES> >* pTo)
        {
            i_Reference.link(pTo, this);
        }
        bool isGridObjectDataLoaded() const { return i_GridObjectDataLoaded; }
        void setGridObjectDataLoaded(bool pLoaded) { i_GridObjectDataLoaded = pLoaded; }

        PeriodicTimer& getRelocationTimer() { return vis_Update; }

        // Visit all Grids (cells) in NGrid (grid)
        template<class T, class TT>
        void VisitAllGrids(TypeContainerVisitor<T, TypeMapContainer<TT> > &visitor)
        {
            for (uint32 x = 0; x < N; ++x)
                for (uint32 y = 0; y < N; ++y)
                    GetGridType(x, y).Visit(visitor);
        }

        // Visit a single Grid (cell) in NGrid (grid)
        template<class T, class TT>
        void VisitGrid(const uint32 x, const uint32 y, TypeContainerVisitor<T, TypeMapContainer<TT> > &visitor)
        {
            GetGridType(x, y).Visit(visitor);
        }

        template<class T>
        uint32 GetWorldObjectCountInNGrid() const
        {
            uint32 count = 0;
            for (uint32 x = 0; x < N; ++x)
                for (uint32 y = 0; y < N; ++y)
                    count += i_cells[x][y].template GetWorldObjectCountInGrid<T>();
            return count;
        }

    private:
        uint32 i_gridId;
        GridReference<NGrid<N, ACTIVE_OBJECT, WORLD_OBJECT_TYPES, GRID_OBJECT_TYPES> > i_Reference;
        int32 i_x;
        int32 i_y;
        grid_state_t i_cellstate;
        GridType i_cells[N][N];
        bool i_GridObjectDataLoaded;
        PeriodicTimer vis_Update;
};
#endif
