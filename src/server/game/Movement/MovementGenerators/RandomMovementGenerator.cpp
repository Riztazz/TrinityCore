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
    constexpr float DEFAULT_WANDER_DISTANCE = 2.0f;
    constexpr float SMOOTH_CORNER_RADIUS = 1.0f;
    constexpr int NUM_WANDER_POINTS = 12;
    constexpr int SMOOTH_CORNER_NUM_POINTS = 5;
}

template<class T>
RandomMovementGenerator<T>::RandomMovementGenerator(float distance) : _init(false), _maxWanderDistance(distance), _wanderSteps(0), _reference(), _pathIndex(0), _timer(0)
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

    if (_maxWanderDistance <= DEFAULT_WANDER_DISTANCE)
        _maxWanderDistance = std::max(DEFAULT_WANDER_DISTANCE, owner->GetWanderDistance());

    // Retail seems to let a creature walk 2 up to 10 splines before triggering a pause
    _wanderSteps = urand(1, ((_maxWanderDistance <= DEFAULT_WANDER_DISTANCE) ? 2 : 8));
    // Should we reset timer? _timer.Reset(0);

    // Only set these on first initialize
    if (!_init)
    {
        _init = true;
        _reference = owner->GetPosition();
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
        // We cache the actual points paths around the circuit, our splines are constructed separately so that we
        // can smooth the vertexes.
        Position src = _paths.empty() ? owner->GetPosition() : Vector3ToPosition(_paths.back().back());
        Position dest;
        // Last path needs to connect to the first point of the first path of the circuit
        if (_paths.size() == NUM_WANDER_POINTS)
        {
            // _paths[1] is the first path of the circuit (_paths[0] is where we started moving to get to the circuit)
            G3D::Vector3& v = _paths[1].front();
            dest.Relocate(v.x, v.y, v.z);
        }
        // Otherwise we need to construct a path to a wander point
        else
        {
            int attempts = 5;
            while (true)
            {
                if (!attempts)
                {
                    _timer.Reset(200);
                    ResetPaths();
                    return;
                }
                --attempts;

                // Using our reference (spawn) point and get a random point in a circle around it
                float distance = frand(MIN_WANDER_DISTANCE, _maxWanderDistance);
                float angle = frand(0.f, M_PI * 2.0f);
                float x = _reference.GetPositionX() + distance * std::cos(angle);
                float y = _reference.GetPositionY() + distance * std::sin(angle);
                dest.Relocate(x, y, _reference.GetPositionZ());
                // Account for collision
                owner->MovePositionToFirstCollision(src, dest, 0.0f, 0.0f);

                if (owner->GetSpawnId() == 80043)
                    TC_LOG_DEBUG("smooth", "Create Path Index: {} Calc Path Distance from Src: {}", _pathIndex, src.GetExactDist(dest));
                if (src.GetExactDist(dest) < SMOOTH_CORNER_RADIUS * 2.0f + 1.0f)
                    continue;

                float srcX = src->GetPositionX();
                float srcY = src->GetPositionY();
                float destX = dest.GetPositionX();
                float destY = dest.GetPositionY();

                float dx = destX - srcX;
                float dy = destY - srcY;

                // Angle from src to dest in world coordinates
                float angleToDest = std::atan2(dy, dx);

                // Owner's current orientation
                float orientation = owner->GetOrientation();

                // Angle difference (dest direction relative to facing)
                float angleDiff = angleToDest - orientation;
                while (angleDiff > M_PI) angleDiff -= 2.0f * M_PI;
                while (angleDiff < -M_PI) angleDiff += 2.0f * M_PI;
                if (std::abs(angleDiff) > 0.75 * M_PI)
                    continue;

                break;
            }
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
            _pathGenerator->SetPathLengthLimit(50.0f);
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

        Movement::PointsArray path = _pathGenerator->GetPath();
        if (path.size() < 2)
        {
            _timer.Reset(100);
            ResetPaths();
            return;
        }

        _paths.push_back(path);
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

    // For debugging purposes move with no smoothing
    if (SMOOTH_CORNER_NUM_POINTS <= 1 || _paths[_pathIndex].size() < 2)
    {
        init.MovebyPath(_paths[_pathIndex]);
    }
    // The first path we just need to truncate the end so we can smooth the next
    else if (_paths.size() == 1)
    {
        Movement::PointsArray modPath = PathGenerator::TruncatePath(owner, _paths[_pathIndex], SMOOTH_CORNER_RADIUS);
        init.MovebyPath(modPath);
        init.SetSmooth();
    }
    // We want to smooth to the next path by splicing the end of the current path with the start of the next path and smoothing the corner
    else
    {
        
        Movement::PointsArray modPath = PathGenerator::TruncatePath(owner, _paths[_pathIndex], SMOOTH_CORNER_RADIUS, true);
        Movement::PointsArray splicePath = PathGenerator::SpliceAndSmoothPath(owner, _paths[_pathIndex].front(), modPath.front(), SMOOTH_CORNER_NUM_POINTS);
        splicePath.insert(splicePath.end(), modPath.begin(), modPath.end());
        if (owner->GetSpawnId() == 80043)
        {
            const G3D::Vector3& ownerPos = PositionToVector3(owner->GetPosition());
            const G3D::Vector3& pathStart = _paths[_pathIndex].front();
            const G3D::Vector3& modPathStart = modPath.front();

            // Vectors in XY
            float abx = pathStart.x - ownerPos.x;
            float aby = pathStart.y - ownerPos.y;
            float bcx = modPathStart.x - pathStart.x;
            float bcy = modPathStart.y - pathStart.y;

            float abLen = std::sqrt(abx * abx + aby * aby);
            float bcLen = std::sqrt(bcx * bcx + bcy * bcy);

            float dot = abx * bcx + aby * bcy;
            float angleRad = std::acos(dot / (abLen * bcLen));
            float angleDeg = angleRad * (180.0f / M_PI);

            TC_LOG_DEBUG("smooth", "owner->pathStart->modPathStart mod path index: {} angle (deg): {}", _pathIndex, angleDeg);

            for (size_t i = 1; i + 1 < splicePath.size(); ++i)
            {
                const G3D::Vector3& prev = splicePath[i - 1];
                const G3D::Vector3& curr = splicePath[i];
                const G3D::Vector3& next = splicePath[i + 1];

                float v1x = curr.x - prev.x;
                float v1y = curr.y - prev.y;
                float v2x = next.x - curr.x;
                float v2y = next.y - curr.y;

                float v1Len = std::sqrt(v1x * v1x + v1y * v1y);
                float v2Len = std::sqrt(v2x * v2x + v2y * v2y);

                // Guard against zero-length segments
                if (v1Len == 0.f || v2Len == 0.f)
                    continue;

                float dot = v1x * v2x + v1y * v2y;
                float angleRad = std::acos(dot / (v1Len * v2Len));
                float angleDeg = angleRad * (180.0f / M_PI);

                TC_LOG_DEBUG("smooth", "splicePath vertex {} (x={}, y={}): angle (deg): {}", i, curr.x, curr.y, angleDeg);
            }
        }
        init.MovebyPath(splicePath);
        init.SetSmooth();
    }

    init.SetWalk(walk);
    init.Launch();

    if (!_paths.empty())
    {
        ++_pathIndex;
        if (_pathIndex > NUM_WANDER_POINTS) // We have NUM_WANDER_POINTS + 1 paths in the cache, so NUM_WANDER_POINTS is max index
            _pathIndex = 1; // We actually skip the first path after cache is constructed
        --_wanderSteps;
    }

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
            // Retail seems to let a creature walk 2 up to 10 splines before triggering a pause
            _wanderSteps = urand(1, ((_maxWanderDistance <= DEFAULT_WANDER_DISTANCE) ? 2 : 8));
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
