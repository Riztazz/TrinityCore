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

#include "Player.h"
#include "AccountMgr.h"
#include "AchievementMgr.h"
#include "ArenaTeam.h"
#include "ArenaTeamMgr.h"
#include "Bag.h"
#include "Battlefield.h"
#include "BattlefieldMgr.h"
#include "Battleground.h"
#include "BattlegroundMgr.h"
#include "BattlegroundScore.h"
#include "CellImpl.h"
#include "Channel.h"
#include "ChannelMgr.h"
#include "CharacterCache.h"
#include "CharacterDatabaseCleaner.h"
#include "Chat.h"
#include "CinematicMgr.h"
#include "CombatLogPackets.h"
#include "CombatPackets.h"
#include "Common.h"
#include "ConditionMgr.h"
#include "Containers.h"
#include "CreatureAI.h"
#include "DBCEnums.h"
#include "DatabaseEnv.h"
#include "DisableMgr.h"
#include "Formulas.h"
#include "GameClient.h"
#include "GameEventMgr.h"
#include "GameObjectAI.h"
#include "GameTime.h"
#include "GitRevision.h"
#include "GossipDef.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "GuardMgr.h"
#include "Group.h"
#include "GroupMgr.h"
#include "Guild.h"
#include "GuildMgr.h"
#include "InstanceSaveMgr.h"
#include "InstanceScript.h"
#include "Item.h"
#include "KillRewarder.h"
#include "Language.h"
#include "LFGMgr.h"
#include "Log.h"
#include "LootItemStorage.h"
#include "LootMgr.h"
#include "Mail.h"
#include "MailPackets.h"
#include "MapManager.h"
#include "MiscPackets.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Opcodes.h"
#include "OutdoorPvP.h"
#include "OutdoorPvPMgr.h"
#include "Pet.h"
#include "PetitionMgr.h"
#include "PoolMgr.h"
#include "QueryHolder.h"
#include "QuestDef.h"
#include "QuestPools.h"
#include "Realm.h"
#include "ReputationMgr.h"
#include "SharedDefines.h"
#include "SkillDiscovery.h"
#include "SocialMgr.h"
#include "Spell.h"
#include "SpellAuraEffects.h"
#include "SpellAuras.h"
#include "SpellHistory.h"
#include "SpellMgr.h"
#include "SpellPackets.h"
#include "StringConvert.h"
#include "TalentPackets.h"
#include "TicketMgr.h"
#include "TradeData.h"
#include "Trainer.h"
// @tswow-begin (Using Rochet2/Transmog)
#include "Transmogrification.h"
// @tswow-end
#include "Transport.h"
#include "UpdateData.h"
#include "UpdateFieldFlags.h"
#include "UpdateMask.h"
#include "Util.h"
#include "Vehicle.h"
#include "Weather.h"
#include "WeatherMgr.h"
#include "World.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "WorldStatePackets.h"
// @tswow-begin
#include "TSProfile.h"
#include "TSEvents.h"
#include "TSFactionTemplate.h"
#include "TSQuest.h"
#include "TSPlayer.h"
#include "TSCreature.h"
#include "TSItem.h"
#include "TSGameObject.h"
#include "TSCorpse.h"
// @tswow-end
// @epoch-begin
#include "AnticheatMgr.h"
// @epoch-end

void Player::PrepareQuestMenu(ObjectGuid guid)
{
    QuestRelationResult objectQR;
    QuestRelationResult objectQIR;

    // pets also can have quests
    Creature* creature = ObjectAccessor::GetCreatureOrPetOrVehicle(*this, guid);
    if (creature)
    {
        objectQR  = sObjectMgr->GetCreatureQuestRelations(creature->GetEntry());
        objectQIR = sObjectMgr->GetCreatureQuestInvolvedRelations(creature->GetEntry());
    }
    else
    {
        //we should obtain map pointer from GetMap() in 99% of cases. Special case
        //only for quests which cast teleport spells on player
        Map* _map = IsInWorld() ? GetMap() : sMapMgr->FindMap(GetMapId(), GetPosition(), GetInstanceId());
        ASSERT(_map);
        GameObject* pGameObject = _map->GetGameObject(guid);
        if (pGameObject)
        {
            objectQR  = sObjectMgr->GetGOQuestRelations(pGameObject->GetEntry());
            objectQIR = sObjectMgr->GetGOQuestInvolvedRelations(pGameObject->GetEntry());
        }
        else
            return;
    }

    QuestMenu &qm = PlayerTalkClass->GetQuestMenu();
    qm.ClearMenu();

    for (uint32 quest_id : objectQIR)
    {
        QuestStatus status = GetQuestStatus(quest_id);
        if (status == QUEST_STATUS_COMPLETE)
            qm.AddMenuItem(quest_id, 4);
        else if (status == QUEST_STATUS_INCOMPLETE)
            qm.AddMenuItem(quest_id, 4);
        //else if (status == QUEST_STATUS_AVAILABLE)
        //    qm.AddMenuItem(quest_id, 2);
    }

    for (uint32 quest_id : objectQR)
    {
        Quest const* quest = sObjectMgr->GetQuestTemplate(quest_id);
        if (!quest)
            continue;

        if (!CanTakeQuest(quest, false))
            continue;

        if (quest->IsAutoComplete() && (!quest->IsRepeatable() || quest->IsDaily() || quest->IsWeekly() || quest->IsMonthly()))
            qm.AddMenuItem(quest_id, 0);
        else if (quest->IsAutoComplete())
            qm.AddMenuItem(quest_id, 4);
        else if (GetQuestStatus(quest_id) == QUEST_STATUS_NONE)
            qm.AddMenuItem(quest_id, 2);
    }
}

void Player::SendPreparedQuest(ObjectGuid guid)
{
    QuestMenu& questMenu = PlayerTalkClass->GetQuestMenu();
    if (questMenu.Empty())
        return;

    // single element case
    if (questMenu.GetMenuItemCount() == 1)
    {
        QuestMenuItem const& qmi0 = questMenu.GetItem(0);
        uint32 questId = qmi0.QuestId;

        // Auto open -- maybe also should verify there is no greeting
        if (Quest const* quest = sObjectMgr->GetQuestTemplate(questId))
        {
            if (qmi0.QuestIcon == 4)
                PlayerTalkClass->SendQuestGiverRequestItems(quest, guid, CanRewardQuest(quest, false), true);
            // Send completable on repeatable and autoCompletable quest if player don't have quest
            /// @todo verify if check for !quest->IsDaily() is really correct (possibly not)
            else
            {
                Object* object = ObjectAccessor::GetObjectByTypeMask(*this, guid, TYPEMASK_UNIT | TYPEMASK_GAMEOBJECT | TYPEMASK_ITEM);
                if (!object || (!object->hasQuest(questId) && !object->hasInvolvedQuest(questId)))
                {
                    PlayerTalkClass->SendCloseGossip();
                    return;
                }

                if (quest->IsAutoAccept() && CanAddQuest(quest, true) && CanTakeQuest(quest, true))
                    AddQuestAndCheckCompletion(quest, object);

                if ((quest->IsAutoComplete() && quest->IsRepeatable() && !quest->IsDailyOrWeekly()) || quest->HasFlag(QUEST_FLAGS_AUTOCOMPLETE))
                    PlayerTalkClass->SendQuestGiverRequestItems(quest, guid, CanCompleteRepeatableQuest(quest), true);
                else
                    PlayerTalkClass->SendQuestGiverQuestDetails(quest, guid, true);
            }
        }
    }
    // multiple entries
    else
    {
        QEmote qe;
        qe._Delay = 0;
        qe._Emote = 0;
        std::string title = "";

        // need pet case for some quests
        Creature* creature = ObjectAccessor::GetCreatureOrPetOrVehicle(*this, guid);
        if (creature)
        {
            uint32 textid = GetGossipTextId(creature);
            GossipText const* gossiptext = sObjectMgr->GetGossipText(textid);
            if (gossiptext)
            {
                qe = gossiptext->Options[0].Emotes[0];

                if (!gossiptext->Options[0].Text_0.empty())
                {
                    title = gossiptext->Options[0].Text_0;

                    LocaleConstant localeConstant = GetSession()->GetSessionDbLocaleIndex();
                    if (localeConstant != LOCALE_enUS)
                        if (NpcTextLocale const* nl = sObjectMgr->GetNpcTextLocale(textid))
                            ObjectMgr::GetLocaleString(nl->Text_0[0], localeConstant, title);
                }
                else
                {
                    title = gossiptext->Options[0].Text_1;

                    LocaleConstant localeConstant = GetSession()->GetSessionDbLocaleIndex();
                    if (localeConstant != LOCALE_enUS)
                        if (NpcTextLocale const* nl = sObjectMgr->GetNpcTextLocale(textid))
                            ObjectMgr::GetLocaleString(nl->Text_1[0], localeConstant, title);
                }
            }
        }
        PlayerTalkClass->SendQuestGiverQuestList(qe, title, guid);
    }
}

bool Player::IsActiveQuest(uint32 quest_id) const
{
    return m_QuestStatus.find(quest_id) != m_QuestStatus.end();
}

Quest const* Player::GetNextQuest(ObjectGuid guid, Quest const* quest) const
{
    QuestRelationResult quests;

    Creature* creature = ObjectAccessor::GetCreatureOrPetOrVehicle(*this, guid);
    if (creature)
        quests  = sObjectMgr->GetCreatureQuestRelations(creature->GetEntry());
    else
    {
        //we should obtain map pointer from GetMap() in 99% of cases. Special case
        //only for quests which cast teleport spells on player
        Map* _map = IsInWorld() ? GetMap() : sMapMgr->FindMap(GetMapId(), GetPosition(), GetInstanceId());
        ASSERT(_map);
        GameObject* pGameObject = _map->GetGameObject(guid);
        if (pGameObject)
            quests  = sObjectMgr->GetGOQuestRelations(pGameObject->GetEntry());
        else
            return nullptr;
    }

    if (uint32 nextQuestID = quest->GetNextQuestInChain())
        if (quests.HasQuest(nextQuestID))
            return sObjectMgr->GetQuestTemplate(nextQuestID);

    return nullptr;
}

bool Player::CanSeeStartQuest(Quest const* quest) const
{
    if (!DisableMgr::IsDisabledFor(DISABLE_TYPE_QUEST, quest->GetQuestId(), this) && SatisfyQuestClass(quest, false) && SatisfyQuestRace(quest, false) &&
        SatisfyQuestSkill(quest, false) && SatisfyQuestExclusiveGroup(quest, false) && SatisfyQuestReputation(quest, false) &&
        SatisfyQuestDependentQuests(quest, false) &&
        SatisfyQuestDay(quest, false) && SatisfyQuestWeek(quest, false) &&
        SatisfyQuestMonth(quest, false) && SatisfyQuestSeasonal(quest, false))
    {
        return GetLevel() + sWorld->getIntConfig(CONFIG_QUEST_HIGH_LEVEL_HIDE_DIFF) >= quest->GetMinLevel();
    }

    return false;
}

bool Player::CanTakeQuest(Quest const* quest, bool msg) const
{
    return !DisableMgr::IsDisabledFor(DISABLE_TYPE_QUEST, quest->GetQuestId(), this)
        && SatisfyQuestStatus(quest, msg) && SatisfyQuestExclusiveGroup(quest, msg)
        && SatisfyQuestClass(quest, msg) && SatisfyQuestRace(quest, msg) && SatisfyQuestLevel(quest, msg)
        && SatisfyQuestSkill(quest, msg) && SatisfyQuestReputation(quest, msg)
        && SatisfyQuestDependentQuests(quest, msg) && SatisfyQuestTimed(quest, msg)
        && SatisfyQuestDay(quest, msg) && SatisfyQuestWeek(quest, msg)
        && SatisfyQuestMonth(quest, msg) && SatisfyQuestSeasonal(quest, msg)
        && SatisfyQuestConditions(quest, msg);
}

bool Player::CanAddQuest(Quest const* quest, bool msg) const
{
    if (!SatisfyQuestLog(msg))
        return false;

    uint32 srcitem = quest->GetSrcItemId();
    if (srcitem > 0)
    {
        uint32 count = quest->GetSrcItemCount();
        ItemPosCountVec dest;
        InventoryResult msg2 = CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, srcitem, count);

        // player already have max number (in most case 1) source item, no additional item needed and quest can be added.
        if (msg2 == EQUIP_ERR_CANT_CARRY_MORE_OF_THIS)
            return true;
        if (msg2 != EQUIP_ERR_OK)
        {
            SendEquipError(msg2, nullptr, nullptr, srcitem);
            return false;
        }
    }
    return true;
}

bool Player::CanCompleteQuest(uint32 quest_id)
{
    if (quest_id)
    {
        Quest const* qInfo = sObjectMgr->GetQuestTemplate(quest_id);
        if (!qInfo)
            return false;

        if (!qInfo->IsRepeatable() && GetQuestRewardStatus(quest_id))
            return false;                                   // not allow re-complete quest

        // auto complete quest
        if ((qInfo->IsAutoComplete() || qInfo->GetFlags() & QUEST_FLAGS_AUTOCOMPLETE) && CanTakeQuest(qInfo, false))
            return true;

        QuestStatusMap::iterator itr = m_QuestStatus.find(quest_id);
        if (itr == m_QuestStatus.end())
            return false;

        QuestStatusData &q_status = itr->second;

        if (q_status.Status == QUEST_STATUS_INCOMPLETE)
        {
            if (qInfo->HasSpecialFlag(QUEST_SPECIAL_FLAGS_DELIVER))
            {
                for (uint8 i = 0; i < QUEST_ITEM_OBJECTIVES_COUNT; i++)
                {
                    if (qInfo->RequiredItemCount[i]!= 0 && q_status.ItemCount[i] < qInfo->RequiredItemCount[i])
                        return false;
                }
            }

            if (qInfo->HasSpecialFlag(QUEST_SPECIAL_FLAGS_KILL | QUEST_SPECIAL_FLAGS_CAST | QUEST_SPECIAL_FLAGS_SPEAKTO))
            {
                for (uint8 i = 0; i < QUEST_OBJECTIVES_COUNT; i++)
                {
                    if (qInfo->RequiredNpcOrGo[i] == 0)
                        continue;

                    if (qInfo->RequiredNpcOrGoCount[i] != 0 && q_status.CreatureOrGOCount[i] < qInfo->RequiredNpcOrGoCount[i])
                        return false;
                }
            }

            if (qInfo->HasSpecialFlag(QUEST_SPECIAL_FLAGS_PLAYER_KILL))
                if (qInfo->GetPlayersSlain() != 0 && q_status.PlayerCount < qInfo->GetPlayersSlain())
                    return false;

            if (qInfo->HasSpecialFlag(QUEST_SPECIAL_FLAGS_EXPLORATION_OR_EVENT) && !q_status.Explored)
                return false;

            if (qInfo->HasSpecialFlag(QUEST_SPECIAL_FLAGS_TIMED) && q_status.Timer == 0)
                return false;

            if (qInfo->GetRewOrReqMoney() < 0)
            {
                if (!HasEnoughMoney(-qInfo->GetRewOrReqMoney()))
                    return false;
            }

            uint32 repFacId = qInfo->GetRepObjectiveFaction();
            if (repFacId && GetReputationMgr().GetReputation(repFacId) < qInfo->GetRepObjectiveValue())
                return false;

            return true;
        }
    }
    return false;
}

bool Player::CanCompleteRepeatableQuest(Quest const* quest)
{
    // Solve problem that player don't have the quest and try complete it.
    // if repeatable she must be able to complete event if player don't have it.
    // Seem that all repeatable quest are DELIVER Flag so, no need to add more.
    if (!CanTakeQuest(quest, false))
        return false;

    if (quest->HasSpecialFlag(QUEST_SPECIAL_FLAGS_DELIVER))
        for (uint8 i = 0; i < QUEST_ITEM_OBJECTIVES_COUNT; i++)
            if (quest->RequiredItemId[i] && quest->RequiredItemCount[i] && !HasItemCount(quest->RequiredItemId[i], quest->RequiredItemCount[i]))
                return false;

    if (!CanRewardQuest(quest, false))
        return false;

    return true;
}

bool Player::CanRewardQuest(Quest const* quest, bool msg)
{
    // quest is disabled
    if (DisableMgr::IsDisabledFor(DISABLE_TYPE_QUEST, quest->GetQuestId(), this))
        return false;

    // not auto complete quest and not completed quest (only cheating case, then ignore without message)
    if (!quest->IsDFQuest() && !quest->IsAutoComplete() && !(quest->GetFlags() & QUEST_FLAGS_AUTOCOMPLETE) && GetQuestStatus(quest->GetQuestId()) != QUEST_STATUS_COMPLETE)
        return false;

    // daily quest can't be rewarded (25 daily quest already completed)
    if (!SatisfyQuestDay(quest, msg) || !SatisfyQuestWeek(quest, msg) || !SatisfyQuestMonth(quest, msg) || !SatisfyQuestSeasonal(quest, msg))
        return false;

    // player no longer satisfies the quest's requirements (skill level etc.)
    if (!SatisfyQuestLevel(quest, msg) || !SatisfyQuestSkill(quest, msg) || !SatisfyQuestReputation(quest, msg))
        return false;

    // rewarded and not repeatable quest (only cheating case, then ignore without message)
    if (GetQuestRewardStatus(quest->GetQuestId()))
        return false;

    // prevent receive reward with quest items in bank
    if (quest->HasSpecialFlag(QUEST_SPECIAL_FLAGS_DELIVER))
    {
        for (uint8 i = 0; i < QUEST_ITEM_OBJECTIVES_COUNT; i++)
        {
            if (quest->RequiredItemCount[i]!= 0 &&
                GetItemCount(quest->RequiredItemId[i]) < quest->RequiredItemCount[i])
            {
                if (msg)
                    SendEquipError(EQUIP_ERR_ITEM_NOT_FOUND, nullptr, nullptr, quest->RequiredItemId[i]);
                return false;
            }
        }
    }

    // prevent receive reward with low money and GetRewOrReqMoney() < 0
    if (quest->GetRewOrReqMoney() < 0 && !HasEnoughMoney(-quest->GetRewOrReqMoney()))
        return false;

    return true;
}

void Player::AddQuestAndCheckCompletion(Quest const* quest, Object* questGiver)
{
    AddQuest(quest, questGiver);

    if (CanCompleteQuest(quest->GetQuestId()))
        CompleteQuest(quest->GetQuestId());

    if (!questGiver)
        return;

    switch (questGiver->GetTypeId())
    {
        case TYPEID_UNIT:
            PlayerTalkClass->ClearMenus();
            // @tswow-begin
            FIRE_ID(questGiver->ToCreature()->GetCreatureTemplate()->events.id,Creature,OnQuestAccept,TSCreature(questGiver->ToCreature()),TSPlayer(this),TSQuest(quest));
            FIRE_ID(
                  quest->events.id
                , Quest,OnAccept
                , TSQuest(quest)
                , TSPlayer(this)
                , TSObject(questGiver)
            );
            // @tswow-end
            questGiver->ToCreature()->AI()->OnQuestAccept(this, quest);
            break;
        case TYPEID_ITEM:
        case TYPEID_CONTAINER:
        {
            Item* item = static_cast<Item*>(questGiver);
            // @tswow-begin
            FIRE_ID(item->GetTemplate()->events.id,Item,OnQuestAccept,TSItem(item),TSPlayer(this),TSQuest(quest));
            FIRE_ID(
                quest->events.id
                , Quest,OnAccept
                , TSQuest(quest)
                , TSPlayer(this)
                , TSObject(item)
            );
            // @tswow-end
            sScriptMgr->OnQuestAccept(this, item, quest);

            // There are two cases where the source item is not destroyed when the quest is accepted:
            // - It is required to finish the quest, and is an unique item
            // - It is the same item present in the source item field (item that would be given on quest accept)
            bool destroyItem = true;

            for (int i = 0; i < QUEST_ITEM_OBJECTIVES_COUNT; ++i)
            {
                if (quest->RequiredItemId[i] == item->GetEntry() && item->GetTemplate()->MaxCount > 0)
                {
                    destroyItem = false;
                    break;
                }
            }

            if (quest->GetSrcItemId() == item->GetEntry())
                destroyItem = false;

            if (destroyItem)
                DestroyItem(item->GetBagSlot(), item->GetSlot(), true);

            break;
        }
        case TYPEID_GAMEOBJECT:
            PlayerTalkClass->ClearMenus();
            // @tswow-begin
            FIRE_ID(questGiver->ToGameObject()->GetGOInfo()->events.id,GameObject,OnQuestAccept,TSGameObject(questGiver->ToGameObject()),TSPlayer(this),TSQuest(quest));
            FIRE_ID(
                  quest->events.id
                , Quest,OnAccept
                , TSQuest(quest)
                , TSPlayer(this)
                , TSObject(questGiver)
            );
            // @tswow-end
            questGiver->ToGameObject()->AI()->OnQuestAccept(this, quest);
            break;
        default:
            break;
    }
}

bool Player::CanRewardQuest(Quest const* quest, uint32 reward, bool msg)
{
    ItemPosCountVec dest;
    if (quest->GetRewChoiceItemsCount() > 0)
    {
        if (quest->RewardChoiceItemId[reward])
        {
            InventoryResult res = CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, quest->RewardChoiceItemId[reward], quest->RewardChoiceItemCount[reward]);
            if (res != EQUIP_ERR_OK)
            {
                if (msg)
                    SendQuestFailed(quest->GetQuestId(), res);

                return false;
            }
        }
    }

    if (quest->GetRewItemsCount() > 0)
    {
        for (uint32 i = 0; i < quest->GetRewItemsCount(); ++i)
        {
            if (quest->RewardItemId[i])
            {
                InventoryResult res = CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, quest->RewardItemId[i], quest->RewardItemIdCount[i]);
                if (res != EQUIP_ERR_OK)
                {
                    if (msg)
                        SendQuestFailed(quest->GetQuestId(), res);

                    return false;
                }
            }
        }
    }

    return true;
}

void Player::AddQuest(Quest const* quest, Object* questGiver)
{
    uint16 log_slot = FindQuestSlot(0);

    if (log_slot >= MAX_QUEST_LOG_SIZE) // Player does not have any free slot in the quest log
        return;

    uint32 quest_id = quest->GetQuestId();

    // if not exist then created with set uState == NEW and rewarded=false
    QuestStatusData& questStatusData = m_QuestStatus[quest_id];

    // check for repeatable quests status reset
    questStatusData.Status = QUEST_STATUS_INCOMPLETE;
    questStatusData.Explored = false;

    if (quest->HasSpecialFlag(QUEST_SPECIAL_FLAGS_DELIVER))
    {
        for (uint8 i = 0; i < QUEST_ITEM_OBJECTIVES_COUNT; ++i)
            questStatusData.ItemCount[i] = 0;
    }

    if (quest->HasSpecialFlag(QUEST_SPECIAL_FLAGS_KILL | QUEST_SPECIAL_FLAGS_CAST | QUEST_SPECIAL_FLAGS_SPEAKTO))
    {
        for (uint8 i = 0; i < QUEST_OBJECTIVES_COUNT; ++i)
            questStatusData.CreatureOrGOCount[i] = 0;
    }

    if (quest->HasSpecialFlag(QUEST_SPECIAL_FLAGS_PLAYER_KILL))
        questStatusData.PlayerCount = 0;

    GiveQuestSourceItem(quest);
    AdjustQuestReqItemCount(quest, questStatusData);

    if (quest->GetRepObjectiveFaction())
        if (FactionEntry const* factionEntry = sFactionStore.LookupEntry(quest->GetRepObjectiveFaction()))
            GetReputationMgr().SetVisible(factionEntry);

    if (quest->GetRepObjectiveFaction2())
        if (FactionEntry const* factionEntry = sFactionStore.LookupEntry(quest->GetRepObjectiveFaction2()))
            GetReputationMgr().SetVisible(factionEntry);

    uint32 qtime = 0;
    if (quest->HasSpecialFlag(QUEST_SPECIAL_FLAGS_TIMED))
    {
        uint32 timeAllowed = quest->GetTimeAllowed();

        // shared timed quest
        if (questGiver && questGiver->GetTypeId() == TYPEID_PLAYER)
            timeAllowed = questGiver->ToPlayer()->getQuestStatusMap()[quest_id].Timer / IN_MILLISECONDS;

        AddTimedQuest(quest_id);
        questStatusData.Timer = timeAllowed * IN_MILLISECONDS;
        qtime = static_cast<uint32>(GameTime::GetGameTime()) + timeAllowed;
    }
    else
        questStatusData.Timer = 0;

    if (quest->HasFlag(QUEST_FLAGS_FLAGS_PVP))
    {
        pvpInfo.IsHostile = true;
        UpdatePvPState();
    }

    SetQuestSlot(log_slot, quest_id, qtime);

    m_QuestStatusSave[quest_id] = QUEST_DEFAULT_SAVE_TYPE;

    StartTimedAchievement(ACHIEVEMENT_TIMED_TYPE_QUEST, quest_id);

    SendQuestUpdate(quest_id);

    if (sWorld->getBoolConfig(CONFIG_QUEST_ENABLE_QUEST_TRACKER)) // check if Quest Tracker is enabled
    {
        // prepare Quest Tracker datas
        CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_INS_QUEST_TRACK);
        stmt->setUInt32(0, quest_id);
        stmt->setUInt32(1, GetGUID().GetCounter());
        stmt->setString(2, GitRevision::GetHash());
        stmt->setString(3, GitRevision::GetDate());

        // add to Quest Tracker
        CharacterDatabase.Execute(stmt);
    }

    // @tswow-begin
    FIRE_ID(
          quest->events.id
        , Quest,OnStatusChanged
        , TSQuest(quest)
        , TSPlayer(this)
    );
    // @tswow-end
    sScriptMgr->OnQuestStatusChange(this, quest_id);
}

void Player::CompleteQuest(uint32 quest_id)
{
    if (quest_id)
    {
        SetQuestStatus(quest_id, QUEST_STATUS_COMPLETE);

        uint16 log_slot = FindQuestSlot(quest_id);
        if (log_slot < MAX_QUEST_LOG_SIZE)
            SetQuestSlotState(log_slot, QUEST_STATE_COMPLETE);

        if (Quest const* qInfo = sObjectMgr->GetQuestTemplate(quest_id))
            if (qInfo->HasFlag(QUEST_FLAGS_TRACKING))
                RewardQuest(qInfo, 0, this, false);
    }

    if (sWorld->getBoolConfig(CONFIG_QUEST_ENABLE_QUEST_TRACKER)) // check if Quest Tracker is enabled
    {
        // prepare Quest Tracker data
        CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_QUEST_TRACK_COMPLETE_TIME);
        stmt->setUInt32(0, quest_id);
        stmt->setUInt32(1, GetGUID().GetCounter());

        // add to Quest Tracker
        CharacterDatabase.Execute(stmt);
    }
}

void Player::IncompleteQuest(uint32 quest_id)
{
    if (quest_id)
    {
        SetQuestStatus(quest_id, QUEST_STATUS_INCOMPLETE);

        uint16 log_slot = FindQuestSlot(quest_id);
        if (log_slot < MAX_QUEST_LOG_SIZE)
            RemoveQuestSlotState(log_slot, QUEST_STATE_COMPLETE);
    }
}

void Player::RewardQuest(Quest const* quest, uint32 reward, Object* questGiver, bool announce)
{
    //this THING should be here to protect code from quest, which cast on player far teleport as a reward
    //should work fine, cause far teleport will be executed in Player::Update()
    SetCanDelayTeleport(true);

    uint32 quest_id = quest->GetQuestId();

    for (uint8 i = 0; i < QUEST_ITEM_OBJECTIVES_COUNT; ++i)
    {
        if (ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(quest->RequiredItemId[i]))
        {
            if (quest->RequiredItemCount[i] > 0 && itemTemplate->Bonding == BIND_QUEST_ITEM && !quest->IsRepeatable() && !HasQuestForItem(quest->RequiredItemId[i], quest_id, true))
                DestroyItemCount(quest->RequiredItemId[i], 9999, true);
            else
                DestroyItemCount(quest->RequiredItemId[i], quest->RequiredItemCount[i], true);
        }
    }
    for (uint8 i = 0; i < QUEST_SOURCE_ITEM_IDS_COUNT; ++i)
    {
        if (ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(quest->ItemDrop[i]))
        {
            if (quest->ItemDropQuantity[i] > 0 && itemTemplate->Bonding == BIND_QUEST_ITEM && !quest->IsRepeatable() && !HasQuestForItem(quest->ItemDrop[i], quest_id))
                DestroyItemCount(quest->ItemDrop[i], 9999, true);
            else
                DestroyItemCount(quest->ItemDrop[i], quest->ItemDropQuantity[i], true);
        }
    }

    RemoveTimedQuest(quest_id);

    if (quest->GetRewItemsCount() > 0)
    {
        for (uint32 i = 0; i < quest->GetRewItemsCount(); ++i)
        {
            if (uint32 itemId = quest->RewardItemId[i])
            {
                ItemPosCountVec dest;
                if (CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, itemId, quest->RewardItemIdCount[i]) == EQUIP_ERR_OK)
                {
                    Item* item = StoreNewItem(dest, itemId, true, GenerateItemRandomPropertyId(itemId));
                    SendNewItem(item, quest->RewardItemIdCount[i], true, false, false, false);
                }
                else if (quest->IsDFQuest())
                    SendItemRetrievalMail(itemId, quest->RewardItemIdCount[i]);
            }
        }
    }

    if (quest->GetRewChoiceItemsCount() > 0)
    {
        if (uint32 itemId = quest->RewardChoiceItemId[reward])
        {
            ItemPosCountVec dest;
            if (CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, itemId, quest->RewardChoiceItemCount[reward]) == EQUIP_ERR_OK)
            {
                Item* item = StoreNewItem(dest, itemId, true, GenerateItemRandomPropertyId(itemId));
                SendNewItem(item, quest->RewardChoiceItemCount[reward], true, false, false, false);
            }
        }
    }

    uint16 log_slot = FindQuestSlot(quest_id);
    if (log_slot < MAX_QUEST_LOG_SIZE)
        SetQuestSlot(log_slot, 0);

    bool rewarded = IsQuestRewarded(quest_id) && !quest->IsDFQuest();

    // Not give XP in case already completed once repeatable quest
    uint32 XP = rewarded ? 0 : uint32(quest->GetXPReward(this)*sWorld->getRate(RATE_XP_QUEST));

    // handle SPELL_AURA_MOD_XP_QUEST_PCT auras
    XP *= GetTotalAuraMultiplier(SPELL_AURA_MOD_XP_QUEST_PCT);
    // @tswow-begin
    FIRE_ID(
          quest->events.id
        , Quest,OnRewardXP
        , TSQuest(quest)
        , TSPlayer(this)
        , TSMutableNumber<uint32>(&XP)
    );
    // @tswow-end

    if (!IsMaxLevel())
        GiveXP(XP, nullptr);

    // Give player extra money if GetRewOrReqMoney > 0 and get ReqMoney if negative
    if (int32 moneyRew = quest->GetRewOrReqMoney(this))
    {
        ModifyMoney(moneyRew);

        if (moneyRew > 0)
            UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_MONEY_FROM_QUEST_REWARD, uint32(moneyRew));
    }

    // honor reward
    if (uint32 honor = quest->CalculateHonorGain(GetLevel()))
        RewardHonor(nullptr, 0, honor);

    // title reward
    if (quest->GetCharTitleId())
    {
        if (CharTitlesEntry const* titleEntry = sCharTitlesStore.LookupEntry(quest->GetCharTitleId()))
            SetTitle(titleEntry);
    }

    // @tswow-begin add permanent talents and move init logic
    if (quest->GetBonusTalents())
    {
        m_questRewardTalentCount += quest->GetBonusTalents();
    }

    if (quest->GetBonusTalentsPerm())
    {
        m_questRewardPermTalentCount += quest->GetBonusTalentsPerm();
    }

    if (quest->GetBonusTalents() || quest->GetBonusTalentsPerm())
    {
        InitTalentForLevel();
    }
    // @tswow-end


    if (quest->GetRewArenaPoints())
        ModifyArenaPoints(quest->GetRewArenaPoints());

    // Send reward mail
    if (uint32 mail_template_id = quest->GetRewMailTemplateId())
    {
        /// @todo Poor design of mail system
        CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
        if (uint32 questMailSender = quest->GetRewMailSenderEntry())
            MailDraft(mail_template_id).SendMailTo(trans, this, questMailSender, MAIL_CHECK_MASK_HAS_BODY, quest->GetRewMailDelaySecs());
        else
            MailDraft(mail_template_id).SendMailTo(trans, this, questGiver, MAIL_CHECK_MASK_HAS_BODY, quest->GetRewMailDelaySecs());
        CharacterDatabase.CommitTransaction(trans);
    }

    if (quest->IsDaily() || quest->IsDFQuest())
    {
        SetDailyQuestStatus(quest_id);
        if (quest->IsDaily())
        {
            UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_COMPLETE_DAILY_QUEST, quest_id);
            UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_COMPLETE_DAILY_QUEST_DAILY, quest_id);
        }
    }
    else if (quest->IsWeekly())
        SetWeeklyQuestStatus(quest_id);
    else if (quest->IsMonthly())
        SetMonthlyQuestStatus(quest_id);
    else if (quest->IsSeasonal())
        SetSeasonalQuestStatus(quest_id);

    RemoveActiveQuest(quest_id, false);
    if (quest->CanIncreaseRewardedQuestCounters())
        SetRewardedQuest(quest_id);

    if (announce)
        SendQuestReward(quest, XP);

    RewardReputation(quest);

    // cast spells after mark quest complete (some spells have quest completed state requirements in spell_area data)
    if (quest->GetRewSpellCast() > 0)
    {
        SpellInfo const* spellInfo = ASSERT_NOTNULL(sSpellMgr->GetSpellInfo(quest->GetRewSpellCast()));
        if (questGiver->IsUnit() && !spellInfo->HasEffect(SPELL_EFFECT_LEARN_SPELL) && !spellInfo->HasEffect(SPELL_EFFECT_CREATE_ITEM) && !spellInfo->IsSelfCast())
        {
            if (Creature* creature = GetMap()->GetCreature(questGiver->GetGUID()))
                creature->CastSpell(this, quest->GetRewSpellCast(), true);
        }
        else
            CastSpell(this, quest->GetRewSpellCast(), true);
    }
    else if (quest->GetRewSpell() > 0)
    {
        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(quest->GetRewSpell());
        if (questGiver->IsUnit() && !spellInfo->HasEffect(SPELL_EFFECT_LEARN_SPELL) && !spellInfo->HasEffect(SPELL_EFFECT_CREATE_ITEM) && !spellInfo->IsSelfCast())
        {
            if (Creature* creature = GetMap()->GetCreature(questGiver->GetGUID()))
                creature->CastSpell(this, quest->GetRewSpell(), true);
        }
        else
            CastSpell(this, quest->GetRewSpell(), true);
    }

    if (quest->GetZoneOrSort() > 0)
        UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_COMPLETE_QUESTS_IN_ZONE, quest->GetZoneOrSort());
    UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_COMPLETE_QUEST_COUNT);
    UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_COMPLETE_QUEST, quest->GetQuestId());

    // make full db save
    SaveToDB(false);

    if (quest->HasFlag(QUEST_FLAGS_FLAGS_PVP))
    {
        pvpInfo.IsHostile = pvpInfo.IsInHostileArea || HasPvPForcingQuest();
        UpdatePvPState();
    }

    SendQuestUpdate(quest_id);

    SendQuestGiverStatusMultiple();

    //lets remove flag for delayed teleports
    SetCanDelayTeleport(false);

    // @tswow-begin
    FIRE_ID(
        quest->events.id
        , Quest,OnStatusChanged
        , TSQuest(quest)
        , TSPlayer(this)
    );
    // @tswow-end
    sScriptMgr->OnQuestStatusChange(this, quest_id);
}

void Player::SetRewardedQuest(uint32 quest_id)
{
    m_RewardedQuests.insert(quest_id);
    m_RewardedQuestsSave[quest_id] = QUEST_DEFAULT_SAVE_TYPE;
}

void Player::FailQuest(uint32 questId)
{
    if (Quest const* quest = sObjectMgr->GetQuestTemplate(questId))
    {
        QuestStatus qStatus = GetQuestStatus(questId);

        // we can only fail incomplete quest or...
        if (qStatus != QUEST_STATUS_INCOMPLETE)
        {
            // completed timed quest with no requirements
            if (qStatus != QUEST_STATUS_COMPLETE || !quest->HasSpecialFlag(QUEST_SPECIAL_FLAGS_TIMED) || !quest->HasSpecialFlag(QUEST_SPECIAL_FLAGS_COMPLETED_AT_START))
                return;
        }

        SetQuestStatus(questId, QUEST_STATUS_FAILED);

        uint16 log_slot = FindQuestSlot(questId);

        if (log_slot < MAX_QUEST_LOG_SIZE)
        {
            SetQuestSlotTimer(log_slot, 1);
            SetQuestSlotState(log_slot, QUEST_STATE_FAIL);
        }

        if (quest->HasSpecialFlag(QUEST_SPECIAL_FLAGS_TIMED))
        {
            QuestStatusData& q_status = m_QuestStatus[questId];

            RemoveTimedQuest(questId);
            q_status.Timer = 0;

            SendQuestTimerFailed(questId);
        }
        else
            SendQuestFailed(questId);

        // Destroy quest items on quest failure.
        for (uint8 i = 0; i < QUEST_ITEM_OBJECTIVES_COUNT; ++i)
            if (ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(quest->RequiredItemId[i]))
                if (quest->RequiredItemCount[i] > 0 && itemTemplate->Bonding == BIND_QUEST_ITEM)
                    DestroyItemCount(quest->RequiredItemId[i], 9999, true);

        for (uint8 i = 0; i < QUEST_SOURCE_ITEM_IDS_COUNT; ++i)
            if (ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(quest->ItemDrop[i]))
                if (quest->ItemDropQuantity[i] > 0 && itemTemplate->Bonding == BIND_QUEST_ITEM)
                    DestroyItemCount(quest->ItemDrop[i], 9999, true);
    }
}

void Player::AbandonQuest(uint32 questId)
{
    if (Quest const* quest = sObjectMgr->GetQuestTemplate(questId))
    {
        // Destroy quest items on quest abandon.
        for (uint8 i = 0; i < QUEST_ITEM_OBJECTIVES_COUNT; ++i)
            if (ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(quest->RequiredItemId[i]))
                if (quest->RequiredItemCount[i] > 0 && itemTemplate->Bonding == BIND_QUEST_ITEM)
                    DestroyItemCount(quest->RequiredItemId[i], 9999, true);

        for (uint8 i = 0; i < QUEST_SOURCE_ITEM_IDS_COUNT; ++i)
            if (ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(quest->ItemDrop[i]))
                if (quest->ItemDropQuantity[i] > 0 && itemTemplate->Bonding == BIND_QUEST_ITEM)
                    DestroyItemCount(quest->ItemDrop[i], 9999, true);
    }
}

bool Player::SatisfyQuestSkill(Quest const* qInfo, bool msg) const
{
    uint32 skill = qInfo->GetRequiredSkill();

    // skip 0 case RequiredSkill
    if (skill == 0)
        return true;

    // check skill value
    if (GetSkillValue(skill) < qInfo->GetRequiredSkillValue())
    {
        if (msg)
        {
            SendCanTakeQuestResponse(INVALIDREASON_DONT_HAVE_REQ);
            TC_LOG_DEBUG("misc", "Player::SatisfyQuestSkill: Sent INVALIDREASON_DONT_HAVE_REQ (QuestID: {}) because player '{}' ({}) doesn't have the required skill value.",
                qInfo->GetQuestId(), GetName(), GetGUID().ToString());
        }

        return false;
    }

    return true;
}

bool Player::SatisfyQuestLevel(Quest const* qInfo, bool msg) const
{
    if (GetLevel() < qInfo->GetMinLevel())
    {
        if (msg)
        {
            SendCanTakeQuestResponse(INVALIDREASON_QUEST_FAILED_LOW_LEVEL);
            TC_LOG_DEBUG("misc", "Player::SatisfyQuestLevel: Sent INVALIDREASON_QUEST_FAILED_LOW_LEVEL (QuestID: {}) because player '{}' ({}) doesn't have the required (min) level.",
                qInfo->GetQuestId(), GetName(), GetGUID().ToString());
        }
        return false;
    }

    if (qInfo->GetMaxLevel() > 0 && GetLevel() > qInfo->GetMaxLevel())
    {
        if (msg)
        {
            SendCanTakeQuestResponse(INVALIDREASON_DONT_HAVE_REQ); // There doesn't seem to be a specific response for too high player level
            TC_LOG_DEBUG("misc", "Player::SatisfyQuestLevel: Sent INVALIDREASON_DONT_HAVE_REQ (QuestID: {}) because player '{}' ({}) doesn't have the required (max) level.",
                qInfo->GetQuestId(), GetName(), GetGUID().ToString());
        }
        return false;
    }
    return true;
}

bool Player::SatisfyQuestLog(bool msg) const
{
    // exist free slot
    if (FindQuestSlot(0) < MAX_QUEST_LOG_SIZE)
        return true;

    if (msg)
    {
        WorldPacket data(SMSG_QUESTLOG_FULL, 0);
        SendDirectMessage(&data);
    }
    return false;
}

bool Player::SatisfyQuestDependentQuests(Quest const* qInfo, bool msg) const
{
    return SatisfyQuestPreviousQuest(qInfo, msg) && SatisfyQuestDependentPreviousQuests(qInfo, msg) &&
           SatisfyQuestBreadcrumbQuest(qInfo, msg) && SatisfyQuestDependentBreadcrumbQuests(qInfo, msg);
}

bool Player::SatisfyQuestPreviousQuest(Quest const* qInfo, bool msg) const
{
    // No previous quest (might be first quest in a series)
    if (!qInfo->GetPrevQuestId())
        return true;

    uint32 prevId = std::abs(qInfo->GetPrevQuestId());
    // If positive previous quest rewarded, return true
    if (qInfo->GetPrevQuestId() > 0 && m_RewardedQuests.count(prevId) > 0)
        return true;

    // If negative previous quest active, return true
    if (qInfo->GetPrevQuestId() < 0 && GetQuestStatus(prevId) == QUEST_STATUS_INCOMPLETE)
        return true;

    // Has positive prev. quest in non-rewarded state
    // and negative prev. quest in non-active state
    if (msg)
    {
        SendCanTakeQuestResponse(INVALIDREASON_DONT_HAVE_REQ);
        TC_LOG_DEBUG("misc", "Player::SatisfyQuestPreviousQuest: Sent INVALIDREASON_DONT_HAVE_REQ (QuestID: {}) because player '{}' ({}) doesn't have required quest {}.",
            qInfo->GetQuestId(), GetName(), GetGUID().ToString(), prevId);
    }

    return false;
}

bool Player::SatisfyQuestDependentPreviousQuests(Quest const* qInfo, bool msg) const
{
    // No previous quest (might be first quest in a series)
    if (qInfo->DependentPreviousQuests.empty())
        return true;

    for (uint32 prevId : qInfo->DependentPreviousQuests)
    {
        // checked in startup
        Quest const* questInfo = sObjectMgr->GetQuestTemplate(prevId);
        ASSERT(questInfo);

        // If any of the previous quests completed, return true
        if (IsQuestRewarded(prevId))
        {
            // skip one-from-all exclusive group
            if (questInfo->GetExclusiveGroup() >= 0)
                return true;

            // each-from-all exclusive group (< 0)
            // can be start if only all quests in prev quest exclusive group completed and rewarded
            auto bounds = sObjectMgr->GetExclusiveQuestGroupBounds(questInfo->GetExclusiveGroup());
            for (auto itr = bounds.first; itr != bounds.second; ++itr)
            {
                // skip checked quest id, only state of other quests in group is interesting
                uint32 exclusiveQuestId = itr->second;
                if (exclusiveQuestId == prevId)
                    continue;

                // alternative quest from group also must be completed and rewarded (reported)
                if (!IsQuestRewarded(exclusiveQuestId))
                {
                    if (msg)
                    {
                        SendCanTakeQuestResponse(INVALIDREASON_DONT_HAVE_REQ);
                        TC_LOG_DEBUG("misc", "Player::SatisfyQuestDependentPreviousQuests: Sent INVALIDREASON_DONT_HAVE_REQ (QuestID: {}) because player '{}' ({}) doesn't have the required quest (1).",
                            qInfo->GetQuestId(), GetName(), GetGUID().ToString());
                    }

                    return false;
                }
            }

            return true;
        }
    }

    // Has only prev. quests in non-rewarded state
    if (msg)
    {
        SendCanTakeQuestResponse(INVALIDREASON_DONT_HAVE_REQ);
        TC_LOG_DEBUG("misc", "Player::SatisfyQuestDependentPreviousQuests: Sent INVALIDREASON_DONT_HAVE_REQ (QuestID: {}) because player '{}' ({}) doesn't have required quest (2).",
            qInfo->GetQuestId(), GetName(), GetGUID().ToString());
    }

    return false;
}

bool Player::SatisfyQuestBreadcrumbQuest(Quest const* qInfo, bool msg) const
{
    uint32 breadcrumbTargetQuestId = std::abs(qInfo->GetBreadcrumbForQuestId());

    //If this is not a breadcrumb quest.
    if (!breadcrumbTargetQuestId)
        return true;

    // If the target quest is not available
    if (!CanTakeQuest(sObjectMgr->GetQuestTemplate(breadcrumbTargetQuestId), false))
    {
        if (msg)
        {
            SendCanTakeQuestResponse(INVALIDREASON_DONT_HAVE_REQ);
            TC_LOG_DEBUG("misc", "Player::SatisfyQuestBreadcrumbQuest: Sent INVALIDREASON_DONT_HAVE_REQ (QuestID: {}) because target quest (QuestID: {}) is not available to player '{}' ({}).",
                qInfo->GetQuestId(), breadcrumbTargetQuestId, GetName(), GetGUID().ToString());
        }

        return false;
    }

    return true;
}

bool Player::SatisfyQuestDependentBreadcrumbQuests(Quest const* qInfo, bool msg) const
{
    for (uint32 breadcrumbQuestId : qInfo->DependentBreadcrumbQuests)
    {
        QuestStatus status = GetQuestStatus(breadcrumbQuestId);
        // If any of the breadcrumb quests are in the quest log, return false.
        if (status == QUEST_STATUS_INCOMPLETE || status == QUEST_STATUS_COMPLETE || status == QUEST_STATUS_FAILED)
        {
            if (msg)
            {
                SendCanTakeQuestResponse(INVALIDREASON_DONT_HAVE_REQ);
                TC_LOG_DEBUG("misc", "Player::SatisfyQuestDependentBreadcrumbQuests: Sent INVALIDREASON_DONT_HAVE_REQ (QuestID: {}) because player '{}' ({}) has a breadcrumb quest towards this quest in the quest log.",
                    qInfo->GetQuestId(), GetName(), GetGUID().ToString());
            }

            return false;
        }
    }
    return true;
}

bool Player::SatisfyQuestClass(Quest const* qInfo, bool msg) const
{
    uint32 reqClass = qInfo->GetRequiredClasses();

    if (reqClass == 0)
        return true;

    if ((reqClass & GetClassMask()) == 0)
    {
        if (msg)
        {
            SendCanTakeQuestResponse(INVALIDREASON_DONT_HAVE_REQ);
            TC_LOG_DEBUG("misc", "Player::SatisfyQuestClass: Sent INVALIDREASON_DONT_HAVE_REQ (QuestID: {}) because player '{}' ({}) doesn't have required class.",
                qInfo->GetQuestId(), GetName(), GetGUID().ToString());
        }

        return false;
    }

    return true;
}

bool Player::SatisfyQuestRace(Quest const* qInfo, bool msg) const
{
    uint32 reqraces = qInfo->GetAllowableRaces();
    if (reqraces == 0)
        return true;
    if ((reqraces & GetRaceMask()) == 0)
    {
        if (msg)
        {
            SendCanTakeQuestResponse(INVALIDREASON_QUEST_FAILED_WRONG_RACE);
            TC_LOG_DEBUG("misc", "Player::SatisfyQuestRace: Sent INVALIDREASON_QUEST_FAILED_WRONG_RACE (QuestID: {}) because player '{}' ({}) doesn't have required race.",
                qInfo->GetQuestId(), GetName(), GetGUID().ToString());

        }
        return false;
    }
    return true;
}

bool Player::SatisfyQuestReputation(Quest const* qInfo, bool msg) const
{
    uint32 fIdMin = qInfo->GetRequiredMinRepFaction();      //Min required rep
    if (fIdMin && GetReputationMgr().GetReputation(fIdMin) < qInfo->GetRequiredMinRepValue())
    {
        if (msg)
        {
            SendCanTakeQuestResponse(INVALIDREASON_DONT_HAVE_REQ);
            TC_LOG_DEBUG("misc", "Player::SatisfyQuestReputation: Sent INVALIDREASON_DONT_HAVE_REQ (QuestID: {}) because player '{}' ({}) doesn't required reputation (min).",
                qInfo->GetQuestId(), GetName(), GetGUID().ToString());
        }
        return false;
    }

    uint32 fIdMax = qInfo->GetRequiredMaxRepFaction();      //Max required rep
    if (fIdMax && GetReputationMgr().GetReputation(fIdMax) >= qInfo->GetRequiredMaxRepValue())
    {
        if (msg)
        {
            SendCanTakeQuestResponse(INVALIDREASON_DONT_HAVE_REQ);
            TC_LOG_DEBUG("misc", "SatisfyQuestReputation: Sent INVALIDREASON_DONT_HAVE_REQ (QuestID: {}) because player '{}' ({}) doesn't required reputation (max).",
                qInfo->GetQuestId(), GetName(), GetGUID().ToString());
        }
        return false;
    }

    // ReputationObjective2 does not seem to be an objective requirement but a requirement
    // to be able to accept the quest
    uint32 fIdObj = qInfo->GetRepObjectiveFaction2();
    if (fIdObj && GetReputationMgr().GetReputation(fIdObj) >= qInfo->GetRepObjectiveValue2())
    {
        if (msg)
        {
            SendCanTakeQuestResponse(INVALIDREASON_DONT_HAVE_REQ);
            TC_LOG_DEBUG("misc", "SatisfyQuestReputation: Sent INVALIDREASON_DONT_HAVE_REQ (questId: {}) because player does not have the required reputation (ReputationObjective2).", qInfo->GetQuestId());
        }
        return false;
    }

    return true;
}

bool Player::SatisfyQuestStatus(Quest const* qInfo, bool msg) const
{
    if (GetQuestStatus(qInfo->GetQuestId()) == QUEST_STATUS_REWARDED)
    {
        if (msg)
        {
            SendCanTakeQuestResponse(INVALIDREASON_QUEST_ALREADY_DONE);
            TC_LOG_DEBUG("misc", "Player::SatisfyQuestStatus: Sent QUEST_STATUS_REWARDED (QuestID: {}) because player '{}' ({}) quest status is already REWARDED.",
                qInfo->GetQuestId(), GetName(), GetGUID().ToString());
        }
        return false;
    }

    if (GetQuestStatus(qInfo->GetQuestId()) != QUEST_STATUS_NONE)
    {
        if (msg)
        {
            SendCanTakeQuestResponse(INVALIDREASON_QUEST_ALREADY_ON);
            TC_LOG_DEBUG("misc", "Player::SatisfyQuestStatus: Sent INVALIDREASON_QUEST_ALREADY_ON (QuestID: {}) because player '{}' ({}) quest status is not NONE.",
                qInfo->GetQuestId(), GetName(), GetGUID().ToString());
        }
        return false;
    }
    return true;
}

bool Player::SatisfyQuestConditions(Quest const* qInfo, bool msg) const
{
    if (!sConditionMgr->IsObjectMeetingNotGroupedConditions(CONDITION_SOURCE_TYPE_QUEST_AVAILABLE, qInfo->GetQuestId(), const_cast<Player*>(this)))
    {
        if (msg)
        {
            SendCanTakeQuestResponse(INVALIDREASON_DONT_HAVE_REQ);
            TC_LOG_DEBUG("misc", "Player::SatisfyQuestConditions: Sent INVALIDREASON_DONT_HAVE_REQ (QuestID: {}) because player '{}' ({}) doesn't meet conditions.",
                qInfo->GetQuestId(), GetName(), GetGUID().ToString());
        }
        TC_LOG_DEBUG("condition", "Player::SatisfyQuestConditions: conditions not met for quest {}", qInfo->GetQuestId());
        return false;
    }
    return true;
}

bool Player::SatisfyQuestTimed(Quest const* qInfo, bool msg) const
{
    if (!m_timedquests.empty() && qInfo->HasSpecialFlag(QUEST_SPECIAL_FLAGS_TIMED))
    {
        if (msg)
        {
            SendCanTakeQuestResponse(INVALIDREASON_QUEST_ONLY_ONE_TIMED);
            TC_LOG_DEBUG("misc", "Player::SatisfyQuestTimed: Sent INVALIDREASON_QUEST_ONLY_ONE_TIMED (QuestID: {}) because player '{}' ({}) is already on a timed quest.",
                qInfo->GetQuestId(), GetName(), GetGUID().ToString());
        }
        return false;
    }
    return true;
}

bool Player::SatisfyQuestExclusiveGroup(Quest const* qInfo, bool msg) const
{
    // non positive exclusive group, if > 0 then can be start if any other quest in exclusive group already started/completed
    if (qInfo->GetExclusiveGroup() <= 0)
        return true;

    auto bounds = sObjectMgr->GetExclusiveQuestGroupBounds(qInfo->GetExclusiveGroup());
    for (auto itr = bounds.first; itr != bounds.second; ++itr)
    {
        uint32 exclude_Id = itr->second;

        // skip checked quest id, only state of other quests in group is interesting
        if (exclude_Id == qInfo->GetQuestId())
            continue;

        // not allow have daily quest if daily quest from exclusive group already recently completed
        Quest const* Nquest = sObjectMgr->GetQuestTemplate(exclude_Id);
        ASSERT(Nquest);
        if (!SatisfyQuestDay(Nquest, false) || !SatisfyQuestWeek(Nquest, false) || !SatisfyQuestSeasonal(Nquest, false))
        {
            if (msg)
            {
                SendCanTakeQuestResponse(INVALIDREASON_DONT_HAVE_REQ);
                TC_LOG_DEBUG("misc", "Player::SatisfyQuestExclusiveGroup: Sent INVALIDREASON_DONT_HAVE_REQ (QuestID: {}) because player '{}' ({}) already did daily quests in exclusive group.",
                    qInfo->GetQuestId(), GetName(), GetGUID().ToString());
            }

            return false;
        }

        // alternative quest already started or completed - but don't check rewarded states if both are repeatable
        if (GetQuestStatus(exclude_Id) != QUEST_STATUS_NONE || (!(qInfo->IsRepeatable() && Nquest->IsRepeatable()) && GetQuestRewardStatus(exclude_Id)))
        {
            if (msg)
            {
                SendCanTakeQuestResponse(INVALIDREASON_DONT_HAVE_REQ);
                TC_LOG_DEBUG("misc", "Player::SatisfyQuestExclusiveGroup: Sent INVALIDREASON_DONT_HAVE_REQ (QuestID: {}) because player '{}' ({}) already did quest in exclusive group.",
                    qInfo->GetQuestId(), GetName(), GetGUID().ToString());
            }
            return false;
        }
    }
    return true;
}

bool Player::SatisfyQuestDay(Quest const* qInfo, bool msg) const
{
    if (!qInfo->IsDaily() && !qInfo->IsDFQuest())
        return true;

    if (qInfo->IsDFQuest())
    {
        if (m_DFQuests.find(qInfo->GetQuestId()) != m_DFQuests.end())
            return false;

        return true;
    }

    bool have_slot = false;
    for (uint32 quest_daily_idx = 0; quest_daily_idx < PLAYER_MAX_DAILY_QUESTS; ++quest_daily_idx)
    {
        uint32 id = GetUInt32Value(PLAYER_FIELD_DAILY_QUESTS_1+quest_daily_idx);
        if (qInfo->GetQuestId() == id)
            return false;

        if (!id)
            have_slot = true;
    }

    if (!have_slot)
    {
        if (msg)
        {
            SendCanTakeQuestResponse(INVALIDREASON_DAILY_QUESTS_REMAINING);
            TC_LOG_DEBUG("misc", "SatisfyQuestDay: Sent INVALIDREASON_DAILY_QUESTS_REMAINING (questId: {}) because player already did all possible quests today.", qInfo->GetQuestId());
        }
        return false;
    }

    return true;
}

bool Player::SatisfyQuestWeek(Quest const* qInfo, bool /*msg*/) const
{
    if (!qInfo->IsWeekly() || m_weeklyquests.empty())
        return true;

    // if not found in cooldown list
    return m_weeklyquests.find(qInfo->GetQuestId()) == m_weeklyquests.end();
}

bool Player::SatisfyQuestSeasonal(Quest const* qInfo, bool /*msg*/) const
{
    if (!qInfo->IsSeasonal() || m_seasonalquests.empty())
        return true;

    auto itr = m_seasonalquests.find(qInfo->GetEventIdForQuest());
    if (itr == m_seasonalquests.end() || itr->second.empty())
        return true;

    // if not found in cooldown list
    return itr->second.find(qInfo->GetQuestId()) == itr->second.end();
}

bool Player::SatisfyQuestMonth(Quest const* qInfo, bool /*msg*/) const
{
    if (!qInfo->IsMonthly() || m_monthlyquests.empty())
        return true;

    // if not found in cooldown list
    return m_monthlyquests.find(qInfo->GetQuestId()) == m_monthlyquests.end();
}

bool Player::GiveQuestSourceItem(Quest const* quest)
{
    uint32 srcitem = quest->GetSrcItemId();
    if (srcitem > 0)
    {
        // Don't give source item if it is the same item used to start the quest
        ItemTemplate const* itemTemplate = ASSERT_NOTNULL(sObjectMgr->GetItemTemplate(srcitem));
        if (quest->GetQuestId() == itemTemplate->StartQuest)
            return true;

        uint32 count = quest->GetSrcItemCount();
        if (count <= 0)
            count = 1;

        ItemPosCountVec dest;
        InventoryResult msg = CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, srcitem, count);
        if (msg == EQUIP_ERR_OK)
        {
            Item* item = StoreNewItem(dest, srcitem, true);
            SendNewItem(item, count, true, false);
            return true;
        }
        // player already have max amount required item, just report success
        else if (msg == EQUIP_ERR_CANT_CARRY_MORE_OF_THIS)
            return true;

        SendEquipError(msg, nullptr, nullptr, srcitem);
        return false;
    }

    return true;
}

bool Player::TakeQuestSourceItem(uint32 questId, bool msg)
{
    Quest const* quest = sObjectMgr->GetQuestTemplate(questId);
    if (quest)
    {
        uint32 srcItemId = quest->GetSrcItemId();
        ItemTemplate const* item = sObjectMgr->GetItemTemplate(srcItemId);

        if (srcItemId > 0)
        {
            uint32 count = quest->GetSrcItemCount();
            if (count <= 0)
                count = 1;

            // There are two cases where the source item is not destroyed:
            // - Item cannot be unequipped (example: non-empty bags)
            // - The source item is the item that started the quest, so the player is supposed to keep it (otherwise it was already destroyed in AddQuestAndCheckCompletion())
            InventoryResult res = CanUnequipItems(srcItemId, count);
            if (res != EQUIP_ERR_OK)
            {
                if (msg)
                    SendEquipError(res, nullptr, nullptr, srcItemId);
                return false;
            }

            ASSERT(item);
            if (item->StartQuest != questId)
                DestroyItemCount(srcItemId, count, true, true);
        }
    }

    return true;
}

bool Player::GetQuestRewardStatus(uint32 quest_id) const
{
    Quest const* qInfo = sObjectMgr->GetQuestTemplate(quest_id);
    if (qInfo)
    {
        if (qInfo->IsSeasonal() && !qInfo->IsRepeatable())
            return !SatisfyQuestSeasonal(qInfo, false);
        // for repeatable quests: rewarded field is set after first reward only to prevent getting XP more than once
        if (!qInfo->IsRepeatable())
            return IsQuestRewarded(quest_id);

        return false;
    }
    return false;
}

QuestStatus Player::GetQuestStatus(uint32 quest_id) const
{
    if (quest_id)
    {
        QuestStatusMap::const_iterator itr = m_QuestStatus.find(quest_id);
        if (itr != m_QuestStatus.end())
            return itr->second.Status;

        if (GetQuestRewardStatus(quest_id))
            return QUEST_STATUS_REWARDED;
    }
    return QUEST_STATUS_NONE;
}

bool Player::CanShareQuest(uint32 quest_id) const
{
    Quest const* qInfo = sObjectMgr->GetQuestTemplate(quest_id);
    if (qInfo && qInfo->HasFlag(QUEST_FLAGS_SHARABLE))
    {
        QuestStatusMap::const_iterator itr = m_QuestStatus.find(quest_id);
        if (itr != m_QuestStatus.end())
        {
            // in pool and not currently available (wintergrasp weekly, dalaran weekly) - can't share
            if (!sQuestPoolMgr->IsQuestActive(quest_id))
            {
                SendPushToPartyResponse(this, QUEST_PARTY_MSG_CANT_BE_SHARED_TODAY);
                return false;
            }

            return true;
        }
    }
    return false;
}

void Player::SetQuestStatus(uint32 questId, QuestStatus status, bool update /*= true*/)
{
    if (Quest const* quest = sObjectMgr->GetQuestTemplate(questId))
    {
        m_QuestStatus[questId].Status = status;

        if (!quest->IsAutoComplete())
            m_QuestStatusSave[questId] = QUEST_DEFAULT_SAVE_TYPE;

        // @tswow-begin
        FIRE_ID(
            quest->events.id
            , Quest,OnStatusChanged
            , TSQuest(quest)
            , TSPlayer(this)
        );
        // @tswow-end
    }

    if (update)
        SendQuestUpdate(questId);
    sScriptMgr->OnQuestStatusChange(this, questId);
}

void Player::RemoveActiveQuest(uint32 questId, bool update /*= true*/)
{
    QuestStatusMap::iterator itr = m_QuestStatus.find(questId);
    if (itr != m_QuestStatus.end())
    {
        m_QuestStatus.erase(itr);
        m_QuestStatusSave[questId] = QUEST_DELETE_SAVE_TYPE;
    }

    if (update)
        SendQuestUpdate(questId);
}

void Player::RemoveRewardedQuest(uint32 questId, bool update /*= true*/)
{
    RewardedQuestSet::iterator rewItr = m_RewardedQuests.find(questId);
    if (rewItr != m_RewardedQuests.end())
    {
        m_RewardedQuests.erase(rewItr);
        m_RewardedQuestsSave[questId] = QUEST_FORCE_DELETE_SAVE_TYPE;
    }

    // Remove seasonal quest also
    Quest const* qInfo = sObjectMgr->GetQuestTemplate(questId);
    ASSERT(qInfo);
    if (qInfo->IsSeasonal())
    {
        uint16 eventId = qInfo->GetEventIdForQuest();
        if (m_seasonalquests.find(eventId) != m_seasonalquests.end())
        {
            m_seasonalquests[eventId].erase(questId);
            m_SeasonalQuestChanged = true;
        }
    }

    if (update)
        SendQuestUpdate(questId);
}

void Player::SendQuestUpdate(uint32 questId)
{
    SpellAreaForQuestMapBounds saBounds = sSpellMgr->GetSpellAreaForQuestMapBounds(questId);

    if (saBounds.first != saBounds.second)
    {
        std::set<uint32> aurasToRemove, aurasToCast;
        uint32 zone = 0, area = 0;
        GetZoneAndAreaId(zone, area);

        for (SpellAreaForQuestMap::const_iterator itr = saBounds.first; itr != saBounds.second; ++itr)
        {
            if (!itr->second->IsFitToRequirements(this, zone, area))
                aurasToRemove.insert(itr->second->spellId);
            else if (itr->second->autocast)
                aurasToCast.insert(itr->second->spellId);
        }

        // Auras matching the requirements will be inside the aurasToCast container.
        // Auras not matching the requirements may prevent using auras matching the requirements.
        // aurasToCast will erase conflicting auras in aurasToRemove container to handle spells used by multiple quests.

        for (auto itr = aurasToRemove.begin(); itr != aurasToRemove.end();)
        {
            bool auraRemoved = false;

            for (const auto i : aurasToCast)
            {
                if (*itr == i)
                {
                    itr = aurasToRemove.erase(itr);
                    auraRemoved = true;
                    break;
                }
            }

            if (!auraRemoved)
                ++itr;
        }

        for (auto spellId : aurasToCast)
            if (!HasAura(spellId))
                CastSpell(this, spellId, true);

        for (auto spellId : aurasToRemove)
            RemoveAurasDueToSpell(spellId);
    }

    UpdateVisibleGameobjectsOrSpellClicks();
}

QuestGiverStatus Player::GetQuestDialogStatus(Object* questgiver)
{
    QuestRelationResult qr, qir;

    switch (questgiver->GetTypeId())
    {
        case TYPEID_GAMEOBJECT:
        {
            if (auto ai = questgiver->ToGameObject()->AI())
                if (auto questStatus = ai->GetDialogStatus(this))
                    return *questStatus;
            qr = sObjectMgr->GetGOQuestRelations(questgiver->GetEntry());
            qir = sObjectMgr->GetGOQuestInvolvedRelations(questgiver->GetEntry());
            break;
        }
        case TYPEID_UNIT:
        {
            if (auto ai = questgiver->ToCreature()->AI())
                if (auto questStatus = ai->GetDialogStatus(this))
                    return *questStatus;
            qr = sObjectMgr->GetCreatureQuestRelations(questgiver->GetEntry());
            qir = sObjectMgr->GetCreatureQuestInvolvedRelations(questgiver->GetEntry());
            break;
        }
        default:
            // it's impossible, but check
            TC_LOG_ERROR("entities.player.quest", "GetQuestDialogStatus called for unexpected type {}", questgiver->GetTypeId());
            return DIALOG_STATUS_NONE;
    }

    QuestGiverStatus result = DIALOG_STATUS_NONE;

    for (uint32 questId : qir)
    {
        QuestGiverStatus result2 = DIALOG_STATUS_NONE;
        Quest const* quest = sObjectMgr->GetQuestTemplate(questId);
        if (!quest)
            continue;

        QuestStatus status = GetQuestStatus(questId);
        if (status == QUEST_STATUS_COMPLETE && !GetQuestRewardStatus(questId))
            result2 = DIALOG_STATUS_REWARD;
        else if (status == QUEST_STATUS_INCOMPLETE)
            result2 = DIALOG_STATUS_INCOMPLETE;

        if (quest->IsAutoComplete() && CanTakeQuest(quest, false) && quest->IsRepeatable() && !quest->IsDailyOrWeekly())
            result2 = DIALOG_STATUS_REWARD_REP;

        if (result2 > result)
            result = result2;
    }

    for (uint32 questId : qr)
    {
        QuestGiverStatus result2 = DIALOG_STATUS_NONE;
        Quest const* quest = sObjectMgr->GetQuestTemplate(questId);
        if (!quest)
            continue;

        if (!sConditionMgr->IsObjectMeetingNotGroupedConditions(CONDITION_SOURCE_TYPE_QUEST_AVAILABLE, quest->GetQuestId(), this))
            continue;

        QuestStatus status = GetQuestStatus(questId);
        if (status == QUEST_STATUS_NONE)
        {
            if (CanSeeStartQuest(quest))
            {
                if (SatisfyQuestLevel(quest, false))
                {
                    bool isNotLowLevelQuest = GetLevel() <= (GetQuestLevel(quest) + sWorld->getIntConfig(CONFIG_QUEST_LOW_LEVEL_HIDE_DIFF));
                    if (quest->IsRepeatable())
                    {
                        if (quest->IsDaily())
                        {
                            if (isNotLowLevelQuest)
                                result2 = DIALOG_STATUS_AVAILABLE_REP;
                            else
                                result2 = DIALOG_STATUS_LOW_LEVEL_AVAILABLE_REP;
                        }
                        else if (quest->IsWeekly() || quest->IsMonthly())
                        {
                            if (isNotLowLevelQuest)
                                result2 = DIALOG_STATUS_AVAILABLE;
                            else
                                result2 = DIALOG_STATUS_LOW_LEVEL_AVAILABLE;
                        }
                        else if (quest->IsAutoComplete())
                        {
                            if (isNotLowLevelQuest)
                                result2 = DIALOG_STATUS_REWARD_REP;
                            else
                                result2 = DIALOG_STATUS_LOW_LEVEL_REWARD_REP;
                        }
                        else
                        {
                            if (isNotLowLevelQuest)
                                result2 = DIALOG_STATUS_AVAILABLE;
                            else
                                result2 = DIALOG_STATUS_LOW_LEVEL_AVAILABLE;
                        }
                    }
                    else if (isNotLowLevelQuest)
                        result2 = DIALOG_STATUS_AVAILABLE;
                    else
                        result2 = DIALOG_STATUS_LOW_LEVEL_AVAILABLE;
                }
                else
                    result2 = DIALOG_STATUS_UNAVAILABLE;
            }
        }

        if (result2 > result)
            result = result2;
    }

    return result;
}

// not used in Trinity, but used in scripting code
uint16 Player::GetReqKillOrCastCurrentCount(uint32 quest_id, int32 entry) const
{
    Quest const* qInfo = sObjectMgr->GetQuestTemplate(quest_id);
    if (!qInfo)
        return 0;

    auto itr = m_QuestStatus.find(quest_id);
    if (itr != m_QuestStatus.end())
        for (uint8 j = 0; j < QUEST_OBJECTIVES_COUNT; ++j)
            if (qInfo->RequiredNpcOrGo[j] == entry)
                return itr->second.CreatureOrGOCount[j];

    return 0;
}

void Player::AdjustQuestReqItemCount(Quest const* quest, QuestStatusData& questStatusData)
{
    if (quest->HasSpecialFlag(QUEST_SPECIAL_FLAGS_DELIVER))
    {
        for (uint8 i = 0; i < QUEST_ITEM_OBJECTIVES_COUNT; ++i)
        {
            uint32 reqitemcount = quest->RequiredItemCount[i];
            if (reqitemcount != 0)
            {
                uint32 curitemcount = GetItemCount(quest->RequiredItemId[i], true);

                questStatusData.ItemCount[i] = std::min(curitemcount, reqitemcount);
                m_QuestStatusSave[quest->GetQuestId()] = QUEST_DEFAULT_SAVE_TYPE;
            }
        }
    }
}

uint16 Player::FindQuestSlot(uint32 quest_id) const
{
    for (uint16 i = 0; i < MAX_QUEST_LOG_SIZE; ++i)
        if (GetQuestSlotQuestId(i) == quest_id)
            return i;

    return MAX_QUEST_LOG_SIZE;
}

uint32 Player::GetQuestSlotQuestId(uint16 slot) const
{
    return GetUInt32Value(PLAYER_QUEST_LOG_1_1 + slot * MAX_QUEST_OFFSET + QUEST_ID_OFFSET);
}

uint32 Player::GetQuestSlotState(uint16 slot)   const
{
    return GetUInt32Value(PLAYER_QUEST_LOG_1_1 + slot * MAX_QUEST_OFFSET + QUEST_STATE_OFFSET);
}

uint16 Player::GetQuestSlotCounter(uint16 slot, uint8 counter) const
{
    return (uint16)(GetUInt64Value(PLAYER_QUEST_LOG_1_1 + slot * MAX_QUEST_OFFSET + QUEST_COUNTS_OFFSET) >> (counter * 16));
}

uint32 Player::GetQuestSlotTime(uint16 slot) const
{
    return GetUInt32Value(PLAYER_QUEST_LOG_1_1 + slot * MAX_QUEST_OFFSET + QUEST_TIME_OFFSET);
}

void Player::SetQuestSlot(uint16 slot, uint32 quest_id, uint32 timer /*= 0*/)
{
    SetUInt32Value(PLAYER_QUEST_LOG_1_1 + slot * MAX_QUEST_OFFSET + QUEST_ID_OFFSET, quest_id);
    SetUInt32Value(PLAYER_QUEST_LOG_1_1 + slot * MAX_QUEST_OFFSET + QUEST_STATE_OFFSET, 0);
    SetUInt32Value(PLAYER_QUEST_LOG_1_1 + slot * MAX_QUEST_OFFSET + QUEST_COUNTS_OFFSET, 0);
    SetUInt32Value(PLAYER_QUEST_LOG_1_1 + slot * MAX_QUEST_OFFSET + QUEST_COUNTS_OFFSET + 1, 0);
    SetUInt32Value(PLAYER_QUEST_LOG_1_1 + slot * MAX_QUEST_OFFSET + QUEST_TIME_OFFSET, timer);
}

void Player::SetQuestSlotCounter(uint16 slot, uint8 counter, uint16 count)
{
    uint64 val = GetUInt64Value(PLAYER_QUEST_LOG_1_1 + slot * MAX_QUEST_OFFSET + QUEST_COUNTS_OFFSET);
    val &= ~((uint64)0xFFFF << (counter * 16));
    val |= ((uint64)count << (counter * 16));
    SetUInt64Value(PLAYER_QUEST_LOG_1_1 + slot * MAX_QUEST_OFFSET + QUEST_COUNTS_OFFSET, val);
}

void Player::SetQuestSlotState(uint16 slot, uint32 state)
{
    SetFlag(PLAYER_QUEST_LOG_1_1 + slot * MAX_QUEST_OFFSET + QUEST_STATE_OFFSET, state);
}

void Player::RemoveQuestSlotState(uint16 slot, uint32 state)
{
    RemoveFlag(PLAYER_QUEST_LOG_1_1 + slot * MAX_QUEST_OFFSET + QUEST_STATE_OFFSET, state);
}

void Player::SetQuestSlotTimer(uint16 slot, uint32 timer)
{
    SetUInt32Value(PLAYER_QUEST_LOG_1_1 + slot * MAX_QUEST_OFFSET + QUEST_TIME_OFFSET, timer);
}

void Player::SwapQuestSlot(uint16 slot1, uint16 slot2)
{
    for (int i = 0; i < MAX_QUEST_OFFSET; ++i)
    {
        uint32 temp1 = GetUInt32Value(PLAYER_QUEST_LOG_1_1 + MAX_QUEST_OFFSET * slot1 + i);
        uint32 temp2 = GetUInt32Value(PLAYER_QUEST_LOG_1_1 + MAX_QUEST_OFFSET * slot2 + i);

        SetUInt32Value(PLAYER_QUEST_LOG_1_1 + MAX_QUEST_OFFSET * slot1 + i, temp2);
        SetUInt32Value(PLAYER_QUEST_LOG_1_1 + MAX_QUEST_OFFSET * slot2 + i, temp1);
    }
}

void Player::AreaExploredOrEventHappens(uint32 questId)
{
    if (questId)
    {
        uint16 log_slot = FindQuestSlot(questId);
        if (log_slot < MAX_QUEST_LOG_SIZE)
        {
            QuestStatusData& q_status = m_QuestStatus[questId];

            // Dont complete failed quest
            if (!q_status.Explored && q_status.Status != QUEST_STATUS_FAILED)
            {
                q_status.Explored = true;
                m_QuestStatusSave[questId] = QUEST_DEFAULT_SAVE_TYPE;

                SendQuestComplete(questId);
            }
        }
        if (CanCompleteQuest(questId))
            CompleteQuest(questId);
    }
}

//not used in Trinityd, function for external script library
void Player::GroupEventHappens(uint32 questId, WorldObject const* pEventObject)
{
    if (Group* group = GetGroup())
    {
        for (GroupReference* itr = group->GetFirstMember(); itr != nullptr; itr = itr->next())
        {
            Player* player = itr->GetSource();

            // for any leave or dead (with not released body) group member at appropriate distance
            if (player && player->IsAtGroupRewardDistance(pEventObject) && !player->GetCorpse())
                player->AreaExploredOrEventHappens(questId);
        }
    }
    else
        AreaExploredOrEventHappens(questId);
}

void Player::ItemAddedQuestCheck(uint32 entry, uint32 count)
{
    for (uint8 i = 0; i < MAX_QUEST_LOG_SIZE; ++i)
    {
        uint32 questid = GetQuestSlotQuestId(i);
        if (questid == 0)
            continue;

        QuestStatusData& q_status = m_QuestStatus[questid];

        if (q_status.Status != QUEST_STATUS_INCOMPLETE)
            continue;

        Quest const* qInfo = sObjectMgr->GetQuestTemplate(questid);
        if (!qInfo || !qInfo->HasSpecialFlag(QUEST_SPECIAL_FLAGS_DELIVER))
            continue;

        for (uint8 j = 0; j < QUEST_ITEM_OBJECTIVES_COUNT; ++j)
        {
            uint32 reqitem = qInfo->RequiredItemId[j];
            if (reqitem == entry)
            {
                uint32 reqitemcount = qInfo->RequiredItemCount[j];
                uint16 curitemcount = q_status.ItemCount[j];
                if (curitemcount < reqitemcount)
                {
                    q_status.ItemCount[j] = std::min<uint16>(q_status.ItemCount[j] + count, reqitemcount);
                    m_QuestStatusSave[questid] = QUEST_DEFAULT_SAVE_TYPE;
                }
                if (CanCompleteQuest(questid))
                    CompleteQuest(questid);
            }
        }
    }
    UpdateVisibleGameobjectsOrSpellClicks();
}

void Player::ItemRemovedQuestCheck(uint32 entry, uint32 count)
{
    for (uint8 i = 0; i < MAX_QUEST_LOG_SIZE; ++i)
    {
        uint32 questid = GetQuestSlotQuestId(i);
        if (!questid)
            continue;

        Quest const* qInfo = sObjectMgr->GetQuestTemplate(questid);
        if (!qInfo)
            continue;

        if (!qInfo->HasSpecialFlag(QUEST_SPECIAL_FLAGS_DELIVER))
            continue;

        for (uint8 j = 0; j < QUEST_ITEM_OBJECTIVES_COUNT; ++j)
        {
            uint32 reqitem = qInfo->RequiredItemId[j];
            if (reqitem == entry)
            {
                QuestStatusData& q_status = m_QuestStatus[questid];

                uint32 reqItemCount = qInfo->RequiredItemCount[j];
                uint16 questStatusItemCount = q_status.ItemCount[j];
                uint16 newItemCount = (count > questStatusItemCount) ? 0 : questStatusItemCount - count;

                if (questStatusItemCount >= reqItemCount) // we may have more than what the status shows, we don't need reduce by count
                    newItemCount = GetItemCount(entry, false);

                newItemCount = std::min<uint16>(newItemCount, reqItemCount);
                if (newItemCount != q_status.ItemCount[j])
                {
                    q_status.ItemCount[j] = newItemCount;
                    m_QuestStatusSave[questid] = QUEST_DEFAULT_SAVE_TYPE;
                    IncompleteQuest(questid);
                }
            }
        }
    }
    UpdateVisibleGameobjectsOrSpellClicks();
}

void Player::KilledMonster(CreatureTemplate const* cInfo, ObjectGuid guid)
{
    ASSERT(cInfo);

    if (cInfo->Entry)
        KilledMonsterCredit(cInfo->Entry, guid);

    for (uint8 i = 0; i < MAX_KILL_CREDIT; ++i)
        if (cInfo->KillCredit[i])
            KilledMonsterCredit(cInfo->KillCredit[i], ObjectGuid::Empty);
}

void Player::KilledMonsterCredit(uint32 entry, ObjectGuid guid /*= ObjectGuid::Empty*/)
{
    uint16 addkillcount = 1;
    uint32 real_entry = entry;
    Creature* killed = nullptr;
    if (guid)
    {
        killed = GetMap()->GetCreature(guid);
        if (killed && killed->GetEntry())
            real_entry = killed->GetEntry();
    }

    StartTimedAchievement(ACHIEVEMENT_TIMED_TYPE_CREATURE, real_entry);   // MUST BE CALLED FIRST
    UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_KILL_CREATURE, real_entry, addkillcount, killed);

    for (uint8 i = 0; i < MAX_QUEST_LOG_SIZE; ++i)
    {
        uint32 questid = GetQuestSlotQuestId(i);
        if (!questid)
            continue;

        Quest const* qInfo = sObjectMgr->GetQuestTemplate(questid);
        if (!qInfo)
            continue;
        // just if !ingroup || !noraidgroup || raidgroup
        QuestStatusData& q_status = m_QuestStatus[questid];
        if (q_status.Status == QUEST_STATUS_INCOMPLETE && (!GetGroup() || !GetGroup()->isRaidGroup() || qInfo->IsAllowedInRaid(GetMap()->GetDifficulty())))
        {
            if (qInfo->HasSpecialFlag(QUEST_SPECIAL_FLAGS_KILL) /*&& !qInfo->HasSpecialFlag(QUEST_SPECIAL_FLAGS_CAST)*/)
            {
                for (uint8 j = 0; j < QUEST_OBJECTIVES_COUNT; ++j)
                {
                    // skip GO activate objective or none
                    if (qInfo->RequiredNpcOrGo[j] <= 0)
                        continue;

                    uint32 reqkill = qInfo->RequiredNpcOrGo[j];

                    if (reqkill == real_entry)
                    {
                        uint32 reqkillcount = qInfo->RequiredNpcOrGoCount[j];
                        uint16 curkillcount = q_status.CreatureOrGOCount[j];
                        if (curkillcount < reqkillcount)
                        {
                            q_status.CreatureOrGOCount[j] = curkillcount + addkillcount;

                            m_QuestStatusSave[questid] = QUEST_DEFAULT_SAVE_TYPE;

                            SendQuestUpdateAddCreatureOrGo(qInfo, guid, j, curkillcount, addkillcount);
                        }
                        if (CanCompleteQuest(questid))
                            CompleteQuest(questid);

                        // same objective target can be in many active quests, but not in 2 objectives for single quest (code optimization).
                        break;
                    }
                }
            }
        }
    }
}

void Player::KilledPlayerCredit(uint16 count)
{
    for (uint8 i = 0; i < MAX_QUEST_LOG_SIZE; ++i)
    {
        uint32 questid = GetQuestSlotQuestId(i);
        if (!questid)
            continue;

        Quest const* qInfo = sObjectMgr->GetQuestTemplate(questid);
        if (!qInfo)
            continue;
        // just if !ingroup || !noraidgroup || raidgroup
        QuestStatusData& q_status = m_QuestStatus[questid];
        if (q_status.Status == QUEST_STATUS_INCOMPLETE && (!GetGroup() || !GetGroup()->isRaidGroup() || qInfo->IsAllowedInRaid(GetMap()->GetDifficulty())))
        {
            // PvP Killing quest require player to be in same zone as quest zone (only 2 quests so no doubt)
            if (qInfo->HasSpecialFlag(QUEST_SPECIAL_FLAGS_PLAYER_KILL) && GetZoneId() == static_cast<uint32>(qInfo->GetZoneOrSort()))
            {
                KilledPlayerCreditForQuest(count, qInfo);
                break; // there is only one quest per zone
            }
        }
    }
}

void Player::KilledPlayerCreditForQuest(uint16 count, Quest const* quest)
{
    uint32 const questId = quest->GetQuestId();
    auto it = m_QuestStatus.find(questId);
    if (it == m_QuestStatus.end())
        return;
    QuestStatusData& questStatus = it->second;

    uint16 curKill = questStatus.PlayerCount;
    uint32 reqKill = quest->GetPlayersSlain();

    if (curKill < reqKill)
    {
        count = std::min<uint16>(reqKill - curKill, count);
        questStatus.PlayerCount = curKill + count;

        m_QuestStatusSave[quest->GetQuestId()] = QUEST_DEFAULT_SAVE_TYPE;

        SendQuestUpdateAddPlayer(quest, curKill, count);
    }

    if (CanCompleteQuest(questId))
        CompleteQuest(questId);
}

void Player::KillCreditGO(uint32 entry, ObjectGuid guid)
{
    uint16 addCastCount = 1;
    for (uint8 i = 0; i < MAX_QUEST_LOG_SIZE; ++i)
    {
        uint32 questid = GetQuestSlotQuestId(i);
        if (!questid)
            continue;

        Quest const* qInfo = sObjectMgr->GetQuestTemplate(questid);
        if (!qInfo)
            continue;

        QuestStatusData& q_status = m_QuestStatus[questid];

        if (q_status.Status == QUEST_STATUS_INCOMPLETE)
        {
            if (qInfo->HasSpecialFlag(QUEST_SPECIAL_FLAGS_CAST) /*&& !qInfo->HasSpecialFlag(QUEST_SPECIAL_FLAGS_KILL)*/)
            {
                for (uint8 j = 0; j < QUEST_OBJECTIVES_COUNT; ++j)
                {
                    uint32 reqTarget = 0;

                    // GO activate objective
                    if (qInfo->RequiredNpcOrGo[j] < 0)
                        // checked at quest_template loading
                        reqTarget = - qInfo->RequiredNpcOrGo[j];

                    // other not this creature/GO related objectives
                    if (reqTarget != entry)
                        continue;

                    uint32 reqCastCount = qInfo->RequiredNpcOrGoCount[j];
                    uint16 curCastCount = q_status.CreatureOrGOCount[j];
                    if (curCastCount < reqCastCount)
                    {
                        q_status.CreatureOrGOCount[j] = curCastCount + addCastCount;

                        m_QuestStatusSave[questid] = QUEST_DEFAULT_SAVE_TYPE;

                        SendQuestUpdateAddCreatureOrGo(qInfo, guid, j, curCastCount, addCastCount);
                    }

                    if (CanCompleteQuest(questid))
                        CompleteQuest(questid);

                    // same objective target can be in many active quests, but not in 2 objectives for single quest (code optimization).
                    break;
                }
            }
        }
    }
}

void Player::TalkedToCreature(uint32 entry, ObjectGuid guid)
{
    uint16 addTalkCount = 1;
    for (uint8 i = 0; i < MAX_QUEST_LOG_SIZE; ++i)
    {
        uint32 questid = GetQuestSlotQuestId(i);
        if (!questid)
            continue;

        Quest const* qInfo = sObjectMgr->GetQuestTemplate(questid);
        if (!qInfo)
            continue;

        QuestStatusData& q_status = m_QuestStatus[questid];

        if (q_status.Status == QUEST_STATUS_INCOMPLETE)
        {
            if (qInfo->HasSpecialFlag(QUEST_SPECIAL_FLAGS_KILL | QUEST_SPECIAL_FLAGS_CAST | QUEST_SPECIAL_FLAGS_SPEAKTO))
            {
                for (uint8 j = 0; j < QUEST_OBJECTIVES_COUNT; ++j)
                {
                    // skip gameobject objectives
                    if (qInfo->RequiredNpcOrGo[j] < 0)
                        continue;

                    uint32 reqTarget = 0;

                    if (qInfo->RequiredNpcOrGo[j] > 0)    // creature activate objectives
                                                            // checked at quest_template loading
                        reqTarget = qInfo->RequiredNpcOrGo[j];
                    else
                        continue;

                    if (reqTarget == entry)
                    {
                        uint32 reqTalkCount = qInfo->RequiredNpcOrGoCount[j];
                        uint16 curTalkCount = q_status.CreatureOrGOCount[j];
                        if (curTalkCount < reqTalkCount)
                        {
                            q_status.CreatureOrGOCount[j] = curTalkCount + addTalkCount;

                            m_QuestStatusSave[questid] = QUEST_DEFAULT_SAVE_TYPE;

                            SendQuestUpdateAddCreatureOrGo(qInfo, guid, j, curTalkCount, addTalkCount);
                        }
                        if (CanCompleteQuest(questid))
                            CompleteQuest(questid);

                        // same objective target can be in many active quests, but not in 2 objectives for single quest (code optimization).
                        continue;
                    }
                }
            }
        }
    }
}

void Player::MoneyChanged(uint32 count)
{
    for (uint8 i = 0; i < MAX_QUEST_LOG_SIZE; ++i)
    {
        uint32 questid = GetQuestSlotQuestId(i);
        if (!questid)
            continue;

        Quest const* qInfo = sObjectMgr->GetQuestTemplate(questid);
        if (qInfo && qInfo->GetRewOrReqMoney() < 0)
        {
            QuestStatusData& q_status = m_QuestStatus[questid];

            if (q_status.Status == QUEST_STATUS_INCOMPLETE)
            {
                if (int32(count) >= -qInfo->GetRewOrReqMoney())
                {
                    if (CanCompleteQuest(questid))
                        CompleteQuest(questid);
                }
            }
            else if (q_status.Status == QUEST_STATUS_COMPLETE)
            {
                if (int32(count) < -qInfo->GetRewOrReqMoney())
                    IncompleteQuest(questid);
            }
        }
    }
}

void Player::ReputationChanged(FactionEntry const* factionEntry)
{
    for (uint8 i = 0; i < MAX_QUEST_LOG_SIZE; ++i)
    {
        if (uint32 questid = GetQuestSlotQuestId(i))
        {
            if (Quest const* qInfo = sObjectMgr->GetQuestTemplate(questid))
            {
                if (qInfo->GetRepObjectiveFaction() == factionEntry->ID)
                {
                    QuestStatusData& q_status = m_QuestStatus[questid];
                    if (q_status.Status == QUEST_STATUS_INCOMPLETE)
                    {
                        if (GetReputationMgr().GetReputation(factionEntry) >= qInfo->GetRepObjectiveValue())
                            if (CanCompleteQuest(questid))
                                CompleteQuest(questid);
                    }
                    else if (q_status.Status == QUEST_STATUS_COMPLETE)
                    {
                        if (GetReputationMgr().GetReputation(factionEntry) < qInfo->GetRepObjectiveValue())
                            IncompleteQuest(questid);
                    }
                }
            }
        }
    }
}

void Player::ReputationChanged2(FactionEntry const* factionEntry)
{
    for (uint8 i = 0; i < MAX_QUEST_LOG_SIZE; ++i)
    {
        if (uint32 questid = GetQuestSlotQuestId(i))
        {
            if (Quest const* qInfo = sObjectMgr->GetQuestTemplate(questid))
            {
                if (qInfo->GetRepObjectiveFaction2() == factionEntry->ID)
                {
                    QuestStatusData& q_status = m_QuestStatus[questid];
                    if (q_status.Status == QUEST_STATUS_INCOMPLETE)
                    {
                        if (GetReputationMgr().GetReputation(factionEntry) >= qInfo->GetRepObjectiveValue2())
                            if (CanCompleteQuest(questid))
                                CompleteQuest(questid);
                    }
                    else if (q_status.Status == QUEST_STATUS_COMPLETE)
                    {
                        if (GetReputationMgr().GetReputation(factionEntry) < qInfo->GetRepObjectiveValue2())
                            IncompleteQuest(questid);
                    }
                }
            }
        }
    }
}

bool Player::HasQuestForItem(uint32 itemid, uint32 excludeQuestId /* 0 */, bool turnIn /* false */) const
{
    for (uint8 i = 0; i < MAX_QUEST_LOG_SIZE; ++i)
    {
        uint32 questid = GetQuestSlotQuestId(i);
        if (questid == 0 || questid == excludeQuestId)
            continue;

        QuestStatusMap::const_iterator qs_itr = m_QuestStatus.find(questid);
        if (qs_itr == m_QuestStatus.end())
            continue;

        QuestStatusData const& q_status = qs_itr->second;

        if ((q_status.Status == QUEST_STATUS_INCOMPLETE) || (turnIn && q_status.Status == QUEST_STATUS_COMPLETE))
        {
            Quest const* qinfo = sObjectMgr->GetQuestTemplate(questid);
            if (!qinfo)
                continue;

            // hide quest if player is in raid-group and quest is no raid quest
            if (GetGroup() && GetGroup()->isRaidGroup() && !qinfo->IsAllowedInRaid(GetMap()->GetDifficulty()))
                if (!InBattleground()) //there are two ways.. we can make every bg-quest a raidquest, or add this code here.. i don't know if this can be exploited by other quests, but i think all other quests depend on a specific area.. but keep this in mind, if something strange happens later
                    continue;

            // There should be no mixed ReqItem/ReqSource drop
            // This part for ReqItem drop
            for (uint8 j = 0; j < QUEST_ITEM_OBJECTIVES_COUNT; ++j)
            {
                if ((itemid == qinfo->RequiredItemId[j] && q_status.ItemCount[j] < qinfo->RequiredItemCount[j]) || (turnIn && q_status.ItemCount[j] >= qinfo->RequiredItemCount[j]))
                    return true;
            }
            // This part - for ReqSource
            for (uint8 j = 0; j < QUEST_SOURCE_ITEM_IDS_COUNT; ++j)
            {
                // examined item is a source item
                if (qinfo->ItemDrop[j] == itemid)
                {
                    ItemTemplate const* pProto = ASSERT_NOTNULL(sObjectMgr->GetItemTemplate(itemid));
                    uint32 ownedCount = GetItemCount(itemid, true);
                    // 'unique' item
                    if ((pProto->MaxCount && int32(ownedCount) < pProto->MaxCount) || (turnIn && int32(ownedCount) >= pProto->MaxCount))
                        return true;

                    // allows custom amount drop when not 0
                    if (qinfo->ItemDropQuantity[j])
                    {
                        if (ownedCount < qinfo->ItemDropQuantity[j] || turnIn )
                            return true;
                    } else if (ownedCount < pProto->GetMaxStackSize())
                        return true;
                }
            }
        }
    }
    return false;
}

void Player::SendQuestComplete(uint32 quest_id) const
{
    if (quest_id)
    {
        WorldPacket data(SMSG_QUESTUPDATE_COMPLETE, 4);
        data << uint32(quest_id);
        SendDirectMessage(&data);
    }
}

void Player::SendQuestReward(Quest const* quest, uint32 XP) const
{
    uint32 questid = quest->GetQuestId();
    sGameEventMgr->HandleQuestComplete(questid);
    WorldPacket data(SMSG_QUESTGIVER_QUEST_COMPLETE, (4+4+4+4+4));
    data << uint32(questid);

    if (!IsMaxLevel())
        data << uint32(XP);
    else
        data << uint32(0);

    data << uint32(quest->GetRewOrReqMoney(this));
    data << uint32(10 * quest->CalculateHonorGain(GetQuestLevel(quest)));
    // @tswow-begin add permanent rewards to the displayed bonus talents
    data << uint32(quest->GetBonusTalents() + (quest->GetBonusTalentsPerm() * sWorld->getRate(RATE_TALENT)));              // bonus talents
    // @tswow-end
    data << uint32(quest->GetRewArenaPoints());
    SendDirectMessage(&data);
}

void Player::SendQuestFailed(uint32 questId, InventoryResult reason) const
{
    if (questId)
    {
        WorldPacket data(SMSG_QUESTGIVER_QUEST_FAILED, 4 + 4);
        data << uint32(questId);
        data << uint32(reason);                             // failed reason (valid reasons: 4, 16, 50, 17, 74, other values show default message)
        SendDirectMessage(&data);
    }
}

void Player::SendQuestTimerFailed(uint32 quest_id) const
{
    if (quest_id)
    {
        WorldPacket data(SMSG_QUESTUPDATE_FAILEDTIMER, 4);
        data << uint32(quest_id);
        SendDirectMessage(&data);
        TC_LOG_DEBUG("network", "WORLD: Sent SMSG_QUESTUPDATE_FAILEDTIMER");
    }
}

void Player::SendCanTakeQuestResponse(QuestFailedReason msg) const
{
    WorldPacket data(SMSG_QUESTGIVER_QUEST_INVALID, 4);
    data << uint32(msg);
    SendDirectMessage(&data);
    TC_LOG_DEBUG("network", "WORLD: Sent SMSG_QUESTGIVER_QUEST_INVALID");
}

void Player::SendQuestConfirmAccept(Quest const* quest, Player* pReceiver) const
{
    if (pReceiver)
    {
        std::string strTitle = quest->GetTitle();

        LocaleConstant localeConstant = pReceiver->GetSession()->GetSessionDbLocaleIndex();
        if (localeConstant != LOCALE_enUS)
            if (QuestLocale const* pLocale = sObjectMgr->GetQuestLocale(quest->GetQuestId()))
                ObjectMgr::GetLocaleString(pLocale->Title, localeConstant, strTitle);

        WorldPacket data(SMSG_QUEST_CONFIRM_ACCEPT, (4 + strTitle.size() + 8));
        data << uint32(quest->GetQuestId());
        data << strTitle;
        data << uint64(GetGUID());
        pReceiver->SendDirectMessage(&data);

        TC_LOG_DEBUG("network", "WORLD: Sent SMSG_QUEST_CONFIRM_ACCEPT");
    }
}

void Player::SendPushToPartyResponse(Player const* player, QuestShareMessages msg) const
{
    if (player)
    {
        WorldPacket data(MSG_QUEST_PUSH_RESULT, 8 + 1);
        data << uint64(player->GetGUID());
        data << uint8(msg);                                 // valid values: 0-8
        SendDirectMessage(&data);
        TC_LOG_DEBUG("network", "WORLD: Sent MSG_QUEST_PUSH_RESULT");
    }
}

void Player::SendQuestUpdateAddItem(Quest const* /*quest*/, uint32 /*item_idx*/, uint16 /*count*/) const
{
    WorldPacket data(SMSG_QUESTUPDATE_ADD_ITEM, 0);
    TC_LOG_DEBUG("network", "WORLD: Sent SMSG_QUESTUPDATE_ADD_ITEM");
    //data << quest->RequiredItemId[item_idx];
    //data << count;
    SendDirectMessage(&data);
}

void Player::SendQuestUpdateAddCreatureOrGo(Quest const* quest, ObjectGuid guid, uint32 creatureOrGO_idx, uint16 old_count, uint16 add_count)
{
    ASSERT(old_count + add_count < 65536 && "mob/GO count store in 16 bits 2^16 = 65536 (0..65536)");

    int32 entry = quest->RequiredNpcOrGo[creatureOrGO_idx];
    if (entry < 0)
        // client expected gameobject template id in form (id|0x80000000)
        entry = (-entry) | 0x80000000;

    WorldPacket data(SMSG_QUESTUPDATE_ADD_KILL, (4*4+8));
    TC_LOG_DEBUG("network", "WORLD: Sent SMSG_QUESTUPDATE_ADD_KILL");
    data << uint32(quest->GetQuestId());
    data << uint32(entry);
    data << uint32(old_count + add_count);
    data << uint32(quest->RequiredNpcOrGoCount[ creatureOrGO_idx ]);
    data << uint64(guid);
    SendDirectMessage(&data);

    uint16 log_slot = FindQuestSlot(quest->GetQuestId());
    if (log_slot < MAX_QUEST_LOG_SIZE)
        SetQuestSlotCounter(log_slot, creatureOrGO_idx, GetQuestSlotCounter(log_slot, creatureOrGO_idx) + add_count);

    sScriptMgr->OnQuestObjectiveProgress(this, quest, creatureOrGO_idx, old_count + add_count);
}

void Player::SendQuestUpdateAddPlayer(Quest const* quest, uint16 old_count, uint16 add_count)
{
    ASSERT(old_count + add_count < 65536 && "player count store in 16 bits");

    WorldPacket data(SMSG_QUESTUPDATE_ADD_PVP_KILL, (3*4));
    data << uint32(quest->GetQuestId());
    data << uint32(old_count + add_count);
    data << uint32(quest->GetPlayersSlain());
    SendDirectMessage(&data);

    uint16 log_slot = FindQuestSlot(quest->GetQuestId());
    if (log_slot < MAX_QUEST_LOG_SIZE)
        SetQuestSlotCounter(log_slot, QUEST_PVP_KILL_SLOT, GetQuestSlotCounter(log_slot, QUEST_PVP_KILL_SLOT) + add_count);
}

void Player::SendQuestGiverStatusMultiple()
{
    uint32 count = 0;

    WorldPacket data(SMSG_QUESTGIVER_STATUS_MULTIPLE, 4);
    data << uint32(count);                                  // placeholder

    for (auto itr = m_clientGUIDs.begin(); itr != m_clientGUIDs.end(); ++itr)
    {
        uint32 questStatus = DIALOG_STATUS_NONE;

        if (itr->IsAnyTypeCreature())
        {
            // need also pet quests case support
            Creature* questgiver = ObjectAccessor::GetCreatureOrPetOrVehicle(*this, *itr);
            if (!questgiver || questgiver->IsHostileTo(this))
                continue;
            if (!questgiver->HasNpcFlag(UNIT_NPC_FLAG_QUESTGIVER))
                continue;

            questStatus = GetQuestDialogStatus(questgiver);

            data << uint64(questgiver->GetGUID());
            data << uint8(questStatus);
            ++count;
        }
        else if (itr->IsGameObject())
        {
            GameObject* questgiver = GetMap()->GetGameObject(*itr);
            if (!questgiver || questgiver->GetGoType() != GAMEOBJECT_TYPE_QUESTGIVER)
                continue;

            questStatus = GetQuestDialogStatus(questgiver);

            data << uint64(questgiver->GetGUID());
            data << uint8(questStatus);
            ++count;
        }
    }

    data.put<uint32>(0, count);                             // write real count
    SendDirectMessage(&data);
}

bool Player::HasPvPForcingQuest() const
{
    for (uint8 i = 0; i < MAX_QUEST_LOG_SIZE; ++i)
    {
        uint32 questId = GetQuestSlotQuestId(i);
        if (questId == 0)
            continue;

        Quest const* quest = sObjectMgr->GetQuestTemplate(questId);
        if (!quest)
            continue;

        if (quest->HasFlag(QUEST_FLAGS_FLAGS_PVP))
            return true;
    }

    return false;
}

bool Player::IsQuestRewarded(uint32 quest_id) const
{
    return m_RewardedQuests.find(quest_id) != m_RewardedQuests.end();
}

void Player::SetDailyQuestStatus(uint32 quest_id)
{
    if (Quest const* qQuest = sObjectMgr->GetQuestTemplate(quest_id))
    {
        if (!qQuest->IsDFQuest())
        {
            for (uint32 quest_daily_idx = 0; quest_daily_idx < PLAYER_MAX_DAILY_QUESTS; ++quest_daily_idx)
            {
                if (!GetUInt32Value(PLAYER_FIELD_DAILY_QUESTS_1+quest_daily_idx))
                {
                    SetUInt32Value(PLAYER_FIELD_DAILY_QUESTS_1+quest_daily_idx, quest_id);
                    m_lastDailyQuestTime = GameTime::GetGameTime();              // last daily quest time
                    m_DailyQuestChanged = true;
                    break;
                }
            }
        } else
        {
            m_DFQuests.insert(quest_id);
            m_lastDailyQuestTime = GameTime::GetGameTime();
            m_DailyQuestChanged = true;
        }
    }
}

bool Player::IsDailyQuestDone(uint32 quest_id)
{
    bool found = false;
    if (sObjectMgr->GetQuestTemplate(quest_id))
    {
        for (uint32 quest_daily_idx = 0; quest_daily_idx < PLAYER_MAX_DAILY_QUESTS; ++quest_daily_idx)
        {
            if (GetUInt32Value(PLAYER_FIELD_DAILY_QUESTS_1 + quest_daily_idx) == quest_id)
            {
                found = true;
                break;
            }
        }
    }

    return found;
}

void Player::SetWeeklyQuestStatus(uint32 quest_id)
{
    m_weeklyquests.insert(quest_id);
    m_WeeklyQuestChanged = true;
}

void Player::SetSeasonalQuestStatus(uint32 quest_id)
{
    Quest const* quest = sObjectMgr->GetQuestTemplate(quest_id);
    if (!quest)
        return;

    m_seasonalquests[quest->GetEventIdForQuest()].insert(quest_id);
    m_SeasonalQuestChanged = true;
}

void Player::SetMonthlyQuestStatus(uint32 quest_id)
{
    m_monthlyquests.insert(quest_id);
    m_MonthlyQuestChanged = true;
}

void Player::ResetDailyQuestStatus()
{
    for (uint32 quest_daily_idx = 0; quest_daily_idx < PLAYER_MAX_DAILY_QUESTS; ++quest_daily_idx)
        SetUInt32Value(PLAYER_FIELD_DAILY_QUESTS_1+quest_daily_idx, 0);

    m_DFQuests.clear(); // Dungeon Finder Quests.

    // DB data deleted in caller
    m_DailyQuestChanged = false;
    m_lastDailyQuestTime = 0;
}

void Player::ResetWeeklyQuestStatus()
{
    if (m_weeklyquests.empty())
        return;

    m_weeklyquests.clear();
    // DB data deleted in caller
    m_WeeklyQuestChanged = false;
}

void Player::ResetSeasonalQuestStatus(uint16 event_id)
{
    if (m_seasonalquests.empty() || m_seasonalquests[event_id].empty())
        return;

    m_seasonalquests.erase(event_id);
    // DB data deleted in caller
    m_SeasonalQuestChanged = false;
}

void Player::ResetMonthlyQuestStatus()
{
    if (m_monthlyquests.empty())
        return;

    m_monthlyquests.clear();
    // DB data deleted in caller
    m_MonthlyQuestChanged = false;
}

// @epoch-start
uint32 Player::GetXPForDifficulty(uint8 difficulty)
{
    QuestXPEntry const* xpentry = sQuestXPStore.LookupEntry(GetLevel());
    if (!xpentry)
        return 0;

    return Quest::RoundXPValue(xpentry->Difficulty[difficulty]);
}
//@epoch-end