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
    constexpr float MIN_WANDER_DISTANCE = 3.0f; // Keep this at min SMOOTH_CORNER_RADIUS * 2 + 1
    constexpr float SMOOTH_CORNER_RADIUS = 1.0f;
    constexpr int NUM_WANDER_POINTS = 12;
    constexpr int SMOOTH_CORNER_NUM_POINTS = 3;
}

template<class T>
RandomMovementGenerator<T>::RandomMovementGenerator(float distance) : _init(false), _maxWanderDistance(distance), _wanderSteps(0), _reference(), _pathIndex(0), _cachedNextWanderPoint(), _timer(0)
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

    if (_maxWanderDistance <= MIN_WANDER_DISTANCE)
        _maxWanderDistance = std::max(MIN_WANDER_DISTANCE, owner->GetWanderDistance());

    // Retail seems to let a creature walk 2 up to 10 splines before triggering a pause
    _wanderSteps = urand(1, ((_maxWanderDistance <= MIN_WANDER_DISTANCE) ? 2 : 8));
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
    if (_paths.size() < NUM_WANDER_POINTS)
    {
        // We cache the actual points paths around the circuit, our splines are constructed separately so that we
        // can smooth the vertexes.
        Position src = _paths.empty() ? owner->GetPosition() : Vector3ToPosition(_paths.back().back());
        Position dest = src;
        // The last path connects to the first path
        if (_paths.size() == NUM_WANDER_POINTS - 1)
        {
            G3D::Vector3& front = _paths[0].front();
            dest.Relocate(front.x, front.y, front.z);
        }
        // We have pre-calculated this point previously, the last two points are calculated together in order
        // to connect to the start point smoothly
        else if (_paths.size() == NUM_WANDER_POINTS - 2)
        {
            // Use the cached value from the previous call
            if (_cachedNextWanderPoint.IsPositionValid())
            {
                dest = _cachedNextWanderPoint;
                _cachedNextWanderPoint = Position();
            }
            // Fallback: random direction
            else
            {
                float angle = frand(-0.5 * M_PI, 0.5 * M_PI);
                owner->MovePositionToFirstCollision(src, dest, MIN_WANDER_DISTANCE, angle);
            }
        }
        // We need very specific point for the second to last point, this is because we want to complete
        // the circuit with no sharp angles, so we need to find the two next points that get us back
        // to the start without a sharp turn
        else if (_paths.size() == NUM_WANDER_POINTS - 3)
        {
            Position first = Vector3ToPosition(_paths[0].front());

            float minDist = MIN_WANDER_DISTANCE;
            float maxTurn = 0.75f * M_PI;
            float step = M_PI / 16.0f; // 32 candidates per point
            float bestScore = std::numeric_limits<float>::max();

            Position bestA, bestB;
            float bestAngleA = 0.f, bestAngleB = 0.f;

            float currentOrientation = owner->GetOrientation();

            // Try all candidate angles for A (third-to-last point)
            for (float angleA = -maxTurn; angleA <= maxTurn; angleA += step)
            {
                float orientationA = currentOrientation + angleA;
                float ax = src.GetPositionX() + minDist * std::cos(orientationA);
                float ay = src.GetPositionY() + minDist * std::sin(orientationA);
                float az = src.GetPositionZ();
                Position A(ax, ay, az, 0.f);

                // Try all candidate angles for B (second-to-last point)
                for (float angleB = -maxTurn; angleB <= maxTurn; angleB += step)
                {
                    float orientationB = orientationA + angleB;
                    float bx = ax + minDist * std::cos(orientationB);
                    float by = ay + minDist * std::sin(orientationB);
                    float bz = az;
                    Position B(bx, by, bz, 0.f);

                    // Check distance from B to first
                    float distBtoFirst = B.GetExactDist(first);
                    if (distBtoFirst < minDist)
                        continue;

                    // Compute turn at A (between src->A and A->B)
                    float dirAtoB = std::atan2(by - ay, bx - ax);
                    float turnAtA = dirAtoB - orientationA;
                    while (turnAtA > M_PI) turnAtA -= 2 * M_PI;
                    while (turnAtA < -M_PI) turnAtA += 2 * M_PI;

                    if (std::fabs(turnAtA) > maxTurn)
                        continue;

                    // Compute turn at B (between A->B and B->first)
                    float dirBtoFirst = std::atan2(first.GetPositionY() - by, first.GetPositionX() - bx);
                    float turnAtB = dirBtoFirst - dirAtoB;
                    while (turnAtB > M_PI) turnAtB -= 2 * M_PI;
                    while (turnAtB < -M_PI) turnAtB += 2 * M_PI;

                    if (std::fabs(turnAtB) > maxTurn)
                        continue;

                    // Score: maximum turn at A or B (can use sum if you prefer)
                    float score = std::max(std::fabs(turnAtA), std::fabs(turnAtB));
                    if (score < bestScore)
                    {
                        bestScore = score;
                        bestA = A;
                        bestB = B;
                        bestAngleA = angleA;
                        bestAngleB = angleB;
                    }
                }
            }

            // Now call MovePositionToFirstCollision ONCE for each, to get the actual valid positions
            if (bestScore < std::numeric_limits<float>::max())
            {
                owner->MovePositionToFirstCollision(src, dest, minDist, bestAngleA);
                Position realB = dest;
                owner->MovePositionToFirstCollision(dest, realB, minDist, bestAngleB);
                _cachedNextWanderPoint = realB;
            }
            // Fallback: if no candidate found, use random directions
            else
            {
                bestAngleA = frand(-0.5 * M_PI, 0.5 * M_PI);
                owner->MovePositionToFirstCollision(src, dest, minDist, bestAngleA);
            }
        }
        // Otherwise we need to construct a path to a wander point
        else
        {
            float distance = frand(MIN_WANDER_DISTANCE, _maxWanderDistance);
            // Determine whether we should steer back towards the spawn point
            float distanceFromSpawn = src.GetExactDist(_reference);
            float angle;
            // If we are close to the boundary, steer back towards the spawn point
            if (distanceFromSpawn > 0.75f * _maxWanderDistance)
            {
                float currentOrientation = owner->GetOrientation();
                float dx = _reference.GetPositionX() - src.GetPositionX();
                float dy = _reference.GetPositionY() - src.GetPositionY();
                float angleToReference = std::atan2(dy, dx);

                float angleDiff = angleToReference - currentOrientation;
                // Normalize angleDiff to [-M_PI, M_PI]
                while (angleDiff > M_PI) angleDiff -= 2 * M_PI;
                while (angleDiff < -M_PI) angleDiff += 2 * M_PI;

                // Clamp angleDiff to [-0.75*M_PI, 0.75*M_PI]
                float maxTurn = 0.75f * M_PI;
                if (angleDiff > maxTurn) angleDiff = maxTurn;
                if (angleDiff < -maxTurn) angleDiff = -maxTurn;

                angle = angleDiff;
            }
            // Else walk in any random direction without sharp turns
            else
                angle = frand(-0.5 * M_PI, 0.5 * M_PI);

            // Move that direction and account for collisions
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
    if (SMOOTH_CORNER_NUM_POINTS <= 1)
    {
        init.MovebyPath(_paths[_pathIndex]);
    }
    // The first path we just need to truncate the end so we can smooth the next
    else if (_paths.size() == 1)
    {
        Movement::PointsArray modPath = PathGenerator::TruncatePath(owner, _paths[_pathIndex], SMOOTH_CORNER_RADIUS);
        init.MovebyPath(modPath);
        //init.SetSmooth();
    }
    // We want to smooth to the next path by splicing the end of the current path with the start of the next path and smoothing the corner
    else
    {
        
        Movement::PointsArray modPath = PathGenerator::TruncatePath(owner, _paths[_pathIndex], SMOOTH_CORNER_RADIUS, true);
        if (owner->GetPosition().GetExactDist(Vector3ToPosition(_paths[_pathIndex].front())) < SMOOTH_CORNER_RADIUS / 2.0f)
        {
            TC_LOG_DEBUG("smooth", "Owner not smooth distance away from  {} 0 (x={}, y={}) -> (x={}, y={}):", _pathIndex, owner->GetPositionX(), owner->GetPositionY(), _paths[_pathIndex].front().x, _paths[_pathIndex].front().y);
        }
        if (Vector3ToPosition(_paths[_pathIndex].front()).GetExactDist(Vector3ToPosition(modPath.front())) < SMOOTH_CORNER_RADIUS / 2.0f)
        {
            TC_LOG_DEBUG("smooth", "Could not fully truncate front for path {} 0 (x={}, y={}) -> (x={}, y={}):", _pathIndex, _paths[_pathIndex].front().x, _paths[_pathIndex].front().y, _paths[_pathIndex].back().x, _paths[_pathIndex].back().y);
        }
        Movement::PointsArray splicePath = PathGenerator::SpliceAndSmoothArc(owner, _paths[_pathIndex].front(), modPath.front(), SMOOTH_CORNER_RADIUS, SMOOTH_CORNER_NUM_POINTS);
        
        if (owner->GetSpawnId() == 80043)
        {
            TC_LOG_DEBUG("smooth", "owner vertex 0 (x={}, y={}):", owner->GetPositionX(), owner->GetPositionY());
            TC_LOG_DEBUG("smooth", "base path vertex 1 (x={}, y={}):", _paths[_pathIndex][0].x, _paths[_pathIndex][0].y);
            TC_LOG_DEBUG("smooth", "modPath vertex 1 (x={}, y={}):", modPath[0].x, modPath[0].y);

            const G3D::Vector3& prev = PositionToVector3(owner->GetPosition());
            const G3D::Vector3& curr = _paths[_pathIndex][0];
            const G3D::Vector3& next = modPath[0];

            float v1x = curr.x - prev.x;
            float v1y = curr.y - prev.y;
            float v2x = next.x - curr.x;
            float v2y = next.y - curr.y;

            float v1Len = std::sqrt(v1x * v1x + v1y * v1y);
            float v2Len = std::sqrt(v2x * v2x + v2y * v2y);

            float dot = v1x * v2x + v1y * v2y;
            float angleRad = std::acos(dot / (v1Len * v2Len));
            float angleDeg = angleRad * (180.0f / M_PI);

            TC_LOG_DEBUG("smooth", "base calculated angle (deg): {}", angleDeg);

            TC_LOG_DEBUG("smooth", "splicePath vertex 0 (x={}, y={}):", splicePath[0].x, splicePath[0].y);
            float angleSum = 0.0f;
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
                angleSum += angleDeg;

                TC_LOG_DEBUG("smooth", "splicePath vertex {} (x={}, y={}): angle (deg): {}", i, curr.x, curr.y, angleDeg);
            }
            TC_LOG_DEBUG("smooth", "splicePath vertex n (x={}, y={}): angleSum(deg): {}", splicePath.back().x, splicePath.back().y, angleSum);
        }

        splicePath.insert(splicePath.end(), modPath.begin(), modPath.end());
        init.MovebyPath(splicePath);
        //init.SetSmooth();
    }

    init.SetWalk(walk);
    init.Launch();

    ++_pathIndex;
    if (_pathIndex >= NUM_WANDER_POINTS)
        _pathIndex = 0;
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
            // Retail seems to let a creature walk 2 up to 10 splines before triggering a pause
            _wanderSteps = urand(1, ((_maxWanderDistance <= MIN_WANDER_DISTANCE) ? 2 : 8));
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
