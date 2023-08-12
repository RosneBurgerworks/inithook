/*
 * nographics.cpp
 *
 *  Created on: Aug 1, 2017
 *      Author: nullifiedcat
 */

#include "common.hpp"
#include "DetourHook.hpp"
#include <e8call.hpp>

static settings::Boolean debug_framerate("debug.framerate", "false");

#if ENABLE_NULL_GRAPHICS
static settings::Boolean fix_friends_crash("hack.fix-vgui-crash", "true");
#else
static settings::Boolean fix_friends_crash("hack.fix-vgui-crash", "false");
#endif

#if !ENABLE_NULL_GRAPHICS
static settings::Boolean null_graphics("hack.nullgraphics", "false");
#endif

typedef ITexture *(*FindTexture_t)(void *, const char *, const char *, bool, int);
typedef IMaterial *(*FindMaterialEx_t)(void *, const char *, const char *, int, bool, const char *);
typedef IMaterial *(*FindMaterial_t)(void *, const char *, const char *, bool, const char *);

// 81
FindTexture_t FindTexture_Original;
FindMaterialEx_t FindMaterialEx_Original;
FindMaterial_t FindMaterial_Original;

ITexture *FindTexture_null_hook(void *this_, char const *pTextureName, const char *pTextureGroupName, bool complain, int nAdditionalCreationFlags)
{
    static ITexture *st = FindTexture_Original(this_, pTextureName, pTextureGroupName, complain, nAdditionalCreationFlags);
    return st;
}

// 123
IMaterial *FindMaterialEx_null_hook(void *this_, char const *pMaterialName, const char *pTextureGroupName, int nContext, bool complain, const char *pComplainPrefix)
{
    static IMaterial *st = FindMaterialEx_Original(this_, pMaterialName, pTextureGroupName, nContext, complain, pComplainPrefix);
    return st;
}

// 73
IMaterial *FindMaterial_null_hook(void *this_, char const *pMaterialName, const char *pTextureGroupName, bool complain, const char *pComplainPrefix)
{
    static IMaterial *st = FindMaterial_Original(this_, pMaterialName, pTextureGroupName, complain, pComplainPrefix);
    return st;
}

void ReloadTextures_null_hook(void *this_)
{
}
void ReloadMaterials_null_hook(void *this_, const char *pSubString)
{
}
void ReloadFilesInList_null_hook(void *this_, IFileList *pFilesToReload)
{
}
void NullHook()
{
    g_IMaterialSystem->SetInStubMode(true);
}
void RemoveNullHook()
{
    g_IMaterialSystem->SetInStubMode(false);
}

static CatCommand ApplyNullhook("debug_material_hook", "Debug", []() { NullHook(); });
static CatCommand RemoveNullhook("debug_material_hook_clear", "Debug", []() { RemoveNullHook(); });

static float framerate = 0.0f;
static Timer send_timer{};

static InitRoutine init_nullhook([]() {
#if ENABLE_NULL_GRAPHICS
    NullHook();
#endif
    EC::Register(
        EC::Paint,
        []() {
            if (!*debug_framerate)
                return;
            framerate = 0.9 * framerate + (1.0 - 0.9) * g_GlobalVars->absoluteframetime;
            if (send_timer.test_and_set(1000))
                logging::Info("FPS: %f", 1.0f / framerate);
        },
        "material_cm");
});

static bool blacklist_file(const char *&filename)
{
    const static char *blacklist[] = { ".ani", ".wav", ".mp3", ".vvd", ".vtx", ".vtf", ".vfe", ".cache" /*, ".pcf"*/ };
    if (!filename || !std::strncmp(filename, "materials/console/", 18))
        return false;

    std::size_t len = std::strlen(filename);
    if (len <= 3)
        return false;

    auto ext_p = strrchr(filename, '.');
    if (!ext_p)
        return false;

    if (!std::strcmp(ext_p, ".vmt"))
    {
        /* Not loading it causes extreme console spam */
        if (std::strstr(filename, "corner"))
            return false;
        /* minor console spam */
        if (std::strstr(filename, "hud") || std::strstr(filename, "vgui"))
            return false;

        return true;
    }
    if (std::strstr(filename, "sound.cache") || std::strstr(filename, "tf2_sound") || std::strstr(filename, "game_sounds"))
        return false;
    if (!std::strncmp(filename, "sound/player/footsteps", 22))
        return false;
    if (!std::strcmp(ext_p, ".mdl"))
    {
        return false;
    }
    if (!std::strncmp(filename, "/decal", 6))
        return true;

    for (int i = 0; i < sizeof(blacklist) / sizeof(blacklist[0]); ++i)
        if (!std::strcmp(ext_p, blacklist[i]))
            return true;

    return false;
}

static void *(*FSorig_Open)(void *, const char *, const char *, const char *);
static void *FSHook_Open(void *this_, const char *pFileName, const char *pOptions, const char *pathID)
{
    if (blacklist_file(pFileName))
        return nullptr;

    return FSorig_Open(this_, pFileName, pOptions, pathID);
}

static bool (*FSorig_ReadFile)(void *, const char *, const char *, void *, int, int, void *);
static bool FSHook_ReadFile(void *this_, const char *pFileName, const char *pPath, void *buf, int nMaxBytes, int nStartingByte, void *pfnAlloc)
{
    if (blacklist_file(pFileName))
        return false;

    return FSorig_ReadFile(this_, pFileName, pPath, buf, nMaxBytes, nStartingByte, pfnAlloc);
}

static void *(*FSorig_OpenEx)(void *, const char *, const char *, unsigned, const char *, char **);
static void *FSHook_OpenEx(void *this_, const char *pFileName, const char *pOptions, unsigned flags, const char *pathID, char **ppszResolvedFilename)
{
    if (pFileName && blacklist_file(pFileName))
        return nullptr;

    return FSorig_OpenEx(this_, pFileName, pOptions, flags, pathID, ppszResolvedFilename);
}

static int (*FSorig_ReadFileEx)(void *, const char *, const char *, void **, bool, bool, int, int, void *);
static int FSHook_ReadFileEx(void *this_, const char *pFileName, const char *pPath, void **ppBuf, bool bNullTerminate, bool bOptimalAlloc, int nMaxBytes, int nStartingByte, void *pfnAlloc)
{
    if (blacklist_file(pFileName))
        return 0;

    return FSorig_ReadFileEx(this_, pFileName, pPath, ppBuf, bNullTerminate, bOptimalAlloc, nMaxBytes, nStartingByte, pfnAlloc);
}

static void (*FSorig_AddFilesToFileCache)(void *, void *, const char **, int, const char *);
static void FSHook_AddFilesToFileCache(void *this_, void *cacheId, const char **ppFileNames, int nFileNames, const char *pPathID)
{
    fprintf(stderr, "AddFilesToFileCache: %d\n", nFileNames);
    for (int i = 0; i < nFileNames; ++i)
        fprintf(stderr, "%s\n", ppFileNames[i]);
}

static int (*FSorig_AsyncReadMultiple)(void *, const char **, int, void *);
static int FSHook_AsyncReadMultiple(void *this_, const char **pRequests, int nRequests, void *phControls)
{
    for (int i = 0; pRequests && i < nRequests; ++i)
    {
        if (blacklist_file(pRequests[i]))
        {
            if (nRequests > 1)
                fprintf(stderr,
                        "FIXME: blocked AsyncReadMultiple for %d requests due to some "
                        "filename being blacklisted\n",
                        nRequests);
            /* FSASYNC_ERR_FILEOPEN */
            return -1;
        }
    }
    return FSorig_AsyncReadMultiple(this_, pRequests, nRequests, phControls);
}

static const char *(*FSorig_FindNext)(void *, void *);
static const char *FSHook_FindNext(void *this_, void *handle)
{
    const char *p;
    do
        p = FSorig_FindNext(this_, handle);
    while (p && blacklist_file(p));

    return p;
}

static const char *(*FSorig_FindFirst)(void *, const char *, void **);
static const char *FSHook_FindFirst(void *this_, const char *pWildCard, void **pHandle)
{
    auto p = FSorig_FindFirst(this_, pWildCard, pHandle);
    while (p && blacklist_file(p))
        p = FSorig_FindNext(this_, *pHandle);

    return p;
}

static bool (*FSorig_Precache)(const char *, const char *);
static bool FSHook_Precache(const char *pFileName, const char *pPathID)
{
    return true;
}

static CatCommand debug_invalidate("invalidate_mdl_cache", "Invalidates MDL cache", []() { g_IBaseClient->InvalidateMdlCache(); });

/* WIP to reduce ram usage further on bots */
static void NullifyVGUI()
{
    static BytePatch patches[] = {
        {
            // CTFWinPanel::OnThink()
            gSignatures.GetClientSignature,
            "55 89 E5 56 53 83 EC 40 8B 5D ? F3 0F 10 83",
            0x00, std::experimental::make_array<uint8_t>(0xC3)
        },
        {
            // CTFWinPanel::FireGameEvent( IGameEvent * event )
            gSignatures.GetClientSignature,
            "55 89 E5 57 56 53 81 EC FC 15 00 00",
            0x00, std::experimental::make_array<uint8_t>(0xC3)
        },
        {
            // CTFHudWeaponAmmo::OnThink()
            gSignatures.GetClientSignature,
            "55 89 E5 57 56 53 83 EC 2C 8B 5D ? E8 ? ? ? ? 89 45",
            0x00, std::experimental::make_array<uint8_t>(0xC3)
        }
    };
    for (auto & patch : patches)
        patch.Patch();
}

static hooks::VMTHook fs_hook{}, fs_hook2{};
static bool hooked_fs = false;
static void ReduceRamUsage()
{
    if (!hooked_fs)
    {
        /* TO DO: Improves load speeds but doesn't reduce memory usage a lot
         * It seems engine still allocates significant parts without them
         * being really used
         * Plan B: null subsystems (Particle, Material, Model partially, Sound and etc.)
         */
        hooked_fs = true;
        fs_hook.Set(reinterpret_cast<void *>(g_IFileSystem));
        fs_hook.HookMethod(FSHook_FindFirst, 27, &FSorig_FindFirst);
        fs_hook.HookMethod(FSHook_FindNext, 28, &FSorig_FindNext);
        fs_hook.HookMethod(FSHook_AsyncReadMultiple, 37, &FSorig_AsyncReadMultiple);
        fs_hook.HookMethod(FSHook_OpenEx, 69, &FSorig_OpenEx);
        fs_hook.HookMethod(FSHook_ReadFileEx, 71, &FSorig_ReadFileEx);
        fs_hook.HookMethod(FSHook_AddFilesToFileCache, 103, &FSorig_AddFilesToFileCache);
        fs_hook.Apply();

        fs_hook2.Set(reinterpret_cast<void *>(g_IFileSystem), 4);
        fs_hook2.HookMethod(FSHook_Open, 2, &FSorig_Open);
        fs_hook2.HookMethod(FSHook_Precache, 9, &FSorig_Precache);
        fs_hook2.HookMethod(FSHook_ReadFile, 14, &FSorig_ReadFile);
        fs_hook2.Apply();
        /* Might give performance benefit, but mostly fixes annoying console
         * spam related to mdl not being able to play sequence that it
         * cannot play on error.mdl
         */
    }

    if (g_IBaseClient)
    {
        static BytePatch playSequence{ gSignatures.GetClientSignature, "55 89 E5 57 56 53 83 EC ? 8B 75 0C 8B 5D 08 85 F6 74 ? 83 BB", 0x00, std::experimental::make_array<uint8_t>(0xC3)};
        playSequence.Patch();

        /* Same explanation as above, but spams about certain particles and models not loaded */
        static BytePatch patches[] = {
            {
                // Particle Create
                gSignatures.GetClientSignature,
                "55 89 E5 56 53 83 EC ? 8B 5D 0C 8B 75 08 85 DB 74 ? A1",
                0x00, std::experimental::make_array<uint8_t>(0x31, 0xC0, 0xC3)
            },
            {
                // Particle Precache
                gSignatures.GetClientSignature,
                "55 89 E5 53 83 EC ? 8B 5D 0C 8B 45 08 85 DB 74 ? 80 3B 00 75 ? 83 C4 ? 5B 5D C3 ? ? ? ? 89 5C 24",
                0x00, std::experimental::make_array<uint8_t>(0x31, 0xC0, 0xC3)
            },
            {
                // Particle Creating
                gSignatures.GetClientSignature,
                "55 89 E5 57 56 53 83 EC ? A1 ? ? ? ? 8B 75 08 85 C0 74 ? 8B 4D",
                0x00, std::experimental::make_array<uint8_t>(0x31, 0xC0, 0xC3)
            },
            {
                // Particle Failed To Create
                gSignatures.GetClientSignature,
                "55 89 E5 57 56 53 83 EC ? 8B 5D ? 8B 83 ? ? ? ? 85 C0 74 0A 80",
                0x00, std::experimental::make_array<uint8_t>(0xC3)
            },
            {
                // Next 3 are model dont have attachment patches
                gSignatures.GetClientSignature,
                "55 89 e5 57 56 53 83 ec ? 8b 45 ? 8b 75 ? 8b 5d ? 8b 7d ? 89 45 ? 8b 45",
                0x00, std::experimental::make_array<uint8_t>(0xC3)
            },
            {
                gSignatures.GetClientSignature,
                "55 89 e5 57 56 53 81 ec ? ? ? ? 8b 75 ? 8b 45 ? 8b 7d ? 85 f6",
                0x00, std::experimental::make_array<uint8_t>(0xC3)
            },
            {
                gSignatures.GetClientSignature,
                "55 89 e5 56 53 83 ec ? 8b 5d ? 85 db 74 ? 8b 03 89 1c ? ff 90 ? ? ? ? 85",
                0x00, std::experimental::make_array<uint8_t>(0x31, 0xC0, 0xC3)
            }
        };
        for (auto & patch : patches)
            patch.Patch();

        NullifyVGUI();
    }
}

static void UnHookFs()
{
    fs_hook.Release();
    fs_hook2.Release();
    hooked_fs = false;
    if (g_IBaseClient)
        g_IBaseClient->InvalidateMdlCache();
}

#if ENABLE_NULL_GRAPHICS
static InitRoutineEarly nullify_textmode([]() {
    const auto hide_sdl_window    = std::experimental::make_array<uint8_t>(0x90, 0x90, 0x90, 0x90, 0x90);
    const auto setup_graphic_addr = e8call_direct(gSignatures.GetEngineSignature("E8 ? ? ? ? 8B 93 ? ? ? ? 85 D2 0F 84")) + 0x18;
    ReduceRamUsage();

    static BytePatch patches[] = {
        {
            // SDL_CreateWindow has a "flag" parameter. We simply give it HIDDEN as a flag (0x8 = SDL_HIDDEN
            gSignatures.GetLauncherSignature,
            "C7 43 ? ? ? ? ? C7 44 24 ? ? ? ? ? C7 44 24",
            0xb, std::experimental::make_array<uint8_t>(0x8)
        },
        {
            gSignatures.GetLauncherSignature,
            "E8 ? ? ? ? C6 43 25 01 83 C4 5C",
            0x0, hide_sdl_window
        },
        {
            gSignatures.GetLauncherSignature,
            "E8 ? ? ? ? 8B 43 14 89 04 24 E8 ? ? ? ? C6 43 25 01 83 C4 1C",
            0x0, hide_sdl_window
        },
        {
            gSignatures.GetLauncherSignature,
            "89 14 24 E8 ? ? ? ? 8B 45 B4",
            0x3, hide_sdl_window
        },
        {
            // CVideoMode_Common::Init  SetupStartupGraphic
            // This makes SetupStartupGraphic instantly return
            setup_graphic_addr,
            std::experimental::make_array<uint8_t>(0x81, 0xC4, 0x6C, 0x20, 0x00, 0x00, 0x5B, 0x5E, 0x5F, 0x5D, 0xC3)
        },
        {
            // CMaterialSystem::SwapBuffers
            sharedobj::materialsystem().Pointer(0x3ECB0),
            std::experimental::make_array<uint8_t>(0x31, 0xC0, 0x40, 0xC3)
        },
        {
            // V_RenderView
            gSignatures.GetEngineSignature,
            "55 89 E5 56 53 83 C4 80 C7 45 ? 00 00 00 00 A1 ? ? ? ? C7 45 ? 00 00 00 00 85 C0",
            0x1d3, std::experimental::make_array<uint8_t>(0x90, 0x90, 0x90, 0x90, 0x90)
        }
    };
    for (auto &patch : patches)
        patch.Patch();
});
#endif

static Timer signon_timer;
static InitRoutine init_nographics([]() {
#if ENABLE_NULL_GRAPHICS
    ReduceRamUsage();

    // Catbots still hit properly, this just makes it easier to stub stuff not needed in textmode
    bool *g_bTextMode_ptr = *((bool **) (gSignatures.GetEngineSignature("A2 ? ? ? ? 8B 43 04") + 0x1));
    *g_bTextMode_ptr      = true;

    // CViewRender::Render
    static BytePatch renderPatch(gSignatures.GetClientSignature, "55 89 E5 57 56 53 81 EC DC 03 00 00 C7 85 ? ? ? ? 00 00 00 00", 0x0, std::experimental::make_array<uint8_t>(0x31, 0xC0, 0x40, 0xC3));
    renderPatch.Patch();

    // FIXME: For some reason this can cause crashes
    //static BytePatch muteResouceDownloading(gSignatures.GetEngineSignature, "0F 85 ? ? ? ? A1 ? ? ? ? 8D 8B ? ? ? ?", 0x1, std::experimental::make_array<uint8_t>(0x81));
    //muteResouceDownloading.Patch();
#else
    null_graphics.installChangeCallback([](bool after) {
        if (after)
            ReduceRamUsage();
        else
            UnHookFs();
    });
#endif
    /* FUN_012ebee0
    *     (**(code **)(g_Telemetry + 0x50))
    *         (g_Telemetry,&local_5c,0,0,0,0,
    *
    *          "/home/buildbot/buildslave/rel_hl2_client_linux/build/src/game/client/tf/vgui/tf_friends_panel.cpp"
    *          ,0x138,&DAT_02060d58,&DAT_01957765,"PositionFriendsList");
    *     local_44 = g_Telemetry;
    */
    static BytePatch fixFriendsList1(gSignatures.GetClientSignature, "55 89 E5 57 56 53 81 EC 8C 00 00 00 C7 45 ? 00 00 00 00 A1 ? ? ? ? C7 45 ? 00 00 00 00 8B 5D ? 85 C0 0F 84 ? ? ? ? 8D 55 ? 89 04 24 89 54 24 ? C7 44 24 ? ? ? ? ? C7 44 24 ? ? ? ? ? C7 44 24 ? ? ? ? ? C7 44 24 ? 38 01 00 00", 0x00, std::experimental::make_array<uint8_t>(0xC3));
    static BytePatch fixFriendsList2(gSignatures.GetClientSignature, "55 89 E5 57 56 53 83 EC 6C 8B 75 ? 89 34 24 E8 ? ? ? ? 89 34 24", 0x00, std::experimental::make_array<uint8_t>(0xC3)); /* FUN_012dc610 */
    if (fix_friends_crash)
    {
        fixFriendsList1.Patch();
        fixFriendsList2.Patch();
    }
    fix_friends_crash.installChangeCallback([](bool after) {
        if (after)
        {
            fixFriendsList1.Patch();
            fixFriendsList2.Patch();
        }
        else
        {
            fixFriendsList1.Shutdown();
            fixFriendsList2.Shutdown();
        }
    });
});
