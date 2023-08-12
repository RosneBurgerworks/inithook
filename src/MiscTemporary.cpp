/*
  Created by Jenny White on 29.04.18.
  Copyright (c) 2018 nullworks. All rights reserved.
*/

#include "MiscTemporary.hpp"
#include "Warp.hpp"
#include "nospread.hpp"

std::array<Timer, 32> timers{};
std::array<int, 32> bruteint{};

int spectator_target;
int anti_balance_attempts = 0;
int namesteal_target_id   = 0;
bool firstcm              = false;
float prevflow            = 0.0f;
int prevflowticks         = 0;
int stored_buttons        = 0;
bool calculated_can_shoot = false;
bool ignoredc             = false;
bool do_fakelag           = false;

bool *bSendPackets{ nullptr };
bool ignoreKeys{ false };
float backup_lerp = 0.0f;

std::string target_name;
std::string current_server_name;

settings::Boolean clean_screenshots{ "visual.clean-screenshots", "false" };
settings::Boolean nolerp{ "misc.no-lerp", "false" };
settings::Boolean no_zoom{ "remove.zoom", "false" };
settings::Boolean disable_visuals{ "visual.disable", "false" };
settings::Int fakelag_amount{ "misc.fakelag", "0" };

static settings::Boolean no_scope{ "remove.scope", "false" };

DetourHook cl_warp_sendmovedetour;
DetourHook cl_nospread_sendmovedetour;

CatCommand spectate("spectate", "Spectate", [](const CCommand &args) {
    if (args.ArgC() < 1)
    {
        spectator_target = 0;
        return;
    }
    int id;
    try
    {
        id = std::stoi(args.Arg(1));
    }
    catch (const std::exception &e)
    {
        logging::Info("Error while setting spectate target. Error: %s", e.what());
        id = 0;
    }
    if (!id)
        spectator_target = 0;
    else
    {
        spectator_target = g_IEngine->GetPlayerForUserID(id);
    }
});

static float slow_change_dist_y{};
static float slow_change_dist_p{};
void DoSlowAim(Vector &input_angle, float speed)
{
    auto viewangles = current_user_cmd->viewangles;

    // Yaw
    if (viewangles.y != input_angle.y)
    {
        // Check if input angle and user angle are on opposing sides of yaw so
        // we can correct for that
        bool slow_opposing = false;
        if ((input_angle.y < -90 && viewangles.y > 90) || (input_angle.y > 90 && viewangles.y < -90))
            slow_opposing = true;

        // Direction
        bool slow_dir = false;
        if (slow_opposing)
        {
            if (input_angle.y > 90 && viewangles.y < -90)
                slow_dir = true;
        }
        else if (viewangles.y > input_angle.y)
            slow_dir = true;

        // Speed, check if opposing. We dont get a new distance due to the
        // opposing sides making the distance spike, so just cheap out and reuse
        // our last one.
        if (!slow_opposing)
            slow_change_dist_y = std::abs(viewangles.y - input_angle.y) / speed;

        // Move in the direction of the input angle
        if (slow_dir)
            input_angle.y = viewangles.y - slow_change_dist_y;
        else
            input_angle.y = viewangles.y + slow_change_dist_y;
    }

    // Pitch
    if (viewangles.x != input_angle.x)
    {
        // Get speed
        slow_change_dist_p = std::abs(viewangles.x - input_angle.x) / speed;

        // Move in the direction of the input angle
        if (viewangles.x > input_angle.x)
            input_angle.x = viewangles.x - slow_change_dist_p;
        else
            input_angle.x = viewangles.x + slow_change_dist_p;
    }

    // Clamp as we changed angles
    fClampAngle(input_angle);
}

typedef bool (*ShouldInterpolate_t)(IClientEntity *);
DetourHook shouldinterpolate_detour{};

bool ShouldInterpolate_hook(IClientEntity *entity)
{
    if (nolerp)
    {
        if (entity && IDX_GOOD(entity->entindex()))
        {
            CachedEntity *ent = ENTITY(entity->entindex());
            if (ent->m_Type() == ENTITY_PLAYER && ent->m_IDX != g_pLocalPlayer->entity_idx)
                return false;
        }
    }
    ShouldInterpolate_t original = (ShouldInterpolate_t) shouldinterpolate_detour.GetOriginalFunc();
    bool ret                     = original(entity);
    shouldinterpolate_detour.RestorePatch();
    return ret;
}

static InitRoutine misc_init([]() {
    static auto cl_sendmove_addr       = gSignatures.GetEngineSignature("55 89 E5 57 56 53 81 EC 2C 10 00 00 C6 85 ? ? ? ? 01");
    static auto shouldinterpolate_addr = gSignatures.GetClientSignature("55 89 E5 53 83 EC 14 A1 ? ? ? ? 8B 5D ? 8B 10 89 04 24 FF 52 ? 8B 53");
    cl_warp_sendmovedetour.Init(cl_sendmove_addr, (void *) hacks::tf2::warp::CL_SendMove_hook);
    cl_nospread_sendmovedetour.Init(cl_sendmove_addr, (void *) hacks::tf2::nospread::CL_SendMove_hook);
    shouldinterpolate_detour.Init(shouldinterpolate_addr, (void *) ShouldInterpolate_hook);

    static BytePatch removeScope(gSignatures.GetClientSignature, "81 EC ? ? ? ? A1 ? ? ? ? 8B 7D 08 8B 10 89 04 24 FF 92", 0x0, std::experimental::make_array<uint8_t>(0x5B, 0x5E, 0x5F, 0x5D, 0xC3));
    static BytePatch keepRifle(gSignatures.GetClientSignature, "74 ? A1 ? ? ? ? 8B 40 ? 85 C0 75 ? C9", 0x0, std::experimental::make_array<uint8_t>(0x70));
    no_scope.installChangeCallback([](bool after) {
        if (after)
            removeScope.Patch();
        else
            removeScope.Shutdown();
    });
    no_zoom.installChangeCallback([](bool after) {
        if (after)
            keepRifle.Patch();
        else
            keepRifle.Shutdown();
    });
    nolerp.installChangeCallback([](bool after) {
        if (!after && backup_lerp)
        {
            cl_interp->SetValue(backup_lerp);
            backup_lerp = 0.0f;
        }
        else
        {
            backup_lerp = cl_interp->GetFloat();
            // We should adjust cl_interp to be as low as possible
            if (cl_interp->GetFloat() > 0.152f)
                cl_interp->SetValue(0.152f);
        }
    });
    EC::Register(
        EC::Shutdown,
        []() {
            cl_interp->SetValue(backup_lerp);
            backup_lerp = 0.0f;
            removeScope.Shutdown();
            keepRifle.Shutdown();
        },
        "misctemp_shutdown");
});
