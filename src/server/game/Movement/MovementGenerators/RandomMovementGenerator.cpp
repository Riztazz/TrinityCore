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
#include "Log.h"
#include "Map.h"
#include "MovementDefines.h"
#include "MoveSpline.h"
#include "MoveSplineInit.h"
#include "PathGenerator.h"
#include "Random.h"
#include <G3D/Vector3.h>

namespace
{
    constexpr float MIN_WANDER_DISTANCE = 1.0f;
    constexpr uint8 NUM_WANDER_POINTS = 12;
    // We will iterate our angles vector by this amount to create a less sharp path e.g if we are at index 0, we will lookup offset[0] = 3, so we will iterate to next angle of [3].
    constexpr int ANGLE_ITERATION_OFFSET[] = {3, 3, 3, -2, -3, -3, -3, 1, 3, 3, 3};
    constexpr float SMOOTH_CORNER_RADIUS = 1.0f;
    constexpr uint32 SMOOTH_CORNER_NUM_POINTS = 5;
}

template<class T>
RandomMovementGenerator<T>::RandomMovementGenerator(float distance) : _wanderDistance(distance), _wanderSteps(0), _reference(), _angleIndex(0), _pathIndex(0), _timer(0)
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
    ResetPaths();

    if (_wanderDistance == 0.f)
        _wanderDistance = owner->GetWanderDistance();

    // Retail seems to let a creature walk 2 up to 10 splines before triggering a pause
    _wanderSteps = urand(1, ((_wanderDistance <= 1.0f) ? 2 : 8));
    // Should we reset timer? _timer.Reset(0);

    // Only set these on first initialize
    if (_angles.empty())
    {
        _reference = owner->GetPosition();
        // Precalculate a spread of angles to use for our wander points, this gives us a more even distribution of 'random' points
        float initAngle = frand(0.f, M_PI * 2.0f);
        for (uint8 i = 0; i < NUM_WANDER_POINTS; ++i)
        {
            _angles.push_back(initAngle + (M_PI * 2.0f / (float)NUM_WANDER_POINTS) * i);
        }
        // Pick an iteration direction for this spawn, we do not reset these 
        _angleIterationSign = urand(0, 1) ? 1 : -1;
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
        ResetPaths();
        return;
    }

    // No cached paths so create a new one
    if (_paths.size() <= NUM_WANDER_POINTS)
    {
        // If this our first path we need to start from the owner position
        // If not, we actually stopped short of the end of the path to smooth the transition, so we need to start from the end of the current path
        Position src = _paths.empty() ? owner->GetPosition() : Vector3ToPosition(_paths.back().back());
        Position dest;
        // Last path needs to connect to the first point of the first path
        if (_paths.size() == NUM_WANDER_POINTS)
        {
            G3D::Vector3& v = _paths.front().front();
            dest.Relocate(v.x, v.y, v.z);
        }
        // Otherwise we need to construct a path to a wander point
        else
        {
            // Using our initial spawn point construct the distance and angle to a new point
            dest = _reference;
            float distance = frand(MIN_WANDER_DISTANCE, std::max(MIN_WANDER_DISTANCE, _wanderDistance));
            float angle = _angles[_angleIndex];
            _angleIndex = (_angleIndex + _angleIterationSign * ANGLE_ITERATION_OFFSET[_angleIndex]) % NUM_WANDER_POINTS;
            _angleIndex = (_angleIndex + NUM_WANDER_POINTS) % NUM_WANDER_POINTS;

            // Modify the wander point accounting for collision
            owner->MovePositionToFirstCollision(src, dest, distance, angle);
        }

        // Check if the destination is in LOS
        if (!owner->IsWithinLOS(src, dest.GetPositionX(), dest.GetPositionY(), dest.GetPositionZ()))
        {
            // Retry later on
            _timer.Reset(200);
            ResetPaths();
            return;
        }

        // Lazy load path generator
        if (!_pathGenerator)
        {
            _pathGenerator = std::make_unique<PathGenerator>(owner);
            _pathGenerator->SetPathLengthLimit(30.0f);
        }

        bool result = _pathGenerator->CalculatePath(PositionToVector3(src), PositionToVector3(dest));
        // PATHFIND_FARFROMPOLY shouldn't be checked as creatures in water are most likely far from poly
        if (!result || (_pathGenerator->GetPathType() & PATHFIND_NOPATH)
                    || (_pathGenerator->GetPathType() & PATHFIND_SHORTCUT)
                    /*|| (_pathGenerator->GetPathType() & PATHFIND_FARFROMPOLY)*/)
        {
            _timer.Reset(100);
            ResetPaths();
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

    // The first path we just need to truncate the end so we can smooth the next
    if (_paths.size() == 1)
    {
        Movement::PointsArray truncatedPath = PathGenerator::TruncateLastSegment(_paths[_pathIndex], SMOOTH_CORNER_RADIUS);
        init.MovebyPath(truncatedPath);
    }
    // We want to smooth to the next path by splicing the end of the current path with the start of the next path and smoothing the corner
    else
    {
        Movement::PointsArray prevPath;
        prevPath.push_back(PositionToVector3(owner->GetPosition()));
        prevPath.push_back(_paths[_pathIndex == 0 ? _paths.size() - 1 : _pathIndex - 1].back());
        Movement::PointsArray nextPath = PathGenerator::TruncateLastSegment(_paths[_pathIndex], SMOOTH_CORNER_RADIUS);
        Movement::PointsArray smoothPath = PathGenerator::SpliceAndSmoothPaths(prevPath, nextPath, SMOOTH_CORNER_RADIUS, SMOOTH_CORNER_NUM_POINTS);
        init.MovebyPath(smoothPath);
    }

    init.SetSmooth();
    init.SetWalk(walk);
    init.Launch();

    _pathIndex = (_pathIndex + 1) % (NUM_WANDER_POINTS + 1);
    --_wanderSteps;

    // Call for creature group update
    owner->SignalFormationMovement();
}

template<class T>
void RandomMovementGenerator<T>::ResetPaths()
{
    _pathIndex = 0;
    _paths.clear();
    _pathGenerator = nullptr;
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
        ResetPaths();
        return true;
    }
    else
        RemoveFlag(MOVEMENTGENERATOR_FLAG_INTERRUPTED);

    _timer.Update(diff);

    // We have to make new splines on speed change
    if (HasFlag(MOVEMENTGENERATOR_FLAG_SPEED_UPDATE_PENDING) && !owner->movespline->Finalized())
    {
        ResetPaths();
        SetRandomLocation(owner);
    }
    else if (owner->movespline->Finalized())
    {
        if (!_wanderSteps)
        {
            _wanderSteps = urand(1, ((_wanderDistance <= 1.0f) ? 2 : 8));
            _timer.Reset(urand(6, 12) * IN_MILLISECONDS); // Retails seems to use rounded numbers so we do as well
        }
        if (_timer.Passed())
            SetRandomLocation(owner);
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
