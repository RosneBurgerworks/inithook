/*
 * hitrate.cpp
 *
 *  Created on: Aug 16, 2017
 *      Author: nullifiedcat
 */

#include "common.hpp"
#include <hacks/Aimbot.hpp>
#include <settings/Bool.hpp>
#include "MiscTemporary.hpp"
#include "init.hpp"
#include "AntiAntiAim.hpp"
#include "hitrate.hpp"

namespace hitrate
{

static settings::Boolean hitrate_debug{ "hitrate.log", "false" };

#define Debug(...)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            \
    if (*hitrate_debug)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     \
    logging::Info("hitrate.cpp: " __VA_ARGS__)

int lastammo{ 0 };

int count_shots{ 0 };
int count_hits_sniper{ 0 };
int count_hits{ 0 };
int count_hits_head{ 0 };

CatCommand clear_hirate("debug_hitrate_clear", "Clear hitrate", []() {
    count_shots       = 0;
    count_hits        = 0;
    count_hits_sniper = 0;
    count_hits_head   = 0;
});

CatCommand debug_hitrate("debug_hitrate", "Debug hitrate", []() {
    int p1 = 0;
    int p2 = 0;
    if (count_shots)
    {
        p1 = float(count_hits) / float(count_shots) * 100.0f;
    }
    if (count_hits)
    {
        p2 = float(count_hits_head) / float(count_hits) * 100.0f;
    }
    logging::Info("%d / %d (%d%%)", count_hits, count_shots, p1);
    logging::Info("%d / %d (%d%%)", count_hits_head, count_hits_sniper, p2);
});

CatCommand debug_ammo("debug_ammo", "Debug ammo", []() {
    for (int i = 0; i < 4; i++)
    {
        logging::Info("%d %d", i, CE_INT(LOCAL_E, netvar.m_iAmmo + i * 4));
    }
});

// If this is true, Update() will consider increasing the brutenum soon if the shot was a miss
bool resolve_soon[PLAYER_ARRAY_SIZE];
std::array<Timer, PLAYER_ARRAY_SIZE> resolve_timer{};

static int aimbot_target_idx   = -1;
static bool aimbot_target_body = false;
static Timer aimbot_shot{};

void OnShot()
{
    ++count_shots;
    resolve_soon[aimbot_target_idx] = true;
    resolve_timer[aimbot_target_idx].update();
}

void OnHit(bool crit, int idx, bool is_sniper)
{
    idx = g_IEngine->GetPlayerForUserID(idx);
    count_hits++;
    if (is_sniper)
        count_hits_sniper++;
    if (crit)
        count_hits_head++;
    if (crit || aimbot_target_body)
    {
        if (idx == aimbot_target_idx)
        {
            auto ent = ENTITY(idx);
            if (CE_GOOD(ent))
            {
                Debug("hit registered on %s", ent->player_info.name);
                hacks::shared::anti_anti_aim::resolver_map[ent->player_info.friendsID].hits_in_a_row++;
                resolve_soon[idx] = false;
            }
        }
    }
}

void AimbotShot(int idx, bool target_body)
{
    aimbot_shot.update();
    aimbot_target_idx  = idx;
    aimbot_target_body = target_body;
    Debug("aimbot shot registered on idx %d", idx);
}

void Update()
{
    CachedEntity *weapon = LOCAL_W;
    // Hitscan only
    if (CE_GOOD(weapon) && g_pLocalPlayer->weapon_mode == weapon_hitscan)
    {

        int ammo = CE_INT(LOCAL_E, netvar.m_iAmmo + 4);
        if (ammo < lastammo && !aimbot_shot.check(500) && aimbot_target_idx != -1)
            OnShot();
        lastammo = ammo;
        // Resolver
        if (LOCAL_W->m_iClassID() == CL_CLASS(CTFSniperRifle) || LOCAL_W->m_iClassID() == CL_CLASS(CTFSniperRifleDecap))
        {
            auto ch = (INetChannel *) g_IEngine->GetNetChannelInfo();
            if (ch)
                for (int i = 1; i < PLAYER_ARRAY_SIZE; i++)
                {
                    if (!resolve_soon[i])
                        continue;
                    // Get general latency and add a safety net
                    unsigned int delay = (ch->GetLatency(FLOW_OUTGOING) + ch->GetLatency(FLOW_INCOMING)) * 1000.0f + 100.0f;
                    if (resolve_timer[i].check(delay))
                    {
                        resolve_soon[i] = false;
                        hacks::shared::anti_anti_aim::increaseBruteNum(i);
                    }
                }
        }
    }
}

class HurtListener : public IGameEventListener
{
public:
    virtual void FireGameEvent(KeyValues *event)
    {
        if (!strcmp("player_hurt", event->GetName()))
        {
            if (g_IEngine->GetPlayerForUserID(event->GetInt("attacker")) == g_IEngine->GetLocalPlayer() && CE_GOOD(LOCAL_W))
            {
                if (LOCAL_W->m_iClassID() == CL_CLASS(CTFSniperRifle) || LOCAL_W->m_iClassID() == CL_CLASS(CTFSniperRifleDecap))
                    OnHit(event->GetBool("crit"), event->GetInt("userid"), true);
                else if (g_pLocalPlayer->weapon_mode == weapon_hitscan)
                    OnHit(false, event->GetInt("userid"), false);
            }
        }
    }
};

static HurtListener listener{};

InitRoutine init([]() {
    g_IGameEventManager->AddListener(&listener, false);
    EC::Register(
        EC::Shutdown, []() { g_IGameEventManager->RemoveListener(&listener); }, "shutdown_hitrate");
});
} // namespace hitrate
