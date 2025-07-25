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

uint8 Player::GetChatTag() const
{
    uint8 tag = CHAT_TAG_NONE;

    if (isGMChat())
        tag |= CHAT_TAG_GM;
    if (isDND())
        tag |= CHAT_TAG_DND;
    if (isAFK())
        tag |= CHAT_TAG_AFK;
    if (IsDeveloper())
        tag |= CHAT_TAG_DEV;

    return tag;
}

bool Player::CanJoinConstantChannelInZone(ChatChannelsEntry const* channel, AreaTableEntry const* zone) const
{
    if (channel->Flags & CHANNEL_DBC_FLAG_ZONE_DEP && zone->Flags & AREA_FLAG_NO_CHAT_CHANNELS)
        return false;

    if ((channel->Flags & CHANNEL_DBC_FLAG_CITY_ONLY) && (!(zone->Flags & AREA_FLAG_ALLOW_TRADE_CHANNEL)))
        return false;

    if ((channel->Flags & CHANNEL_DBC_FLAG_GUILD_REQ) && GetGuildId())
        return false;

    return true;
}

void Player::CleanupChannels()
{
    while (!m_channels.empty())
    {
        Channel* ch = *m_channels.begin();
        m_channels.erase(m_channels.begin());               // remove from player's channel list
        ch->LeaveChannel(this, false);                     // not send to client, not remove from player's channel list

        // delete channel if empty
        if (ChannelMgr* cMgr = ChannelMgr::forTeam(GetTeam()))
            if (ch->IsConstant())
                cMgr->LeftChannel(ch->GetChannelId(), ch->GetZoneEntry());
    }
    TC_LOG_DEBUG("chat.system", "Player::CleanupChannels: Channels of player '{}' ({}) cleaned up.", GetName(), GetGUID().ToString());
}

void Player::UpdateLocalChannels(uint32 newZone)
{
    if (GetSession()->PlayerLoading() && !IsBeingTeleportedFar())
        return;                                              // The client handles it automatically after loading, but not after teleporting

    // @epoch-start
    bool cancel = false;
    FIRE(
        Player
        , OnBeforeUpdateLocalChannels
        , TSPlayer(this)
        , TSMutable<bool, bool>(&cancel)
    );
    if (cancel)
        return;
    // @epoch-end

    AreaTableEntry const* current_zone = sAreaTableStore.LookupEntry(newZone);
    if (!current_zone)
        return;

    ChannelMgr* cMgr = ChannelMgr::forTeam(GetTeam());
    if (!cMgr)
        return;

    for (uint32 i = 0; i < sChatChannelsStore.GetNumRows(); ++i)
    {
        ChatChannelsEntry const* channelEntry = sChatChannelsStore.LookupEntry(i);
        if (!channelEntry)
            continue;

        Channel* usedChannel = nullptr;
        for (Channel* channel : m_channels)
        {
            if (channel->GetChannelId() == i)
            {
                usedChannel = channel;
                break;
            }
        }

        Channel* removeChannel = nullptr;
        Channel* joinChannel = nullptr;
        bool sendRemove = true;

        if (CanJoinConstantChannelInZone(channelEntry, current_zone))
        {
            if (!(channelEntry->Flags & CHANNEL_DBC_FLAG_GLOBAL))
            {
                if (channelEntry->Flags & CHANNEL_DBC_FLAG_CITY_ONLY && usedChannel)
                    continue;                            // Already on the channel, as city channel names are not changing

                joinChannel = cMgr->GetSystemChannel(channelEntry->ID, current_zone);
                if (usedChannel)
                {
                    if (joinChannel != usedChannel)
                    {
                        removeChannel = usedChannel;
                        sendRemove = false;              // Do not send leave channel, it already replaced at client
                    }
                    else
                        joinChannel = nullptr;
                }
            }
            else
                joinChannel = cMgr->GetSystemChannel(channelEntry->ID);
        }
        else
            removeChannel = usedChannel;

        if (joinChannel)
            joinChannel->JoinChannel(this);          // Changed Channel: ... or Joined Channel: ...

        if (removeChannel)
        {
            removeChannel->LeaveChannel(this, sendRemove);                                      // Leave old channel

            LeftChannel(removeChannel);                                                         // Remove from player's channel list
            cMgr->LeftChannel(removeChannel->GetChannelId(), removeChannel->GetZoneEntry());    // Delete if empty
        }
    }
}

void Player::Say(std::string_view text, Language language, WorldObject const* /*= nullptr*/)
{
    std::string _text(text);
    sScriptMgr->OnPlayerChat(this, CHAT_MSG_SAY, language, _text);

    WorldPacket data;
    ChatHandler::BuildChatPacket(data, CHAT_MSG_SAY, language, this, this, _text);
    SendMessageToSetInRange(&data, sWorld->getFloatConfig(CONFIG_LISTEN_RANGE_SAY), true, false, true);
}

void Player::Say(uint32 textId, WorldObject const* target /*= nullptr*/)
{
    Talk(textId, CHAT_MSG_SAY, sWorld->getFloatConfig(CONFIG_LISTEN_RANGE_SAY), target);
}

void Player::Yell(std::string_view text, Language language, WorldObject const* /*= nullptr*/)
{
    std::string _text(text);
    sScriptMgr->OnPlayerChat(this, CHAT_MSG_YELL, language, _text);

    WorldPacket data;
    ChatHandler::BuildChatPacket(data, CHAT_MSG_YELL, language, this, this, _text);
    SendMessageToSetInRange(&data, sWorld->getFloatConfig(CONFIG_LISTEN_RANGE_YELL), true, false, true);
}

void Player::Yell(uint32 textId, WorldObject const* target /*= nullptr*/)
{
    Talk(textId, CHAT_MSG_YELL, sWorld->getFloatConfig(CONFIG_LISTEN_RANGE_YELL), target);
}

void Player::TextEmote(std::string_view text, WorldObject const* /*= nullptr*/, bool /*= false*/)
{
    std::string _text(text);
    sScriptMgr->OnPlayerChat(this, CHAT_MSG_EMOTE, LANG_UNIVERSAL, _text);

    WorldPacket data;
    ChatHandler::BuildChatPacket(data, CHAT_MSG_EMOTE, LANG_UNIVERSAL, this, this, _text);
    SendMessageToSetInRange(&data, sWorld->getFloatConfig(CONFIG_LISTEN_RANGE_TEXTEMOTE), true, !sWorld->getBoolConfig(CONFIG_ALLOW_TWO_SIDE_INTERACTION_EMOTE), true);
}

void Player::TextEmote(uint32 textId, WorldObject const* target /*= nullptr*/, bool /*isBossEmote = false*/)
{
    Talk(textId, CHAT_MSG_EMOTE, sWorld->getFloatConfig(CONFIG_LISTEN_RANGE_TEXTEMOTE), target);
}

void Player::Whisper(std::string_view text, Language language, Player* target, bool /*= false*/)
{
    ASSERT(target);

    bool isAddonMessage = language == LANG_ADDON;

    if (!isAddonMessage)                                    // if not addon data
        language = LANG_UNIVERSAL;                          // whispers should always be readable

    std::string _text(text);
    sScriptMgr->OnPlayerChat(this, CHAT_MSG_WHISPER, language, _text, target);

    WorldPacket data;
    ChatHandler::BuildChatPacket(data, CHAT_MSG_WHISPER, Language(language), this, this, _text);
    target->SendDirectMessage(&data);

    // rest stuff shouldn't happen in case of addon message
    if (isAddonMessage)
        return;

    ChatHandler::BuildChatPacket(data, CHAT_MSG_WHISPER_INFORM, Language(language), target, target, _text);
    SendDirectMessage(&data);

    if (!isAcceptWhispers() && !IsGameMaster() && !target->IsGameMaster())
    {
        SetAcceptWhispers(true);
        ChatHandler(GetSession()).SendSysMessage(LANG_COMMAND_WHISPERON);
    }

    // announce afk or dnd message
    if (target->isAFK())
        ChatHandler(GetSession()).PSendSysMessage(LANG_PLAYER_AFK, target->GetName().c_str(), target->autoReplyMsg.c_str());
    else if (target->isDND())
        ChatHandler(GetSession()).PSendSysMessage(LANG_PLAYER_DND, target->GetName().c_str(), target->autoReplyMsg.c_str());
}

void Player::Whisper(uint32 textId, Player* target, bool /*isBossWhisper = false*/)
{
    if (!target)
        return;

    BroadcastText const* bct = sObjectMgr->GetBroadcastText(textId);
    if (!bct)
    {
        TC_LOG_ERROR("entities.unit", "WorldObject::MonsterWhisper: `broadcast_text` was not {} found", textId);
        return;
    }

    LocaleConstant locale = target->GetSession()->GetSessionDbLocaleIndex();
    WorldPacket data;
    ChatHandler::BuildChatPacket(data, CHAT_MSG_WHISPER, LANG_UNIVERSAL, this, target, bct->GetText(locale, GetGender()), 0, "", locale);
    target->SendDirectMessage(&data);
}

// TODO move to world session
void Player::UpdateSpeakTime(ChatFloodThrottle::Index index)
{
    // ignore chat spam protection for GMs in any mode
    if (GetSession()->HasPermission(rbac::RBAC_PERM_SKIP_CHECK_CHAT_SPAM))
        return;

    uint32 limit;
    uint32 delay;
    switch (index)
    {
        case ChatFloodThrottle::REGULAR:
            limit = sWorld->getIntConfig(CONFIG_CHATFLOOD_MESSAGE_COUNT);
            delay = sWorld->getIntConfig(CONFIG_CHATFLOOD_MESSAGE_DELAY);
            break;
        case ChatFloodThrottle::ADDON:
            limit = sWorld->getIntConfig(CONFIG_CHATFLOOD_ADDON_MESSAGE_COUNT);
            delay = sWorld->getIntConfig(CONFIG_CHATFLOOD_ADDON_MESSAGE_DELAY);
            break;
        default:
            return;
    }

    time_t current = GameTime::GetGameTime();
    if (m_chatFloodData[index].Time > current)
    {
        if (!limit)
            return;

        ++m_chatFloodData[index].Count;
        if (m_chatFloodData[index].Count >= limit)
        {
            // prevent overwrite mute time, if message send just before mutes set, for example.
            time_t new_mute = current + sWorld->getIntConfig(CONFIG_CHATFLOOD_MUTE_TIME);
            if (GetSession()->m_muteTime < new_mute)
                GetSession()->m_muteTime = new_mute;

            m_chatFloodData[index].Count = 0;
        }
    }
    else
        m_chatFloodData[index].Count = 1;

    m_chatFloodData[index].Time = current + delay;
}