#ifndef _GRIDOBJECT_H
#define _GRIDOBJECT_H

#include "Cell.h"
#include "GridReference.h"
#include "GridRefManager.h"

template<class T>
class GridObject
{
    public:
        virtual ~GridObject() { }

        bool IsInGrid() const { return _gridRef.isValid(); }
        void AddToGrid(GridRefManager<T>& m)
        {
            ASSERT(!IsInGrid());
            _gridRef.link(&m, (T*)this);
        }
        void RemoveFromGrid()
        {
            ASSERT(IsInGrid());
            _gridRef.unlink();
        }
        Cell const& GetCell() const { return _cell; }
        void SetCell(Cell const& cell) { _cell = cell; }
    private:
        GridReference<T> _gridRef;
        Cell _cell;
};

#endif
