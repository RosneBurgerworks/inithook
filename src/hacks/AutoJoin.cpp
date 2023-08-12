/*
 * AutoJoin.cpp
 *
 *  Created on: Jul 28, 2017
 *      Author: nullifiedcat
 */

#include <settings/Int.hpp>
#include "HookTools.hpp"

#include "common.hpp"
#include "hack.hpp"
#include "DataCenter.hpp"

static settings::Boolean autojoin_team{ "autojoin.team", "false" };
static settings::Int autojoin_class{ "autojoin.class", "0" };
static settings::Boolean auto_queue{ "autojoin.auto-queue", "false" };
static settings::Int auto_queue_restart{ "autojoin.auto-queue.restart", "0" };
static settings::Int auto_queue_party_min{ "autojoin.auto-queue.party_min", "1" };
static settings::Boolean auto_requeue{ "autojoin.auto-requeue", "false" };
static settings::Boolean auto_accept_invites{ "autojoin.accept_invites", "false" };

/* Removes cooldown before game automatically forces you to join game */
static settings::Boolean no_autojoin{ "autojoin.no-autojoin", "true" };
static settings::Boolean store_invites{ "autojoin.no-autojoin.store_invites", "false" };
static settings::Boolean store_invites_lite{ "autojoin.no-autojoin.store_invites.lite", "false" };

#if ENABLE_NULL_GRAPHICS
static settings::Int die_on_long_join{ "autojoin.die-on-long-joins", "600" };
#else
static settings::Int die_on_long_join{ "autojoin.die-on-long-joins", "0" };
#endif

namespace hacks::shared::autojoin
{

/*
 * Credits to Blackfire for helping me with auto-requeue!
 */

const std::string classnames[] = { "scout", "sniper", "soldier", "demoman", "medic", "heavyweapons", "pyro", "spy", "engineer" };

bool UnassignedTeam()
{
    return !g_pLocalPlayer->team or (g_pLocalPlayer->team == TEAM_SPEC);
}

bool UnassignedClass()
{
    return g_pLocalPlayer->clazz != *autojoin_class;
}

static Timer autoteam_timer{}, startqueue_timer{}, accept_invites_lag{}, queue_time_timer{};
static CTimer startqueue_restart{};
static int join_time  = 0;
static int queue_time = 0;

static bool isLeader(re::CTFPartyClient *pc)
{
    int num_members = pc->GetNumMembers();
    if (num_members <= 1)
        return true;

    CSteamID id;
    pc->GetCurrentPartyLeader(id);

    return id == g_ISteamUser->GetSteamID();
}

void updateSearch()
{
    auto gc = re::CTFGCClientSystem::GTFGCClientSystem();
    auto pc = re::CTFPartyClient::GTFPartyClient();
    if (!gc || !pc)
        return;

    int invites = tfmm::getPendingInvites();
    auto lobby  = CTFLobbyShared::GetLobby();

#if ENABLE_IPC
    if (ipc::peer && ipc::peer->memory)
        ipc::peer->memory->peer_user_data[ipc::peer->client_id].has_match_invite = invites;
#endif

    if (*auto_accept_invites)
    {
        bool can_join = !gc->BConnectedToMatchServer(false);
        if (can_join)
        {
            if (accept_invites_lag.test_and_set(1000))
            {
                if (gc->BHaveLiveMatch())
                {
                    gc->JoinMMMatch();
                    startqueue_timer.update();
                }

                if (!lobby)
                {
                    join_time++;

                    if (invites)
                    {
                        g_IEngine->ClientCmd_Unrestricted("tf_mm_accept_match_invite 0");
                        startqueue_timer.update();
                    }

                    // Thanks alex
                    if (*die_on_long_join > 0 && join_time >= *die_on_long_join)
                    {
                        logging::InfoConsole("FATAL! We've tried to accept invite or have been queued for %d (%i) seconds. It is likely Steam is dead", *die_on_long_join, join_time);
                        std::_Exit(EXIT_SUCCESS);
                    }
                }
                else
                    join_time = 0;
            }
        }
        else
            join_time = 0;
    }
    bool is_leader = isLeader(pc);
    if (is_leader && (auto_queue || store_invites))
    {
        if (pc->GetNumMembers() < *auto_queue_party_min)
            return;
        if (tfmm::isMMBanned() || pc->IsAnybodyBanned(false))
            return;
        bool in_queue = pc->BInQueueForMatchGroup(tfmm::getQueue());
        if (queue_time_timer.test_and_set(1000))
        {
            if (in_queue)
                queue_time++;
            else
                queue_time = 0;
#if ENABLE_IPC
            if (ipc::peer && ipc::peer->memory)
                ipc::peer->memory->peer_user_data[ipc::peer->client_id].queue_time = queue_time;
#endif
        }
        if (*store_invites)
        {
            if (!in_queue && (!store_invites_lite || !invites) && startqueue_timer.test_and_set(1000))
            {
                hacks::tf2::datacenter::Refresh();
                tfmm::startQueue();
                startqueue_timer.update();
            }
        }
        else
        {
            if (*auto_queue_restart > 0 && (in_queue || invites))
                startqueue_restart.set(*auto_queue_restart);
            if (in_queue)
                return;
            bool is_playing = gc->BConnectedToMatchServer(false) || gc->BHaveLiveMatch() || invites;
            if (*auto_queue_restart > 0 && startqueue_restart.check() || !is_playing && startqueue_timer.check(6000))
            {
                hacks::tf2::datacenter::Refresh();
                tfmm::startQueue();
                startqueue_timer.update();
            }
            if (is_playing)
                startqueue_timer.update();
        }
    }
    else if (!is_leader && auto_requeue)
    {
        /* We are always interested to requeue. No matter we are playing or not */
        if (!startqueue_timer.test_and_set(1000) && !invites && !pc->BInQueueForStandby())
        {
            hacks::tf2::datacenter::Refresh();
            tfmm::startQueueStandby();
        }
    }
}

static CatCommand debug_queue_time("debug_queue_time", "Print queue time", []() { logging::InfoConsole("Queue time (seconds): %d", queue_time); });

static void update()
{
    if (autoteam_timer.test_and_set(500))
    {
        if (autojoin_team and UnassignedTeam())
        {
            hack::ExecuteCommand("autoteam");
        }
        else if (autojoin_class and UnassignedClass())
        {
            if (int(autojoin_class) < 10)
                g_IEngine->ExecuteClientCmd(format("join_class ", classnames[int(autojoin_class) - 1]).c_str());
        }
    }
}

static InitRoutine init([]() {
    EC::Register(EC::CreateMove, update, "cm_autojoin", EC::average);
    EC::Register(EC::Paint, updateSearch, "paint_autojoin", EC::average);

    static BytePatch removeInviteTime(gSignatures.GetClientSignature, "55 89 e5 57 56 53 83 ec ? 8b ? ? 89 1c ? e8 ? ? ? ? f7 ? ? ? ? ? fd", 0x00, std::experimental::make_array<uint8_t>(0xC3));
    if (*no_autojoin)
        removeInviteTime.Patch();
    no_autojoin.installChangeCallback([](bool new_val) {
        if (new_val)
            removeInviteTime.Patch();
        else
            removeInviteTime.Shutdown();
    });
    EC::Register(
        EC::Shutdown, []() { removeInviteTime.Shutdown(); }, "shutdown_autojoin");
});
} // namespace hacks::shared::autojoin
