/*
 * tfmm.cpp
 *
 *  Created on: Dec 7, 2017
 *      Author: nullifiedcat
 */

#include <settings/Int.hpp>
#include "common.hpp"
#include "core/e8call.hpp"

static settings::Int queue{ "autoqueue.mode", "7" };

#if ENABLE_NULL_GRAPHICS
static settings::Boolean auto_mark_party{"player-tools.set-party-state", "false"};
#else
static settings::Boolean auto_mark_party{"player-tools.set-party-state", "true"};
#endif

CatCommand cmd_queue_start("mm_queue_casual", "Start casual queue", []() { tfmm::startQueue(); });
CatCommand queue_party("mm_queue_party", "Queue for Party", []() {
    re::CTFPartyClient *client = re::CTFPartyClient::GTFPartyClient();
    client->RequestQueueForStandby();
});
CatCommand cmd_abandon("mm_abandon", "Abandon match", []() { tfmm::abandon(); });

CatCommand abandoncmd("disconnect_and_abandon", "Disconnect and abandon", []() { tfmm::disconnectAndAbandon(); });


CatCommand join("mm_join", "Join mm Match", []() {
    auto gc = re::CTFGCClientSystem::GTFGCClientSystem();
    if (gc)
        gc->JoinMMMatch();
});
CatCommand get_state("mm_state", "Get party state", []() {
    re::CTFParty *party = re::CTFParty::GetParty();
    if (!party)
    {
        logging::Info("Party == NULL");
        return;
    }
    logging::Info("State: %d", re::CTFParty::state_(party));
});

static CatCommand mm_stop_queue("mm_stop_queue", "Stop current TF2 MM queue", []() { tfmm::leaveQueue(); });
static CatCommand mm_debug_leader("mm_debug_leader", "Get party's leader", []() {
    CSteamID id;
    bool success = re::CTFPartyClient::GTFPartyClient()->GetCurrentPartyLeader(id);
    if (success)
        logging::InfoConsole("%u", id.GetAccountID());
    else
        logging::InfoConsole("Failed to get party leader");
});
static CatCommand mm_debug_promote("mm_debug_promote", "Promote player to leader", [](const CCommand &args) {
    if (args.ArgC() < 2)
        return;

    uint32_t id32 = std::strtoul(args.Arg(1), nullptr, 10);
    CSteamID id = CSteamIDFrom32(id32);
    logging::InfoConsole("Attempting to promote %u", id32);
    int succ = re::CTFPartyClient::GTFPartyClient()->PromotePlayerToLeader(id);
    logging::InfoConsole("Success ? %d", succ);
});
static CatCommand mm_debug_kick("mm_debug_kick", "Kick player from party", [](const CCommand &args) {
    if (args.ArgC() < 2)
        return;

    uint32_t id32 = std::strtoul(args.Arg(1), nullptr, 10);
    CSteamID id = CSteamIDFrom32(id32);
    logging::InfoConsole("Attempting to kick %u", id);
    int succ = re::CTFPartyClient::GTFPartyClient()->KickPlayer(id);
    logging::InfoConsole("Success ? %d", succ);
});
static CatCommand mm_debug_chat("mm_debug_chat", "Debug party chat", [](const CCommand &args) {
    if (args.ArgC() <= 1)
        return;

    re::CTFPartyClient::GTFPartyClient()->SendPartyChat(args.ArgS());
});
static CatCommand mm_debug_users("mm_debug_users", "Print steamIDs of all party members", []() {
    CSteamID id;
    auto pc = re::CTFPartyClient::GTFPartyClient();
    int num = pc->GetNumMembers();

    logging::InfoConsole("Total party members: %u", num);
    for (int i = 0; i < num; ++i)
    {
        if (!pc->GetMemberID(i, id))
        {
            logging::InfoConsole("Failed to get member %d", i);
            continue;
        }
        logging::InfoConsole("%u %d %d", id.GetAccountID(),
            pc->IsMemberBanned(false, i), pc->IsMemberBanned(true, i));
    }
});
static CatCommand mm_friend_members("mm_friend_members", "Set friendly state for every party participant", []() {
    CSteamID id;

    auto pc = re::CTFPartyClient::GTFPartyClient();
    int num = pc->GetNumMembers();

    for (int i = 0; i < num; ++i)
    {
        pc->GetMemberID(i, id);
        auto &data = playerlist::AccessData(id.GetAccountID());
        data.state = playerlist::EState::FRIEND;
        logging::Info("Friended %u", id.GetAccountID());
    }
});
static CatCommand mm_debug_params("mm_debug_params", "Print several GC params",
[]() {
    auto pc = re::CTFPartyClient::GTFPartyClient();
    logging::InfoConsole("In queue for match: %d", pc->BInQueueForMatchGroup(tfmm::getQueue()));
    logging::InfoConsole("In standby queue: %d", pc->BInQueueForStandby());
    logging::InfoConsole("Pending invites: %d", tfmm::getPendingInvites());
    auto gc = re::CTFGCClientSystem::GTFGCClientSystem();
    if (!gc)
        return;

    logging::InfoConsole("BConnectedToMatchServer(false) = %d", gc->BConnectedToMatchServer(false));
    logging::InfoConsole("BHaveLiveMatch() = %d", gc->BHaveLiveMatch());
});

namespace tfmm
{
int queuecount = 0;

static bool old_isMMBanned()
{
    auto client = re::CTFPartyClient::GTFPartyClient();
    if (!client || ((client->BInQueueForMatchGroup(7) || client->BInQueueForStandby()) && queuecount < 10))
    {
        queuecount = 0;
        return false;
    }
    return true;
}
static bool getMMBanData(int type, int *time, int *time2)
{
    typedef bool (*GetMMBanData_t)(int, int*, int*);
    static uintptr_t addr = gSignatures.GetClientSignature("55 89 E5 57 56 53 83 EC ? 8B 5D 08 8B 75 0C 8B 7D 10 83 FB FF");
    static GetMMBanData_t GetMMBanData_fn = GetMMBanData_t(addr);

    if (!addr)
    {
        *time  = -1;
        *time2 = -1;
        logging::InfoConsole("GetMMBanData sig is broken");
        return old_isMMBanned();
    }
    return GetMMBanData_fn(type, time, time2);
}

static CatCommand mm_debug_banned("mm_debug_banned",
    "Prints if your are MM banned and extra data if you are banned",
[]() {
    int i, time, left;
    for (i = 0; i < 1; ++i)
    {
        time = left = 0;
        bool banned = getMMBanData(0, &time, &left);
        logging::InfoConsole("%d: banned %d, time %d, left %d", banned, time, left);
    }
});

bool isMMBanned()
{
    /* Competitive only bans are not interesting */
    return getMMBanData(0, nullptr, nullptr);
}
int getQueue()
{
    return *queue;
}

void startQueue()
{
    re::CTFPartyClient *client = re::CTFPartyClient::GTFPartyClient();
    if (client)
    {
        if (*queue == 7)
            client->LoadSavedCasualCriteria();
        client->RequestQueueForMatch((int) queue);
        // client->RequestQueueForStandby();
        queuecount++;
    }
    else
        logging::Info("queue_start: CTFPartyClient == null!");
}
void startQueueStandby()
{
    re::CTFPartyClient *client = re::CTFPartyClient::GTFPartyClient();
    if (client)
    {
        client->RequestQueueForStandby();
    }
}
void leaveQueue()
{
    re::CTFPartyClient *client = re::CTFPartyClient::GTFPartyClient();
    if (client)
        client->RequestLeaveForMatch((int) queue);
    else
        logging::Info("queue_start: CTFPartyClient == null!");
}

void disconnectAndAbandon()
{
    abandon();
    leaveQueue();
}

void abandon()
{
    re::CTFGCClientSystem *gc = re::CTFGCClientSystem::GTFGCClientSystem();
    if (gc != nullptr /*&& gc->BConnectedToMatchServer(false)*/)
        gc->AbandonCurrentMatch();
    else
        logging::Info("abandon: CTFGCClientSystem == null!");
}

int getPendingInvites()
{
    static uintptr_t addr = gSignatures.GetClientSignature("C7 04 24 ? ? ? ? 8D 7D ? 31 F6 E8");
    static uintptr_t offset0 = uintptr_t(*(uintptr_t *)(addr + 0x3));
    static uintptr_t offset1 = uintptr_t(e8call((void *)(addr + 0xD)));
    typedef int (*GetPendingInvites_t)(uintptr_t);
    GetPendingInvites_t GetPendingInvites = GetPendingInvites_t(offset1);

    return GetPendingInvites(offset0);
}

static Timer friend_party_t{};
void friend_party()
{
    if (auto_mark_party && friend_party_t.test_and_set(10000))
    {
        re::CTFPartyClient *pc = re::CTFPartyClient::GTFPartyClient();
        if (pc)
        {
            unsigned steamid_local                = g_ISteamUser->GetSteamID().GetAccountID();
            std::vector<unsigned> valid_steam_ids = pc->GetPartySteamIDs();
            for (auto steamid : valid_steam_ids)
                if (steamid && steamid != steamid_local)
                    playerlist::ChangeState(steamid, playerlist::EState::PARTY);
        }
    }
}

static InitRoutine init([]() { EC::Register(EC::Paint, friend_party, "paint_friendparty"); });

} // namespace tfmm
