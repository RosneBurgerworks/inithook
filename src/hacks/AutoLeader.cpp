/* License: GPLv3
 * See LICENSE for more information about licensing
 */
#include <settings/Bool.hpp>
#include "common.hpp"

static settings::Boolean auto_leader{ "auto_leader.enable", "false" };
#if ENABLE_IPC
static settings::Boolean auto_leader_ipc{ "auto_leader.force_ipc", "true" };
#endif
static settings::Int banned_leave{ "auto_leader.banned_leave", "0" };

static CSteamID member_ids[5];
static int member_count, selected_member;

static inline void ipc_auto_party_state(bool block)
{
#if ENABLE_IPC
    ipc::block_auto_party = block;
#endif
}

#if ENABLE_IPC
static int leadership_ipc(const CSteamID &id)
{
    auto m = ipc::peer->memory;
    for (unsigned i = 0; i < cat_ipc::max_peers; ++i)
    {
        if (m->peer_data[i].free)
            continue;
        const auto &ud = m->peer_user_data[i];
        if (ud.friendid == id.GetAccountID())
            return 1 + ud.connected;
    }
    return 0;
}
#endif

static void leadership_check()
{
    static int ban_came_back, was_playing;
    static std::time_t t, ban_t;
    CSteamID my_id, id;
    int i, num_members;

    auto gc = re::CTFGCClientSystem::GTFGCClientSystem();
    if (!gc)
        return;

    std::time_t now = std::time(nullptr);
    auto pc = re::CTFPartyClient::GTFPartyClient();
    /* Don't do anything in empty party */
    if ((num_members = pc->GetNumMembers()) <= 1)
    {
        /* If come back timer was set after ban leave try to rejoin
         * party through any of the id we just remembered
         */
        if (*banned_leave > 0 && member_count && ban_t - now <= 0)
        {
            /* If not joined during 10 seconds try another user id */
            ban_t += 10;
            pc->BRequestJoinPlayer(member_ids[selected_member]);
            selected_member = (selected_member + 1) % member_count;
            logging::Info("Coming back to party! Trying to join %u", member_ids[selected_member].GetAccountID());
            ban_came_back = 1;
        }
        t = now + 10;
        return;
    }
    /* After we came back, wait for requeue to work */
    if (ban_came_back)
    {
        if (ban_came_back == 1)
        {
            t = now + 10;
            ban_came_back = 2;
            member_count = 0;
        }
        if (t - now > 0)
            return;

        ban_came_back = 0;
    }
    my_id = g_ISteamUser->GetSteamID();
    /* Am I party leader ? */
    if (!pc->GetCurrentPartyLeader(id) || my_id != id)
    {
        /* If banned and not in standby queue, leave */
        if (tfmm::isMMBanned() && *banned_leave && !pc->BInQueueForStandby() &&
            !gc->BConnectedToMatchServer(false) && !gc->BHaveLiveMatch() &&
            !tfmm::getPendingInvites())
        {
            if (t - now > 0)
                return;

            t = now + 5;
            member_count = 0;
            /* Remember party members */
            if (*banned_leave > 0)
            {
                for (i = 0; i < num_members; ++i)
                {
                    pc->GetMemberID(i, member_ids[member_count]);
                    /* Don't join ourselves
                     * Don't join banned members, they can't queue
                     */
                    if (member_ids[member_count] == my_id || pc->IsMemberBanned(false, i))
                        continue;

                    ++member_count;
                }
                /* No point in doing this. Everybody banned */
                if (!member_count)
                    return;

                ipc_auto_party_state(true);
                ban_t = now + *banned_leave;
            }
            logging::Info("I am banned. Leaving party");
            if (*banned_leave > 0)
                logging::Info("I'll be back in %d seconds (have %d backup members)",
                    *banned_leave, member_count);

            g_IEngine->ClientCmd_Unrestricted("tf_party_leave");
            return;
        }
        was_playing = false;
        t = now + 2;
        ban_t = 0;
        ipc_auto_party_state(false);
        return;
    }
    ban_t = 0;
    ipc_auto_party_state(false);
    /* If we are playing, don't do anything else. Party members
     * will eventually come back
     */
    if (gc->BConnectedToMatchServer(false) || gc->BHaveLiveMatch() ||
        tfmm::getPendingInvites())
    {
        was_playing = true;
        t = now + 2;
        return;
    }
    /* Don't do anything if searching for game and nobody is banned
     * Try to rejoin party members game first before starting new queue
     */
    if (!tfmm::isMMBanned() && !pc->IsAnybodyBanned(false) &&
        !was_playing && pc->BInQueueForMatchGroup(tfmm::getQueue()))
    {
        t = now + 2;
        return;
    }
    /* These things. They take time.
     * - Gabe Newell */
    if (t - now > 0)
        return;

    t = now + 10;
    id.Clear();
    bool found_leader = false;
    for (int i = 0; i < num_members; ++i)
    {
        pc->GetMemberID(i, id);
        const bool me = id == my_id;
#if ENABLE_IPC
        if (!me && leadership_ipc(id) == 2)
        {
            found_leader = true;
            break;
        }
        if (*auto_leader_ipc)
            continue;
#endif
        /* Promote next user in party */
        if (me)
        {
            pc->GetMemberID(++i % num_members, id);
#if ENABLE_IPC
            /* IPC member but disconnected from server */
            if (leadership_ipc(id) == 1)
                continue;
#endif
            found_leader = true;
            break;
        }
    }
    if (!found_leader)
        return;

    logging::Info("Giving leadership to %u (%d)", id.GetAccountID(), pc->PromotePlayerToLeader(id));
}
CatCommand leader_debug("autoleader_debug", "", leadership_check);

static void register_auto_leader(bool enable)
{
    if (enable)
        EC::Register(EC::Paint, leadership_check, "pass_leadership");
    else
        EC::Unregister(EC::Paint, "pass_leadership");
}

static InitRoutine init([]() {
    auto_leader.installChangeCallback([](bool new_val) {
        register_auto_leader(new_val);
    });
    if (*auto_leader)
        register_auto_leader(true);
});
