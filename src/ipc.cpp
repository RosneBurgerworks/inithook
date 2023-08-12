/*
 * ipc.cpp
 *
 *  Created on: Jun 27, 2019
 *      Author: Alex
 */

#include <hacks/CatBot.hpp>
#include <settings/Bool.hpp>
#include "ipc.hpp"

#include "common.hpp"
#include "hack.hpp"
#include "hitrate.hpp"

#if ENABLE_IPC

static settings::Boolean ipc_update_list{ "ipc.update-player-list", "true" };
static settings::Boolean ipc_auto_party{ "ipc.auto_party", "false" };
static settings::Int ipc_auto_party_max{ "ipc.auto_party.max", "6" };
static settings::String server_name{ "ipc.server", "cathook_followbot_server" };

namespace ipc
{

CatCommand fix_deadlock("ipc_fix_deadlock", "Fix deadlock", []() {
    if (peer)
    {
        pthread_mutex_unlock(&peer->memory->mutex);
    }
});

CatCommand id("ipc_id", "Echo ipc id", []() {
    if (!peer)
    {
        logging::InfoConsole("Not connected to IPC!");
        return;
    }

    logging::InfoConsole("%d", ipc::peer->client_id);
});

CatCommand connect("ipc_connect", "Connect to IPC server", [](const CCommand &args) {
    if (peer)
    {
        logging::InfoConsole("Already connected!");
        return;
    }
    int slot = -1;
    if (args.ArgC() >= 2)
        slot = strtoul(args.Arg(1), nullptr, 10);

    peer = new peer_t((*server_name).c_str(), false, false);
    if (!peer->Connect(slot))
    {
        delete peer;
        peer = nullptr;
        logging::InfoConsole("Failed to connect to IPC (check stderr for details)");
        return;
    }
    logging::InfoConsole("peer count: %i", peer->memory->peer_count);
    logging::InfoConsole("magic number: 0x%08x", peer->memory->global_data.magic_number);
    logging::InfoConsole("magic number offset: 0x%08x", (uintptr_t) &peer->memory->global_data.magic_number - (uintptr_t) peer->memory);
    peer->SetCommandHandler(commands::execute_client_cmd, [](cat_ipc::command_s &command, void *payload) { hack::command_stack().push(std::string((const char *) &command.cmd_data)); });
    peer->SetCommandHandler(commands::execute_client_cmd_long, [](cat_ipc::command_s &command, void *payload) { hack::command_stack().push(std::string((const char *) payload)); });
    user_data_s &data = peer->memory->peer_user_data[peer->client_id];

    // Preserve accumulated data
    ipc::user_data_s::accumulated_t accumulated;
    memcpy(&accumulated, &data.accumulated, sizeof(accumulated));
    memset(&data, 0, sizeof(data));
    memcpy(&data.accumulated, &accumulated, sizeof(accumulated));

    data.namesteal_target = 0;
    data.namesteal_target = 0;
    StoreClientData();
});

CatCommand disconnect("ipc_disconnect", "Disconnect from IPC server", []() {
    if (peer)
        delete peer;
    peer = nullptr;
});

CatCommand exec("ipc_exec", "Execute command (first argument = bot ID)", [](const CCommand &args) {
    if (!peer)
    {
        logging::InfoConsole("Not connected to IPC!");
        return;
    }

    char *endptr       = nullptr;
    unsigned target_id = strtol(args.Arg(1), &endptr, 10);
    if (endptr == args.Arg(1))
    {
        logging::InfoConsole("Target id is NaN!");
        return;
    }
    if (target_id > 255)
    {
        logging::InfoConsole("Invalid target id: %u", target_id);
        return;
    }
    {
        if (peer->memory->peer_data[target_id].free)
        {
            logging::InfoConsole("Trying to send command to a dead peer");
            return;
        }
    }
    std::string command = std::string(args.ArgS());
    command             = command.substr(command.find(' ', 0) + 1);
    ReplaceString(command, " && ", " ; ");
    if (command.length() >= 63)
    {
        peer->SendMessage(0, target_id, ipc::commands::execute_client_cmd_long, command.c_str(), command.length() + 1);
    }
    else
    {
        peer->SendMessage(command.c_str(), target_id, ipc::commands::execute_client_cmd, 0, 0);
    }
});

CatCommand exec_all("ipc_exec_all", "Execute command (on every peer)", [](const CCommand &args) {
    if (!peer)
    {
        logging::InfoConsole("Not connected to IPC!");
        return;
    }

    std::string command = args.ArgS();
    ReplaceString(command, " && ", " ; ");
    if (command.length() >= 63)
    {
        peer->SendMessage(0, -1, ipc::commands::execute_client_cmd_long, command.c_str(), command.length() + 1);
    }
    else
    {
        peer->SendMessage(command.c_str(), -1, ipc::commands::execute_client_cmd, 0, 0);
    }
});

peer_t *peer{ nullptr };

CatCommand debug_get_ingame_ipc("ipc_debug_dump_server", "Show other bots on server", []() {
    if (!peer)
    {
        logging::InfoConsole("Not connected to IPC!");
        return;
    }

    std::vector<unsigned> players{};
    for (int j = 1; j < 32; j++)
    {
        player_info_s info;
        if (g_IEngine->GetPlayerInfo(j, &info))
        {
            if (info.friendsID)
                players.push_back(info.friendsID);
        }
    }
    int count = 0;
    std::vector<unsigned> botlist{};
    for (unsigned i = 0; i < cat_ipc::max_peers; i++)
    {
        if (!ipc::peer->memory->peer_data[i].free)
        {
            for (auto &k : players)
            {
                if (ipc::peer->memory->peer_user_data[i].friendid && k == ipc::peer->memory->peer_user_data[i].friendid)
                {
                    botlist.push_back(i);
                    logging::InfoConsole("-> %u (%u)", i, ipc::peer->memory->peer_user_data[i].friendid);
                    count++;
                }
            }
        }
    }
    logging::InfoConsole("%d other IPC players on server", count);
});

void UpdateServerAddress(bool shutdown)
{
    if (!peer)
        return;
    const char *s_addr = "0.0.0.0";
    if (!shutdown and g_IEngine->GetNetChannelInfo())
        s_addr = g_IEngine->GetNetChannelInfo()->GetAddress();

    strncpy(peer->memory->peer_user_data[peer->client_id].ingame.server, s_addr, 24);
}

void update_mapname()
{
    if (!peer)
        return;

    user_data_s &data = peer->memory->peer_user_data[peer->client_id];
    strncpy(data.ingame.mapname, GetLevelName().c_str(), sizeof(data.ingame.mapname));
}
float framerate = 0.0f;
void UpdateTemporaryData()
{
    user_data_s &data = peer->memory->peer_user_data[peer->client_id];

    data.party_size = re::CTFPartyClient::GTFPartyClient()->GetNumMembers();
    data.connected  = g_IEngine->IsInGame();
    // TODO kills, deaths
    data.accumulated.shots     = hitrate::count_shots;
    data.accumulated.hits      = hitrate::count_hits;
    data.accumulated.headshots = hitrate::count_hits_head;

    if (data.connected)
    {
        if (CE_GOOD(LOCAL_E))
        {
            IClientEntity *player = RAW_ENT(LOCAL_E);
            int m_IDX             = LOCAL_E->m_IDX;
            data.ingame.good      = true;
            // TODO kills, deaths, shots, hits, headshots

            int score_saved = data.ingame.score;

            data.ingame.score      = g_pPlayerResource->GetScore(m_IDX);
            data.ingame.team       = g_pPlayerResource->GetTeam(m_IDX);
            data.ingame.role       = g_pPlayerResource->GetClass(LOCAL_E);
            data.ingame.life_state = NET_BYTE(player, netvar.iLifeState);
            data.ingame.health     = NET_INT(player, netvar.iHealth);
            data.ingame.health_max = g_pPlayerResource->GetMaxHealth(LOCAL_E);

            if (score_saved > data.ingame.score)
                score_saved = 0;

            data.accumulated.score += data.ingame.score - score_saved;

            data.ingame.x = g_pLocalPlayer->v_Origin.x;
            data.ingame.y = g_pLocalPlayer->v_Origin.y;
            data.ingame.z = g_pLocalPlayer->v_Origin.z;

            int players = 0;

            for (int i = 1; i < g_GlobalVars->maxClients; ++i)
            {
                if (g_IEntityList->GetClientEntity(i))
                    ++players;
                else
                    continue;
            }

            data.ingame.player_count = players;
            hacks::shared::catbot::update_ipc_data(data);
        }
        else
        {
            data.ingame.good = false;
        }
        if (g_IEngine->GetLevelName())
            update_mapname();
    }
}

void StoreClientData()
{
    if (!peer)
        return;

    UpdateServerAddress();
    user_data_s &data = peer->memory->peer_user_data[peer->client_id];
    data.friendid     = g_ISteamUser->GetSteamID().GetAccountID();
    data.ts_injected  = time_injected;
    if (g_ISteamUser && g_ISteamFriends)
        strncpy(data.name, hooked_methods::methods::GetFriendPersonaName(g_ISteamFriends, g_ISteamUser->GetSteamID()), sizeof(data.name));
}

void UpdatePlayerlist()
{
    if (peer && ipc_update_list)
    {
        for (unsigned i = 0; i < cat_ipc::max_peers; i++)
        {
            if (!peer->memory->peer_data[i].free)
            {
                playerlist::ChangeState(peer->memory->peer_user_data[i].friendid, playerlist::EState::IPC);
            }
        }
    }
}

bool block_auto_party{ false };

static CatCommand unblock_auto_party("auto_party_unblock", "Unblocks autoparty that was blocked by auto_leader.banned_leave", []() { block_auto_party = false; });

static void AutoParty()
{
    if (block_auto_party || !*ipc_auto_party || *ipc_auto_party_max <= 1)
        return;

    auto pc         = re::CTFPartyClient::GTFPartyClient();
    int party_size  = std::clamp(*ipc_auto_party_max, 1, 6);
    int num_members = pc->GetNumMembers();
    /* I am in a party with correct size, don't do anything */
    if (pc->GetNumMembers() == party_size)
        return;

    const CSteamID *members = pc->GetMembersArray();
    const auto isInParty    = [&](const CSteamID &id) -> bool {
        if (members)
            for (unsigned i = 0; i < num_members; ++i)
                if (id == members[i])
                    return true;
        return false;
    };
    user_data_s *to_join = nullptr;
    auto m               = peer->memory;
    unsigned peer_idx    = 0;
    bool invite_mode     = false;
    /* Search for somebody to join */
    for (unsigned i = 0; i < cat_ipc::max_peers; ++i, ++peer_idx)
    {
        if (m->peer_data[i].free)
        {
            --peer_idx;
            continue;
        }
        if (peer_idx % party_size == 0)
        {
            /* I am potential party leader, wait for other peers to join */
            if (peer->client_id == i)
            {
                if (!invite_mode)
                {
                    invite_mode = true;
                    continue;
                }
                break;
            }
            to_join = &m->peer_user_data[i];
        }
        if (invite_mode)
        {
            CSteamID to_join = CSteamIDFrom32(m->peer_user_data[i].friendid);
            if (!isInParty(to_join))
                pc->BInvitePlayerToParty(to_join);
            continue;
        }
        if (peer->client_id != i)
            continue;
        if (!to_join || to_join->party_size >= party_size)
            break;

        CSteamID leader, join_id = CSteamIDFrom32(to_join->friendid);
        pc->GetCurrentPartyLeader(leader);
        if (leader != join_id && !isInParty(join_id))
            pc->BRequestJoinPlayer(join_id);

        break;
    }
}

static void Paint()
{
    if (!peer)
        return;

    static Timer long_timer{}, short_timer{};
    if (long_timer.test_and_set(1000 * 10))
    {
        StoreClientData();
        AutoParty();
    }
    if (short_timer.test_and_set(1000))
    {
        if (ipc::peer && ipc::peer->memory && peer->HasCommands())
            peer->ProcessCommands();

        UpdateTemporaryData();
        UpdatePlayerlist();
    }
}

static InitRoutine init([]() { EC::Register(EC::Paint, Paint, "ipc_paint", EC::very_early); });
} // namespace ipc

#endif
