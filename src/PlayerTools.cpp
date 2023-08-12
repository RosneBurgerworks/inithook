/*
  Created on 23.06.18.
*/

#include "common.hpp"
#include <unordered_map>
#include <playerlist.hpp>
#include "PlayerTools.hpp"
#include "entitycache.hpp"
#include "settings/Bool.hpp"
#include "Aimbot.hpp"

static settings::Int betrayal_limit{ "player-tools.betrayal-limit", "2" };

static settings::Boolean taunting{ "player-tools.ignore.taunting", "false" };
static settings::Boolean ignoreCathook{ "player-tools.ignore.cathook", "true" };
static settings::Boolean attackIPC{"player-tools.attack-ipc", "false"};

static std::unordered_map<unsigned, unsigned> betrayal_list{};

namespace player_tools
{

bool shouldTargetSteamId(unsigned id)
{
    if (betrayal_limit)
    {
        if (betrayal_list[id] >= *betrayal_limit)
            return true;
    }

    auto &pl = playerlist::AccessData(id);
    if (pl.state == playerlist::EState::IPC && attackIPC)
        return true;
    if (playerlist::IsFriendly(pl.state) || (pl.state == playerlist::EState::CAT && *ignoreCathook))
        return false;

    return true;
}
bool shouldTarget(CachedEntity *entity)
{
    if (entity->m_Type() == ENTITY_PLAYER)
    {
        if (taunting && HasCondition<TFCond_Taunting>(entity) && CE_INT(entity, netvar.m_iTauntIndex) == 3)
            return false;
        if (HasCondition<TFCond_HalloweenGhostMode>(entity))
            return false;
        if (hacks::shared::aimbot::ignore_cloak && IsPlayerInvisible(entity))
            return false;
        return shouldTargetSteamId(entity->player_info.friendsID);
    }

    return true;
}

bool shouldAlwaysRenderEspSteamId(unsigned id)
{
    if (id == 0)
        return false;

    auto &pl = playerlist::AccessData(id);
    if (pl.state != playerlist::EState::DEFAULT && pl.state != playerlist::EState::RAGE)
        return true;

    return false;
}
bool shouldAlwaysRenderEsp(CachedEntity *entity)
{
    if (entity->m_Type() == ENTITY_PLAYER)
    {
        return shouldAlwaysRenderEspSteamId(entity->player_info.friendsID);
    }

    return false;
}

#if ENABLE_VISUALS
std::optional<colors::rgba_t> forceEspColorSteamId(unsigned id)
{
    if (id == 0)
        return std::nullopt;

    auto pl = playerlist::Color(id);
    if (pl != colors::empty)
        return std::optional<colors::rgba_t>{ pl };

    return std::nullopt;
}
std::optional<colors::rgba_t> forceEspColor(CachedEntity *entity)
{
    if (entity->m_Type() == ENTITY_PLAYER)
    {
        return forceEspColorSteamId(entity->player_info.friendsID);
    }

    return std::nullopt;
}
#endif

void loadBetrayList()
{
    std::ifstream betray_list_r(DATA_PATH "/betray_list.txt", std::ios::in);

    try
    {
        std::string line;
        while (std::getline(betray_list_r, line))
        {
            size_t spacePos = line.find(' ');
            if (spacePos == std::string::npos)
                continue;
            unsigned steamid = std::stoi(line.substr(0, spacePos));
            if (betrayal_list.find(steamid) == betrayal_list.end() || betrayal_list[steamid] < *betrayal_limit)
                betrayal_list[steamid] = *betrayal_limit;
        }
    }
    catch (std::exception &e)
    {
        logging::InfoConsole("Reading betray list unsuccessful: %s", e.what());
    }
    logging::InfoConsole("Reading betray list successful!");
}

bool appendBetrayToFile(unsigned steamID, const char *username)
{
    std::ifstream betray_list_r(DATA_PATH "/betray_list.txt", std::ios::in);
    std::string steamID_str = std::to_string(steamID);

    // Check to make sure this ID is not already saved by another bot (shouldn't happen)
    try
    {
        std::string line;
        while (std::getline(betray_list_r, line))
        {
            if (line.find(steamID_str) != std::string::npos)
                return false;
        }

        betray_list_r.close();
    }
    catch (std::exception &e)
    {
        logging::InfoConsole("Reading betray list for appending [U:1:%u] (%s) unsuccessful: %s", steamID, username, e.what());
    }

    std::ofstream betray_list_a(DATA_PATH "/betray_list.txt", std::ios::app);
    try
    {
        betray_list_a << steamID_str << " " << username << "\n";
        betray_list_a.close();
    }
    catch (std::exception &e)
    {
        logging::InfoConsole("Appending of [U:1:%u] (%s) to betray list unsuccessful: %s", steamID, username, e.what());
    }
    logging::InfoConsole("Appending of [U:1:%u] (%s) to betray list successful!", steamID, username);
    return true;
}

class PlayerDeathListener : public IGameEventListener2
{
public:
    void FireGameEvent(IGameEvent *event) override
    {
        player_info_s killer_info{};
        int attacker_id = g_IEngine->GetPlayerForUserID(event->GetInt("attacker"));
        int victim_id   = g_IEngine->GetPlayerForUserID(event->GetInt("userid"));
        if (!g_IEngine->GetPlayerInfo(attacker_id, &killer_info))
            return;

        unsigned id = killer_info.friendsID;
        auto &pl    = playerlist::AccessData(id);

        if (victim_id == g_IEngine->GetLocalPlayer() && pl.state == playerlist::EState::CAT && pl.state != playerlist::EState::IPC && pl.can_betray)
        {
            if (betrayal_list.find(id) == betrayal_list.end())
                betrayal_list[id] = 1;
            else
                betrayal_list[id]++;

            logging::InfoConsole("Incremented betray on [U:1:%u] (%s) (%d TOTAL)", id, killer_info.name, betrayal_list[id]);

            if (id && betrayal_list[id] == *betrayal_limit)
            {
                logging::InfoConsole("New betray: [U:1:%u] (%s)", id, killer_info.name);

                // Try to add this traitor to the betray list file
                if (appendBetrayToFile(id, killer_info.name))
                {
                    // Make IPC peers refresh their betray list
                    if (ipc::peer && ipc::peer->connected)
                    {
                        const std::string command = "cat_pt_load_betraylist";
                        if (command.length() >= 63)
                            ipc::peer->SendMessage(nullptr, -1, ipc::commands::execute_client_cmd_long, command.c_str(), command.length() + 1);
                        else
                            ipc::peer->SendMessage(command.c_str(), -1, ipc::commands::execute_client_cmd, nullptr, 0);
                    }
                }
            }
        }
    }
};

static CatCommand load_betraylist("pt_load_betraylist", "Load betrayal list", []() { loadBetrayList(); });
static CatCommand clear_betraylist("pt_betraylist_forgive_all", "Forgive all betrayal list entries", []() { betrayal_list.clear(); });

static CatCommand print_betraylist("pt_print_betraylist", "Print betrayal list", []() {
    for (auto entry : betrayal_list)
        logging::InfoConsole("[U:1:%u]", entry.first);
});

static CatCommand add_betraylist_entry("pt_add_betraylist_entry", "Add entry to betrayal list", [](const CCommand &args) {
    if (args.ArgC() < 2)
    {
        logging::InfoConsole("Invalid input.");
        return;
    }

    betrayal_list[std::stoi(args.Arg(1))] = std::stoi(args.Arg(2));
});

static CatCommand clear_betraylist_entry("pt_betraylist_forgive", "Forgive a entry on the betrayal list", [](const CCommand &args) {
    if (args.ArgC() < 1)
    {
        logging::InfoConsole("Invalid input.");
        return;
    }

    betrayal_list[std::stoi(args.Arg(1))] = 0;
});

static PlayerDeathListener betray_listener;
static InitRoutine init_betray([]() { g_IEventManager2->AddListener(&betray_listener, "player_death", false); });

} // namespace player_tools
