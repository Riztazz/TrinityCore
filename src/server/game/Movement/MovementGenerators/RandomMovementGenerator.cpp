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

#include "RandomMovementGenerator.h"
#include "Creature.h"
#include "G3DPosition.hpp"
#include "Map.h"
#include "MovementDefines.h"
#include "MoveSpline.h"
#include "MoveSplineInit.h"
#include "PathGenerator.h"
#include "Random.h"
#include <random>
#include <algorithm>
#include <G3D/Vector3.h>

template<class T>
RandomMovementGenerator<T>::RandomMovementGenerator(float distance) : _timer(0), _reference(),
_wanderDistance(distance), _wanderSteps(0), _needsPause(false), _angleIndex(0), _pathIndex(0), _smoothSplineId(0)
{
    this->Mode = MOTION_MODE_DEFAULT;
    this->Priority = MOTION_PRIORITY_NORMAL;
    this->Flags = MOVEMENTGENERATOR_FLAG_INITIALIZATION_PENDING;
    this->BaseUnitState = UNIT_STATE_ROAMING;
}

template RandomMovementGenerator<Creature>::RandomMovementGenerator(float/* distance*/);

template<class T>
MovementGeneratorType RandomMovementGenerator<T>::GetMovementGeneratorType() const
{
    return RANDOM_MOTION_TYPE;
}

template<class T>
void RandomMovementGenerator<T>::Pause(uint32 timer /*= 0*/)
{
    if (timer)
    {
        this->AddFlag(MOVEMENTGENERATOR_FLAG_TIMED_PAUSED);
        _timer.Reset(timer);
        this->RemoveFlag(MOVEMENTGENERATOR_FLAG_PAUSED);
    }
    else
    {
        this->AddFlag(MOVEMENTGENERATOR_FLAG_PAUSED);
        this->RemoveFlag(MOVEMENTGENERATOR_FLAG_TIMED_PAUSED);
    }
}

template<class T>
void RandomMovementGenerator<T>::Resume(uint32 overrideTimer /*= 0*/)
{
    if (overrideTimer)
        _timer.Reset(overrideTimer);

    this->RemoveFlag(MOVEMENTGENERATOR_FLAG_PAUSED);
}

template MovementGeneratorType RandomMovementGenerator<Creature>::GetMovementGeneratorType() const;

template<class T>
void RandomMovementGenerator<T>::DoInitialize(T*) { }

template<>
void RandomMovementGenerator<Creature>::DoInitialize(Creature* owner)
{
    RemoveFlag(MOVEMENTGENERATOR_FLAG_INITIALIZATION_PENDING | MOVEMENTGENERATOR_FLAG_TRANSITORY | MOVEMENTGENERATOR_FLAG_DEACTIVATED | MOVEMENTGENERATOR_FLAG_TIMED_PAUSED);
    AddFlag(MOVEMENTGENERATOR_FLAG_INITIALIZED);

    if (!owner || !owner->IsAlive())
        return;

    owner->StopMoving();
    _timer.Reset(0);
    
    _pathIndex = 0;
    _paths.clear();
    _smoothSplineId = 0;
    _pathGenerator = nullptr;

    if (_wanderDistance == 0.f)
        _wanderDistance = owner->GetWanderDistance();

    // Retail seems to let a creature walk 2 up to 10 splines before triggering a pause
    _wanderSteps = urand(1, ((_wanderDistance <= 1.0f) ? 2 : 8));
    _needsPause = false;

    // Only set these on first initialize
    if (_angles.empty())
    {
        _reference = owner->GetPosition();
        // Precalculate a spread of angles to use for our wander points, this gives us a more even distribution of 'random' points
        float initAngle = frand(0.f, M_PI * 2.0f);
        std::vector<float> tempAngles;
        for (uint8 i = 0; i < NUM_WANDER_POINTS; ++i)
        {
            tempAngles.push_back(initAngle + (M_PI * 2.0f / (float)NUM_WANDER_POINTS) * i);
        }
        std::random_device rd;
        std::mt19937 g(rd());
        std::shuffle(tempAngles.begin(), tempAngles.end(), g);
        _angles.insert(_angles.end(), tempAngles.begin(), tempAngles.end());
    }
}

template<class T>
void RandomMovementGenerator<T>::DoReset(T*) { }

template<>
void RandomMovementGenerator<Creature>::DoReset(Creature* owner)
{
    RemoveFlag(MOVEMENTGENERATOR_FLAG_TRANSITORY | MOVEMENTGENERATOR_FLAG_DEACTIVATED);

    DoInitialize(owner);
}

template<class T>
void RandomMovementGenerator<T>::SetRandomLocation(T*) { }

template<>
void RandomMovementGenerator<Creature>::SetRandomLocation(Creature* owner)
{
    if (!owner)
        return;

    if (owner->HasUnitState(UNIT_STATE_NOT_MOVE | UNIT_STATE_LOST_CONTROL) || owner->IsMovementPreventedByCasting())
    {
        AddFlag(MOVEMENTGENERATOR_FLAG_INTERRUPTED);
        owner->StopMoving();
        _pathIndex = 0;
        _paths.clear();
        _smoothSplineId = 0;
        _pathGenerator = nullptr;
        return;
    }

    // No cached paths so create a new one
    if (_paths.size() <= NUM_WANDER_POINTS)
    {
        Position position;
        if (_paths.size() == NUM_WANDER_POINTS)
        {
            // Last path needs to connect to the first point
            G3D::Vector3& v = _paths[0][0];
            position.Relocate(v.x, v.y, v.z);
        }
        else
        {
            position = _reference;
            float distance = frand(MIN_WANDER_DISTANCE, std::max(MIN_WANDER_DISTANCE, _wanderDistance));
            float angle = _angles[_angleIndex];
            _angleIndex = (_angleIndex + 1) % NUM_WANDER_POINTS;
            // Project destination position to the first collision
            owner->MovePositionToFirstCollision(position, distance, angle);
        }

        // Check if the destination is in LOS
        if (!owner->IsWithinLOS(position.GetPositionX(), position.GetPositionY(), position.GetPositionZ()))
        {
            // Retry later on
            _timer.Reset(200);
            // Always clear the cache if we fail to complete the loop at any step
            _pathIndex = 0;
            _paths.clear();
            _smoothSplineId = 0;
            return;
        }

        // Lazy load path generator
        if (!_pathGenerator)
        {
            _pathGenerator = std::make_unique<PathGenerator>(owner);
            _pathGenerator->SetPathLengthLimit(30.0f);
        }

        bool result = _pathGenerator->CalculatePath(position.GetPositionX(), position.GetPositionY(), position.GetPositionZ());
        // PATHFIND_FARFROMPOLY shouldn't be checked as creatures in water are most likely far from poly
        if (!result || (_pathGenerator->GetPathType() & PATHFIND_NOPATH)
                    || (_pathGenerator->GetPathType() & PATHFIND_SHORTCUT)
                    /*|| (_pathGenerator->GetPathType() & PATHFIND_FARFROMPOLY)*/)
        {
            _timer.Reset(100);
            // Always clear the cache if we fail to complete the loop at any step
            _pathIndex = 0;
            _paths.clear();
            _smoothSplineId = 0;
            return;
        }

        _paths.push_back(_pathGenerator->GetPath());
    }

    RemoveFlag(MOVEMENTGENERATOR_FLAG_TRANSITORY | MOVEMENTGENERATOR_FLAG_TIMED_PAUSED);

    owner->AddUnitState(UNIT_STATE_ROAMING_MOVE);

    bool walk = true;
    switch (owner->GetMovementTemplate().GetRandom())
    {
        case CreatureRandomMovementType::CanRun:
            walk = owner->IsWalking();
            break;
        case CreatureRandomMovementType::AlwaysRun:
            walk = false;
            break;
        default:
            break;
    }

    Movement::MoveSplineInit init(owner);
    // If we are still moving on the smooth spline
    if (!owner->movespline->Finalized() && owner->movespline->GetId() == _smoothSplineId)
    {
        Movement::PointsArray smoothPath;
        smoothPath.reserve(_paths[_pathIndex].size() + 1);
        smoothPath.push_back(PositionToVector3(owner->GetPosition()));
        smoothPath.insert(smoothPath.end(), _paths[_pathIndex].begin(), _paths[_pathIndex].end());
        init.MovebyPath(smoothPath);
        _smoothSplineId = 0;
    }
    else
        init.MovebyPath(_paths[_pathIndex]);
    init.SetSmooth();
    init.SetWalk(walk);
    init.Launch();

    if (sWorld->getBoolConfig(CONFIG_DONT_CACHE_RANDOM_MOVEMENT_PATHS))
        _paths.clear();
    else
        _pathIndex = (_pathIndex + 1) % (NUM_WANDER_POINTS + 1);

    --_wanderSteps;
    if (!_wanderSteps)
    {
        _wanderSteps = urand(1, ((_wanderDistance <= 1.0f) ? 2 : 8));
        _needsPause = true; // We need to pause at the end of this spline
    }

    // Call for creature group update
    owner->SignalFormationMovement();
}

template<class T>
bool RandomMovementGenerator<T>::DoUpdate(T*, uint32)
{
    return false;
}

template<>
bool RandomMovementGenerator<Creature>::DoUpdate(Creature* owner, uint32 diff)
{
    if (!owner || !owner->IsAlive())
        return true;

    if (HasFlag(MOVEMENTGENERATOR_FLAG_FINALIZED | MOVEMENTGENERATOR_FLAG_PAUSED))
        return true;

    if (owner->HasUnitState(UNIT_STATE_NOT_MOVE) || owner->IsMovementPreventedByCasting())
    {
        AddFlag(MOVEMENTGENERATOR_FLAG_INTERRUPTED);
        owner->StopMoving();
        _pathIndex = 0;
        _paths.clear();
        _smoothSplineId = 0;
        _pathGenerator = nullptr;
        return true;
    }
    else
        RemoveFlag(MOVEMENTGENERATOR_FLAG_INTERRUPTED);

    _timer.Update(diff);

    // We have to make new splines on speed change
    if (HasFlag(MOVEMENTGENERATOR_FLAG_SPEED_UPDATE_PENDING) && !owner->movespline->Finalized()) {
        _pathIndex = 0;
        _paths.clear();
        _smoothSplineId = 0;
        SetRandomLocation(owner);
    }
    // Wait out any timer
    else if (_timer.Passed())
    {
        // We can only do spline smoothing under certain conditions:
        // 1. We have the next path needed cached - we need to ensure we have a valid path to splice to
        // 2. We are not pausing at the end of the current path - otherwise we need to walk to the last point and wait
        // 3. The spline is not finalized - if we are finalized it is too late to splice paths anyway
        // 4. The current path index is the next to last path index - here we will try to splice the remaining segment to the next path
        if (_paths.size() >= NUM_WANDER_POINTS && !_needsPause && !owner->movespline->Finalized() && owner->movespline->MaxPathIdx() >= 1 && owner->movespline->currentPathIdx() >= owner->movespline->MaxPathIdx() - 1)
        {
            _smoothSplineId = owner->movespline->GetId();
            SetRandomLocation(owner);
        }
        // Else we need to wait until the spline is finalized
        else if (owner->movespline->Finalized())
        {
            // If we have indicated we need to pause then set the timer and wait
            if (_needsPause)
            {
                _needsPause = false;
                _timer.Reset(urand(6, 12) * IN_MILLISECONDS); // Retails seems to use rounded numbers so we do as well
            }
            else
                SetRandomLocation(owner);
        }
    }

    return true;
}

template<class T>
void RandomMovementGenerator<T>::DoDeactivate(T*) { }

template<>
void RandomMovementGenerator<Creature>::DoDeactivate(Creature* owner)
{
    AddFlag(MOVEMENTGENERATOR_FLAG_DEACTIVATED);
    owner->ClearUnitState(UNIT_STATE_ROAMING_MOVE);
}

template<class T>
void RandomMovementGenerator<T>::DoFinalize(T*, bool, bool) { }

template<>
void RandomMovementGenerator<Creature>::DoFinalize(Creature* owner, bool active, bool/* movementInform*/)
{
    AddFlag(MOVEMENTGENERATOR_FLAG_FINALIZED);
    if (active)
    {
        owner->ClearUnitState(UNIT_STATE_ROAMING_MOVE);
        owner->StopMoving();

        // TODO: Research if this modification is needed, which most likely isnt
        owner->SetWalk(false);
    }
}
