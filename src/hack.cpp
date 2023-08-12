/*
 * hack.cpp
 *
 *  Created on: Oct 3, 2016
 *      Author: nullifiedcat
 */

#define __USE_GNU
#include <execinfo.h>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <visual/SDLHooks.hpp>
#include "hack.hpp"
#include "common.hpp"
#include <link.h>
#include <pwd.h>

#include <hacks/hacklist.hpp>
#include <boost/stacktrace.hpp>

#if EXTERNAL_DRAWING
#include "xoverlay.h"
#endif
#define STRINGIFY(x) #x
#define TO_STRING(x) STRINGIFY(x)

#include "copypasted/CDumper.hpp"
#include "teamroundtimer.hpp"
#include "PlayerTools.hpp"
#include "version.h"

/*
 *  Credits to josh33901 aka F1ssi0N for butifel F1Public and Darkstorm 2015
 * Linux
 */

// game_shutdown = Is full game shutdown or just detach
bool hack::game_shutdown = true;
bool hack::shutdown      = false;
bool hack::initialized   = false;

const std::string &hack::GetVersion()
{
    static std::string version("Unknown Version");
    static bool version_set = false;
    if (version_set)
        return version;
#if defined(GIT_COMMIT_HASH) && defined(GIT_COMMITTER_DATE)
    version = "Version: #" GIT_COMMIT_HASH " " GIT_COMMITTER_DATE;
#endif
    version_set = true;
    return version;
}

const std::string &hack::GetType()
{
    static std::string version("Unknown Type");
    static bool version_set = false;
    if (version_set)
        return version;
    version = "";
#if not ENABLE_IPC
    version += " NOIPC";
#endif
#if not ENABLE_GUI
    version += " NOGUI";
#else
    version += " GUI";
#endif

#ifndef DYNAMIC_CLASSES

#ifdef GAME_SPECIFIC
    version += " GAME " TO_STRING(GAME);
#else
    version += " UNIVERSAL";
#endif

#else
    version += " DYNAMIC";
#endif

#if not ENABLE_VISUALS
    version += " NOVISUALS";
#endif

    version     = version.substr(1);
    version_set = true;
    return version;
}

std::mutex hack::command_stack_mutex;
std::stack<std::string> &hack::command_stack()
{
    static std::stack<std::string> stack;
    return stack;
}

void hack::ExecuteCommand(const std::string &command)
{
    std::lock_guard<std::mutex> guard(hack::command_stack_mutex);
    hack::command_stack().push(command);
}

void critical_error_handler(int signum)
{
    namespace st = boost::stacktrace;
    ::signal(signum, SIG_DFL);
    passwd *pwd = getpwuid(getuid());
    std::ofstream out(std::string(DATA_PATH) + "/logs/crashes/cathook-" + pwd->pw_name + "-" + std::to_string(getpid()) + "-segfault.log");

    Dl_info info;
    if (!dladdr(reinterpret_cast<void *>(hack::ExecuteCommand), &info))
        return;

    for (auto i : st::stacktrace())
    {
        Dl_info info2;
        if (dladdr(i.address(), &info2))
        {
            unsigned int offset = (unsigned int) i.address() - (unsigned int) info2.dli_fbase;
            out << (!strcmp(info2.dli_fname, info.dli_fname) ? "cathook" : info2.dli_fname) << '\t' << (void *) offset << std::endl;
        }
    }

    out.close();
    ::raise(SIGABRT);
}

static void InitRandom()
{
    int rand_seed;
    FILE *rnd = fopen("/dev/urandom", "rb");
    if (!rnd || fread(&rand_seed, sizeof(rand_seed), 1, rnd) < 1)
    {
        logging::Info("Warning!!! Failed read from /dev/urandom (%s). Randomness is "
                      "going to be weak",
                      strerror(errno));
        timespec t{};
        clock_gettime(CLOCK_MONOTONIC, &t);
        rand_seed = t.tv_nsec ^ (t.tv_sec & getpid());
    }
    srand(rand_seed);
    if (rnd)
        fclose(rnd);
}

void hack::Hook()
{
    uintptr_t *clientMode = 0;
    while (!(clientMode = **(uintptr_t ***) ((uintptr_t)((*(void ***) g_IBaseClient)[10]) + 1)))
    {
        usleep(10000);
    }
    hooks::clientmode.Set((void *) clientMode);
    hooks::clientmode.HookMethod(HOOK_ARGS(CreateMove));
#if ENABLE_VISUALS
    hooks::clientmode.HookMethod(HOOK_ARGS(OverrideView));
#endif
    hooks::clientmode.HookMethod(HOOK_ARGS(LevelInit));
    hooks::clientmode.HookMethod(HOOK_ARGS(LevelShutdown));
    hooks::clientmode.Apply();

    hooks::client.Set(g_IBaseClient);
    hooks::client.HookMethod(HOOK_ARGS(DispatchUserMessage));
#if ENABLE_VISUALS
    hooks::client.HookMethod(HOOK_ARGS(FrameStageNotify));
    hooks::client.HookMethod(HOOK_ARGS(IN_KeyEvent));
#endif
    hooks::client.Apply();

#if ENABLE_VISUALS || ENABLE_NULL_GRAPHICS
    hooks::panel.Set(g_IPanel);
    hooks::panel.HookMethod(hooked_methods::methods::PaintTraverse, offsets::PaintTraverse(), &hooked_methods::original::PaintTraverse);
    hooks::panel.Apply();
#endif

#if ENABLE_VISUALS
    hooks::vstd.Set((void *) g_pUniformStream);
    hooks::vstd.HookMethod(HOOK_ARGS(RandomInt));
    hooks::vstd.Apply();

    auto chat_hud = g_CHUD->FindElement("CHudChat");
    while (!(chat_hud = g_CHUD->FindElement("CHudChat")))
        usleep(1000);
    hooks::chathud.Set(chat_hud);
    hooks::chathud.HookMethod(HOOK_ARGS(StartMessageMode));
    hooks::chathud.HookMethod(HOOK_ARGS(StopMessageMode));
    hooks::chathud.Apply();
#endif

    hooks::input.Set(g_IInput);
    hooks::input.HookMethod(HOOK_ARGS(GetUserCmd));
    hooks::input.HookMethod(HOOK_ARGS(CreateMoveInput));
    hooks::input.Apply();
#if ENABLE_VISUALS || ENABLE_NULL_GRAPHICS
    hooks::modelrender.Set(g_IVModelRender);
    hooks::modelrender.HookMethod(HOOK_ARGS(DrawModelExecute));
    hooks::modelrender.Apply();
#endif
    hooks::enginevgui.Set(g_IEngineVGui);
    hooks::enginevgui.HookMethod(HOOK_ARGS(Paint));
    hooks::enginevgui.Apply();

    hooks::engine.Set(g_IEngine);
    hooks::engine.Apply();

#if ENABLE_VISUALS
    hooks::eventmanager2.Set(g_IEventManager2);
    hooks::eventmanager2.HookMethod(HOOK_ARGS(FireEventClientSide));
    hooks::eventmanager2.Apply();
#endif

    hooks::steamfriends.Set(g_ISteamFriends);
    hooks::steamfriends.HookMethod(HOOK_ARGS(GetFriendPersonaName));
    hooks::steamfriends.Apply();

    hooks::prediction.Set(g_IPrediction);
    hooks::prediction.HookMethod(HOOK_ARGS(RunCommand));
    hooks::prediction.Apply();

    hooks::toolbox.Set(g_IToolFramework);
    hooks::toolbox.HookMethod(HOOK_ARGS(Think));
    hooks::toolbox.Apply();

#if ENABLE_VISUALS
    sdl_hooks::applySdlHooks();
#endif
    logging::Info("Hooked!");
}

void hack::Initialize()
{
    ::signal(SIGSEGV, &critical_error_handler);
    ::signal(SIGABRT, &critical_error_handler);
    time_injected = time(nullptr);

#if ENABLE_VISUALS
    std::ifstream exists(std::string(DATA_PATH) + "/fonts/tf2build.ttf", std::ios::in);
    if (!exists)
        Error("Missing essential file: fonts/tf2build.ttf \nYou MUST run install-data script to finish installation");
#endif /* TEXTMODE */
    logging::Info("Initializing...");
    InitRandom();
    sharedobj::LoadEarlyObjects();
    CreateEarlyInterfaces();

    // Applying the defaults needs to be delayed, because preloaded Cathook can not properly convert SDL codes to names before TF2 init
    settings::Manager::instance().applyDefaults();

    logging::Info("Clearing Early initializer stack");
    while (!init_stack_early().empty())
    {
        init_stack_early().top()();
        init_stack_early().pop();
    }
    logging::Info("Early Initializer stack done");
    sharedobj::LoadAllSharedObjects();
    CreateInterfaces();
    CDumper dumper;
    dumper.SaveDump();
    logging::Info("Is TF2? %d", IsTF2());
    logging::Info("Is TF2C? %d", IsTF2C());
    logging::Info("Is HL2DM? %d", IsHL2DM());
    logging::Info("Is CSS? %d", IsCSS());
    logging::Info("Is TF? %d", IsTF());
    InitClassTable();

    BeginConVars();
    g_Settings.Init();
    EndConVars();

#if ENABLE_VISUALS
    draw::Initialize();
#if ENABLE_GUI
// FIXME put gui here
#endif

#endif /* TEXTMODE */

    gNetvars.init();
    InitNetVars();
    g_pLocalPlayer    = new LocalPlayer();
    g_pPlayerResource = new TFPlayerResource();
    g_pTeamRoundTimer = new CTeamRoundTimer();

    playerlist::Load();
    player_tools::loadBetrayList();

#if ENABLE_VISUALS
    InitStrings();
#ifndef FEATURE_EFFECTS_DISABLED
    if (g_ppScreenSpaceRegistrationHead && g_pScreenSpaceEffects)
    {
        effect_glow::g_pEffectGlow = new CScreenSpaceEffectRegistration("_cathook_glow", &effect_glow::g_EffectGlow);
        g_pScreenSpaceEffects->EnableScreenSpaceEffect("_cathook_glow");
    }
    logging::Info("SSE enabled..");
#endif
#endif /* TEXTMODE */
    logging::Info("Clearing initializer stack");
    while (!init_stack().empty())
    {
        init_stack().top()();
        init_stack().pop();
    }
    logging::Info("Initializer stack done");
    hack::command_stack().push("exec cat_autoexec");
    auto extra_exec = std::getenv("CH_EXEC");
    if (extra_exec)
        hack::command_stack().push(extra_exec);

    hack::initialized = true;
    for (int i = 0; i < 12; i++)
    {
        re::ITFMatchGroupDescription *desc = re::GetMatchGroupDescription(i);
        if (!desc || desc->m_iID > 9) // ID's over 9 are invalid
            continue;
        if (desc->m_bForceCompetitiveSettings)
        {
            desc->m_bForceCompetitiveSettings = false;
            logging::Info("Bypassed force competitive cvars!");
        }
    }
    hack::Hook();
}

void hack::Shutdown()
{
    if (hack::shutdown)
        return;
    hack::shutdown = true;
    // Stop cathook stuff
    settings::RVarLock.store(true);
#if ENABLE_VISUALS
    sdl_hooks::cleanSdlHooks();
#endif
    logging::Info("Unregistering convars..");
    ConVar_Unregister();
    logging::Info("Unloading sharedobjects..");
    sharedobj::UnloadAllSharedObjects();
    logging::Info("Deleting global interfaces...");
    delete g_pLocalPlayer;
    delete g_pTeamRoundTimer;
    delete g_pPlayerResource;
    if (!hack::game_shutdown)
    {
        logging::Info("Running shutdown handlers");
        EC::run(EC::Shutdown);
#if ENABLE_VISUALS
        g_pScreenSpaceEffects->DisableScreenSpaceEffect("_cathook_glow");
#if EXTERNAL_DRAWING
        xoverlay_destroy();
#endif
#endif
    }
    logging::Info("Releasing VMT hooks..");
    hooks::ReleaseAllHooks();
    logging::Info("Success..");
}
