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
    constexpr int NUM_WANDER_PATHS = 12;
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

    // Create path for caching
    if (_paths.size() < NUM_WANDER_PATHS)
    {
        Movement::PointsArray path;
        // A path is constructed from two segments so that we can smooth the vertexes
        for (size_t i = 0; i < 2; ++i)
        {
            // Get the starting point for the segment
            Position src = i > 0 ? Vector3ToPosition(path.back()) : owner->GetPosition();
            Position dest = src;

            if (_paths.size() == NUM_WANDER_PATHS - 1)
            {
                // The last point is just the first point
                if (i == 1)
                {
                    G3D::Vector3& first = _paths.front().front();
                    dest.Relocate(first.x, first.y, first.z);
                }
                else
                {
                    // The second to last point is pre-calculated
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
            }
            // Second to last path
            else if (_paths.size() == NUM_WANDER_PATHS - 2)
            {
                // We always just walk straight for our first segment
                if (i == 0)
                {
                    float distance = frand(MIN_WANDER_DISTANCE, _maxWanderDistance);
                    float halfDistance = distance / 2.0f;
                    owner->MovePositionToFirstCollision(src, dest, halfDistance, 0.f);
                }
                // We need very specific points for the third and second to last point, this is because we want to complete
                // the circuit with no sharp angles, so we need to find the two next points that get us back
                // to the start without a sharp turn
                else
                {
                    Position first = Vector3ToPosition(_paths.front().front());

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
    
                    if (bestScore < std::numeric_limits<float>::max())
                    {
                        owner->MovePositionToFirstCollision(src, dest, minDist, bestAngleA);
                        Position realB = dest;
                        owner->MovePositionToFirstCollision(dest, realB, minDist, bestAngleB);
                        _cachedNextWanderPoint = realB;
                    }
                    // Fallback: random direction
                    else
                    {
                        bestAngleA = frand(-0.5 * M_PI, 0.5 * M_PI);
                        owner->MovePositionToFirstCollision(src, dest, minDist, bestAngleA);
                    }
                }
            }
            // Normal Random Path
            else
            {
                float distance = frand(MIN_WANDER_DISTANCE, _maxWanderDistance);
                float halfDistance = distance / 2.0f;
                float angle = 0.0f;
                // The first segment of our path is always straight, so leave angle as 0.0f
                // The second segment we need to determine where to go next
                if (i == 1)
                {
                    // Determine whether we should steer back towards the spawn point
                    float distanceFromSpawn = src.GetExactDist(_reference);
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
                    // Else pick a random 'forwardish' direction
                    else
                        angle = frand(-0.5f * M_PI, 0.5f * M_PI);
                }
                // Move accounting for collisions
                owner->MovePositionToFirstCollision(src, dest, halfDistance, angle);
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

            Movement::PointsArray tempPath = _pathGenerator->GetPath();
            if (tempPath.size() < 2)
            {
                _timer.Reset(100);
                ResetPaths();
                return;
            }

            if (i == 0)
            {
                path = tempPath;
                if (owner->GetSpawnId() == 80043)
                    TC_LOG_DEBUG("smooth", "First path Points: {}, Length: {}", tempPath.size(), PathGenerator::ComputePathLength(tempPath));
            }
            else
            {
                //path.insert(path.end(), tempPath.begin(), tempPath.end());
                if (owner->GetSpawnId() == 80043)
                    TC_LOG_DEBUG("smooth", "Second path Points: {}, Length: {}", tempPath.size(), PathGenerator::ComputePathLength(tempPath));

                // For debugging purposes move with no smoothing
                if (SMOOTH_CORNER_NUM_POINTS <= 1)
                    path.insert(path.end(), tempPath.begin(), tempPath.end());
                else
                {
                    // Truncate the back of the first path
                    Movement::PointsArray A = PathGenerator::TruncatePath(owner, path, SMOOTH_CORNER_RADIUS);
                    // Truncate the front of the second path
                    Movement::PointsArray C = PathGenerator::TruncatePath(owner, tempPath, SMOOTH_CORNER_RADIUS, true);
                    // Calculate an arc between the last point of A and the first point of C
                    Movement::PointsArray B = PathGenerator::SpliceAndSmoothArc(owner, A.back(), tempPath.front(), C.front(), SMOOTH_CORNER_NUM_POINTS);
                    // Splice the three paths together
                    path = Movement::PointsArray(A.begin(), A.end());
                    path.insert(path.end(), B.begin(), B.end());
                    path.insert(path.end(), C.begin(), C.end());
                    if (owner->GetSpawnId() == 80043)
                        TC_LOG_DEBUG("smooth", "Final path Points: {}, Length: {}", path.size(), PathGenerator::ComputePathLength(path));

                    if (owner->GetSpawnId() == 80043)
                    {
                        const G3D::Vector3& prev = A.back();
                        const G3D::Vector3& curr = tempPath.front();
                        const G3D::Vector3& next = C.front();

                        float v1x = curr.x - prev.x;
                        float v1y = curr.y - prev.y;
                        float v2x = next.x - curr.x;
                        float v2y = next.y - curr.y;

                        float v1Len = std::sqrt(v1x * v1x + v1y * v1y);
                        float v2Len = std::sqrt(v2x * v2x + v2y * v2y);

                        float dot = v1x * v2x + v1y * v2y;
                        float angleRad = std::acos(dot / (v1Len * v2Len));
                        float angleDeg = angleRad * (180.0f / M_PI);

                        TC_LOG_DEBUG("smooth", "base calculated A: {},{}, B: {},{}, C:{},{}, Angle (deg): {}", prev.x, prev.y, curr.x, curr.y, next.x, next.y, angleDeg);

                        TC_LOG_DEBUG("smooth", "splicePath vertex 0 (x={}, y={}):", B[0].x, B[0].y);
                        float angleSum = 0.0f;
                        for (size_t i = 1; i + 1 < B.size(); ++i)
                        {
                            const G3D::Vector3& prev = B[i - 1];
                            const G3D::Vector3& curr = B[i];
                            const G3D::Vector3& next = B[i + 1];

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
                        TC_LOG_DEBUG("smooth", "splicePath vertex n (x={}, y={}): angleSum(deg): {}", B.back().x, B.back().y, angleSum);
                    }
                }
            }
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
    init.MovebyPath(_paths[_pathIndex]);
    //init.SetSmooth();
    init.SetWalk(walk);
    init.Launch();

    ++_pathIndex;
    if (_pathIndex >= NUM_WANDER_PATHS)
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
