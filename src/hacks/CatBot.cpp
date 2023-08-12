/*
 * CatBot.cpp
 *
 *  Created on: Dec 30, 2017
 *      Author: nullifiedcat
 */

#include <settings/Bool.hpp>
#include "CatBot.hpp"
#include "common.hpp"
#include "hack.hpp"
#include "PlayerTools.hpp"
#include "MiscTemporary.hpp"

static settings::Boolean auto_disguise{ "misc.autodisguise", "true" };

static settings::Boolean abandon_if{ "cat-bot.abandon-if.enable", "false" };
static settings::Boolean abandon_if_invite_required{ "cat-bot.abandon-if.invite-required", "false" };
static settings::Boolean abandon_if_count_pending{ "cat-bot.abandon-if.count-pending-lobby-members", "true" };

static settings::Int abandon_if_bots_gte{ "cat-bot.abandon-if.bots-gte", "0" };
static settings::Int abandon_if_ipc_bots_gte{ "cat-bot.abandon-if.ipc-bots-gte", "0" };
static settings::Int abandon_if_humans_lte{ "cat-bot.abandon-if.humans-lte", "0" };
static settings::Int abandon_if_players_lte{ "cat-bot.abandon-if.players-lte", "0" };
static settings::Int abandon_if_team_lte{ "cat-bot.abandon-if.team-lte", "0" };
static settings::Int abandon_if_timer{ "cat-bot.abandon-if.hour.timer", "1" };
#if ENABLE_TEXTMODE
  static settings::Boolean abandon_if_hour{"cat-bot.abandon-if.hour", "true"};
#elif ENABLE_IPC
  static settings::Boolean abandon_if_hour{"cat-bot.abandon-if.hour", "false"};
#endif

static settings::Boolean micspam{ "cat-bot.micspam.enable", "false" };
static settings::Int micspam_on{ "cat-bot.micspam.interval-on", "1" };
static settings::Int micspam_off{ "cat-bot.micspam.interval-off", "0" };

static settings::Boolean auto_crouch{ "cat-bot.auto-crouch", "false" };
static settings::Boolean always_crouch{ "cat-bot.always-crouch", "false" };
static settings::Int autoReport{ "cat-bot.autoreport", "60" };
static settings::Int autoReportReason{ "cat-bot.autoreport.reason", "1" };
static settings::Int autoReportDelay{ "cat-bot.autoreport.interval", "200" };
static settings::Boolean autovote_map{ "cat-bot.autovote-map", "true" };

namespace hacks::shared::catbot
{
settings::Boolean catbotmode{ "cat-bot.enable", "false" };
settings::Boolean anti_motd{ "cat-bot.anti-motd", "false" };

static Timer timer_catbot_list{};
static Timer timer_abandon{};

Timer level_init_timer{};
Timer micspam_on_timer{}, micspam_off_timer{}, report_delay{};
static bool patched_report;

static std::vector<uint64_t> to_report;
static void report(uint64_t id, int reason)
{
    typedef int (*ReportPlayer_t)(uint64_t, int);
    static uintptr_t addr1                = gSignatures.GetClientSignature("55 89 E5 57 56 53 81 EC ? ? ? ? 8B 5D ? 8B 7D ? 89 D8");
    static ReportPlayer_t ReportPlayer_fn = ReportPlayer_t(addr1);
    if (!addr1)
        return;
    if (!patched_report)
    {
        /* Bypass client side time limit on repetitive reports */
        static BytePatch patch(gSignatures.GetClientSignature, "55 89 E5 57 56 53 83 EC ? 8B 45 ? 8B 7D ? 8B 75 ? 89 45 ? A1", 0x00, std::experimental::make_array<uint8_t>(0x31, 0xC0, 0x40, 0xC3));
        /* Mute report related console messages */
        /*static BytePatch muteReports(gSignatures.GetClientSignature,
            "E8 ? ? ? ? A1 ? ? ? ? 89 7D", 0x00,
            std::experimental::make_array<uint8_t>(0xEB, 0x03)); */
        patch.Patch();
        // muteReports.Patch(); TODO: Sadly this patch died, get a new signature
        patched_report = true;
    }
    if (!reason)
        reason = UniformRandomInt(1, 4);

    int success = ReportPlayer_fn(id, reason);
    // logging::InfoConsole("Reported [U:1:%u] for reason: %d (success: %d)", CSteamID(id).GetAccountID(), reason, success);
}

static void report_paint()
{
    if (report_delay.test_and_set(*autoReportDelay))
    {
        report(to_report.back(), *autoReportReason);
        to_report.pop_back();
    }
    if (to_report.empty())
        EC::Unregister(EC::Paint, "paint_report");
}

void reportall()
{
    /* Don't initiate if not reported everybody */
    if (!to_report.empty())
        return;

    for (int i = 1; i <= g_IEngine->GetMaxClients(); i++)
    {
        CachedEntity *ent = ENTITY(i);
        // We only want a nullptr check since dormant entities are still
        // on the server
        if (!ent)
            continue;

        // Pointer comparison is fine
        if (ent == LOCAL_E)
            continue;
        player_info_s info;
        if (g_IEngine->GetPlayerInfo(i, &info) && info.friendsID)
        {
            if (!player_tools::shouldTargetSteamId(info.friendsID))
                continue;
            to_report.emplace_back(CSteamIDFrom32(info.friendsID).ConvertToUint64());
        }
    }
    if (!to_report.empty())
        EC::Register(EC::Paint, report_paint, "paint_report");
}

CatCommand report_debug("report_debug", "debug", [](const CCommand &arg) {
    if (arg.ArgC() <= 1)
        return void(reportall());

    uint32_t id = std::strtoul(arg.Arg(1), nullptr, 10);
    if (!id)
    {
        logging::InfoConsole("Invalid steam id");
        return;
    }
    int reason = 1;
    if (arg.ArgC() >= 3)
        reason = std::strtol(arg.Arg(2), nullptr, 10);

    report(CSteamIDFrom32(id).ConvertToUint64(), reason);
});

void report_player(uint32_t id)
{
    if (*autoReport > 0)
        report(CSteamIDFrom32(id).ConvertToUint64(), *autoReportReason);
}

void report_player(uint64_t id)
{
    if (*autoReport > 0)
        report(id, *autoReportReason);
}

Timer crouchcdr{};
void smart_crouch()
{
    if (g_Settings.bInvalid)
        return;
    if (!current_user_cmd)
        return;
    if (*always_crouch)
    {
        current_user_cmd->buttons |= IN_DUCK;
        if (crouchcdr.test_and_set(10000))
            current_user_cmd->buttons &= ~IN_DUCK;
        return;
    }
    bool foundtar      = false;
    static bool crouch = false;
    if (crouchcdr.test_and_set(2000))
    {
        for (int i = 1; i <= g_IEngine->GetMaxClients(); i++)
        {
            auto ent = ENTITY(i);
            if (CE_BAD(ent) || ent->m_Type() != ENTITY_PLAYER || ent->m_iTeam() == LOCAL_E->m_iTeam() || !(ent->hitboxes.GetHitbox(0)) || !(ent->m_bAlivePlayer()) || !player_tools::shouldTarget(ent))
                continue;
            bool failedvis = false;
            for (int j = 0; j < 18; j++)
                if (IsVectorVisible(g_pLocalPlayer->v_Eye, ent->hitboxes.GetHitbox(j)->center))
                    failedvis = true;
            if (failedvis)
                continue;
            for (int j = 0; j < 18; j++)
            {
                if (!LOCAL_E->hitboxes.GetHitbox(j))
                    continue;
                // Check if they see my hitboxes
                if (!IsVectorVisible(ent->hitboxes.GetHitbox(0)->center, LOCAL_E->hitboxes.GetHitbox(j)->center) && !IsVectorVisible(ent->hitboxes.GetHitbox(0)->center, LOCAL_E->hitboxes.GetHitbox(j)->min) && !IsVectorVisible(ent->hitboxes.GetHitbox(0)->center, LOCAL_E->hitboxes.GetHitbox(j)->max))
                    continue;
                foundtar = true;
                crouch   = true;
            }
        }
        if (!foundtar && crouch)
            crouch = false;
    }
    if (crouch)
        current_user_cmd->buttons |= IN_DUCK;
}

CatCommand print_ammo("debug_print_ammo", "debug", []() {
    if (CE_BAD(LOCAL_E) || !LOCAL_E->m_bAlivePlayer() || CE_BAD(LOCAL_W))
        return;
    logging::Info("Current slot: %d", re::C_BaseCombatWeapon::GetSlot(RAW_ENT(LOCAL_W)));
    for (int i = 0; i < 10; i++)
        logging::Info("Ammo Table %d: %d", i, CE_INT(LOCAL_E, netvar.m_iAmmo + i * 4));
});

static Timer disguise{};
static Timer report_timer{};
static std::string health = "Health: 0/0";
static std::string ammo   = "Ammo: 0/0";
static int max_ammo;
static CachedEntity *local_w;
// TODO: add more stuffs
static void cm()
{
    if (*autoReport > 0 && report_timer.test_and_set(*autoReport * 1000))
        reportall();

    if (!*catbotmode)
        return;

    if (CE_GOOD(LOCAL_E))
    {
        if (LOCAL_W != local_w)
        {
            local_w  = LOCAL_W;
            max_ammo = 0;
        }
        float max_hp  = g_pPlayerResource->GetMaxHealth(LOCAL_E);
        float curr_hp = CE_INT(LOCAL_E, netvar.iHealth);
        int ammo0     = CE_INT(LOCAL_E, netvar.m_iClip2);
        int ammo2     = CE_INT(LOCAL_E, netvar.m_iClip1);
        if (ammo0 + ammo2 > max_ammo)
            max_ammo = ammo0 + ammo2;
        health = format("Health: ", curr_hp, "/", max_hp);
        ammo   = format("Ammo: ", ammo0 + ammo2, "/", max_ammo);
    }
    if (g_Settings.bInvalid)
        return;

    if (CE_BAD(LOCAL_E) || CE_BAD(LOCAL_W))
        return;

    if (*auto_crouch)
        smart_crouch();

    static const int classes[3]{ tf_spy, tf_sniper, tf_pyro };
    if (*auto_disguise && g_pPlayerResource->GetClass(LOCAL_E) == tf_spy && !IsPlayerDisguised(LOCAL_E) && disguise.test_and_set(3000))
    {
        int teamtodisguise = (LOCAL_E->m_iTeam() == TEAM_RED) ? TEAM_RED - 1 : TEAM_BLU - 1;
        int classtojoin    = classes[rand() % 3];
        g_IEngine->ClientCmd_Unrestricted(format("disguise ", classtojoin, " ", teamtodisguise).c_str());
    }
}

int count_ipc{ 0 };
static std::vector<unsigned> ipc_list{ 0 };
static std::vector<unsigned> ipc_blacklist{};

static bool waiting_for_quit_bool{ false };
static Timer waiting_for_quit_timer{};

#if ENABLE_IPC
void update_ipc_data(ipc::user_data_s &data)
{
    data.ingame.bot_count = count_ipc;
}
static Timer abandon_unix_timestamps{};
  static void Paint() {
    if (abandon_if && abandon_if_hour && ipc::peer) {
      if (abandon_unix_timestamps.test_and_set(30000)) {
        int diff = time(nullptr) - ipc::peer->memory->peer_user_data[ipc::peer->client_id].ts_connected;

        if (diff >= *abandon_if_timer * 60^2) {
          logging::Info("We have been in a match for too long! abandoning");

          tfmm::abandon();
          g_IEngine->ClientCmd_Unrestricted("killserver;disconnect");
        }
      }
    }
  }
#endif

void update()
{
    if (!catbotmode)
        return;

    if (g_Settings.bInvalid)
        return;

    if (CE_BAD(LOCAL_E))
        return;

#if ENABLE_NULL_GRAPHICS
    static Timer unstuck{};
    if (LOCAL_E->m_bAlivePlayer())
        unstuck.update();
    if (unstuck.test_and_set(10000))
        hack::command_stack().push("menuclosed");
#endif

    if (micspam)
    {
        if (micspam_on && micspam_on_timer.test_and_set(*micspam_on * 1000))
            g_IEngine->ClientCmd_Unrestricted("+voicerecord");
        if (micspam_off && micspam_off_timer.test_and_set(*micspam_off * 1000))
            g_IEngine->ClientCmd_Unrestricted("-voicerecord");
    }

    if (abandon_if && timer_abandon.test_and_set(2000) && level_init_timer.check(13000))
    {
        count_ipc = 0;
        ipc_list.clear();
        int count_total = 0;
        int count_team  = 0;
        int count_bot   = 0;

        auto lobby = CTFLobbyShared::GetLobby();
        if (!lobby)
            return;

        CTFLobbyPlayer *player;
        CTFLobbyPlayer *local_player = lobby->GetPlayer(lobby->GetMemberIndexBySteamID(g_ISteamUser->GetSteamID()));
        if (!local_player)
            return;

        int i, members, local_team;
        local_team = local_player->GetTeam();

        members = lobby->GetNumMembers();
        for (i = 0; i < members; ++i)
        {
            player = lobby->GetPlayer(i);
            if (!player)
                continue;
            int team  = player->GetTeam();
            uint32 id = player->GetID().GetAccountID();

            ++count_total;
            if (team == local_team)
                count_team++;

            auto &pl   = playerlist::AccessData(id);
            auto state = pl.state;

            if (state == playerlist::EState::CAT || state == playerlist::EState::ANTIBOT || state == playerlist::EState::PARTY)
                count_bot++;
            if (state == playerlist::EState::IPC)
            {
                ipc_list.push_back(id);
                count_ipc++;
                count_bot++;
            }
        }

        if (abandon_if_count_pending)
        {
            int pending = lobby->GetNumPendingPlayers();
            for (i = 0; i < pending; ++i)
            {
                player = lobby->GetPendingPlayer(i);
                if (!player)
                    continue;

                int team  = player->GetTeam();
                uint32 id = player->GetID().GetAccountID();

                ++count_total;
                if (team == local_team)
                    count_team++;

                auto &pl   = playerlist::AccessData(id);
                auto state = pl.state;

                if (state == playerlist::EState::CAT || state == playerlist::EState::ANTIBOT || state == playerlist::EState::PARTY)
                    count_bot++;
                if (state == playerlist::EState::IPC)
                {
                    ipc_list.push_back(id);
                    count_ipc++;
                    count_bot++;
                }
            }
        }

        if (abandon_if_ipc_bots_gte)
        {
            if (count_ipc >= int(abandon_if_ipc_bots_gte))
            {
                // Store local IPC Id and assign to the quit_id variable for later
                // comparisions
                unsigned local_ipcid = ipc::peer->client_id;
                unsigned quit_id     = local_ipcid;

                // Iterate all the players marked as bot
                for (auto &id : ipc_list)
                {
                    // We already know we shouldn't quit, so just break out of the loop
                    if (quit_id < local_ipcid)
                        break;

                    // Reduce code size
                    auto &peer_mem = ipc::peer->memory;

                    // Iterate all ipc peers
                    for (unsigned i = 0; i < cat_ipc::max_peers; i++)
                    {
                        // If that ipc peer is alive and in has the steamid of that player
                        if (!peer_mem->peer_data[i].free && peer_mem->peer_user_data[i].friendid == id)
                        {
                            // Check against blacklist
                            if (std::find(ipc_blacklist.begin(), ipc_blacklist.end(), i) != ipc_blacklist.end())
                                continue;

                            // Found someone with a lower ipc id
                            if (i < local_ipcid)
                            {
                                quit_id = i;
                                break;
                            }
                        }
                    }
                }
                // Only quit if you are the player with the lowest ipc id
                if (quit_id == local_ipcid)
                {
                    // Clear blacklist related stuff
                    waiting_for_quit_bool = false;
                    ipc_blacklist.clear();
                    if (abandon_if_ipc_bots_gte)
                    {
                        logging::Info("Abandoning because there are %d local players "
                                      "in game, and abandon_if_ipc_bots_gte is %d.",
                                      count_ipc, int(abandon_if_ipc_bots_gte));
                        tfmm::abandon();
                    }
                    return;
                }
                else
                {
                    if (!waiting_for_quit_bool)
                    {
                        // Waiting for that ipc id to quit, we use this timer in order to
                        // blacklist ipc peers which refuse to quit for some reason
                        waiting_for_quit_bool = true;
                        waiting_for_quit_timer.update();
                    }
                    else
                    {
                        // IPC peer isn't leaving, blacklist for now
                        if (waiting_for_quit_timer.test_and_set(10000))
                        {
                            ipc_blacklist.push_back(quit_id);
                            waiting_for_quit_bool = false;
                        }
                    }
                }
            }
            else
            {
                // Reset Bool because no reason to quit
                waiting_for_quit_bool = false;
                ipc_blacklist.clear();
            }
        }
        if (abandon_if_humans_lte)
        {
            if (count_total - count_bot <= int(abandon_if_humans_lte))
            {
                if (abandon_if_invite_required)
                {
                    if (tfmm::getPendingInvites())
                    {
                        logging::Info("Abandoning because there are %d non-bots in "
                                      "game, and abandon_if_humans_lte is %d, and we "
                                      "have an invite ready.",
                                      count_total - count_bot, int(abandon_if_humans_lte));
                        tfmm::abandon();
                        g_IEngine->ClientCmd_Unrestricted("tf_mm_accept_match_invite 0");
                    }
                }
                else
                {
                    tfmm::abandon();
                    logging::Info("Abandoning because there are %d non-bots in "
                                  "game, and abandon_if_humans_lte is %d.",
                                  count_total - count_bot, int(abandon_if_humans_lte));
                }
                return;
            }
        }
        if (abandon_if_players_lte)
        {
            if (count_total <= int(abandon_if_players_lte))
            {
                if (abandon_if_invite_required)
                {
                    if (tfmm::getPendingInvites())
                    {
                        logging::Info("Abandoning because there are %d total players "
                                      "in game, and abandon_if_players_lte is %d, and we "
                                      "have an invite ready.",
                                      count_total, int(abandon_if_players_lte));
                        tfmm::abandon();
                        g_IEngine->ClientCmd_Unrestricted("tf_mm_accept_match_invite 0");
                    }
                }
                else
                {
                    logging::Info("Abandoning because there are %d total players "
                                  "in game, and abandon_if_players_lte is %d.",
                                  count_total, int(abandon_if_players_lte));
                    tfmm::abandon();
                }
                return;
            }
        }
        if (abandon_if_bots_gte)
        {
            if (count_bot >= int(abandon_if_bots_gte))
            {
                if (abandon_if_invite_required)
                {
                    if (tfmm::getPendingInvites())
                    {
                        logging::Info("Abandoning because there are %d total bots "
                                      "in game, and abandon_if_bots_gte is %d, and we "
                                      "have an invite ready.",
                                      count_total, int(abandon_if_players_lte));
                        tfmm::abandon();
                        g_IEngine->ClientCmd_Unrestricted("tf_mm_accept_match_invite 0");
                    }
                }
                else
                {
                    logging::Info("Abandoning because there are %d total bots "
                                  "in game, and abandon_if_bots_gte is %d.",
                                  count_total, int(abandon_if_players_lte));
                    tfmm::abandon();
                }
                return;
            }
        }
        if (abandon_if_team_lte)
        {
            if (count_team <= int(abandon_if_team_lte))
            {
                if (abandon_if_invite_required)
                {
                    if (tfmm::getPendingInvites())
                    {
                        logging::Info("Abandoning because there are %d total teammates "
                                      "in game, and abandon_if_team_lte is %d, and we "
                                      "have an invite ready.",
                                      count_team, int(abandon_if_team_lte));
                        tfmm::abandon();
                        g_IEngine->ClientCmd_Unrestricted("tf_mm_accept_match_invite 0");
                    }
                }
                else
                {
                    logging::Info("Abandoning because there are %d total teammates "
                                  "in game, and abandon_if_team_lte is %d.",
                                  count_team, int(abandon_if_team_lte));
                    tfmm::abandon();
                }
                return;
            }
        }
    }
}

void level_init()
{
    level_init_timer.update();
}

#if ENABLE_VISUALS
static void draw()
{
    if (!catbotmode || !anti_motd)
        return;
    if (CE_BAD(LOCAL_E) || !LOCAL_E->m_bAlivePlayer())
        return;
    AddCenterString(health, colors::green);
    AddCenterString(ammo, colors::yellow);
}
#endif

class MapVoteListener : public IGameEventListener2
{
    void FireGameEvent(IGameEvent *) override
    {
        if (catbotmode && autovote_map)
            g_IEngine->ServerCmd("next_map_vote 0");
    }
};

MapVoteListener &listener2()
{
    static MapVoteListener object{};
    return object;
}

static InitRoutine runinit([]() {
    EC::Register(EC::CreateMove, cm, "cm_catbot", EC::average);
    EC::Register(EC::CreateMove, update, "cm2_catbot", EC::average);
    EC::Register(EC::LevelInit, level_init, "levelinit_catbot", EC::average);
#if ENABLE_VISUALS
    EC::Register(EC::Draw, draw, "draw_catbot", EC::average);
#endif
    g_IEventManager2->AddListener(&listener2(), "vote_maps_changed", false);
});
} // namespace hacks::shared::catbot
