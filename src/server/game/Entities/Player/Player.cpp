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

using namespace Trinity;

static uint32 corpseReclaimDelay[MAX_DEATH_COUNT] = { 30, 60, 120 };

uint32 const MAX_MONEY_AMOUNT = static_cast<uint32>(std::numeric_limits<int32>::max());

Player::Player(WorldSession* session): Unit(true)
// @tswow-begin
, m_msg_buffer(TSPlayer(this))
, m_db_json(DBJsonEntityType::PLAYER,0)
// @tswow-end
{
    m_objectType |= TYPEMASK_PLAYER;
    m_objectTypeId = TYPEID_PLAYER;

    m_valuesCount = PLAYER_END;

    m_session = session;

    m_ingametime = 0;
    m_sharedQuestId = 0;

    m_ExtraFlags = 0;

    m_spellModTakingSpell = nullptr;
    //m_pad = 0;

    // players always accept
    if (!GetSession()->HasPermission(rbac::RBAC_PERM_CAN_FILTER_WHISPERS))
        SetAcceptWhispers(true);

    m_usedTalentCount = 0;
    m_questRewardTalentCount = 0;
    // @tswow-begin
    m_questRewardPermTalentCount = 0;
    // @tswow-end

    m_regenTimer = 0;
    m_foodEmoteTimerCount = 0;
    m_carryHealthRegen = 0;
    m_weaponChangeTimer = 0;

    m_zoneUpdateId = uint32(-1);
    m_zoneUpdateTimer = 0;

    m_areaUpdateId = 0;
    m_team = 0;

    m_needsZoneUpdate = false;

    m_nextSave = sWorld->getIntConfig(CONFIG_INTERVAL_SAVE);

    memset(m_items, 0, sizeof(Item*)*PLAYER_SLOTS_COUNT);

    m_social = nullptr;

    // group is initialized in the reference constructor
    SetGroupInvite(nullptr);
    m_groupUpdateMask = 0;
    m_auraRaidUpdateMask = 0;
    m_bPassOnGroupLoot = false;

    m_GuildIdInvited = 0;
    m_ArenaTeamIdInvited = 0;

    m_atLoginFlags = AT_LOGIN_NONE;

    mSemaphoreTeleport_Near = false;
    mSemaphoreTeleport_Far = false;

    m_DelayedOperations = 0;
    m_bCanDelayTeleport = false;
    m_bHasDelayedTeleport = false;
    m_teleport_options = 0;
    // @epoch-begin
    m_canTeleport = false;
    m_canKnockback = false;
    // @epoch-end

    m_trade = nullptr;

    m_cinematic = 0;

    m_movie = 0;

    PlayerTalkClass = new PlayerMenu(GetSession());
    m_currentBuybackSlot = BUYBACK_SLOT_START;

    m_DailyQuestChanged = false;
    m_lastDailyQuestTime = 0;

    // Init rune flags
    for (uint8 i = 0; i < MAX_RUNES; ++i)
    {
        SetRuneTimer(i, 0xFFFFFFFF);
        SetLastRuneGraceTimer(i, 0);
    }

    for (uint8 i=0; i < MAX_TIMERS; i++)
        m_MirrorTimer[i] = DISABLED_MIRROR_TIMER;

    m_MirrorTimerFlags = UNDERWATER_NONE;
    m_MirrorTimerFlagsLast = UNDERWATER_NONE;
    m_hostileReferenceCheckTimer = 0;
    m_drunkTimer = 0;
    m_deathTimer = 0;
    m_deathExpireTime = 0;

    m_swingErrorMsg = 0;

    for (uint8 j = 0; j < PLAYER_MAX_BATTLEGROUND_QUEUES; ++j)
    {
        m_bgBattlegroundQueueID[j].bgQueueTypeId = BATTLEGROUND_QUEUE_NONE;
        m_bgBattlegroundQueueID[j].invitedToInstance = 0;
    }

    m_logintime = GameTime::GetGameTime();
    m_Last_tick = m_logintime;
    m_Played_time[PLAYED_TIME_TOTAL] = 0;
    m_Played_time[PLAYED_TIME_LEVEL] = 0;
    m_WeaponProficiency = 0;
    m_ArmorProficiency = 0;
    m_canParry = false;
    m_canBlock = false;
    m_canTitanGrip = false;
    m_titanGripPenaltySpellId = 0;
    m_ammoDPS = 0.0f;

    m_temporaryUnsummonedPetNumber = 0;
    //cache for UNIT_CREATED_BY_SPELL to allow
    //returning reagents for temporarily removed pets
    //when dying/logging out
    m_oldpetspell = 0;
    m_lastpetnumber = 0;

    ////////////////////Rest System/////////////////////
    _restTime = 0;
    inn_triggerId = 0;
    m_rest_bonus = 0;
    _restFlagMask = 0;
    ////////////////////Rest System/////////////////////

    m_mailsUpdated = false;
    unReadMails = 0;
    m_nextMailDelivereTime = 0;

    m_resetTalentsCost = 0;
    m_resetTalentsTime = 0;
    m_itemUpdateQueueBlocked = false;

    /////////////////// Instance System /////////////////////

    m_HomebindTimer = 0;
    m_InstanceValid = true;
    m_dungeonDifficulty = DUNGEON_DIFFICULTY_NORMAL;
    m_raidDifficulty = RAID_DIFFICULTY_10MAN_NORMAL;
    m_raidMapDifficulty = RAID_DIFFICULTY_10MAN_NORMAL;

    m_lastPotionId = 0;

    m_activeSpec = 0;
    m_specsCount = 1;

    for (uint8 i = 0; i < MAX_TALENT_SPECS; ++i)
    {
        for (uint8 g = 0; g < MAX_GLYPH_SLOT_INDEX; ++g)
            m_Glyphs[i][g] = 0;

        m_talents[i] = new PlayerTalentMap();
    }

    for (uint8 i = 0; i < BASEMOD_END; ++i)
    {
        m_auraBaseFlatMod[i] = 0.0f;
        m_auraBasePctMod[i] = 1.0f;
    }

    for (uint8 i = 0; i < MAX_COMBAT_RATING; i++)
        m_baseRatingValue[i] = 0;

    m_baseSpellPower = 0;
    m_baseFeralAP = 0;
    m_baseManaRegen = 0;
    m_baseHealthRegen = 0;
    m_spellPenetrationItemMod = 0;

    // Honor System
    m_lastHonorUpdateTime = GameTime::GetGameTime();

    m_IsBGRandomWinner = false;

    // Player summoning
    m_summon_expire = 0;

    m_seer = this;

    m_homebindMapId = 0;
    m_homebindAreaId = 0;
    m_homebindX = 0;
    m_homebindY = 0;
    m_homebindZ = 0;

    m_contestedPvPTimer = 0;

    m_declinedname = nullptr;

    m_isActive = true;

    m_runes = nullptr;

    m_lastFallTime = 0;
    m_lastFallZ = 0;

    m_grantableLevels = 0;
    m_fishingSteps = 0;

    m_ControlledByPlayer = true;

    sWorld->IncreasePlayerCount();

    m_ChampioningFaction = 0;

    for (uint8 i = 0; i < MAX_POWERS; ++i)
        m_powerFraction[i] = 0;

    isDebugAreaTriggers = false;

    m_WeeklyQuestChanged = false;

    m_MonthlyQuestChanged = false;

    m_SeasonalQuestChanged = false;

    SetPendingBind(0, 0);

    _activeCheats = CHEAT_NONE;
    healthBeforeDuel = 0;
    manaBeforeDuel = 0;

    _cinematicMgr = new CinematicMgr(this);

    m_achievementMgr = new AchievementMgr(this);
    m_reputationMgr = new ReputationMgr(this);

    m_groupUpdateTimer.Reset(5000);
}

Player::~Player()
{
    // it must be unloaded already in PlayerLogout and accessed only for logged in player
    //m_social = nullptr;

    // Note: buy back item already deleted from DB when player was saved
    for (uint8 i = 0; i < PLAYER_SLOTS_COUNT; ++i)
        delete m_items[i];

    for (uint8 i = 0; i < MAX_TALENT_SPECS; ++i)
    {
        for (PlayerTalentMap::const_iterator itr = m_talents[i]->begin(); itr != m_talents[i]->end(); ++itr)
            delete itr->second;
        delete m_talents[i];
    }

    //all mailed items should be deleted, also all mail should be deallocated
    for (PlayerMails::iterator itr = m_mail.begin(); itr != m_mail.end(); ++itr)
        delete *itr;

    for (ItemMap::iterator iter = mMitems.begin(); iter != mMitems.end(); ++iter)
        delete iter->second;                                //if item is duplicated... then server may crash ... but that item should be deallocated

    delete PlayerTalkClass;

    for (size_t x = 0; x < ItemSetEff.size(); x++)
        delete ItemSetEff[x];

    delete m_declinedname;
    delete m_runes;
    delete m_achievementMgr;
    delete m_reputationMgr;
    delete _cinematicMgr;

    sWorld->DecreasePlayerCount();
}

void Player::CleanupsBeforeDelete(bool finalCleanup)
{
    TradeCancel(false);
    DuelComplete(DUEL_INTERRUPTED);

    Unit::CleanupsBeforeDelete(finalCleanup);

    // clean up player-instance binds, may unload some instance saves
    for (uint8 i = 0; i < MAX_DIFFICULTY; ++i)
        for (BoundInstancesMap::iterator itr = m_boundInstances[i].begin(); itr != m_boundInstances[i].end(); ++itr)
            itr->second.save->RemovePlayer(this);
}

void Player::Initialize(ObjectGuid::LowType guid)
{
    Object::_Create(guid, 0, HighGuid::Player);
}

bool Player::IsLoading() const
{
    return GetSession()->PlayerLoading();
}

bool Player::Create(ObjectGuid::LowType guidlow, CharacterCreateInfo* createInfo)
{
    //FIXME: outfitId not used in player creating
    /// @todo need more checks against packet modifications

    Object::_Create(guidlow, 0, HighGuid::Player);
    // @tswow-begin
    m_db_json = TSDBJson(DBJsonEntityType::PLAYER, GetGUID().GetRawValue());
    // @tswow-end

    m_name = createInfo->Name;

    PlayerInfo const* info = sObjectMgr->GetPlayerInfo(createInfo->Race, createInfo->Class);
    if (!info)
    {
        TC_LOG_ERROR("entities.player.cheat", "Player::Create: Possible hacking attempt: Account {} tried to create a character named '{}' with an invalid race/class pair ({}/{}) - refusing to do so.",
                GetSession()->GetAccountId(), m_name, createInfo->Race, createInfo->Class);
        return false;
    }

    for (uint8 i = 0; i < PLAYER_SLOTS_COUNT; i++)
        m_items[i] = nullptr;

    Relocate(info->positionX, info->positionY, info->positionZ, info->orientation);

    ChrClassesEntry const* cEntry = sChrClassesStore.LookupEntry(createInfo->Class);
    if (!cEntry)
    {
        TC_LOG_ERROR("entities.player.cheat", "Player::Create: Possible hacking attempt: Account {} tried to create a character named '{}' with an invalid character class ({}) - refusing to do so (wrong DBC-files?)",
                GetSession()->GetAccountId(), m_name, createInfo->Class);
        return false;
    }

    SetMap(sMapMgr->CreateMap(info->mapId, GetPosition(), this));

    uint8 powertype = cEntry->DisplayPower;

    SetObjectScale(1.0f);

    SetFactionForRace(createInfo->Race);

    if (!IsValidGender(createInfo->Gender))
    {
        TC_LOG_ERROR("entities.player.cheat", "Player::Create: Possible hacking attempt: Account {} tried to create a character named '{}' with an invalid gender ({}) - refusing to do so",
                GetSession()->GetAccountId(), m_name, createInfo->Gender);
        return false;
    }

    if (!ValidateAppearance(createInfo->Race, createInfo->Class, createInfo->Gender, createInfo->HairStyle, createInfo->HairColor, createInfo->Face, createInfo->FacialHair, createInfo->Skin, true))
    {
        TC_LOG_ERROR("entities.player.cheat", "Player::Create: Possible hacking attempt: Account {} tried to create a character named '{}' with invalid appearance attributes - refusing to do so",
            GetSession()->GetAccountId(), m_name);
        return false;
    }

    SetRace(createInfo->Race);
    SetClass(createInfo->Class);
    SetGender(Gender(createInfo->Gender));
    SetPowerType(Powers(powertype), false);
    InitDisplayIds();
    if (sWorld->getIntConfig(CONFIG_GAME_TYPE) == REALM_TYPE_PVP || sWorld->getIntConfig(CONFIG_GAME_TYPE) == REALM_TYPE_RPPVP)
    {
        SetPvpFlag(UNIT_BYTE2_FLAG_PVP);
        SetUnitFlag(UNIT_FLAG_PLAYER_CONTROLLED);
    }

    SetModCastingSpeed(1.0f);               // fix cast time showed in spell tooltip on client
    SetHoverHeight(1.0f);            // default for players in 3.0.3

    SetInt32Value(PLAYER_FIELD_WATCHED_FACTION_INDEX, uint32(-1));  // -1 is default value

    SetSkinId(createInfo->Skin);
    SetFaceId(createInfo->Face);
    SetHairStyleId(createInfo->HairStyle);
    SetHairColorId(createInfo->HairColor);
    SetFacialStyle(createInfo->FacialHair);
    SetRestState((GetSession()->IsARecruiter() || GetSession()->GetRecruiterId() != 0) ? REST_STATE_RAF_LINKED : REST_STATE_NOT_RAF_LINKED);
    SetNativeGender(Gender(createInfo->Gender));
    SetArenaFaction(0);

    SetUInt32Value(PLAYER_GUILDID, 0);
    SetRank(0);
    SetUInt32Value(PLAYER_GUILD_TIMESTAMP, 0);

    for (int i = 0; i < KNOWN_TITLES_SIZE; ++i)
        SetUInt64Value(PLAYER__FIELD_KNOWN_TITLES + i, 0);  // 0=disabled
    SetUInt32Value(PLAYER_CHOSEN_TITLE, 0);

    SetUInt32Value(PLAYER_FIELD_KILLS, 0);
    SetUInt32Value(PLAYER_FIELD_LIFETIME_HONORABLE_KILLS, 0);
    SetUInt32Value(PLAYER_FIELD_TODAY_CONTRIBUTION, 0);
    SetUInt32Value(PLAYER_FIELD_YESTERDAY_CONTRIBUTION, 0);

    // set starting level
    uint32 start_level = GetClass() != CLASS_DEATH_KNIGHT
        ? sWorld->getIntConfig(CONFIG_START_PLAYER_LEVEL)
        : sWorld->getIntConfig(CONFIG_START_DEATH_KNIGHT_PLAYER_LEVEL);

    if (m_session->HasPermission(rbac::RBAC_PERM_USE_START_GM_LEVEL))
    {
        uint32 gm_level = GetClass() != CLASS_DEATH_KNIGHT
            ? sWorld->getIntConfig(CONFIG_START_GM_LEVEL)
            : std::max(sWorld->getIntConfig(CONFIG_START_GM_LEVEL), sWorld->getIntConfig(CONFIG_START_DEATH_KNIGHT_PLAYER_LEVEL));

        if (gm_level > start_level)
            start_level = gm_level;
    }

    SetLevel(start_level, false);
    // @tswow-begin  -- autolearn all spells from level 1 at character creation
    ApplyAutolearnSpells(0);
    // @tswow-end

    InitRunes();

    SetMoney(sWorld->getIntConfig(CONFIG_START_PLAYER_MONEY));
    SetHonorPoints(sWorld->getIntConfig(CONFIG_START_HONOR_POINTS));
    SetArenaPoints(sWorld->getIntConfig(CONFIG_START_ARENA_POINTS));

    // Played time
    m_Last_tick = GameTime::GetGameTime();
    m_Played_time[PLAYED_TIME_TOTAL] = 0;
    m_Played_time[PLAYED_TIME_LEVEL] = 0;

    // base stats and related field values
    InitStatsForLevel();
    InitTaxiNodesForLevel();
    InitGlyphsForLevel();
    InitTalentForLevel();
    InitPrimaryProfessions();                               // to max set before any spell added

    // apply original stats mods before spell loading or item equipment that call before equip _RemoveStatsMods()
    UpdateMaxHealth();                                      // Update max Health (for add bonus from stamina)
    SetFullHealth();
    SetFullPower(POWER_MANA);

    // original spells
    LearnDefaultSkills();
    LearnCustomSpells();

    // original action bar
    for (PlayerCreateInfoActions::const_iterator action_itr = info->action.begin(); action_itr != info->action.end(); ++action_itr)
        addActionButton(action_itr->button, action_itr->action, action_itr->type);

    // original items
    if (CharStartOutfitEntry const* oEntry = GetCharStartOutfitEntry(createInfo->Race, createInfo->Class, createInfo->Gender))
    {
        for (int j = 0; j < MAX_OUTFIT_ITEMS; ++j)
        {
            if (oEntry->ItemID[j] <= 0)
                continue;

            uint32 itemId = oEntry->ItemID[j];

            // just skip, reported in ObjectMgr::LoadItemTemplates
            ItemTemplate const* iProto = sObjectMgr->GetItemTemplate(itemId);
            if (!iProto)
                continue;

            // BuyCount by default
            uint32 count = iProto->BuyCount;

            // special amount for food/drink
            if (iProto->Class == ITEM_CLASS_CONSUMABLE && iProto->SubClass == ITEM_SUBCLASS_FOOD)
            {
                switch (iProto->Spells[0].SpellCategory)
                {
                    case SPELL_CATEGORY_FOOD:                                // food
                        count = GetClass() == CLASS_DEATH_KNIGHT ? 10 : 4;
                        break;
                    case SPELL_CATEGORY_DRINK:                                // drink
                        count = 2;
                        break;
                }
                if (iProto->GetMaxStackSize() < count)
                    count = iProto->GetMaxStackSize();
            }
            StoreNewItemInBestSlots(itemId, count);
        }
    }

    for (PlayerCreateInfoItems::const_iterator item_id_itr = info->item.begin(); item_id_itr != info->item.end(); ++item_id_itr)
        StoreNewItemInBestSlots(item_id_itr->item_id, item_id_itr->item_amount);

    // bags and main-hand weapon must equipped at this moment
    // now second pass for not equipped (offhand weapon/shield if it attempt equipped before main-hand weapon)
    // or ammo not equipped in special bag
    for (uint8 i = INVENTORY_SLOT_ITEM_START; i < INVENTORY_SLOT_ITEM_END; i++)
    {
        if (Item* pItem = GetItemByPos(INVENTORY_SLOT_BAG_0, i))
        {
            uint16 eDest;
            // equip offhand weapon/shield if it attempt equipped before main-hand weapon
            InventoryResult msg = CanEquipItem(NULL_SLOT, eDest, pItem, false);
            if (msg == EQUIP_ERR_OK)
            {
                RemoveItem(INVENTORY_SLOT_BAG_0, i, true);
                EquipItem(eDest, pItem, true);
            }
            // move other items to more appropriate slots (ammo not equipped in special bag)
            else
            {
                ItemPosCountVec sDest;
                msg = CanStoreItem(NULL_BAG, NULL_SLOT, sDest, pItem, false);
                if (msg == EQUIP_ERR_OK)
                {
                    RemoveItem(INVENTORY_SLOT_BAG_0, i, true);
                    StoreItem(sDest, pItem, true);
                }

                // if  this is ammo then use it
                msg = CanUseAmmo(pItem->GetEntry());
                if (msg == EQUIP_ERR_OK)
                    SetAmmo(pItem->GetEntry());
            }
        }
    }
    // all item positions resolved

    GetThreatManager().Initialize();

    return true;
}

void Player::SendMirrorTimer(MirrorTimerType Type, uint32 MaxValue, uint32 CurrentValue, int32 Regen)
{
    if (int(MaxValue) == DISABLED_MIRROR_TIMER)
    {
        if (int(CurrentValue) != DISABLED_MIRROR_TIMER)
            StopMirrorTimer(Type);
        return;
    }

    SendDirectMessage(WorldPackets::Misc::StartMirrorTimer(Type, CurrentValue, MaxValue, Regen, false, 0).Write());
}

void Player::StopMirrorTimer(MirrorTimerType Type)
{
    m_MirrorTimer[Type] = DISABLED_MIRROR_TIMER;
    SendDirectMessage(WorldPackets::Misc::StopMirrorTimer(Type).Write());
}

bool Player::IsImmuneToEnvironmentalDamage() const
{
    // check for GM and death state included in isAttackableByAOE
    return !isTargetableForAttack(false);
}

uint32 Player::EnvironmentalDamage(EnviromentalDamage type, uint32 damage)
{
    if (IsImmuneToEnvironmentalDamage())
        return 0;

    // Absorb, resist some environmental damage type
    uint32 absorb = 0;
    uint32 resist = 0;
    switch (type)
    {
        case DAMAGE_LAVA:
        case DAMAGE_SLIME:
        case DAMAGE_FIRE:
        {
            DamageInfo dmgInfo(this, this, damage, nullptr, type == DAMAGE_SLIME ? SPELL_SCHOOL_MASK_NATURE : SPELL_SCHOOL_MASK_FIRE, DIRECT_DAMAGE, BASE_ATTACK);
            Unit::CalcAbsorbResist(dmgInfo);
            absorb = dmgInfo.GetAbsorb();
            resist = dmgInfo.GetResist();
            damage = dmgInfo.GetDamage();
            break;
        }
        default:
            break;
    }

    Unit::DealDamageMods(this, damage, &absorb);

    WorldPackets::CombatLog::EnvironmentalDamageLog packet;
    packet.Victim = GetGUID();
    packet.Type = type != DAMAGE_FALL_TO_VOID ? type : DAMAGE_FALL;
    packet.Amount = damage;
    packet.Absorbed = absorb;
    packet.Resisted = resist;
    SendMessageToSet(packet.Write(), true);

    uint32 final_damage = Unit::DealDamage(this, this, damage, nullptr, SELF_DAMAGE, SPELL_SCHOOL_MASK_NORMAL, nullptr, false);

    if (!IsAlive())
    {
        if (type == DAMAGE_FALL)                               // DealDamage does not apply item durability loss from self-induced damage.
        {
            TC_LOG_DEBUG("entities.player", "Player::EnvironmentalDamage: Player '{}' ({}) fall to death, losing {} durability",
                GetName(), GetGUID().ToString(), sWorld->getRate(RATE_DURABILITY_LOSS_ON_DEATH));
            DurabilityLossAll(sWorld->getRate(RATE_DURABILITY_LOSS_ON_DEATH), false);
            // durability lost message
            SendDurabilityLoss();
        }

        UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_DEATHS_FROM, 1, type);
    }

    return final_damage;
}

int32 Player::getMaxTimer(MirrorTimerType timer) const
{
    switch (timer)
    {
        case FATIGUE_TIMER:
            return MINUTE * IN_MILLISECONDS;
        case BREATH_TIMER:
        {
            if (!IsAlive() || HasAuraType(SPELL_AURA_WATER_BREATHING) || GetSession()->GetSecurity() >= AccountTypes(sWorld->getIntConfig(CONFIG_DISABLE_BREATHING)))
                return DISABLED_MIRROR_TIMER;

            int32 UnderWaterTime = 1 * MINUTE * IN_MILLISECONDS;
            UnderWaterTime *= GetTotalAuraMultiplier(SPELL_AURA_MOD_WATER_BREATHING);
            return UnderWaterTime;
        }
        case FIRE_TIMER:
        {
            if (!IsAlive())
                return DISABLED_MIRROR_TIMER;
            return 1 * IN_MILLISECONDS;
        }
        default:
            return 0;
    }
}

void Player::UpdateMirrorTimers()
{
    // Desync flags for update on next HandleDrowning
    if (m_MirrorTimerFlags)
        m_MirrorTimerFlagsLast = ~m_MirrorTimerFlags;
}

void Player::StopMirrorTimers()
{
    StopMirrorTimer(FATIGUE_TIMER);
    StopMirrorTimer(BREATH_TIMER);
    StopMirrorTimer(FIRE_TIMER);
}

bool Player::IsMirrorTimerActive(MirrorTimerType type) const
{
    return m_MirrorTimer[type] == getMaxTimer(type);
}

void Player::HandleDrowning(uint32 time_diff)
{
    if (!m_MirrorTimerFlags)
        return;

    // In water
    if (m_MirrorTimerFlags & UNDERWATER_INWATER)
    {
        // Breath timer not activated - activate it
        if (m_MirrorTimer[BREATH_TIMER] == DISABLED_MIRROR_TIMER)
        {
            m_MirrorTimer[BREATH_TIMER] = getMaxTimer(BREATH_TIMER);
            SendMirrorTimer(BREATH_TIMER, m_MirrorTimer[BREATH_TIMER], m_MirrorTimer[BREATH_TIMER], -1);
        }
        else                                                              // If activated - do tick
        {
            m_MirrorTimer[BREATH_TIMER] -= time_diff;
            // Timer limit - need deal damage
            if (m_MirrorTimer[BREATH_TIMER] < 0)
            {
                m_MirrorTimer[BREATH_TIMER] += 1 * IN_MILLISECONDS;
                // Calculate and deal damage
                /// @todo Check this formula
                uint32 damage = GetMaxHealth() / 5 + urand(0, GetLevel() - 1);
                EnvironmentalDamage(DAMAGE_DROWNING, damage);
            }
            else if (!(m_MirrorTimerFlagsLast & UNDERWATER_INWATER))      // Update time in client if need
                SendMirrorTimer(BREATH_TIMER, getMaxTimer(BREATH_TIMER), m_MirrorTimer[BREATH_TIMER], -1);
        }
    }
    else if (m_MirrorTimer[BREATH_TIMER] != DISABLED_MIRROR_TIMER)        // Regen timer
    {
        int32 UnderWaterTime = getMaxTimer(BREATH_TIMER);
        // Need breath regen
        m_MirrorTimer[BREATH_TIMER] += 10 * time_diff;
        if (m_MirrorTimer[BREATH_TIMER] >= UnderWaterTime || !IsAlive())
            StopMirrorTimer(BREATH_TIMER);
        else if (m_MirrorTimerFlagsLast & UNDERWATER_INWATER)
            SendMirrorTimer(BREATH_TIMER, UnderWaterTime, m_MirrorTimer[BREATH_TIMER], 10);
    }

    // In dark water
    if (m_MirrorTimerFlags & UNDERWATER_INDARKWATER)
    {
        // Fatigue timer not activated - activate it
        if (m_MirrorTimer[FATIGUE_TIMER] == DISABLED_MIRROR_TIMER)
        {
            m_MirrorTimer[FATIGUE_TIMER] = getMaxTimer(FATIGUE_TIMER);
            SendMirrorTimer(FATIGUE_TIMER, m_MirrorTimer[FATIGUE_TIMER], m_MirrorTimer[FATIGUE_TIMER], -1);
        }
        else
        {
            m_MirrorTimer[FATIGUE_TIMER] -= time_diff;
            // Timer limit - need deal damage or teleport ghost to graveyard
            if (m_MirrorTimer[FATIGUE_TIMER] < 0)
            {
                m_MirrorTimer[FATIGUE_TIMER] += 1 * IN_MILLISECONDS;
                if (IsAlive())                                            // Calculate and deal damage
                {
                    uint32 damage = GetMaxHealth() / 5 + urand(0, GetLevel() - 1);
                    EnvironmentalDamage(DAMAGE_EXHAUSTED, damage);
                }
                else if (HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_GHOST))       // Teleport ghost to graveyard
                    RepopAtGraveyard();
            }
            else if (!(m_MirrorTimerFlagsLast & UNDERWATER_INDARKWATER))
                SendMirrorTimer(FATIGUE_TIMER, getMaxTimer(FATIGUE_TIMER), m_MirrorTimer[FATIGUE_TIMER], -1);
        }
    }
    else if (m_MirrorTimer[FATIGUE_TIMER] != DISABLED_MIRROR_TIMER)       // Regen timer
    {
        int32 DarkWaterTime = getMaxTimer(FATIGUE_TIMER);
        m_MirrorTimer[FATIGUE_TIMER] += 10 * time_diff;
        if (m_MirrorTimer[FATIGUE_TIMER] >= DarkWaterTime || !IsAlive())
            StopMirrorTimer(FATIGUE_TIMER);
        else if (m_MirrorTimerFlagsLast & UNDERWATER_INDARKWATER)
            SendMirrorTimer(FATIGUE_TIMER, DarkWaterTime, m_MirrorTimer[FATIGUE_TIMER], 10);
    }

    if (m_MirrorTimerFlags & (UNDERWATER_INLAVA /*| UNDERWATER_INSLIME*/) && !(_lastLiquid && _lastLiquid->SpellID))
    {
        // Breath timer not activated - activate it
        if (m_MirrorTimer[FIRE_TIMER] == DISABLED_MIRROR_TIMER)
            m_MirrorTimer[FIRE_TIMER] = getMaxTimer(FIRE_TIMER);
        else
        {
            m_MirrorTimer[FIRE_TIMER] -= time_diff;
            if (m_MirrorTimer[FIRE_TIMER] < 0)
            {
                m_MirrorTimer[FIRE_TIMER] += 1 * IN_MILLISECONDS;
                // Calculate and deal damage
                /// @todo Check this formula
                uint32 damage = urand(600, 700);
                if (m_MirrorTimerFlags & UNDERWATER_INLAVA)
                    EnvironmentalDamage(DAMAGE_LAVA, damage);
                // need to skip Slime damage in Undercity,
                // maybe someone can find better way to handle environmental damage
                //else if (m_zoneUpdateId != 1497)
                //    EnvironmentalDamage(DAMAGE_SLIME, damage);
            }
        }
    }
    else
        m_MirrorTimer[FIRE_TIMER] = DISABLED_MIRROR_TIMER;

    // Recheck timers flag
    m_MirrorTimerFlags &= ~UNDERWATER_EXIST_TIMERS;
    for (uint8 i = 0; i < MAX_TIMERS; ++i)
    {
        if (m_MirrorTimer[i] != DISABLED_MIRROR_TIMER)
        {
            m_MirrorTimerFlags |= UNDERWATER_EXIST_TIMERS;
            break;
        }
    }
    m_MirrorTimerFlagsLast = m_MirrorTimerFlags;
}

///The player sobers by 1% every 9 seconds
void Player::HandleSobering()
{
    m_drunkTimer = 0;

    uint8 currentDrunkValue = GetDrunkValue();
    uint8 drunk = currentDrunkValue ? --currentDrunkValue : 0;
    SetDrunkValue(drunk);
}

/*static*/ DrunkenState Player::GetDrunkenstateByValue(uint8 value)
{
    if (value >= 90)
        return DRUNKEN_SMASHED;
    if (value >= 50)
        return DRUNKEN_DRUNK;
    if (value)
        return DRUNKEN_TIPSY;
    return DRUNKEN_SOBER;
}

void Player::SetDrunkValue(uint8 newDrunkValue, uint32 itemId /*= 0*/)
{
    newDrunkValue = std::min<uint8>(newDrunkValue, 100);
    if (newDrunkValue == GetDrunkValue())
        return;

    uint32 oldDrunkenState = Player::GetDrunkenstateByValue(GetDrunkValue());
    uint32 newDrunkenState = Player::GetDrunkenstateByValue(newDrunkValue);

    SetByteValue(PLAYER_BYTES_3, PLAYER_BYTES_3_OFFSET_INEBRIATION, newDrunkValue);
    UpdateInvisibilityDrunkDetect();

    m_drunkTimer = 0; // reset sobering timer

    if (newDrunkenState == oldDrunkenState)
        return;

    WorldPackets::Misc::CrossedInebriationThreshold data;
    data.Guid = GetGUID();
    data.Threshold = newDrunkenState;
    data.ItemID = itemId;

    SendMessageToSet(data.Write(), true);
}

void Player::UpdateInvisibilityDrunkDetect()
{
    // select drunk percent or total SPELL_AURA_MOD_FAKE_INEBRIATE amount, whichever is higher for visibility updates
    uint8 drunkValue        = GetDrunkValue();
    int32 fakeDrunkValue    = GetFakeDrunkValue();
    int32 maxDrunkValue     = std::max<int32>(drunkValue, fakeDrunkValue);

    if (maxDrunkValue != 0)
    {
        m_invisibilityDetect.AddFlag(INVISIBILITY_DRUNK);
        m_invisibilityDetect.SetValue(INVISIBILITY_DRUNK, maxDrunkValue);
    }
    else
        m_invisibilityDetect.DelFlag(INVISIBILITY_DRUNK);

    if (IsInWorld())
        UpdateObjectVisibility();
}

void Player::Update(uint32 p_time)
{
    if (!IsInWorld())
        return;

    ZoneScopedN("Player::Update")

    // undelivered mail
    if (m_nextMailDelivereTime && m_nextMailDelivereTime <= GameTime::GetGameTime())
    {
        SendNewMail();
        ++unReadMails;

        // It will be recalculate at mailbox open (for unReadMails important non-0 until mailbox open, it also will be recalculated)
        m_nextMailDelivereTime = 0;
    }

    // Update cinematic location, if 500ms have passed and we're doing a cinematic now.
    _cinematicMgr->m_cinematicDiff += p_time;
    if (_cinematicMgr->m_cinematicCamera && _cinematicMgr->m_activeCinematicCameraId && GetMSTimeDiffToNow(_cinematicMgr->m_lastCinematicCheck) > CINEMATIC_UPDATEDIFF)
    {
        _cinematicMgr->m_lastCinematicCheck = GameTime::GetGameTimeMS();
        _cinematicMgr->UpdateCinematicLocation(p_time);
    }

    //used to implement delayed far teleports
    SetCanDelayTeleport(true);
    ExecuteSortedCastRequests();
    Unit::Update(p_time);
    SetCanDelayTeleport(false);

    time_t now = GameTime::GetGameTime();

    UpdatePvPFlag(now);

    UpdateContestedPvP(p_time);

    UpdateDuelFlag(now);

    CheckDuelDistance(now);

    UpdateAfkReport(now);

    Unit::AIUpdateTick(p_time);

    // Once per second, update items that have just a limited lifetime
    if (now > m_Last_tick)
    {
        UpdateItemDuration(uint32(now - m_Last_tick));
        UpdateSoulboundTradeItems();
    }

    // If mute expired, remove it from the DB
    if (GetSession()->m_muteTime && GetSession()->m_muteTime < now)
    {
        GetSession()->m_muteTime = 0;
        LoginDatabasePreparedStatement* stmt = LoginDatabase.GetPreparedStatement(LOGIN_UPD_MUTE_TIME);
        stmt->setInt64(0, 0); // Set the mute time to 0
        stmt->setString(1, "");
        stmt->setString(2, "");
        stmt->setUInt32(3, GetSession()->GetAccountId());
        LoginDatabase.Execute(stmt);
    }

    if (!m_timedquests.empty())
    {
        QuestSet::iterator iter = m_timedquests.begin();
        while (iter != m_timedquests.end())
        {
            QuestStatusData& q_status = m_QuestStatus[*iter];
            if (q_status.Timer <= p_time)
            {
                uint32 quest_id  = *iter;
                ++iter;                                     // current iter will be removed in FailQuest
                FailQuest(quest_id);
            }
            else
            {
                q_status.Timer -= p_time;
                m_QuestStatusSave[*iter] = QUEST_DEFAULT_SAVE_TYPE;
                ++iter;
            }
        }
    }

    m_achievementMgr->UpdateTimedAchievements(p_time);

    if (HasUnitState(UNIT_STATE_MELEE_ATTACKING) && !HasUnitState(UNIT_STATE_CASTING | UNIT_STATE_CHARGING))
    {
        if (Unit* victim = GetVictim())
        {
            // default combat reach 10
            /// @todo add weapon, skill check

            if (isAttackReady(BASE_ATTACK))
            {
                if (!IsWithinMeleeRange(victim))
                {
                    setAttackTimer(BASE_ATTACK, 100);
                    if (m_swingErrorMsg != 1)               // send single time (client auto repeat)
                    {
                        SendAttackSwingNotInRange();
                        m_swingErrorMsg = 1;
                    }
                }
                //120 degrees of radiant range
                else if (!HasInArc(2 * float(M_PI) / 3, victim))
                {
                    setAttackTimer(BASE_ATTACK, 100);
                    if (m_swingErrorMsg != 2)               // send single time (client auto repeat)
                    {
                        SendAttackSwingBadFacingAttack();
                        m_swingErrorMsg = 2;
                    }
                }
                else
                {
                    m_swingErrorMsg = 0;                    // reset swing error state

                    // prevent base and off attack in same time, delay attack at 0.2 sec
                    if (haveOffhandWeapon())
                        if (getAttackTimer(OFF_ATTACK) < ATTACK_DISPLAY_DELAY)
                            setAttackTimer(OFF_ATTACK, ATTACK_DISPLAY_DELAY);

                    // do attack
                    AttackerStateUpdate(victim, BASE_ATTACK);
                    resetAttackTimer(BASE_ATTACK);
                }
            }

            if (haveOffhandWeapon() && isAttackReady(OFF_ATTACK))
            {
                if (!IsWithinMeleeRange(victim))
                    setAttackTimer(OFF_ATTACK, 100);
                else if (!HasInArc(2 * float(M_PI) / 3, victim))
                    setAttackTimer(OFF_ATTACK, 100);
                else
                {
                    // prevent base and off attack in same time, delay attack at 0.2 sec
                    if (getAttackTimer(BASE_ATTACK) < ATTACK_DISPLAY_DELAY)
                        setAttackTimer(BASE_ATTACK, ATTACK_DISPLAY_DELAY);

                    // do attack
                    AttackerStateUpdate(victim, OFF_ATTACK);
                    resetAttackTimer(OFF_ATTACK);
                }
            }

            /*Unit* owner = victim->GetOwner();
            Unit* u = owner ? owner : victim;
            if (u->IsPvP() && (!duel || duel->opponent != u))
            {
                UpdatePvP(true);
                RemoveAurasWithInterruptFlags(AURA_INTERRUPT_FLAG_ENTER_PVP_COMBAT);
            }*/
        }
    }

    if (HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_RESTING))
    {
        if (roll_chance_i(3) && _restTime > 0)      // freeze update
        {
            time_t currTime = GameTime::GetGameTime();
            time_t timeDiff = currTime - _restTime;
            if (timeDiff >= 10)                             // freeze update
            {
                _restTime = currTime;

                float bubble = 0.125f * sWorld->getRate(RATE_REST_INGAME);
                float extraPerSec = ((float)GetUInt32Value(PLAYER_NEXT_LEVEL_XP) / 72000.0f) * bubble;

                // speed collect rest bonus (section/in hour)
                float currRestBonus = GetRestBonus();
                SetRestBonus(currRestBonus + timeDiff * extraPerSec);
            }
        }
    }

    if (m_weaponChangeTimer > 0)
    {
        if (p_time >= m_weaponChangeTimer)
            m_weaponChangeTimer = 0;
        else
            m_weaponChangeTimer -= p_time;
    }

    if (m_zoneUpdateTimer > 0)
    {
        if (p_time >= m_zoneUpdateTimer)
        {
            // On zone update tick check if we are still in an inn if we are supposed to be in one
            if (HasRestFlag(REST_FLAG_IN_TAVERN))
            {
                AreaTriggerEntry const* atEntry = sAreaTriggerStore.LookupEntry(GetInnTriggerId());
                if (!atEntry || !IsInAreaTriggerRadius(atEntry))
                    RemoveRestFlag(REST_FLAG_IN_TAVERN);
            }

            uint32 newzone, newarea;
            GetZoneAndAreaId(newzone, newarea);

            if (m_zoneUpdateId != newzone)
                UpdateZone(newzone, newarea);                // also update area
            else
            {
                // use area updates as well
                // needed for free far all arenas for example
                if (m_areaUpdateId != newarea)
                    UpdateArea(newarea);

                m_zoneUpdateTimer = ZONE_UPDATE_INTERVAL;
            }
        }
        else
            m_zoneUpdateTimer -= p_time;
    }

    // @epoch-begin
    sScriptMgr->OnPlayerUpdate(this, p_time);
    // @epoch-end

    if (IsAlive())
    {
        m_regenTimer += p_time;
        HandleFoodEmotes(p_time);
        if (m_regenTimer >= REGEN_TIME_FULL_PLAYER)
            RegenerateAll(m_regenTimer / 100 * 100);
    }

    if (m_deathState == JUST_DIED)
        KillPlayer();

    if (m_nextSave > 0)
    {
        if (p_time >= m_nextSave)
        {
            // m_nextSave reset in SaveToDB call
            SaveToDB();
            TC_LOG_DEBUG("entities.player", "Player::Update: Player '{}' ({}) saved", GetName(), GetGUID().ToString());
        }
        else
            m_nextSave -= p_time;
    }

    //Handle Water/drowning
    HandleDrowning(p_time);

    // Played time
    if (now > m_Last_tick)
    {
        uint32 elapsed = uint32(now - m_Last_tick);
        m_Played_time[PLAYED_TIME_TOTAL] += elapsed;        // Total played time
        m_Played_time[PLAYED_TIME_LEVEL] += elapsed;        // Level played time
        m_Last_tick = now;
    }

    if (GetDrunkValue())
    {
        m_drunkTimer += p_time;
        if (m_drunkTimer > 9 * IN_MILLISECONDS)
            HandleSobering();
    }

    if (HasPendingBind())
    {
        if (_pendingBindTimer <= p_time)
        {
            // Player left the instance
            if (_pendingBindId == GetInstanceId())
                BindToInstance();
            SetPendingBind(0, 0);
        }
        else
            _pendingBindTimer -= p_time;
    }

    // not auto-free ghost from body in instances or if its affected by risen ally
    if (m_deathTimer > 0 && !GetMap()->Instanceable() && !HasAuraType(SPELL_AURA_PREVENT_RESURRECTION) && !IsGhouled())
    {
        if (p_time >= m_deathTimer)
        {
            m_deathTimer = 0;
            BuildPlayerRepop();
            RepopAtGraveyard();
        }
        else
            m_deathTimer -= p_time;
    }

    UpdateEnchantTime(p_time);
    UpdateHomebindTime(p_time);

    if (!_instanceResetTimes.empty())
    {
        for (InstanceTimeMap::iterator itr = _instanceResetTimes.begin(); itr != _instanceResetTimes.end();)
        {
            if (itr->second < now)
                _instanceResetTimes.erase(itr++);
            else
                ++itr;
        }
    }
    //@tswow-begin
    if (HasRunes())
    {
    //@tswow-end
        // Update rune timers
        for (uint8 i = 0; i < MAX_RUNES; ++i)
        {
            uint32 timer = GetRuneTimer(i);

            // Don't update timer if rune is disabled
            if (GetRuneCooldown(i))
                continue;

            // Timer has began
            if (timer < 0xFFFFFFFF)
            {
                timer += p_time;
                SetRuneTimer(i, std::min(uint32(2500), timer));
            }
        }
    }

    // group update
    m_groupUpdateTimer.Update(p_time);
    if (m_groupUpdateTimer.Passed())
    {
        SendUpdateToOutOfRangeGroupMembers();
        m_groupUpdateTimer.Reset(5000);
    }

    Pet* pet = GetPet();
    if (pet && !pet->IsWithinDistInMap(this, GetMap()->GetVisibilityRange()) && !pet->isPossessed())
    //if (pet && !pet->IsWithinDistInMap(this, GetMap()->GetVisibilityDistance()) && (GetCharmGUID() && (pet->GetGUID() != GetCharmGUID())))
        RemovePet(pet, PET_SAVE_NOT_IN_SLOT, true);

    if (IsAlive())
    {
        if (m_hostileReferenceCheckTimer <= p_time)
        {
            m_hostileReferenceCheckTimer = 15 * IN_MILLISECONDS;
            if (!GetMap()->IsDungeon())
                GetCombatManager().EndCombatBeyondRange(GetVisibilityRange(), true);
        }
        else
            m_hostileReferenceCheckTimer -= p_time;
    }

    if (IsHasDelayedTeleport())
        TeleportTo(m_teleport_dest, m_teleport_options);

    // For now, do this at the end of the update
    // Periodically send player updated visibility of all surrounding units
    vis_Update.TUpdate(p_time);
    if (vis_Update.TPassed())
    {
        vis_Update.TReset(p_time, GetMap()->GetVisibilityNotifyPeriod());

        WorldObject const* viewPoint = m_seer;
        if (viewPoint->isNeedNotify(NOTIFY_VISIBILITY_CHANGED) && (this == viewPoint || viewPoint->IsPositionValid()))
        {
            ZoneScopedN("Player::Update::RelocationNotifier")
            PlayerRelocationNotifier relocate(*this);
            Cell::VisitAllObjects(viewPoint, relocate, 100, false);
            relocate.SendToSelf();
        }

        ResetAllNotifies();
    }
}

void Player::setDeathState(DeathState s)
{
    uint32 ressSpellId = 0;

    bool cur = IsAlive();

    if (s == JUST_DIED)
    {
        if (!cur)
        {
            TC_LOG_ERROR("entities.player", "Player::setDeathState: Attempt to kill a dead player '{}' ({})", GetName(), GetGUID().ToString());
            return;
        }

        // drunken state is cleared on death
        SetDrunkValue(0);
        // lost combo points at any target (targeted combo points clear in Unit::setDeathState)
        ClearComboPoints();

        ClearResurrectRequestData();

        //FIXME: is pet dismissed at dying or releasing spirit? if second, add setDeathState(DEAD) to HandleRepopRequest and define pet unsummon here with (s == DEAD)
        RemovePet(nullptr, PET_SAVE_NOT_IN_SLOT, true);

        // save value before aura remove in Unit::setDeathState
        ressSpellId = GetUInt32Value(PLAYER_SELF_RES_SPELL);

        // passive spell
        if (!ressSpellId)
            ressSpellId = GetResurrectionSpellId();
        UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_DEATH_AT_MAP, 1);
        UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_DEATH, 1);
        UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_DEATH_IN_DUNGEON, 1);

        // reset all death criterias
        ResetAchievementCriteria(ACHIEVEMENT_CRITERIA_CONDITION_NO_DEATH, 0);
    }

    Unit::setDeathState(s);

    // restore resurrection spell id for player after aura remove
    if (s == JUST_DIED && cur && ressSpellId)
        SetUInt32Value(PLAYER_SELF_RES_SPELL, ressSpellId);

    if (IsAlive() && !cur)
        //clear aura case after resurrection by another way (spells will be applied before next death)
        SetUInt32Value(PLAYER_SELF_RES_SPELL, 0);
}

void Player::ToggleAFK()
{
    ToggleFlag(PLAYER_FLAGS, PLAYER_FLAGS_AFK);

    // afk player not allowed in battleground
    if (!GetSession()->HasPermission(rbac::RBAC_PERM_CAN_AFK_ON_BATTLEGROUND) && isAFK() && InBattleground() && !InArena())
        LeaveBattleground();
}

void Player::ToggleDND()
{
    ToggleFlag(PLAYER_FLAGS, PLAYER_FLAGS_DND);
}

void Player::ProcessDelayedOperations()
{
    if (m_DelayedOperations == 0)
        return;

    if (m_DelayedOperations & DELAYED_RESURRECT_PLAYER)
        ResurrectUsingRequestDataImpl();

    if (m_DelayedOperations & DELAYED_SAVE_PLAYER)
        SaveToDB();

    if (m_DelayedOperations & DELAYED_SPELL_CAST_DESERTER)
        CastSpell(this, 26013, true);               // Deserter

    if (m_DelayedOperations & DELAYED_BG_MOUNT_RESTORE)
    {
        if (m_bgData.mountSpell)
        {
            CastSpell(this, m_bgData.mountSpell, true);
            m_bgData.mountSpell = 0;
        }
    }

    if (m_DelayedOperations & DELAYED_BG_TAXI_RESTORE)
    {
        if (m_bgData.HasTaxiPath())
        {
            m_taxi.AddTaxiDestination(m_bgData.taxiPath[0]);
            m_taxi.AddTaxiDestination(m_bgData.taxiPath[1]);
            m_bgData.ClearTaxiPath();

            ContinueTaxiFlight();
        }
    }

    //we have executed ALL delayed ops, so clear the flag
    m_DelayedOperations = 0;
}

void Player::SetObjectScale(float scale)
{
    Unit::SetObjectScale(scale);
    SetBoundingRadius(scale * DEFAULT_PLAYER_BOUNDING_RADIUS);
    SetCombatReach(scale * DEFAULT_PLAYER_COMBAT_REACH);
}

bool Player::IsImmunedToSpellEffect(SpellInfo const* spellInfo, SpellEffectInfo const& spellEffectInfo, WorldObject const* caster, bool requireImmunityPurgesEffectAttribute /*= false*/) const
{
    // players are immune to taunt (the aura and the spell effect)
    if (spellEffectInfo.IsAura(SPELL_AURA_MOD_TAUNT))
        return true;
    if (spellEffectInfo.IsEffect(SPELL_EFFECT_ATTACK_ME))
        return true;

    return Unit::IsImmunedToSpellEffect(spellInfo, spellEffectInfo, caster, requireImmunityPurgesEffectAttribute);
}

void Player::HandleFoodEmotes(uint32 diff)
{
    m_foodEmoteTimerCount += diff;

    // Handles the emotes for drinking and eating.
    // According to sniffs there is a background timer going on that repeats independed from the time window where the aura applies.
    // That's why we dont need to reset the timer on apply. In sniffs I have seen that the first call for the spell visual is totally random, then after
    // 5 seconds over and over again which confirms my theory that we have a independed timer.
    if (m_foodEmoteTimerCount >= 5000)
    {
        std::vector<AuraEffect*> auraList;
        AuraEffectList const& ModRegenAuras = GetAuraEffectsByType(SPELL_AURA_MOD_REGEN);
        AuraEffectList const& ModPowerRegenAuras = GetAuraEffectsByType(SPELL_AURA_MOD_POWER_REGEN);

        auraList.reserve(ModRegenAuras.size() + ModPowerRegenAuras.size());
        auraList.insert(auraList.end(), ModRegenAuras.begin(), ModRegenAuras.end());
        auraList.insert(auraList.end(), ModPowerRegenAuras.begin(), ModPowerRegenAuras.end());

        for (auto itr = auraList.begin(); itr != auraList.end(); ++itr)
        {
            // Food emote comes above drinking emote if we have to decide (mage regen food for example)
            if ((*itr)->GetBase()->HasEffectType(SPELL_AURA_MOD_REGEN) && (*itr)->GetSpellInfo()->AuraInterruptFlags & AURA_INTERRUPT_FLAG_NOT_SEATED)
            {
                SendPlaySpellVisual(SPELL_VISUAL_KIT_FOOD);
                break;
            }
            else if ((*itr)->GetBase()->HasEffectType(SPELL_AURA_MOD_POWER_REGEN) && (*itr)->GetSpellInfo()->AuraInterruptFlags & AURA_INTERRUPT_FLAG_NOT_SEATED)
            {
                SendPlaySpellVisual(SPELL_VISUAL_KIT_DRINK);
                break;
            }
        }
        m_foodEmoteTimerCount -= 5000;
    }
}

void Player::RegenerateAll(uint32 diff)
{
    // Not in combat or they have regeneration
    if (!IsInCombat() || HasAuraType(SPELL_AURA_MOD_REGEN_DURING_COMBAT) ||
            HasAuraType(SPELL_AURA_MOD_HEALTH_REGEN_IN_COMBAT))
    {
        RegenerateHealth(diff);
        if (!IsInCombat() && !HasAuraType(SPELL_AURA_INTERRUPT_REGEN))
            Regenerate(POWER_RAGE, diff);
    }

    Regenerate(POWER_ENERGY, diff);

    Regenerate(POWER_MANA, diff);

    m_regenTimer -= diff;
}

void Player::Regenerate(Powers power, uint32 diff)
{
    uint32 maxValue = GetMaxPower(power);
    if (!maxValue)
        return;

    uint32 curValue = GetPower(power);

    /// @todo possible use of miscvalueb instead of amount
    if (HasAuraTypeWithValue(SPELL_AURA_PREVENT_REGENERATE_POWER, power))
        return;

    float addvalue = 0.0f;

    switch (power)
    {
        case POWER_MANA:
        {
            bool recentCast = IsUnderLastManaUseEffect();
            float ManaIncreaseRate = sWorld->getRate(RATE_POWER_MANA);

            if (recentCast) // Trinity Updates Mana in intervals of 2s, which is correct
                addvalue += GetFloatValue(UNIT_FIELD_POWER_REGEN_INTERRUPTED_FLAT_MODIFIER) * ManaIncreaseRate * uint32(float(diff) / 1000);
            else
                addvalue += GetFloatValue(UNIT_FIELD_POWER_REGEN_FLAT_MODIFIER) * ManaIncreaseRate * uint32(float(diff) / 1000);
        }   break;
        case POWER_RAGE:                                    // Regenerate rage
        {
            float RageDecreaseRate = sWorld->getRate(RATE_POWER_RAGE_LOSS);
            addvalue += uint32(float(diff) / 200) * (-2.5 * RageDecreaseRate); // decay 2.5 rage per 2 seconds
            addvalue *= GetTotalAuraMultiplierByMiscValue(SPELL_AURA_MOD_POWER_REGEN_PERCENT, power);
        }   break;
        case POWER_ENERGY:                                  // Regenerate energy (rogue)
        {
            float EnergyRate = sWorld->getRate(RATE_POWER_ENERGY);
            addvalue = 20 * EnergyRate * GetTotalAuraMultiplierByMiscValue(SPELL_AURA_MOD_POWER_REGEN_PERCENT, power);
        }   break;
        case POWER_RUNE:
        case POWER_RUNIC_POWER:
        case POWER_FOCUS:
        case POWER_HAPPINESS:
        case POWER_HEALTH:
            return;
        default:
            break;
    }

    if (addvalue < 0.0f)
    {
        if (curValue == 0)
            return;
    }
    else if (addvalue > 0.0f)
    {
        if (curValue == maxValue)
            return;
    }
    else
        return;

    addvalue += m_powerFraction[power];
    uint32 integerValue = uint32(std::fabs(addvalue));

    if (addvalue < 0.0f)
    {
        if (curValue > integerValue)
        {
            curValue -= integerValue;
            m_powerFraction[power] = addvalue + integerValue;
        }
        else
        {
            curValue = 0;
            m_powerFraction[power] = 0;
        }
    }
    else
    {
        curValue += integerValue;

        if (curValue > maxValue)
        {
            curValue = maxValue;
            m_powerFraction[power] = 0;
        }
        else
            m_powerFraction[power] = addvalue - integerValue;
    }

    SetPower(power, curValue);
}

void Player::RegenerateHealth(uint32 diff)
{
    uint32 curValue = GetHealth();
    uint32 maxValue = GetMaxHealth();

    if (curValue >= maxValue)
        return;

    float HealthIncreaseRate = sWorld->getRate(RATE_HEALTH);

    float addValue = 0.0f;

    // normal regen case (maybe partly in combat case)
    if (!IsInCombat() || HasAuraType(SPELL_AURA_MOD_REGEN_DURING_COMBAT))
    {
        addValue = OCTRegenHPPerSpirit() * HealthIncreaseRate;

        if (! IsInCombat())
        {
            addValue *= GetTotalAuraMultiplier(SPELL_AURA_MOD_HEALTH_REGEN_PERCENT);

            // Food
            float food = 0.0f;
            AuraEffectList const& regenAuras = GetAuraEffectsByType(SPELL_AURA_MOD_REGEN);
            for (AuraEffect const* aurEff : regenAuras)
                food += (float(aurEff->GetAmount()) * 0.4f) * 0.5f;

            addValue += food;
        }
        else if (HasAuraType(SPELL_AURA_MOD_REGEN_DURING_COMBAT))
            addValue *= GetTotalAuraModifier(SPELL_AURA_MOD_REGEN_DURING_COMBAT) / 100.0f;

        if (! IsStandState())
            addValue *= 1.5f;
    }

    // always regeneration bonus (including combat)
    addValue += (GetTotalAuraModifier(SPELL_AURA_MOD_HEALTH_REGEN_IN_COMBAT) / 5.0f);

    addValue *= float(diff) / 1000;

    // Health fractions get carried to the next tick
    addValue += m_carryHealthRegen;
    m_carryHealthRegen = addValue - int32(addValue);

    if (addValue < 0.0f)
        addValue = 0.0f;

    ModifyHealth(int32(addValue));
}

void Player::ResetAllPowers()
{
    SetFullHealth();
    switch (GetPowerType())
    {
        case POWER_MANA:
            SetFullPower(POWER_MANA);
            break;
        case POWER_RAGE:
            SetPower(POWER_RAGE, 0);
            break;
        case POWER_ENERGY:
            SetFullPower(POWER_ENERGY);
            break;
        case POWER_RUNIC_POWER:
            SetPower(POWER_RUNIC_POWER, 0);
            break;
        default:
            break;
    }
}

bool Player::CanInteractWithQuestGiver(Object* questGiver) const
{
    switch (questGiver->GetTypeId())
    {
        case TYPEID_UNIT:
            return GetNPCIfCanInteractWith(questGiver->GetGUID(), UNIT_NPC_FLAG_QUESTGIVER) != nullptr;
        case TYPEID_GAMEOBJECT:
            return GetGameObjectIfCanInteractWith(questGiver->GetGUID(), GAMEOBJECT_TYPE_QUESTGIVER) != nullptr;
        case TYPEID_PLAYER:
            return IsAlive() && questGiver->ToPlayer()->IsAlive();
        case TYPEID_ITEM:
            return IsAlive();
        default:
            break;
    }
    return false;
}

Creature* Player::GetNPCIfCanInteractWith(ObjectGuid const& guid, NPCFlags npcFlags) const
{
    // unit checks
    if (!guid)
        return nullptr;

    if (!IsInWorld())
        return nullptr;

    if (IsInFlight())
        return nullptr;

    // exist (we need look pets also for some interaction (quest/etc)
    Creature* creature = ObjectAccessor::GetCreatureOrPetOrVehicle(*this, guid);
    if (!creature)
        return nullptr;

    // Deathstate checks
    if (!IsAlive() && !(creature->GetCreatureTemplate()->type_flags & CREATURE_TYPE_FLAG_VISIBLE_TO_GHOSTS))
        return nullptr;

    // alive or spirit healer
    if (!creature->IsAlive() && !(creature->GetCreatureTemplate()->type_flags & CREATURE_TYPE_FLAG_INTERACT_WHILE_DEAD))
        return nullptr;

    // appropriate npc type
    if (npcFlags && !creature->HasNpcFlag(npcFlags))
        return nullptr;

    // not allow interaction under control, but allow with own pets
    if (creature->GetCharmerGUID())
        return nullptr;

    // not unfriendly/hostile
    if (creature->GetReactionTo(this) <= REP_UNFRIENDLY)
        return nullptr;

    // not too far, taken from CGGameUI::SetInteractTarget
    if (!creature->IsWithinDistInMap(this, creature->GetCombatReach() + 4.0f))
        return nullptr;

    return creature;
}

GameObject* Player::GetGameObjectIfCanInteractWith(ObjectGuid const& guid) const
{
    if (!guid)
        return nullptr;

    if (!IsInWorld())
        return nullptr;

    if (IsInFlight())
        return nullptr;

    // exist
    GameObject* go = ObjectAccessor::GetGameObject(*this, guid);
    if (!go)
        return nullptr;

    // Players cannot interact with gameobjects that use the "Point" icon
    if (go->GetGOInfo()->IconName == "Point")
        return nullptr;

    if (!go->IsWithinDistInMap(this))
        return nullptr;

    return go;
}

GameObject* Player::GetGameObjectIfCanInteractWith(ObjectGuid const& guid, GameobjectTypes type) const
{
    GameObject* go = GetGameObjectIfCanInteractWith(guid);
    if (!go)
        return nullptr;

    if (go->GetGoType() != type)
        return nullptr;

    return go;
}

bool Player::IsInAreaTriggerRadius(AreaTriggerEntry const* trigger) const
{
    if (!trigger || GetMapId() != trigger->ContinentID)
        return false;

    if (trigger->Radius > 0.f)
    {
        // if we have radius check it
        float dist = GetDistance(trigger->Pos.X, trigger->Pos.Y, trigger->Pos.Z);
        if (dist > trigger->Radius)
            return false;
    }
    else
    {
        Position center(trigger->Pos.X, trigger->Pos.Y, trigger->Pos.Z, trigger->BoxYaw);
        if (!IsWithinBox(center, trigger->BoxLength / 2.f, trigger->BoxWidth / 2.f, trigger->BoxHeight / 2.f))
            return false;
    }

    return true;
}

void Player::SetGameMaster(bool on)
{
    if (on)
    {
        m_ExtraFlags |= PLAYER_EXTRA_GM_ON;
        SetFaction(FACTION_FRIENDLY);
        SetFlag(PLAYER_FLAGS, PLAYER_FLAGS_GM);
        SetUnitFlag2(UNIT_FLAG2_ALLOW_CHEAT_SPELLS);

        if (Pet* pet = GetPet())
            pet->SetFaction(FACTION_FRIENDLY);

        RemovePvpFlag(UNIT_BYTE2_FLAG_FFA_PVP);
        ResetContestedPvP();

        CombatStopWithPets();

        SetPhaseMask(uint32(PHASEMASK_ANYWHERE), false);    // see and visible in all phases
        m_serverSideVisibilityDetect.SetValue(SERVERSIDE_VISIBILITY_GM, GetSession()->GetSecurity());
    }
    else
    {
        // restore phase
        uint32 newPhase = 0;
        AuraEffectList const& phases = GetAuraEffectsByType(SPELL_AURA_PHASE);
        if (!phases.empty())
            for (AuraEffectList::const_iterator itr = phases.begin(); itr != phases.end(); ++itr)
                newPhase |= (*itr)->GetMiscValue();

        if (!newPhase)
            newPhase = PHASEMASK_NORMAL;

        SetPhaseMask(newPhase, false);

        m_ExtraFlags &= ~ PLAYER_EXTRA_GM_ON;
        SetFactionForRace(GetRace());
        RemoveFlag(PLAYER_FLAGS, PLAYER_FLAGS_GM);
        RemoveUnitFlag2(UNIT_FLAG2_ALLOW_CHEAT_SPELLS);

        if (Pet* pet = GetPet())
            pet->SetFaction(GetFaction());

        // restore FFA PvP Server state
        if (sWorld->IsFFAPvPRealm())
            SetPvpFlag(UNIT_BYTE2_FLAG_FFA_PVP);

        // restore FFA PvP area state, remove not allowed for GM mounts
        UpdateArea(m_areaUpdateId);

        m_serverSideVisibilityDetect.SetValue(SERVERSIDE_VISIBILITY_GM, SEC_PLAYER);
    }

    UpdateObjectVisibility();
}

bool Player::CanBeGameMaster() const
{
    return GetSession()->HasPermission(rbac::RBAC_PERM_COMMAND_GM);
}

void Player::SetGMVisible(bool on)
{
    if (on)
    {
        m_ExtraFlags &= ~PLAYER_EXTRA_GM_INVISIBLE;         //remove flag
        m_serverSideVisibility.SetValue(SERVERSIDE_VISIBILITY_GM, SEC_PLAYER);
    }
    else
    {
        m_ExtraFlags |= PLAYER_EXTRA_GM_INVISIBLE;          //add flag

        SetAcceptWhispers(false);
        SetGameMaster(true);

        m_serverSideVisibility.SetValue(SERVERSIDE_VISIBILITY_GM, GetSession()->GetSecurity());
    }

    for (Channel* channel : m_channels)
        channel->SetInvisible(this, !on);
}

bool Player::IsGroupVisibleFor(Player const* p) const
{
    switch (sWorld->getIntConfig(CONFIG_GROUP_VISIBILITY))
    {
        default: return IsInSameGroupWith(p);
        case 1:  return IsInSameRaidWith(p);
        case 2:  return GetTeam() == p->GetTeam();
        case 3:  return false;
    }
}

bool Player::IsInSameGroupWith(Player const* p) const
{
    return p == this || (GetGroup() != nullptr &&
        GetGroup() == p->GetGroup() &&
        GetGroup()->SameSubGroup(this, p));
}

bool Player::IsInSameRaidWith(Player const* p) const
{
    return p == this || (GetGroup() != nullptr && GetGroup() == p->GetGroup());
}

///- If the player is invited, remove him. If the group if then only 1 person, disband the group.
void Player::UninviteFromGroup()
{
    Group* group = GetGroupInvite();
    if (!group)
        return;

    group->RemoveInvite(this);

    if (group->IsCreated())
    {
        if (group->GetMembersCount() <= 1) // group has just 1 member => disband
            group->Disband(true);
    }
    else
    {
        if (group->GetInviteeCount() <= 1)
        {
            group->RemoveAllInvites();
            delete group;
        }
    }
}

void Player::RemoveFromGroup(Group* group, ObjectGuid guid, RemoveMethod method /*= GROUP_REMOVEMETHOD_DEFAULT*/, ObjectGuid kicker /*= ObjectGuid::Empty*/, char const* reason /*= nullptr*/)
{
    if (!group)
        return;

    group->RemoveMember(guid, method, kicker, reason);
}

void Player::SendLogXPGain(uint32 GivenXP, Unit* victim, uint32 BonusXP, bool recruitAFriend, float /*group_rate*/) const
{
    WorldPacket data(SMSG_LOG_XPGAIN, 21); // guess size?
    data << uint64(victim ? victim->GetGUID() : ObjectGuid::Empty);
    data << uint32(GivenXP + BonusXP);                      // given experience
    data << uint8(victim ? 0 : 1);                          // 00-kill_xp type, 01-non_kill_xp type

    if (victim)
    {
        data << uint32(GivenXP);                            // experience without bonus

        // should use group_rate here but can't figure out how
        data << float(1);                                   // 1 - none 0 - 100% group bonus output
    }

    data << uint8(recruitAFriend ? 1 : 0);                  // does the GivenXP include a RaF bonus?
    SendDirectMessage(&data);
}

void Player::GiveXP(uint32 xp, Unit* victim, float group_rate)
{
    if (xp < 1)
        return;

    if (!IsAlive() && !GetBattlegroundId())
        return;

    if (HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_NO_XP_GAIN))
        return;

    if (victim && victim->GetTypeId() == TYPEID_UNIT && !victim->ToCreature()->hasLootRecipient())
        return;

    uint8 level = GetLevel();

    sScriptMgr->OnGivePlayerXP(this, xp, victim);

    // XP to money conversion processed in Player::RewardQuest
    if (IsMaxLevel())
        return;

    uint32 bonus_xp;
    bool recruitAFriend = GetsRecruitAFriendBonus(true);

    // RaF does NOT stack with rested experience
    if (recruitAFriend)
        bonus_xp = 2 * xp; // xp + bonus_xp must add up to 3 * xp for RaF; calculation for quests done client-side
    else
        bonus_xp = victim ? GetXPRestBonus(xp) : 0; // XP resting bonus

    SendLogXPGain(xp, victim, bonus_xp, recruitAFriend, group_rate);

    uint32 nextLvlXP = GetUInt32Value(PLAYER_NEXT_LEVEL_XP);
    uint32 newXP = GetXP() + xp + bonus_xp;

    while (newXP >= nextLvlXP && !IsMaxLevel())
    {
        newXP -= nextLvlXP;

        if (!IsMaxLevel())
            GiveLevel(level + 1);

        level = GetLevel();
        nextLvlXP = GetUInt32Value(PLAYER_NEXT_LEVEL_XP);
    }

    SetXP(newXP);
}

// Update player to next level
// Current player experience not update (must be update by caller)
void Player::GiveLevel(uint8 level)
{
    uint8 oldLevel = GetLevel();
    if (level == oldLevel)
        return;

    if (Guild* guild = GetGuild())
        guild->UpdateMemberData(this, GUILD_MEMBER_DATA_LEVEL, level);

    PlayerLevelInfo info;
    sObjectMgr->GetPlayerLevelInfo(GetRace(), GetClass(), level, &info);

    PlayerClassLevelInfo classInfo;
    sObjectMgr->GetPlayerClassLevelInfo(GetClass(), level, &classInfo);

    WorldPackets::Misc::LevelUpInfo packet;
    packet.Level = level;
    packet.HealthDelta = int32(classInfo.basehealth) - int32(GetCreateHealth());

    /// @todo find some better solution
    // for (int i = 0; i < MAX_POWERS; ++i)
    packet.PowerDelta[0] = int32(classInfo.basemana) - int32(GetCreateMana());
    packet.PowerDelta[1] = 0;
    packet.PowerDelta[2] = 0;
    packet.PowerDelta[3] = 0;
    packet.PowerDelta[4] = 0;
    packet.PowerDelta[5] = 0;

    for (uint8 i = STAT_STRENGTH; i < MAX_STATS; ++i)
        packet.StatDelta[i] = int32(info.stats[i]) - GetCreateStat(Stats(i));

    SendDirectMessage(packet.Write());

    SetUInt32Value(PLAYER_NEXT_LEVEL_XP, sObjectMgr->GetXPForLevel(level));

    //update level, max level of skills
    m_Played_time[PLAYED_TIME_LEVEL] = 0;                   // Level Played Time reset

    _ApplyAllLevelScaleItemMods(false);

    SetLevel(level);

    UpdateSkillsForLevel();

    // save base values (bonuses already included in stored stats
    for (uint8 i = STAT_STRENGTH; i < MAX_STATS; ++i)
        SetCreateStat(Stats(i), info.stats[i]);

    SetCreateHealth(classInfo.basehealth);
    SetCreateMana(classInfo.basemana);

    InitTalentForLevel();
    InitTaxiNodesForLevel();
    InitGlyphsForLevel();

    UpdateAllStats();

    if (sWorld->getBoolConfig(CONFIG_ALWAYS_MAXSKILL)) // Max weapon skill when leveling up
        UpdateWeaponsSkillsToMaxSkillsForLevel();

    _ApplyAllLevelScaleItemMods(true);

    // set current level health and mana/energy to maximum after applying all mods.
    SetFullHealth();
    SetFullPower(POWER_MANA);

    // update level to hunter/summon pet
    if (Pet* pet = GetPet())
        pet->SynchronizeLevelWithOwner();

    if (MailLevelReward const* mailReward = sObjectMgr->GetMailLevelReward(level, GetRaceMask()))
    {
        /// @todo Poor design of mail system
        CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
        MailDraft(mailReward->mailTemplateId).SendMailTo(trans, this, MailSender(MAIL_CREATURE, mailReward->senderEntry));
        CharacterDatabase.CommitTransaction(trans);
    }

    UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_REACH_LEVEL);

    // Refer-A-Friend
    if (GetSession()->GetRecruiterId())
        if (level < sWorld->getIntConfig(CONFIG_MAX_RECRUIT_A_FRIEND_BONUS_PLAYER_LEVEL))
            if (level % 2 == 0)
            {
                ++m_grantableLevels;

                if (!HasByteFlag(PLAYER_FIELD_BYTES, PLAYER_FIELD_BYTES_OFFSET_RAF_GRANTABLE_LEVEL, 0x01))
                    SetByteFlag(PLAYER_FIELD_BYTES, PLAYER_FIELD_BYTES_OFFSET_RAF_GRANTABLE_LEVEL, 0x01);
            }

    SendQuestGiverStatusMultiple();

    // @tswow-begin
    ApplyAutolearnSpells(oldLevel+1);
    // @tswow-end

    sScriptMgr->OnPlayerLevelChanged(this, oldLevel);
}

bool Player::IsMaxLevel() const
{
    return GetLevel() >= GetUInt32Value(PLAYER_FIELD_MAX_LEVEL);
}

void Player::InitStatsForLevel(bool reapplyMods)
{
    if (reapplyMods)                                        //reapply stats values only on .reset stats (level) command
        _RemoveAllStatBonuses();

    PlayerClassLevelInfo classInfo;
    sObjectMgr->GetPlayerClassLevelInfo(GetClass(), GetLevel(), &classInfo);

    PlayerLevelInfo info;
    sObjectMgr->GetPlayerLevelInfo(GetRace(), GetClass(), GetLevel(), &info);

    uint8 exp_max_lvl = GetMaxLevelForExpansion(GetSession()->Expansion());
    uint8 conf_max_lvl = sWorld->getIntConfig(CONFIG_MAX_PLAYER_LEVEL);
    if (exp_max_lvl == DEFAULT_MAX_LEVEL || exp_max_lvl >= conf_max_lvl)
        SetUInt32Value(PLAYER_FIELD_MAX_LEVEL, conf_max_lvl);
    else
        SetUInt32Value(PLAYER_FIELD_MAX_LEVEL, exp_max_lvl);
    SetUInt32Value(PLAYER_NEXT_LEVEL_XP, sObjectMgr->GetXPForLevel(GetLevel()));

    // reset before any aura state sources (health set/aura apply)
    SetUInt32Value(UNIT_FIELD_AURASTATE, 0);

    UpdateSkillsForLevel();

    // set default cast time multiplier
    SetModCastingSpeed(1.0f);

    // reset size before reapply auras
    SetObjectScale(1.0f);

    // save base values (bonuses already included in stored stats
    for (uint8 i = STAT_STRENGTH; i < MAX_STATS; ++i)
        SetCreateStat(Stats(i), info.stats[i]);

    for (uint8 i = STAT_STRENGTH; i < MAX_STATS; ++i)
        SetStat(Stats(i), info.stats[i]);

    SetCreateHealth(classInfo.basehealth);

    //set create powers
    SetCreateMana(classInfo.basemana);

    SetArmor(int32(m_createStats[STAT_AGILITY]*2));

    InitStatBuffMods();

    //reset rating fields values
    for (uint16 index = PLAYER_FIELD_COMBAT_RATING_1; index < PLAYER_FIELD_COMBAT_RATING_1 + MAX_COMBAT_RATING; ++index)
        SetUInt32Value(index, 0);

    SetUInt32Value(PLAYER_FIELD_MOD_HEALING_DONE_POS, 0);
    for (uint8 i = SPELL_SCHOOL_NORMAL; i < MAX_SPELL_SCHOOL; ++i)
    {
        SetInt32Value(PLAYER_FIELD_MOD_DAMAGE_DONE_NEG + i, 0);
        SetInt32Value(PLAYER_FIELD_MOD_DAMAGE_DONE_POS + i, 0);
        SetFloatValue(PLAYER_FIELD_MOD_DAMAGE_DONE_PCT + i, 1.00f);
    }

    //reset attack power, damage and attack speed fields
    for (uint8 i = BASE_ATTACK; i < MAX_ATTACK; ++i)
        SetFloatValue(UNIT_FIELD_BASEATTACKTIME + i, float(BASE_ATTACK_TIME));

    SetFloatValue(UNIT_FIELD_MINDAMAGE, 0.0f);
    SetFloatValue(UNIT_FIELD_MAXDAMAGE, 0.0f);
    SetFloatValue(UNIT_FIELD_MINOFFHANDDAMAGE, 0.0f);
    SetFloatValue(UNIT_FIELD_MAXOFFHANDDAMAGE, 0.0f);
    SetFloatValue(UNIT_FIELD_MINRANGEDDAMAGE, 0.0f);
    SetFloatValue(UNIT_FIELD_MAXRANGEDDAMAGE, 0.0f);

    SetAttackPower(0);
    SetAttackPowerModPos(0);
    SetAttackPowerModNeg(0);
    SetAttackPowerMultiplier(0.0f);
    SetRangedAttackPower(0);
    SetRangedAttackPowerModPos(0);
    SetRangedAttackPowerModNeg(0);
    SetRangedAttackPowerMultiplier(0.0f);

    // Base crit values (will be recalculated in UpdateAllStats() at loading and in _ApplyAllStatBonuses() at reset
    SetFloatValue(PLAYER_CRIT_PERCENTAGE, 0.0f);
    SetFloatValue(PLAYER_OFFHAND_CRIT_PERCENTAGE, 0.0f);
    SetFloatValue(PLAYER_RANGED_CRIT_PERCENTAGE, 0.0f);

    // Init spell schools (will be recalculated in UpdateAllStats() at loading and in _ApplyAllStatBonuses() at reset
    for (uint8 i = 0; i < 7; ++i)
        SetFloatValue(PLAYER_SPELL_CRIT_PERCENTAGE1+i, 0.0f);

    SetFloatValue(PLAYER_PARRY_PERCENTAGE, 0.0f);
    SetFloatValue(PLAYER_BLOCK_PERCENTAGE, 0.0f);
    SetUInt32Value(PLAYER_SHIELD_BLOCK, 0);

    // Dodge percentage
    SetFloatValue(PLAYER_DODGE_PERCENTAGE, 0.0f);

    // set armor (resistance 0) to original value (create_agility*2)
    SetArmor(int32(m_createStats[STAT_AGILITY]*2));
    SetFloatValue(UNIT_FIELD_RESISTANCEBUFFMODSPOSITIVE + AsUnderlyingType(SPELL_SCHOOL_NORMAL), 0.0f);
    SetFloatValue(UNIT_FIELD_RESISTANCEBUFFMODSNEGATIVE + AsUnderlyingType(SPELL_SCHOOL_NORMAL), 0.0f);
    // set other resistance to original value (0)
    for (uint8 i = SPELL_SCHOOL_HOLY; i < MAX_SPELL_SCHOOL; ++i)
    {
        SetResistance(SpellSchools(i), 0);
        SetFloatValue(UNIT_FIELD_RESISTANCEBUFFMODSPOSITIVE + i, 0.0f);
        SetFloatValue(UNIT_FIELD_RESISTANCEBUFFMODSNEGATIVE + i, 0.0f);
    }

    SetUInt32Value(PLAYER_FIELD_MOD_TARGET_RESISTANCE, 0);
    SetUInt32Value(PLAYER_FIELD_MOD_TARGET_PHYSICAL_RESISTANCE, 0);
    for (uint8 i = SPELL_SCHOOL_NORMAL; i < MAX_SPELL_SCHOOL; ++i)
    {
        SetUInt32Value(UNIT_FIELD_POWER_COST_MODIFIER + i, 0);
        SetFloatValue(UNIT_FIELD_POWER_COST_MULTIPLIER + i, 0.0f);
    }
    // Reset no reagent cost field
    for (uint8 i = 0; i < 3; ++i)
        SetUInt32Value(PLAYER_NO_REAGENT_COST_1 + i, 0);
    // Init data for form but skip reapply item mods for form
    InitDataForForm(reapplyMods);

    // save new stats
    for (uint8 i = POWER_MANA; i < MAX_POWERS; ++i)
        SetMaxPower(Powers(i), uint32(GetCreatePowerValue(Powers(i))));

    SetMaxHealth(classInfo.basehealth);                     // stamina bonus will applied later

    // cleanup mounted state (it will set correctly at aura loading if player saved at mount.
    SetMountDisplayId(0);

    // cleanup unit flags (will be re-applied if need at aura load).
    RemoveUnitFlag(
        UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_REMOVE_CLIENT_CONTROL | UNIT_FLAG_NOT_ATTACKABLE_1 |
        UNIT_FLAG_IMMUNE_TO_PC | UNIT_FLAG_IMMUNE_TO_NPC  | UNIT_FLAG_LOOTING          |
        UNIT_FLAG_PET_IN_COMBAT  | UNIT_FLAG_SILENCED     | UNIT_FLAG_PACIFIED         |
        UNIT_FLAG_STUNNED        | UNIT_FLAG_IN_COMBAT    | UNIT_FLAG_DISARMED         |
        UNIT_FLAG_CONFUSED       | UNIT_FLAG_FLEEING      | UNIT_FLAG_UNINTERACTIBLE   |
        UNIT_FLAG_SKINNABLE      | UNIT_FLAG_MOUNT        | UNIT_FLAG_ON_TAXI          );
    SetUnitFlag(UNIT_FLAG_PLAYER_CONTROLLED);   // must be set

    // cleanup player flags (will be re-applied if need at aura load), to avoid have ghost flag without ghost aura, for example.
    RemoveFlag(PLAYER_FLAGS, PLAYER_FLAGS_AFK | PLAYER_FLAGS_DND | PLAYER_FLAGS_GM | PLAYER_FLAGS_GHOST | PLAYER_ALLOW_ONLY_ABILITY);

    RemoveVisFlag(UNIT_VIS_FLAGS_ALL);                 // one form stealth modified bytes
    RemovePvpFlag(UNIT_BYTE2_FLAG_FFA_PVP | UNIT_BYTE2_FLAG_SANCTUARY);

    // restore if need some important flags
    SetUInt32Value(PLAYER_FIELD_BYTES2, 0);                 // flags empty by default

    if (reapplyMods)                                        // reapply stats values only on .reset stats (level) command
        _ApplyAllStatBonuses();

    // set current level health and mana/energy to maximum after applying all mods.
    SetFullHealth();
    SetFullPower(POWER_MANA);
    SetFullPower(POWER_ENERGY);
    if (GetPower(POWER_RAGE) > GetMaxPower(POWER_RAGE))
        SetFullPower(POWER_RAGE);
    SetFullPower(POWER_FOCUS);
    SetPower(POWER_HAPPINESS, 0);
    SetPower(POWER_RUNIC_POWER, 0);

    // update level to hunter/summon pet
    if (Pet* pet = GetPet())
        pet->SynchronizeLevelWithOwner();
}

void Player::DestroyForPlayer(Player* target, bool onDeath) const
{
    Unit::DestroyForPlayer(target, onDeath);

    for (uint8 i = 0; i < EQUIPMENT_SLOT_END; ++i)
    {
        if (m_items[i] == nullptr)
            continue;

        m_items[i]->DestroyForPlayer(target);
    }

    if (target == this)
    {
        for (uint8 i = INVENTORY_SLOT_BAG_START; i < BANK_SLOT_BAG_END; ++i)
        {
            if (m_items[i] == nullptr)
                continue;

            m_items[i]->DestroyForPlayer(target);
        }
        for (uint8 i = KEYRING_SLOT_START; i < CURRENCYTOKEN_SLOT_END; ++i)
        {
            if (m_items[i] == nullptr)
                continue;

            m_items[i]->DestroyForPlayer(target);
        }
    }
}

/* Preconditions:
  - a resurrectable corpse must not be loaded for the player (only bones)
  - the player must be in world
*/
void Player::BuildPlayerRepop()
{
    WorldPackets::Misc::PreRessurect packet;
    packet.PlayerGUID = GetGUID();
    GetSession()->SendPacket(packet.Write());

    if (GetRace() == RACE_NIGHTELF)
        CastSpell(this, 20584, true);
    CastSpell(this, 8326, true);

    // there must be SMSG.FORCE_RUN_SPEED_CHANGE, SMSG.FORCE_SWIM_SPEED_CHANGE, SMSG.MOVE_WATER_WALK
    // there must be SMSG.STOP_MIRROR_TIMER

    // the player cannot have a corpse already on current map, only bones which are not returned by GetCorpse
    if (GetCorpseLocation().GetMapId() == GetMapId())
    {
        TC_LOG_ERROR("entities.player", "Player::BuildPlayerRepop: Player '{}' ({}) already has a corpse", GetName(), GetGUID().ToString());
        ResurrectPlayer(100, false);
        return;
    }

    // create a corpse and place it at the player's location
    Corpse* corpse = CreateCorpse();
    if (!corpse)
    {
        TC_LOG_ERROR("entities.player", "Player::BuildPlayerRepop: Error creating corpse for player '{}' ({})", GetName(), GetGUID().ToString());
        return;
    }
    GetMap()->AddToMap(corpse);

    // convert player body to ghost
    setDeathState(DEAD);
    SetHealth(1);

    SetMovement(MOVE_WATER_WALK);
    if (!GetSession()->isLogingOut())
        SetMovement(MOVE_UNROOT);

    // BG - remove insignia related
    RemoveUnitFlag(UNIT_FLAG_SKINNABLE);

    int32 corpseReclaimDelay = CalculateCorpseReclaimDelay();

    if (corpseReclaimDelay >= 0)
        SendCorpseReclaimDelay(corpseReclaimDelay);

    // to prevent cheating
    corpse->ResetGhostTime();
    _corpseTime = corpse->GetGhostTime();

    StopMirrorTimers();                                     //disable timers(bars)

    // OnPlayerRepop hook
    sScriptMgr->OnPlayerRepop(this);
}

void Player::ReclaimCorpse()
{
    if (IsAlive())
        return;

    // do not allow corpse reclaim in arena
    if (InArena())
        return;

    // body not released yet
    if (!HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_GHOST))
        return;

    Corpse* corpse = GetCorpse();
    // Original Logic
    if (corpse)
    {
        // prevent resurrect before 30-sec delay after body release not finished
        if (time_t(corpse->GetGhostTime() + GetCorpseReclaimDelay(corpse->GetType() == CORPSE_RESURRECTABLE_PVP)) > GameTime::GetGameTime())
            return;

        if (!corpse->IsWithinDistInMap(this, CORPSE_RECLAIM_RADIUS, true))
            return;

        // resurrect
        ResurrectPlayer(InBattleground() ? 1.0f : 0.5f);

        // spawn bones
        SpawnCorpseBones();
    }
    // Cross partition logic
    else if (_corpseLocation.GetMapId() == GetMapId())
    {
        // prevent resurrect before 30-sec delay after body release not finished
        if (time_t(_corpseTime + GetCorpseReclaimDelay(false)) > GameTime::GetGameTime())
            return;

        if (!IsInDist(_corpseLocation, CORPSE_RECLAIM_RADIUS))
            return;

        // resurrect
        ResurrectPlayer(InBattleground() ? 1.0f : 0.5f);
        
        // we cannot actually spawn bones immediately since we are in the wrong partition,
        // but we need to call it anyway to reset our _corpseLocation
        // corpse will be converted to bones on map.AddPlayerToPartition if we ever return to the same partition
        SpawnCorpseBones();
    }
}

void Player::ResurrectPlayer(float restore_percent, bool applySickness)
{
    WorldPackets::Misc::DeathReleaseLoc packet;
    packet.MapID = -1;
    GetSession()->SendPacket(packet.Write());

    // speed change, land walk

    // remove death flag + set aura
    RemoveFlag(PLAYER_FLAGS, PLAYER_FLAGS_IS_OUT_OF_BOUNDS);

    // This must be called always even on Players with race != RACE_NIGHTELF in case of faction change
    RemoveAurasDueToSpell(20584);                           // RACE_NIGHTELF speed bonuses
    RemoveAurasDueToSpell(8326);                            // SPELL_AURA_GHOST

    if (GetSession()->IsARecruiter() || (GetSession()->GetRecruiterId() != 0))
        SetDynamicFlag(UNIT_DYNFLAG_REFER_A_FRIEND);

    setDeathState(ALIVE);

    SetMovement(MOVE_LAND_WALK);
    SetMovement(MOVE_UNROOT);

    m_deathTimer = 0;

    // set health/powers (0- will be set in caller)
    if (restore_percent > 0.0f)
    {
        SetHealth(uint32(GetMaxHealth()*restore_percent));
        SetPower(POWER_MANA, uint32(GetMaxPower(POWER_MANA)*restore_percent));
        SetPower(POWER_RAGE, 0);
        SetPower(POWER_ENERGY, uint32(GetMaxPower(POWER_ENERGY)*restore_percent));
    }

    // trigger update zone for alive state zone updates
    uint32 newzone, newarea;
    GetZoneAndAreaId(newzone, newarea);
    UpdateZone(newzone, newarea);
    sOutdoorPvPMgr->HandlePlayerResurrects(this, newzone);

    if (InBattleground())
    {
        if (Battleground* bg = GetBattleground())
            bg->HandlePlayerResurrect(this);
    }

    // update visibility
    UpdateObjectVisibility();

    // recast lost by death auras of any items held in the inventory
    CastAllObtainSpells();

    if (!applySickness)
        return;

    //Characters from level 1-10 are not affected by resurrection sickness.
    //Characters from level 11-19 will suffer from one minute of sickness
    //for each level they are above 10.
    //Characters level 20 and up suffer from ten minutes of sickness.
    int32 startLevel = sWorld->getIntConfig(CONFIG_DEATH_SICKNESS_LEVEL);
    ChrRacesEntry const* raceEntry = sChrRacesStore.AssertEntry(GetRace());

    if (int32(GetLevel()) >= startLevel)
    {
        // set resurrection sickness
        CastSpell(this, raceEntry->ResSicknessSpellID, true);

        // not full duration
        if (int32(GetLevel()) < startLevel+9)
        {
            int32 delta = (int32(GetLevel()) - startLevel + 1)*MINUTE;

            if (Aura* aur = GetAura(raceEntry->ResSicknessSpellID, GetGUID()))
            {
                aur->SetDuration(delta*IN_MILLISECONDS);
            }
        }
    }
}

void Player::RemoveGhoul()
{
    RemoveAura(SPELL_DK_RAISE_ALLY);
}

void Player::KillPlayer()
{
    if (IsFlying() && !GetTransport())
        GetMotionMaster()->MoveFall();

    SetMovement(MOVE_ROOT);

    StopMirrorTimers();                                     //disable timers(bars)

    setDeathState(CORPSE);
    //SetUnitFlag(UNIT_FLAG_NOT_IN_PVP);

    ReplaceAllDynamicFlags(UNIT_DYNFLAG_NONE);
    ApplyModFlag(PLAYER_FIELD_BYTES, PLAYER_FIELD_BYTE_RELEASE_TIMER, !sMapStore.LookupEntry(GetMapId())->Instanceable() && !HasAuraType(SPELL_AURA_PREVENT_RESURRECTION));

    // 6 minutes until repop at graveyard
    m_deathTimer = 6 * MINUTE * IN_MILLISECONDS;

    UpdateCorpseReclaimDelay();                             // dependent at use SetDeathPvP() call before kill

    int32 corpseReclaimDelay = CalculateCorpseReclaimDelay();

    if (corpseReclaimDelay >= 0)
        SendCorpseReclaimDelay(corpseReclaimDelay);

    // don't create corpse at this moment, player might be falling

    // update visibility
    UpdateObjectVisibility();
}

void Player::OfflineResurrect(ObjectGuid const& guid, CharacterDatabaseTransaction trans)
{
    Corpse::DeleteFromDB(guid, trans);
    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_ADD_AT_LOGIN_FLAG);
    stmt->setUInt16(0, uint16(AT_LOGIN_RESURRECT));
    stmt->setUInt64(1, guid.GetCounter());
    CharacterDatabase.ExecuteOrAppend(trans, stmt);
}

Corpse* Player::CreateCorpse()
{
    // prevent the existence of 2 corpses for one player
    SpawnCorpseBones();

    uint32 _cfb1, _cfb2;

    Corpse* corpse = new Corpse((m_ExtraFlags & PLAYER_EXTRA_PVP_DEATH) ? CORPSE_RESURRECTABLE_PVP : CORPSE_RESURRECTABLE_PVE);
    SetPvPDeath(false);

    if (!corpse->Create(GetMap()->GenerateLowGuid<HighGuid::Corpse>(), this))
    {
        delete corpse;
        return nullptr;
    }

    _corpseLocation.WorldRelocate(*this);
    _corpseTime = corpse->GetGhostTime();

    _cfb1 = ((0x00) | (GetRace() << 8) | (GetNativeGender() << 16) | (GetSkinId() << 24));
    _cfb2 = (GetFaceId() | (GetHairStyleId() << 8) | (GetHairColorId() << 16) | (GetFacialStyle() << 24));

    corpse->SetUInt32Value(CORPSE_FIELD_BYTES_1, _cfb1);
    corpse->SetUInt32Value(CORPSE_FIELD_BYTES_2, _cfb2);

    uint32 flags = CORPSE_FLAG_UNK2;
    if (HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_HIDE_HELM))
        flags |= CORPSE_FLAG_HIDE_HELM;
    if (HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_HIDE_CLOAK))
        flags |= CORPSE_FLAG_HIDE_CLOAK;
    if (InBattleground() && !InArena())
        flags |= CORPSE_FLAG_LOOTABLE;                      // to be able to remove insignia
    corpse->SetUInt32Value(CORPSE_FIELD_FLAGS, flags);

    corpse->SetUInt32Value(CORPSE_FIELD_DISPLAY_ID, GetNativeDisplayId());

    corpse->SetUInt32Value(CORPSE_FIELD_GUILD, GetGuildId());

    uint32 iDisplayID;
    uint32 iIventoryType;
    uint32 _cfi;
    for (uint8 i = 0; i < EQUIPMENT_SLOT_END; i++)
    {
        if (m_items[i])
        {
            iDisplayID = m_items[i]->GetTemplate()->DisplayInfoID;
            iIventoryType = m_items[i]->GetTemplate()->InventoryType;

            _cfi = iDisplayID | (iIventoryType << 24);
            corpse->SetUInt32Value(CORPSE_FIELD_ITEM + i, _cfi);
        }
    }

    // register for player, but not show
    GetMap()->AddCorpse(corpse);

    // we do not need to save corpses for BG/arenas
    if (!GetMap()->IsBattlegroundOrArena())
        corpse->SaveToDB();

    return corpse;
}

void Player::SpawnCorpseBones(bool triggerSave /*= true*/)
{
    _corpseLocation.WorldRelocate();
    if (GetMap()->ConvertCorpseToBones(GetGUID()))
        if (triggerSave && !GetSession()->PlayerLogoutWithSave())   // at logout we will already store the player
            SaveToDB();                                             // prevent loading as ghost without corpse
}

Corpse* Player::GetCorpse() const
{
    return GetMap()->GetCorpseByPlayer(GetGUID());
}

void Player::RepopAtGraveyard()
{
    // note: this can be called also when the player is alive
    // for example from WorldSession::HandleMovementOpcodes

    AreaTableEntry const* zone = sAreaTableStore.LookupEntry(GetAreaId());

    bool shouldResurrect = false;
    // Such zones are considered unreachable as a ghost and the player must be automatically revived
    if ((!IsAlive() && zone && zone->Flags & AREA_FLAG_NEED_FLY) || GetTransport() || GetPositionZ() < GetMap()->GetMinHeight(GetPositionX(), GetPositionY()))
    {
        shouldResurrect = true;
        SpawnCorpseBones();
    }

    WorldSafeLocsEntry const* ClosestGrave;

    // Special handle for battleground maps
    if (Battleground* bg = GetBattleground())
        ClosestGrave = bg->GetClosestGraveyard(this);
    else
    {
        if (Battlefield* bf = sBattlefieldMgr->GetBattlefieldToZoneId(GetZoneId()))
            ClosestGrave = bf->GetClosestGraveyard(this);
        else
            ClosestGrave = sObjectMgr->GetClosestGraveyard(GetPositionX(), GetPositionY(), GetPositionZ(), GetMapId(), GetTeam());
    }

    // stop countdown until repop
    m_deathTimer = 0;

    // if no grave found, stay at the current location
    // and don't show spirit healer location
    if (ClosestGrave)
    {
        TeleportTo(ClosestGrave->Continent, ClosestGrave->Loc.X, ClosestGrave->Loc.Y, ClosestGrave->Loc.Z, GetOrientation(), shouldResurrect ? TELE_REVIVE_AT_TELEPORT : 0);
        if (isDead())                                        // not send if alive, because it used in TeleportTo()
        {
            WorldPackets::Misc::DeathReleaseLoc packet;
            packet.MapID = ClosestGrave->Continent;
            packet.Loc = Position(ClosestGrave->Loc.X, ClosestGrave->Loc.Y, ClosestGrave->Loc.Z);
            GetSession()->SendPacket(packet.Write());
        }
    }
    else if (GetPositionZ() < GetMap()->GetMinHeight(GetPositionX(), GetPositionY()))
        TeleportTo(m_homebindMapId, m_homebindX, m_homebindY, m_homebindZ, GetOrientation());

    RemoveFlag(PLAYER_FLAGS, PLAYER_FLAGS_IS_OUT_OF_BOUNDS);
}

void Player::UpdateDefense()
{
    if (UpdateSkill(SKILL_DEFENSE, sWorld->getIntConfig(CONFIG_SKILL_GAIN_DEFENSE)))
        UpdateDefenseBonusesMod(); // update dependent from defense skill part
}

void Player::HandleBaseModFlatValue(BaseModGroup modGroup, float amount, bool apply)
{
    if (modGroup >= BASEMOD_END)
    {
        TC_LOG_ERROR("spells", "Player::HandleBaseModValue: Invalid BaseModGroup/BaseModType ({}/{}) for player '{}' ({})",
            modGroup, FLAT_MOD, GetName(), GetGUID().ToString());
        return;
    }

    m_auraBaseFlatMod[modGroup] += apply ? amount : -amount;
    UpdateBaseModGroup(modGroup);
}

void Player::ApplyBaseModPctValue(BaseModGroup modGroup, float pct)
{
    if (modGroup >= BASEMOD_END)
    {
        TC_LOG_ERROR("spells", "Player::HandleBaseModValue: Invalid BaseModGroup/BaseModType ({}/{}) for player '{}' ({})",
            modGroup, FLAT_MOD, GetName(), GetGUID().ToString());
        return;
    }

    m_auraBasePctMod[modGroup] += CalculatePct(1.0f, pct);
    UpdateBaseModGroup(modGroup);
}

void Player::SetBaseModFlatValue(BaseModGroup modGroup, float val)
{
    if (m_auraBaseFlatMod[modGroup] == val)
        return;

    m_auraBaseFlatMod[modGroup] = val;
    UpdateBaseModGroup(modGroup);
}

void Player::SetBaseModPctValue(BaseModGroup modGroup, float val)
{
    if (m_auraBasePctMod[modGroup] == val)
        return;

    m_auraBasePctMod[modGroup] = val;
    UpdateBaseModGroup(modGroup);
}

void Player::UpdateDamageDoneMods(WeaponAttackType attackType, int32 skipEnchantSlot /*= -1*/)
{
    Unit::UpdateDamageDoneMods(attackType, skipEnchantSlot);

    UnitMods unitMod;
    switch (attackType)
    {
        case BASE_ATTACK:
            unitMod = UNIT_MOD_DAMAGE_MAINHAND;
            break;
        case OFF_ATTACK:
            unitMod = UNIT_MOD_DAMAGE_OFFHAND;
            break;
        case RANGED_ATTACK:
            unitMod = UNIT_MOD_DAMAGE_RANGED;
            break;
        default:
            ABORT();
            break;
    }

    float amount = 0.0f;
    Item* item = GetWeaponForDamageMods(attackType);
    if (!item)
        return;

    for (uint8 slot = 0; slot < MAX_ENCHANTMENT_SLOT; ++slot)
    {
        if (skipEnchantSlot == slot)
            continue;

        SpellItemEnchantmentEntry const* enchantmentEntry = sSpellItemEnchantmentStore.LookupEntry(item->GetEnchantmentId(EnchantmentSlot(slot)));
        if (!enchantmentEntry)
            continue;

        for (uint8 i = 0; i < MAX_ITEM_ENCHANTMENT_EFFECTS; ++i)
        {
            switch (enchantmentEntry->Effect[i])
            {
                case ITEM_ENCHANTMENT_TYPE_DAMAGE:
                    amount += enchantmentEntry->EffectPointsMin[i];
                    break;
                case ITEM_ENCHANTMENT_TYPE_TOTEM:
                    if (GetClass() == CLASS_SHAMAN)
                        amount += enchantmentEntry->EffectPointsMin[i] * item->GetTemplate()->Delay / 1000.0f; // Wrath Rockbiter increases dps by flat amount
                    break;
                default:
                    break;
            }
        }
    }

    // For +damage enchants and rockbiter we add flat mods to the primary weapon's spell school
    SpellSchools school = GetMeleeDamageSchool(attackType, 0);
    HandleDamageFlatModifier(unitMod, school, amount, true);
}

void Player::UpdateBaseModGroup(BaseModGroup modGroup)
{
    if (!CanModifyStats())
        return;

    switch (modGroup)
    {
        case CRIT_PERCENTAGE:              UpdateCritPercentage(BASE_ATTACK);                          break;
        case RANGED_CRIT_PERCENTAGE:       UpdateCritPercentage(RANGED_ATTACK);                        break;
        case OFFHAND_CRIT_PERCENTAGE:      UpdateCritPercentage(OFF_ATTACK);                           break;
        case SHIELD_BLOCK_VALUE:           UpdateShieldBlockValue();                                   break;
        default: break;
    }
}

float Player::GetBaseModValue(BaseModGroup modGroup, BaseModType modType) const
{
    if (modGroup >= BASEMOD_END || modType >= MOD_END)
    {
        TC_LOG_ERROR("spells", "Player::GetBaseModValue: Invalid BaseModGroup/BaseModType ({}/{}) for player '{}' ({})",
            modGroup, modType, GetName(), GetGUID().ToString());
        return 0.0f;
    }

    return (modType == FLAT_MOD ? m_auraBaseFlatMod[modGroup] : m_auraBasePctMod[modGroup]);
}

float Player::GetTotalBaseModValue(BaseModGroup modGroup) const
{
    if (modGroup >= BASEMOD_END)
    {
        TC_LOG_ERROR("spells", "Player::GetTotalBaseModValue: Invalid BaseModGroup ({}) for player '{}' ({})",
            modGroup, GetName(), GetGUID().ToString());
        return 0.0f;
    }

    return m_auraBaseFlatMod[modGroup] * m_auraBasePctMod[modGroup];
}

uint32 Player::GetShieldBlockValue() const
{
    /** @epoch-start */
    // float value = std::max(0.f, (m_auraBaseFlatMod[SHIELD_BLOCK_VALUE] + GetStat(STAT_STRENGTH) * 0.5f - 10) * m_auraBasePctMod[SHIELD_BLOCK_VALUE]);
    float value = std::max(0.f, (m_auraBaseFlatMod[SHIELD_BLOCK_VALUE] + GetStat(STAT_STRENGTH) / 20 - 1) * m_auraBasePctMod[SHIELD_BLOCK_VALUE]);
    /** @epoch-end */

    return uint32(value);
}

float Player::GetMeleeCritFromAgility() const
{
    uint8 level = GetLevel();
    uint32 pclass = GetClass();

    if (level > GT_MAX_LEVEL)
        level = GT_MAX_LEVEL;

    GtChanceToMeleeCritBaseEntry const* critBase  = sGtChanceToMeleeCritBaseStore.LookupEntry(pclass-1);
    GtChanceToMeleeCritEntry     const* critRatio = sGtChanceToMeleeCritStore.LookupEntry((pclass-1)*GT_MAX_LEVEL + level-1);
    if (critBase == nullptr || critRatio == nullptr)
        return 0.0f;

    float crit = critBase->Data + GetStat(STAT_AGILITY)*critRatio->Data;
    return crit*100.0f;
}

// @tswow-begin move dodge_base/crit_to_doge values to global scope and remove const
// Table for base dodge values
float dodge_base[MAX_CLASSES] =
{
     0.036640f, // Warrior
     0.034943f, // Paladin
    -0.040873f, // Hunter
     0.020957f, // Rogue
     0.034178f, // Priest
     0.036640f, // DK
     0.021080f, // Shaman
     0.036587f, // Mage
     0.024211f, // Warlock
     0.0f,      // ??
     0.056097f,  // Druid

     // default values for custom classes
    .035f,.035f,.035f,.035f,.035f,.035f,.035f,
    .035f,.035f,.035f,.035f,.035f,.035f,.035f,
    .035f,.035f,.035f,.035f,.035f,.035f,.035f,
};
// Crit/agility to dodge/agility coefficient multipliers; 3.2.0 increased required agility by 15%
float crit_to_dodge[MAX_CLASSES] =
{
    1.1f, // [1]  Warrior
    1.0f, // [2]  Paladin
    1.6f, // [3]  Hunter
    2.0f, // [4]  Rogue
    1.0f, // [5]  Priest
    1.0f, // [6]  DK
    1.0f, // [7]  Shaman
    1.0f, // [8]  Mage
    1.0f, // [9]  Warlock
    0.0f, // [10] <Unused>
    1.7f, // [11] Druid

    // default values for custom classes
    .87f,.87f,.87f,.87f,.87f,.87f,.87f,
    .87f,.87f,.87f,.87f,.87f,.87f,.87f,
    .87f,.87f,.87f,.87f,.87f,.87f,.87f,
};

float Player::GetDodgeFromAgility(float amount) const
{
// @tswow-end

    uint8 level = GetLevel();
    uint32 pclass = GetClass();

    if (level > GT_MAX_LEVEL)
        level = GT_MAX_LEVEL;

    // Dodge per agility is proportional to crit per agility, which is available from DBC files
    GtChanceToMeleeCritEntry  const* dodgeRatio = sGtChanceToMeleeCritStore.LookupEntry((pclass-1)*GT_MAX_LEVEL + level-1);
    if (dodgeRatio == nullptr || pclass > MAX_CLASSES)
        return 0.0f;

    return (100.0f * amount * dodgeRatio->Data * crit_to_dodge[pclass - 1]);
}

float Player::GetSpellCritFromIntellect() const
{
    uint8 level = GetLevel();
    uint32 pclass = GetClass();

    if (level > GT_MAX_LEVEL)
        level = GT_MAX_LEVEL;

    GtChanceToSpellCritBaseEntry const* critBase  = sGtChanceToSpellCritBaseStore.LookupEntry(pclass-1);
    GtChanceToSpellCritEntry     const* critRatio = sGtChanceToSpellCritStore.LookupEntry((pclass-1)*GT_MAX_LEVEL + level-1);
    if (critBase == nullptr || critRatio == nullptr)
        return 0.0f;

    float crit = critBase->Data + GetStat(STAT_INTELLECT) * critRatio->Data;
    return crit * 100.0f;
}

float Player::GetRatingMultiplier(CombatRating cr) const
{
    uint8 level = GetLevel();

    if (level > GT_MAX_LEVEL)
        level = GT_MAX_LEVEL;

    GtCombatRatingsEntry const* Rating = sGtCombatRatingsStore.LookupEntry(cr*GT_MAX_LEVEL+level-1);
    // gtOCTClassCombatRatingScalarStore.dbc starts with 1, CombatRating with zero, so cr+1
    GtOCTClassCombatRatingScalarEntry const* classRating = sGtOCTClassCombatRatingScalarStore.LookupEntry((GetClass()-1)*GT_MAX_RATING+cr+1);
    if (!Rating || !classRating)
        return 1.0f;                                        // By default use minimum coefficient (not must be called)

    return classRating->Data / Rating->Data;
}

float Player::GetRatingBonusValue(CombatRating cr) const
{
    return float(GetUInt32Value(PLAYER_FIELD_COMBAT_RATING_1 + AsUnderlyingType(cr))) * GetRatingMultiplier(cr);
}

float Player::GetExpertiseDodgeOrParryReduction(WeaponAttackType attType) const
{
    switch (attType)
    {
        case BASE_ATTACK:
            return GetUInt32Value(PLAYER_EXPERTISE) / 4.0f;
        case OFF_ATTACK:
            return GetUInt32Value(PLAYER_OFFHAND_EXPERTISE) / 4.0f;
        default:
            break;
    }
    return 0.0f;
}

void Player::ApplyRatingMod(CombatRating combatRating, int32 value, bool apply)
{
    float oldRating = m_baseRatingValue[combatRating];
    m_baseRatingValue[combatRating] += (apply ? value : -value);

    // explicit affected values
    float const multiplier = GetRatingMultiplier(combatRating);
    float const oldVal = oldRating * multiplier;
    float const newVal = m_baseRatingValue[combatRating] * multiplier;
    switch (combatRating)
    {
        case CR_HASTE_MELEE:
            ApplyAttackTimePercentMod(BASE_ATTACK, oldVal, false);
            ApplyAttackTimePercentMod(OFF_ATTACK, oldVal, false);
            ApplyAttackTimePercentMod(BASE_ATTACK, newVal, true);
            ApplyAttackTimePercentMod(OFF_ATTACK, newVal, true);
            break;
        case CR_HASTE_RANGED:
            ApplyAttackTimePercentMod(RANGED_ATTACK, oldVal, false);
            ApplyAttackTimePercentMod(RANGED_ATTACK, newVal, true);
            break;
        case CR_HASTE_SPELL:
            ApplyCastTimePercentMod(oldVal, false);
            ApplyCastTimePercentMod(newVal, true);
            break;
        default:
            break;
    }

    UpdateRating(combatRating);
}

void Player::UpdateRating(CombatRating cr)
{
    int32 amount = m_baseRatingValue[cr];
    // Apply bonus from SPELL_AURA_MOD_RATING_FROM_STAT
    // stat used stored in miscValueB for this aura
    AuraEffectList const& modRatingFromStat = GetAuraEffectsByType(SPELL_AURA_MOD_RATING_FROM_STAT);
    for (AuraEffect const* aurEff : modRatingFromStat)
        if (aurEff->GetMiscValue() & (1 << cr))
            amount += int32(CalculatePct(GetStat(Stats(aurEff->GetMiscValueB())), aurEff->GetAmount()));

    if (amount < 0)
        amount = 0;
    SetUInt32Value(PLAYER_FIELD_COMBAT_RATING_1 + AsUnderlyingType(cr), uint32(amount));

    bool affectStats = CanModifyStats();

    switch (cr)
    {
        case CR_WEAPON_SKILL:                               // Implemented in Unit::RollMeleeOutcomeAgainst
        case CR_DEFENSE_SKILL:
            UpdateDefenseBonusesMod();
            break;
        case CR_DODGE:
            UpdateDodgePercentage();
            break;
        case CR_PARRY:
            UpdateParryPercentage();
            break;
        case CR_BLOCK:
            UpdateBlockPercentage();
            break;
        case CR_HIT_MELEE:
            UpdateMeleeHitChances();
            break;
        case CR_HIT_RANGED:
            UpdateRangedHitChances();
            break;
        case CR_HIT_SPELL:
            UpdateSpellHitChances();
            break;
        case CR_CRIT_MELEE:
            if (affectStats)
            {
                UpdateCritPercentage(BASE_ATTACK);
                UpdateCritPercentage(OFF_ATTACK);
            }
            break;
        case CR_CRIT_RANGED:
            if (affectStats)
                UpdateCritPercentage(RANGED_ATTACK);
            break;
        case CR_CRIT_SPELL:
            if (affectStats)
                UpdateAllSpellCritChances();
            break;
        case CR_HIT_TAKEN_MELEE:                            // Implemented in Unit::MeleeMissChanceCalc
        case CR_HIT_TAKEN_RANGED:
            break;
        case CR_HIT_TAKEN_SPELL:                            // Implemented in Unit::MagicSpellHitResult
            break;
        case CR_CRIT_TAKEN_MELEE:                           // Implemented in Unit::RollMeleeOutcomeAgainst (only for chance to crit)
        case CR_CRIT_TAKEN_RANGED:
            break;
        case CR_CRIT_TAKEN_SPELL:                           // Implemented in Unit::SpellCriticalBonus (only for chance to crit)
            break;
        case CR_HASTE_MELEE:                                // Implemented in Player::ApplyRatingMod
        case CR_HASTE_RANGED:
        case CR_HASTE_SPELL:
            break;
        case CR_WEAPON_SKILL_MAINHAND:                      // Implemented in Unit::RollMeleeOutcomeAgainst
        case CR_WEAPON_SKILL_OFFHAND:
        case CR_WEAPON_SKILL_RANGED:
            break;
        case CR_EXPERTISE:
            if (affectStats)
            {
                UpdateExpertise(BASE_ATTACK);
                UpdateExpertise(OFF_ATTACK);
            }
            break;
        case CR_ARMOR_PENETRATION:
            if (affectStats)
                UpdateArmorPenetration(amount);
            break;
    }
}

void Player::UpdateAllRatings()
{
    for (uint8 cr = 0; cr < MAX_COMBAT_RATING; ++cr)
        UpdateRating(CombatRating(cr));
}

void Player::SetRegularAttackTime()
{
    for (uint8 i = 0; i < MAX_ATTACK; ++i)
    {
        Item* tmpitem = GetWeaponForAttack(WeaponAttackType(i), true);
        if (tmpitem && !tmpitem->IsBroken())
        {
            ItemTemplate const* proto = tmpitem->GetTemplate();
            if (proto->Delay)
                SetAttackTime(WeaponAttackType(i), proto->Delay);
        }
        else
            SetAttackTime(WeaponAttackType(i), BASE_ATTACK_TIME);  // If there is no weapon reset attack time to base (might have been changed from forms)
    }
}

void Player::StoreRaidMapDifficulty()
{
    m_raidMapDifficulty = GetMap()->GetDifficulty();
}

void Player::SendActionButtons(uint32 state) const
{
    WorldPacket data(SMSG_ACTION_BUTTONS, 1+(MAX_ACTION_BUTTONS*4));
    data << uint8(state);
    /*
        state can be 0, 1, 2
        0 - Looks to be sent when initial action buttons get sent, however on Trinity we use 1 since 0 had some difficulties
        1 - Used in any SMSG_ACTION_BUTTONS packet with button data on Trinity. Only used after spec swaps on retail.
        2 - Clears the action bars client sided. This is sent during spec swap before unlearning and before sending the new buttons
    */
    if (state != 2)
    {
        for (uint8 button = 0; button < MAX_ACTION_BUTTONS; ++button)
        {
            ActionButtonList::const_iterator itr = m_actionButtons.find(button);
            if (itr != m_actionButtons.end() && itr->second.uState != ACTIONBUTTON_DELETED)
                data << uint32(itr->second.packedData);
            else
                data << uint32(0);
        }
    }

    SendDirectMessage(&data);

}

bool Player::IsActionButtonDataValid(uint8 button, uint32 action, uint8 type) const
{
    if (button >= MAX_ACTION_BUTTONS)
    {
        TC_LOG_ERROR("entities.player", "Player::IsActionButtonDataValid: Action {} not added into button {} for player {} ({}): button must be < {}",
            action, button, GetName(), GetGUID().ToString(), MAX_ACTION_BUTTONS);
        return false;
    }

    if (action >= MAX_ACTION_BUTTON_ACTION_VALUE)
    {
        TC_LOG_ERROR("entities.player", "Player::IsActionButtonDataValid: Action {} not added into button {} for player {} ({}): action must be < {}",
            action, button, GetName(), GetGUID().ToString(), MAX_ACTION_BUTTON_ACTION_VALUE);
        return false;
    }

    switch (type)
    {
        case ACTION_BUTTON_SPELL:
            if (!sSpellMgr->GetSpellInfo(action))
            {
                TC_LOG_DEBUG("entities.player", "Player::IsActionButtonDataValid: Spell action {} not added into button {} for player {} ({}): spell does not exist. This can be due to a character imported from a different expansion",
                    action, button, GetName(), GetGUID().ToString());
                return false;
            }

            if (!HasSpell(action))
            {
                TC_LOG_DEBUG("entities.player", "Player::IsActionButtonDataValid: Spell action {} not added into button {} for player {} ({}): player does not known this spell, this can be due to a player changing their talents",
                    action, button, GetName(), GetGUID().ToString());
                return false;
            }
            break;
        case ACTION_BUTTON_ITEM:
            if (!sObjectMgr->GetItemTemplate(action))
            {
                TC_LOG_ERROR("entities.player", "Player::IsActionButtonDataValid: Item action {} not added into button {} for player {} ({}): item not exist",
                    action, button, GetName(), GetGUID().ToString());
                return false;
            }
            break;
        case ACTION_BUTTON_C:
        case ACTION_BUTTON_CMACRO:
        case ACTION_BUTTON_MACRO:
        case ACTION_BUTTON_EQSET:
            break;
        default:
            TC_LOG_ERROR("entities.player", "Player::IsActionButtonDataValid: Unknown action type {}", type);
            return false;                                          // other cases not checked at this moment
    }

    return true;
}

ActionButton* Player::addActionButton(uint8 button, uint32 action, uint8 type)
{
    if (!IsActionButtonDataValid(button, action, type))
        return nullptr;

    // it create new button (NEW state) if need or return existing
    ActionButton& ab = m_actionButtons[button];

    // set data and update to CHANGED if not NEW
    ab.SetActionAndType(action, ActionButtonType(type));

    TC_LOG_DEBUG("entities.player", "Player::AddActionButton: Player '{}' ({}) added action '{}' (type {}) to button '{}'",
        GetName(), GetGUID().ToString(), action, type, button);
    return &ab;
}

void Player::removeActionButton(uint8 button)
{
    ActionButtonList::iterator buttonItr = m_actionButtons.find(button);
    if (buttonItr == m_actionButtons.end() || buttonItr->second.uState == ACTIONBUTTON_DELETED)
        return;

    if (buttonItr->second.uState == ACTIONBUTTON_NEW)
        m_actionButtons.erase(buttonItr);                   // new and not saved
    else
        buttonItr->second.uState = ACTIONBUTTON_DELETED;    // saved, will deleted at next save

    TC_LOG_DEBUG("entities.player", "Player::RemoveActionButton: Player '{}' ({}) removed action button '{}'",
        GetName(), GetGUID().ToString(), button);
}

ActionButton const* Player::GetActionButton(uint8 button)
{
    ActionButtonList::iterator buttonItr = m_actionButtons.find(button);
    if (buttonItr == m_actionButtons.end() || buttonItr->second.uState == ACTIONBUTTON_DELETED)
        return nullptr;

    return &buttonItr->second;
}

void Player::SendMessageToSetInRange(WorldPacket const* data, float dist, bool self) const
{
    if (self)
        SendDirectMessage(data);

    Trinity::MessageDistDeliverer notifier(this, data, dist);
    Cell::VisitWorldObjects(this, notifier, dist);
}

void Player::SendMessageToSetInRange(WorldPacket const* data, float dist, bool self, bool own_team_only, bool required3dDist /*= false*/) const
{
    if (self)
        SendDirectMessage(data);

    Trinity::MessageDistDeliverer notifier(this, data, dist, own_team_only, nullptr, required3dDist);
    Cell::VisitWorldObjects(this, notifier, dist);
}

void Player::SendMessageToSet(WorldPacket const* data, Player const* skipped_rcvr) const
{
    if (skipped_rcvr != this)
        SendDirectMessage(data);

    // we use World::GetMaxVisibleDistance() because i cannot see why not use a distance
    // update: replaced by GetMap()->GetVisibilityDistance()
    Trinity::MessageDistDeliverer notifier(this, data, GetVisibilityRange(), false, skipped_rcvr);
    Cell::VisitWorldObjects(this, notifier, GetVisibilityRange());
}

void Player::SendDirectMessage(WorldPacket const* data) const
{
    m_session->SendPacket(data);
}

void Player::SendCinematicStart(uint32 CinematicSequenceId) const
{
    WorldPackets::Misc::TriggerCinematic packet;
    packet.CinematicID = CinematicSequenceId;
    SendDirectMessage(packet.Write());

    if (CinematicSequencesEntry const* sequence = sCinematicSequencesStore.LookupEntry(CinematicSequenceId))
        _cinematicMgr->SetActiveCinematicCamera(sequence->Camera[0]);
}

void Player::SendMovieStart(uint32 movieId)
{
    SetMovie(movieId);
    WorldPackets::Misc::TriggerMovie packet;
    packet.MovieID = movieId;
    SendDirectMessage(packet.Write());
}

uint32 Player::TeamForRace(uint8 race)
{
    if (ChrRacesEntry const* rEntry = sChrRacesStore.LookupEntry(race))
    {
        switch (rEntry->BaseLanguage)
        {
            case 1: return HORDE;
            case 7: return ALLIANCE;
        }
        TC_LOG_ERROR("entities.player", "Race ({}) has wrong teamid ({}) in DBC: wrong DBC files?", uint32(race), rEntry->BaseLanguage);
    }
    else
        TC_LOG_ERROR("entities.player", "Race ({}) not found in DBC: wrong DBC files?", uint32(race));

    return ALLIANCE;
}

void Player::SetFactionForRace(uint8 race)
{
    m_team = TeamForRace(race);

    ChrRacesEntry const* rEntry = sChrRacesStore.LookupEntry(race);
    SetFaction(rEntry ? rEntry->FactionID : 0);
}

ReputationRank Player::GetReputationRank(uint32 faction) const
{
    FactionEntry const* factionEntry = sFactionStore.LookupEntry(faction);
    return GetReputationMgr().GetRank(factionEntry);
}

// Calculate total reputation percent player gain with quest/creature level
int32 Player::CalculateReputationGain(ReputationSource source, uint32 creatureOrQuestLevel, int32 rep, int32 faction, bool noQuestBonus)
{
    float percent = 100.0f;

    float repMod = noQuestBonus ? 0.0f : float(GetTotalAuraModifier(SPELL_AURA_MOD_REPUTATION_GAIN));

    // faction specific auras only seem to apply to kills
    if (source == REPUTATION_SOURCE_KILL)
        repMod += GetTotalAuraModifierByMiscValue(SPELL_AURA_MOD_FACTION_REPUTATION_GAIN, faction);

    percent += rep > 0 ? repMod : -repMod;

    float rate;
    switch (source)
    {
        case REPUTATION_SOURCE_KILL:
            rate = sWorld->getRate(RATE_REPUTATION_LOWLEVEL_KILL);
            break;
        case REPUTATION_SOURCE_QUEST:
        case REPUTATION_SOURCE_DAILY_QUEST:
        case REPUTATION_SOURCE_WEEKLY_QUEST:
        case REPUTATION_SOURCE_MONTHLY_QUEST:
        case REPUTATION_SOURCE_REPEATABLE_QUEST:
            rate = sWorld->getRate(RATE_REPUTATION_LOWLEVEL_QUEST);
            break;
        case REPUTATION_SOURCE_SPELL:
        default:
            rate = 1.0f;
            break;
    }

    // @tswow-begin player argument
    if (rate != 1.0f && creatureOrQuestLevel <= Trinity::XP::GetGrayLevel(this,GetLevel()))
    // @tswow-end
        percent *= rate;

    if (percent <= 0.0f)
        return 0;

    // Multiply result with the faction specific rate
    if (RepRewardRate const* repData = sObjectMgr->GetRepRewardRate(faction))
    {
        float repRate = 0.0f;
        switch (source)
        {
            case REPUTATION_SOURCE_KILL:
                repRate = repData->creatureRate;
                break;
            case REPUTATION_SOURCE_QUEST:
                repRate = repData->questRate;
                break;
            case REPUTATION_SOURCE_DAILY_QUEST:
                repRate = repData->questDailyRate;
                break;
            case REPUTATION_SOURCE_WEEKLY_QUEST:
                repRate = repData->questWeeklyRate;
                break;
            case REPUTATION_SOURCE_MONTHLY_QUEST:
                repRate = repData->questMonthlyRate;
                break;
            case REPUTATION_SOURCE_REPEATABLE_QUEST:
                repRate = repData->questRepeatableRate;
                break;
            case REPUTATION_SOURCE_SPELL:
                repRate = repData->spellRate;
                break;
        }

        // for custom, a rate of 0.0 will totally disable reputation gain for this faction/type
        if (repRate <= 0.0f)
            return 0;

        percent *= repRate;
    }

    if (source != REPUTATION_SOURCE_SPELL && GetsRecruitAFriendBonus(false))
        percent *= 1.0f + sWorld->getRate(RATE_REPUTATION_RECRUIT_A_FRIEND_BONUS);

    return CalculatePct(rep, percent);
}

// Calculates how many reputation points player gains in victim's enemy factions
void Player::RewardReputation(Unit* victim, float rate)
{
    if (!victim || victim->GetTypeId() == TYPEID_PLAYER)
        return;

    if (victim->ToCreature()->IsReputationGainDisabled())
        return;

    ReputationOnKillEntry const* Rep = sObjectMgr->GetReputationOnKilEntry(victim->ToCreature()->GetCreatureTemplate()->Entry);
    if (!Rep)
        return;

    uint32 ChampioningFaction = 0;

    if (GetChampioningFaction())
    {
        // support for: Championing - http://www.wowwiki.com/Championing
        Map const* map = GetMap();
        if (map->IsNonRaidDungeon())
            if (LFGDungeonEntry const* dungeon = GetLFGDungeon(map->GetId(), map->GetDifficulty()))
                if (dungeon->TargetLevel == 80)
                    ChampioningFaction = GetChampioningFaction();
    }

    uint32 team = GetTeam();

    if (Rep->RepFaction1 && (!Rep->TeamDependent || team == ALLIANCE))
    {
        int32 donerep1 = CalculateReputationGain(REPUTATION_SOURCE_KILL, victim->GetLevel(), Rep->RepValue1, ChampioningFaction ? ChampioningFaction : Rep->RepFaction1);
        donerep1 = int32(donerep1 * rate);

        FactionEntry const* factionEntry1 = sFactionStore.LookupEntry(ChampioningFaction ? ChampioningFaction : Rep->RepFaction1);
        uint32 current_reputation_rank1 = GetReputationMgr().GetRank(factionEntry1);
        if (factionEntry1)
            GetReputationMgr().ModifyReputation(factionEntry1, donerep1, current_reputation_rank1 > Rep->ReputationMaxCap1);
    }

    if (Rep->RepFaction2 && (!Rep->TeamDependent || team == HORDE))
    {
        int32 donerep2 = CalculateReputationGain(REPUTATION_SOURCE_KILL, victim->GetLevel(), Rep->RepValue2, ChampioningFaction ? ChampioningFaction : Rep->RepFaction2);
        donerep2 = int32(donerep2 * rate);

        FactionEntry const* factionEntry2 = sFactionStore.LookupEntry(ChampioningFaction ? ChampioningFaction : Rep->RepFaction2);
        uint32 current_reputation_rank2 = GetReputationMgr().GetRank(factionEntry2);
        if (factionEntry2)
            GetReputationMgr().ModifyReputation(factionEntry2, donerep2, current_reputation_rank2 > Rep->ReputationMaxCap2);
    }
}

// Calculate how many reputation points player gain with the quest
void Player::RewardReputation(Quest const* quest)
{
    for (uint8 i = 0; i < QUEST_REPUTATIONS_COUNT; ++i)
    {
        uint32 rewardFactionId = quest->RewardFactionId[i];

        if (!rewardFactionId)
            continue;

        if (!GetReputationMgr().IsReputationAllowedForTeam(GetTeamId(), rewardFactionId))
            continue;

        int32 rep = 0;

        if (quest->RewardFactionValueIdOverride[i])
        {
            rep = quest->RewardFactionValueIdOverride[i] / 100;
        }
        else
        {
            uint32 row = ((quest->RewardFactionValueId[i] < 0) ? 1 : 0) + 1;
            if (QuestFactionRewEntry const* questFactionRewEntry = sQuestFactionRewardStore.LookupEntry(row))
            {
                uint32 field = abs(quest->RewardFactionValueId[i]);
                rep = questFactionRewEntry->Difficulty[field];
            }
        }

        if (!rep)
            continue;

        if (quest->IsDaily())
            rep = CalculateReputationGain(REPUTATION_SOURCE_DAILY_QUEST, GetQuestLevel(quest), rep, rewardFactionId, false);
        else if (quest->IsWeekly())
            rep = CalculateReputationGain(REPUTATION_SOURCE_WEEKLY_QUEST, GetQuestLevel(quest), rep, rewardFactionId, false);
        else if (quest->IsMonthly())
            rep = CalculateReputationGain(REPUTATION_SOURCE_MONTHLY_QUEST, GetQuestLevel(quest), rep, rewardFactionId, false);
        else if (quest->IsRepeatable())
            rep = CalculateReputationGain(REPUTATION_SOURCE_REPEATABLE_QUEST, GetQuestLevel(quest), rep, rewardFactionId, false);
        else
            rep = CalculateReputationGain(REPUTATION_SOURCE_QUEST, GetQuestLevel(quest), rep, rewardFactionId, false);

        if (FactionEntry const* factionEntry = sFactionStore.LookupEntry(rewardFactionId))
            GetReputationMgr().ModifyReputation(factionEntry, rep);
    }
}

ScalingStatDistributionEntry const* Player::GetScalingStatDistributionFor(ItemTemplate const& itemTemplate) const
{
    if (!itemTemplate.ScalingStatDistribution)
        return nullptr;

    return sScalingStatDistributionStore.LookupEntry(itemTemplate.ScalingStatDistribution);
}

ScalingStatValuesEntry const* Player::GetScalingStatValuesFor(ItemTemplate const& itemTemplate) const
{
    if (!itemTemplate.ScalingStatValue)
        return nullptr;

    ScalingStatDistributionEntry const* ssd = GetScalingStatDistributionFor(itemTemplate);
    if (!ssd)
        return nullptr;

    // req. check at equip, but allow use for extended range if range limit max level, set proper level
    uint32 const ssd_level = std::min(uint32(GetLevel()), ssd->Maxlevel);
    return sScalingStatValuesStore.LookupEntry(ssd_level);
}

void Player::_ApplyItemBonuses(ItemTemplate const* proto, uint8 slot, bool apply, bool only_level_scale /*= false*/)
{
    if (slot >= INVENTORY_SLOT_BAG_END || !proto)
        return;

    ScalingStatDistributionEntry const* ssd = GetScalingStatDistributionFor(*proto);
    ScalingStatValuesEntry const* ssv = GetScalingStatValuesFor(*proto);
    if (only_level_scale && (!ssd || !ssv))
        return;

    for (uint8 i = 0; i < MAX_ITEM_PROTO_STATS; ++i)
    {
        uint32 statType = 0;
        int32  val = 0;
        // If set ScalingStatDistribution need get stats and values from it
        if (ssd && ssv)
        {
            if (ssd->StatID[i] < 0)
                continue;
            statType = ssd->StatID[i];
            val = (ssv->getssdMultiplier(proto->ScalingStatValue) * ssd->Bonus[i]) / 10000;
        }
        else
        {
            if (i >= proto->StatsCount)
                continue;
            statType = proto->ItemStat[i].ItemStatType;
            val = proto->ItemStat[i].ItemStatValue;
        }

        if (val == 0)
            continue;

        switch (statType)
        {
            case ITEM_MOD_MANA:
                HandleStatFlatModifier(UNIT_MOD_MANA, BASE_VALUE, float(val), apply);
                break;
            case ITEM_MOD_HEALTH:                           // modify HP
                HandleStatFlatModifier(UNIT_MOD_HEALTH, BASE_VALUE, float(val), apply);
                break;
            case ITEM_MOD_AGILITY:                          // modify agility
                HandleStatFlatModifier(UNIT_MOD_STAT_AGILITY, BASE_VALUE, float(val), apply);
                UpdateStatBuffMod(STAT_AGILITY);
                break;
            case ITEM_MOD_STRENGTH:                         //modify strength
                HandleStatFlatModifier(UNIT_MOD_STAT_STRENGTH, BASE_VALUE, float(val), apply);
                UpdateStatBuffMod(STAT_STRENGTH);
                break;
            case ITEM_MOD_INTELLECT:                        //modify intellect
                HandleStatFlatModifier(UNIT_MOD_STAT_INTELLECT, BASE_VALUE, float(val), apply);
                UpdateStatBuffMod(STAT_INTELLECT);
                break;
            case ITEM_MOD_SPIRIT:                           //modify spirit
                HandleStatFlatModifier(UNIT_MOD_STAT_SPIRIT, BASE_VALUE, float(val), apply);
                UpdateStatBuffMod(STAT_SPIRIT);
                break;
            case ITEM_MOD_STAMINA:                          //modify stamina
                HandleStatFlatModifier(UNIT_MOD_STAT_STAMINA, BASE_VALUE, float(val), apply);
                UpdateStatBuffMod(STAT_STAMINA);
                break;
            case ITEM_MOD_DEFENSE_SKILL_RATING:
                ApplyRatingMod(CR_DEFENSE_SKILL, int32(val), apply);
                break;
            case ITEM_MOD_DODGE_RATING:
                ApplyRatingMod(CR_DODGE, int32(val), apply);
                break;
            case ITEM_MOD_PARRY_RATING:
                ApplyRatingMod(CR_PARRY, int32(val), apply);
                break;
            case ITEM_MOD_BLOCK_RATING:
                ApplyRatingMod(CR_BLOCK, int32(val), apply);
                break;
            case ITEM_MOD_HIT_MELEE_RATING:
                ApplyRatingMod(CR_HIT_MELEE, int32(val), apply);
                break;
            case ITEM_MOD_HIT_RANGED_RATING:
                ApplyRatingMod(CR_HIT_RANGED, int32(val), apply);
                break;
            case ITEM_MOD_HIT_SPELL_RATING:
                ApplyRatingMod(CR_HIT_SPELL, int32(val), apply);
                break;
            case ITEM_MOD_CRIT_MELEE_RATING:
                ApplyRatingMod(CR_CRIT_MELEE, int32(val), apply);
                break;
            case ITEM_MOD_CRIT_RANGED_RATING:
                ApplyRatingMod(CR_CRIT_RANGED, int32(val), apply);
                break;
            case ITEM_MOD_CRIT_SPELL_RATING:
                ApplyRatingMod(CR_CRIT_SPELL, int32(val), apply);
                break;
            case ITEM_MOD_HIT_TAKEN_MELEE_RATING:
                ApplyRatingMod(CR_HIT_TAKEN_MELEE, int32(val), apply);
                break;
            case ITEM_MOD_HIT_TAKEN_RANGED_RATING:
                ApplyRatingMod(CR_HIT_TAKEN_RANGED, int32(val), apply);
                break;
            case ITEM_MOD_HIT_TAKEN_SPELL_RATING:
                ApplyRatingMod(CR_HIT_TAKEN_SPELL, int32(val), apply);
                break;
            case ITEM_MOD_CRIT_TAKEN_MELEE_RATING:
                ApplyRatingMod(CR_CRIT_TAKEN_MELEE, int32(val), apply);
                break;
            case ITEM_MOD_CRIT_TAKEN_RANGED_RATING:
                ApplyRatingMod(CR_CRIT_TAKEN_RANGED, int32(val), apply);
                break;
            case ITEM_MOD_CRIT_TAKEN_SPELL_RATING:
                ApplyRatingMod(CR_CRIT_TAKEN_SPELL, int32(val), apply);
                break;
            case ITEM_MOD_HASTE_MELEE_RATING:
                ApplyRatingMod(CR_HASTE_MELEE, int32(val), apply);
                break;
            case ITEM_MOD_HASTE_RANGED_RATING:
                ApplyRatingMod(CR_HASTE_RANGED, int32(val), apply);
                break;
            case ITEM_MOD_HASTE_SPELL_RATING:
                ApplyRatingMod(CR_HASTE_SPELL, int32(val), apply);
                break;
            case ITEM_MOD_HIT_RATING:
                ApplyRatingMod(CR_HIT_MELEE, int32(val), apply);
                ApplyRatingMod(CR_HIT_RANGED, int32(val), apply);
                ApplyRatingMod(CR_HIT_SPELL, int32(val), apply);
                break;
            case ITEM_MOD_CRIT_RATING:
                ApplyRatingMod(CR_CRIT_MELEE, int32(val), apply);
                ApplyRatingMod(CR_CRIT_RANGED, int32(val), apply);
                ApplyRatingMod(CR_CRIT_SPELL, int32(val), apply);
                break;
            case ITEM_MOD_HIT_TAKEN_RATING:
                ApplyRatingMod(CR_HIT_TAKEN_MELEE, int32(val), apply);
                ApplyRatingMod(CR_HIT_TAKEN_RANGED, int32(val), apply);
                ApplyRatingMod(CR_HIT_TAKEN_SPELL, int32(val), apply);
                break;
            case ITEM_MOD_CRIT_TAKEN_RATING:
                ApplyRatingMod(CR_CRIT_TAKEN_MELEE, int32(val), apply);
                ApplyRatingMod(CR_CRIT_TAKEN_RANGED, int32(val), apply);
                ApplyRatingMod(CR_CRIT_TAKEN_SPELL, int32(val), apply);
                break;
            case ITEM_MOD_RESILIENCE_RATING:
                ApplyRatingMod(CR_CRIT_TAKEN_MELEE, int32(val), apply);
                ApplyRatingMod(CR_CRIT_TAKEN_RANGED, int32(val), apply);
                ApplyRatingMod(CR_CRIT_TAKEN_SPELL, int32(val), apply);
                break;
            case ITEM_MOD_HASTE_RATING:
                ApplyRatingMod(CR_HASTE_MELEE, int32(val), apply);
                ApplyRatingMod(CR_HASTE_RANGED, int32(val), apply);
                ApplyRatingMod(CR_HASTE_SPELL, int32(val), apply);
                break;
            case ITEM_MOD_EXPERTISE_RATING:
                ApplyRatingMod(CR_EXPERTISE, int32(val), apply);
                break;
            case ITEM_MOD_ATTACK_POWER:
                HandleAttackPowerModifier(MELEE_AP_MODS, (val > 0) ? AP_MOD_POSITIVE_FLAT : AP_MOD_NEGATIVE_FLAT, float(val), apply);
                HandleAttackPowerModifier(RANGED_AP_MODS, (val > 0) ? AP_MOD_POSITIVE_FLAT : AP_MOD_NEGATIVE_FLAT, float(val), apply);
                break;
            case ITEM_MOD_RANGED_ATTACK_POWER:
                HandleAttackPowerModifier(RANGED_AP_MODS, (val > 0) ? AP_MOD_POSITIVE_FLAT : AP_MOD_NEGATIVE_FLAT, float(val), apply);
                break;
//            case ITEM_MOD_FERAL_ATTACK_POWER:
//                ApplyFeralAPBonus(int32(val), apply);
//                break;
            case ITEM_MOD_MANA_REGENERATION:
                ApplyManaRegenBonus(int32(val), apply);
                break;
            case ITEM_MOD_ARMOR_PENETRATION_RATING:
                ApplyRatingMod(CR_ARMOR_PENETRATION, int32(val), apply);
                break;
            case ITEM_MOD_SPELL_POWER:
                ApplySpellPowerBonus(int32(val), apply);
                break;
            case ITEM_MOD_HEALTH_REGEN:
                ApplyHealthRegenBonus(int32(val), apply);
                break;
            case ITEM_MOD_SPELL_PENETRATION:
                ApplySpellPenetrationBonus(val, apply);
                break;
            case ITEM_MOD_BLOCK_VALUE:
                HandleBaseModFlatValue(SHIELD_BLOCK_VALUE, float(val), apply);
                break;
            // deprecated item mods
            case ITEM_MOD_SPELL_HEALING_DONE:
            case ITEM_MOD_SPELL_DAMAGE_DONE:
                break;
        }
    }

    // Apply Spell Power from ScalingStatValue if set
    if (ssv)
        if (int32 spellbonus = ssv->getSpellBonus(proto->ScalingStatValue))
            ApplySpellPowerBonus(spellbonus, apply);

    /** @epoch-start */
    // // If set ScalingStatValue armor get it or use item armor
    // uint32 armor = proto->Armor;
    // if (ssv)
    // {
    //     if (uint32 ssvarmor = ssv->getArmorMod(proto->ScalingStatValue))
    //         armor = ssvarmor;
    // }
    // else if (armor && proto->ArmorDamageModifier)
    //     armor -= uint32(proto->ArmorDamageModifier);

    // if (armor)
    // {
    //     UnitModifierFlatType modType = TOTAL_VALUE;
    //     if (proto->Class == ITEM_CLASS_ARMOR)
    //     {
    //         switch (proto->SubClass)
    //         {
    //             case ITEM_SUBCLASS_ARMOR_CLOTH:
    //             case ITEM_SUBCLASS_ARMOR_LEATHER:
    //             case ITEM_SUBCLASS_ARMOR_MAIL:
    //             case ITEM_SUBCLASS_ARMOR_PLATE:
    //             case ITEM_SUBCLASS_ARMOR_SHIELD:
    //                 modType = BASE_VALUE;
    //             break;
    //         }
    //     }
    //     HandleStatFlatModifier(UNIT_MOD_ARMOR, modType, float(armor), apply);
    // }

    // // Add armor bonus from ArmorDamageModifier if > 0
    // if (proto->ArmorDamageModifier > 0)
    //     HandleStatFlatModifier(UNIT_MOD_ARMOR, TOTAL_VALUE, float(proto->ArmorDamageModifier), apply);

    uint32 armor = proto->Armor;
    if (armor)
        HandleStatFlatModifier(UNIT_MOD_ARMOR, BASE_VALUE, float(armor), apply);
    /** @epoch-end */

    if (proto->Block)
        HandleBaseModFlatValue(SHIELD_BLOCK_VALUE, float(proto->Block), apply);

    if (proto->HolyRes)
        HandleStatFlatModifier(UNIT_MOD_RESISTANCE_HOLY, BASE_VALUE, float(proto->HolyRes), apply);

    if (proto->FireRes)
        HandleStatFlatModifier(UNIT_MOD_RESISTANCE_FIRE, BASE_VALUE, float(proto->FireRes), apply);

    if (proto->NatureRes)
        HandleStatFlatModifier(UNIT_MOD_RESISTANCE_NATURE, BASE_VALUE, float(proto->NatureRes), apply);

    if (proto->FrostRes)
        HandleStatFlatModifier(UNIT_MOD_RESISTANCE_FROST, BASE_VALUE, float(proto->FrostRes), apply);

    if (proto->ShadowRes)
        HandleStatFlatModifier(UNIT_MOD_RESISTANCE_SHADOW, BASE_VALUE, float(proto->ShadowRes), apply);

    if (proto->ArcaneRes)
        HandleStatFlatModifier(UNIT_MOD_RESISTANCE_ARCANE, BASE_VALUE, float(proto->ArcaneRes), apply);

    WeaponAttackType attType = Player::GetAttackBySlot(slot);
    if (attType != MAX_ATTACK)
        _ApplyWeaponDamage(slot, proto, apply);

    // Druids get feral AP bonus from weapon dps (also use DPS from ScalingStatValue)
    if (GetClass() == CLASS_DRUID)
    {
        int32 dpsMod = 0;
        int32 feral_bonus = 0;
        if (ssv)
        {
            dpsMod = ssv->getDPSMod(proto->ScalingStatValue);
            feral_bonus += ssv->getFeralBonus(proto->ScalingStatValue);
        }

        feral_bonus += proto->getFeralBonus(dpsMod);
        if (feral_bonus)
            ApplyFeralAPBonus(feral_bonus, apply);
    }
}

void Player::_ApplyWeaponDamage(uint8 slot, ItemTemplate const* proto, bool apply)
{
    WeaponAttackType attType = Player::GetAttackBySlot(slot);
    if (!IsInFeralForm() && apply && !CanUseAttackType(attType))
        return;

    ScalingStatValuesEntry const* ssv = GetScalingStatValuesFor(*proto);
    for (uint8 i = 0; i < MAX_ITEM_PROTO_DAMAGES; ++i)
    {
        float minDamage = proto->Damage[i].DamageMin;
        float maxDamage = proto->Damage[i].DamageMax;

        // If set dpsMod in ScalingStatValue use it for min (70% from average), max (130% from average) damage
        if (ssv && i == 0) // scaling stats only for first damage
        {
            int32 extraDPS = ssv->getDPSMod(proto->ScalingStatValue);
            if (extraDPS)
            {
                float average = extraDPS * proto->Delay / 1000.0f;
                float mod = ssv->isTwoHand(proto->ScalingStatValue) ? 0.2f : 0.3f;

                minDamage = (1.0f - mod) * average;
                maxDamage = (1.0f + mod) * average;
            }
        }

        if (apply)
        {
            if (minDamage > 0.f)
                SetBaseWeaponDamage(attType, MINDAMAGE, minDamage, i);

            if (maxDamage > 0.f)
                SetBaseWeaponDamage(attType, MAXDAMAGE, maxDamage, i);
        }
    }

    if (!apply)
    {
        for (uint8 i = 0; i < MAX_ITEM_PROTO_DAMAGES; ++i)
        {
            SetBaseWeaponDamage(attType, MINDAMAGE, 0.f, i);
            SetBaseWeaponDamage(attType, MAXDAMAGE, 0.f, i);
        }

        if (attType == BASE_ATTACK)
        {
            SetBaseWeaponDamage(BASE_ATTACK, MINDAMAGE, BASE_MINDAMAGE);
            SetBaseWeaponDamage(BASE_ATTACK, MAXDAMAGE, BASE_MAXDAMAGE);
        }
    }

    if (proto->Delay && !IsInFeralForm())
        SetAttackTime(attType, apply ? proto->Delay : BASE_ATTACK_TIME);

    // No need to modify any physical damage for ferals as it is calculated from stats only
    if (IsInFeralForm())
        return;

    if (CanModifyStats() && (GetWeaponDamageRange(attType, MAXDAMAGE) || proto->Delay))
        UpdateDamagePhysical(attType);
}

// this one rechecks weapon auras and stores them in BaseModGroup container
// needed for things like axe specialization applying only to axe weapons in case of dual-wield
void Player::UpdateWeaponDependentCritAuras(WeaponAttackType attackType)
{
    BaseModGroup modGroup;
    switch (attackType)
    {
        case BASE_ATTACK:
            modGroup = CRIT_PERCENTAGE;
            break;
        case OFF_ATTACK:
            modGroup = OFFHAND_CRIT_PERCENTAGE;
            break;
        case RANGED_ATTACK:
            modGroup = RANGED_CRIT_PERCENTAGE;
            break;
        default:
            ABORT();
            break;
    }

    float amount = 0.0f;
    amount += GetTotalAuraModifier(SPELL_AURA_MOD_WEAPON_CRIT_PERCENT, std::bind(&Unit::CheckAttackFitToAuraRequirement, this, attackType, std::placeholders::_1));

    // these auras don't have item requirement (only Combat Expertise in 3.3.5a)
    amount += GetTotalAuraModifier(SPELL_AURA_MOD_CRIT_PCT);

    SetBaseModFlatValue(modGroup, amount);
}

void Player::UpdateAllWeaponDependentCritAuras()
{
    for (uint8 i = BASE_ATTACK; i < MAX_ATTACK; ++i)
        UpdateWeaponDependentCritAuras(WeaponAttackType(i));
}

void Player::UpdateWeaponDependentAuras(WeaponAttackType attackType)
{
    UpdateWeaponDependentCritAuras(attackType);
    UpdateDamageDoneMods(attackType);
    UpdateDamagePctDoneMods(attackType);
}

void Player::ApplyItemDependentAuras(Item* item, bool apply)
{
    if (apply)
    {
        PlayerSpellMap const& spells = GetSpellMap();
        for (auto itr = spells.begin(); itr != spells.end(); ++itr)
        {
            if (itr->second.state == PLAYERSPELL_REMOVED || itr->second.disabled)
                continue;

            SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(itr->first);
            if (!spellInfo || !spellInfo->IsPassive() || spellInfo->EquippedItemClass < 0)
                continue;

            if (!HasAura(itr->first) && HasItemFitToSpellRequirements(spellInfo))
                AddAura(itr->first, this);  // no SMSG_SPELL_GO in sniff found
        }
    }
    else
        RemoveItemDependentAurasAndCasts(item);
}

bool Player::CheckAttackFitToAuraRequirement(WeaponAttackType attackType, AuraEffect const* aurEff) const
{
    SpellInfo const* spellInfo = aurEff->GetSpellInfo();
    if (spellInfo->EquippedItemClass == -1)
        return true;

    Item* item = GetWeaponForAttack(attackType, true);
    if (!item || !item->IsFitToSpellRequirements(spellInfo))
        return false;

    return true;
}

void Player::CastItemCombatSpell(DamageInfo const& damageInfo)
{
    Unit* target = damageInfo.GetVictim();
    if (!target || !target->IsAlive() || target == this)
        return;

    for (uint8 i = EQUIPMENT_SLOT_START; i < EQUIPMENT_SLOT_END; ++i)
    {
        // If usable, try to cast item spell
        if (Item* item = GetItemByPos(INVENTORY_SLOT_BAG_0, i))
        {
            if (!item->IsBroken() && CanUseAttackType(damageInfo.GetAttackType()))
            {
                if (ItemTemplate const* proto = item->GetTemplate())
                {
                    // Additional check for weapons
                    if (proto->Class == ITEM_CLASS_WEAPON)
                    {
                        // offhand item cannot proc from main hand hit etc
                        EquipmentSlots slot;
                        switch (damageInfo.GetAttackType())
                        {
                            case BASE_ATTACK:
                                slot = EQUIPMENT_SLOT_MAINHAND;
                                break;
                            case OFF_ATTACK:
                                slot = EQUIPMENT_SLOT_OFFHAND;
                                break;
                            case RANGED_ATTACK:
                                slot = EQUIPMENT_SLOT_RANGED;
                                break;
                            default:
                                slot = EQUIPMENT_SLOT_END;
                                break;
                        }
                        if (slot != i)
                            continue;
                        // Check if item is useable (forms or disarm)
                        if (damageInfo.GetAttackType() == BASE_ATTACK)
                            if (!IsUseEquipedWeapon(true) && !IsInFeralForm())
                                continue;
                    }

                    CastItemCombatSpell(damageInfo, item, proto);
                }
            }
        }
    }
}

void Player::CastItemCombatSpell(DamageInfo const& damageInfo, Item* item, ItemTemplate const* proto)
{
    // Can do effect if any damage done to target
    // for done procs allow normal + critical + absorbs by default
    bool canTrigger = (damageInfo.GetHitMask() & (PROC_HIT_NORMAL | PROC_HIT_CRITICAL | PROC_HIT_ABSORB)) != 0;
    if (canTrigger)
    {
        for (uint8 i = 0; i < MAX_ITEM_PROTO_SPELLS; ++i)
        {
            _Spell const& spellData = proto->Spells[i];

            // no spell
            if (spellData.SpellId <= 0)
                continue;

            // wrong triggering type
            if (spellData.SpellTrigger != ITEM_SPELLTRIGGER_CHANCE_ON_HIT)
                continue;

            SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellData.SpellId);
            if (!spellInfo)
            {
                TC_LOG_ERROR("entities.player.items", "Player::CastItemCombatSpell: Player '{}' ({}) cast unknown item spell (ID: {})",
                    GetName(), GetGUID().ToString(), spellData.SpellId);
                continue;
            }

            float chance = (float)spellInfo->ProcChance;

            if (spellData.SpellPPMRate)
            {
                uint32 WeaponSpeed = GetAttackTime(damageInfo.GetAttackType());
                chance = GetPPMProcChance(WeaponSpeed, spellData.SpellPPMRate, spellInfo);
            }
            else if (chance > 100.0f)
                chance = GetWeaponProcChance();

            if (roll_chance_f(chance) && sScriptMgr->OnCastItemCombatSpell(this, damageInfo.GetVictim(), spellInfo, item))
                CastSpell(damageInfo.GetVictim(), spellInfo->Id, item);
        }
    }

    // item combat enchantments
    for (uint8 e_slot = 0; e_slot < MAX_ENCHANTMENT_SLOT; ++e_slot)
    {
        uint32 enchant_id = item->GetEnchantmentId(EnchantmentSlot(e_slot));
        SpellItemEnchantmentEntry const* pEnchant = sSpellItemEnchantmentStore.LookupEntry(enchant_id);
        if (!pEnchant)
            continue;

        for (uint8 s = 0; s < MAX_ITEM_ENCHANTMENT_EFFECTS; ++s)
        {
            if (pEnchant->Effect[s] != ITEM_ENCHANTMENT_TYPE_COMBAT_SPELL)
                continue;

            SpellEnchantProcEntry const* entry = sSpellMgr->GetSpellEnchantProcEvent(enchant_id);
            if (entry && entry->HitMask)
            {
                // Check hit/crit/dodge/parry requirement
                if ((entry->HitMask & damageInfo.GetHitMask()) == 0)
                    continue;
            }
            else
            {
                // Can do effect if any damage done to target
                // for done procs allow normal + critical + absorbs by default
                if (!canTrigger)
                    continue;
            }

            // check if enchant procs only on white hits
            if (entry && (entry->AttributesMask & ENCHANT_PROC_ATTR_WHITE_HIT) && damageInfo.GetSpellInfo())
                continue;

            SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(pEnchant->EffectArg[s]);
            if (!spellInfo)
            {
                TC_LOG_ERROR("entities.player.items", "Player::CastItemCombatSpell: Player '{}' ({}) cast unknown spell (EnchantID: {}, SpellID: {}), ignoring",
                    GetName(), GetGUID().ToString(), pEnchant->ID, pEnchant->EffectArg[s]);
                continue;
            }

            float chance = pEnchant->EffectPointsMin[s] != 0 ? float(pEnchant->EffectPointsMin[s]) : GetWeaponProcChance();
            if (entry)
            {
                if (entry->ProcsPerMinute)
                    chance = GetPPMProcChance(proto->Delay, entry->ProcsPerMinute, spellInfo);
                else if (entry->Chance)
                    chance = entry->Chance;
            }

            // Apply spell mods
            ApplySpellMod(pEnchant->EffectArg[s], SPELLMOD_CHANCE_OF_SUCCESS, chance);

            // Shiv has 100% chance to apply the poison
            if (FindCurrentSpellBySpellId(5938) && e_slot == TEMP_ENCHANTMENT_SLOT)
                chance = 100.0f;

            // @epoch-start
            FIRE_ID(
                spellInfo->events.id,
                Spell,OnBeforeItemEnchantmentProc,
                TSSpellInfo(spellInfo),
                TSPlayer(this),
                TSItem(item),
                TSDamageInfo(&damageInfo),
                enchant_id,
                e_slot,
                s,
                TSMutableNumber<float>(&chance)
            );
            // @epoch-end

            if (roll_chance_f(chance))
            {
                Unit* target = spellInfo->IsPositive() ? this : damageInfo.GetVictim();

                CastSpellExtraArgs args(item);
                // reduce effect values if enchant is limited
                if (entry && (entry->AttributesMask & ENCHANT_PROC_ATTR_LIMIT_60) && target->GetLevel() > 60)
                {
                    int32 const lvlDifference = target->GetLevel() - 60;
                    int32 const lvlPenaltyFactor = 4; // 4% lost effectiveness per level

                    int32 const effectPct = std::max(0, 100 - (lvlDifference * lvlPenaltyFactor));

                    for (SpellEffectInfo const& spellEffectInfo : spellInfo->GetEffects())
                        if (spellEffectInfo.IsEffect())
                            args.AddSpellMod(static_cast<SpellValueMod>(SPELLVALUE_BASE_POINT0 + AsUnderlyingType(spellEffectInfo.EffectIndex)), CalculatePct(spellEffectInfo.CalcValue(this), effectPct));
                }
                CastSpell(target, spellInfo->Id, args);
            }
        }
    }
}

void Player::CastItemUseSpell(Item* item, SpellCastTargets const& targets, uint8 cast_count, uint32 glyphIndex)
{
    ItemTemplate const* proto = item->GetTemplate();
    // special learning case
    if (proto->Spells[0].SpellId == 483 || proto->Spells[0].SpellId == 55884)
    {
        uint32 learn_spell_id = proto->Spells[0].SpellId;
        uint32 learning_spell_id = proto->Spells[1].SpellId;

        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(learn_spell_id);
        if (!spellInfo)
        {
            TC_LOG_ERROR("entities.player", "Player::CastItemUseSpell: Item (Entry: {}) has wrong spell id {}, ignoring.", proto->ItemId, learn_spell_id);
            SendEquipError(EQUIP_ERR_NONE, item, nullptr);
            return;
        }

        Spell* spell = new Spell(this, spellInfo, TRIGGERED_NONE);
        spell->m_fromClient = true;
        spell->m_CastItem = item;
        spell->m_cast_count = cast_count;                   //set count of casts
        spell->SetSpellValue(SPELLVALUE_BASE_POINT0, learning_spell_id);
        spell->prepare(targets);
        return;
    }

    // item spells cast at use
    for (uint8 i = 0; i < MAX_ITEM_PROTO_SPELLS; ++i)
    {
        _Spell const& spellData = proto->Spells[i];

        // no spell
        if (spellData.SpellId <= 0)
            continue;

        // wrong triggering type
        if (spellData.SpellTrigger != ITEM_SPELLTRIGGER_ON_USE)
            continue;

        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellData.SpellId);
        if (!spellInfo)
        {
            TC_LOG_ERROR("entities.player", "Player::CastItemUseSpell: Item (Entry: {}) has wrong spell id {}, ignoring", proto->ItemId, spellData.SpellId);
            continue;
        }

        Spell* spell = new Spell(this, spellInfo, TRIGGERED_NONE);
        spell->m_fromClient = true;
        spell->m_CastItem = item;
        spell->m_cast_count = cast_count;                   // set count of casts
        spell->m_glyphIndex = glyphIndex;                   // glyph index
        spell->prepare(targets);
        return;
    }

    // Item enchantments spells cast at use
    for (uint8 e_slot = 0; e_slot < MAX_ENCHANTMENT_SLOT; ++e_slot)
    {
        uint32 enchant_id = item->GetEnchantmentId(EnchantmentSlot(e_slot));
        SpellItemEnchantmentEntry const* pEnchant = sSpellItemEnchantmentStore.LookupEntry(enchant_id);
        if (!pEnchant)
            continue;
        for (uint8 s = 0; s < MAX_ITEM_ENCHANTMENT_EFFECTS; ++s)
        {
            if (pEnchant->Effect[s] != ITEM_ENCHANTMENT_TYPE_USE_SPELL)
                continue;

            SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(pEnchant->EffectArg[s]);
            if (!spellInfo)
            {
                TC_LOG_ERROR("entities.player", "Player::CastItemUseSpell: Enchant {}, cast unknown spell {}", pEnchant->ID, pEnchant->EffectArg[s]);
                continue;
            }

            Spell* spell = new Spell(this, spellInfo, TRIGGERED_NONE);
            spell->m_fromClient = true;
            spell->m_CastItem = item;
            spell->m_cast_count = cast_count;               // set count of casts
            spell->m_glyphIndex = glyphIndex;               // glyph index
            spell->prepare(targets);
            return;
        }
    }
}

void Player::_RemoveAllItemMods()
{
    TC_LOG_DEBUG("entities.player.items", "_RemoveAllItemMods start.");

    for (uint8 i = 0; i < INVENTORY_SLOT_BAG_END; ++i)
    {
        if (m_items[i])
        {
            ItemTemplate const* proto = m_items[i]->GetTemplate();
            if (!proto)
                continue;

            // item set bonuses not dependent from item broken state
            if (proto->ItemSet)
                RemoveItemsSetItem(this, proto);

            if (m_items[i]->IsBroken() || !CanUseAttackType(GetAttackBySlot(i)))
                continue;

            ApplyItemEquipSpell(m_items[i], false);
            ApplyEnchantment(m_items[i], false);
        }
    }

    for (uint8 i = 0; i < INVENTORY_SLOT_BAG_END; ++i)
    {
        if (m_items[i])
        {
            if (m_items[i]->IsBroken() || !CanUseAttackType(GetAttackBySlot(i)))
                continue;
            ItemTemplate const* proto = m_items[i]->GetTemplate();
            if (!proto)
                continue;

            ApplyItemDependentAuras(m_items[i], false);
            _ApplyItemBonuses(proto, i, false);

            if (i == EQUIPMENT_SLOT_RANGED)
                _ApplyAmmoBonuses();
        }
    }

    TC_LOG_DEBUG("entities.player.items", "_RemoveAllItemMods complete.");
}

void Player::_ApplyAllItemMods()
{
    TC_LOG_DEBUG("entities.player.items", "_ApplyAllItemMods start.");

    for (uint8 i = 0; i < INVENTORY_SLOT_BAG_END; ++i)
    {
        if (m_items[i])
        {
            if (m_items[i]->IsBroken() || !CanUseAttackType(GetAttackBySlot(i)))
                continue;

            ItemTemplate const* proto = m_items[i]->GetTemplate();
            if (!proto)
                continue;

            ApplyItemDependentAuras(m_items[i], true);
            _ApplyItemBonuses(proto, i, true);

            WeaponAttackType const attackType = Player::GetAttackBySlot(i);
            if (attackType != MAX_ATTACK)
                UpdateWeaponDependentAuras(attackType);

            if (i == EQUIPMENT_SLOT_RANGED)
                _ApplyAmmoBonuses();
        }
    }

    for (uint8 i = 0; i < INVENTORY_SLOT_BAG_END; ++i)
    {
        if (m_items[i])
        {
            ItemTemplate const* proto = m_items[i]->GetTemplate();
            if (!proto)
                continue;

            // item set bonuses not dependent from item broken state
            if (proto->ItemSet)
                AddItemsSetItem(this, m_items[i]);

            if (m_items[i]->IsBroken() || !CanUseAttackType(GetAttackBySlot(i)))
                continue;

            ApplyItemEquipSpell(m_items[i], true);
            ApplyEnchantment(m_items[i], true);
        }
    }

    TC_LOG_DEBUG("entities.player.items", "_ApplyAllItemMods complete.");
}

void Player::_ApplyAllLevelScaleItemMods(bool apply)
{
    for (uint8 i = 0; i < INVENTORY_SLOT_BAG_END; ++i)
    {
        if (m_items[i])
        {
            if (m_items[i]->IsBroken() || !CanUseAttackType(GetAttackBySlot(i)))
                continue;

            ItemTemplate const* proto = m_items[i]->GetTemplate();
            if (!proto)
                continue;

            _ApplyItemBonuses(proto, i, apply, true);
        }
    }
}

void Player::_ApplyAmmoBonuses()
{
    // check ammo
    uint32 ammo_id = GetUInt32Value(PLAYER_AMMO_ID);
    if (!ammo_id)
        return;

    float currentAmmoDPS;

    ItemTemplate const* ammo_proto = sObjectMgr->GetItemTemplate(ammo_id);
    if (!ammo_proto || ammo_proto->Class != ITEM_CLASS_PROJECTILE || !CheckAmmoCompatibility(ammo_proto))
        currentAmmoDPS = 0.0f;
    else
        currentAmmoDPS = (ammo_proto->Damage[0].DamageMin + ammo_proto->Damage[0].DamageMax) / 2;

    if (currentAmmoDPS == GetAmmoDPS())
        return;

    m_ammoDPS = currentAmmoDPS;

    if (CanModifyStats())
        UpdateDamagePhysical(RANGED_ATTACK);
}

/*  If in a battleground a player dies, and an enemy removes the insignia, the player's bones is lootable
    Called by remove insignia spell effect    */
void Player::RemovedInsignia(Player* looterPlr)
{
    // If player is not in battleground and not in worldpvpzone
    if (!GetBattlegroundId() && !IsInWorldPvpZone())
        return;

    // If not released spirit, do it !
    if (m_deathTimer > 0)
    {
        m_deathTimer = 0;
        BuildPlayerRepop();
        RepopAtGraveyard();
    }

    _corpseLocation.WorldRelocate();

    // We have to convert player corpse to bones, not to be able to resurrect there
    // SpawnCorpseBones isn't handy, 'cos it saves player while he in BG
    Corpse* bones = GetMap()->ConvertCorpseToBones(GetGUID(), true);
    if (!bones)
        return;

    // Now we must make bones lootable, and send player loot
    bones->SetFlag(CORPSE_FIELD_DYNAMIC_FLAGS, CORPSE_DYNFLAG_LOOTABLE);

    // We store the level of our player in the gold field
    // We retrieve this information at Player::SendLoot()
    bones->loot.gold = GetLevel();
    bones->lootRecipient = looterPlr;
    looterPlr->SendLoot(bones->GetGUID(), LOOT_INSIGNIA);
}

void Player::SendUpdateWorldState(uint32 variable, uint32 value) const
{
    WorldPackets::WorldState::UpdateWorldState worldstate;
    worldstate.VariableID = variable;
    worldstate.Value = value;
    SendDirectMessage(worldstate.Write());
}

// TODO - InitWorldStates should NOT always send the same states
//        Some should keep the same value between different zoneIds and areaIds on the same map
void Player::SendInitWorldStates(uint32 zoneId, uint32 areaId)
{
    uint32 mapId = GetMapId();
    Battleground* battleground = GetBattleground();
    OutdoorPvP* outdoorPvP = sOutdoorPvPMgr->GetOutdoorPvPToZoneId(zoneId);
    InstanceScript* instance = GetInstanceScript();
    Battlefield* battlefield = sBattlefieldMgr->GetBattlefieldToZoneId(zoneId);

    TC_LOG_DEBUG("network", "Player::SendInitWorldStates: Sending SMSG_INIT_WORLD_STATES for Map: {}, Zone: {}", mapId, zoneId);

    WorldPackets::WorldState::InitWorldStates packet;
    packet.MapID = mapId;
    packet.ZoneID = zoneId;
    packet.AreaID = areaId;

    packet.Worldstates.emplace_back(2264, 0); // SCOURGE_EVENT_WORLDSTATE_EASTERN_PLAGUELANDS
    packet.Worldstates.emplace_back(2263, 0); // SCOURGE_EVENT_WORLDSTATE_TANARIS
    packet.Worldstates.emplace_back(2262, 0); // SCOURGE_EVENT_WORLDSTATE_BURNING_STEPPES
    packet.Worldstates.emplace_back(2261, 0); // SCOURGE_EVENT_WORLDSTATE_BLASTED_LANDS
    packet.Worldstates.emplace_back(2260, 0); // SCOURGE_EVENT_WORLDSTATE_AZSHARA
    packet.Worldstates.emplace_back(2259, 0); // SCOURGE_EVENT_WORLDSTATE_WINTERSPRING

    // ARENA_SEASON_IN_PROGRESS
    //   7 - arena season in progress
    //   0 - end of season
    packet.Worldstates.emplace_back(3191, sWorld->getBoolConfig(CONFIG_ARENA_SEASON_IN_PROGRESS) ? sWorld->getIntConfig(CONFIG_ARENA_SEASON_ID) : 0);

    // Previous arena season id
    int32 previousArenaSeason = 0;
    if (sWorld->getBoolConfig(CONFIG_ARENA_SEASON_IN_PROGRESS) && sWorld->getIntConfig(CONFIG_ARENA_SEASON_ID) > 0)
        previousArenaSeason = sWorld->getIntConfig(CONFIG_ARENA_SEASON_ID) - 1;
    packet.Worldstates.emplace_back(3901, previousArenaSeason);

    if (mapId == 530) // Outland
    {
        packet.Worldstates.emplace_back(2495, 0);  // NA_UI_OUTLAND_01 "Progress: %2494w"
        packet.Worldstates.emplace_back(2493, 15); // NA_UI_GUARDS_MAX
        packet.Worldstates.emplace_back(2491, 15); // NA_UI_GUARDS_LEFT
    }

    switch (zoneId)
    {
        case 1: // Dun Morogh
        case 11: // Wetlands
        case 12: // Elwynn Forest
        case 38: // Loch Modan
        case 40: // Westfall
        case 51: // Searing Gorge
        case 1519: // Stormwind City
        case 1537: // Ironforge
        case 2257: // Deeprun Tram
        case 3703: // Shattrath City
            break;
        case 139: // Eastern Plaguelands
            if (outdoorPvP && outdoorPvP->GetTypeId() == OUTDOOR_PVP_EP)
                outdoorPvP->FillInitialWorldStates(packet);
            else
            {
                packet.Worldstates.emplace_back(2426, 0);  // GENERAL_WORLDSTATES_01 "Progress: %2427w"
                packet.Worldstates.emplace_back(2327, 0);  // EP_UI_TOWER_COUNT_A
                packet.Worldstates.emplace_back(2328, 0);  // EP_UI_TOWER_COUNT_H
                packet.Worldstates.emplace_back(2427, 50); // GENERAL_WORLDSTATES_02
                packet.Worldstates.emplace_back(2428, 50); // GENERAL_WORLDSTATES_03
                packet.Worldstates.emplace_back(2355, 1);  // EP_CGT_N
                packet.Worldstates.emplace_back(2374, 0);  // EP_CGT_N_A
                packet.Worldstates.emplace_back(2375, 0);  // EP_CGT_N_H
                packet.Worldstates.emplace_back(2376, 0);  // GENERAL_WORLDSTATES_04
                packet.Worldstates.emplace_back(2377, 0);  // GENERAL_WORLDSTATES_05
                packet.Worldstates.emplace_back(2378, 0);  // EP_CGT_A
                packet.Worldstates.emplace_back(2379, 0);  // EP_CGT_H
                packet.Worldstates.emplace_back(2354, 0);  // EP_EWT_A
                packet.Worldstates.emplace_back(2356, 0);  // EP_EWT_H
                packet.Worldstates.emplace_back(2357, 0);  // GENERAL_WORLDSTATES_06
                packet.Worldstates.emplace_back(2358, 0);  // GENERAL_WORLDSTATES_07
                packet.Worldstates.emplace_back(2359, 0);  // EP_EWT_N_A
                packet.Worldstates.emplace_back(2360, 0);  // EP_EWT_N_H
                packet.Worldstates.emplace_back(2361, 1);  // EP_EWT_N
                packet.Worldstates.emplace_back(2352, 1);  // EP_NPT_N
                packet.Worldstates.emplace_back(2362, 0);  // EP_NPT_N_A
                packet.Worldstates.emplace_back(2363, 0);  // GENERAL_WORLDSTATES_08
                packet.Worldstates.emplace_back(2364, 0);  // GENERAL_WORLDSTATES_09
                packet.Worldstates.emplace_back(2365, 0);  // GENERAL_WORLDSTATES_10
                packet.Worldstates.emplace_back(2372, 0);  // EP_NPT_A
                packet.Worldstates.emplace_back(2373, 0);  // EP_NPT_H
                packet.Worldstates.emplace_back(2353, 1);  // EP_PWT_N
                packet.Worldstates.emplace_back(2366, 0);  // EP_PWT_N_A
                //packet.Worldstates.emplace_back(2367, 1); // GENERAL_WORLDSTATES_13 grey horde not in dbc!
                packet.Worldstates.emplace_back(2368, 0);  // GENERAL_WORLDSTATES_11
                packet.Worldstates.emplace_back(2369, 0);  // GENERAL_WORLDSTATES_12
                packet.Worldstates.emplace_back(2370, 0);  // EP_PWT_A
                packet.Worldstates.emplace_back(2371, 0);  // EP_PWT_H
            }
            break;
        case 1377: // Silithus
            if (outdoorPvP && outdoorPvP->GetTypeId() == OUTDOOR_PVP_SI)
                outdoorPvP->FillInitialWorldStates(packet);
            else
            {
                packet.Worldstates.emplace_back(2313, 0); // SI_GATHERED_A
                packet.Worldstates.emplace_back(2314, 0); // SI_GATHERED_H
                packet.Worldstates.emplace_back(2317, 0); // SI_SILITHYST_MAX
            }
            // unknown, aq opening?
            packet.Worldstates.emplace_back(2322, 0); // AQ_SANDWORM_N
            packet.Worldstates.emplace_back(2323, 0); // AQ_SANDWORM_S
            packet.Worldstates.emplace_back(2324, 0); // AQ_SANDWORM_SW
            packet.Worldstates.emplace_back(2325, 0); // AQ_SANDWORM_E
            break;
        case 2597: // Alterac Valley
            if (battleground && battleground->GetTypeID(true) == BATTLEGROUND_AV)
                battleground->FillInitialWorldStates(packet);
            else
            {
                packet.Worldstates.emplace_back(1966, 1); // AV_SNOWFALL_N
                packet.Worldstates.emplace_back(1330, 1); // AV_FROSTWOLFHUT_H_C
                packet.Worldstates.emplace_back(1329, 0); // AV_FROSTWOLFHUT_A_C
                packet.Worldstates.emplace_back(1326, 0); // AV_AID_A_A
                packet.Worldstates.emplace_back(1393, 0); // East Frostwolf Tower Horde Assaulted - UNUSED
                packet.Worldstates.emplace_back(1392, 0); // West Frostwolf Tower Horde Assaulted - UNUSED
                packet.Worldstates.emplace_back(1383, 1); // AV_FROSTWOLFE_CONTROLLED
                packet.Worldstates.emplace_back(1382, 1); // AV_FROSTWOLFW_CONTROLLED
                packet.Worldstates.emplace_back(1360, 1); // AV_N_MINE_N
                packet.Worldstates.emplace_back(1348, 0); // AV_ICEBLOOD_A_A
                packet.Worldstates.emplace_back(1334, 0); // AV_PIKEGRAVE_H_C
                packet.Worldstates.emplace_back(1333, 1); // AV_PIKEGRAVE_A_C
                packet.Worldstates.emplace_back(1304, 0); // AV_STONEHEART_A_A
                packet.Worldstates.emplace_back(1303, 0); // AV_STONEHEART_H_A
                packet.Worldstates.emplace_back(1396, 0); // unk
                packet.Worldstates.emplace_back(1395, 0); // Iceblood Tower Horde Assaulted - UNUSED
                packet.Worldstates.emplace_back(1394, 0); // Towerpoint Horde Assaulted - UNUSED
                packet.Worldstates.emplace_back(1391, 0); // unk
                packet.Worldstates.emplace_back(1390, 0); // AV_ICEBLOOD_ASSAULTED
                packet.Worldstates.emplace_back(1389, 0); // AV_TOWERPOINT_ASSAULTED
                packet.Worldstates.emplace_back(1388, 0); // AV_FROSTWOLFE_ASSAULTED
                packet.Worldstates.emplace_back(1387, 0); // AV_FROSTWOLFW_ASSAULTED
                packet.Worldstates.emplace_back(1386, 1); // unk
                packet.Worldstates.emplace_back(1385, 1); // AV_ICEBLOOD_CONTROLLED
                packet.Worldstates.emplace_back(1384, 1); // AV_TOWERPOINT_CONTROLLED
                packet.Worldstates.emplace_back(1381, 0); // AV_STONEH_ASSAULTED
                packet.Worldstates.emplace_back(1380, 0); // AV_ICEWING_ASSAULTED
                packet.Worldstates.emplace_back(1379, 0); // AV_DUNN_ASSAULTED
                packet.Worldstates.emplace_back(1378, 0); // AV_DUNS_ASSAULTED
                packet.Worldstates.emplace_back(1377, 0); // Stoneheart Bunker Alliance Assaulted - UNUSED
                packet.Worldstates.emplace_back(1376, 0); // Icewing Bunker Alliance Assaulted - UNUSED
                packet.Worldstates.emplace_back(1375, 0); // Dunbaldar South Alliance Assaulted - UNUSED
                packet.Worldstates.emplace_back(1374, 0); // Dunbaldar North Alliance Assaulted - UNUSED
                packet.Worldstates.emplace_back(1373, 0); // AV_STONEH_DESTROYED
                packet.Worldstates.emplace_back(966, 0);  // AV_UNK_02
                packet.Worldstates.emplace_back(964, 0);  // AV_UNK_01
                packet.Worldstates.emplace_back(962, 0);  // AV_STORMPIKE_COMMANDERS
                packet.Worldstates.emplace_back(1302, 1); // AV_STONEHEART_A_C
                packet.Worldstates.emplace_back(1301, 0); // AV_STONEHEART_H_C
                packet.Worldstates.emplace_back(950, 0);  // AV_STORMPIKE_LIEUTENANTS
                packet.Worldstates.emplace_back(1372, 0); // AV_ICEWING_DESTROYED
                packet.Worldstates.emplace_back(1371, 0); // AV_DUNN_DESTROYED
                packet.Worldstates.emplace_back(1370, 0); // AV_DUNS_DESTROYED
                packet.Worldstates.emplace_back(1369, 0); // unk
                packet.Worldstates.emplace_back(1368, 0); // AV_ICEBLOOD_DESTROYED
                packet.Worldstates.emplace_back(1367, 0); // AV_TOWERPOINT_DESTROYED
                packet.Worldstates.emplace_back(1366, 0); // AV_FROSTWOLFE_DESTROYED
                packet.Worldstates.emplace_back(1365, 0); // AV_FROSTWOLFW_DESTROYED
                packet.Worldstates.emplace_back(1364, 1); // AV_STONEH_CONTROLLED
                packet.Worldstates.emplace_back(1363, 1); // AV_ICEWING_CONTROLLED
                packet.Worldstates.emplace_back(1362, 1); // AV_DUNN_CONTROLLED
                packet.Worldstates.emplace_back(1361, 1); // AV_DUNS_CONTROLLED
                packet.Worldstates.emplace_back(1359, 0); // AV_N_MINE_H
                packet.Worldstates.emplace_back(1358, 0); // AV_N_MINE_A
                packet.Worldstates.emplace_back(1357, 1); // AV_S_MINE_N
                packet.Worldstates.emplace_back(1356, 0); // AV_S_MINE_H
                packet.Worldstates.emplace_back(1355, 0); // AV_S_MINE_A
                packet.Worldstates.emplace_back(1349, 0); // AV_ICEBLOOD_H_A
                packet.Worldstates.emplace_back(1347, 1); // AV_ICEBLOOD_H_C
                packet.Worldstates.emplace_back(1346, 0); // AV_ICEBLOOD_A_C
                packet.Worldstates.emplace_back(1344, 0); // AV_SNOWFALL_H_A
                packet.Worldstates.emplace_back(1343, 0); // AV_SNOWFALL_A_A
                packet.Worldstates.emplace_back(1342, 0); // AV_SNOWFALL_H_C
                packet.Worldstates.emplace_back(1341, 0); // AV_SNOWFALL_A_C
                packet.Worldstates.emplace_back(1340, 0); // AV_FROSTWOLF_H_A
                packet.Worldstates.emplace_back(1339, 0); // AV_FROSTWOLF_A_A
                packet.Worldstates.emplace_back(1338, 1); // AV_FROSTWOLF_H_C
                packet.Worldstates.emplace_back(1337, 0); // AV_FROSTWOLF_A_C
                packet.Worldstates.emplace_back(1336, 0); // AV_PIKEGRAVE_H_A
                packet.Worldstates.emplace_back(1335, 0); // AV_PIKEGRAVE_A_A
                packet.Worldstates.emplace_back(1332, 0); // AV_FROSTWOLFHUT_H_A
                packet.Worldstates.emplace_back(1331, 0); // AV_FROSTWOLFHUT_A_A
                packet.Worldstates.emplace_back(1328, 0); // AV_AID_H_A
                packet.Worldstates.emplace_back(1327, 0); // AV_AID_H_C
                packet.Worldstates.emplace_back(1325, 1); // AV_AID_A_C
            }
            break;
        case 3277: // Warsong Gulch
            if (battleground && battleground->GetTypeID(true) == BATTLEGROUND_WS)
                battleground->FillInitialWorldStates(packet);
            else
            {
                packet.Worldstates.emplace_back(1581, 0); // alliance flag captures
                packet.Worldstates.emplace_back(1582, 0); // horde flag captures
                packet.Worldstates.emplace_back(1545, 0); // unk, set to 1 on alliance flag pickup...
                packet.Worldstates.emplace_back(1546, 0); // unk, set to 1 on horde flag pickup, after drop it's -1
                packet.Worldstates.emplace_back(1547, 2); // unk
                packet.Worldstates.emplace_back(1601, 3); // unk (max flag captures?)
                packet.Worldstates.emplace_back(2338, 1); // horde (0 - hide, 1 - flag ok, 2 - flag picked up (flashing), 3 - flag picked up (not flashing)
                packet.Worldstates.emplace_back(2339, 1); // alliance (0 - hide, 1 - flag ok, 2 - flag picked up (flashing), 3 - flag picked up (not flashing)
            }
            break;
        case 3358: // Arathi Basin
            if (battleground && battleground->GetTypeID(true) == BATTLEGROUND_AB)
                battleground->FillInitialWorldStates(packet);
            else
            {
                packet.Worldstates.emplace_back(1767, 0);    // stables alliance
                packet.Worldstates.emplace_back(1768, 0);    // stables horde
                packet.Worldstates.emplace_back(1769, 0);    // stables alliance controlled
                packet.Worldstates.emplace_back(1770, 0);    // stables horde controlled
                packet.Worldstates.emplace_back(1772, 0);    // farm alliance
                packet.Worldstates.emplace_back(1773, 0);    // farm horde
                packet.Worldstates.emplace_back(1774, 0);    // farm alliance controlled
                packet.Worldstates.emplace_back(1775, 0);    // farm horde controlled
                packet.Worldstates.emplace_back(1776, 0);    // alliance resources
                packet.Worldstates.emplace_back(1777, 0);    // horde resources
                packet.Worldstates.emplace_back(1778, 0);    // horde bases
                packet.Worldstates.emplace_back(1779, 0);    // alliance bases
                packet.Worldstates.emplace_back(1780, 2000); // max resources (2000)
                packet.Worldstates.emplace_back(1782, 0);    // blacksmith alliance
                packet.Worldstates.emplace_back(1783, 0);    // blacksmith horde
                packet.Worldstates.emplace_back(1784, 0);    // blacksmith alliance controlled
                packet.Worldstates.emplace_back(1785, 0);    // blacksmith horde controlled
                packet.Worldstates.emplace_back(1787, 0);    // gold mine alliance
                packet.Worldstates.emplace_back(1788, 0);    // gold mine horde
                packet.Worldstates.emplace_back(1789, 0);    // gold mine alliance controlled
                packet.Worldstates.emplace_back(1790, 0);    // gold mine horde controlled
                packet.Worldstates.emplace_back(1792, 0);    // lumber mill alliance
                packet.Worldstates.emplace_back(1793, 0);    // lumber mill horde
                packet.Worldstates.emplace_back(1794, 0);    // lumber mill alliance controlled
                packet.Worldstates.emplace_back(1795, 0);    // lumber mill horde controlled
                packet.Worldstates.emplace_back(1842, 1);    // stables (1 - uncontrolled)
                packet.Worldstates.emplace_back(1843, 1);    // gold mine (1 - uncontrolled)
                packet.Worldstates.emplace_back(1844, 1);    // lumber mill (1 - uncontrolled)
                packet.Worldstates.emplace_back(1845, 1);    // farm (1 - uncontrolled)
                packet.Worldstates.emplace_back(1846, 1);    // blacksmith (1 - uncontrolled)
                packet.Worldstates.emplace_back(1861, 2);    // unk
                packet.Worldstates.emplace_back(1955, 1800); // warning limit (1800)
            }
            break;
        case 3820: // Eye of the Storm
            if (battleground && battleground->GetTypeID(true) == BATTLEGROUND_EY)
                battleground->FillInitialWorldStates(packet);
            else
            {
                packet.Worldstates.emplace_back(2753, 0);   // Horde Bases
                packet.Worldstates.emplace_back(2752, 0);   // Alliance Bases
                packet.Worldstates.emplace_back(2742, 0);   // Mage Tower - Horde conflict
                packet.Worldstates.emplace_back(2741, 0);   // Mage Tower - Alliance conflict
                packet.Worldstates.emplace_back(2740, 0);   // Fel Reaver - Horde conflict
                packet.Worldstates.emplace_back(2739, 0);   // Fel Reaver - Alliance conflict
                packet.Worldstates.emplace_back(2738, 0);   // Draenei - Alliance conflict
                packet.Worldstates.emplace_back(2737, 0);   // Draenei - Horde conflict
                packet.Worldstates.emplace_back(2736, 0);   // unk (0 at start)
                packet.Worldstates.emplace_back(2735, 0);   // unk (0 at start)
                packet.Worldstates.emplace_back(2733, 0);   // Draenei - Horde control
                packet.Worldstates.emplace_back(2732, 0);   // Draenei - Alliance control
                packet.Worldstates.emplace_back(2731, 1);   // Draenei uncontrolled (1 - yes, 0 - no)
                packet.Worldstates.emplace_back(2730, 0);   // Mage Tower - Alliance control
                packet.Worldstates.emplace_back(2729, 0);   // Mage Tower - Horde control
                packet.Worldstates.emplace_back(2728, 1);   // Mage Tower uncontrolled (1 - yes, 0 - no)
                packet.Worldstates.emplace_back(2727, 0);   // Fel Reaver - Horde control
                packet.Worldstates.emplace_back(2726, 0);   // Fel Reaver - Alliance control
                packet.Worldstates.emplace_back(2725, 1);   // Fel Reaver uncontrolled (1 - yes, 0 - no)
                packet.Worldstates.emplace_back(2724, 0);   // Boold Elf - Horde control
                packet.Worldstates.emplace_back(2723, 0);   // Boold Elf - Alliance control
                packet.Worldstates.emplace_back(2722, 1);   // Boold Elf uncontrolled (1 - yes, 0 - no)
                packet.Worldstates.emplace_back(2757, 1);   // Flag (1 - show, 0 - hide) - doesn't work exactly this way!
                packet.Worldstates.emplace_back(2770, 1);   // Horde top-stats (1 - show, 0 - hide) // 02 -> horde picked up the flag
                packet.Worldstates.emplace_back(2769, 1);   // Alliance top-stats (1 - show, 0 - hide) // 02 -> alliance picked up the flag
                packet.Worldstates.emplace_back(2750, 0);   // Horde resources
                packet.Worldstates.emplace_back(2749, 0);   // Alliance resources
                packet.Worldstates.emplace_back(2565, 142); // unk, constant?
                packet.Worldstates.emplace_back(2720, 0);   // Capturing progress-bar (100 -> empty (only grey), 0 -> blue|red (no grey), default 0)
                packet.Worldstates.emplace_back(2719, 0);   // Capturing progress-bar (0 - left, 100 - right)
                packet.Worldstates.emplace_back(2718, 0);   // Capturing progress-bar (1 - show, 0 - hide)
                packet.Worldstates.emplace_back(3085, 379); // unk, constant?
                // missing unknowns
            }
            break;
        case 3483: // Hellfire Peninsula
            if (outdoorPvP && outdoorPvP->GetTypeId() == OUTDOOR_PVP_HP)
                outdoorPvP->FillInitialWorldStates(packet);
            else
            {
                packet.Worldstates.emplace_back(2490, 1);   // add ally tower main gui icon
                packet.Worldstates.emplace_back(2489, 1);   // add horde tower main gui icon
                packet.Worldstates.emplace_back(2485, 0);   // show neutral broken hill icon
                packet.Worldstates.emplace_back(2484, 1);   // show icon above broken hill
                packet.Worldstates.emplace_back(2483, 0);   // show ally broken hill icon
                packet.Worldstates.emplace_back(2482, 0);   // show neutral overlook icon
                packet.Worldstates.emplace_back(2481, 1);   // show the overlook arrow
                packet.Worldstates.emplace_back(2480, 0);   // show ally overlook icon
                packet.Worldstates.emplace_back(2478, 0);   // horde pvp objectives captured
                packet.Worldstates.emplace_back(2476, 0);   // ally pvp objectives captured
                packet.Worldstates.emplace_back(2475, 100); // horde slider grey area
                packet.Worldstates.emplace_back(2474, 50);  // horde slider percentage, 100 for ally, 0 for horde
                packet.Worldstates.emplace_back(2473, 0);   // horde slider display
                packet.Worldstates.emplace_back(2472, 0);   // show the neutral stadium icon
                packet.Worldstates.emplace_back(2471, 0);   // show the ally stadium icon
                packet.Worldstates.emplace_back(2470, 1);   // show the horde stadium icon
            }
            break;
        case 3518: // Nagrand
            if (outdoorPvP && outdoorPvP->GetTypeId() == OUTDOOR_PVP_NA)
                outdoorPvP->FillInitialWorldStates(packet);
            else
            {
               packet.Worldstates.emplace_back(2503, 0); // NA_UI_HORDE_GUARDS_SHOW
               packet.Worldstates.emplace_back(2502, 0); // NA_UI_ALLIANCE_GUARDS_SHOW
               packet.Worldstates.emplace_back(2493, 0); // NA_UI_GUARDS_MAX
               packet.Worldstates.emplace_back(2491, 0); // NA_UI_GUARDS_LEFT
               packet.Worldstates.emplace_back(2495, 0); // NA_UI_OUTLAND_01
               packet.Worldstates.emplace_back(2494, 0); // NA_UI_UNK_1
               packet.Worldstates.emplace_back(2497, 0); // NA_UI_UNK_2
               packet.Worldstates.emplace_back(2762, 0); // NA_MAP_WYVERN_NORTH_NEU_H
               packet.Worldstates.emplace_back(2662, 0); // NA_MAP_WYVERN_NORTH_NEU_A
               packet.Worldstates.emplace_back(2663, 0); // NA_MAP_WYVERN_NORTH_H
               packet.Worldstates.emplace_back(2664, 0); // NA_MAP_WYVERN_NORTH_A
               packet.Worldstates.emplace_back(2760, 0); // NA_MAP_WYVERN_SOUTH_NEU_H
               packet.Worldstates.emplace_back(2670, 0); // NA_MAP_WYVERN_SOUTH_NEU_A
               packet.Worldstates.emplace_back(2668, 0); // NA_MAP_WYVERN_SOUTH_H
               packet.Worldstates.emplace_back(2669, 0); // NA_MAP_WYVERN_SOUTH_A
               packet.Worldstates.emplace_back(2761, 0); // NA_MAP_WYVERN_WEST_NEU_H
               packet.Worldstates.emplace_back(2667, 0); // NA_MAP_WYVERN_WEST_NEU_A
               packet.Worldstates.emplace_back(2665, 0); // NA_MAP_WYVERN_WEST_H
               packet.Worldstates.emplace_back(2666, 0); // NA_MAP_WYVERN_WEST_A
               packet.Worldstates.emplace_back(2763, 0); // NA_MAP_WYVERN_EAST_NEU_H
               packet.Worldstates.emplace_back(2659, 0); // NA_MAP_WYVERN_EAST_NEU_A
               packet.Worldstates.emplace_back(2660, 0); // NA_MAP_WYVERN_EAST_H
               packet.Worldstates.emplace_back(2661, 0); // NA_MAP_WYVERN_EAST_A
               packet.Worldstates.emplace_back(2671, 0); // NA_MAP_HALAA_NEUTRAL
               packet.Worldstates.emplace_back(2676, 0); // NA_MAP_HALAA_NEU_A
               packet.Worldstates.emplace_back(2677, 0); // NA_MAP_HALAA_NEU_H
               packet.Worldstates.emplace_back(2672, 0); // NA_MAP_HALAA_HORDE
               packet.Worldstates.emplace_back(2673, 0); // NA_MAP_HALAA_ALLIANCE
            }
            break;
        case 3519: // Terokkar Forest
            if (outdoorPvP && outdoorPvP->GetTypeId() == OUTDOOR_PVP_TF)
                outdoorPvP->FillInitialWorldStates(packet);
            else
            {
                packet.Worldstates.emplace_back(2625, 0);  // TF_UI_CAPTURE_BAR_POS
                packet.Worldstates.emplace_back(2624, 20); // TF_UI_CAPTURE_BAR_NEUTRAL
                packet.Worldstates.emplace_back(2623, 0);  // TF_UI_SHOW CAPTURE BAR
                packet.Worldstates.emplace_back(2622, 0);  // TF_UI_TOWER_COUNT_H
                packet.Worldstates.emplace_back(2621, 5);  // TF_UI_TOWER_COUNT_A
                packet.Worldstates.emplace_back(2620, 0);  // TF_UI_TOWERS_CONTROLLED_DISPLAY
                packet.Worldstates.emplace_back(2696, 0);  // TF_TOWER_NUM_15 - SE Neutral
                packet.Worldstates.emplace_back(2695, 0);  // TF_TOWER_NUM_14 - SE Horde
                packet.Worldstates.emplace_back(2694, 0);  // TF_TOWER_NUM_13 - SE Alliance
                packet.Worldstates.emplace_back(2693, 0);  // TF_TOWER_NUM_12 - S Neutral
                packet.Worldstates.emplace_back(2692, 0);  // TF_TOWER_NUM_11 - S Horde
                packet.Worldstates.emplace_back(2691, 0);  // TF_TOWER_NUM_10 - S Alliance
                packet.Worldstates.emplace_back(2690, 0);  // TF_TOWER_NUM_09 - NE Neutral
                packet.Worldstates.emplace_back(2689, 0);  // TF_TOWER_NUM_08 - NE Horde
                packet.Worldstates.emplace_back(2688, 0);  // TF_TOWER_NUM_07 - NE Alliance
                packet.Worldstates.emplace_back(2687, 0);  // TF_TOWER_NUM_16 - unk
                packet.Worldstates.emplace_back(2686, 0);  // TF_TOWER_NUM_06 - N Neutral
                packet.Worldstates.emplace_back(2685, 0);  // TF_TOWER_NUM_05 - N Horde
                packet.Worldstates.emplace_back(2684, 0);  // TF_TOWER_NUM_04 - N Alliance
                packet.Worldstates.emplace_back(2683, 0);  // TF_TOWER_NUM_03 - NW Alliance
                packet.Worldstates.emplace_back(2682, 0);  // TF_TOWER_NUM_02 - NW Horde
                packet.Worldstates.emplace_back(2681, 0);  // TF_TOWER_NUM_01 - NW Neutral
                packet.Worldstates.emplace_back(2512, 5);  // TF_UI_LOCKED_TIME_MINUTES_FIRST_DIGIT
                packet.Worldstates.emplace_back(2510, 0);  // TF_UI_LOCKED_TIME_MINUTES_SECOND_DIGIT
                packet.Worldstates.emplace_back(2509, 0);  // TF_UI_LOCKED_TIME_HOURS
                packet.Worldstates.emplace_back(2508, 0);  // TF_UI_LOCKED_DISPLAY_NEUTRAL
                packet.Worldstates.emplace_back(2768, 0);  // TF_UI_LOCKED_DISPLAY_HORDE
                packet.Worldstates.emplace_back(2767, 1);  // TF_UI_LOCKED_DISPLAY_ALLIANCE
            }
            break;
        case 3521: // Zangarmarsh
            if (outdoorPvP && outdoorPvP->GetTypeId() == OUTDOOR_PVP_ZM)
                outdoorPvP->FillInitialWorldStates(packet);
            else
            {
                packet.Worldstates.emplace_back(2529, 0); // ZM_UNK_1
                packet.Worldstates.emplace_back(2528, 0); // ZM_UNK_2
                packet.Worldstates.emplace_back(2527, 0); // ZM_UNK_3
                packet.Worldstates.emplace_back(2653, 1); // ZM_WORLDSTATE_UNK_1
                packet.Worldstates.emplace_back(2652, 0); // ZM_MAP_TOWER_EAST_N
                packet.Worldstates.emplace_back(2651, 1); // ZM_MAP_TOWER_EAST_H
                packet.Worldstates.emplace_back(2650, 0); // ZM_MAP_TOWER_EAST_A
                packet.Worldstates.emplace_back(2649, 1); // ZM_MAP_GRAVEYARD_H - Twin spire graveyard horde
                packet.Worldstates.emplace_back(2648, 0); // ZM_MAP_GRAVEYARD_A
                packet.Worldstates.emplace_back(2647, 0); // ZM_MAP_GRAVEYARD_N
                packet.Worldstates.emplace_back(2646, 0); // ZM_MAP_TOWER_WEST_N
                packet.Worldstates.emplace_back(2645, 1); // ZM_MAP_TOWER_WEST_H
                packet.Worldstates.emplace_back(2644, 0); // ZM_MAP_TOWER_WEST_A
                packet.Worldstates.emplace_back(2535, 0); // ZM_UNK_4
                packet.Worldstates.emplace_back(2534, 0); // ZM_UNK_5
                packet.Worldstates.emplace_back(2533, 0); // ZM_UNK_6
                packet.Worldstates.emplace_back(2560, 0); // ZM_UI_TOWER_EAST_N
                packet.Worldstates.emplace_back(2559, 1); // ZM_UI_TOWER_EAST_H
                packet.Worldstates.emplace_back(2558, 0); // ZM_UI_TOWER_EAST_A
                packet.Worldstates.emplace_back(2557, 0); // ZM_UI_TOWER_WEST_N
                packet.Worldstates.emplace_back(2556, 1); // ZM_UI_TOWER_WEST_H
                packet.Worldstates.emplace_back(2555, 0); // ZM_UI_TOWER_WEST_A
                packet.Worldstates.emplace_back(2658, 0); // ZM_MAP_HORDE_FLAG_READY
                packet.Worldstates.emplace_back(2657, 1); // ZM_MAP_HORDE_FLAG_NOT_READY
                packet.Worldstates.emplace_back(2656, 1); // ZM_MAP_ALLIANCE_FLAG_NOT_READY
                packet.Worldstates.emplace_back(2655, 0); // ZM_MAP_ALLIANCE_FLAG_READY
            }
            break;
        case 3698: // Nagrand Arena
            if (battleground && battleground->GetTypeID(true) == BATTLEGROUND_NA)
                battleground->FillInitialWorldStates(packet);
            else
            {
                 packet.Worldstates.emplace_back(2575, 0); // BATTLEGROUND_NAGRAND_ARENA_GOLD
                 packet.Worldstates.emplace_back(2576, 0); // BATTLEGROUND_NAGRAND_ARENA_GREEN
                 packet.Worldstates.emplace_back(2577, 0); // BATTLEGROUND_NAGRAND_ARENA_SHOW
            }
            break;
        case 3702: // Blade's Edge Arena
            if (battleground && battleground->GetTypeID(true) == BATTLEGROUND_BE)
                battleground->FillInitialWorldStates(packet);
            else
            {
                packet.Worldstates.emplace_back(2544, 0); // BATTLEGROUND_BLADES_EDGE_ARENA_GOLD
                packet.Worldstates.emplace_back(2545, 0); // BATTLEGROUND_BLADES_EDGE_ARENA_GREEN
                packet.Worldstates.emplace_back(2547, 0); // BATTLEGROUND_BLADES_EDGE_ARENA_SHOW
            }
            break;
        case 3968: // Ruins of Lordaeron
            if (battleground && battleground->GetTypeID(true) == BATTLEGROUND_RL)
                battleground->FillInitialWorldStates(packet);
            else
            {
                packet.Worldstates.emplace_back(3000, 0); // BATTELGROUND_RUINS_OF_LORDAERNON_GOLD
                packet.Worldstates.emplace_back(3001, 0); // BATTELGROUND_RUINS_OF_LORDAERNON_GREEN
                packet.Worldstates.emplace_back(3002, 0); // BATTELGROUND_RUINS_OF_LORDAERNON_SHOW
            }
            break;
        case 4378: // Dalaran Sewers
            if (battleground && battleground->GetTypeID(true) == BATTLEGROUND_DS)
                battleground->FillInitialWorldStates(packet);
            else
            {
                packet.Worldstates.emplace_back(3601, 0); // ARENA_WORLD_STATE_ALIVE_PLAYERS_GOLD
                packet.Worldstates.emplace_back(3600, 0); // ARENA_WORLD_STATE_ALIVE_PLAYERS_GREEN
                packet.Worldstates.emplace_back(3610, 0); // ARENA_WORLD_STATE_ALIVE_PLAYERS_SHOW
            }
            break;
        case 4384: // Strand of the Ancients
            if (battleground && battleground->GetTypeID(true) == BATTLEGROUND_SA)
                battleground->FillInitialWorldStates(packet);
            else
            {
                packet.Worldstates.emplace_back(3849, 0); // Gate of Temple
                packet.Worldstates.emplace_back(3638, 0); // Gate of Yellow Moon
                packet.Worldstates.emplace_back(3623, 0); // Gate of Green Emerald
                packet.Worldstates.emplace_back(3620, 0); // Gate of Blue Sapphire
                packet.Worldstates.emplace_back(3617, 0); // Gate of Red Sun
                packet.Worldstates.emplace_back(3614, 0); // Gate of Purple Ametyst
                packet.Worldstates.emplace_back(3571, 0); // bonus timer (1 - on, 0 - off)
                packet.Worldstates.emplace_back(3565, 0); // Horde Attacker
                packet.Worldstates.emplace_back(3564, 0); // Alliance Attacker

                // End Round timer, example: 19:59 -> A:BC
                packet.Worldstates.emplace_back(3561, 0); // C
                packet.Worldstates.emplace_back(3560, 0); // B
                packet.Worldstates.emplace_back(3559, 0); // A

                packet.Worldstates.emplace_back(3637, 0); // BG_SA_CENTER_GY_ALLIANCE
                packet.Worldstates.emplace_back(3636, 0); // BG_SA_RIGHT_GY_ALLIANCE
                packet.Worldstates.emplace_back(3635, 0); // BG_SA_LEFT_GY_ALLIANCE
                packet.Worldstates.emplace_back(3634, 0); // BG_SA_CENTER_GY_HORDE
                packet.Worldstates.emplace_back(3633, 0); // BG_SA_LEFT_GY_HORDE
                packet.Worldstates.emplace_back(3632, 0); // BG_SA_RIGHT_GY_HORDE
                packet.Worldstates.emplace_back(3631, 0); // BG_SA_HORDE_DEFENCE_TOKEN
                packet.Worldstates.emplace_back(3630, 0); // BG_SA_ALLIANCE_DEFENCE_TOKEN
                packet.Worldstates.emplace_back(3629, 0); // BG_SA_LEFT_ATT_TOKEN_HRD
                packet.Worldstates.emplace_back(3628, 0); // BG_SA_RIGHT_ATT_TOKEN_HRD
                packet.Worldstates.emplace_back(3627, 0); // BG_SA_RIGHT_ATT_TOKEN_ALL
                packet.Worldstates.emplace_back(3626, 0); // BG_SA_LEFT_ATT_TOKEN_ALL
                // missing unknowns
            }
            break;
        case 4406: // Ring of Valor
            if (battleground && battleground->GetTypeID(true) == BATTLEGROUND_RV)
                battleground->FillInitialWorldStates(packet);
            else
            {
               packet.Worldstates.emplace_back(3600, 0); // ARENA_WORLD_STATE_ALIVE_PLAYERS_GREEN
               packet.Worldstates.emplace_back(3601, 0); // ARENA_WORLD_STATE_ALIVE_PLAYERS_GOLD
               packet.Worldstates.emplace_back(3610, 0); // ARENA_WORLD_STATE_ALIVE_PLAYERS_SHOW
            }
            break;
        case 4710: // Isle of Conquest
            if (battleground && battleground->GetTypeID(true) == BATTLEGROUND_IC)
                battleground->FillInitialWorldStates(packet);
            else
            {
                packet.Worldstates.emplace_back(4221, 1);   // BG_IC_ALLIANCE_RENFORT_SET
                packet.Worldstates.emplace_back(4222, 1);   // BG_IC_HORDE_RENFORT_SET
                packet.Worldstates.emplace_back(4226, 300); // BG_IC_ALLIANCE_RENFORT
                packet.Worldstates.emplace_back(4227, 300); // BG_IC_HORDE_RENFORT
                packet.Worldstates.emplace_back(4322, 1);   // BG_IC_GATE_FRONT_H_WS_OPEN
                packet.Worldstates.emplace_back(4321, 1);   // BG_IC_GATE_WEST_H_WS_OPEN
                packet.Worldstates.emplace_back(4320, 1);   // BG_IC_GATE_EAST_H_WS_OPEN
                packet.Worldstates.emplace_back(4323, 1);   // BG_IC_GATE_FRONT_A_WS_OPEN
                packet.Worldstates.emplace_back(4324, 1);   // BG_IC_GATE_WEST_A_WS_OPEN
                packet.Worldstates.emplace_back(4325, 1);   // BG_IC_GATE_EAST_A_WS_OPEN
                packet.Worldstates.emplace_back(4317, 1);   // unk
                packet.Worldstates.emplace_back(4301, 1);   // BG_IC_DOCKS_UNCONTROLLED
                packet.Worldstates.emplace_back(4296, 1);   // BG_IC_HANGAR_UNCONTROLLED
                packet.Worldstates.emplace_back(4306, 1);   // BG_IC_QUARRY_UNCONTROLLED
                packet.Worldstates.emplace_back(4311, 1);   // BG_IC_REFINERY_UNCONTROLLED
                packet.Worldstates.emplace_back(4294, 1);   // BG_IC_WORKSHOP_UNCONTROLLED
                packet.Worldstates.emplace_back(4243, 1);   // unk
                packet.Worldstates.emplace_back(4345, 1);   // unk
            }
            break;
        case 4987: // The Ruby Sanctum
            if (instance)
                instance->FillInitialWorldStates(packet);
            else
            {
                packet.Worldstates.emplace_back(5049, 50); // WORLDSTATE_CORPOREALITY_MATERIAL
                packet.Worldstates.emplace_back(5050, 50); // WORLDSTATE_CORPOREALITY_TWILIGHT
                packet.Worldstates.emplace_back(5051, 0);  // WORLDSTATE_CORPOREALITY_TOGGLE
            }
            break;
        case 4812: // Icecrown Citadel
            if (instance)
                instance->FillInitialWorldStates(packet);
            else
            {
                packet.Worldstates.emplace_back(4903, 0);  // WORLDSTATE_SHOW_TIMER (Blood Quickening weekly)
                packet.Worldstates.emplace_back(4904, 30); // WORLDSTATE_EXECUTION_TIME
                packet.Worldstates.emplace_back(4940, 0);  // WORLDSTATE_SHOW_ATTEMPTS
                packet.Worldstates.emplace_back(4941, 50); // WORLDSTATE_ATTEMPTS_REMAINING
                packet.Worldstates.emplace_back(4942, 50); // WORLDSTATE_ATTEMPTS_MAX
            }
            break;
        case 4100: // The Culling of Stratholme
            if (instance)
                instance->FillInitialWorldStates(packet);
            else
            {
                packet.Worldstates.emplace_back(3479, 0);  // WORLDSTATE_SHOW_CRATES
                packet.Worldstates.emplace_back(3480, 0);  // WORLDSTATE_CRATES_REVEALED
                packet.Worldstates.emplace_back(3504, 0);  // WORLDSTATE_WAVE_COUNT
                packet.Worldstates.emplace_back(3931, 25); // WORLDSTATE_TIME_GUARDIAN
                packet.Worldstates.emplace_back(3932, 0);  // WORLDSTATE_TIME_GUARDIAN_SHOW
            }
            break;
        case 4228: // The Oculus
            if (instance)
                instance->FillInitialWorldStates(packet);
            else
            {
                packet.Worldstates.emplace_back(3524, 0); // WORLD_STATE_CENTRIFUGE_CONSTRUCT_SHOW
                packet.Worldstates.emplace_back(3486, 0); // WORLD_STATE_CENTRIFUGE_CONSTRUCT_AMOUNT
            }
            break;
        case 4273: // Ulduar
            if (instance)
                instance->FillInitialWorldStates(packet);
            else
            {
                packet.Worldstates.emplace_back(4132, 0); // WORLDSTATE_ALGALON_TIMER_ENABLED
                packet.Worldstates.emplace_back(4131, 0); // WORLDSTATE_ALGALON_DESPAWN_TIMER
            }
            break;
        case 4415: // Violet Hold
            if (instance)
                instance->FillInitialWorldStates(packet);
            else
            {
               packet.Worldstates.emplace_back(3816, 0);   // WORLD_STATE_VH_SHOW
               packet.Worldstates.emplace_back(3815, 100); // WORLD_STATE_VH_PRISON_STATE
               packet.Worldstates.emplace_back(3810, 0);   // WORLD_STATE_VH_WAVE_COUNT
            }
            break;
        case 4820: // Halls of Refection
            if (instance)
                instance->FillInitialWorldStates(packet);
            else
            {
                packet.Worldstates.emplace_back(4884, 0); // WORLD_STATE_HOR_WAVES_ENABLED
                packet.Worldstates.emplace_back(4882, 0); // WORLD_STATE_HOR_WAVE_COUNT
            }
            break;
        case AREA_WINTERGRASP: // Wintergrasp
            if (battlefield && battlefield->GetTypeId() == BATTLEFIELD_WG)
                battlefield->FillInitialWorldStates(packet);
            else
            {
            }
            break;
        default:
            break;
    }

    SendDirectMessage(packet.Write());
    SendBGWeekendWorldStates();
    SendBattlefieldWorldStates();
}

uint32 Player::GetXPRestBonus(uint32 xp)
{
    uint32 rested_bonus = (uint32)GetRestBonus();           // xp for each rested bonus

    if (rested_bonus > xp)                                   // max rested_bonus == xp or (r+x) = 200% xp
        rested_bonus = xp;

    SetRestBonus(GetRestBonus() - rested_bonus);

    TC_LOG_DEBUG("entities.player", "Player::GetXPRestBonus: Player '{}' ({}) gain {} xp (+{} Rested Bonus). Rested points={}", GetGUID().ToString(), GetName(), xp + rested_bonus, rested_bonus, GetRestBonus());
    return rested_bonus;
}

void Player::SetBindPoint(ObjectGuid guid) const
{
    WorldPackets::Misc::BinderConfirm packet(guid);
    SendDirectMessage(packet.Write());
}

void Player::SendAttackSwingCantAttack() const
{
    SendDirectMessage(WorldPackets::Combat::AttackSwingCantAttack().Write());
}

void Player::SendAttackSwingCancelAttack() const
{
    SendDirectMessage(WorldPackets::Combat::CancelCombat().Write());
}

void Player::SendAttackSwingDeadTarget() const
{
    SendDirectMessage(WorldPackets::Combat::AttackSwingDeadTarget().Write());
}

void Player::SendAttackSwingNotInRange() const
{
    SendDirectMessage(WorldPackets::Combat::AttackSwingNotInRange().Write());
}

void Player::SendAttackSwingBadFacingAttack() const
{
    SendDirectMessage(WorldPackets::Combat::AttackSwingBadFacing().Write());
}

void Player::SendAutoRepeatCancel(Unit* target)
{
    WorldPackets::Combat::CancelAutoRepeat cancelAutoRepeat;
    cancelAutoRepeat.Guid = target->GetPackGUID();      // may be it's target guid
    SendMessageToSet(cancelAutoRepeat.Write(), true);
}

void Player::SendExplorationExperience(uint32 Area, uint32 Experience) const
{
    WorldPacket data(SMSG_EXPLORATION_EXPERIENCE, 8);
    data << uint32(Area);
    data << uint32(Experience);
    SendDirectMessage(&data);
}

void Player::SendDungeonDifficulty(bool IsInGroup) const
{
    ZoneScopedNC("Player::SendDungeonDifficulty", WORLD_UPDATE_COLOR)

    uint8 val = 0x00000001;
    WorldPacket data(MSG_SET_DUNGEON_DIFFICULTY, 12);
    data << (uint32)GetDungeonDifficulty();
    data << uint32(val);
    data << uint32(IsInGroup);
    SendDirectMessage(&data);
}

void Player::SendRaidDifficulty(bool IsInGroup, int32 forcedDifficulty) const
{
    uint8 val = 0x00000001;
    WorldPacket data(MSG_SET_RAID_DIFFICULTY, 12);
    data << uint32(forcedDifficulty == -1 ? GetRaidDifficulty() : forcedDifficulty);
    data << uint32(val);
    data << uint32(IsInGroup);
    SendDirectMessage(&data);
}

void Player::SendResetFailedNotify(uint32 mapid) const
{
    WorldPacket data(SMSG_RESET_FAILED_NOTIFY, 4);
    data << uint32(mapid);
    SendDirectMessage(&data);
}

InstancePlayerBind* Player::GetBoundInstance(uint32 mapid, Difficulty difficulty, bool withExpired)
{
    ZoneScopedNC("InstancePlayerBind* Player::GetBoundInstance", WORLD_UPDATE_COLOR)

    // some instances only have one difficulty
    MapDifficulty const* mapDiff = GetDownscaledMapDifficultyData(mapid, difficulty);
    if (!mapDiff)
        return nullptr;

    BoundInstancesMap::iterator itr = m_boundInstances[difficulty].find(mapid);
    if (itr != m_boundInstances[difficulty].end())
        if (itr->second.extendState || withExpired)
            return &itr->second;
    return nullptr;
}

InstanceSave* Player::GetInstanceSave(uint32 mapid, bool raid)
{
    InstancePlayerBind* pBind = GetBoundInstance(mapid, GetDifficulty(raid));
    InstanceSave* pSave = pBind ? pBind->save : nullptr;
    if (!pBind || !pBind->perm)
        if (Group* group = GetGroup())
            if (InstanceGroupBind* groupBind = group->GetBoundInstance(GetDifficulty(raid), mapid))
                pSave = groupBind->save;

    return pSave;
}

void Player::UnbindInstance(uint32 mapid, Difficulty difficulty, bool unload)
{
    BoundInstancesMap::iterator itr = m_boundInstances[difficulty].find(mapid);
    UnbindInstance(itr, difficulty, unload);
}

void Player::UnbindInstance(BoundInstancesMap::iterator &itr, Difficulty difficulty, bool unload)
{
    if (itr != m_boundInstances[difficulty].end())
    {
        if (!unload)
        {
            CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHAR_INSTANCE_BY_INSTANCE_GUID);

            stmt->setUInt32(0, GetGUID().GetCounter());
            stmt->setUInt32(1, itr->second.save->GetInstanceId());

            CharacterDatabase.Execute(stmt);
        }

        if (itr->second.perm)
            GetSession()->SendCalendarRaidLockoutRemoved(itr->second.save);

        itr->second.save->RemovePlayer(this);               // save can become invalid
        m_boundInstances[difficulty].erase(itr++);
    }
}

InstancePlayerBind* Player::BindToInstance(InstanceSave* save, bool permanent, BindExtensionState extendState, bool load)
{
    if (save)
    {
        InstancePlayerBind& bind = m_boundInstances[save->GetDifficulty()][save->GetMapId()];
        if (extendState == EXTEND_STATE_KEEP) // special flag, keep the player's current extend state when updating for new boss down
        {
            if (save == bind.save)
                extendState = bind.extendState;
            else
                extendState = EXTEND_STATE_NORMAL;
        }
        if (!load)
        {
            if (bind.save)
            {
                // update the save when the group kills a boss
                if (permanent != bind.perm || save != bind.save || extendState != bind.extendState)
                {
                    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_CHAR_INSTANCE);

                    stmt->setUInt32(0, save->GetInstanceId());
                    stmt->setBool(1, permanent);
                    stmt->setUInt8(2, extendState);
                    stmt->setUInt32(3, GetGUID().GetCounter());
                    stmt->setUInt32(4, bind.save->GetInstanceId());

                    CharacterDatabase.Execute(stmt);
                }
            }
            else
            {
                CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_INS_CHAR_INSTANCE);

                stmt->setUInt32(0, GetGUID().GetCounter());
                stmt->setUInt32(1, save->GetInstanceId());
                stmt->setBool(2, permanent);
                stmt->setUInt8(3, extendState);

                CharacterDatabase.Execute(stmt);
            }
        }

        if (bind.save != save)
        {
            if (bind.save)
                bind.save->RemovePlayer(this);
            save->AddPlayer(this);
        }

        if (permanent)
            save->SetCanReset(false);

        bind.save = save;
        bind.perm = permanent;
        bind.extendState = extendState;
        if (!load)
            TC_LOG_DEBUG("maps", "Player::BindToInstance: Player '{}' ({}) is now bound to map (ID: {}, Instance: {}, Difficulty: {})",
                GetName(), GetGUID().ToString(), save->GetMapId(), save->GetInstanceId(), static_cast<uint32>(save->GetDifficulty()));
        sScriptMgr->OnPlayerBindToInstance(this, save->GetDifficulty(), save->GetMapId(), permanent, extendState);
        return &bind;
    }

    return nullptr;
}

void Player::BindToInstance()
{
    InstanceSave* mapSave = sInstanceSaveMgr->GetInstanceSave(_pendingBindId);
    if (!mapSave) //it seems sometimes mapSave is nullptr, but I did not check why
        return;

    WorldPacket data(SMSG_INSTANCE_SAVE_CREATED, 4);
    data << uint32(0);
    SendDirectMessage(&data);
    if (!IsGameMaster())
    {
        BindToInstance(mapSave, true, EXTEND_STATE_KEEP);
        GetSession()->SendCalendarRaidLockoutAdded(mapSave);
    }
}

void Player::SetPendingBind(uint32 instanceId, uint32 bindTimer)
{
    _pendingBindId = instanceId;
    _pendingBindTimer = bindTimer;
}

void Player::SendRaidInfo()
{
    uint32 counter = 0;

    WorldPacket data(SMSG_RAID_INSTANCE_INFO, 4);

    size_t p_counter = data.wpos();
    data << uint32(counter);                                // placeholder

    time_t now = GameTime::GetGameTime();

    for (uint8 i = 0; i < MAX_DIFFICULTY; ++i)
    {
        for (BoundInstancesMap::iterator itr = m_boundInstances[i].begin(); itr != m_boundInstances[i].end(); ++itr)
        {
            InstancePlayerBind const& bind = itr->second;
            if (bind.perm)
            {
                InstanceSave* save = bind.save;
                data << uint32(save->GetMapId());                          // map id
                data << uint32(save->GetDifficulty());                     // difficulty
                data << uint64(save->GetInstanceId());                     // instance id
                data << uint8(bind.extendState != EXTEND_STATE_EXPIRED);   // expired = 0
                data << uint8(bind.extendState == EXTEND_STATE_EXTENDED);  // extended = 1
                time_t nextReset = save->GetResetTime();
                if (bind.extendState == EXTEND_STATE_EXTENDED)
                    nextReset = sInstanceSaveMgr->GetSubsequentResetTime(save->GetMapId(), save->GetDifficulty(), save->GetResetTime());
                data << uint32(nextReset - now);                // reset time
                ++counter;
            }
        }
    }
    data.put<uint32>(p_counter, counter);
    SendDirectMessage(&data);
}

bool Player::Satisfy(AccessRequirement const* ar, uint32 target_map, bool report)
{
    if (!IsGameMaster() && ar)
    {
        uint8 LevelMin = 0;
        uint8 LevelMax = 0;

        MapEntry const* mapEntry = sMapStore.LookupEntry(target_map);
        if (!mapEntry)
            return false;

        if (!sWorld->getBoolConfig(CONFIG_INSTANCE_IGNORE_LEVEL))
        {
            if (ar->levelMin && GetLevel() < ar->levelMin)
                LevelMin = ar->levelMin;
            if (ar->levelMax && GetLevel() > ar->levelMax)
                LevelMax = ar->levelMax;
        }

        uint32 missingItem = 0;
        if (ar->item)
        {
            if (!HasItemCount(ar->item) &&
                (!ar->item2 || !HasItemCount(ar->item2)))
                missingItem = ar->item;
        }
        else if (ar->item2 && !HasItemCount(ar->item2))
            missingItem = ar->item2;

        if (DisableMgr::IsDisabledFor(DISABLE_TYPE_MAP, target_map, this))
        {
            GetSession()->SendAreaTriggerMessage("%s", GetSession()->GetTrinityString(LANG_INSTANCE_CLOSED));
            return false;
        }

        uint32 missingQuest = 0;
        if (GetTeam() == ALLIANCE && ar->quest_A && !GetQuestRewardStatus(ar->quest_A))
            missingQuest = ar->quest_A;
        else if (GetTeam() == HORDE && ar->quest_H && !GetQuestRewardStatus(ar->quest_H))
            missingQuest = ar->quest_H;

        uint32 missingAchievement = 0;
        Player* leader = this;
        ObjectGuid leaderGuid = GetGroup() ? GetGroup()->GetLeaderGUID() : GetGUID();
        if (leaderGuid != GetGUID())
            leader = ObjectAccessor::FindPlayer(leaderGuid);

        if (ar->achievement)
            if (!leader || !leader->HasAchieved(ar->achievement))
                missingAchievement = ar->achievement;

        Difficulty target_difficulty = GetDifficulty(mapEntry->IsRaid());
        MapDifficulty const* mapDiff = GetDownscaledMapDifficultyData(target_map, target_difficulty);
        if (LevelMin || LevelMax || missingItem || missingQuest || missingAchievement)
        {
            if (report)
            {
                if (missingQuest && !ar->questFailedText.empty())
                    ChatHandler(GetSession()).PSendSysMessage("%s", ar->questFailedText.c_str());
                else if (mapDiff->hasErrorMessage) // if (missingAchievement) covered by this case
                    SendTransferAborted(target_map, TRANSFER_ABORT_DIFFICULTY, target_difficulty);
                else if (missingItem)
                    GetSession()->SendAreaTriggerMessage(GetSession()->GetTrinityString(LANG_LEVEL_MINREQUIRED_AND_ITEM), LevelMin, ASSERT_NOTNULL(sObjectMgr->GetItemTemplate(missingItem))->Name1.c_str());
                else if (LevelMin)
                    GetSession()->SendAreaTriggerMessage(GetSession()->GetTrinityString(LANG_LEVEL_MINREQUIRED), LevelMin);
            }
            return false;
        }
    }
    return true;
}

bool Player::IsInstanceLoginGameMasterException() const
{
    if (CanBeGameMaster())
    {
        ChatHandler(GetSession()).SendSysMessage(LANG_INSTANCE_LOGIN_GAMEMASTER_EXCEPTION);
        return true;
    }
    else
        return false;
}

bool Player::CheckInstanceValidity(bool /*isLogin*/)
{
    // game masters' instances are always valid
    if (IsGameMaster())
        return true;

    // non-instances are always valid
    Map* map = FindMap();
    if (!map || !map->IsDungeon())
        return true;

    // raid instances require the player to be in a raid group to be valid
    if (map->IsRaid() && !sWorld->getBoolConfig(CONFIG_INSTANCE_IGNORE_RAID))
        if (!GetGroup() || !GetGroup()->isRaidGroup())
            return false;

    if (Group* group = GetGroup())
    {
        // check if player's group is bound to this instance
        InstanceGroupBind* bind = group->GetBoundInstance(map->GetDifficulty(), map->GetId());
        if (!bind || !bind->save || bind->save->GetInstanceId() != map->GetInstanceId())
            return false;

        Map::PlayerList const& players = map->GetPlayers();
        if (!players.isEmpty())
            for (Map::PlayerList::const_iterator it = players.begin(); it != players.end(); ++it)
            {
                if (Player* otherPlayer = it->GetSource())
                {
                    if (otherPlayer->IsGameMaster())
                        continue;
                    if (!otherPlayer->m_InstanceValid) // ignore players that currently have a homebind timer active
                        continue;
                    if (group != otherPlayer->GetGroup())
                        return false;
                }
            }
    }
    else
    {
        // instance is invalid if we are not grouped and there are other players
        if (map->GetPlayersCountExceptGMs() > 1)
            return false;

        // check if the player is bound to this instance
        InstancePlayerBind* bind = GetBoundInstance(map->GetId(), map->GetDifficulty());
        if (!bind || !bind->save || bind->save->GetInstanceId() != map->GetInstanceId())
            return false;
    }

    return true;
}

/// Reset all solo instances and optionally send a message on success for each
void Player::ResetInstances(uint8 method, bool isRaid)
{
    ZoneScopedNC("Player::ResetInstances", WORLD_UPDATE_COLOR)

    // method can be INSTANCE_RESET_ALL, INSTANCE_RESET_CHANGE_DIFFICULTY, INSTANCE_RESET_GROUP_JOIN

    // we assume that when the difficulty changes, all instances that can be reset will be
    Difficulty diff = GetDifficulty(isRaid);

    for (BoundInstancesMap::iterator itr = m_boundInstances[diff].begin(); itr != m_boundInstances[diff].end();)
    {
        InstanceSave* p = itr->second.save;
        MapEntry const* entry = sMapStore.LookupEntry(itr->first);
        if (!entry || entry->IsRaid() != isRaid || !p->CanReset())
        {
            ++itr;
            continue;
        }

        if (method == INSTANCE_RESET_ALL)
        {
            // the "reset all instances" method can only reset normal maps
            if (entry->InstanceType == MAP_RAID || diff == DUNGEON_DIFFICULTY_HEROIC)
            {
                ++itr;
                continue;
            }
        }

        // if the map is loaded, reset it
        Map* map = sMapMgr->FindMap(p->GetMapId(), Position(), p->GetInstanceId());
        if (map && map->IsDungeon())
            if (!map->ToInstanceMap()->Reset(method))
            {
                ++itr;
                continue;
            }

        // since this is a solo instance there should not be any players inside
        if (method == INSTANCE_RESET_ALL || method == INSTANCE_RESET_CHANGE_DIFFICULTY)
            SendResetInstanceSuccess(p->GetMapId());

        p->DeleteFromDB();
        m_boundInstances[diff].erase(itr++);

        // the following should remove the instance save from the manager and delete it as well
        p->RemovePlayer(this);
    }
}

void Player::SendResetInstanceSuccess(uint32 MapId) const
{
    WorldPacket data(SMSG_INSTANCE_RESET, 4);
    data << uint32(MapId);
    SendDirectMessage(&data);
}

void Player::SendResetInstanceFailed(uint32 reason, uint32 MapId) const
{
    /*reasons for instance reset failure:
    // 0: There are players inside the instance.
    // 1: There are players offline in your party.
    // 2>: There are players in your party attempting to zone into an instance.
    */
    WorldPacket data(SMSG_INSTANCE_RESET_FAILED, 4);
    data << uint32(reason);
    data << uint32(MapId);
    SendDirectMessage(&data);
}

Pet* Player::GetPet() const
{
    if (ObjectGuid pet_guid = GetPetGUID())
    {
        if (!pet_guid.IsPet())
            return nullptr;

        Pet* pet = ObjectAccessor::GetPet(*this, pet_guid);

        if (!pet)
            return nullptr;

        if (IsInWorld())
            return pet;

        // there may be a guardian in this slot
        //TC_LOG_ERROR("entities.player", "Player::GetPet: Pet {} does not exist.", GUID_LOPART(pet_guid));
        //const_cast<Player*>(this)->SetPetGUID(0);
    }

    return nullptr;
}

void Player::RemovePet(Pet* pet, PetSaveMode mode, bool returnreagent)
{
    if (!pet)
        pet = GetPet();

    if (pet)
    {
        TC_LOG_DEBUG("entities.pet", "Player::RemovePet: Player '{}' ({}), Pet (Entry: {}, Mode: {}, ReturnReagent: {})",
            GetName(), GetGUID().ToString(), pet->GetEntry(), mode, returnreagent);

        if (pet->m_removed)
            return;
    }

    if (returnreagent && (pet || m_temporaryUnsummonedPetNumber) && !InBattleground())
    {
        //returning of reagents only for players, so best done here
        uint32 spellId = pet ? pet->GetUInt32Value(UNIT_CREATED_BY_SPELL) : m_oldpetspell;
        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);

        if (spellInfo)
        {
            for (uint32 i = 0; i < MAX_SPELL_REAGENTS; ++i)
            {
                if (spellInfo->Reagent[i] > 0)
                {
                    ItemPosCountVec dest;                   //for succubus, voidwalker, felhunter and felguard credit soulshard when despawn reason other than death (out of range, logout)
                    InventoryResult msg = CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, spellInfo->Reagent[i], spellInfo->ReagentCount[i]);
                    if (msg == EQUIP_ERR_OK)
                    {
                        Item* item = StoreNewItem(dest, spellInfo->Reagent[i], true);
                        if (IsInWorld())
                            SendNewItem(item, spellInfo->ReagentCount[i], true, false);
                    }
                }
            }
        }
        m_temporaryUnsummonedPetNumber = 0;
    }

    if (!pet)
    {
        if (mode == PET_SAVE_NOT_IN_SLOT && m_petStable && m_petStable->CurrentPet)
        {
            // Handle removing pet while it is in "temporarily unsummoned" state, for example on mount
            CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_CHAR_PET_SLOT_BY_ID);
            stmt->setUInt8(0, PET_SAVE_NOT_IN_SLOT);
            stmt->setUInt32(1, GetGUID().GetCounter());
            stmt->setUInt32(2, m_petStable->CurrentPet->PetNumber);
            CharacterDatabase.Execute(stmt);

            m_petStable->UnslottedPets.push_back(std::move(*m_petStable->CurrentPet));
            m_petStable->CurrentPet.reset();
        }

        return;
    }

    pet->CombatStop();

    // only if current pet in slot
    pet->SavePetToDB(mode);

    ASSERT(m_petStable->CurrentPet && m_petStable->CurrentPet->PetNumber == pet->GetCharmInfo()->GetPetNumber());
    if (mode == PET_SAVE_NOT_IN_SLOT)
    {
        m_petStable->UnslottedPets.push_back(std::move(*m_petStable->CurrentPet));
        m_petStable->CurrentPet.reset();
    }
    else if (mode == PET_SAVE_AS_DELETED)
        m_petStable->CurrentPet.reset();
    // else if (stable slots) handled in opcode handlers due to required swaps
    // else (current pet) doesnt need to do anything

    SetMinion(pet, false);

    pet->AddObjectToRemoveList();
    pet->m_removed = true;

    if (pet->isControlled())
    {
        WorldPacket data(SMSG_PET_SPELLS, 8);
        data << uint64(0);
        SendDirectMessage(&data);

        if (GetGroup())
            SetGroupUpdateFlag(GROUP_UPDATE_PET);
    }
}

void Player::AddPetAura(PetAura const* petSpell)
{
    m_petAuras.insert(petSpell);
    if (Pet* pet = GetPet())
        pet->CastPetAura(petSpell);
}

void Player::RemovePetAura(PetAura const* petSpell)
{
    m_petAuras.erase(petSpell);
    if (Pet* pet = GetPet())
        pet->RemoveAurasDueToSpell(petSpell->GetAura(pet->GetEntry()));
}

void Player::StopCastingCharm()
{
    if (IsGhouled())
    {
        RemoveGhoul();
        return;
    }

    Unit* charm = GetCharmed();
    if (!charm)
        return;

    if (charm->GetTypeId() == TYPEID_UNIT)
    {
        if (charm->ToCreature()->HasUnitTypeMask(UNIT_MASK_PUPPET))
            static_cast<Puppet*>(charm)->UnSummon();
        else if (charm->IsVehicle())
        {
            ExitVehicle();

            // Temporary for issue https://github.com/TrinityCore/TrinityCore/issues/24876
            if (!GetCharmedGUID().IsEmpty() && !charm->HasAuraTypeWithCaster(SPELL_AURA_CONTROL_VEHICLE, GetGUID()))
            {
                TC_LOG_FATAL("entities.player", "Player::StopCastingCharm Player '{}' ({}) is not able to uncharm vehicle ({}) because of missing SPELL_AURA_CONTROL_VEHICLE",
                    GetName(), GetGUID().ToString(), GetCharmedGUID().ToString());

                // attempt to recover from missing HandleAuraControlVehicle unapply handling
                // THIS IS A HACK, NEED TO FIND HOW IS IT EVEN POSSBLE TO NOT HAVE THE AURA
                _ExitVehicle();
            }
        }
    }
    if (GetCharmedGUID())
        charm->RemoveCharmAuras();

    if (GetCharmedGUID())
    {
        TC_LOG_FATAL("entities.player", "Player::StopCastingCharm: Player '{}' ({}) is not able to uncharm unit ({})", GetName(), GetGUID().ToString(), GetCharmedGUID().ToString());
        if (!charm->GetCharmerGUID().IsEmpty())
        {
            TC_LOG_FATAL("entities.player", "Player::StopCastingCharm: Charmed unit has charmer {}\nPlayer debug info: {}\nCharm debug info: {}",
                charm->GetCharmerGUID().ToString(), GetDebugInfo(), charm->GetDebugInfo());
            ABORT();
        }

        SetCharm(charm, false);
    }
}

void Player::SendOnCancelExpectedVehicleRideAura() const
{
    WorldPacket data(SMSG_ON_CANCEL_EXPECTED_RIDE_VEHICLE_AURA, 0);
    SendDirectMessage(&data);
}

void Player::PetSpellInitialize()
{
    Pet* pet = GetPet();

    if (!pet)
        return;

    CharmInfo* charmInfo = pet->GetCharmInfo();

    WorldPacket data(SMSG_PET_SPELLS, 8+2+4+4+4*MAX_UNIT_ACTION_BAR_INDEX+1+1);
    data << uint64(pet->GetGUID());
    data << uint16(pet->GetCreatureTemplate()->family);         // creature family (required for pet talents)
    data << uint32(pet->GetDuration());
    data << uint8(pet->GetReactState());
    data << uint8(charmInfo->GetCommandState());
    data << uint16(0); // Flags, mostly unknown

    TC_LOG_DEBUG("entities.pet", "Player::PetspellInitialize: Creating spellgroups for summoned pet");

    // action bar loop
    charmInfo->BuildActionBar(&data);

    size_t spellsCountPos = data.wpos();

    // spells count
    uint8 addlist = 0;
    data << uint8(addlist);                                 // placeholder

    if (pet->IsPermanentPetFor(this))
    {
        // spells loop
        for (PetSpellMap::iterator itr = pet->m_spells.begin(); itr != pet->m_spells.end(); ++itr)
        {
            if (itr->second.state == PETSPELL_REMOVED)
                continue;

            data << uint32(MAKE_UNIT_ACTION_BUTTON(itr->first, itr->second.active));
            ++addlist;
        }
    }

    data.put<uint8>(spellsCountPos, addlist);

    //Cooldowns
    pet->GetSpellHistory()->WritePacket<Pet>(data);

    SendDirectMessage(&data);
}

void Player::PossessSpellInitialize()
{
    Unit* charm = GetCharmed();
    if (!charm)
        return;

    CharmInfo* charmInfo = charm->GetCharmInfo();

    if (!charmInfo)
    {
        TC_LOG_ERROR("entities.player", "Player::PossessSpellInitialize: charm ({}) has no charminfo!", charm->GetGUID().ToString());
        return;
    }

    WorldPacket data(SMSG_PET_SPELLS, 8+2+4+4+4*MAX_UNIT_ACTION_BAR_INDEX+1+1);
    data << uint64(charm->GetGUID());
    data << uint16(0);
    data << uint32(0);
    data << uint32(0);

    charmInfo->BuildActionBar(&data);

    data << uint8(0);                                       // spells count
    data << uint8(0);                                       // cooldowns count

    SendDirectMessage(&data);
}

void Player::VehicleSpellInitialize()
{
    Creature* vehicle = GetVehicleCreatureBase();
    if (!vehicle)
        return;

    uint8 cooldownCount = vehicle->GetSpellHistory()->GetCooldownsSizeForPacket();

    WorldPacket data(SMSG_PET_SPELLS, 8 + 2 + 4 + 4 + 4 * 10 + 1 + 1 + cooldownCount * (4 + 2 + 4 + 4));
    data << uint64(vehicle->GetGUID());                     // Guid
    data << uint16(0);                                      // Pet Family (0 for all vehicles)
    data << uint32(vehicle->IsSummon() ? vehicle->ToTempSummon()->GetTimer() : 0); // Duration
    // The following three segments are read by the client as one uint32
    data << uint8(vehicle->GetReactState());                // React State
    data << uint8(0);                                       // Command State
    data << uint16(0x800);                                  // DisableActions (set for all vehicles)

    for (uint32 i = 0; i < MAX_CREATURE_SPELLS; ++i)
    {
        uint32 spellId = vehicle->m_spells[i];
        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
        if (!spellInfo)
        {
            data << uint16(0) << uint8(0) << uint8(i+8);
            continue;
        }

        if (!sConditionMgr->IsObjectMeetingVehicleSpellConditions(vehicle->GetEntry(), spellId, this, vehicle))
        {
            TC_LOG_DEBUG("condition", "Player::VehicleSpellInitialize: Player '{}' ({}) doesn't meet conditions for vehicle (Entry: {}, Spell: {})",
                GetName(), GetGUID().ToString(), vehicle->ToCreature()->GetEntry(), spellId);
            data << uint16(0) << uint8(0) << uint8(i+8);
            continue;
        }

        if (spellInfo->IsPassive())
            vehicle->CastSpell(vehicle, spellId, true);

        data << uint32(MAKE_UNIT_ACTION_BUTTON(spellId, i+8));
    }

    for (uint32 i = MAX_CREATURE_SPELLS; i < MAX_SPELL_CONTROL_BAR; ++i)
        data << uint32(0);

    data << uint8(0); // Auras?

    // Cooldowns
    vehicle->GetSpellHistory()->WritePacket<Pet>(data);
    SendDirectMessage(&data);
}

void Player::CharmSpellInitialize()
{
    Unit* charm = GetFirstControlled();
    if (!charm)
        return;

    CharmInfo* charmInfo = charm->GetCharmInfo();
    if (!charmInfo)
    {
        TC_LOG_ERROR("entities.player", "Player::CharmSpellInitialize(): Player '{}' ({}) has a charm ({}) but no no charminfo!",
            GetName(), GetGUID().ToString(), charm->GetGUID().ToString());
        return;
    }

    uint8 addlist = 0;
    if (charm->GetTypeId() != TYPEID_PLAYER)
    {
        //CreatureInfo const* cinfo = charm->ToCreature()->GetCreatureTemplate();
        //if (cinfo && cinfo->type == CREATURE_TYPE_DEMON && GetClass() == CLASS_WARLOCK)
        {
            for (uint32 i = 0; i < MAX_SPELL_CHARM; ++i)
                if (charmInfo->GetCharmSpell(i)->GetAction())
                    ++addlist;
        }
    }

    WorldPacket data(SMSG_PET_SPELLS, 8+2+4+4+4*MAX_UNIT_ACTION_BAR_INDEX+1+4*addlist+1);
    data << uint64(charm->GetGUID());
    data << uint16(0);
    data << uint32(0);

    if (charm->GetTypeId() != TYPEID_PLAYER)
        data << uint8(charm->ToCreature()->GetReactState()) << uint8(charmInfo->GetCommandState()) << uint16(0);
    else
        data << uint32(0);

    charmInfo->BuildActionBar(&data);

    data << uint8(addlist);

    if (addlist)
    {
        for (uint32 i = 0; i < MAX_SPELL_CHARM; ++i)
        {
            CharmSpellInfo* cspell = charmInfo->GetCharmSpell(i);
            if (cspell->GetAction())
                data << uint32(cspell->packedData);
        }
    }

    data << uint8(0);                                       // cooldowns count

    SendDirectMessage(&data);
}

void Player::SendRemoveControlBar() const
{
    WorldPacket data(SMSG_PET_SPELLS, 8);
    data << uint64(0);
    SendDirectMessage(&data);
}

void Player::RemovePetitionsAndSigns(ObjectGuid guid, CharterTypes type)
{
    sPetitionMgr->RemoveSignaturesBySignerAndType(guid, type);
    sPetitionMgr->RemovePetitionsByOwnerAndType(guid, type);
}

void Player::SetRestBonus(float rest_bonus_new)
{
    // Prevent resting on max level
    if (IsMaxLevel())
        rest_bonus_new = 0;

    if (rest_bonus_new < 0)
        rest_bonus_new = 0;

    float rest_bonus_max = (float)GetUInt32Value(PLAYER_NEXT_LEVEL_XP)*1.5f/2;

    if (rest_bonus_new > rest_bonus_max)
        m_rest_bonus = rest_bonus_max;
    else
        m_rest_bonus = rest_bonus_new;

    // update data for client
    if ((GetsRecruitAFriendBonus(true) && (GetSession()->IsARecruiter() || GetSession()->GetRecruiterId() != 0)))
        SetRestState(REST_STATE_RAF_LINKED);
    else
    {
        if (m_rest_bonus > 10)
            SetRestState(REST_STATE_RESTED);
        else if (m_rest_bonus <= 1)
            SetRestState(REST_STATE_NOT_RAF_LINKED);
    }

    //RestTickUpdate
    SetUInt32Value(PLAYER_REST_STATE_EXPERIENCE, uint32(m_rest_bonus));
}

bool Player::ActivateTaxiPathTo(std::vector<uint32> const& nodes, Creature* npc /*= nullptr*/, uint32 spellid /*= 0*/)
{
    if (nodes.size() < 2)
        return false;

    bool cancel = false;

    FIRE(Player,OnActivateTaxiPathEarly
        ,TSPlayer(this)
        ,TSMutable<bool, bool>(&cancel)
    );

    if (cancel)
        return false;

    // not let cheating with start flight in time of logout process || while in combat || has type state: stunned || has type state: root
    if (GetSession()->isLogingOut() || IsInCombat() || HasUnitState(UNIT_STATE_STUNNED) || HasUnitState(UNIT_STATE_ROOT))
    {
        GetSession()->SendActivateTaxiReply(ERR_TAXIPLAYERBUSY);
        return false;
    }

    if (HasUnitFlag(UNIT_FLAG_REMOVE_CLIENT_CONTROL))
        return false;

    // taximaster case
    if (npc)
    {
        // not let cheating with start flight mounted
        if (IsMounted())
        {
            GetSession()->SendActivateTaxiReply(ERR_TAXIPLAYERALREADYMOUNTED);
            return false;
        }

        if (IsInDisallowedMountForm())
        {
            GetSession()->SendActivateTaxiReply(ERR_TAXIPLAYERSHAPESHIFTED);
            return false;
        }

        // not let cheating with start flight in time of logout process || if casting not finished || while in combat || if not use Spell's with EffectSendTaxi
        if (IsNonMeleeSpellCast(false))
        {
            GetSession()->SendActivateTaxiReply(ERR_TAXIPLAYERBUSY);
            return false;
        }
    }
    // cast case or scripted call case
    else
    {
        RemoveAurasByType(SPELL_AURA_MOUNTED);

        if (IsInDisallowedMountForm())
            RemoveAurasByType(SPELL_AURA_MOD_SHAPESHIFT);

        if (Spell* spell = GetCurrentSpell(CURRENT_GENERIC_SPELL))
            if (spell->m_spellInfo->Id != spellid)
                InterruptSpell(CURRENT_GENERIC_SPELL, false);

        InterruptSpell(CURRENT_AUTOREPEAT_SPELL, false);

        if (Spell* spell = GetCurrentSpell(CURRENT_CHANNELED_SPELL))
            if (spell->m_spellInfo->Id != spellid)
                InterruptSpell(CURRENT_CHANNELED_SPELL, true);
    }

    uint32 sourcenode = nodes[0];

    // starting node too far away (cheat?)
    TaxiNodesEntry const* node = sTaxiNodesStore.LookupEntry(sourcenode);
    if (!node)
    {
        GetSession()->SendActivateTaxiReply(ERR_TAXINOSUCHPATH);
        return false;
    }

    // Prepare to flight start now
    UnsummonPetTemporaryIfAny();

    // stop combat at start taxi flight if any
    CombatStop();

    StopCastingCharm();
    StopCastingBindSight();
    ExitVehicle();

    // stop trade (client cancel trade at taxi map open but cheating tools can be used for reopen it)
    TradeCancel(true);

    // clean not finished taxi path if any
    m_taxi.ClearTaxiDestinations();

    // 0 element current node
    m_taxi.AddTaxiDestination(sourcenode);

    // fill destinations path tail
    uint32 sourcepath = 0;
    uint32 totalcost = 0;
    uint32 firstcost = 0;

    uint32 prevnode = sourcenode;
    uint32 lastnode;

    for (uint32 i = 1; i < nodes.size(); ++i)
    {
        uint32 path, cost;

        lastnode = nodes[i];
        sObjectMgr->GetTaxiPath(prevnode, lastnode, path, cost);

        if (!path)
        {
            m_taxi.ClearTaxiDestinations();
            return false;
        }

        totalcost += cost;
        if (i == 1)
            firstcost = cost;

        if (prevnode == sourcenode)
            sourcepath = path;

        m_taxi.AddTaxiDestination(lastnode);

        prevnode = lastnode;
    }

    // get mount model (in case non taximaster (npc == NULL) allow more wide lookup)
    //
    // Hack-Fix for Alliance not being able to use Acherus taxi. There is
    // only one mount ID for both sides. Probably not good to use 315 in case DBC nodes
    // change but I couldn't find a suitable alternative. OK to use class because only DK
    // can use this taxi.
    uint32 mount_display_id = sObjectMgr->GetTaxiMountDisplayId(sourcenode, GetTeam(), npc == nullptr || (sourcenode == 315 && GetClass() == CLASS_DEATH_KNIGHT));

    // in spell case allow 0 model
    if ((mount_display_id == 0 && spellid == 0) || sourcepath == 0)
    {
        GetSession()->SendActivateTaxiReply(ERR_TAXIUNSPECIFIEDSERVERERROR);
        m_taxi.ClearTaxiDestinations();
        return false;
    }

    uint32 money = GetMoney();

    if (npc)
    {
        float discount = GetReputationPriceDiscount(npc);
        totalcost = uint32(ceil(totalcost * discount));
        firstcost = uint32(ceil(firstcost * discount));
        m_taxi.SetFlightMasterFactionTemplateId(npc->GetFaction());
    }
    else
        m_taxi.SetFlightMasterFactionTemplateId(0);

    if (money < totalcost)
    {
        GetSession()->SendActivateTaxiReply(ERR_TAXINOTENOUGHMONEY);
        m_taxi.ClearTaxiDestinations();
        return false;
    }

    //Checks and preparations done, DO FLIGHT
    UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_FLIGHT_PATHS_TAKEN, 1);

    // prevent stealth flight
    //RemoveAurasWithInterruptFlags(AURA_INTERRUPT_FLAG_TALK);

    if (sWorld->getBoolConfig(CONFIG_INSTANT_TAXI))
    {
        TaxiNodesEntry const* lastPathNode = sTaxiNodesStore.LookupEntry(nodes[nodes.size()-1]);
        ASSERT(lastPathNode);
        m_taxi.ClearTaxiDestinations();
        ModifyMoney(-(int32)totalcost);
        UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_GOLD_SPENT_FOR_TRAVELLING, totalcost);
        TeleportTo(lastPathNode->ContinentID, lastPathNode->Pos.X, lastPathNode->Pos.Y, lastPathNode->Pos.Z, GetOrientation());
        return false;
    }
    else
    {
        ModifyMoney(-(int32)firstcost);
        UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_GOLD_SPENT_FOR_TRAVELLING, firstcost);
        GetSession()->SendActivateTaxiReply(ERR_TAXIOK);
        GetSession()->SendDoFlight(mount_display_id, sourcepath);
    }
    return true;
}

bool Player::ActivateTaxiPathTo(uint32 taxi_path_id, uint32 spellid /*= 0*/)
{
    TaxiPathEntry const* entry = sTaxiPathStore.LookupEntry(taxi_path_id);
    if (!entry)
        return false;

    std::vector<uint32> nodes;

    nodes.resize(2);
    nodes[0] = entry->FromTaxiNode;
    nodes[1] = entry->ToTaxiNode;

    return ActivateTaxiPathTo(nodes, nullptr, spellid);
}

void Player::FinishTaxiFlight()
{
    if (!IsInFlight())
        return;

    GetMotionMaster()->Remove(FLIGHT_MOTION_TYPE);
    m_taxi.ClearTaxiDestinations(); // not destinations, clear source node
}

void Player::CleanupAfterTaxiFlight()
{
    m_taxi.ClearTaxiDestinations(); // not destinations, clear source node
    Dismount();
    RemoveUnitFlag(UNIT_FLAG_REMOVE_CLIENT_CONTROL | UNIT_FLAG_ON_TAXI);
}

void Player::ContinueTaxiFlight() const
{
    uint32 sourceNode = m_taxi.GetTaxiSource();
    if (!sourceNode)
        return;

    TC_LOG_DEBUG("entities.unit", "Player::ContinueTaxiFlight: Restart {} taxi flight", GetGUID().ToString());

    uint32 mountDisplayId = sObjectMgr->GetTaxiMountDisplayId(sourceNode, GetTeam(), true);
    if (!mountDisplayId)
        return;

    uint32 path = m_taxi.GetCurrentTaxiPath();

    // search appropriate start path node
    uint32 startNode = 0;

    TaxiPathNodeList const& nodeList = sTaxiPathNodesByPath[path];

    float distPrev;
    float distNext = GetExactDistSq(nodeList[0]->Loc.X, nodeList[0]->Loc.Y, nodeList[0]->Loc.Z);

    for (uint32 i = 1; i < nodeList.size(); ++i)
    {
        TaxiPathNodeEntry const* node = nodeList[i];
        TaxiPathNodeEntry const* prevNode = nodeList[i-1];

        // skip nodes at another map
        if (node->ContinentID != GetMapId())
            continue;

        distPrev = distNext;

        distNext = GetExactDistSq(node->Loc.X, node->Loc.Y, node->Loc.Z);

        float distNodes =
            (node->Loc.X - prevNode->Loc.X)*(node->Loc.X - prevNode->Loc.X) +
            (node->Loc.Y - prevNode->Loc.Y)*(node->Loc.Y - prevNode->Loc.Y) +
            (node->Loc.Z - prevNode->Loc.Z)*(node->Loc.Z - prevNode->Loc.Z);

        if (distNext + distPrev < distNodes)
        {
            startNode = i;
            break;
        }
    }

    GetSession()->SendDoFlight(mountDisplayId, path, startNode);
}

void Player::SendTaxiNodeStatusMultiple()
{
    for (auto itr = m_clientGUIDs.begin(); itr != m_clientGUIDs.end(); ++itr)
    {
        if (!itr->IsCreature())
            continue;
        Creature* creature = ObjectAccessor::GetCreature(*this, *itr);
        if (!creature || creature->IsHostileTo(this))
            continue;
        if (!creature->HasNpcFlag(UNIT_NPC_FLAG_FLIGHTMASTER))
            continue;
        uint32 nearestNode = sObjectMgr->GetNearestTaxiNode(creature->GetPositionX(), creature->GetPositionY(), creature->GetPositionZ(), creature->GetMapId(), GetTeam());
        if (!nearestNode)
            continue;
        WorldPacket data(SMSG_TAXINODE_STATUS, 9);
        data << *itr;
        data << uint8(m_taxi.IsTaximaskNodeKnown(nearestNode) ? 1 : 0);
        SendDirectMessage(&data);
    }
}

void Player::InitDataForForm(bool reapplyMods)
{
    ShapeshiftForm form = GetShapeshiftForm();

    SpellShapeshiftFormEntry const* ssEntry = sSpellShapeshiftFormStore.LookupEntry(form);
    if (ssEntry && ssEntry->CombatRoundTime)
    {
        SetAttackTime(BASE_ATTACK, ssEntry->CombatRoundTime);
        SetAttackTime(OFF_ATTACK, ssEntry->CombatRoundTime);
        SetAttackTime(RANGED_ATTACK, BASE_ATTACK_TIME);
    }
    else
        SetRegularAttackTime();

    UpdateDisplayPower();

    // update auras at form change, ignore this at mods reapply (.reset stats/etc) when form not change.
    if (!reapplyMods)
        UpdateEquipSpellsAtFormChange();

    UpdateAttackPowerAndDamage();
    UpdateAttackPowerAndDamage(true);
}

void Player::InitDisplayIds()
{
    PlayerInfo const* info = sObjectMgr->GetPlayerInfo(GetRace(), GetClass());
    if (!info)
    {
        TC_LOG_ERROR("entities.player", "Player::InitDisplayIds: Player '{}' ({}) has incorrect race/class pair. Can't init display ids.", GetName(), GetGUID().ToString());
        return;
    }

    uint8 gender = GetNativeGender();
    switch (gender)
    {
        case GENDER_FEMALE:
            SetDisplayId(info->displayId_f);
            SetNativeDisplayId(info->displayId_f);
            break;
        case GENDER_MALE:
            SetDisplayId(info->displayId_m);
            SetNativeDisplayId(info->displayId_m);
            break;
        default:
            TC_LOG_ERROR("entities.player", "Player::InitDisplayIds: Player '{}' ({}) has invalid gender {}", GetName(), GetGUID().ToString(), gender);
    }
}

uint32 Player::GetMaxPersonalArenaRatingRequirement(uint32 minarenaslot) const
{
    // returns the maximal personal arena rating that can be used to purchase items requiring this condition
    // the personal rating of the arena team must match the required limit as well
    // so return max[in arenateams](min(personalrating[teamtype], teamrating[teamtype]))
    uint32 max_personal_rating = 0;
    for (uint8 i = minarenaslot; i < MAX_ARENA_SLOT; ++i)
    {
        if (ArenaTeam* at = sArenaTeamMgr->GetArenaTeamById(GetArenaTeamId(i)))
        {
            uint32 p_rating = GetArenaPersonalRating(i);
            uint32 t_rating = at->GetRating();
            p_rating = p_rating < t_rating ? p_rating : t_rating;
            if (max_personal_rating < p_rating)
                max_personal_rating = p_rating;
        }
    }
    return max_personal_rating;
}

void Player::UpdateHomebindTime(uint32 time)
{
    // GMs never get homebind timer online
    if (m_InstanceValid || IsGameMaster())
    {
        if (m_HomebindTimer)                                 // instance valid, but timer not reset
        {
            // hide reminder
            WorldPacket data(SMSG_RAID_GROUP_ONLY, 4+4);
            data << uint32(0);
            data << uint32(0);
            SendDirectMessage(&data);
        }
        // instance is valid, reset homebind timer
        m_HomebindTimer = 0;
    }
    else if (m_HomebindTimer > 0)
    {
        if (time >= m_HomebindTimer)
        {
            // teleport to nearest graveyard
            RepopAtGraveyard();
        }
        else
            m_HomebindTimer -= time;
    }
    else
    {
        // instance is invalid, start homebind timer
        m_HomebindTimer = 60000;
        // send message to player
        WorldPacket data(SMSG_RAID_GROUP_ONLY, 4+4);
        data << uint32(m_HomebindTimer);
        data << uint32(1);
        SendDirectMessage(&data);
        TC_LOG_DEBUG("maps", "Player::UpdateHomebindTime: Player '{}' ({}) will be teleported to homebind in 60 seconds",
            GetName(), GetGUID().ToString());
    }
}

void Player::UpdatePotionCooldown(Spell* spell)
{
    // no potion used i combat or still in combat
    if (!m_lastPotionId || IsInCombat())
        return;

    // Call not from spell cast, send cooldown event for item spells if no in combat
    if (!spell)
    {
        // spell/item pair let set proper cooldown (except non-existing charged spell cooldown spellmods for potions)
        if (ItemTemplate const* proto = sObjectMgr->GetItemTemplate(m_lastPotionId))
            for (uint8 idx = 0; idx < MAX_ITEM_PROTO_SPELLS; ++idx)
                if (proto->Spells[idx].SpellId && proto->Spells[idx].SpellTrigger == ITEM_SPELLTRIGGER_ON_USE)
                    if (SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(proto->Spells[idx].SpellId))
                        GetSpellHistory()->SendCooldownEvent(spellInfo, m_lastPotionId);
    }
    // from spell cases (m_lastPotionId set in Spell::SendSpellCooldown)
    else
    {
        if (spell->IsIgnoringCooldowns())
            return;
        else
            GetSpellHistory()->SendCooldownEvent(spell->m_spellInfo, m_lastPotionId, spell);
    }

    m_lastPotionId = 0;
}

void Player::SetResurrectRequestData(WorldObject const* caster, uint32 health, uint32 mana, uint32 appliedAura)
{
    ASSERT(!IsResurrectRequested());
    _resurrectionData.reset(new ResurrectionData());
    _resurrectionData->GUID = caster->GetGUID();
    _resurrectionData->Location.WorldRelocate(*caster);
    _resurrectionData->Health = health;
    _resurrectionData->Mana = mana;
    _resurrectionData->Aura = appliedAura;
}

WorldLocation Player::GetStartPosition() const
{
    PlayerInfo const* info = sObjectMgr->GetPlayerInfo(GetRace(), GetClass());
    ASSERT(info);
    uint32 mapId = info->mapId;
    if (GetClass() == CLASS_DEATH_KNIGHT && HasSpell(50977))
        mapId = 0;
    return WorldLocation(mapId, info->positionX, info->positionY, info->positionZ, 0);
}

bool Player::HaveAtClient(Object const* u) const
{
    return u == this || m_clientGUIDs.find(u->GetGUID()) != m_clientGUIDs.end();
}

bool Player::IsNeverVisible(bool allowServersideObjects) const
{
    if (Unit::IsNeverVisible(allowServersideObjects))
        return true;

    if (GetSession()->PlayerLogout() || GetSession()->PlayerLoading())
        return true;

    return false;
}

bool Player::CanAlwaysSee(WorldObject const* obj) const
{
    // Always can see self
    if (GetCharmedOrSelf() == obj)
        return true;

    if (ObjectGuid guid = GetGuidValue(PLAYER_FARSIGHT))
        if (obj->GetGUID() == guid)
            return true;

    return false;
}

bool Player::IsAlwaysDetectableFor(WorldObject const* seer) const
{
    if (Unit::IsAlwaysDetectableFor(seer))
        return true;

    if (duel && duel->State != DUEL_STATE_CHALLENGED && duel->Opponent == seer)
        return false;

    if (Player const* seerPlayer = seer->ToPlayer())
        if (IsGroupVisibleFor(seerPlayer))
            return true;

    return false;
}

bool Player::IsVisibleGloballyFor(Player const* u) const
{
    if (!u)
        return false;

    // Always can see self
    if (u == this)
        return true;

    // Visible units, always are visible for all players
    if (IsVisible())
        return true;

    // GMs are visible for higher gms (or players are visible for gms)
    if (!AccountMgr::IsPlayerAccount(u->GetSession()->GetSecurity()))
        return GetSession()->GetSecurity() <= u->GetSession()->GetSecurity();

    // non faction visibility non-breakable for non-GMs
    return false;
}

template<class T>
inline void UpdateVisibilityOf_helper(GuidUnorderedSet& s64, T* target, std::set<Unit*>& /*v*/)
{
    s64.insert(target->GetGUID());
}

template<>
inline void UpdateVisibilityOf_helper(GuidUnorderedSet& s64, GameObject* target, std::set<Unit*>& /*v*/)
{
    // @HACK: This is to prevent objects like deeprun tram from disappearing when player moves far from its spawn point while riding it
    if ((target->GetGOInfo()->type != GAMEOBJECT_TYPE_TRANSPORT))
        s64.insert(target->GetGUID());
}

template<>
inline void UpdateVisibilityOf_helper(GuidUnorderedSet& s64, Creature* target, std::set<Unit*>& v)
{
    s64.insert(target->GetGUID());
    v.insert(target);
}

template<>
inline void UpdateVisibilityOf_helper(GuidUnorderedSet& s64, Player* target, std::set<Unit*>& v)
{
    s64.insert(target->GetGUID());
    v.insert(target);
}

template<class T>
inline void BeforeVisibilityDestroy(T* /*t*/, Player* /*p*/) { }

template<>
inline void BeforeVisibilityDestroy<Creature>(Creature* t, Player* p)
{
    if (p->GetPetGUID() == t->GetGUID() && t->IsPet())
        t->ToPet()->Remove(PET_SAVE_NOT_IN_SLOT, true);
}

void Player::UpdateVisibilityOf(WorldObject* target)
{
    if (HaveAtClient(target))
    {
        if (!CanSeeOrDetect(target, false, true))
        {
            if (target->GetTypeId() == TYPEID_UNIT)
                BeforeVisibilityDestroy<Creature>(target->ToCreature(), this);

            target->DestroyForPlayer(this);
            m_clientGUIDs.erase(target->GetGUID());

            #ifdef TRINITY_DEBUG
                TC_LOG_DEBUG("maps", "Object {} out of range for player {}. Distance = {}", target->GetGUID().ToString(), GetGUID().ToString(), GetDistance(target));
            #endif
        }
    }
    else
    {
        if (CanSeeOrDetect(target, false, true))
        {
            target->SendUpdateToPlayer(this);
            m_clientGUIDs.insert(target->GetGUID());

            #ifdef TRINITY_DEBUG
                TC_LOG_DEBUG("maps", "Object {} is visible now for player {}. Distance = {}", target->GetGUID().ToString(), GetGUID().ToString(), GetDistance(target));
            #endif

            // target aura duration for caster show only if target exist at caster client
            // send data at target visibility change (adding to client)
            if (target->IsUnit())
                SendInitialVisiblePackets(static_cast<Unit*>(target));
        }
    }
}

void Player::UpdateTriggerVisibility()
{
    if (m_clientGUIDs.empty())
        return;

    if (!IsInWorld())
        return;

    UpdateData udata;
    WorldPacket packet;
    for (auto itr = m_clientGUIDs.begin(); itr != m_clientGUIDs.end(); ++itr)
    {
        if (itr->IsCreatureOrVehicle())
        {
            Creature* creature = GetMap()->GetCreature(*itr);
            // Update fields of triggers, transformed units or uninteractible units (values dependent on GM state)
            if (!creature || (!creature->IsTrigger() && !creature->HasAuraType(SPELL_AURA_TRANSFORM) && !creature->HasUnitFlag(UNIT_FLAG_UNINTERACTIBLE)))
                continue;

            creature->SetFieldNotifyFlag(UF_FLAG_PUBLIC);
            creature->BuildValuesUpdateBlockForPlayer(&udata, this);
            creature->RemoveFieldNotifyFlag(UF_FLAG_PUBLIC);
        }
        else if (itr->IsGameObject())
        {
            GameObject* go = GetMap()->GetGameObject(*itr);
            if (!go)
                continue;

            go->SetFieldNotifyFlag(UF_FLAG_PUBLIC);
            go->BuildValuesUpdateBlockForPlayer(&udata, this);
            go->RemoveFieldNotifyFlag(UF_FLAG_PUBLIC);
        }
    }

    if (!udata.HasData())
        return;

    udata.BuildPacket(&packet);
    SendDirectMessage(&packet);
}

void Player::SendInitialVisiblePackets(Unit* target) const
{
    SendAurasForTarget(target);
    if (target->IsAlive())
    {
        if (target->HasUnitState(UNIT_STATE_MELEE_ATTACKING) && target->GetVictim())
            target->SendMeleeAttackStart(target->GetVictim());
    }
}

template<class T>
void Player::UpdateVisibilityOf(T* target, UpdateData& data, std::set<Unit*>& visibleNow)
{
    if (HaveAtClient(target))
    {
        if (!CanSeeOrDetect(target, false, true))
        {
            BeforeVisibilityDestroy<T>(target, this);

            target->BuildOutOfRangeUpdateBlock(&data);
            m_clientGUIDs.erase(target->GetGUID());

            #ifdef TRINITY_DEBUG
                TC_LOG_DEBUG("maps", "Object {} is out of range for player {}. Distance = {}", target->GetGUID().ToString(), GetGUID().ToString(), GetDistance(target));
            #endif
        }
    }
    else //if (visibleNow.size() < 30 || target->GetTypeId() == TYPEID_UNIT && target->ToCreature()->IsVehicle())
    {
        if (CanSeeOrDetect(target, false, true))
        {
            target->BuildCreateUpdateBlockForPlayer(&data, this);
            UpdateVisibilityOf_helper(m_clientGUIDs, target, visibleNow);

            #ifdef TRINITY_DEBUG
                TC_LOG_DEBUG("maps", "Object {} is visible now for player {}. Distance = {}", target->GetGUID().ToString(), GetGUID().ToString(), GetDistance(target));
            #endif
        }
    }
}

template void Player::UpdateVisibilityOf(Player*        target, UpdateData& data, std::set<Unit*>& visibleNow);
template void Player::UpdateVisibilityOf(Creature*      target, UpdateData& data, std::set<Unit*>& visibleNow);
template void Player::UpdateVisibilityOf(Corpse*        target, UpdateData& data, std::set<Unit*>& visibleNow);
template void Player::UpdateVisibilityOf(GameObject*    target, UpdateData& data, std::set<Unit*>& visibleNow);
template void Player::UpdateVisibilityOf(DynamicObject* target, UpdateData& data, std::set<Unit*>& visibleNow);

void Player::UpdateObjectVisibility(bool forced)
{
    // Prevent updating visibility if player is not in world (example: LoadFromDB sets drunkstate which updates invisibility while player is not in map)
    if (!IsInWorld())
        return;

    if (!forced)
        AddToNotify(NOTIFY_VISIBILITY_CHANGED);
    else
    {
        Unit::UpdateObjectVisibility(true);
        UpdateVisibilityForPlayer();
    }
}

void Player::UpdateVisibilityForPlayer()
{
    // updates visibility of all objects around point of view for current player
    Trinity::VisibleNotifier notifier(*this);
    Cell::VisitAllObjects(m_seer, notifier, GetSightRange());
    notifier.SendToSelf();   // send gathered data
}

// @tswow-begin
void Player::SetPhaseMask(uint32 newPhaseMask, bool update, uint64 newPhaseId)
{
    if (newPhaseMask == GetPhaseMask() && newPhaseId == m_phase_id)
        return;

    Unit::SetPhaseMask(newPhaseMask, false, newPhaseId);

    if (Unit* vehicle = GetVehicleRoot())
        vehicle->SetPhaseMask(newPhaseMask, update, newPhaseId);

    if (update)
        UpdateObjectVisibility();
}
// @tswow-end

Unit* Player::GetSelectedUnit() const
{
    if (ObjectGuid selectionGUID = GetTarget())
        return ObjectAccessor::GetUnit(*this, selectionGUID);
    return nullptr;
}

Player* Player::GetSelectedPlayer() const
{
    if (ObjectGuid selectionGUID = GetTarget())
        return ObjectAccessor::FindConnectedPlayer(selectionGUID);
    return nullptr;
}

void Player::SetGroup(Group* group, int8 subgroup)
{
    if (group == nullptr)
        m_group.unlink();
    else
    {
        // never use SetGroup without a subgroup unless you specify NULL for group
        ASSERT(subgroup >= 0);
        m_group.link(group, this);
        m_group.setSubGroup((uint8)subgroup);
    }

    UpdateObjectVisibility(false);
}

void Player::SendUpdateToOutOfRangeGroupMembers()
{
    if (m_groupUpdateMask == GROUP_UPDATE_FLAG_NONE)
        return;
    if (Group* group = GetGroup())
        group->UpdatePlayerOutOfRange(this);

    m_groupUpdateMask = GROUP_UPDATE_FLAG_NONE;
    m_auraRaidUpdateMask = 0;
    if (Pet* pet = GetPet())
        pet->ResetAuraUpdateMaskForRaid();
}

void Player::SendTransferAborted(uint32 mapid, TransferAbortReason reason, uint8 arg) const
{
    WorldPacket data(SMSG_TRANSFER_ABORTED, 4+2);
    data << uint32(mapid);
    data << uint8(reason);                                 // transfer abort reason
    switch (reason)
    {
        case TRANSFER_ABORT_INSUF_EXPAN_LVL:
        case TRANSFER_ABORT_DIFFICULTY:
        case TRANSFER_ABORT_UNIQUE_MESSAGE:
            // these are the ONLY cases that have an extra argument in the packet!!!
            data << uint8(arg);
            break;
        default:
            break;
    }
    SendDirectMessage(&data);
}

void Player::SendInstanceResetWarning(uint32 mapid, Difficulty difficulty, uint32 time, bool welcome) const
{
    // type of warning, based on the time remaining until reset
    uint32 type;
    if (welcome)
        type = RAID_INSTANCE_WELCOME;
    else if (time > 21600)
        type = RAID_INSTANCE_WELCOME;
    else if (time > 3600)
        type = RAID_INSTANCE_WARNING_HOURS;
    else if (time > 300)
        type = RAID_INSTANCE_WARNING_MIN;
    else
        type = RAID_INSTANCE_WARNING_MIN_SOON;

    WorldPacket data(SMSG_RAID_INSTANCE_MESSAGE, 4+4+4+4);
    data << uint32(type);
    data << uint32(mapid);
    data << uint32(difficulty);                             // difficulty
    data << uint32(time);
    if (type == RAID_INSTANCE_WELCOME)
    {
        data << uint8(0);                                   // is locked
        data << uint8(0);                                   // is extended, ignored if prev field is 0
    }
    SendDirectMessage(&data);
}

void Player::SendAurasForTarget(Unit* target, bool force /*= false*/) const
{
    if (!target || (!force && target->GetVisibleAuras().empty()))                  // speedup things
        return;

    WorldPacket data(SMSG_AURA_UPDATE_ALL);
    data << target->GetPackGUID();

    Unit::VisibleAuraMap const& visibleAuras = target->GetVisibleAuras();
    for (Unit::VisibleAuraMap::const_iterator itr = visibleAuras.begin(); itr != visibleAuras.end(); ++itr)
    {
        AuraApplication * auraApp = itr->second;
        auraApp->BuildUpdatePacket(data, false);
    }

    SendDirectMessage(&data);
}

float Player::GetReputationPriceDiscount(Creature const* creature) const
{
    return GetReputationPriceDiscount(creature->GetFactionTemplateEntry(), creature);
}

// @tswow-begin rewrite flow for hook
float Player::GetReputationPriceDiscount(FactionTemplateEntry const* factionTemplate, Creature const* creature) const
{
    float money;
    if (!factionTemplate || !factionTemplate->Faction)
        money = 1.0f;
    else {
        ReputationRank rank = GetReputationRank(factionTemplate->Faction);
        if (rank <= REP_NEUTRAL)
            money = 1.0f;
        else
            money = 1.0f - 0.05f * (rank - REP_NEUTRAL);

        FIRE(Player, OnReputationPriceDiscount
            , TSPlayer(const_cast<Player*>(this))
            , TSFactionTemplate(factionTemplate)
            , TSCreature(const_cast<Creature*>(creature))
            , TSMutableNumber<float>(&money)
        );
    }

    return money;
}
// @tswow-end

bool Player::HasQuestForGO(int32 GOId) const
{
    for (uint8 i = 0; i < MAX_QUEST_LOG_SIZE; ++i)
    {
        uint32 questid = GetQuestSlotQuestId(i);
        if (questid == 0)
            continue;

        QuestStatusMap::const_iterator qs_itr = m_QuestStatus.find(questid);
        if (qs_itr == m_QuestStatus.end())
            continue;

        QuestStatusData const& qs = qs_itr->second;

        if (qs.Status == QUEST_STATUS_INCOMPLETE)
        {
            Quest const* qinfo = sObjectMgr->GetQuestTemplate(questid);
            if (!qinfo)
                continue;

            if (GetGroup() && GetGroup()->isRaidGroup() && !qinfo->IsAllowedInRaid(GetMap()->GetDifficulty()))
                continue;

            for (uint8 j = 0; j < QUEST_OBJECTIVES_COUNT; ++j)
            {
                if (qinfo->RequiredNpcOrGo[j] >= 0)       //skip non GO case
                    continue;

                if ((-1)*GOId == qinfo->RequiredNpcOrGo[j] && qs.CreatureOrGOCount[j] < qinfo->RequiredNpcOrGoCount[j])
                    return true;
            }
        }
    }
    return false;
}

void Player::UpdateVisibleGameobjectsOrSpellClicks()
{
    if (m_clientGUIDs.empty())
        return;

    UpdateData udata;
    WorldPacket packet;
    for (auto itr = m_clientGUIDs.begin(); itr != m_clientGUIDs.end(); ++itr)
    {
        if (itr->IsGameObject())
        {
            if (GameObject* obj = ObjectAccessor::GetGameObject(*this, *itr))
                obj->BuildValuesUpdateBlockForPlayer(&udata, this);
        }
        else if (itr->IsCreatureOrVehicle())
        {
            Creature* obj = ObjectAccessor::GetCreatureOrPetOrVehicle(*this, *itr);
            if (!obj)
                continue;

            // check if this unit requires quest specific flags
            if (!obj->HasNpcFlag(UNIT_NPC_FLAG_SPELLCLICK))
                continue;

            auto clickBounds = sObjectMgr->GetSpellClickInfoMapBounds(obj->GetEntry());
            for (auto const& clickPair : clickBounds)
            {
                if (sConditionMgr->GetConditionsForSpellClickEvent(obj->GetEntry(), clickPair.second.spellId))
                {
                    obj->BuildValuesUpdateBlockForPlayer(&udata, this);
                    break;
                }
            }
        }
    }
    udata.BuildPacket(&packet);
    SendDirectMessage(&packet);
}

bool Player::HasSummonPending() const
{
    return m_summon_expire >= GameTime::GetGameTime();
}

void Player::SendSummonRequestFrom(Unit* summoner)
{
    if (!summoner)
        return;

    // Player already has active summon request
    if (HasSummonPending())
        return;

    // Evil Twin (ignore player summon, but hide this for summoner)
    if (HasAura(23445))
        return;

    m_summon_expire = GameTime::GetGameTime() + MAX_PLAYER_SUMMON_DELAY;
    m_summon_location.WorldRelocate(*summoner);

    WorldPacket data(SMSG_SUMMON_REQUEST, 8 + 4 + 4);
    data << uint64(summoner->GetGUID());                     // summoner guid
    data << uint32(summoner->GetZoneId());                   // summoner zone
    data << uint32(MAX_PLAYER_SUMMON_DELAY*IN_MILLISECONDS); // auto decline after msecs
    SendDirectMessage(&data);
}

void Player::SummonIfPossible(bool agree)
{
    // @epoch-start
    bool cancel = false;
    FIRE(
        Player
        , OnSummonIfPossible
        , TSPlayer(this)
        , TSMutable<bool, bool>(&cancel)
    );
    // @epoch-end

    if (!agree || cancel)
    {
        m_summon_expire = 0;
        return;
    }

    // expire and auto declined
    if (m_summon_expire < GameTime::GetGameTime())
        return;

    // stop taxi flight at summon
    FinishTaxiFlight();

    // drop flag at summon
    // this code can be reached only when GM is summoning player who carries flag, because player should be immune to summoning spells when he carries flag
    if (Battleground* bg = GetBattleground())
        bg->EventPlayerDroppedFlag(this);

    m_summon_expire = 0;

    UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_ACCEPTED_SUMMONINGS, 1);

    TeleportTo(m_summon_location);
}

bool Player::HasItemFitToSpellRequirements(SpellInfo const* spellInfo, Item const* ignoreItem) const
{
    if (spellInfo->EquippedItemClass < 0)
        return true;

    // scan other equipped items for same requirements (mostly 2 daggers/etc)
    // for optimize check 2 used cases only
    switch (spellInfo->EquippedItemClass)
    {
        case ITEM_CLASS_WEAPON:
        {
            for (uint8 i = EQUIPMENT_SLOT_MAINHAND; i < EQUIPMENT_SLOT_TABARD; ++i)
                if (Item* item = GetUseableItemByPos(INVENTORY_SLOT_BAG_0, i))
                    if (item != ignoreItem && item->IsFitToSpellRequirements(spellInfo))
                        return true;
            break;
        }
        case ITEM_CLASS_ARMOR:
        {
            // most used check: shield only
            if (spellInfo->EquippedItemSubClassMask & ((1 << ITEM_SUBCLASS_ARMOR_BUCKLER) | (1 << ITEM_SUBCLASS_ARMOR_SHIELD)))
            {
                if (Item* item = GetUseableItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_OFFHAND))
                    if (item != ignoreItem && item->IsFitToSpellRequirements(spellInfo))
                        return true;

                // special check to filter things like Shield Wall, the aura is not permanent and must stay even without required item
                if (!spellInfo->IsPassive())
                    for (SpellEffectInfo const& spellEffectInfo : spellInfo->GetEffects())
                        if (spellEffectInfo.IsAura())
                            return true;
            }

            // tabard not have dependent spells
            for (uint8 i = EQUIPMENT_SLOT_START; i < EQUIPMENT_SLOT_MAINHAND; ++i)
                if (Item* item = GetUseableItemByPos(INVENTORY_SLOT_BAG_0, i))
                    if (item != ignoreItem && item->IsFitToSpellRequirements(spellInfo))
                        return true;

            // ranged slot can have some armor subclasses
            if (Item* item = GetUseableItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_RANGED))
                if (item != ignoreItem && item->IsFitToSpellRequirements(spellInfo))
                    return true;
            break;
        }
        default:
            TC_LOG_ERROR("entities.player", "Player::HasItemFitToSpellRequirements: Not handled spell requirement for item class {}", spellInfo->EquippedItemClass);
            break;
    }

    return false;
}

bool Player::CanNoReagentCast(SpellInfo const* spellInfo) const
{
    // don't take reagents for spells with SPELL_ATTR5_NO_REAGENT_WHILE_PREP
    if (spellInfo->HasAttribute(SPELL_ATTR5_NO_REAGENT_WHILE_PREP) &&
        HasUnitFlag(UNIT_FLAG_PREPARATION))
        return true;

    // Check no reagent use mask
    flag96 noReagentMask;
    noReagentMask[0] = GetUInt32Value(PLAYER_NO_REAGENT_COST_1);
    noReagentMask[1] = GetUInt32Value(PLAYER_NO_REAGENT_COST_1+1);
    noReagentMask[2] = GetUInt32Value(PLAYER_NO_REAGENT_COST_1+2);
    if (spellInfo->SpellFamilyFlags  & noReagentMask)
        return true;

    return false;
}

void Player::RemoveItemDependentAurasAndCasts(Item* pItem)
{
    for (AuraMap::iterator itr = m_ownedAuras.begin(); itr != m_ownedAuras.end();)
    {
        Aura* aura = itr->second;

        // skip not self applied auras
        SpellInfo const* spellInfo = aura->GetSpellInfo();
        if (aura->GetCasterGUID() != GetGUID())
        {
            ++itr;
            continue;
        }

        // skip if not item dependent or have alternative item
        if (HasItemFitToSpellRequirements(spellInfo, pItem))
        {
            ++itr;
            continue;
        }

        // no alt item, remove aura, restart check
        RemoveOwnedAura(itr);
    }

    // currently cast spells can be dependent from item
    for (uint32 i = 0; i < CURRENT_MAX_SPELL; ++i)
        if (Spell* spell = GetCurrentSpell(CurrentSpellTypes(i)))
            if (spell->getState() != SPELL_STATE_DELAYED && !HasItemFitToSpellRequirements(spell->m_spellInfo, pItem))
                InterruptSpell(CurrentSpellTypes(i));
}

uint32 Player::GetResurrectionSpellId()
{
    // search priceless resurrection possibilities
    uint32 prio = 0;
    uint32 spell_id = 0;
    AuraEffectList const& dummyAuras = GetAuraEffectsByType(SPELL_AURA_DUMMY);
    for (AuraEffectList::const_iterator itr = dummyAuras.begin(); itr != dummyAuras.end(); ++itr)
    {
        // Soulstone Resurrection                           // prio: 3 (max, non death persistent)
        if (prio < 2 && (*itr)->GetSpellInfo()->SpellVisual[0] == 99 && (*itr)->GetSpellInfo()->SpellIconID == 92)
        {
            switch ((*itr)->GetId())
            {
                case 20707: spell_id =  3026; break;        // rank 1
                case 20762: spell_id = 20758; break;        // rank 2
                case 20763: spell_id = 20759; break;        // rank 3
                case 20764: spell_id = 20760; break;        // rank 4
                case 20765: spell_id = 20761; break;        // rank 5
                case 27239: spell_id = 27240; break;        // rank 6
                case 47883: spell_id = 47882; break;        // rank 7
                default:
                    TC_LOG_ERROR("entities.player", "Unhandled spell {}: S.Resurrection", (*itr)->GetId());
                    continue;
            }

            prio = 3;
        }
        // Twisting Nether                                  // prio: 2 (max)
        else if ((*itr)->GetId() == 23701 && roll_chance_i(10))
        {
            prio = 2;
            spell_id = 23700;
        }
    }

    // Reincarnation (passive spell)  // prio: 1                  // Glyph of Renewed Life
    if (prio < 1 && HasSpell(20608) && !GetSpellHistory()->HasCooldown(21169) && (HasAura(58059) || HasItemCount(17030)))
        spell_id = 21169;

    return spell_id;
}

// Used in triggers for check "Only to targets that grant experience or honor" req
bool Player::isHonorOrXPTarget(Unit* victim) const
{
    uint8 v_level = victim->GetLevel();
    // @tswow-begin player argument
    uint8 k_grey  = Trinity::XP::GetGrayLevel(const_cast<Player*>(this), GetLevel());
    // @tswow-end

    // Victim level less gray level
    if (v_level <= k_grey && !sWorld->getIntConfig(CONFIG_MIN_CREATURE_SCALED_XP_RATIO))
        return false;

    if (Creature const* creature = victim->ToCreature())
    {
        if (creature->IsCritter() || creature->IsTotem())
            return false;
    }
    return true;
}

bool Player::GetsRecruitAFriendBonus(bool forXP)
{
    bool recruitAFriend = false;
    if (GetLevel() <= sWorld->getIntConfig(CONFIG_MAX_RECRUIT_A_FRIEND_BONUS_PLAYER_LEVEL) || !forXP)
    {
        if (Group* group = GetGroup())
        {
            for (GroupReference* itr = group->GetFirstMember(); itr != nullptr; itr = itr->next())
            {
                Player* player = itr->GetSource();
                if (!player)
                    continue;

                if (!player->IsAtRecruitAFriendDistance(this))
                    continue;                               // member (alive or dead) or his corpse at req. distance

                if (forXP)
                {
                    // level must be allowed to get RaF bonus
                    if (player->GetLevel() > sWorld->getIntConfig(CONFIG_MAX_RECRUIT_A_FRIEND_BONUS_PLAYER_LEVEL))
                        continue;

                    // level difference must be small enough to get RaF bonus, UNLESS we are lower level
                    if (player->GetLevel() < GetLevel())
                        if (uint8(GetLevel() - player->GetLevel()) > sWorld->getIntConfig(CONFIG_MAX_RECRUIT_A_FRIEND_BONUS_PLAYER_LEVEL_DIFFERENCE))
                            continue;
                }

                bool ARecruitedB = (player->GetSession()->GetRecruiterId() == GetSession()->GetAccountId());
                bool BRecruitedA = (GetSession()->GetRecruiterId() == player->GetSession()->GetAccountId());
                if (ARecruitedB || BRecruitedA)
                {
                    recruitAFriend = true;
                    break;
                }
            }
        }
    }
    return recruitAFriend;
}

void Player::RewardPlayerAndGroupAtKill(Unit* victim, bool isBattleGround)
{
    KillRewarder(this, victim, isBattleGround).Reward();
}

void Player::RewardPlayerAndGroupAtEvent(uint32 creature_id, WorldObject* pRewardSource)
{
    if (!pRewardSource)
        return;

    ObjectGuid creature_guid = (pRewardSource->GetTypeId() == TYPEID_UNIT) ? pRewardSource->GetGUID() : ObjectGuid::Empty;

    // prepare data for near group iteration
    if (Group* group = GetGroup())
    {
        for (GroupReference* itr = group->GetFirstMember(); itr != nullptr; itr = itr->next())
        {
            Player* player = itr->GetSource();
            if (!player)
                continue;

            if (!player->IsAtGroupRewardDistance(pRewardSource))
                continue;                               // member (alive or dead) or his corpse at req. distance

            // quest objectives updated only for alive group member or dead but with not released body
            if (player->IsAlive()|| !player->GetCorpse())
                player->KilledMonsterCredit(creature_id, creature_guid);
        }
    }
    else                                                    // if (!group)
        KilledMonsterCredit(creature_id, creature_guid);
}

bool Player::IsAtGroupRewardDistance(WorldObject const* pRewardSource) const
{
    WorldObject const* player = GetCorpse();
    if (!player || IsAlive())
        player = this;

    if (!pRewardSource || !player->IsInMap(pRewardSource))
        return false;

    if (pRewardSource->GetMap()->IsDungeon())
        return true;

    return pRewardSource->GetDistance(player) <= sWorld->getFloatConfig(CONFIG_GROUP_XP_DISTANCE);
}

bool Player::IsAtLootRewardDistance(WorldObject const* pRewardSource) const
{
    if (!IsAtGroupRewardDistance(pRewardSource))
        return false;

    if (HasPendingBind())
        return false;

    return pRewardSource->HasAllowedLooter(GetGUID());
}

bool Player::IsAtRecruitAFriendDistance(WorldObject const* pOther) const
{
    if (!pOther || !IsInMap(pOther))
        return false;

    WorldObject const* player = GetCorpse();
    if (!player || IsAlive())
        player = this;

    return pOther->GetDistance(player) <= sWorld->getFloatConfig(CONFIG_MAX_RECRUIT_A_FRIEND_DISTANCE);
}

uint32 Player::GetBaseWeaponSkillValue(WeaponAttackType attType) const
{
    Item* item = GetWeaponForAttack(attType, true);

    // unarmed only with base attack
    if (attType != BASE_ATTACK && !item)
        return 0;

    // weapon skill or (unarmed for base attack)
    uint32  skill = item ? item->GetSkill() : uint32(SKILL_UNARMED);
    return GetBaseSkillValue(skill);
}

void Player::ResurrectUsingRequestData()
{
    RemoveGhoul();

    if (uint32 aura = _resurrectionData->Aura)
    {
        CastSpell(this, aura, _resurrectionData->GUID);
        return;
    }

    // Teleport before resurrecting by player, otherwise the player might get attacked from creatures near his corpse
    TeleportTo(_resurrectionData->Location);

    if (IsBeingTeleported())
    {
        ScheduleDelayedOperation(DELAYED_RESURRECT_PLAYER);
        return;
    }

    ResurrectUsingRequestDataImpl();
}

void Player::ResurrectUsingRequestDataImpl()
{
    // save health and mana before resurrecting, _resurrectionData can be erased
    uint32 resurrectHealth = _resurrectionData->Health;
    uint32 resurrectMana = _resurrectionData->Mana;

    ResurrectPlayer(0.0f, false);

    SetHealth(resurrectHealth);
    SetPower(POWER_MANA, resurrectMana);

    SetPower(POWER_RAGE, 0);
    SetFullPower(POWER_ENERGY);

    SpawnCorpseBones();
}

void Player::SetClientControl(Unit* target, bool allowMove)
{
    // a player can never client control nothing
    ASSERT(target);

    // don't allow possession to be overridden
    if (target->HasUnitState(UNIT_STATE_CHARMED) && (GetGUID() != target->GetCharmerGUID()))
    {
        TC_LOG_ERROR("entities.player", "Player '{}' attempt to client control '{}', which is charmed by GUID {}",
                     GetName(), target->GetName(), target->GetCharmerGUID().ToString());
        return;
    }

    // still affected by some aura that shouldn't allow control, only allow on last such aura to be removed
    if (target->HasUnitState(UNIT_STATE_FLEEING | UNIT_STATE_CONFUSED))
        allowMove = false;

    WorldPacket data(SMSG_CLIENT_CONTROL_UPDATE, target->GetPackGUID().size()+1);
    data << target->GetPackGUID();
    data << uint8(allowMove ? 1 : 0);
    SendDirectMessage(&data);

    WorldObject* viewpoint = GetViewpoint();
    if (!viewpoint)
        viewpoint = this;
    if (target != viewpoint)
    {
        if (viewpoint != this)
            SetViewpoint(viewpoint, false);
        if (target != this)
            SetViewpoint(target, true);
    }

    GetGameClient()->SetMovedUnit(target, allowMove);
}

uint32 Player::GetCorpseReclaimDelay(bool pvp) const
{
    if (pvp)
    {
        if (!sWorld->getBoolConfig(CONFIG_DEATH_CORPSE_RECLAIM_DELAY_PVP))
            return corpseReclaimDelay[0];
    }
    else if (!sWorld->getBoolConfig(CONFIG_DEATH_CORPSE_RECLAIM_DELAY_PVE))
        return 0;

    time_t now = GameTime::GetGameTime();
    // 0..2 full period
    // should be ceil(x)-1 but not floor(x)
    uint64 count = (now < m_deathExpireTime - 1) ? (m_deathExpireTime - 1 - now) / DEATH_EXPIRE_STEP : 0;
    return corpseReclaimDelay[count];
}

void Player::UpdateCorpseReclaimDelay()
{
    bool pvp = (m_ExtraFlags & PLAYER_EXTRA_PVP_DEATH) != 0;

    if ((pvp && !sWorld->getBoolConfig(CONFIG_DEATH_CORPSE_RECLAIM_DELAY_PVP)) ||
        (!pvp && !sWorld->getBoolConfig(CONFIG_DEATH_CORPSE_RECLAIM_DELAY_PVE)))
        return;

    time_t now = GameTime::GetGameTime();

    if (now < m_deathExpireTime)
    {
        // full and partly periods 1..3
        uint64 count = (m_deathExpireTime - now) / DEATH_EXPIRE_STEP + 1;

        if (count < MAX_DEATH_COUNT)
            m_deathExpireTime = now+(count + 1) * DEATH_EXPIRE_STEP;
        else
            m_deathExpireTime = now + MAX_DEATH_COUNT*DEATH_EXPIRE_STEP;
    }
    else
        m_deathExpireTime = now + DEATH_EXPIRE_STEP;
}

int32 Player::CalculateCorpseReclaimDelay(bool load) const
{
    Corpse* corpse = GetCorpse();

    if (load && !corpse)
        return -1;

    bool pvp = corpse ? corpse->GetType() == CORPSE_RESURRECTABLE_PVP : (m_ExtraFlags & PLAYER_EXTRA_PVP_DEATH) != 0;

    uint32 delay;

    if (load)
    {
        if (corpse->GetGhostTime() > m_deathExpireTime)
            return -1;

        uint64 count = 0;

        if ((pvp && sWorld->getBoolConfig(CONFIG_DEATH_CORPSE_RECLAIM_DELAY_PVP)) ||
           (!pvp && sWorld->getBoolConfig(CONFIG_DEATH_CORPSE_RECLAIM_DELAY_PVE)))
        {
            count = (m_deathExpireTime - corpse->GetGhostTime()) / DEATH_EXPIRE_STEP;

            if (count >= MAX_DEATH_COUNT)
                count = MAX_DEATH_COUNT - 1;
        }

        time_t expected_time = corpse->GetGhostTime() + corpseReclaimDelay[count];
        time_t now = GameTime::GetGameTime();

        if (now >= expected_time)
            return -1;

        delay = expected_time - now;
    }
    else
        delay = GetCorpseReclaimDelay(pvp);

    return delay * IN_MILLISECONDS;
}

void Player::SendCorpseReclaimDelay(uint32 delay) const
{
    WorldPackets::Misc::CorpseReclaimDelay packet;
    packet.Remaining = delay;
    GetSession()->SendPacket(packet.Write());
}

Player* Player::GetNextRandomRaidMember(float radius)
{
    Group* group = GetGroup();
    if (!group)
        return nullptr;

    std::vector<Player*> nearMembers;
    nearMembers.reserve(group->GetMembersCount());

    for (GroupReference* itr = group->GetFirstMember(); itr != nullptr; itr = itr->next())
    {
        Player* Target = itr->GetSource();

        // IsHostileTo check duel and controlled by enemy
        if (Target && Target != this && IsWithinDistInMap(Target, radius) &&
            !Target->HasInvisibilityAura() && !IsHostileTo(Target))
            nearMembers.push_back(Target);
    }

    if (nearMembers.empty())
        return nullptr;

    uint32 randTarget = urand(0, nearMembers.size()-1);
    return nearMembers[randTarget];
}

PartyResult Player::CanUninviteFromGroup(ObjectGuid guidMember) const
{
    Group const* grp = GetGroup();
    if (!grp)
        return ERR_NOT_IN_GROUP;

    if (grp->isLFGGroup())
    {
        ObjectGuid gguid = grp->GetGUID();
        if (!sLFGMgr->GetKicksLeft(gguid))
            return ERR_PARTY_LFG_BOOT_LIMIT;

        lfg::LfgState state = sLFGMgr->GetState(gguid);
        if (sLFGMgr->IsVoteKickActive(gguid))
            return ERR_PARTY_LFG_BOOT_IN_PROGRESS;

        if (grp->GetMembersCount() <= lfg::LFG_GROUP_KICK_VOTES_NEEDED)
            return ERR_PARTY_LFG_BOOT_TOO_FEW_PLAYERS;

        if (state == lfg::LFG_STATE_FINISHED_DUNGEON)
            return ERR_PARTY_LFG_BOOT_DUNGEON_COMPLETE;

        if (grp->isRollLootActive())
            return ERR_PARTY_LFG_BOOT_LOOT_ROLLS;

        /// @todo Should also be sent when anyone has recently left combat, with an aprox ~5 seconds timer.
        for (GroupReference const* itr = grp->GetFirstMember(); itr != nullptr; itr = itr->next())
            if (itr->GetSource() && itr->GetSource()->IsInMap(this) && itr->GetSource()->IsInCombat())
                return ERR_PARTY_LFG_BOOT_IN_COMBAT;

        /* Missing support for these types
            return ERR_PARTY_LFG_BOOT_COOLDOWN_S;
            return ERR_PARTY_LFG_BOOT_NOT_ELIGIBLE_S;
        */
    }
    else
    {
        if (!grp->IsLeader(GetGUID()) && !grp->IsAssistant(GetGUID()))
            return ERR_NOT_LEADER;

        if (InBattleground())
            return ERR_INVITE_RESTRICTED;

        if (grp->IsLeader(guidMember))
            return ERR_NOT_LEADER;
    }

    return ERR_PARTY_RESULT_OK;
}

bool Player::isUsingLfg() const
{
    return sLFGMgr->GetState(GetGUID()) != lfg::LFG_STATE_NONE;
}

bool Player::inRandomLfgDungeon() const
{
    if (sLFGMgr->selectedRandomLfgDungeon(GetGUID()))
    {
        Map const* map = GetMap();
        return sLFGMgr->inLfgDungeonMap(GetGUID(), map->GetId(), map->GetDifficulty());
    }

    return false;
}

void Player::SetBattlegroundOrBattlefieldRaid(Group* group, int8 subgroup)
{
    //we must move references from m_group to m_originalGroup
    SetOriginalGroup(GetGroup(), GetSubGroup());

    m_group.unlink();
    m_group.link(group, this);
    m_group.setSubGroup((uint8)subgroup);
}

void Player::RemoveFromBattlegroundOrBattlefieldRaid()
{
    //remove existing reference
    m_group.unlink();
    if (Group* group = GetOriginalGroup())
    {
        m_group.link(group, this);
        m_group.setSubGroup(GetOriginalSubGroup());
    }
    SetOriginalGroup(nullptr);
}

void Player::SetOriginalGroup(Group* group, int8 subgroup)
{
    if (group == nullptr)
        m_originalGroup.unlink();
    else
    {
        // never use SetOriginalGroup without a subgroup unless you specify NULL for group
        ASSERT(subgroup >= 0);
        m_originalGroup.link(group, this);
        m_originalGroup.setSubGroup((uint8)subgroup);
    }
}

void Player::ProcessTerrainStatusUpdate(ZLiquidStatus oldLiquidStatus, Optional<LiquidData> const& newLiquidData)
{
    // process liquid auras using generic unit code
    Unit::ProcessTerrainStatusUpdate(oldLiquidStatus, newLiquidData);

    // player specific logic for mirror timers
    if (GetLiquidStatus() && newLiquidData)
    {
        // Breath bar state (under water in any liquid type)
        if (newLiquidData->type_flags & MAP_ALL_LIQUIDS)
        {
            if (GetLiquidStatus() & LIQUID_MAP_UNDER_WATER)
                m_MirrorTimerFlags |= UNDERWATER_INWATER;
            else
                m_MirrorTimerFlags &= ~UNDERWATER_INWATER;
        }

        // Fatigue bar state (if not on flight path or transport)
        if ((newLiquidData->type_flags & MAP_LIQUID_TYPE_DARK_WATER) && !IsInFlight() && !GetTransport())
            m_MirrorTimerFlags |= UNDERWATER_INDARKWATER;
        else
            m_MirrorTimerFlags &= ~UNDERWATER_INDARKWATER;

        // Lava state (any contact)
        if (newLiquidData->type_flags & MAP_LIQUID_TYPE_MAGMA)
        {
            if (GetLiquidStatus() & MAP_LIQUID_STATUS_IN_CONTACT)
                m_MirrorTimerFlags |= UNDERWATER_INLAVA;
            else
                m_MirrorTimerFlags &= ~UNDERWATER_INLAVA;
        }

        // Slime state (any contact)
        if (newLiquidData->type_flags & MAP_LIQUID_TYPE_SLIME)
        {
            if (GetLiquidStatus() & MAP_LIQUID_STATUS_IN_CONTACT)
                m_MirrorTimerFlags |= UNDERWATER_INSLIME;
            else
                m_MirrorTimerFlags &= ~UNDERWATER_INSLIME;
        }
    }
    else
        m_MirrorTimerFlags &= ~(UNDERWATER_INWATER | UNDERWATER_INLAVA | UNDERWATER_INSLIME | UNDERWATER_INDARKWATER);
}

void Player::AtExitCombat()
{
    Unit::AtExitCombat();
    /** @epoch-start */
    // UpdatePotionCooldown();
    /** @epoch-end */

    // @tswow-begin
    if (HasRunes())
    {
        for (uint8 i = 0; i < MAX_RUNES; ++i)
        {
            SetRuneTimer(i, 0xFFFFFFFF);
            SetLastRuneGraceTimer(i, 0);
        }
    }
    // @tswow-end
}

void Player::SetCanParry(bool value)
{
    if (m_canParry == value)
        return;

    m_canParry = value;
    UpdateParryPercentage();
}

void Player::SetCanBlock(bool value)
{
    if (m_canBlock == value)
        return;

    m_canBlock = value;
    UpdateBlockPercentage();
}

bool ItemPosCount::isContainedIn(std::vector<ItemPosCount> const& vec) const
{
    for (ItemPosCountVec::const_iterator itr = vec.begin(); itr != vec.end(); ++itr)
        if (itr->pos == pos)
            return true;
    return false;
}

void Player::StopCastingBindSight() const
{
    if (Unit* target = Object::ToUnit(GetViewpoint()))
    {
        target->RemoveAurasByType(SPELL_AURA_BIND_SIGHT, GetGUID());
        target->RemoveAurasByType(SPELL_AURA_MOD_POSSESS, GetGUID());
        target->RemoveAurasByType(SPELL_AURA_MOD_POSSESS_PET, GetGUID());
    }
}

void Player::SetViewpoint(WorldObject* target, bool apply)
{
    if (apply)
    {
        TC_LOG_DEBUG("maps", "Player::CreateViewpoint: Player '{}' ({}) creates seer (Entry: {}, TypeId: {}).",
            GetName(), GetGUID().ToString(), target->GetEntry(), target->GetTypeId());

        if (!AddGuidValue(PLAYER_FARSIGHT, target->GetGUID()))
        {
            TC_LOG_FATAL("entities.player", "Player::CreateViewpoint: Player '{}' ({}) cannot add new viewpoint!", GetName(), GetGUID().ToString());
            return;
        }

        // farsight dynobj or puppet may be very far away
        UpdateVisibilityOf(target);

        if (Unit* targetUnit = target->ToUnit(); targetUnit && targetUnit != GetVehicleBase())
            targetUnit->AddPlayerToVision(this);
        SetSeer(target);
    }
    else
    {
        TC_LOG_DEBUG("maps", "Player::CreateViewpoint: Player {} removed seer", GetName());

        if (!RemoveGuidValue(PLAYER_FARSIGHT, target->GetGUID()))
        {
            TC_LOG_FATAL("entities.player", "Player::CreateViewpoint: Player '{}' ({}) cannot remove current viewpoint!", GetName(), GetGUID().ToString());
            return;
        }

        if (Unit* targetUnit = target->ToUnit(); targetUnit && targetUnit != GetVehicleBase())
            targetUnit->RemovePlayerFromVision(this);

        //must immediately set seer back otherwise may crash
        SetSeer(this);

        //WorldPacket data(SMSG_CLEAR_FAR_SIGHT_IMMEDIATE, 0);
        //SendDirectMessage(&data);
    }

    // HACK: Make sure update for PLAYER_FARSIGHT is received before SMSG_PET_SPELLS to properly hide "Release spirit" dialog
    if (target->GetTypeId() == TYPEID_UNIT && static_cast<Unit*>(target)->HasUnitTypeMask(UNIT_MASK_MINION) && static_cast<Minion*>(target)->IsRisenAlly())
    {
        if (apply)
        {
            UpdateDataMapType update_players;
            BuildUpdate(update_players);
            WorldPacket packet;
            for (UpdateDataMapType::iterator iter = update_players.begin(); iter != update_players.end(); ++iter)
            {
                iter->second.BuildPacket(&packet);
                iter->first->SendDirectMessage(&packet);
                packet.clear();
            }
        }
        else
        {
            m_deathTimer = 6 * MINUTE * IN_MILLISECONDS;

            // Reset "Release spirit" timer clientside
            WorldPacket data(SMSG_FORCED_DEATH_UPDATE);
            SendDirectMessage(&data);
        }
    }
}

WorldObject* Player::GetViewpoint() const
{
    if (ObjectGuid guid = GetGuidValue(PLAYER_FARSIGHT))
        return static_cast<WorldObject*>(ObjectAccessor::GetObjectByTypeMask(*this, guid, TYPEMASK_SEER));
    return nullptr;
}

uint32 Player::GetBarberShopCost(uint8 newhairstyle, uint8 newhaircolor, uint8 newfacialhair, BarberShopStyleEntry const* newSkin) const
{
    uint8 level = GetLevel();

    if (level > GT_MAX_LEVEL)
        level = GT_MAX_LEVEL;                               // max level in this dbc

    uint8 hairstyle = GetHairStyleId();
    uint8 haircolor = GetHairColorId();
    uint8 facialhair = GetFacialStyle();
    uint8 skincolor = GetSkinId();

    if ((hairstyle == newhairstyle) && (haircolor == newhaircolor) && (facialhair == newfacialhair) && (!newSkin || (newSkin->Data == skincolor)))
        return 0;

    GtBarberShopCostBaseEntry const* bsc = sGtBarberShopCostBaseStore.LookupEntry(level - 1);

    if (!bsc)                                                // shouldn't happen
        return 0xFFFFFFFF;

    float cost = 0;

    if (hairstyle != newhairstyle)
        cost += bsc->Data;                                  // full price

    if ((haircolor != newhaircolor) && (hairstyle == newhairstyle))
        cost += bsc->Data * 0.5f;                           // +1/2 of price

    if (facialhair != newfacialhair)
        cost += bsc->Data * 0.75f;                          // +3/4 of price

    if (newSkin && skincolor != newSkin->Data)
        cost += bsc->Data * 0.75f;                          // +5/6 of price

    return uint32(cost);
}

bool Player::isTotalImmune() const
{
    AuraEffectList const& immune = GetAuraEffectsByType(SPELL_AURA_SCHOOL_IMMUNITY);

    uint32 immuneMask = 0;
    for (AuraEffectList::const_iterator itr = immune.begin(); itr != immune.end(); ++itr)
    {
        immuneMask |= (*itr)->GetMiscValue();
        if (immuneMask & SPELL_SCHOOL_MASK_ALL)            // total immunity
            return true;
    }
    return false;
}

bool Player::HasTitle(uint32 bitIndex) const
{
    if (bitIndex > MAX_TITLE_INDEX)
        return false;

    uint32 fieldIndexOffset = bitIndex / 32;
    uint32 flag = 1 << (bitIndex % 32);
    return HasFlag(PLAYER__FIELD_KNOWN_TITLES + fieldIndexOffset, flag);
}

bool Player::HasTitle(CharTitlesEntry const* title) const
{
    return HasTitle(title->MaskID);
}

void Player::SetTitle(CharTitlesEntry const* title, bool lost)
{
    uint32 fieldIndexOffset = title->MaskID / 32;
    uint32 flag = 1 << (title->MaskID % 32);

    if (lost)
    {
        if (!HasFlag(PLAYER__FIELD_KNOWN_TITLES + fieldIndexOffset, flag))
            return;

        RemoveFlag(PLAYER__FIELD_KNOWN_TITLES + fieldIndexOffset, flag);
    }
    else
    {
        if (HasFlag(PLAYER__FIELD_KNOWN_TITLES + fieldIndexOffset, flag))
            return;

        SetFlag(PLAYER__FIELD_KNOWN_TITLES + fieldIndexOffset, flag);
    }

    WorldPacket data(SMSG_TITLE_EARNED, 4 + 4);
    data << uint32(title->MaskID);
    data << uint32(lost ? 0 : 1);                           // 1 - earned, 0 - lost
    SendDirectMessage(&data);
}

uint32 Player::GetRuneBaseCooldown(uint8 index)
{
    uint8 rune = GetBaseRune(index);
    uint32 cooldown = RUNE_BASE_COOLDOWN;

    AuraEffectList const& regenAura = GetAuraEffectsByType(SPELL_AURA_MOD_POWER_REGEN_PERCENT);
    for (AuraEffectList::const_iterator i = regenAura.begin();i != regenAura.end(); ++i)
    {
        if ((*i)->GetMiscValue() == POWER_RUNE && (*i)->GetMiscValueB() == rune)
            cooldown = cooldown * (100 - (*i)->GetAmount()) / 100;
    }

    return cooldown;
}

void Player::SetRuneCooldown(uint8 index, uint32 cooldown, bool casted /*= false*/)
{
    uint32 gracePeriod = GetRuneTimer(index);

    if (casted && IsInCombat())
    {
        if (gracePeriod < 0xFFFFFFFF && cooldown > 0)
        {
            uint32 lessCd = std::min(uint32(2500), gracePeriod);
            cooldown = (cooldown > lessCd) ? (cooldown - lessCd) : 0;
            SetLastRuneGraceTimer(index, lessCd);
        }

        SetRuneTimer(index, 0);
    }

    m_runes->runes[index].Cooldown = cooldown;
    m_runes->SetRuneState(index, (cooldown == 0) ? true : false);

    ResyncRunes();
}

void Player::SetRuneConvertAura(uint8 index, AuraEffect const* aura)
{
    m_runes->runes[index].ConvertAuras.insert(aura);
}

void Player::RemoveRuneConvertAura(uint8 index, AuraEffect const* aura)
{
    m_runes->runes[index].ConvertAuras.erase(aura);
}

void Player::AddRuneByAuraEffect(uint8 index, RuneType newType, AuraEffect const* aura)
{
    SetRuneConvertAura(index, aura);
    ConvertRune(index, newType);
}

void Player::RemoveRunesByAuraEffect(AuraEffect const* aura)
{
    for (uint8 itr = 0; itr < MAX_RUNES; ++itr)
    {
        RemoveRuneConvertAura(itr, aura);

        if (m_runes->runes[itr].ConvertAuras.empty())
            ConvertRune(itr, GetBaseRune(itr));
    }
}

void Player::RestoreBaseRune(uint8 index)
{
    std::vector<AuraEffect const*> removeList;
    std::unordered_set<AuraEffect const*>& auras = m_runes->runes[index].ConvertAuras;

    auto criteria = [&removeList](AuraEffect const* storedAura) -> bool
    {
        // AuraEffect already gone
        if (!storedAura)
            return true;

        if (storedAura->GetSpellInfo()->HasAttribute(SPELL_ATTR0_PASSIVE))
        {
            // Don't drop passive talents providing rune conversion
            if (storedAura->GetAuraType() == SPELL_AURA_CONVERT_RUNE)
                removeList.push_back(storedAura);
            return true;
        }

        // If rune was converted by a non-passive aura that is still active we should keep it converted
        return false;
    };

    Trinity::Containers::EraseIf(auras, criteria);

    if (!auras.empty())
        return;

    ConvertRune(index, GetBaseRune(index));

    if (removeList.empty())
        return;

    // Filter auras set to be removed if they are converting any other rune index
    for (AuraEffect const* storedAura : removeList)
    {
        uint8 itr = 0;
        for (; itr < MAX_RUNES; ++itr)
        {
            if (m_runes->runes[itr].ConvertAuras.find(storedAura) != m_runes->runes[itr].ConvertAuras.end())
                break;
        }

        if (itr == MAX_RUNES)
            storedAura->GetBase()->Remove();
    }
}

void Player::ConvertRune(uint8 index, RuneType newType)
{
    SetCurrentRune(index, newType);

    WorldPacket data(SMSG_CONVERT_RUNE, 2);
    data << uint8(index);
    data << uint8(newType);
    SendDirectMessage(&data);
}

void Player::ResyncRunes() const
{
    //@tswow-begin
    if (!HasRunes())
        return;
    //@tswow-end

    WorldPackets::Spells::ResyncRunes packet;
    packet.Count = MAX_RUNES;
    for (uint32 itr = 0; itr < MAX_RUNES; ++itr)
    {
        WorldPackets::Spells::ResyncRune resyncRune;
        resyncRune.RuneType = GetCurrentRune(itr);
        resyncRune.Cooldown = uint8(255) - uint8(GetRuneCooldown(itr) * uint32(255) / uint32(RUNE_BASE_COOLDOWN)); // cooldown time (0-255)
        packet.Runes.emplace_back(resyncRune);
    }
    SendDirectMessage(packet.Write());
}

void Player::AddRunePower(uint8 index) const
{
    WorldPacket data(SMSG_ADD_RUNE_POWER, 4);
    data << uint32(1 << index);                             // mask (0x00-0x3F probably)
    SendDirectMessage(&data);
}

static RuneType runeSlotTypes[MAX_RUNES] =
{
    /*0*/ RUNE_BLOOD,
    /*1*/ RUNE_BLOOD,
    /*2*/ RUNE_UNHOLY,
    /*3*/ RUNE_UNHOLY,
    /*4*/ RUNE_FROST,
    /*5*/ RUNE_FROST
};

void Player::InitRunes()
{
    //@tswow-begin
    if (!HasRunes())
        return;
    //@tswow-end

    m_runes = new Runes;

    m_runes->runeState = 0;
    m_runes->lastUsedRune = RUNE_BLOOD;

    for (uint8 i = 0; i < MAX_RUNES; ++i)
    {
        SetBaseRune(i, runeSlotTypes[i]);                               // init base types
        SetCurrentRune(i, runeSlotTypes[i]);                            // init current types
        SetRuneCooldown(i, 0);                                          // reset cooldowns
        SetRuneTimer(i, 0xFFFFFFFF);                                    // Reset rune flags
        SetLastRuneGraceTimer(i, 0);
        SetRuneConvertAura(i, nullptr);
        m_runes->SetRuneState(i);
    }

    for (uint8 i = 0; i < NUM_RUNE_TYPES; ++i)
        SetFloatValue(PLAYER_RUNE_REGEN_1 + i, 0.1f);
}

bool Player::IsBaseRuneSlotsOnCooldown(RuneType runeType) const
{
    for (uint8 i = 0; i < MAX_RUNES; ++i)
        if (GetBaseRune(i) == runeType && GetRuneCooldown(i) == 0)
            return false;

    return true;
}

bool Player::CanFlyInZone(uint32 mapid, uint32 zone, SpellInfo const* bySpell) const
{
    // continent checked in SpellInfo::CheckLocation at cast and area update
    uint32 v_map = GetVirtualMapForMapAndZone(mapid, zone);
    if (v_map == 571 && !bySpell->HasAttribute(SPELL_ATTR7_IGNORE_COLD_WEATHER_FLYING))
        if (!HasSpell(54197)) // 54197 = Cold Weather Flying
            return false;

    return true;
}

uint32 Player::GetPhaseMaskForSpawn() const
{
    uint32 phase = PHASEMASK_NORMAL;
    if (!IsGameMaster())
        phase = GetPhaseMask();
    else
    {
        AuraEffectList const& phases = GetAuraEffectsByType(SPELL_AURA_PHASE);
        if (!phases.empty())
            phase = phases.front()->GetMiscValue();
    }

    // some aura phases include 1 normal map in addition to phase itself
    if (uint32 n_phase = phase & ~PHASEMASK_NORMAL)
        return n_phase;

    return PHASEMASK_NORMAL;
}

void Player::SetFallInformation(uint32 time, float z)
{
    m_lastFallTime = time;
    m_lastFallZ = z;
}

void Player::HandleFall(MovementInfo const& movementInfo)
{
    // calculate total z distance of the fall
    float z_diff = m_lastFallZ - movementInfo.pos.GetPositionZ();
    //TC_LOG_DEBUG("zDiff = {}", z_diff);

    //Players with low fall distance, Feather Fall or physical immunity (charges used) are ignored
    // 14.57 can be calculated by resolving damageperc formula below to 0
    if (z_diff >= 14.57f && !isDead() && !IsGameMaster() &&
        !HasAuraType(SPELL_AURA_HOVER) && !HasAuraType(SPELL_AURA_FEATHER_FALL) &&
        !HasAuraType(SPELL_AURA_FLY) && !IsImmunedToDamage(SPELL_SCHOOL_MASK_NORMAL))
    {
        //Safe fall, fall height reduction
        int32 safe_fall = GetTotalAuraModifier(SPELL_AURA_SAFE_FALL);

        float damageperc = 0.018f*(z_diff-safe_fall)-0.2426f;

        if (damageperc > 0)
        {
            uint32 damage = (uint32)(damageperc * GetMaxHealth()*sWorld->getRate(RATE_DAMAGE_FALL));

            if (GetCommandStatus(CHEAT_GOD))
                damage = 0;

            float height = movementInfo.pos.m_positionZ;
            UpdateGroundPositionZ(movementInfo.pos.m_positionX, movementInfo.pos.m_positionY, height);

            if (damage > 0)
            {
                //Prevent fall damage from being more than the player maximum health
                if (damage > GetMaxHealth())
                    damage = GetMaxHealth();

                // Gust of Wind
                if (HasAura(43621))
                    damage = GetMaxHealth()/2;

                uint32 original_health = GetHealth();
                uint32 final_damage = EnvironmentalDamage(DAMAGE_FALL, damage);

                // recheck alive, might have died of EnvironmentalDamage, avoid cases when player die in fact like Spirit of Redemption case
                if (IsAlive() && final_damage < original_health)
                    UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_FALL_WITHOUT_DYING, uint32(z_diff*100));
            }

            //Z given by moveinfo, LastZ, FallTime, WaterZ, MapZ, Damage, Safefall reduction
            TC_LOG_DEBUG("entities.player.falldamage", "FALLDAMAGE z={} sz={} pZ={} FallTime={} mZ={} damage={} SF={}\nPlayer debug info:\n{}", movementInfo.pos.GetPositionZ(), height, GetPositionZ(), movementInfo.fallTime, height, damage, safe_fall, GetDebugInfo());
        }
    }
}

void Player::ResetAchievements()
{
    m_achievementMgr->Reset();
}

void Player::SendRespondInspectAchievements(Player* player) const
{
    m_achievementMgr->SendRespondInspectAchievements(player);
}

bool Player::HasAchieved(uint32 achievementId) const
{
    return m_achievementMgr->HasAchieved(achievementId);
}

void Player::StartTimedAchievement(AchievementCriteriaTimedTypes type, uint32 entry, uint32 timeLost/* = 0*/)
{
    m_achievementMgr->StartTimedAchievement(type, entry, timeLost);
}

void Player::RemoveTimedAchievement(AchievementCriteriaTimedTypes type, uint32 entry)
{
    m_achievementMgr->RemoveTimedAchievement(type, entry);
}

void Player::ResetAchievementCriteria(AchievementCriteriaCondition condition, uint32 value, bool evenIfCriteriaComplete /* = false*/)
{
    m_achievementMgr->ResetAchievementCriteria(condition, value, evenIfCriteriaComplete);
}

void Player::UpdateAchievementCriteria(AchievementCriteriaTypes type, uint32 miscValue1 /*= 0*/, uint32 miscValue2 /*= 0*/, WorldObject* ref /*= nullptr*/)
{
    ZoneScopedNC("Player::UpdateAchievementCriteria", WORLD_UPDATE_COLOR)

    m_achievementMgr->UpdateAchievementCriteria(type, miscValue1, miscValue2, ref);
}

void Player::CompletedAchievement(AchievementEntry const* entry)
{
    m_achievementMgr->CompletedAchievement(entry);
}

// @tswow-begin
void Player::UnlockAchievement(uint32 entry)
{
    AchievementEntry const* achievement = sAchievementMgr->GetAchievement(entry);
    if (!achievement)
        return;

    CompletedAchievement(achievement);
}
// @tswow-end

void Player::UpdateFallInformationIfNeed(MovementInfo const& minfo, uint16 opcode)
{
    if (m_lastFallTime >= minfo.fallTime || m_lastFallZ <= minfo.pos.GetPositionZ() || opcode == MSG_MOVE_FALL_LAND)
        SetFallInformation(minfo.fallTime, minfo.pos.GetPositionZ());
}

void Player::UnsummonPetTemporaryIfAny()
{
    Pet* pet = GetPet();
    if (!pet)
        return;

    if (!m_temporaryUnsummonedPetNumber && pet->isControlled() && !pet->isTemporarySummoned())
    {
        m_temporaryUnsummonedPetNumber = pet->GetCharmInfo()->GetPetNumber();
        m_oldpetspell = pet->GetUInt32Value(UNIT_CREATED_BY_SPELL);
    }

    RemovePet(pet, PET_SAVE_AS_CURRENT);
}

void Player::ResummonPetTemporaryUnSummonedIfAny()
{
    if (!m_temporaryUnsummonedPetNumber)
        return;

    // not resummon in not appropriate state
    if (IsPetNeedBeTemporaryUnsummoned())
        return;

    if (GetPetGUID())
        return;

    Pet* NewPet = new Pet(this);
    if (!NewPet->LoadPetFromDB(this, 0, m_temporaryUnsummonedPetNumber, true))
        delete NewPet;

    m_temporaryUnsummonedPetNumber = 0;
}

bool Player::IsPetNeedBeTemporaryUnsummoned() const
{
    return !IsInWorld() || !IsAlive() /*|| IsMounted()*/ /*+in flight*/;
}

bool Player::CanSeeSpellClickOn(Creature const* c) const
{
    if (!c->HasNpcFlag(UNIT_NPC_FLAG_SPELLCLICK))
        return false;

    auto clickBounds = sObjectMgr->GetSpellClickInfoMapBounds(c->GetEntry());
    if (clickBounds.begin() == clickBounds.end())
        return false;

    for (auto const& clickPair : clickBounds)
    {
        if (!clickPair.second.IsFitToRequirements(this, c))
            return false;

        if (sConditionMgr->IsObjectMeetingSpellClickConditions(c->GetEntry(), clickPair.second.spellId, const_cast<Player*>(this), const_cast<Creature*>(c)))
            return true;
    }

    return false;
}

void Player::RemoveAtLoginFlag(AtLoginFlags flags, bool persist /*= false*/)
{
    m_atLoginFlags &= ~flags;

    if (persist)
    {
        CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_REM_AT_LOGIN_FLAG);

        stmt->setUInt16(0, uint16(flags));
        stmt->setUInt32(1, GetGUID().GetCounter());

        CharacterDatabase.Execute(stmt);
    }
}

void Player::LoadActions(PreparedQueryResult result)
{
    if (result)
        _LoadActions(result);

    SendActionButtons(1);
}

void Player::SetReputation(uint32 factionentry, uint32 value)
{
    GetReputationMgr().SetReputation(sFactionStore.LookupEntry(factionentry), value);
}
uint32 Player::GetReputation(uint32 factionentry) const
{
    return GetReputationMgr().GetReputation(sFactionStore.LookupEntry(factionentry));
}

std::string const& Player::GetGuildName() const
{
    return sGuildMgr->GetGuildById(GetGuildId())->GetName();
}

void Player::SendDuelCountdown(uint32 counter)
{
    WorldPacket data(SMSG_DUEL_COUNTDOWN, 4);
    data << uint32(counter);                                // seconds
    SendDirectMessage(&data);
}

PetStable& Player::GetOrInitPetStable()
{
    if (!m_petStable)
        m_petStable = std::make_unique<PetStable>();

    return *m_petStable;
}

void Player::SetRandomWinner(bool isWinner)
{
    m_IsBGRandomWinner = isWinner;
    if (m_IsBGRandomWinner)
    {
        CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_INS_BATTLEGROUND_RANDOM);

        stmt->setUInt32(0, GetGUID().GetCounter());

        CharacterDatabase.Execute(stmt);
    }
}

float Player::GetAverageItemLevel() const
{
    float sum = 0;
    uint32 count = 0;

    for (int i = EQUIPMENT_SLOT_START; i < EQUIPMENT_SLOT_END; ++i)
    {
        // don't check tabard, ranged, offhand or shirt
        if (i == EQUIPMENT_SLOT_TABARD || i == EQUIPMENT_SLOT_RANGED || i == EQUIPMENT_SLOT_OFFHAND || i == EQUIPMENT_SLOT_BODY)
            continue;

        if (m_items[i] && m_items[i]->GetTemplate())
            sum += m_items[i]->GetTemplate()->GetItemLevelIncludingQuality();

        ++count;
    }

    return ((float)sum) / count;
}

bool Player::SetDisableGravity(bool disable, bool packetOnly /*= false*/, bool updateAnimTier /*= true*/)
{
    if (!packetOnly && !Unit::SetDisableGravity(disable, packetOnly, updateAnimTier))
        return false;

    WorldPacket data(disable ? SMSG_MOVE_GRAVITY_DISABLE : SMSG_MOVE_GRAVITY_ENABLE, 12);
    data << GetPackGUID();
    data << uint32(0);          //! movement counter
    SendDirectMessage(&data);

    data.Initialize(MSG_MOVE_GRAVITY_CHNG, 64);
    data << GetPackGUID();
    BuildMovementPacket(&data);
    SendMessageToSet(&data, false);
    return true;
}

bool Player::SetCanFly(bool apply, bool packetOnly /*= false*/)
{
    if (!apply)
        SetFallInformation(0, GetPositionZ());

    WorldPacket data(apply ? SMSG_MOVE_SET_CAN_FLY : SMSG_MOVE_UNSET_CAN_FLY, 12);
    data << GetPackGUID();
    data << uint32(0);          //! movement counter
    SendDirectMessage(&data);

    if (packetOnly || Unit::SetCanFly(apply))
    {
        data.Initialize(MSG_MOVE_UPDATE_CAN_FLY, 64);
        data << GetPackGUID();
        BuildMovementPacket(&data);
        SendMessageToSet(&data, false);
        return true;
    }
    else
        return false;
}

bool Player::SetHover(bool apply, bool packetOnly /*= false*/, bool updateAnimTier /*= true*/)
{
    if (!packetOnly && !Unit::SetHover(apply, packetOnly, updateAnimTier))
        return false;

    WorldPacket data(apply ? SMSG_MOVE_SET_HOVER : SMSG_MOVE_UNSET_HOVER, 12);
    data << GetPackGUID();
    data << uint32(0);          //! movement counter
    SendDirectMessage(&data);

    data.Initialize(MSG_MOVE_HOVER, 64);
    data << GetPackGUID();
    BuildMovementPacket(&data);
    SendMessageToSet(&data, false);
    return true;
}

bool Player::SetWaterWalking(bool apply, bool packetOnly /*= false*/)
{
    if (!packetOnly && !Unit::SetWaterWalking(apply))
        return false;

    WorldPacket data(apply ? SMSG_MOVE_WATER_WALK : SMSG_MOVE_LAND_WALK, 12);
    data << GetPackGUID();
    data << uint32(0);          //! movement counter
    SendDirectMessage(&data);

    data.Initialize(MSG_MOVE_WATER_WALK, 64);
    data << GetPackGUID();
    BuildMovementPacket(&data);
    SendMessageToSet(&data, false);
    return true;
}

bool Player::SetFeatherFall(bool apply, bool packetOnly /*= false*/)
{
    if (!packetOnly && !Unit::SetFeatherFall(apply))
        return false;

    WorldPacket data(apply ? SMSG_MOVE_FEATHER_FALL : SMSG_MOVE_NORMAL_FALL, 12);
    data << GetPackGUID();
    data << uint32(0);          //! movement counter
    SendDirectMessage(&data);

    data.Initialize(MSG_MOVE_FEATHER_FALL, 64);
    data << GetPackGUID();
    BuildMovementPacket(&data);
    SendMessageToSet(&data, false);
    return true;
}

void Player::SetInGuild(uint32 guildId)
{
    SetUInt32Value(PLAYER_GUILDID, guildId);
    sCharacterCache->UpdateCharacterGuildId(GetGUID(), guildId);
}

Guild* Player::GetGuild()
{
    uint32 guildId = GetGuildId();
    return guildId ? sGuildMgr->GetGuildById(guildId) : nullptr;
}

Pet* Player::SummonPet(uint32 entry, float x, float y, float z, float ang, PetType petType, uint32 duration)
{
    PetStable& petStable = GetOrInitPetStable();

    Pet* pet = new Pet(this, petType);

    if (petType == SUMMON_PET && pet->LoadPetFromDB(this, entry, 0, false))
    {
        // Remove Demonic Sacrifice auras (known pet)
        Unit::AuraEffectList const& auraClassScripts = GetAuraEffectsByType(SPELL_AURA_OVERRIDE_CLASS_SCRIPTS);
        for (Unit::AuraEffectList::const_iterator itr = auraClassScripts.begin(); itr != auraClassScripts.end();)
        {
            if ((*itr)->GetMiscValue() == 2228)
            {
                RemoveAurasDueToSpell((*itr)->GetId());
                itr = auraClassScripts.begin();
            }
            else
                ++itr;
        }

        if (duration > 0)
            pet->SetDuration(duration);

        // Generate a new name for the newly summoned ghoul
        if (pet->IsPetGhoul())
        {
            std::string new_name = sObjectMgr->GeneratePetName(entry);
            if (!new_name.empty())
                pet->SetName(new_name);
        }

        pet->InitSummon();

        return nullptr;
    }

    // petentry == 0 for hunter "call pet" (current pet summoned if any)
    if (!entry)
    {
        delete pet;
        return nullptr;
    }

    pet->Relocate(x, y, z, ang);
    if (!pet->IsPositionValid())
    {
        TC_LOG_ERROR("misc", "Player::SummonPet: Pet ({}, Entry: {}) not summoned. Suggested coordinates aren't valid (X: {} Y: {})", pet->GetGUID().ToString(), pet->GetEntry(), pet->GetPositionX(), pet->GetPositionY());
        delete pet;
        return nullptr;
    }

    Map* map = GetMap();
    uint32 pet_number = sObjectMgr->GeneratePetNumber();
    if (!pet->Create(map->GenerateLowGuid<HighGuid::Pet>(), map, GetPhaseMask(), entry, pet_number))
    {
        TC_LOG_ERROR("misc", "Player::SummonPet: No such creature entry {}", entry);
        delete pet;
        return nullptr;
    }

    if (petType == SUMMON_PET && petStable.CurrentPet)
        RemovePet(nullptr, PET_SAVE_NOT_IN_SLOT);

    pet->SetCreatorGUID(GetGUID());
    pet->SetFaction(GetFaction());

    pet->ReplaceAllNpcFlags(UNIT_NPC_FLAG_NONE);
    pet->InitStatsForLevel(GetLevel());

    SetMinion(pet, true);

    switch (petType)
    {
        case SUMMON_PET:
            // this enables pet details window (Shift+P)
            pet->GetCharmInfo()->SetPetNumber(pet_number, true);
            pet->SetClass(CLASS_MAGE);
            pet->SetPetExperience(0);
            pet->SetPetNextLevelExperience(1000);
            pet->SetFullHealth();
            pet->SetPower(POWER_MANA, pet->GetMaxPower(POWER_MANA));
            pet->SetPetNameTimestamp(uint32(GameTime::GetGameTime())); // cast can't be helped in this case
            break;
        default:
            break;
    }

    map->AddToMap(pet->ToCreature());

    ASSERT(!petStable.CurrentPet && (petType != HUNTER_PET || !petStable.GetUnslottedHunterPet()));
    pet->FillPetInfo(&petStable.CurrentPet.emplace());

    switch (petType)
    {
        case SUMMON_PET:
            pet->InitPetCreateSpells();
            pet->InitTalentForLevel();
            pet->SavePetToDB(PET_SAVE_AS_CURRENT);
            PetSpellInitialize();
            break;
        default:
            break;
    }

    if (petType == SUMMON_PET)
    {
        // Remove Demonic Sacrifice auras (known pet)
        Unit::AuraEffectList const& auraClassScripts = GetAuraEffectsByType(SPELL_AURA_OVERRIDE_CLASS_SCRIPTS);
        for (Unit::AuraEffectList::const_iterator itr = auraClassScripts.begin(); itr != auraClassScripts.end();)
        {
            if ((*itr)->GetMiscValue() == 2228)
            {
                RemoveAurasDueToSpell((*itr)->GetId());
                itr = auraClassScripts.begin();
            }
            else
                ++itr;
        }
    }

    if (duration > 0)
        pet->SetDuration(duration);

    //ObjectAccessor::UpdateObjectVisibility(pet);

    pet->InitSummon();

    return pet;
}

void Player::SendSupercededSpell(uint32 oldSpell, uint32 newSpell) const
{
    WorldPacket data(SMSG_SUPERCEDED_SPELL, 8);
    data << uint32(oldSpell) << uint32(newSpell);
    SendDirectMessage(&data);
}

bool Player::ValidateAppearance(uint8 race, uint8 class_, uint8 gender, uint8 hairID, uint8 hairColor, uint8 faceID, uint8 facialHair, uint8 skinColor, bool create /*=false*/)
{
    auto validateCharSection = [class_, create](CharSectionsEntry const* entry) -> bool
    {
        if (!entry)
            return false;

        // Check Death Knight exclusive
        if (class_ != CLASS_DEATH_KNIGHT && entry->HasFlag(SECTION_FLAG_DEATH_KNIGHT))
            return false;

        // Character creation/customize has some limited sections (as opposed to barbershop)
        if (create && !entry->HasFlag(SECTION_FLAG_PLAYER))
            return false;

        return true;
    };

    // For Skin type is always 0
    CharSectionsEntry const* skinEntry = GetCharSectionEntry(race, SECTION_TYPE_SKIN, gender, 0, skinColor);
    if (!validateCharSection(skinEntry))
        return false;

    // Skin Color defined as Face color, too
    CharSectionsEntry const* faceEntry = GetCharSectionEntry(race, SECTION_TYPE_FACE, gender, faceID, skinColor);
    if (!validateCharSection(faceEntry))
        return false;

    // Check Hair
    CharSectionsEntry const* hairEntry = GetCharSectionEntry(race, SECTION_TYPE_HAIR, gender, hairID, hairColor);
    if (!validateCharSection(hairEntry))
        return false;

    // These combinations don't have an entry of Type SECTION_TYPE_FACIAL_HAIR, exclude them from that check
    bool const excludeCheck = (race == RACE_TAUREN) || (race == RACE_DRAENEI) || (gender == GENDER_FEMALE && race != RACE_NIGHTELF && race != RACE_UNDEAD_PLAYER);
    if (!excludeCheck)
    {
        CharSectionsEntry const* facialHairEntry = GetCharSectionEntry(race, SECTION_TYPE_FACIAL_HAIR, gender, facialHair, hairColor);
        if (!validateCharSection(facialHairEntry))
            return false;
    }

    CharacterFacialHairStylesEntry const* entry = GetCharFacialHairEntry(race, gender, facialHair);
    if (!entry)
        return false;

    return true;
}

void Player::SetRestFlag(RestFlag restFlag, uint32 triggerId /*= 0*/)
{
    uint32 oldRestMask = _restFlagMask;
    _restFlagMask |= restFlag;

    if (!oldRestMask && _restFlagMask) // only set flag/time on the first rest state
    {
        _restTime = GameTime::GetGameTime();
        SetFlag(PLAYER_FLAGS, PLAYER_FLAGS_RESTING);
    }

    if (triggerId)
        inn_triggerId = triggerId;
}

void Player::RemoveRestFlag(RestFlag restFlag)
{
    uint32 oldRestMask = _restFlagMask;
    _restFlagMask &= ~restFlag;

    if (oldRestMask && !_restFlagMask) // only remove flag/time on the last rest state remove
    {
        _restTime = 0;
        RemoveFlag(PLAYER_FLAGS, PLAYER_FLAGS_RESTING);
    }
}

uint32 Player::DoRandomRoll(uint32 minimum, uint32 maximum)
{
    ASSERT(maximum <= 10000);

    uint32 roll = urand(minimum, maximum);

    WorldPackets::Misc::RandomRoll randomRoll;
    randomRoll.Min = minimum;
    randomRoll.Max = maximum;
    randomRoll.Result = roll;
    randomRoll.Roller = GetGUID();
    if (Group* group = GetGroup())
        group->BroadcastPacket(randomRoll.Write(), false);
    else
        SendDirectMessage(randomRoll.Write());

    return roll;
}

void Player::RemoveSocial()
{
    sSocialMgr->RemovePlayerSocial(GetGUID());
    m_social = nullptr;
}

std::string Player::GetDebugInfo() const
{
    std::stringstream sstr;
    sstr << Unit::GetDebugInfo();
    return sstr.str();
}

GameClient* Player::GetGameClient() const
{
    return GetSession()->GetGameClient();
}

// @tswow-begin
void Player::SetSelection(ObjectGuid guid) {
    uint64_t old = GetGuidValue(UNIT_FIELD_TARGET).GetRawValue();
    SetGuidValue(UNIT_FIELD_TARGET, guid);
    FIRE(Unit,OnSetTarget, TSUnit(this), guid.GetRawValue(), old);
}
// @tswow-end

// @epoch-start
uint32 Player::GetXPForDifficulty(uint8 difficulty)
{
    QuestXPEntry const* xpentry = sQuestXPStore.LookupEntry(GetLevel());
    if (!xpentry)
        return 0;

    return Quest::RoundXPValue(xpentry->Difficulty[difficulty]);
}

void Player::SetCanSeeTransmog(bool on)
{
    if (m_canSeeTransmog == on)
        return;

    m_canSeeTransmog = on;

    // update own item display
    for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
        ForceValuesUpdateAtIndex(PLAYER_VISIBLE_ITEM_1_ENTRYID + (slot * 2));

    if (m_clientGUIDs.empty())
        return;

    for (auto itr = m_clientGUIDs.begin(); itr != m_clientGUIDs.end(); ++itr)
    {
        if (itr->GetTypeId() != TYPEID_PLAYER)
            continue;

        Player* pp = ObjectAccessor::FindPlayer(*itr);

        for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
            pp->ForceValuesUpdateAtIndex(PLAYER_VISIBLE_ITEM_1_ENTRYID + (slot * 2));
    }
}
//@epoch-end