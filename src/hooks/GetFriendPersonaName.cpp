#include <settings/String.hpp>
#include "HookedMethods.hpp"
#include "PlayerTools.hpp"
#include <MiscTemporary.hpp>

/* Name with format characters can be longer than 32 */
#define NAME_LENGTH 192
#define Debug(...)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            \
    if (*namesteal_debug)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     \
    logging::Info("GetFriendPersonaName.cpp: " __VA_ARGS__)

static settings::String ipc_name{ "name.ipc", "" };
static settings::String force_name{ "name.custom", "" };
static settings::Boolean random_name{ "name.random-name", "false" };
static settings::Int namesteal{ "name.namesteal", "0" };
static settings::Boolean namesteal_debug{ "name.namesteal.debug", "false" };
static settings::Boolean namesteal_friendly{ "name.namesteal.friendly", "true" };
static settings::Boolean namesteal_preserve{ "name.namesteal.preserve", "true" };
static settings::Boolean namesteal_preserve_aggressive{ "name.namesteal.preserve.aggressive", "false" };
static settings::Int namesteal_wait{ "name.namesteal.wait", "0" };
static settings::String namesteal_format{ "name.namesteal.format", "\\u202C" };
static settings::Boolean namesteal_format_random_pos{ "name.namesteal.format.random-pos", "false" };
static settings::Boolean f2p_priority{ "name.namesteal.f2p-priority", "false" };

static char forced_name[32], stolen_name[32];
static void FormatStolenName(char *out_name, const char *name)
{
    char tmp[NAME_LENGTH];
    std::string formatted_name;
    if (namesteal_format_random_pos)
    {
        int pos = UniformRandomInt(1, strlen(name));

        formatted_name = std::string(name);
        formatted_name = formatted_name.substr(0, pos) + *namesteal_format + formatted_name.substr(pos, formatted_name.length());
    }
    else
        formatted_name = std::string(name) + *namesteal_format;
    std::strncpy(tmp, formatted_name.c_str(), sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = 0;
    ReplaceSpecials(tmp);

    Debug("namesteal result name length: %d", std::strlen(tmp));
    std::strncpy(out_name, tmp, 31);
    out_name[31] = 0;
}

// Either sets stolen name or returns false to indicate that we hadn't
// stole any name previously
static bool SetStolenName(char *name)
{
    if (*namesteal_preserve && stolen_name[0])
    {
        FormatStolenName(name, stolen_name);
        return true;
    }
    return false;
}

static std::vector<std::pair<std::string, int>> potential_targets;
static bool stolen_name_found = false;
static size_t fmt_len;

static bool GetTargetsLobby(CTFLobbyShared *lobby)
{
    CSteamID our_steamID = g_ISteamUser->GetSteamID();

    CTFLobbyPlayer *player;
    CTFLobbyPlayer *local_player = lobby->GetPlayer(lobby->GetMemberIndexBySteamID(our_steamID));

    if (!local_player)
    {
        Debug("namesteal we are waiting for local player");
        return false;
    }

    // Go through non-pending players looking for potential targets
    int members = lobby->GetNumMembers();
    for (int i = 0; i < members; ++i)
    {
        player = lobby->GetPlayer(i);

        // Check if member is a good
        if (!player || player == local_player)
            continue;

        // If set to steal teammates names only, don't even try to steal enemy name
        if (*namesteal_friendly && player->GetTeam() != local_player->GetTeam())
            continue;

        CSteamID steamID = player->GetID();
        int steamID32    = steamID.GetAccountID();
        const char *name = g_ISteamFriends->GetFriendPersonaName(steamID);

        // Too early!
        if (!std::strcmp(name, "[unknown]") || name[0] == '\0')
        {
            Debug("namesteal lobby member index %i (%u) too early!", i, steamID32);
            g_ISteamFriends->RequestUserInformation(steamID, true);
            continue;
        }

        // Don't steal CATs or other friendly states
        auto &pl = playerlist::AccessData(steamID32);
        if (pl.state != playerlist::EState::DEFAULT && pl.state != playerlist::EState::RAGE)
            continue;

        // Check if we already stole this name
        if (!std::strcmp(stolen_name, name))
        {
            stolen_name_found = true;
            continue;
        }

        // Check formatted name won't exceed 31 bytes
        if (std::strlen(name) + fmt_len > 31)
            continue;

        Debug("namesteal candidate: %s (%lu)", name, std::strlen(name));

        // Note: length check was performed above
        potential_targets.emplace_back(name, steamID32);
    }

    return true;
}

static void GetTargetsEntities()
{
    // Go through entities looking for potential targets
    for (int i = 1; i <= g_IEngine->GetMaxClients(); i++)
    {
        auto ent = ENTITY(i);
        // Check if ent is a good target
        if (CE_INVALID(ent) || ent == LOCAL_E || ent->m_Type() != ENTITY_PLAYER)
            continue;

        player_info_s info{};
        if (!g_IEngine->GetPlayerInfo(ent->m_IDX, &info))
            continue;

        // If set to steal teammates names only, don't even try to steal enemy name
        if (*namesteal_friendly && ent->m_bEnemy())
            continue;

        // Don't steal CATs or other friendly states
        auto &pl = playerlist::AccessData(info.friendsID);
        if (pl.state != playerlist::EState::DEFAULT && pl.state != playerlist::EState::RAGE)
            continue;

        // Check if we already stole this name
        if (!std::strcmp(stolen_name, info.name))
        {
            stolen_name_found = true;
            continue;
        }

        // Check formated name won't exceed 31 bytes
        if (std::strlen(info.name) + fmt_len > 31)
            continue;

        Debug("namesteal candidate: %s (%lu)", info.name, std::strlen(info.name));

        // Note: length check was performed above
        potential_targets.emplace_back(info.name, info.friendsID);
    }
}

static Timer name_wait_timer{};
// Func to get a new user to steal name from and returns true if a target has been found
bool StealName(char *name)
{
    potential_targets.clear();
    stolen_name_found = false;

    fmt_len = namesteal_format.toString().length();
    if (!fmt_len)
    {
        logging::Info("Can't steal with empty name.namesteal.format string!");
        return SetStolenName(name);
    }

    if (*namesteal_wait > 0 && g_IEngine->IsInGame() && !name_wait_timer.check(*namesteal_wait * 1000))
    {
        Debug("namesteal we are waiting");
        return SetStolenName(name);
    }

    Debug("namesteal format length %d", fmt_len);

    auto lobby = CTFLobbyShared::GetLobby();
    if (!lobby)
    {
        // Community server, can't use lobby object
        if (g_IEngine->IsInGame())
            GetTargetsEntities();
        else
            return SetStolenName(name);
    }
    else if (!GetTargetsLobby(lobby))
        return SetStolenName(name);

#if ENABLE_IPC
    // Additional check for IPC bots to avoid collisions
    if (ipc::peer)
    {
        // Loop all peers
        for (unsigned i = 0; i < cat_ipc::max_peers; i++)
        {
            if (i != ipc::peer->client_id && !ipc::peer->memory->peer_data[i].free && ipc::peer->memory->peer_user_data[i].namesteal_target)
            {
                // The peer has a valid namesteal target, check if we have the same target in our vector, if so, erase it.
                for (size_t j = 0; j < potential_targets.size(); j++)
                {
                    if (potential_targets[j].second == ipc::peer->memory->peer_user_data[i].namesteal_target)
                    {
                        Debug("namesteal removing %s [U:1:%u] due to ipc collision avoidance", potential_targets[j].first.c_str(), potential_targets[j].second);
                        potential_targets.erase(potential_targets.begin() + j);
                    }
                }
            }
        }
    }
#endif
    // No targets, return previously stolen name
    if (potential_targets.empty())
    {
        Debug("namesteal ended up empty");
        return SetStolenName(name);
    }

    // If passive namesteal is enabled, check if stolen name is still being used
    if (*namesteal == 1 && stolen_name_found)
    {
        Debug("namesteal passive success going with %s (%lu)", stolen_name, std::strlen(stolen_name));
        FormatStolenName(name, stolen_name);
        return true;
    }

    std::vector<std::pair<std::string, int>> potential_priority_targets;
    for (const auto& target : potential_targets)
    {
        auto &pl = playerlist::AccessData(target.second);
        if (pl.state == playerlist::EState::RAGE || pl.state == playerlist::EState::ANTIBOT || (f2p_priority && g_ISteamFriends->GetFriendSteamLevel(CSteamIDFrom32(target.second)) == 0))
            potential_priority_targets.emplace_back(target);
    }

    int idx;
    if (!potential_priority_targets.empty())
        idx = UniformRandomInt(0, potential_priority_targets.size() - 1);
    else
        idx = UniformRandomInt(0, potential_targets.size() - 1);

    target_name         = potential_targets[idx].first;
    namesteal_target_id = potential_targets[idx].second;

#if ENABLE_IPC
    if (ipc::peer && ipc::peer->memory)
        ipc::peer->memory->peer_user_data[ipc::peer->client_id].namesteal_target = potential_targets[idx].second;
#endif

    std::strcpy(stolen_name, target_name.c_str());
    Debug("namesteal ended with %s (%lu)", target_name.c_str(), target_name.size());
    FormatStolenName(name, stolen_name);

    if (namesteal_preserve_aggressive && !stolen_name_found && anti_balance_attempts < 2 && g_IEngine->IsInGame())
    {
        if (!std::strcmp(forced_name, LOCAL_E->player_info.name))
        {
            ignoredc = true;
            anti_balance_attempts++;
            logging::Info("Rejoin: preserve aggressive");
            g_IEngine->ClientCmd_Unrestricted("killserver; wait 10; cat_mm_join");
        }
    }
    return true;
}

/* Obtain currently forced name
 * Returns true on success and writes name into "name" (32 chars max)
 * Returns false otherwise and nothing is written into "name"
 */
bool GetForcedName(char *name, CSteamID steam_id)
{
    char tmp[NAME_LENGTH];

    if (steam_id != g_ISteamUser->GetSteamID())
        return false;

    if (random_name)
    {
        if (need_name_change)
        {
            static TextFile file;
            if (file.TryLoad("names.txt"))
            {
                std::string rand_name = file.lines.at(rand() % file.lines.size());
                std::strncpy(tmp, rand_name.c_str(), sizeof(tmp) - 1);
                tmp[sizeof(tmp) - 1] = 0;

                ReplaceSpecials(tmp);

                std::strncpy(name, tmp, 31);
                name[31] = 0;

                need_name_change = false;
            }
        }
        return true;
    }

    // Apply user custom name if set
    if ((*force_name).size() > 1)
    {
        std::strncpy(tmp, (*force_name).c_str(), sizeof(tmp) - 1);
        tmp[sizeof(tmp) - 1] = 0;

        ReplaceSpecials(tmp);

        std::strncpy(name, tmp, 31);
        name[31] = 0;

        return true;
    }

#if ENABLE_IPC
    // Check this to allow individual bots to be configured with
    // either custom name or name steal
    if (ipc::peer && (*ipc_name).length() > 3)
    {
        char num[16];

        std::snprintf(num, sizeof(num), "%d", ipc::peer->client_id);
        std::strncpy(tmp, (*ipc_name).c_str(), sizeof(tmp) - 1);
        tmp[sizeof(tmp) - 1] = 0;

        ReplaceSpecials(tmp);
        ReplaceString(tmp, sizeof(tmp), "%%", num);

        std::strncpy(name, tmp, 31);
        name[31] = 0;

        return true;
    }
#endif

    auto lobby = CTFLobbyShared::GetLobby();
    if (!lobby && !g_IEngine->IsInGame())
        return false;

    if (*namesteal && StealName(name))
        return true;

    return false;
}

namespace hooked_methods
{
DEFINE_HOOKED_METHOD(GetFriendPersonaName, const char *, ISteamFriends *this_, CSteamID steam_id)
{
    if (GetForcedName(forced_name, steam_id))
        return forced_name;

    return original::GetFriendPersonaName(this_, steam_id);
}

static InitRoutine init([]() {
    namesteal.installChangeCallback([](int new_val) {
        if (!new_val || !GetForcedName(forced_name, g_ISteamUser->GetSteamID()))
            return;

        Debug("namesteal TriggerNameChange");
        TriggerNameChange(forced_name);
    });
    force_name.installChangeCallback([](const std::string &new_val) {
        if (new_val.length() < 1 || !GetForcedName(forced_name, g_ISteamUser->GetSteamID()))
            return;

        Debug("force_name TriggerNameChange");
        TriggerNameChange(forced_name);
    });
});

static Timer set_name{};
static void Paint()
{
    auto lobby = CTFLobbyShared::GetLobby();

    if (random_name)
    {
        if (!GetForcedName(forced_name, g_ISteamUser->GetSteamID()))
            return;

        Debug("random_name TriggerNameChange");
        TriggerNameChange(forced_name);
        return;
    }

    if (!namesteal || !lobby || g_IEngine->IsInGame())
    {
#if ENABLE_IPC
        if (ipc::peer && ipc::peer->memory)
            ipc::peer->memory->peer_user_data[ipc::peer->client_id].namesteal_state = 0;
#endif
        return;
    }

#if ENABLE_IPC
    if (ipc::peer && ipc::peer->memory)
        ipc::peer->memory->peer_user_data[ipc::peer->client_id].namesteal_state = 1;
#endif

    CTFLobbyPlayer *player;
    int members = lobby->GetNumMembers();
    for (int i = 0; i < members; ++i)
    {
        player = lobby->GetPlayer(i);
        if (!player)
            continue;
        g_ISteamFriends->RequestUserInformation(player->GetID(), true);
    }

    // We don't want to spam set name
    if (set_name.test_and_set(1000))
        GetForcedName(forced_name, g_ISteamUser->GetSteamID());
}

static Timer set_name_cm{};
static void CreateMove()
{
    if (!namesteal || !g_IEngine->IsInGame())
        return;
#if ENABLE_IPC
    if (ipc::peer && ipc::peer->memory)
        ipc::peer->memory->peer_user_data[ipc::peer->client_id].namesteal_state = 2;
#endif
    /* TO DO: Read server variable (on custom servers it is 30 seconds) */
    if (!set_name_cm.test_and_set(4000))
        return;
    if (!GetForcedName(forced_name, g_ISteamUser->GetSteamID()))
        return;
    // Didn't change name - update timer a bit
    if (!std::strcmp(forced_name, LOCAL_E->player_info.name))
    {
        set_name_cm.update();
        return;
    }
    Debug("CM TriggerNameChange");
    TriggerNameChange(forced_name);
}

static InitRoutine runinit([]() {
    EC::Register(EC::CreateMove, CreateMove, "cm_namesteal", EC::late);
    EC::Register(EC::Paint, Paint, "paint_namesteal");
});

} // namespace hooked_methods
