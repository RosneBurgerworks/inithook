/*
 * Misc.cpp
 *
 *  Created on: Nov 5, 2016
 *      Author: nullifiedcat
 */

#include "common.hpp"
#include "AntiAim.hpp"
#include "DynamicBytepatch.hpp"
#include "MiscTemporary.hpp"

#include <regex>

static settings::Boolean anti_afk{ "misc.anti-afk", "false" };
static settings::Boolean auto_strafe{ "misc.autostrafe", "false" };
static settings::Boolean tauntslide{ "misc.tauntslide-tf2c", "false" };
static settings::Boolean tauntslide_tf2{ "misc.tauntslide", "false" };
static settings::Boolean flashlight_spam{ "misc.flashlight-spam", "false" };
static settings::Boolean auto_balance_spam{ "misc.auto-balance-spam", "false" };
static settings::Boolean nopush_enabled{ "misc.no-push", "false" };
static settings::Boolean ping_reducer{ "misc.ping-reducer.enable", "false" };
static settings::Int force_ping{ "misc.ping-reducer.target", "0" };
static settings::Boolean patch_cmd_limit{ "misc.patch-cmd-limit", "false" };
static settings::Boolean lobby_watchdog{ "misc.lobby-watchdog", "false" };

#if ENABLE_VISUALS
/* Allows editing of the rich presence info in steam's friends UI */
static settings::Boolean rich_presence{ "misc.rich-presence", "false" };
static settings::String rich_presence_file{ "misc.rich-presence.file-name", "rich_presence.txt" };
static settings::Int rich_presence_party_size{ "misc.rich-presence.party_size", "1337" };
static settings::Int rich_presence_change_delay{ "misc.rich-presence.delay", "5000" };
#endif

#if ENABLE_VISUALS
static settings::Boolean debug_info{ "misc.debug-info", "false" };
static settings::Boolean show_spectators{ "misc.show-spectators", "false" };
static settings::Int show_spectators_x{ "misc.show-spectators.x", "10" };
static settings::Int show_spectators_y{ "misc.show-spectators.y", "10" };
#endif

#if ENABLE_VISUALS
static settings::Boolean render_zoomed{ "visual.render-local-zoomed", "true" };

static void tryPatchLocalPlayerShouldDraw(bool after)
{
    static BytePatch patch_shoulddraw{ gSignatures.GetClientSignature, "80 BB ? ? ? ? ? 75 DE", 0xD, std::experimental::make_array<uint8_t>(0xE0) };
    static BytePatch patch_shoulddraw_wearable{ gSignatures.GetClientSignature("0F 85 ? ? ? ? E9 ? ? ? ? 85 D2") + 1, std::experimental::make_array<uint8_t>(0x80) };

    if (after)
    {
        patch_shoulddraw.Patch();
        patch_shoulddraw_wearable.Patch();
    }
    else
    {
        patch_shoulddraw.Shutdown();
        patch_shoulddraw_wearable.Shutdown();
    }
}
#endif

static Timer anti_afk_timer{};
static int last_buttons{ 0 };

static void updateAntiAfk()
{
    if (current_user_cmd->buttons != last_buttons || g_pLocalPlayer->life_state)
    {
        anti_afk_timer.update();
        last_buttons = current_user_cmd->buttons;
    }
    else
    {
        if (anti_afk_timer.test_and_set(60000))
        {
            if (current_user_cmd->buttons & IN_DUCK)
                current_user_cmd->buttons &= ~IN_DUCK;
            else
                current_user_cmd->buttons = IN_DUCK;
            current_user_cmd->buttons |= IN_FORWARD;
        }
    }
}

CatCommand fix_cursor("fix_cursor", "Fix the GUI cursor being visible", []() {
    g_ISurface->LockCursor();
    g_ISurface->SetCursorAlwaysVisible(false);
});

namespace hacks::shared::misc
{

// Use to send a autobalance request to the server that doesnt prevent you from
// using it again, Allowing infinite use of it.
void SendAutoBalanceRequest()
{ // Credits to blackfire
    if (!g_IEngine->IsInGame())
        return;
    KeyValues *kv = new KeyValues("AutoBalanceVolunteerReply");
    kv->SetInt("response", 1);
    g_IEngine->ServerCmdKeyValues(kv);
}

// Catcommand for above
CatCommand SendAutoBlRqCatCom("request_balance", "Request Infinite Auto-Balance", [](const CCommand &args) { SendAutoBalanceRequest(); });

static int last_number{ 0 };
static int last_checked_command_number{ 0 };
static IClientEntity *last_checked_weapon{ nullptr };
static bool flash_light_spam_switch{ false };
static Timer auto_balance_timer{};

static ConVar *teammatesPushaway{ nullptr };
static std::unordered_map<unsigned, unsigned> pending_timers{};

void CreateMove()
{
    if (current_user_cmd->command_number)
        last_number = current_user_cmd->command_number;

    if (anti_afk)
        updateAntiAfk();

    // Automaticly airstrafes in the air
    if (auto_strafe)
    {
        auto ground = (bool) (CE_INT(g_pLocalPlayer->entity, netvar.iFlags) & FL_ONGROUND);
        if (!ground)
        {
            if (current_user_cmd->mousedx > 1 || current_user_cmd->mousedx < -1)
            {
                current_user_cmd->sidemove = current_user_cmd->mousedx > 1 ? 450.f : -450.f;
            }
        }
    }

    if (lobby_watchdog)
    {
        static Timer timer_watchdog;
        auto lobby = CTFLobbyShared::GetLobby();
        if (lobby && timer_watchdog.test_and_set(1000))
        {
            CTFLobbyPlayer *player;
            for (int i = 0; i < lobby->GetNumPendingPlayers(); ++i)
            {
                player = lobby->GetPendingPlayer(i);
                if (!player)
                    continue;
                auto steamid = player->GetID().GetAccountID();
                if (pending_timers.find(steamid) == pending_timers.end())
                    pending_timers[steamid] = 1;
                else
                    pending_timers[steamid]++;

                // Over 15 seconds, log a warning.
                if (pending_timers[steamid] >= 15)
                    logging::InfoConsole("Pending lobby member [U:1:%u] has been pending for %d seconds!", player->GetID().GetAccountID(), pending_timers[steamid]);
            }
        }
    }

    // TF2c Tauntslide
    IF_GAME(IsTF2C())
    {
        if (tauntslide)
            RemoveCondition<TFCond_Taunting>(LOCAL_E);
    }

    // HL2DM flashlight spam
    IF_GAME(IsHL2DM())
    {
        if (flashlight_spam)
        {
            if (flash_light_spam_switch && !current_user_cmd->impulse)
                current_user_cmd->impulse = 100;
            flash_light_spam_switch = !flash_light_spam_switch;
        }
    }

    IF_GAME(IsTF2())
    {
        // Tauntslide needs improvement for movement but it mostly works
        if (tauntslide_tf2)
        {
            // Check to prevent crashing
            if (CE_GOOD(LOCAL_E))
            {
                if (HasCondition<TFCond_Taunting>(LOCAL_E))
                {
                    // get directions
                    float forward = 0;
                    float side    = 0;
                    if (current_user_cmd->buttons & IN_FORWARD)
                        forward += 450;
                    if (current_user_cmd->buttons & IN_BACK)
                        forward -= 450;
                    if (current_user_cmd->buttons & IN_MOVELEFT)
                        side -= 450;
                    if (current_user_cmd->buttons & IN_MOVERIGHT)
                        side += 450;
                    current_user_cmd->forwardmove = forward;
                    current_user_cmd->sidemove    = side;

                    QAngle camera_angle;
                    g_IEngine->GetViewAngles(camera_angle);

                    // Doesnt work with anti-aim as well as I hoped... I guess
                    // this is as far as I can go with such a simple tauntslide
                    if (!hacks::shared::antiaim::isEnabled())
                        current_user_cmd->viewangles.y = camera_angle[1];
                    g_pLocalPlayer->v_OrigViewangles.y = camera_angle[1];

                    // Use silent since we dont want to prevent the player from
                    // looking around
                    g_pLocalPlayer->bUseSilentAngles = true;
                }
            }
        }

        // Spams infinite autobalance spam function
        if (auto_balance_spam && auto_balance_timer.test_and_set(150))
            SendAutoBalanceRequest();

        // Simple No-Push through cvars
        if (teammatesPushaway)
        {
            if (*nopush_enabled == teammatesPushaway->GetBool())
                teammatesPushaway->SetValue(!nopush_enabled);
        }
        else
            teammatesPushaway = g_ICvar->FindVar("tf_avoidteammates_pushaway");

        // Ping Reducer
        if (ping_reducer)
        {
            static ConVar *cmdrate = g_ICvar->FindVar("cl_cmdrate");
            if (cmdrate == nullptr)
            {
                cmdrate = g_ICvar->FindVar("cl_cmdrate");
                return;
            }
            int ping = g_pPlayerResource->GetPing(g_IEngine->GetLocalPlayer());
            static Timer updateratetimer{};
            if (updateratetimer.test_and_set(500) && g_IEngine->GetNetChannelInfo())
            {
                if (*force_ping <= ping)
                {
                    NET_SetConVar command("cl_cmdrate", "-1");
                    ((INetChannel *) g_IEngine->GetNetChannelInfo())->SendNetMsg(command);
                }
                else if (*force_ping > ping)
                {
                    NET_SetConVar command("cl_cmdrate", cmdrate->GetString());
                    ((INetChannel *) g_IEngine->GetNetChannelInfo())->SendNetMsg(command);
                }
            }
        }
    }
}

#if ENABLE_VISUALS
void DrawText()
{
    if (show_spectators && !g_pLocalPlayer->spectators.empty())
    {
        if (!g_IEngine->IsInGame() || CE_BAD(LOCAL_E) || !LOCAL_E->m_bAlivePlayer())
            return;

        int i = 0;
        for (auto spectator : g_pLocalPlayer->spectators)
        {
            std::string mode_string = "N/A";
            rgba_t col              = colors::gui;

            switch (spectator.mode)
            {
            case LocalPlayer::FIRSTPERSON:
                mode_string = "1st Person";
                col         = colors::red_s;
                break;
            case LocalPlayer::THIRDPERSON:
                mode_string = "3rd Person";
                break;
            case LocalPlayer::FREECAM:
                mode_string = "Freecam";
                break;
            }

            auto pl_col = playerlist::Color(spectator.info.friendsID);
            if (pl_col != colors::empty)
                col = pl_col;

            draw::String(*show_spectators_x, *show_spectators_y + i, col, format(spectator.info.name, " - (", mode_string, ")").c_str(), *fonts::center_screen);
            i += 15;
        }
    }
    if (!debug_info)
        return;
    if (CE_GOOD(g_pLocalPlayer->weapon()))
    {
        AddSideString(format("Slot: ", re::C_BaseCombatWeapon::GetSlot(RAW_ENT(g_pLocalPlayer->weapon()))));
        AddSideString(format("Taunt Concept: ", CE_INT(LOCAL_E, netvar.m_iTauntConcept)));
        AddSideString(format("Taunt Index: ", CE_INT(LOCAL_E, netvar.m_iTauntIndex)));
        AddSideString(format("Sequence: ", CE_INT(LOCAL_E, netvar.m_nSequence)));
        AddSideString("Est. Velocity");
        Vector est_vel;
        re::C_BaseEntity::EstimateAbsVelocity(RAW_ENT(LOCAL_E), est_vel);
        AddSideString(format("Velocity: ", est_vel.x, ' ', est_vel.y, ' ', est_vel.z));
        AddSideString(format("Velocity3: ", est_vel.Length()));
        AddSideString(format("Velocity2: ", est_vel.Length2D()));
        AddSideString("NetVar Velocity");
        Vector vel = CE_VECTOR(LOCAL_E, netvar.vVelocity);
        AddSideString(format("Velocity: ", vel.x, ' ', vel.y, ' ', vel.z));
        AddSideString(format("Velocity3: ", vel.Length()));
        AddSideString(format("Velocity2: ", vel.Length2D()));
        AddSideString(format("flSimTime: ", LOCAL_E->var<float>(netvar.m_flSimulationTime)));
        if (current_user_cmd)
            AddSideString(format("command_number: ", last_cmd_number));
        AddSideString(format("Clip1: ", CE_INT(g_pLocalPlayer->weapon(), netvar.m_iClip1)));
        AddSideString(format("Clip2: ", CE_INT(g_pLocalPlayer->weapon(), netvar.m_iClip2)));
        if (LOCAL_W->m_iClassID() == CL_CLASS(CTFMinigun))
            AddSideString(format("Weapon state: ", CE_INT(LOCAL_W, netvar.iWeaponState)));
        AddSideString(format("ItemDefinitionIndex: ", CE_INT(LOCAL_W, netvar.iItemDefinitionIndex)));
        AddSideString(format("Angle mode: ", g_pLocalPlayer->isFakeAngleCM ? "FAKE" : "REAL"));
    }
}

#endif

CatCommand name("name_set", "Immediate name change", [](const CCommand &args) {
    if (args.ArgC() < 2)
    {
        logging::Info("Set a name, silly");
        return;
    }
    ChangeName(args.ArgS());
});
CatCommand set_value("set", "Set value", [](const CCommand &args) {
    if (args.ArgC() < 2)
        return;
    ConVar *var = g_ICvar->FindVar(args.Arg(1));
    if (!var)
        return;
    std::string value(args.Arg(2));
    ReplaceSpecials(value);
    var->m_fMaxVal = 999999999.9f;
    var->m_fMinVal = -999999999.9f;
    var->SetValue(value.c_str());
    logging::Info("Set '%s' to '%s'", args.Arg(1), value.c_str());
});
CatCommand disconnect("disconnect", "Disconnect with custom reason", [](const CCommand &args) {
    INetChannel *ch = (INetChannel *) g_IEngine->GetNetChannelInfo();
    if (!ch)
        return;
    ch->Shutdown(args.ArgS());
});
CatCommand logip("debug_ip", "Log ip to party chat and console", []() {
  INetChannel *ch = (INetChannel *) g_IEngine->GetNetChannelInfo();
  if (!ch)
      return;
  logging::InfoConsole(ch->GetAddress());
  re::CTFPartyClient::GTFPartyClient()->SendPartyChat(ch->GetAddress());
});

CatCommand disconnect_vac("disconnect_vac", "Disconnect (fake VAC)", []() {
    INetChannel *ch = (INetChannel *) g_IEngine->GetNetChannelInfo();
    if (!ch)
        return;
    ch->Shutdown("VAC banned from secure server\n");
});

// Netvars stuff
void DumpRecvTable(CachedEntity *ent, RecvTable *table, int depth, const char *ft, unsigned acc_offset)
{
    bool forcetable = ft && strlen(ft);
    if (!forcetable || !strcmp(ft, table->GetName()))
        logging::Info("==== TABLE: %s", table->GetName());
    for (int i = 0; i < table->GetNumProps(); i++)
    {
        RecvProp *prop = table->GetProp(i);
        if (!prop)
            continue;
        if (prop->GetDataTable())
        {
            DumpRecvTable(ent, prop->GetDataTable(), depth + 1, ft, acc_offset + prop->GetOffset());
        }
        if (forcetable && strcmp(ft, table->GetName()))
            continue;
        switch (prop->GetType())
        {
        case SendPropType::DPT_Float:
            logging::Info("TABLE %s IN DEPTH %d: %s [0x%04x] = %f", table ? table->GetName() : "none", depth, prop->GetName(), prop->GetOffset(), CE_FLOAT(ent, acc_offset + prop->GetOffset()));
            break;
        case SendPropType::DPT_Int:
            logging::Info("TABLE %s IN DEPTH %d: %s [0x%04x] = %i | %u | %hd | %hu", table ? table->GetName() : "none", depth, prop->GetName(), prop->GetOffset(), CE_INT(ent, acc_offset + prop->GetOffset()), CE_VAR(ent, acc_offset + prop->GetOffset(), unsigned int), CE_VAR(ent, acc_offset + prop->GetOffset(), short), CE_VAR(ent, acc_offset + prop->GetOffset(), unsigned short));
            break;
        case SendPropType::DPT_String:
            logging::Info("TABLE %s IN DEPTH %d: %s [0x%04x] = %s", table ? table->GetName() : "none", depth, prop->GetName(), prop->GetOffset(), CE_VAR(ent, prop->GetOffset(), char *));
            break;
        case SendPropType::DPT_Vector:
            logging::Info("TABLE %s IN DEPTH %d: %s [0x%04x] = (%f, %f, %f)", table ? table->GetName() : "none", depth, prop->GetName(), prop->GetOffset(), CE_FLOAT(ent, acc_offset + prop->GetOffset()), CE_FLOAT(ent, acc_offset + prop->GetOffset() + 4), CE_FLOAT(ent, acc_offset + prop->GetOffset() + 8));
            break;
        case SendPropType::DPT_VectorXY:
            logging::Info("TABLE %s IN DEPTH %d: %s [0x%04x] = (%f, %f)", table ? table->GetName() : "none", depth, prop->GetName(), prop->GetOffset(), CE_FLOAT(ent, acc_offset + prop->GetOffset()), CE_FLOAT(ent, acc_offset + prop->GetOffset() + 4));
            break;
        }
    }
    if (!ft || !strcmp(ft, table->GetName()))
        logging::Info("==== END OF TABLE: %s IN DEAPTH %d", table->GetName(), depth);
}

// CatCommand to dumb netvar info
static CatCommand dump_vars("debug_dump_netvars", "Dump netvars of entity", [](const CCommand &args) {
    if (args.ArgC() < 1)
        return;
    if (!atoi(args[1]))
        return;
    int idx           = atoi(args[1]);
    CachedEntity *ent = ENTITY(idx);
    if (CE_BAD(ent))
        return;
    ClientClass *clz = RAW_ENT(ent)->GetClientClass();
    logging::Info("Entity %i: %s", ent->m_IDX, clz->GetName());
    const char *ft = (args.ArgC() > 1 ? args[2] : 0);
    DumpRecvTable(ent, clz->m_pRecvTable, 0, ft, 0);
});
static CatCommand dump_vars_by_name("debug_dump_netvars_name", "Dump netvars of entity with target name", [](const CCommand &args) {
    if (args.ArgC() < 1)
        return;
    std::string name(args.Arg(1));
    for (int i = 0; i < HIGHEST_ENTITY; i++)
    {
        CachedEntity *ent = ENTITY(i);
        if (CE_BAD(ent))
            continue;
        ClientClass *clz = RAW_ENT(ent)->GetClientClass();
        if (!clz)
            continue;
        std::string clazz_name(clz->GetName());
        if (clazz_name.find(name) == clazz_name.npos)
            continue;
        logging::Info("Entity %i: %s", ent->m_IDX, clz->GetName());
        const char *ft = (args.ArgC() > 1 ? args[2] : 0);
        DumpRecvTable(ent, clz->m_pRecvTable, 0, ft, 0);
    }
});
#if ENABLE_VISUALS
// This makes us able to see enemy class and status in scoreboard and player panel
static std::unique_ptr<BytePatch> patch_playerpanel;
static std::unique_ptr<BytePatch> patch_scoreboard;
#endif

void Shutdown()
{
    if (CE_BAD(LOCAL_E))
        return;
#if ENABLE_VISUALS
    tryPatchLocalPlayerShouldDraw(false);
    patch_playerpanel->Shutdown();
    patch_scoreboard->Shutdown();
#endif
}

static void remove_cmd_limit(bool enable)
{
    static BytePatch p{ gSignatures.GetEngineSignature, "39 C8 89 8E ? ? ? ? 0F", 0x00, std::experimental::make_array<uint8_t>(0xEB, 0x0C) };
    if (enable)
        p.Patch();
    else
        p.Shutdown();
}

#if ENABLE_VISUALS

static std::vector<std::string> rich_presence_text;
static size_t current_presence_idx = 0;
static Timer richPresenceTimer;

void PresenceReload(std::string after)
{
    rich_presence_text.clear();
    current_presence_idx = 0;
    if (!after.empty())
    {
        static TextFile teamspam;
        if (teamspam.TryLoad(after))
            rich_presence_text = teamspam.lines;
    }
}

static void PresencePaint()
{
    if (!rich_presence || !richPresenceTimer.test_and_set(*rich_presence_change_delay))
        return;

    if (!rich_presence_text.empty())
    {
        g_ISteamFriends->SetRichPresence("steam_display", "#TF_RichPresence_Display");
        g_ISteamFriends->SetRichPresence("state", "PlayingMatchGroup");
        g_ISteamFriends->SetRichPresence("matchgrouploc", "SpecialEvent");

        if (current_presence_idx >= rich_presence_text.size())
            current_presence_idx = 0;

        g_ISteamFriends->SetRichPresence("currentmap", rich_presence_text[current_presence_idx].c_str());
        current_presence_idx++;
    }
    g_ISteamFriends->SetRichPresence("steam_player_group_size", std::to_string(*rich_presence_party_size + 1).c_str());
}

static CatCommand reload_presence("presence_reload", "Reload rich presence file", []() { PresenceReload(*rich_presence_file); });

#endif

static std::string generate_schema()
{
    std::ifstream in("tf/scripts/items/items_game.txt");
    std::string outS((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    std::regex a(R"("equip_regions?".*?".*?")"), b(R"("equip_regions?"\s*?\n\s*?\{[\s\S\n]*?\})");
    return std::regex_replace(std::regex_replace(outS, a, ""), b, "");
}

static CatCommand generateschema("schema_generate", "Generate custom schema", [](const CCommand &args) {
    const char *path = DATA_PATH "/items_game.txt";
    if (args.ArgC() > 1)
        path = args.Arg(1);

    std::ofstream out(path);
    out << generate_schema();
    out.close();
});

static void set_custom_schema(const char *buf, std::size_t length)
{
    static auto GetItemSchema   = reinterpret_cast<void *(*) (void)>(gSignatures.GetClientSignature("55 89 E5 57 56 53 83 EC ? 8B 1D ? ? ? ? 85 DB 89 D8"));
    static auto BInitTextBuffer = reinterpret_cast<bool (*)(void *, CUtlBuffer &, CUtlVector<CUtlString> *)>(gSignatures.GetClientSignature("55 89 E5 57 56 53 8D 9D ? ? ? ? 81 EC ? ? ? ? 8B 7D ? 89 1C 24 "));
    void *schema                = (void *) ((uintptr_t) GetItemSchema() + 0x4);

    logging::InfoConsole("Loading item schema...");
    CUtlBuffer utl(buf, length, 9);
    bool ret = BInitTextBuffer(schema, utl, nullptr);
    logging::InfoConsole("Loading %s", ret ? "Successful" : "Unsuccessful");
}

class ServerSpawnListener : public IGameEventListener2
{
    virtual void FireGameEvent(IGameEvent *event)
    {
        if (!strcmp(event->GetName(), "server_spawn"))
            current_server_name = event->GetString("hostname");
    }
};

static ServerSpawnListener server_spawn_listener{};

CatCommand schema("schema", "Load custom schema", [](const CCommand &args) {
    const char *path = DATA_PATH "/items_game.txt";
    if (args.ArgC() > 1)
        path = args.Arg(1);

    std::ifstream in(path);
    std::string contents((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    set_custom_schema(contents.c_str(), contents.size());
});

CatCommand hack_equip_regions("remove_equip_regions", "Modify item schema in a way that allows you to equip items with conflicting equip regions", []() {
    std::string result = generate_schema();
    set_custom_schema(result.c_str(), result.size());
});

static void ResetWatchdog()
{
    pending_timers.clear();
}

CatCommand die("quit", "Kill game", []() { std::_Exit(EXIT_SUCCESS); });
CatCommand crash("crash", "Force crash", []() { *(int *) nullptr = 0; });

static InitRoutine init([]() {
    teammatesPushaway = g_ICvar->FindVar("tf_avoidteammates_pushaway");
    EC::Register(EC::Shutdown, Shutdown, "draw_local_player", EC::average);
    EC::Register(EC::CreateMove, CreateMove, "cm_misc_hacks", EC::average);
    EC::Register(EC::CreateMoveWarp, CreateMove, "cmw_misc_hacks", EC::average);
    EC::Register(EC::LevelInit, ResetWatchdog, "levelinit_misc_watchdog", EC::average);
#if ENABLE_VISUALS
    EC::Register(EC::Draw, DrawText, "draw_misc_hacks", EC::average);
    patch_playerpanel = std::make_unique<BytePatch>(gSignatures.GetClientSignature, "0F 94 45 DF", 0x0, std::experimental::make_array<uint8_t>(0xC6, 0x45, 0xDF, 0x01));
    patch_scoreboard  = std::make_unique<BytePatch>(gSignatures.GetClientSignature, "83 F8 02 75 09", 0x0, std::experimental::make_array<uint8_t>(0xE9, 0xC1, 0x06, 0x00, 0x00));
    patch_playerpanel->Patch();
    patch_scoreboard->Patch();
    // Remove clientside shield turn limit
    static BytePatch patch(gSignatures.GetClientSignature, "75 16 F3 0F 10 45", 0x0, std::experimental::make_array<uint8_t>(0x90, 0x90));
    patch.Patch();
#endif
#if ENABLE_VISUALS
    render_zoomed.installChangeCallback([](bool after) { tryPatchLocalPlayerShouldDraw(after); });
    if (render_zoomed)
        tryPatchLocalPlayerShouldDraw(true);
#endif
    patch_cmd_limit.installChangeCallback([](bool new_val) { remove_cmd_limit(new_val); });
    if (patch_cmd_limit)
        remove_cmd_limit(true);
    g_IEventManager2->AddListener(&server_spawn_listener, "server_spawn", false);
#if ENABLE_VISUALS
    rich_presence_file.installChangeCallback([](std::string after) { PresenceReload(after); });
    EC::Register(EC::Paint, PresencePaint, "paint_rich_presence");
#endif
    static BytePatch patchEndRoundBoard(gSignatures.GetClientSignature, "55 89 E5 57 56 53 83 EC 2C 8B 5D ? 89 1C 24 E8 ? ? ? ? 8B 83 ? ? ? ? 0F B6 84", 0x00, std::experimental::make_array<uint8_t>(0xC3));
    patchEndRoundBoard.Patch();
    static BytePatch vguimatsurfaceCrash(gSignatures.GetClientSignature, "55 89 E5 57 56 53 83 EC 5C 89 55 ? 8B 18", 0x00, std::experimental::make_array<uint8_t>(0xC3));
    vguimatsurfaceCrash.Patch();
});

#if ENABLE_VISUALS

// Credits to UNKN0WN
namespace ScoreboardColoring
{
static settings::Boolean match_colors{ "misc.scoreboard.match-custom-team-colors", "false" };

// Colors taken from client.so
Color &GetPlayerColor(int idx, int team, bool dead = false)
{
    static Color returnColor(0, 0, 0, 0);

    int col_red[] = { match_colors ? int(colors::red.r * 255) : 255, match_colors ? int(colors::red.g * 255) : 64, match_colors ? int(colors::red.b * 255) : 64 };
    int col_blu[] = { match_colors ? int(colors::blu.r * 255) : 153, match_colors ? int(colors::blu.g * 255) : 204, match_colors ? int(colors::blu.b * 255) : 255 };

    switch (team)
    {
    case TEAM_RED:
        returnColor.SetColor(col_red[0], col_red[1], col_red[2], 255);
        break;
    case TEAM_BLU:
        returnColor.SetColor(col_blu[0], col_blu[1], col_blu[2], 255);
        break;
    default:
        returnColor.SetColor(245, 229, 196, 255);
    }

    player_info_s pinfo{};
    if (g_IEngine->GetPlayerInfo(idx, &pinfo))
    {
        rgba_t cust = playerlist::Color(pinfo.friendsID);
        if (cust != colors::empty)
            returnColor.SetColor(cust.r * 255, cust.g * 255, cust.b * 255, 255);
    }

    if (dead)
        for (int i = 0; i < 3; i++)
            returnColor[i] /= 1.5f;

    return returnColor;
}

// This gets playerIndex with assembly magic, then sets a Color for the scoreboard entries
Color &GetTeamColor(void *, int team)
{
    int playerIndex;
    __asm__("mov %%esi, %0" : "=r"(playerIndex));
    return GetPlayerColor(playerIndex, team);
}

// This is some assembly magic in order to get playerIndex and team from already existing
// variables and then set scoreboard entries
Color &GetDeadPlayerColor()
{
    int playerIndex;
    int team;
    __asm__("mov %%esi, %0" : "=r"(playerIndex));
    team = g_pPlayerResource->GetTeam(playerIndex);
    return GetPlayerColor(playerIndex, team, true);
}

static InitRoutine init([]() {
    uintptr_t addr1 = gSignatures.GetClientSignature("89 04 24 FF 92 ? ? ? ? 8B 00") + 3;
    uintptr_t addr2 = gSignatures.GetClientSignature("75 1B 83 FB 02") + 2;
    if (addr1 == 3 || addr2 == 2)
        return;
    logging::Info("Patching scoreboard colors");

    // Used to Detour, we need to detour at two parts in order to do this properly
    auto relAddr1 = ((uintptr_t) GetTeamColor - (uintptr_t) addr1) - 5;
    auto relAddr2 = ((uintptr_t) GetDeadPlayerColor - (uintptr_t) addr2) - 5;

    // Construct BytePatch1
    std::vector<unsigned char> patch1 = { 0xE8 };
    for (size_t i = 0; i < sizeof(uintptr_t); i++)
        patch1.push_back(((unsigned char *) &relAddr1)[i]);
    for (int i = patch1.size(); i < 6; i++)
        patch1.push_back(0x90);

    // Construct BytePatch2
    std::vector<unsigned char> patch2 = { 0xE8 };
    for (size_t i = 0; i < sizeof(uintptr_t); i++)
        patch2.push_back(((unsigned char *) &relAddr2)[i]);
    patch2.push_back(0x8B);
    patch2.push_back(0x00);
    for (int i = patch2.size(); i < 27; i++)
        patch2.push_back(0x90);

    static DynamicBytePatch patch_scoreboardcolor1(addr1, patch1);
    static DynamicBytePatch patch_scoreboardcolor2(addr2, patch2);

    // Patch!
    patch_scoreboardcolor1.Patch();
    patch_scoreboardcolor2.Patch();
});
} // namespace ScoreboardColoring

#endif

} // namespace hacks::shared::misc
