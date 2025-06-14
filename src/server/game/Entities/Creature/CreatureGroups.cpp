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

CreatureGroup::CreatureGroup(ObjectGuid::LowType leaderSpawnId) : _leader(nullptr), _members(), _leaderSpawnId(leaderSpawnId), _engaging(false)
{
}

CreatureGroup::~CreatureGroup()
{
}

void CreatureGroup::AddMember(Creature* member)
{
    FormationInfo* formationInfo = ASSERT_NOTNULL(sFormationMgr->GetFormationInfo(member->GetSpawnId()));
    _members.emplace(member, formationInfo);
    member->SetFormation(this);

    if (_leader)
        member->Relocate(_leader->GetPosition());
}

void CreatureGroup::RemoveMember(Creature* member)
{
    _members.erase(member);
    member->SetFormation(nullptr);

    // If we remove the leader we need to reset the formation
    if (member == _leader)
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
    TC_LOG_DEBUG("formation", "CreatureGroup::FormationReset called {}", _leaderSpawnId);
    Creature* currentLeader = nullptr;
    Creature* defaultLeader = nullptr;
    Creature* firstAliveMember = nullptr;
    bool resetMemberMotion = false;
    for (auto const& pair : _members)
    {
        if (pair.first == _leader)
            currentLeader = pair.first;
        if (pair.first->GetSpawnId() == _leaderSpawnId)
            defaultLeader = pair.first;
        else if (!firstAliveMember && pair.first->IsAlive())
            firstAliveMember = pair.first;
    }
    
    // The CreatureGroup is not yet formed
    if (!_leader)
    {
        TC_LOG_DEBUG("formation", "Creature Group not yet formed {}", _leaderSpawnId);
        // Can only form a CreatureGroup initially when the defaultLeader is present and alive
        if (defaultLeader && defaultLeader->IsAlive())
        {
            TC_LOG_DEBUG("formation", "Default Leader found and alive, forming initial group {}", _leaderSpawnId);
            defaultLeader->GetMotionMaster()->Initialize();
            _leader = defaultLeader;
            resetMemberMotion = true;
        }
    }
    // The leader is the present defaultLeader
    else if (_leader == defaultLeader)
    {
        TC_LOG_DEBUG("formation", "Existing Group Leader is the Default Leader {}", _leaderSpawnId);
        // If the defaultLeader is dead we need to find a new leader
        if (!defaultLeader->IsAlive())
        {
            TC_LOG_DEBUG("formation", "Default Leader is dead, finding new leader {}", _leaderSpawnId);
            // Attempt to find a new leader
            if (firstAliveMember)
            {
                // If the defaultLeader has a waypoint path we need to copy the path and waypoint info to the new leader
                if (defaultLeader->GetWaypointPath())
                {
                    firstAliveMember->LoadPath(defaultLeader->GetWaypointPath());
                    firstAliveMember->UpdateCurrentWaypointInfo(defaultLeader->GetCurrentWaypointInfo().first, defaultLeader->GetCurrentWaypointInfo().second);
                    firstAliveMember->GetMotionMaster()->MovePath(firstAliveMember->GetWaypointPath(), true);
                }
                else
                    firstAliveMember->GetMotionMaster()->Initialize();

                _leader = firstAliveMember;
                resetMemberMotion = true;
            }
            else
            {
                _leader = nullptr;
                resetMemberMotion = true;
            }
        }
    }
    // The leader is or was a temporary leader
    else
    {
        TC_LOG_DEBUG("formation", "Existing Group Leader is or was a temporary Leader {}", _leaderSpawnId);
        Creature* newLeader = nullptr;
        // Always switch to the defaultLeader if it is present and alive
        if (defaultLeader && defaultLeader->IsAlive())
        {
            TC_LOG_DEBUG("formation", "Default Leader is present and alive, switching to default leader {}", _leaderSpawnId);
            newLeader = defaultLeader;
        }
        // Else if the leader was removed or newly dead switch to the first alive member (or null if none)
        else if (!currentLeader || !currentLeader->IsAlive())
        {
            TC_LOG_DEBUG("formation", "Current Leader is removed or newly dead, switching to first alive member {}", _leaderSpawnId);
            newLeader = firstAliveMember;
        }

        // If there is a a new leader
        if (newLeader)
        {
            // If the old leader has a waypoint path we need to copy the path and waypoint info to the new leader
            if (_leader->GetWaypointPath())
            {
                newLeader->UpdateCurrentWaypointInfo(_leader->GetCurrentWaypointInfo().first, _leader->GetCurrentWaypointInfo().second);
                newLeader->GetMotionMaster()->MovePath(_leader->GetWaypointPath(), true);
                // If not the default leader we can lear the path and waypoint info
                if (_leader->GetSpawnId() != _leaderSpawnId)
                {
                    _leader->LoadPath(0);
                    _leader->UpdateCurrentWaypointInfo(0, 0);
                }
            }
            else
                newLeader->GetMotionMaster()->Initialize();

            _leader = newLeader;
            resetMemberMotion = true;
        }
        else if (!currentLeader || !currentLeader->IsAlive())
        {
            TC_LOG_DEBUG("formation", "No new leader found, resetting formation {}", _leaderSpawnId);
            _leader = nullptr;
            resetMemberMotion = true;
        }
    }

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
