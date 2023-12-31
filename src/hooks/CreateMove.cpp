/*
 * CreateMove.cpp
 *
 *  Created on: Jan 8, 2017
 *      Author: nullifiedcat
 */

#include "common.hpp"
#include "hack.hpp"
#include "MiscTemporary.hpp"
#include <link.h>
#include <hacks/hacklist.hpp>
#include <settings/Bool.hpp>
#include <hacks/AntiAntiAim.hpp>
#include "HookTools.hpp"
#include "Warp.hpp"

#include "HookedMethods.hpp"

static settings::Boolean minigun_jump{ "misc.minigun-jump-tf2c", "false" };
static settings::Boolean roll_speedhack{ "misc.roll-speedhack", "false" };
static settings::Boolean forward_speedhack{ "misc.roll-speedhack.forward", "false" };
settings::Boolean engine_pred{ "misc.engine-prediction", "true" };
static settings::Boolean debug_projectiles{ "debug.projectiles", "false" };
static settings::Int semiauto{ "misc.semi-auto", "0" };
static settings::Boolean fuckmode{ "misc.fuckmode", "false" };

#if ENABLE_VISUALS
static settings::Button fakelag_key{ "misc.fakelag.key", "<null>" };
static settings::Int fakelag_key_mode{ "misc.fakelag.key.mode", "0" };
#endif

class CMoveData;
namespace engine_prediction
{
static Vector original_origin;

void RunEnginePrediction(IClientEntity *ent, CUserCmd *ucmd)
{
    if (!ent)
        return;

    typedef void (*SetupMoveFn)(IPrediction *, IClientEntity *, CUserCmd *, class IMoveHelper *, CMoveData *);
    typedef void (*FinishMoveFn)(IPrediction *, IClientEntity *, CUserCmd *, CMoveData *);

    void **predictionVtable  = *((void ***) g_IPrediction);
    SetupMoveFn oSetupMove   = (SetupMoveFn)(*(unsigned *) (predictionVtable + 19));
    FinishMoveFn oFinishMove = (FinishMoveFn)(*(unsigned *) (predictionVtable + 20));
    auto object              = std::make_unique<char[]>(165);
    CMoveData *pMoveData     = (CMoveData *) object.get();

    // Backup
    float frameTime = g_GlobalVars->frametime;
    float curTime   = g_GlobalVars->curtime;
    int tickcount   = g_GlobalVars->tickcount;
    original_origin = ent->GetAbsOrigin();

    CUserCmd defaultCmd{};
    if (ucmd == nullptr)
    {
        ucmd = &defaultCmd;
    }

    // Set Usercmd for prediction
    NET_VAR(ent, 4188, CUserCmd *) = ucmd;

    // Set correct CURTIME
    g_GlobalVars->curtime   = g_GlobalVars->interval_per_tick * NET_INT(ent, netvar.nTickBase);
    g_GlobalVars->frametime = g_GlobalVars->interval_per_tick;

    *g_PredictionRandomSeed = MD5_PseudoRandom(current_user_cmd->command_number) & 0x7FFFFFFF;

    // Run The Prediction
    g_IGameMovement->StartTrackPredictionErrors(reinterpret_cast<CBasePlayer *>(ent));
    oSetupMove(g_IPrediction, ent, ucmd, NULL, pMoveData);
    g_IGameMovement->ProcessMovement(reinterpret_cast<CBasePlayer *>(ent), pMoveData);
    oFinishMove(g_IPrediction, ent, ucmd, pMoveData);
    g_IGameMovement->FinishTrackPredictionErrors(reinterpret_cast<CBasePlayer *>(ent));

    // Reset User CMD
    NET_VAR(ent, 4188, CUserCmd *) = nullptr;

    g_GlobalVars->frametime = frameTime;
    g_GlobalVars->curtime   = curTime;
    g_GlobalVars->tickcount = tickcount;

    // Adjust tickbase
    NET_INT(ent, netvar.nTickBase)++;
}

// Restore Origin
void FinishEnginePrediction(IClientEntity *ent, CUserCmd *ucmd)
{
    const_cast<Vector &>(ent->GetAbsOrigin()) = original_origin;
    original_origin.Invalidate();
}

} // namespace engine_prediction

void PrecalculateCanShoot()
{
    auto weapon = g_pLocalPlayer->weapon();
    // Check if player and weapon are good
    if (CE_BAD(g_pLocalPlayer->entity) || CE_BAD(weapon))
    {
        calculated_can_shoot = false;
        return;
    }

    // flNextPrimaryAttack without reload
    static float next_attack = 0.0f;
    // Last shot fired using weapon
    static float last_attack = 0.0f;
    // Last weapon used
    static CachedEntity *last_weapon = nullptr;
    float server_time                = (float) (CE_INT(g_pLocalPlayer->entity, netvar.nTickBase)) * g_GlobalVars->interval_per_tick;
    float new_next_attack            = CE_FLOAT(weapon, netvar.flNextPrimaryAttack);
    float new_last_attack            = CE_FLOAT(weapon, netvar.flLastFireTime);

    // Reset everything if using a new weapon/shot fired
    if (GetWeaponMode() == weapon_melee || new_last_attack != last_attack || last_weapon != weapon)
    {
        next_attack = new_next_attack;
        last_attack = new_last_attack;
        last_weapon = weapon;
    }
    // Check if can shoot
    calculated_can_shoot = next_attack <= server_time;
}

bool ShouldFakelagKey()
{
#if ENABLE_VISUALS
    static bool flip              = false;
    static bool pressed_last_tick = false;
    bool allow                    = true;

    // Check if key is used
    if (fakelag_key && fakelag_key_mode)
    {
        // Grab whether the key is depressed
        bool key_down = fakelag_key.isKeyDown();
        switch (*fakelag_key_mode)
        {
        case 1: // Only while key is depressed
            if (!key_down)
                allow = false;
            break;
        case 2: // Key acts like a toggle switch
            if (!pressed_last_tick && key_down)
                flip = !flip;
            if (!flip)
                allow = false;
            break;
        default:
            break;
        }
        pressed_last_tick = key_down;
    }
    return allow;
#else
    return true;
#endif
}

static int attackticks = 0;
namespace hooked_methods
{
DEFINE_HOOKED_METHOD(CreateMove, bool, void *this_, float input_sample_time, CUserCmd *cmd)
{
    g_Settings.is_create_move = true;
    bool time_replaced, ret, speedapplied;
    float curtime_old, servertime, speed, yaw;
    Vector vsilent, ang;

    current_user_cmd = cmd;
    EC::run(EC::CreateMoveEarly);
    IF_GAME(IsTF2C())
    {
        if (CE_GOOD(LOCAL_W) && minigun_jump && LOCAL_W->m_iClassID() == CL_CLASS(CTFMinigun))
            CE_INT(LOCAL_W, netvar.iWeaponState) = 0;
    }
    ret = original::CreateMove(this_, input_sample_time, cmd);

    if (!cmd)
    {
        g_Settings.is_create_move = false;
        return ret;
    }

    // Disabled because this causes EXTREME aimbot inaccuracy
    // Actually dont disable it. It causes even more inaccuracy
    if (!cmd->command_number)
    {
        g_Settings.is_create_move = false;
        return ret;
    }

    tickcount++;

    if (!isHackActive())
    {
        g_Settings.is_create_move = false;
        return ret;
    }

    if (!g_IEngine->IsInGame())
    {
        g_Settings.bInvalid       = true;
        g_Settings.is_create_move = false;
        return true;
    }

    PROF_SECTION(CreateMove);
#if ENABLE_VISUALS
    stored_buttons = current_user_cmd->buttons;
    if (freecam_is_toggled)
    {
        current_user_cmd->sidemove    = 0.0f;
        current_user_cmd->forwardmove = 0.0f;
    }
#endif
    if (current_user_cmd && current_user_cmd->command_number)
        last_cmd_number = current_user_cmd->command_number;

    time_replaced = false;
    curtime_old   = g_GlobalVars->curtime;

    INetChannel *ch;
    ch = (INetChannel *) g_IEngine->GetNetChannelInfo();
    if (ch && !hooks::netchannel.IsHooked((void *) ch))
    {
        hooks::netchannel.Set(ch);
        hooks::netchannel.HookMethod(HOOK_ARGS(SendDatagram));
        hooks::netchannel.HookMethod(HOOK_ARGS(SendNetMsg));
        hooks::netchannel.HookMethod(HOOK_ARGS(Shutdown));
        hooks::netchannel.Apply();
#if ENABLE_IPC
        ipc::UpdateServerAddress();
#endif
    }
    if (*fuckmode)
    {
        static int prevbuttons = 0;
        current_user_cmd->buttons |= prevbuttons;
        prevbuttons |= current_user_cmd->buttons;
    }
    if (!g_Settings.bInvalid && CE_GOOD(g_pLocalPlayer->entity))
    {
        servertime            = (float) CE_INT(g_pLocalPlayer->entity, netvar.nTickBase) * g_GlobalVars->interval_per_tick;
        g_GlobalVars->curtime = servertime;
        time_replaced         = true;
    }
    if (g_Settings.bInvalid)
    {
        entity_cache::Invalidate();
    }

    // Do not update if in warp, since the entities will stay identical either way
    if (!hacks::tf2::warp::in_warp)
    {
        PROF_SECTION(EntityCache);
        entity_cache::Update();
    }
    //	PROF_END("Entity Cache updating");
    {
        PROF_SECTION(CM_PlayerResource);
        g_pPlayerResource->Update();
    }
    {
        PROF_SECTION(CM_LocalPlayer);
        g_pLocalPlayer->Update();
    }
    PrecalculateCanShoot();
    if (firstcm)
        firstcm = false;
    g_Settings.bInvalid = false;

    if (CE_GOOD(g_pLocalPlayer->entity))
    {
        if (!g_pLocalPlayer->life_state && CE_GOOD(g_pLocalPlayer->weapon()))
        {
            // Walkbot can leave game.
            if (!g_IEngine->IsInGame())
            {
                g_Settings.is_create_move = false;
                return ret;
            }
            if (current_user_cmd->buttons & IN_ATTACK)
                ++attackticks;
            else
                attackticks = 0;
            if (semiauto)
                if (current_user_cmd->buttons & IN_ATTACK)
                    if (attackticks % *semiauto < *semiauto - 1)
                        current_user_cmd->buttons &= ~IN_ATTACK;
            g_pLocalPlayer->isFakeAngleCM = false;
            static int fakelag_queue      = 0;
            if (CE_GOOD(LOCAL_E))
                if ((fakelag_amount && ShouldFakelagKey()) || (hacks::shared::antiaim::force_fakelag && hacks::shared::antiaim::isEnabled()))
                {
                    // Do not fakelag when trying to attack
                    bool do_fakelag = true;
                    switch (g_pLocalPlayer->weapon_mode)
                    {
                    case weapon_melee:
                    {
                        if (g_pLocalPlayer->weapon_melee_damage_tick)
                            do_fakelag = false;
                        break;
                    }
                    case weapon_hitscan:
                    {
                        if ((CanShoot() || isRapidFire(RAW_ENT(LOCAL_W))) && current_user_cmd->buttons & IN_ATTACK)
                            do_fakelag = false;
                        break;
                    }
                    default:
                        break;
                    }

                    if (!hacks::tf2::warp::charged)
                        do_fakelag = false;

                    if (do_fakelag)
                    {
                        int fakelag_amnt = (*fakelag_amount > 1) ? *fakelag_amount : 1;
                        *bSendPackets    = fakelag_amnt == fakelag_queue;
                        if (*bSendPackets)
                            g_pLocalPlayer->isFakeAngleCM = true;
                        fakelag_queue++;
                        if (fakelag_queue > fakelag_amnt)
                            fakelag_queue = 0;
                    }
                }
            {
                PROF_SECTION(CM_antiaim);
                hacks::shared::antiaim::ProcessUserCmd(cmd);
            }
            if (debug_projectiles)
                projectile_logging::Update();
        }
    }
    {
        PROF_SECTION(CM_WRAPPER);
        EC::run(EC::CreateMove_NoEnginePred);
        if (engine_pred)
        {
            engine_prediction::RunEnginePrediction(RAW_ENT(LOCAL_E), current_user_cmd);
            g_pLocalPlayer->UpdateEye();
        }

        if (hacks::tf2::warp::in_warp)
            EC::run(EC::CreateMoveWarp);
        else
            EC::run(EC::CreateMove);
    }
    if (time_replaced)
        g_GlobalVars->curtime = curtime_old;
    g_Settings.bInvalid = false;
    {
        PROF_SECTION(CM_chat_stack);
        chat_stack::OnCreateMove();
    }

    // TODO Auto Steam Friend

    if (CE_GOOD(g_pLocalPlayer->entity))
    {
        speedapplied = false;
        if (roll_speedhack && cmd->buttons & IN_DUCK && !(cmd->buttons & IN_ATTACK) && !HasCondition<TFCond_Charging>(LOCAL_E))
        {
            speed                     = Vector{ cmd->forwardmove, cmd->sidemove, 0.0f }.Length();
            static float prevspeedang = 0.0f;
            if (fabs(speed) > 0.0f)
            {
                if (forward_speedhack)
                {
                    cmd->forwardmove *= -1.0f;
                    cmd->sidemove *= -1.0f;
                    cmd->viewangles.x = 91;
                }
                Vector vecMove(cmd->forwardmove, cmd->sidemove, 0.0f);
                vecMove *= -1;
                float flLength = vecMove.Length();
                Vector angMoveReverse{};
                VectorAngles(vecMove, angMoveReverse);
                cmd->forwardmove = -flLength;
                cmd->sidemove    = 0.0f; // Move only backwards, no sidemove
                float res        = g_pLocalPlayer->v_OrigViewangles.y - angMoveReverse.y;
                while (res > 180)
                    res -= 360;
                while (res < -180)
                    res += 360;
                if (res - prevspeedang > 90.0f)
                    res = (res + prevspeedang) / 2;
                prevspeedang                     = res;
                cmd->viewangles.y                = res;
                cmd->viewangles.z                = 90.0f;
                g_pLocalPlayer->bUseSilentAngles = true;
                speedapplied                     = true;
            }
        }
        if (g_pLocalPlayer->bUseSilentAngles)
        {
            if (!speedapplied)
            {
                vsilent.x = cmd->forwardmove;
                vsilent.y = cmd->sidemove;
                vsilent.z = cmd->upmove;
                speed     = sqrt(vsilent.x * vsilent.x + vsilent.y * vsilent.y);
                VectorAngles(vsilent, ang);
                yaw                 = DEG2RAD(ang.y - g_pLocalPlayer->v_OrigViewangles.y + cmd->viewangles.y);
                cmd->forwardmove    = cos(yaw) * speed;
                cmd->sidemove       = sin(yaw) * speed;
                float clamped_pitch = fabsf(fmodf(cmd->viewangles.x, 360.0f));
                if (clamped_pitch >= 90 && clamped_pitch <= 270)
                    cmd->forwardmove = -cmd->forwardmove;
            }

            ret = false;
        }
        g_pLocalPlayer->UpdateEnd();
    }
    g_Settings.is_create_move = false;
    if (nolerp)
    {
        static const ConVar *pUpdateRate = g_pCVar->FindVar("cl_updaterate");
        if (!pUpdateRate)
            pUpdateRate = g_pCVar->FindVar("cl_updaterate");
        else
        {
            float interp = MAX(cl_interp->GetFloat(), cl_interp_ratio->GetFloat() / pUpdateRate->GetFloat());
            cmd->tick_count += TIME_TO_TICKS(interp);
        }
    }
    return ret;
}

void WriteCmd(IInput *input, CUserCmd *cmd, int sequence_nr)
{
    // Write the usercmd
    GetVerifiedCmds(input)[sequence_nr % VERIFIED_CMD_SIZE].m_cmd = *cmd;
    GetVerifiedCmds(input)[sequence_nr % VERIFIED_CMD_SIZE].m_crc = GetChecksum(cmd);
    GetCmds(input)[sequence_nr % VERIFIED_CMD_SIZE]               = *cmd;
}

// This gets called before the other CreateMove, but since we run original first in here all the stuff gets called after normal CreateMove is done
DEFINE_HOOKED_METHOD(CreateMoveInput, void, IInput *this_, int sequence_nr, float input_sample_time, bool arg3)
{
    bSendPackets = reinterpret_cast<bool *>((uintptr_t) __builtin_frame_address(1) - 8);
    // Call original function, includes Normal CreateMove
    original::CreateMoveInput(this_, sequence_nr, input_sample_time, arg3);

    CUserCmd *cmd = nullptr;
    if (this_ && GetCmds(this_) && sequence_nr > 0)
        cmd = this_->GetUserCmd(sequence_nr);

    if (!cmd)
        return;

    current_late_user_cmd = cmd;

    if (!isHackActive())
    {
        WriteCmd(this_, current_late_user_cmd, sequence_nr);
        return;
    }

    if (!g_IEngine->IsInGame())
    {
        WriteCmd(this_, current_late_user_cmd, sequence_nr);
        return;
    }

    PROF_SECTION(CreateMoveInput);

    // Run EC
    EC::run(EC::CreateMoveLate);

    if (CE_GOOD(LOCAL_E))
    {
        // Restore prediction
        if (engine_prediction::original_origin.IsValid())
            engine_prediction::FinishEnginePrediction(RAW_ENT(LOCAL_E), current_late_user_cmd);
    }
    // Write the usercmd
    WriteCmd(this_, current_late_user_cmd, sequence_nr);
}

} // namespace hooked_methods
