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

#include "CreatureGroups.h"
#include "Containers.h"
#include "Creature.h"
#include "CreatureAI.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "Map.h"
#include "MotionMaster.h"
#include "MovementGenerator.h"
#include "ObjectMgr.h"

#define MAX_DESYNC 5.0f

FormationMgr::FormationMgr()
{
}

FormationMgr::~FormationMgr()
{
}

FormationMgr* FormationMgr::instance()
{
    static FormationMgr instance;
    return &instance;
}

void FormationMgr::AddCreatureToGroup(ObjectGuid::LowType leaderSpawnId, Creature* creature)
{
    Map* map = creature->GetMap();

    auto itr = map->CreatureGroupHolder.find(leaderSpawnId);
    if (itr != map->CreatureGroupHolder.end())
    {
        //Add member to an existing group
        TC_LOG_DEBUG("entities.unit", "Group found: {}, inserting creature {}, Group InstanceID {}", leaderSpawnId, creature->GetGUID().ToString(), creature->GetInstanceId());

        // With dynamic spawn the creature may have just respawned
        // we need to find previous instance of creature and delete it from the formation, as it'll be invalidated
        auto bounds = Trinity::Containers::MapEqualRange(map->GetCreatureBySpawnIdStore(), creature->GetSpawnId());
        for (auto const& pair : bounds)
        {
            Creature* other = pair.second;
            if (other == creature)
                continue;

            if (itr->second->HasMember(other))
                itr->second->RemoveMember(other);
        }
    }
    else
    {
        //Create new group
        TC_LOG_DEBUG("entities.unit", "Group not found: {}. Creating new group.", leaderSpawnId);
        CreatureGroup* group = new CreatureGroup(leaderSpawnId);
        std::tie(itr, std::ignore) = map->CreatureGroupHolder.emplace(leaderSpawnId, group);
    }

    itr->second->AddMember(creature);
}

void FormationMgr::RemoveCreatureFromGroup(CreatureGroup* group, Creature* member)
{
    TC_LOG_DEBUG("entities.unit", "Deleting member pointer to GUID: {} from group {}", group->GetLeaderSpawnId(), member->GetSpawnId());
    group->RemoveMember(member);

    if (group->IsEmpty())
    {
        Map* map = member->GetMap();

        TC_LOG_DEBUG("entities.unit", "Deleting group with InstanceID {}", member->GetInstanceId());
        auto itr = map->CreatureGroupHolder.find(group->GetLeaderSpawnId());
        ASSERT(itr != map->CreatureGroupHolder.end(), "Not registered group %u in map %u", group->GetLeaderSpawnId(), map->GetId());
        map->CreatureGroupHolder.erase(itr);
        delete group;
    }
}

void FormationMgr::LoadCreatureFormations()
{
    uint32 oldMSTime = getMSTime();

    //Get group data
    QueryResult result = WorldDatabase.Query("SELECT leaderGUID, memberGUID, dist, angle, groupAI, point_1, point_2 FROM creature_formations ORDER BY leaderGUID");
    if (!result)
    {
        TC_LOG_INFO("server.loading", ">>  Loaded 0 creatures in formations. DB table `creature_formations` is empty!");
        return;
    }

    uint32 count = 0;
    std::unordered_set<ObjectGuid::LowType> leaderSpawnIds;
    do
    {
        Field* fields = result->Fetch();

        //Load group member data
        FormationInfo member;
        member.LeaderSpawnId              = fields[0].GetUInt32();
        ObjectGuid::LowType memberSpawnId = fields[1].GetUInt32();
        member.FollowDist                 = 0.f;
        member.FollowAngle                = 0.f;

        //If creature is group leader we may skip loading of dist/angle
        if (member.LeaderSpawnId != memberSpawnId)
        {
            member.FollowDist             = fields[2].GetFloat();
            member.FollowAngle            = fields[3].GetFloat() * float(M_PI) / 180.0f;
        }

        member.GroupAI                    = fields[4].GetUInt32();
        for (uint8 i = 0; i < 2; ++i)
            member.LeaderWaypointIDs[i]   = fields[5 + i].GetUInt16();

        // check data correctness
        {
            if (!sObjectMgr->GetCreatureData(member.LeaderSpawnId))
            {
                TC_LOG_ERROR("sql.sql", "creature_formations table leader guid {} incorrect (not exist)", member.LeaderSpawnId);
                continue;
            }

            if (!sObjectMgr->GetCreatureData(memberSpawnId))
            {
                TC_LOG_ERROR("sql.sql", "creature_formations table member guid {} incorrect (not exist)", memberSpawnId);
                continue;
            }

            leaderSpawnIds.insert(member.LeaderSpawnId);
        }

        _creatureGroupMap.emplace(memberSpawnId, std::move(member));
        ++count;
    } while (result->NextRow());

    for (ObjectGuid::LowType leaderSpawnId : leaderSpawnIds)
    {
        if (!_creatureGroupMap.count(leaderSpawnId))
        {
            TC_LOG_ERROR("sql.sql", "creature_formation contains leader spawn {} which is not included on its formation, removing", leaderSpawnId);
            for (auto itr = _creatureGroupMap.begin(); itr != _creatureGroupMap.end();)
            {
                if (itr->second.LeaderSpawnId == leaderSpawnId)
                {
                    itr = _creatureGroupMap.erase(itr);
                    continue;
                }

                ++itr;
            }
        }
    }

    TC_LOG_INFO("server.loading", ">> Loaded {} creatures in formations in {} ms", count, GetMSTimeDiffToNow(oldMSTime));
}

FormationInfo* FormationMgr::GetFormationInfo(ObjectGuid::LowType spawnId)
{
    return Trinity::Containers::MapGetValuePtr(_creatureGroupMap, spawnId);
}

void FormationMgr::AddFormationMember(ObjectGuid::LowType spawnId, float followAng, float followDist, ObjectGuid::LowType leaderSpawnId, uint32 groupAI)
{
    FormationInfo member;
    member.LeaderSpawnId = leaderSpawnId;
    member.FollowDist    = followDist;
    member.FollowAngle   = followAng;
    member.GroupAI       = groupAI;
    for (uint8 i = 0; i < 2; ++i)
        member.LeaderWaypointIDs[i] = 0;

    _creatureGroupMap.emplace(spawnId, std::move(member));
}

CreatureGroup::CreatureGroup(ObjectGuid::LowType leaderSpawnId) : _leader(nullptr), _members(), _leaderSpawnId(leaderSpawnId), _formed(false), _engaging(false)
{
}

CreatureGroup::~CreatureGroup()
{
}

void CreatureGroup::AddMember(Creature* member)
{
    // formation must be registered at this point
    FormationInfo* formationInfo = ASSERT_NOTNULL(sFormationMgr->GetFormationInfo(member->GetSpawnId()));
    _members.emplace(member, formationInfo);
    member->SetFormation(this);
    // we wait until Motion_Initialize to call FormationReset
}

void CreatureGroup::RemoveMember(Creature* member)
{
    _members.erase(member);
    member->SetFormation(nullptr);

    if (member->GetSpawnId() == _leaderSpawnId || member == _leader)
        FormationReset();
}

void CreatureGroup::MemberEngagingTarget(Creature* member, Unit* target)
{
    // used to prevent recursive calls
    if (_engaging)
        return;

    uint8 groupAI = ASSERT_NOTNULL(sFormationMgr->GetFormationInfo(member->GetSpawnId()))->GroupAI;
    if (!groupAI)
        return;

    if (member == _leader)
    {
        if (!(groupAI & FLAG_MEMBERS_ASSIST_LEADER))
            return;
    }
    else if (!(groupAI & FLAG_LEADER_ASSISTS_MEMBER))
        return;

    _engaging = true;

    for (auto const& pair : _members)
    {
        Creature* other = pair.first;
        if (other == member)
            continue;

        if (!other->IsAlive())
            continue;

        if (((other != _leader && (groupAI & FLAG_MEMBERS_ASSIST_LEADER)) || (other == _leader && (groupAI & FLAG_LEADER_ASSISTS_MEMBER))) && other->IsValidAttackTarget(target))
            other->EngageWithTarget(target);
    }

    _engaging = false;
}

// Smartly reset the CreatureGroup
bool CreatureGroup::FormationReset()
{
    Creature* defaultLeader = nullptr;
    Creature* firstAliveMember = nullptr;
    bool resetMemberMotion = false;
    for (auto const& pair : _members)
    {
        if (pair.first->GetSpawnId() == _leaderSpawnId)
            defaultLeader = pair.first;
        else if (!firstAliveMember && pair.first->IsAlive())
            firstAliveMember = pair.first;
    }

    // We can't handle groups without a default leader (from db)
    if (!defaultLeader)
    {
        // dismiss the group if their is a temporary leader
        if (_leader)
        {
            _leader = nullptr;
            resetMemberMotion = true;
        }
    }
    else if (defaultLeader->IsAlive())
    {
        // initial group formation we just need to set the leader, use default motion initialization for all other members
        if (!_leader || _leader == defaultLeader)
        {
            TC_LOG_DEBUG("formation", "Default Leader take initial leadership");
            _leader = defaultLeader;
        }
        else
        {
            TC_LOG_DEBUG("formation", "Temp Leader Alive {} Default Leader Distance to Temp Leader {} Temp Leader FollowDist {}", _leader->IsAlive(), defaultLeader->GetDistance(_leader), _members[_leader]->FollowDist);
            // switch leaders if temp leader is dead, or not a waypoint moveer, or if default leader is close enough to take leadership
            if (!_leader->IsAlive() || !_leader->GetWaypointPath() ||defaultLeader->GetDistance(_leader) < _members[_leader]->FollowDist + 3.0f)
            {
                TC_LOG_DEBUG("formation", "Default Leader take leadership from temp leader");
                // If a temporary leader exists AND has a waypoint path
                if (_leader->GetWaypointPath())
                {
                    // continue the previous leaders path
                    defaultLeader->UpdateCurrentWaypointInfo(_leader->GetCurrentWaypointInfo().first, _leader->GetCurrentWaypointInfo().second);
                    defaultLeader->GetMotionMaster()->MovePath(_leader->GetWaypointPath(), true, _leader->GetCurrentWaypointInfo().first);
                    // also reset the temporary leader path
                    _leader->LoadPath(0);
                    _leader->UpdateCurrentWaypointInfo(0, 0);
                }
                // else simply initialize the new leaders motion
                else
                {
                    defaultLeader->GetMotionMaster()->Initialize();
                }

                _leader = defaultLeader;
                resetMemberMotion = true;
            }
        }
    }
    // Else if no temporary leader exists or the temporary leader is newly dead
    else if (!_leader || !_leader->IsAlive())
    {
        // set first alive member as leader
        if (firstAliveMember)
        {
            // If the default leader has a waypoint path we need to copy the path and waypoint info
            if (defaultLeader->GetWaypointPath())
            {
                firstAliveMember->LoadPath(defaultLeader->GetWaypointPath());
                firstAliveMember->UpdateCurrentWaypointInfo(defaultLeader->GetCurrentWaypointInfo().first, defaultLeader->GetCurrentWaypointInfo().second);
                firstAliveMember->GetMotionMaster()->MovePath(firstAliveMember->GetWaypointPath(), true, firstAliveMember->GetCurrentWaypointInfo().first);
            }
            // else simply initialize the new leaders motion
            else
            {
                firstAliveMember->GetMotionMaster()->Initialize();
            }

            _leader = firstAliveMember;
            resetMemberMotion = true;
        }
        // we have noone left to take leader but we still consider formation as reset
        else if (_leader)
        {
            _leader = nullptr;
            resetMemberMotion = true;
        }
    }

    // We now consider the group as formed if there is a leader (default or temporary)
    _formed = _leader != nullptr;

    // Reset all other members motion when the formation is adjusted, this will get overridden when Leader signals to members
    if (resetMemberMotion)
    {
        for (auto const& pair : _members)
        {
            if (pair.first != _leader)
                pair.first->GetMotionMaster()->Initialize();
        }
    }

    // Return whether we adjusted the formation so that we know to not override motion set in this method
    return resetMemberMotion;
}

bool CreatureGroup::ShouldTakeLeadership(Creature* defaultLeader, Creature* temporaryLeader)
{
    if (!defaultLeader || !defaultLeader->IsAlive())
        return false;
    if (!temporaryLeader || !temporaryLeader->IsAlive())
        return true;

    
}

void CreatureGroup::LeaderStartedMoving()
{
    if (!_leader)
        return;

    for (auto const& pair : _members)
    {
        Creature* member = pair.first;
        if (member == _leader || !member->IsAlive() || member->IsEngaged() || !(pair.second->GroupAI & FLAG_IDLE_IN_FORMATION))
            continue;

        float angle = pair.second->FollowAngle + float(M_PI); // for some reason, someone thought it was a great idea to invert relativ angles...
        float dist = pair.second->FollowDist;

        if (!member->HasUnitState(UNIT_STATE_FOLLOW_FORMATION))
            member->GetMotionMaster()->MoveFormation(_leader, dist, angle, pair.second->LeaderWaypointIDs[0], pair.second->LeaderWaypointIDs[1]);
    }
}

bool CreatureGroup::CanLeaderStartMoving() const
{
    for (std::unordered_map<Creature*, FormationInfo*>::value_type const& pair : _members)
    {
        if (pair.first != _leader && pair.first->IsAlive())
        {
            if (pair.first->IsEngaged() || pair.first->IsReturningHome())
                return false;
        }
    }

    return true;
}
