#include "common.hpp"

#if ENABLE_IPC

namespace ipctriggers
{

static settings::Boolean enable{ "ipc.triggers.enabled", "false" };
static settings::Int gte{ "ipc.triggers.gte", "0" };

static settings::String set_exec{ "ipc.triggers.exec", "" };
static settings::String off_exec{ "ipc.triggers.exec.off", "" };

static int getTeamIPCCount()
{
    int count = 0;
    for (int i = 1; i < g_GlobalVars->maxClients; i++)
    {
        auto ent = ENTITY(i);
        if (CE_BAD(ent) || ent->m_bEnemy())
            continue;
        auto &pl = playerlist::AccessData(ent);
        if (pl.state != playerlist::EState::IPC)
            continue;
        count++;
    }
    return count;
}

static bool exec_ran{ false };

static void Reset()
{
    if (!ipc::peer)
        return;
    if (exec_ran)
    {
        g_IEngine->ClientCmd_Unrestricted(format_cstr("exec %s", (*off_exec).c_str()).get());
        logging::InfoConsole("IPC Triggers: executed reset config \'%s\' (IPC ID %d)", (*off_exec).c_str(), ipc::peer->client_id);
        exec_ran = false;
    }
}

static Timer ipc_triggers_timer;
static void CreateMove()
{
    if (*gte <= 0)
        return;
    if (!ipc::peer)
        return;
    // TODO: Better way of determining if bot should run this
    if (ipc::peer->client_id % 2 != 0)
        return;
    if (!ipc_triggers_timer.test_and_set(5000))
        return;
    int ipcCount = getTeamIPCCount();
    if (ipcCount >= *gte)
    {
        if (!exec_ran)
        {
            g_IEngine->ClientCmd_Unrestricted(format_cstr("exec %s", (*set_exec).c_str()).get());
            logging::InfoConsole("IPC Triggers: executed config \'%s\' (IPC ID %d)", (*set_exec).c_str(), ipc::peer->client_id);
            exec_ran = true;
        }
    }
    else
        Reset();
}

static void register_ipctriggers(bool enable)
{
    if (enable)
    {
        EC::Register(EC::CreateMove, CreateMove, "cm_ipctriggers");
        EC::Register(EC::LevelInit, Reset, "levelinit_reset_ipctriggers");
        EC::Register(EC::LevelShutdown, Reset, "leveshutdown_reset_ipctriggers");
    }
    else
    {
        EC::Unregister(EC::CreateMove, "cm_ipctriggers");
        EC::Unregister(EC::LevelInit, "levelinit_reset_ipctriggers");
        EC::Unregister(EC::LevelShutdown, "leveshutdown_reset_ipctriggers");
    }
}

static InitRoutine init([]() {
    enable.installChangeCallback([](bool new_val) { 
        register_ipctriggers(new_val); 
    });
    if (*enable)
        register_ipctriggers(true);
});

} // namespace ipctriggers

#endif
