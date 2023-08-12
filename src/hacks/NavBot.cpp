#include "Settings.hpp"
#include "init.hpp"
#include "HookTools.hpp"
#include "interfaces.hpp"
#include "navparser.hpp"
#include "playerresource.h"
#include "localplayer.hpp"
#include "sdk.hpp"
#include "entitycache.hpp"
#include "CaptureLogic.hpp"
#include "PlayerTools.hpp"
#include "Aimbot.hpp"
#include "AntiAim.hpp"
#include "teamroundtimer.hpp"
#include "Warp.hpp"

namespace hacks::tf2::NavBot
{

static settings::Boolean enabled("navbot.enabled", "false");
static settings::Boolean search_health("navbot.search-health", "true");
static settings::Boolean search_ammo("navbot.search-ammo", "true");
static settings::Boolean stay_near("navbot.stay-near", "true");
static settings::Boolean capture_objectives("navbot.capture-objectives", "true");
static settings::Boolean randomize_cp("navbot.capture-objectives.randomize-cp", "true");
static settings::Boolean defend_intel("navbot.defend-intel", "true");
static settings::Boolean roam_defend("navbot.defend-idle", "true");
static settings::Boolean snipe_sentries("navbot.snipe-sentries", "true");
static settings::Boolean snipe_sentries_shortrange("navbot.snipe-sentries.shortrange", "false");
static settings::Boolean primary_only("navbot.primary-only", "true");
static settings::Boolean melee_mode("navbot.allow-melee", "true");
static settings::Boolean autojump("navbot.autojump.enabled", "false");
static settings::Boolean engie_mode("navbot.engineer-mode", "true");
static settings::Float jump_distance("navbot.autojump.trigger-distance", "300");
static settings::Int repath_delay("navbot.repath-time.delay", "2000");
static settings::Int capture_delay("navbot.capture-objectives.delay", "400");

// Allow for custom distance configs, mainly for debugging purposes
static settings::Boolean distance_config_custom("navbot.distance-config.enabled", "false");
static settings::Boolean distance_config_custom_prefer_far("navbot.distance-config.perfer_far", "true");
static settings::Float distance_config_custom_min_very_dangerous("navbot.distance-config.min_very_dangerous", "300");
static settings::Float distance_config_custom_min_slightly_dangerous("navbot.distance-config.min_slightly_dangerous", "500");
static settings::Float distance_config_custom_max_slight_distance("navbot.distance-config.max_slight_distance", "4000");

// Overwrite to return true for payload carts as an example
static bool overwrite_capture = false;
// Doomsday is a ctf + payload map which breaks capturing...
static bool is_doomsday = false;

// Draw blacklist, mainly for debugging purposes
#if ENABLE_VISUALS
static settings::Boolean draw_blacklist("navbot.draw-blacklist", "false");
#endif

// Controls the bot parameters like distance from enemy
struct bot_class_config
{
    float min_very_dangerous;
    float min_slightly_dangerous;
    float max;
    bool prefer_far;
};

constexpr bot_class_config CONFIG_SHORT_RANGE         = { 140.0f, 400.0f, 600.0f, false };
constexpr bot_class_config CONFIG_MID_RANGE           = { 200.0f, 500.0f, 3000.0f, true };
constexpr bot_class_config CONFIG_LONG_RANGE          = { 300.0f, 600.0f, 4000.0f, true };
constexpr bot_class_config CONFIG_ENGINEER            = { 200.0f, 500.0f, 3000.0f, false };
constexpr bot_class_config CONFIG_GUNSLINGER_ENGINEER = { 50.0f, 300.0f, 2000.0f, false };
bot_class_config selected_config                      = CONFIG_MID_RANGE;

static Timer health_cooldown{};
static Timer ammo_cooldown{};
static Timer melee_timer{};
static Timer last_jump{};
// Should we search health at all?
bool shouldSearchHealth(bool low_priority = false)
{
    // Priority too high
    if (navparser::NavEngine::current_priority > health)
        return false;
    float health_percent = LOCAL_E->m_iHealth() / (float) g_pPlayerResource->GetMaxHealth(LOCAL_E);
    // Get health when below 65%, or below 75% and just patrol
    return health_percent < 0.64f || (low_priority && (navparser::NavEngine::current_priority <= patrol || navparser::NavEngine::current_priority == lowprio_health) && health_percent <= 0.75f);
}

// Should we search ammo at all?
bool shouldSearchAmmo()
{
    if (CE_BAD(LOCAL_W))
        return false;
    // Priority too high
    if (navparser::NavEngine::current_priority > ammo)
        return false;

    int *weapon_list = (int *) ((uint64_t)(RAW_ENT(LOCAL_E)) + netvar.hMyWeapons);
    if (!weapon_list)
        return false;
    if (g_pLocalPlayer->holding_sniper_rifle && CE_INT(LOCAL_E, netvar.m_iAmmo + 1 * 4) <= 5)
        return true;
    for (int i = 0; weapon_list[i]; i++)
    {
        int handle = weapon_list[i];
        int eid    = handle & 0xFFF;
        if (eid > MAX_PLAYERS && eid <= HIGHEST_ENTITY)
        {
            IClientEntity *weapon = g_IEntityList->GetClientEntity(eid);
            if (weapon and re::C_BaseCombatWeapon::IsBaseCombatWeapon(weapon) && re::C_TFWeaponBase::UsesPrimaryAmmo(weapon) && !re::C_TFWeaponBase::HasPrimaryAmmo(weapon))
                return true;
        }
    }
    return false;
}

// Vector of sniper spot positions we can nav to
static std::vector<Vector> sniper_spots;

// Used for time between refreshing sniperspots
static Timer refresh_sniperspots_timer{};
void refreshSniperSpots()
{
    if (!refresh_sniperspots_timer.test_and_set(60000))
        return;

    sniper_spots.clear();

    // Search all nav areas for valid sniper spots
    for (auto &area : navparser::NavEngine::getNavFile()->m_areas)
        for (auto &hiding_spot : area.m_hidingSpots)
            // Spots actually marked for sniping
            if (hiding_spot.IsExposed() || hiding_spot.IsGoodSniperSpot() || hiding_spot.IsIdealSniperSpot())
                sniper_spots.emplace_back(hiding_spot.m_pos);
}

static std::pair<CachedEntity *, float> getNearestPlayerDistance(bool vischeck, bool ycheck)
{
    float distance         = FLT_MAX;
    CachedEntity *best_ent = nullptr;
    for (int i = 1; i <= g_IEngine->GetMaxClients(); i++)
    {
        CachedEntity *ent = ENTITY(i);
        if (CE_VALID(ent) && ent->m_vecDormantOrigin() && ent->m_bAlivePlayer() && ent->m_bEnemy() && g_pLocalPlayer->v_Origin.DistTo(ent->m_vecOrigin()) < distance && player_tools::shouldTarget(ent) && (!vischeck || VisCheckEntFromEnt(LOCAL_E, ent)))
        {
            if (ycheck && (ent->m_vecOrigin().y - LOCAL_E->m_vecOrigin().y) > 300.0f)
                continue;
            if (hacks::shared::aimbot::ignore_cloak && IsPlayerInvisible(ent))
                continue;
            distance = g_pLocalPlayer->v_Origin.DistTo(*ent->m_vecDormantOrigin());
            best_ent = ent;
        }
    }
    return { best_ent, distance };
}

static std::vector<Vector> building_spots;

inline bool HasGunslinger(CachedEntity *ent)
{
    return HasWeapon(ent, 142);
}

inline bool isEngieMode()
{
    return *engie_mode && g_pLocalPlayer->clazz == tf_engineer;
}

bool BlacklistedFromBuilding(CNavArea *area)
{
    for (auto blacklisted_area : *navparser::NavEngine::getFreeBlacklist())
    {
        if (blacklisted_area.first == area && blacklisted_area.second.value == navparser::BlacklistReason_enum::BAD_BUILDING_SPOT)
            return true;
    }
    return false;
}

static Timer refresh_buildingspots_timer;
void refreshBuildingSpots(bool force = false)
{
    if (!isEngieMode())
        return;
    if (force || refresh_buildingspots_timer.test_and_set(5000))
    {
        building_spots.clear();
        std::optional<Vector> target;

        auto our_flag = flagcontroller::getFlag(g_pLocalPlayer->team);
        target        = our_flag.spawn_pos;

        if (!target)
        {
            auto nearest = getNearestPlayerDistance(false, false);
            if (CE_GOOD(nearest.first))
                target = *nearest.first->m_vecDormantOrigin();
        }
        if (target)
        {
            // Search all nav areas for valid spots
            for (auto &area : navparser::NavEngine::getNavFile()->m_areas)
            {
                // Blacklisted :(
                if (BlacklistedFromBuilding(&area))
                    continue;
                // Don't try to build in spawn
                if ((area.m_TFattributeFlags & TF_NAV_SPAWN_ROOM_RED) != 0 || (area.m_TFattributeFlags & TF_NAV_SPAWN_ROOM_BLUE) != 0 || (area.m_TFattributeFlags & TF_NAV_SPAWN_ROOM_EXIT) != 0)
                    continue;

                if ((area.m_TFattributeFlags & TF_NAV_SENTRY_SPOT) != 0)
                    building_spots.emplace_back(area.m_center);
                else
                {
                    for (auto &hiding_spot : area.m_hidingSpots)
                        if (hiding_spot.HasGoodCover())
                            building_spots.emplace_back(hiding_spot.m_pos);
                }
            }
            // Sort by distance to nearest, lower is better
            std::sort(building_spots.begin(), building_spots.end(), [target](Vector a, Vector b) { return a.DistTo(*target) < b.DistTo(*target); });
            // Bad hack, we want sentry spot diversity, but also don't want to pick a spot very far from us
            building_spots.resize(5);
        }
    }
}

static CachedEntity *mySentry    = nullptr;
static CachedEntity *myDispenser = nullptr;

void refreshLocalBuildings()
{
    if (isEngieMode())
    {
        mySentry = nullptr;
        myDispenser = nullptr;
        if (CE_GOOD(LOCAL_E))
        {
            for (int i = g_IEngine->GetMaxClients() + 1; i < MAX_ENTITIES; i++)
            {
                CachedEntity *ent = ENTITY(i);
                if (!ent || CE_BAD(ent) || ent->m_bEnemy() || !ent->m_bAlivePlayer())
                    continue;
                auto cid = ent->m_iClassID();
                if (cid != CL_CLASS(CObjectSentrygun) && cid != CL_CLASS(CObjectDispenser))
                    continue;
                if (HandleToIDX(CE_INT(ent, netvar.m_hBuilder)) != LOCAL_E->m_IDX)
                    continue;
                if (CE_INT(ent, netvar.m_bPlacing))
                    continue;

                if (cid == CL_CLASS(CObjectSentrygun))
                    mySentry = ent;
                else if (cid == CL_CLASS(CObjectDispenser))
                    myDispenser = ent;
            }
        }
    }
}

static Vector current_building_spot;
static bool navToSentrySpot()
{
    static Timer wait_until_path_sentry;
    // Wait a bit before pathing again
    if (!wait_until_path_sentry.test_and_set(300))
        return false;
    // Try to nav to our existing sentry
    if (CE_GOOD(mySentry) && mySentry->m_bAlivePlayer() && mySentry->m_vecDormantOrigin())
    {
        if (navparser::NavEngine::navTo(*mySentry->m_vecDormantOrigin(), engineer))
            return true;
    }
    else
        mySentry = nullptr;

    // No building spots
    if (building_spots.empty())
        return false;
    // Don't overwrite current nav
    if (navparser::NavEngine::current_priority == engineer)
        return false;
    // Max 10 attempts
    for (int attempts = 0; attempts < 10 && attempts < building_spots.size(); ++attempts)
    {
        // Get a random building spot
        auto random = select_randomly(building_spots.begin(), building_spots.end());
        // Try to nav there
        if (navparser::NavEngine::navTo(*random, engineer))
        {
            current_building_spot = *random;
            return true;
        }
    }

    return false;
}

// Get Valid Dispensers (Used for health/ammo)
std::vector<CachedEntity *> getDispensers()
{
    std::vector<CachedEntity *> entities;
    for (int i = g_IEngine->GetMaxClients() + 1; i < MAX_ENTITIES; i++)
    {
        CachedEntity *ent = ENTITY(i);
        if (CE_BAD(ent) || ent->m_iClassID() != CL_CLASS(CObjectDispenser) || ent->m_iTeam() != g_pLocalPlayer->team)
            continue;
        if (CE_BYTE(ent, netvar.m_bCarryDeploy) || CE_BYTE(ent, netvar.m_bHasSapper) || CE_BYTE(ent, netvar.m_bBuilding))
            continue;

        // This fixes the fact that players can just place dispensers in unreachable locations
        auto local_nav = navparser::NavEngine::findClosestNavSquare(ent->m_vecOrigin());
        if (local_nav->getNearestPoint(ent->m_vecOrigin().AsVector2D()).DistTo(ent->m_vecOrigin()) > 300.0f || local_nav->getNearestPoint(ent->m_vecOrigin().AsVector2D()).z - ent->m_vecOrigin().z > navparser::PLAYER_JUMP_HEIGHT)
            continue;
        entities.push_back(ent);
    }
    // Sort by distance, closer is better
    std::sort(entities.begin(), entities.end(), [](CachedEntity *a, CachedEntity *b) { return a->m_flDistance() < b->m_flDistance(); });
    return entities;
}

// Get entities of given itemtypes (Used for health/ammo)
std::vector<CachedEntity *> getEntities(const std::vector<k_EItemType> &itemtypes)
{
    std::vector<CachedEntity *> entities;
    for (int i = g_IEngine->GetMaxClients() + 1; i < MAX_ENTITIES; i++)
    {
        CachedEntity *ent = ENTITY(i);
        if (CE_BAD(ent))
            continue;
        for (auto &itemtype : itemtypes)
        {
            if (ent->m_ItemType() == itemtype)
            {
                entities.push_back(ent);
                break;
            }
        }
    }
    // Sort by distance, closer is better
    std::sort(entities.begin(), entities.end(), [](CachedEntity *a, CachedEntity *b) { return a->m_flDistance() < b->m_flDistance(); });
    return entities;
}

// Find health if needed
bool getHealth(bool low_priority = false)
{
    Priority_list priority = low_priority ? lowprio_health : health;
    if (!health_cooldown.check(*repath_delay / 2))
        return navparser::NavEngine::current_priority == priority;
    if (shouldSearchHealth(low_priority))
    {
        // Already pathing, only try to repath occasionally
        if (navparser::NavEngine::current_priority == priority)
        {
            static Timer repath_timer;
            if (!repath_timer.test_and_set(*repath_delay))
                return true;
        }
        auto healthpacks = getEntities({ ITEM_HEALTH_SMALL, ITEM_HEALTH_MEDIUM, ITEM_HEALTH_LARGE });
        auto dispensers  = getDispensers();

        auto total_ents = healthpacks;

        // Add dispensers and sort list again
        if (!dispensers.empty())
        {
            total_ents.reserve(healthpacks.size() + dispensers.size());
            total_ents.insert(total_ents.end(), dispensers.begin(), dispensers.end());
            std::sort(total_ents.begin(), total_ents.end(), [](CachedEntity *a, CachedEntity *b) { return a->m_flDistance() < b->m_flDistance(); });
        }

        for (auto healthpack : total_ents)
            // If we succeeed, don't try to path to other packs
            if (navparser::NavEngine::navTo(healthpack->m_vecOrigin(), priority, true, healthpack->m_vecOrigin().DistToSqr(g_pLocalPlayer->v_Origin) > 200.0f * 200.0f))
                return true;
        health_cooldown.update();
    }
    else if (navparser::NavEngine::current_priority == priority)
        navparser::NavEngine::cancelPath();
    return false;
}

// Find ammo if needed
bool getAmmo(bool force = false)
{
    if (!force && !ammo_cooldown.check(*repath_delay / 2))
        return navparser::NavEngine::current_priority == ammo;
    if (force || shouldSearchAmmo())
    {
        // Already pathing, only try to repath occasionally
        if (navparser::NavEngine::current_priority == ammo)
        {
            static Timer repath_timer;
            if (!repath_timer.test_and_set(*repath_delay))
                return true;
        }
        auto ammopacks  = getEntities({ ITEM_AMMO_SMALL, ITEM_AMMO_MEDIUM, ITEM_AMMO_LARGE });
        auto dispensers = getDispensers();

        auto total_ents = ammopacks;

        // Add dispensers and sort list again
        if (!dispensers.empty())
        {
            total_ents.reserve(ammopacks.size() + dispensers.size());
            total_ents.insert(total_ents.end(), dispensers.begin(), dispensers.end());
            std::sort(total_ents.begin(), total_ents.end(), [](CachedEntity *a, CachedEntity *b) { return a->m_flDistance() < b->m_flDistance(); });
        }
        for (auto ammopack : total_ents)
            // If we succeeed, don't try to path to other packs
            if (navparser::NavEngine::navTo(ammopack->m_vecOrigin(), ammo, true, ammopack->m_vecOrigin().DistToSqr(g_pLocalPlayer->v_Origin) > 200.0f * 200.0f))
                return true;
        ammo_cooldown.update();
    }
    else if (navparser::NavEngine::current_priority == ammo)
        navparser::NavEngine::cancelPath();
    return false;
}

enum slots
{
    primary   = 1,
    secondary = 2,
    melee     = 3
};
static int slot;

// Check if an area is valid for stay near. the Third parameter is to save some performance.
bool isAreaValidForStayNear(Vector ent_origin, CNavArea *area, bool fix_local_z = true)
{
    if (fix_local_z)
        ent_origin.z += navparser::PLAYER_JUMP_HEIGHT;
    auto area_origin = area->m_center;
    area_origin.z += navparser::PLAYER_JUMP_HEIGHT;

    // Do all the distance checks
    float distance = ent_origin.DistToSqr(area_origin);

    // Too close
    if (distance < selected_config.min_very_dangerous * selected_config.min_very_dangerous)
        return false;
    // Blacklisted
    if (navparser::NavEngine::getFreeBlacklist()->find(area) != navparser::NavEngine::getFreeBlacklist()->end())
        return false;
    // Too far away
    if (distance > selected_config.max * selected_config.max)
        return false;
    // Attempt to vischeck
    if (!IsVectorVisibleNavigation(ent_origin, area_origin))
        return false;
    return true;
}

// Actual logic, used to de-duplicate code
bool stayNearTarget(std::optional<Vector> ent_origin)
{
    // No origin recorded, don't bother
    if (!ent_origin)
        return false;

    // Add the vischeck height
    ent_origin->z += navparser::PLAYER_JUMP_HEIGHT;

    // Use std::pair to avoid using the distance functions more than once
    std::vector<std::pair<CNavArea *, float>> good_areas{};

    for (auto &area : navparser::NavEngine::getNavFile()->m_areas)
    {
        auto area_origin = area.m_center;

        // Is this area valid for stay near purposes?
        if (!isAreaValidForStayNear(*ent_origin, &area, false))
            continue;

        float distance = (*ent_origin).DistToSqr(area_origin);
        // Good area found
        good_areas.emplace_back(&area, distance);
    }
    // Sort based on distance
    if (selected_config.prefer_far)
        std::sort(good_areas.begin(), good_areas.end(), [](std::pair<CNavArea *, float> a, std::pair<CNavArea *, float> b) { return a.second > b.second; });
    else
        std::sort(good_areas.begin(), good_areas.end(), [](std::pair<CNavArea *, float> a, std::pair<CNavArea *, float> b) { return a.second < b.second; });

    // If we're not already pathing we should reallign with the center of the area
    bool should_path_to_local = !navparser::NavEngine::isPathing();
    // Try to path to all the good areas, based on distance
    for (auto &area : good_areas)
        if (navparser::NavEngine::navTo(area.first->m_center, staynear, true, should_path_to_local))
            return true;

    return false;
}

// A bunch of basic checks to ensure we don't try to target an invalid entity
bool isStayNearTargetValid(CachedEntity *ent)
{
    return CE_VALID(ent) && g_pPlayerResource->isAlive(ent->m_IDX) && ent->m_IDX != g_pLocalPlayer->entity_idx && g_pLocalPlayer->team != ent->m_iTeam() && player_tools::shouldTarget(ent) && !IsPlayerInvisible(ent) && !IsPlayerInvulnerable(ent);
}

// Try to stay near enemies and stalk them (or in case of sniper, try to stay far from them
// and snipe them)
bool stayNear()
{
    PROF_SECTION(stayNear)
    static Timer staynear_cooldown{};
    static CachedEntity *previous_target = nullptr;

    // Stay near is expensive so we have to cache. We achieve this by only checking a pre-determined amount of players every
    // CreateMove
    constexpr int MAX_STAYNEAR_CHECKS_RANGE = 3;
    constexpr int MAX_STAYNEAR_CHECKS_CLOSE = 2;
    static int lowest_check_index           = 0;

    // Stay near is off
    if (!stay_near)
        return false;
    // Don't constantly path, it's slow.
    // Far range classes do not need to repath nearly as often as close range ones.
    if (!staynear_cooldown.test_and_set(selected_config.prefer_far ? *repath_delay : *repath_delay))
        return navparser::NavEngine::current_priority == staynear;

    // Too high priority, so don't try
    if (navparser::NavEngine::current_priority > staynear)
        return false;

    // Check and use our previous target if available
    if (isStayNearTargetValid(previous_target))
    {
        auto ent_origin = previous_target->m_vecDormantOrigin();
        if (ent_origin)
        {
            // Check if current target area is valid
            if (navparser::NavEngine::isPathing())
            {
                auto crumbs = navparser::NavEngine::getCrumbs();
                // We cannot just use the last crumb, as it is always nullptr
                if (crumbs->size() > 1)
                {
                    auto last_crumb = (*crumbs)[crumbs->size() - 2];
                    // Area is still valid, stay on it
                    if (isAreaValidForStayNear(*ent_origin, last_crumb.navarea))
                        return true;
                }
            }
            // Else Check our origin for validity (Only for ranged classes)
            else if (selected_config.prefer_far && isAreaValidForStayNear(*ent_origin, navparser::NavEngine::findClosestNavSquare(LOCAL_E->m_vecOrigin())))
                return true;
        }
        // Else we try to path again
        if (stayNearTarget(previous_target->m_vecDormantOrigin()))
            return true;
        // Failed, invalidate previous target and try others
        previous_target = nullptr;
    }

    auto advance_count = selected_config.prefer_far ? MAX_STAYNEAR_CHECKS_RANGE : MAX_STAYNEAR_CHECKS_CLOSE;

    // Ensure it is in bounds and also wrap around
    if (lowest_check_index > g_IEngine->GetMaxClients())
        lowest_check_index = 0;

    int calls = 0;
    // Test all entities
    for (int i = lowest_check_index; i <= g_IEngine->GetMaxClients(); i++)
    {
        if (calls >= advance_count)
            break;
        calls++;
        lowest_check_index++;
        CachedEntity *ent = ENTITY(i);
        if (!isStayNearTargetValid(ent))
        {
            calls--;
            continue;
        }
        // Succeeded pathing
        if (stayNearTarget(ent->m_vecDormantOrigin()))
        {
            previous_target = ent;
            return true;
        }
    }

    // Stay near failed to find any good targets, add extra delay
    staynear_cooldown.last += std::chrono::seconds(3);
    return false;
}

// Basically the same as isAreaValidForStayNear, but some restrictions lifted.
bool isAreaValidForSnipe(Vector ent_origin, Vector area_origin, bool fix_sentry_z = true)
{
    if (fix_sentry_z)
        ent_origin.z += 40.0f;
    area_origin.z += navparser::PLAYER_JUMP_HEIGHT;

    float distance = ent_origin.DistToSqr(area_origin);
    // Too close to be valid
    if (distance <= (1100.0f + navparser::HALF_PLAYER_WIDTH) * (1100.0f + navparser::HALF_PLAYER_WIDTH))
        return false;
    // Fails vischeck, bad
    if (!IsVectorVisibleNavigation(area_origin, ent_origin))
        return false;
    return true;
}

// Try to snipe the sentry
bool tryToSnipe(CachedEntity *ent)
{
    auto ent_origin = GetBuildingPosition(ent);
    // Add some z to dormant sentries as it only returns origin
    if (CE_BAD(ent))
        ent_origin.z += 40.0f;

    std::vector<std::pair<CNavArea *, float>> good_areas;
    for (auto &area : navparser::NavEngine::getNavFile()->m_areas)
    {
        // Not usable
        if (!isAreaValidForSnipe(ent_origin, area.m_center, false))
            continue;
        good_areas.emplace_back(&area, area.m_center.DistToSqr(ent_origin));
    }

    // Sort based on distance
    if (selected_config.prefer_far)
        std::sort(good_areas.begin(), good_areas.end(), [](std::pair<CNavArea *, float> a, std::pair<CNavArea *, float> b) { return a.second > b.second; });
    else
        std::sort(good_areas.begin(), good_areas.end(), [](std::pair<CNavArea *, float> a, std::pair<CNavArea *, float> b) { return a.second < b.second; });

    for (auto &area : good_areas)
        if (navparser::NavEngine::navTo(area.first->m_center, snipe_sentry))
            return true;
    return false;
}

// Is our target valid?
bool isSnipeTargetValid(CachedEntity *ent)
{
    return CE_VALID(ent) && ent->m_bAlivePlayer() && ent->m_iTeam() != g_pLocalPlayer->team && ent->m_iClassID() == CL_CLASS(CObjectSentrygun);
}

// Try to Snipe sentries
bool snipeSentries()
{
    static Timer sentry_snipe_cooldown;
    static CachedEntity *previous_target = nullptr;

    if (!snipe_sentries)
        return false;

    // Sentries don't move often, so we can use a slightly longer timer
    if (!sentry_snipe_cooldown.test_and_set(5000))
        return navparser::NavEngine::current_priority == snipe_sentry || isSnipeTargetValid(previous_target);

    if (isSnipeTargetValid(previous_target))
    {
        auto crumbs = navparser::NavEngine::getCrumbs();
        // We cannot just use the last crumb, as it is always nullptr
        if (crumbs->size() > 1)
        {
            auto last_crumb = (*crumbs)[crumbs->size() - 2];
            // Area is still valid, stay on it
            if (isAreaValidForSnipe(GetBuildingPosition(previous_target), last_crumb.navarea->m_center))
                return true;
        }
        if (tryToSnipe(previous_target))
            return true;
    }

    // Make sure we don't try to do it on shortrange classes unless specified
    if (!snipe_sentries_shortrange && (g_pLocalPlayer->clazz == tf_scout || g_pLocalPlayer->clazz == tf_pyro))
        return false;

    for (int i = g_IEngine->GetMaxClients() + 1; i < MAX_ENTITIES; i++)
    {
        CachedEntity *ent = ENTITY(i);
        // Invalid sentry
        if (!isSnipeTargetValid(ent))
            continue;
        // Succeeded in trying to snipe it
        if (tryToSnipe(ent))
        {
            previous_target = ent;
            return true;
        }
    }
    return false;
}

int getCarriedBuilding()
{
    if (CE_INT(LOCAL_E, netvar.m_bCarryingObject))
        return HandleToIDX(CE_INT(LOCAL_E, netvar.m_hCarriedObject));
    for (int i = 1; i < MAX_ENTITIES; i++)
    {
        auto ent = ENTITY(i);
        if (CE_BAD(ent) || ent->m_Type() != ENTITY_BUILDING)
            continue;
        if (HandleToIDX(CE_INT(ent, netvar.m_hBuilder)) != LOCAL_E->m_IDX)
            continue;
        if (!CE_INT(ent, netvar.m_bPlacing))
            continue;
        return i;
    }
    return -1;
}

enum building
{
    dispenser = 0,
    sentry    = 2
};

static int build_attempts = 0;
static bool buildBuilding(int building)
{
    // Blacklist this spot and refresh the building spots
    if (build_attempts >= 15)
    {
        (*navparser::NavEngine::getFreeBlacklist())[navparser::NavEngine::findClosestNavSquare(g_pLocalPlayer->v_Origin)] = navparser::BlacklistReason_enum::BAD_BUILDING_SPOT;
        refreshBuildingSpots(true);
        current_building_spot.Invalidate();
        build_attempts = 0;
        return false;
    }
    // Make sure we have right amount of ammo
    int required = (HasGunslinger(LOCAL_E) || building == dispenser) ? 100 : 130;
    if (CE_INT(LOCAL_E, netvar.m_iAmmo + 12) < required)
        return getAmmo(true);

    // Try to build! we are close enough
    if (current_building_spot.IsValid() && current_building_spot.DistTo(g_pLocalPlayer->v_Origin) <= 200.0f)
    {
        // TODO: Rotate to the correct angle automatically, also rotate building itself to face enemies ?
        // Viewangle correction to increase build success chance
        current_user_cmd->viewangles.x = 20.0f;
        current_user_cmd->viewangles.y += 2.0f;

        // Gives us 4 1/2 seconds to build
        static Timer attempt_timer;
        if (attempt_timer.test_and_set(300))
            build_attempts++;

        if (getCarriedBuilding() == -1)
        {
            static Timer command_timer;
            if (command_timer.test_and_set(100))
                g_IEngine->ClientCmd_Unrestricted(format_cstr("build %d", building).get());
        }
        else if (CE_INT(ENTITY(getCarriedBuilding()), netvar.m_bCanPlace))
            current_user_cmd->buttons |= IN_ATTACK;
        return true;
    }
    else
        return navToSentrySpot();

    return false;
}

static bool buildingNeedsToBeSmacked(CachedEntity *ent)
{
    if (CE_BAD(ent))
        return false;

    if (CE_INT(ent, netvar.iUpgradeLevel) != 3 || ent->m_iHealth() / ent->m_iMaxHealth() <= 0.80f)
        return true;
    if (ent->m_iClassID() == CL_CLASS(CObjectSentrygun))
    {
        int max_ammo = 0;
        switch (CE_INT(ent, netvar.iUpgradeLevel))
        {
        case 1:
            max_ammo = 150;
            break;
        case 2:
        case 3:
            max_ammo = 200;
            break;
        }

        return CE_INT(ent, netvar.m_iAmmoShells) / max_ammo <= 0.50f;
    }
    return false;
}

static bool smackBuilding(CachedEntity *ent)
{
    if (CE_BAD(ent))
        return false;
    if (!CE_INT(LOCAL_E, netvar.m_iAmmo + 12))
        return getAmmo(true);

    if (ent->m_flDistance() <= 100.0f && g_pLocalPlayer->weapon_mode == weapon_melee)
    {
        AimAt(g_pLocalPlayer->v_Eye, GetBuildingPosition(ent), current_user_cmd);
        current_user_cmd->buttons |= IN_ATTACK;
    }
    else
        return navparser::NavEngine::navTo(*ent->m_vecDormantOrigin(), engineer);
    return true;
}

static bool runEngineerLogic()
{
    if (!isEngieMode())
        return false;

    // Already have a sentry
    if (CE_VALID(mySentry) && mySentry->m_bAlivePlayer())
    {
        if (HasGunslinger(LOCAL_E))
        {
            // Too far away, destroy it
            if (mySentry->m_flDistance() >= 1800.0f)
            {
                // If we have a valid building
                if (mySentry->m_Type() == CL_CLASS(CObjectSentrygun))
                    g_IEngine->ClientCmd_Unrestricted("destroy 2");
            }
            // Return false so we run another task
            return false;
        }
        else
        {
            // Try to smack sentry first
            if (buildingNeedsToBeSmacked(mySentry))
                return smackBuilding(mySentry);
            else
            {
                // We put dispenser by sentry #turtlegang
                if (CE_BAD(myDispenser))
                    return buildBuilding(dispenser);
                else
                {
                    // We already have a dispenser, see if it needs to be smacked
                    if (buildingNeedsToBeSmacked(myDispenser))
                        return smackBuilding(myDispenser);
                }
            }
        }
    }
    else
        // Try to build a sentry
        return buildBuilding(sentry);
    return false;
}

enum capture_type
{
    no_capture,
    ctf,
    payload,
    controlpoints
};
static capture_type current_capturetype = no_capture;

std::optional<Vector> getCtfGoal(int our_team, int enemy_team)
{
    // Get Flag related information
    auto status   = flagcontroller::getStatus(enemy_team);
    auto position = flagcontroller::getPosition(enemy_team);
    auto carrier  = flagcontroller::getCarrier(enemy_team);

    auto status_friendly   = flagcontroller::getStatus(our_team);
    auto position_friendly = flagcontroller::getPosition(our_team);
    auto carrier_friendly  = flagcontroller::getCarrier(our_team);

    // No flags :(
    if (!position || !position_friendly)
        return std::nullopt;

    current_capturetype = ctf;

    // Defend our intel and help other bots escort enemy intel, this is priority over capturing
    if (defend_intel)
    {
        // Goto the enemy who took our intel - highest priority
        if (status_friendly == TF_FLAGINFO_STOLEN)
        {
            if (player_tools::shouldTargetSteamId(carrier_friendly->player_info.friendsID))
                return carrier_friendly->m_vecDormantOrigin();
        }
        // Standby a dropped friendly flag - medium priority
        if (status_friendly == TF_FLAGINFO_DROPPED)
        {
            // Dont spam nav if we are already there
            if ((*position_friendly).DistTo(LOCAL_E->m_vecOrigin()) < 100.0f)
            {
                overwrite_capture = true;
                return std::nullopt;
            }
            return position_friendly;
        }
        // Assist other bots with capturing - low priority
        if (status == TF_FLAGINFO_STOLEN && carrier != LOCAL_E)
        {
            if (!player_tools::shouldTargetSteamId(carrier->player_info.friendsID))
                return carrier->m_vecDormantOrigin();
        }
    }

    // Flag is taken by us
    if (status == TF_FLAGINFO_STOLEN)
    {
        // CTF is the current capture type.
        if (carrier == LOCAL_E)
        {
            // Return our capture point location
            auto team_flag = flagcontroller::getFlag(our_team);
            return team_flag.spawn_pos;
        }
    }
    // Get the flag if not taken by us already
    else
    {
        return position;
    }
    return std::nullopt;
}

std::optional<Vector> getPayloadGoal(int our_team)
{
    auto position = plcontroller::getClosestPayload(g_pLocalPlayer->v_Origin, our_team);
    // No payloads found :(
    if (!position)
        return std::nullopt;
    current_capturetype = payload;

    // Adjust position so it's not floating high up, provided the local player is close.
    if (LOCAL_E->m_vecOrigin().DistTo(*position) <= 150.0f)
        (*position).z = LOCAL_E->m_vecOrigin().z;
    // If close enough, don't move (mostly due to lifts)
    if ((*position).DistTo(LOCAL_E->m_vecOrigin()) <= 15.0f)
    {
        overwrite_capture = true;
        return std::nullopt;
    }
    else
        return position;
}

std::optional<Vector> getControlPointGoal(int our_team)
{
    static Vector previous_position(0.0f);
    static Vector randomized_position(0.0f);

    auto position = cpcontroller::getClosestControlPoint(g_pLocalPlayer->v_Origin, our_team);
    // No points found :(
    if (!position)
        return std::nullopt;
    if ((*position).DistTo(LOCAL_E->m_vecOrigin()) < 100.0f)
    {
        overwrite_capture = true;
        return std::nullopt;
    }
    if (*randomize_cp)
    {
        // Randomize where on the point we walk a bit so bots don't just stand there
        if (previous_position != *position || !navparser::NavEngine::isPathing())
        {
            previous_position   = *position;
            randomized_position = *position;
            randomized_position.x += RandomFloat(0.0f, 50.0f);
            randomized_position.y += RandomFloat(0.0f, 50.0f);
        }

        current_capturetype = controlpoints;
        // Try to navigate
        return randomized_position;
    }
    else
        return position;
}

// Try to capture objectives
bool captureObjectives()
{
    static Timer capture_timer;
    static Vector previous_target(0.0f);
    // Not active or on a doomsday map
    if ((!capture_objectives) && capture_timer.test_and_set(*capture_delay) || is_doomsday)
        return false;

    // Priority too high, don't try
    if (navparser::NavEngine::current_priority > capture)
        return false;

    // Where we want to go
    std::optional<Vector> target;

    int our_team   = g_pLocalPlayer->team;
    int enemy_team = our_team == TEAM_BLU ? TEAM_RED : TEAM_BLU;

    current_capturetype = no_capture;
    overwrite_capture   = false;

    // Run ctf logic
    target = getCtfGoal(our_team, enemy_team);
    // Not ctf, run payload
    if (current_capturetype == no_capture)
    {
        target = getPayloadGoal(our_team);
        // Not payload, run control points
        if (current_capturetype == no_capture)
        {
            target = getControlPointGoal(our_team);
        }
    }

    // Overwritten, for example because we are currently on the payload, cancel any sort of pathing and return true
    if (overwrite_capture)
    {
        navparser::NavEngine::cancelPath();
        return true;
    }
    // No target, bail and set on cooldown
    else if (!target)
    {
        capture_timer.update();
        return false;
    }
    // If priority is not capturing or we have a new target, try to path there
    else if (navparser::NavEngine::current_priority != capture || *target != previous_target)
    {
        if (navparser::NavEngine::navTo(*target, capture, true, !navparser::NavEngine::isPathing()))
        {
            previous_target = *target;
            return true;
        }
        else
            capture_timer.update();
    }
    return false;
}

// Roam around map
bool doRoam()
{
    static Timer roam_timer;
    // Don't path constantly
    if (!roam_timer.test_and_set(*repath_delay))
        return false;

    // Defend our objective if possible
    if (roam_defend)
    {
        int enemy_team = g_pLocalPlayer->team == TEAM_BLU ? TEAM_RED : TEAM_BLU;

        std::optional<Vector> target;
        target = getPayloadGoal(enemy_team);
        if (!target)
            target = getControlPointGoal(enemy_team);
        if (target)
        {
            if ((*target).DistTo(g_pLocalPlayer->v_Origin) <= 250.0f)
            {
                navparser::NavEngine::cancelPath();
                return true;
            }
            if (navparser::NavEngine::navTo(*target, patrol))
                return true;
        }
    }

    // No sniper spots :shrug:
    if (sniper_spots.empty())
        return false;
    // Don't overwrite current roam
    if (navparser::NavEngine::current_priority == patrol)
        return false;
    // Max 10 attempts
    for (int attempts = 0; attempts < 10 && attempts < sniper_spots.size(); ++attempts)
    {
        // Get a random sniper spot
        auto random = select_randomly(sniper_spots.begin(), sniper_spots.end());
        // Try to nav there
        if (navparser::NavEngine::navTo(*random, patrol))
            return true;
    }

    return false;
}

static bool isMeleeNeeded(CachedEntity *ent)
{
    if (!ent->m_bAlivePlayer() || IsPlayerInvulnerable(ent) || IsPlayerInvisible(ent))
        return false;
    int weapon_idx = CE_INT(ent, netvar.hActiveWeapon) & 0xFFF;
    if (IDX_GOOD(weapon_idx))
    {
        CachedEntity *weapon = ENTITY(weapon_idx);
        if (CE_GOOD(weapon))
            return GetWeaponMode(ent) == weaponmode::weapon_melee || IsPlayerResistantToCurrentClass(ent, true) || weapon->m_iClassID() == CL_CLASS(CTFFlameThrower) || weapon->m_iClassID() == CL_CLASS(CTFFists) || weapon->m_iClassID() == CL_CLASS(CTFCompoundBow) || weapon->m_iClassID() == CL_CLASS(CTFShotgun_HWG);
    }
    return false;
}

static std::pair<CachedEntity *, float> nearest;

static void autoJump()
{
    if (!autojump)
        return;
    if (!last_jump.test_and_set(200) || CE_BAD(nearest.first))
        return;

    if (nearest.second <= *jump_distance && !isMeleeNeeded(nearest.first))
    {
        current_user_cmd->buttons |= IN_JUMP | IN_DUCK;

        if (!hacks::shared::antiaim::isEnabled())
        {
            // Do a flip to avoid getting stuck on edges
            Vector flip{ g_pLocalPlayer->v_OrigViewangles.x, g_pLocalPlayer->v_OrigViewangles.y + 180.0f, g_pLocalPlayer->v_Eye.z };
            fClampAngle(flip);

            current_user_cmd->viewangles = flip;
            *bSendPackets                = true;
        }
    }
}

static void meleeNearest()
{
    if (!melee_mode)
        return;
    if (slot == melee && !isEngieMode() && CE_GOOD(nearest.first) && nearest.first->m_bAlivePlayer() && nearest.first->IsVisible())
    {
        if (!hacks::shared::aimbot::isAiming())
            AimAtHitbox(nearest.first, spine_2, current_user_cmd);
        if (!melee_timer.test_and_set(nearest.second <= 200 ? 100 : *repath_delay / 2) && navparser::NavEngine::isPathing())
            return;
        warp::forced_warp = nearest.second >= 40.0f && warp::warp_amount >= 3;
        navparser::NavEngine::navTo(*nearest.first->m_vecDormantOrigin(), melee_enemy);
    }
    else
        warp::forced_warp = false;
}

static slots getBestSlot(slots active_slot)
{
    nearest = getNearestPlayerDistance(false, true);
    switch (g_pLocalPlayer->clazz)
    {
    case tf_scout:
    case tf_heavy:
        return primary;
    case tf_medic:
        return secondary;
    case tf_spy:
    {
        if (nearest.second > 200 && active_slot == primary)
            return active_slot;
        else if (nearest.second >= 250)
            return primary;
        else
            return melee;
    }
    case tf_sniper:
    {
        // Have a Huntsman, Always use primary
        if (HasWeapon(LOCAL_E, 56) || HasWeapon(LOCAL_E, 1005) || HasWeapon(LOCAL_E, 1092))
            return primary;
        if (melee_mode && nearest.second <= 350 && isMeleeNeeded(nearest.first) && nearest.first->IsVisible())
            return melee;
        if (nearest.second <= 300 && nearest.first->m_iHealth() < 75)
            return secondary;
        else if (nearest.second <= 350 && nearest.first->m_iHealth() < 75)
            return active_slot;
        else
            return primary;
    }
    case tf_pyro:
    {
        if (nearest.second > 450 && active_slot == secondary)
            return active_slot;
        if (melee_mode && nearest.second <= 300 && isMeleeNeeded(nearest.first) && nearest.first->IsVisible())
            return melee;
        else if (nearest.second <= 550)
            return primary;
        else
            return secondary;
    }
    case tf_soldier:
    {
        if (melee_mode && nearest.second <= 300 && isMeleeNeeded(nearest.first) && nearest.first->IsVisible())
            return melee;
        if (nearest.second <= 200)
            return secondary;
        else if (nearest.second <= 300)
            return active_slot;
        else
            return primary;
    }
    case tf_engineer:
    {
        if (((CE_GOOD(mySentry) && mySentry->m_flDistance() <= 300) || (CE_GOOD(myDispenser) && myDispenser->m_flDistance() <= 300)) || (current_building_spot.IsValid() && current_building_spot.DistTo(g_pLocalPlayer->v_Origin) <= 300.0f))
        {
            if (active_slot >= melee)
                return active_slot;
            else
                return melee;
        }
        else if (nearest.second <= 500)
            return primary;
        else
            return secondary;
    }
    default:
    {
        if (melee_mode && nearest.second <= 300 && isMeleeNeeded(nearest.first) && nearest.first->IsVisible())
            return melee;
        if (nearest.second <= 400)
            return secondary;
        else if (nearest.second <= 500)
            return active_slot;
        else
            return primary;
    }
    }
}

static void updateSlot()
{
    static Timer slot_timer{};
    if (!primary_only || !slot_timer.test_and_set(300))
        return;
    if (CE_GOOD(LOCAL_E) && CE_GOOD(LOCAL_W) && !g_pLocalPlayer->life_state)
    {
        IClientEntity *weapon = RAW_ENT(LOCAL_W);
        if (re::C_BaseCombatWeapon::IsBaseCombatWeapon(weapon))
        {
            slot        = re::C_BaseCombatWeapon::GetSlot(weapon) + 1;
            int newslot = getBestSlot(static_cast<slots>(slot));
            if (slot != newslot)
                g_IEngine->ClientCmd_Unrestricted(format("slot", newslot).c_str());
        }
    }
}

void CreateMove()
{
    if (!enabled || !navparser::NavEngine::isReady())
        return;
    if (CE_BAD(LOCAL_E) || !LOCAL_E->m_bAlivePlayer() || HasCondition<TFCond_HalloweenGhostMode>(LOCAL_E))
        return;

    refreshSniperSpots();
    refreshLocalBuildings();
    refreshBuildingSpots();

    if (distance_config_custom)
    {
        selected_config = { *distance_config_custom_min_very_dangerous, *distance_config_custom_min_slightly_dangerous, *distance_config_custom_max_slight_distance, *distance_config_custom_prefer_far };
    }
    else
    {
        // Update the distance config
        switch (g_pLocalPlayer->clazz)
        {
        case tf_scout:
        case tf_heavy:
        case tf_engineer:
            selected_config = isEngieMode() ? HasGunslinger(LOCAL_E) ? CONFIG_GUNSLINGER_ENGINEER : CONFIG_ENGINEER : CONFIG_SHORT_RANGE;
            break;
        case tf_sniper:
            selected_config = g_pLocalPlayer->weapon()->m_iClassID() == CL_CLASS(CTFCompoundBow) ? CONFIG_MID_RANGE : CONFIG_LONG_RANGE;
            break;
        default:
            selected_config = CONFIG_MID_RANGE;
        }
    }

    updateSlot();
    autoJump();
    meleeNearest();

    // Try getting health first of all
    if (getHealth())
        return;
    // If we aren't getting health, get ammo
    else if (getAmmo())
        return;
    // Try to run engineer logic
    else if (runEngineerLogic())
        return;
    // Try to capture objectives
    else if (captureObjectives())
        return;
    // Try to snipe sentries
    else if (snipeSentries())
        return;
    // Try to stalk enemies
    else if (stayNear())
        return;
    // Try to get health with a lower prioritiy
    else if (getHealth(true))
        return;
    // We have nothing else to do, roam
    else if (doRoam())
        return;
}

void LevelInit()
{
    // Make it run asap
    refresh_sniperspots_timer.last -= std::chrono::seconds(60);
    sniper_spots.clear();
    is_doomsday = false;

    // Doomsday sucks
    // TODO: add proper doomsday implementation
    auto map_name = std::string(g_IEngine->GetLevelName());
    if (g_IEngine->GetLevelName() && map_name.find("sd_doomsday") != map_name.npos)
        is_doomsday = true;
}
#if ENABLE_VISUALS
void Draw()
{
    if (!draw_blacklist || !navparser::NavEngine::isReady())
        return;
    for (auto &area : *navparser::NavEngine::getFreeBlacklist())
    {
        Vector out;

        if (draw::WorldToScreen(area.first->m_center, out))
            draw::Rectangle(out.x - 2.0f, out.y - 2.0f, 4.0f, 4.0f, colors::red_s);
    }
}
#endif

static InitRoutine init([]() {
    EC::Register(EC::CreateMove, CreateMove, "navbot_cm");
    EC::Register(EC::LevelInit, LevelInit, "navbot_levelinit");
#if ENABLE_VISUALS
    EC::Register(EC::Draw, Draw, "navbot_draw");
#endif
    LevelInit();
});

} // namespace hacks::tf2::NavBot
