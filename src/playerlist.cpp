/*
 * playerlist.cpp
 *
 *  Created on: Apr 11, 2017
 *      Author: nullifiedcat
 */

#include "playerlist.hpp"
#include "common.hpp"
#include <cstdint>
#include <dirent.h>
#include <iterator>

namespace playerlist
{

std::unordered_map<uint32, userdata> data{};

const std::string names[]                                     = { "DEFAULT", "FRIEND", "RAGE", "IPC", "CAT", "PARTY", "ANTIBOT" };
const char *const pszNames[]                                  = { "DEFAULT", "FRIEND", "RAGE", "IPC", "CAT", "PARTY", "ANTIBOT" };
const std::array<std::pair<EState, size_t>, 5> arrGUIStates    = { std::pair(EState::DEFAULT, 0), { EState::FRIEND, 1 }, { EState::RAGE, 2 }, { EState::ANTIBOT, 3 } };

#if ENABLE_VISUALS
rgba_t Colors[] = { colors::empty, colors::FromRGBA8(99, 226, 161, 255), colors::FromRGBA8(226, 204, 99, 255), colors::FromRGBA8(232, 134, 6, 255), colors::empty, colors::FromRGBA8(99, 226, 161, 255) };
#endif

void addID(const char *id3, const char *state)
{
    uint32_t id = std::strtoul(id3, nullptr, 10);
    for (int i = 0; i <= int(EState::STATE_LAST); ++i)
        if (names[i] == state)
        {
            AccessData(id).state = EState(i);
            return;
        }

    logging::InfoConsole("Unknown State");
}

static void Cleanup()
{
    userdata empty{};
    size_t counter = 0;
    for (auto it = data.begin(); it != data.end(); ++it)
    {
        if (std::memcmp(&it->second, &empty, sizeof(empty)))
            continue;

        ++counter;
        data.erase(it);
        /* Start all over again. Iterator is invalidated once erased. */
        it = data.begin();
    }
    logging::InfoConsole("%lu elements were removed", counter);
}

std::vector<std::string> SplitWhiteSpace(std::string const &input)
{
    std::istringstream buffer(input);
    std::vector<std::string> ret((std::istream_iterator<std::string>(buffer)), std::istream_iterator<std::string>());
    return ret;
}

void Load()
{
    DIR *cathook_directory = opendir(DATA_PATH);
    if (!cathook_directory)
        return;
    else
        closedir(cathook_directory);
    try
    {
        std::ifstream pl(DATA_PATH "/plist.txt");
        std::string line;
        data.clear();

        while (std::getline(pl, line))
        {
            // Skip invalid line
            if (!isdigit(line[0]))
                continue;

            std::vector<std::string> split = SplitWhiteSpace(line);
            addID(split[0].c_str(), split[1].c_str());
        }
        pl.close();
    }
    catch (std::exception &e)
    {
        logging::InfoConsole("Reading playerlist unsuccessful: %s", e.what());
    }
    logging::InfoConsole("Reading playerlist successful!");
}

#if ENABLE_VISUALS
rgba_t Color(uint32 steamid)
{
    const auto &pl = AccessData(steamid);
    if (pl.state == EState::CAT)
        return colors::RainbowCurrent();
    if (pl.state == EState::ANTIBOT)
        return colors::yellow;
    else if (pl.color.a)
        return pl.color;

    int state = static_cast<int>(pl.state);
    if (state < sizeof(Colors) / sizeof(*Colors))
        return Colors[state];

    return colors::empty;
}

rgba_t Color(CachedEntity *player)
{
    if (CE_GOOD(player))
        return Color(player->player_info.friendsID);
    return colors::empty;
}
#endif
userdata &AccessData(uint32 steamid)
{
    return data[steamid];
}

// Assume player is non-null
userdata &AccessData(CachedEntity *player)
{
    if (player && player->player_info.friendsID)
        return AccessData(player->player_info.friendsID);
    return AccessData(0U);
}

bool IsDefault(uint32 steamid)
{
    const userdata &data = AccessData(steamid);
#if ENABLE_VISUALS
    return data.state == EState::DEFAULT && !data.color.a;
#endif
    return data.state == EState ::DEFAULT;
}

bool IsDefault(CachedEntity *entity)
{
    if (entity && entity->player_info.friendsID)
        return IsDefault(entity->player_info.friendsID);
    return true;
}

bool IsFriend(uint32 steamid)
{
    const userdata &data = AccessData(steamid);
    return data.state == EState::PARTY || data.state == EState::FRIEND;
}

bool IsFriend(CachedEntity *entity)
{
    if (entity && entity->player_info.friendsID)
        return IsFriend(entity->player_info.friendsID);
    return false;
}

bool ChangeState(uint32 steamid, EState state, bool force)
{
    auto &data = AccessData(steamid);
    if (force)
    {
        data.state = state;
        return true;
    }
    switch (data.state)
    {
    case EState::FRIEND:
        return false;
    case EState::PARTY:
        if (state == EState::FRIEND)
        {
            ChangeState(steamid, state, true);
            return true;
        }
        else
            return false;
    case EState::IPC:
        if (state == EState::FRIEND || state == EState::PARTY)
        {
            ChangeState(steamid, state, true);
            return true;
        }
        else
            return false;
    case EState::CAT:
        if (state == EState::FRIEND || state == EState::IPC || state == EState::PARTY)
        {
            ChangeState(steamid, state, true);
            return true;
        }
        else
            return false;
    case EState::DEFAULT:
        if (state != EState::DEFAULT)
        {
            ChangeState(steamid, state, true);
            return true;
        }
        else
            return false;
    case EState::RAGE:
    case EState::ANTIBOT:
        return false;
    }
    return true;
}

bool ChangeState(CachedEntity *entity, EState state, bool force)
{
    if (entity && entity->player_info.friendsID)
        return ChangeState(entity->player_info.friendsID, state, force);
    return false;
}

CatCommand pl_load("pl_load", "Load playerlist", [](const CCommand &args) { Load(); });
CatCommand pl_print("pl_print", "Print current player list", [](const CCommand &args) {
    userdata empty{};
    bool include_all = args.ArgC() >= 2 && *args.Arg(1) == '1';

    logging::InfoConsole("Known entries: %lu", data.size());
    for (auto &it : data)
    {
        if (!include_all && !std::memcmp(&it.second, &empty, sizeof(empty)))
            continue;

        const auto &ent = it.second;
#if ENABLE_VISUALS
        logging::InfoConsole("%u -> %d (%f,%f,%f,%f) %f %u %u", it.first, ent.state, ent.color.r, ent.color.g, ent.color.b, ent.color.a);
#else
      logging::Info("%u -> %d %f %u %u", it.first, ent.state);
#endif
    }
});

CatCommand pl_add_id("pl_add_id", "Sets state for steamid on local plist", [](const CCommand &args) {
    if (args.ArgC() <= 2)
        return;

    addID(args.Arg(1), args.Arg(2));
});

CatCommand pl_clean("pl_clean", "Removes empty entries to reduce RAM usage", Cleanup);

CatCommand pl_set_state("pl_set_state", "cat_pl_set_state [playername] [state] (Tab to autocomplete)", [](const CCommand &args) {
    if (args.ArgC() != 3)
    {
        logging::InfoConsole("Invalid call");
        return;
    }
    auto name = args.Arg(1);
    int id    = -1;
    for (int i = 1; i <= g_IEngine->GetMaxClients(); i++)
    {
        player_info_s info;
        if (!g_IEngine->GetPlayerInfo(i, &info))
            continue;
        std::string currname(info.name);
        std::replace(currname.begin(), currname.end(), ' ', '-');
        std::replace_if(currname.begin(), currname.end(), [](char x) { return !isprint(x); }, '*');
        if (currname.find(name) != 0)
            continue;
        id = i;
        break;
    }
    if (id == -1)
    {
        logging::InfoConsole("Unknown Player Name. (Use tab for autocomplete)");
        return;
    }
    std::string state = args.Arg(2);
    StringToUpper(state.data(), state.data());
    player_info_s info{};
    g_IEngine->GetPlayerInfo(id, &info);

    for (int i = 0; i <= int(EState::STATE_LAST); ++i)
        if (names[i] == state)
        {
            AccessData(info.friendsID).state = EState(i);
            return;
        }

    logging::InfoConsole("Unknown State %s. (Use tab for autocomplete)", state.c_str());
});

static int cat_pl_set_state_completionCallback(const char *c_partial, char commands[COMMAND_COMPLETION_MAXITEMS][COMMAND_COMPLETION_ITEM_LENGTH])
{
    std::string partial = c_partial;
    std::string parts[2]{};
    auto j    = 0u;
    auto f    = false;
    int count = 0;

    for (auto i = 0u; i < partial.size() && j < 3; ++i)
    {
        auto space = (bool) isspace(partial[i]);
        if (!space)
        {
            if (j)
                parts[j - 1].push_back(partial[i]);
            f = true;
        }

        if (i == partial.size() - 1 || (f && space))
        {
            if (space)
                ++j;
            f = false;
        }
    }

    std::vector<std::string> names;

    for (int i = 1; i <= g_IEngine->GetMaxClients(); i++)
    {
        player_info_s info{};
        if (!g_IEngine->GetPlayerInfo(i, &info))
            continue;
        std::string name(info.name);
        std::replace(name.begin(), name.end(), ' ', '-');
        std::replace_if(
            name.begin(), name.end(), [](char x) { return !isprint(x); }, '*');
        names.push_back(name);
    }
    std::sort(names.begin(), names.end());

    if (parts[0].empty() || parts[1].empty() && (!parts[0].empty() && partial.back() != ' '))
    {
        StringToLower(parts[0].data(), parts[0].data());
        for (const auto &s : names)
        {
            std::string c(s);
            StringToLower(c.data(), c.data());
            if (c.find(parts[0]) == 0)
            {
                snprintf(commands[count++], COMMAND_COMPLETION_ITEM_LENGTH - 1, "cat_pl_set_state %s", s.c_str());
            }
        }
        return count;
    }
    StringToLower(parts[1].data(), parts[1].data());
    for (const auto &s : names)
    {
        std::string c(s);
        StringToLower(c.data(), c.data());
        if (c.find(parts[1]) == 0)
        {
            snprintf(commands[count++], COMMAND_COMPLETION_ITEM_LENGTH - 1, "cat_pl_set_state %s %s", parts[0].c_str(), s.c_str());
            if (count == COMMAND_COMPLETION_MAXITEMS)
                break;
        }
    }
    return count;
}

#if ENABLE_VISUALS
CatCommand pl_set_color("pl_set_color", "pl_set_color uniqueid r g b", [](const CCommand &args) {
    if (args.ArgC() < 5)
    {
        logging::InfoConsole("Invalid call");
        return;
    }
    uint32 steamid          = strtoul(args.Arg(1), nullptr, 10);
    int r                     = strtol(args.Arg(2), nullptr, 10);
    int g                     = strtol(args.Arg(3), nullptr, 10);
    int b                     = strtol(args.Arg(4), nullptr, 10);
    rgba_t color              = colors::FromRGBA8(r, g, b, 255);
    AccessData(steamid).color = color;
    logging::InfoConsole("Changed %d's color", steamid);
});
#endif

CatCommand pl_info("pl_info", "pl_info uniqueid", [](const CCommand &args) {
    if (args.ArgC() < 2)
    {
        logging::InfoConsole("Invalid call");
        return;
    }
    uint32 steamid;
    try
    {
        steamid = strtoul(args.Arg(1), nullptr, 10);
    }
    catch (const std::invalid_argument &)
    {
        return;
    }
    auto &pl = AccessData(steamid);
    const char *str_state;
    if (pl.state < EState::STATE_FIRST || pl.state > EState::STATE_LAST)
        str_state = "UNKNOWN";
    else
        str_state = pszNames[int(pl.state)];

    logging::InfoConsole("Data for %i: ", steamid);
    logging::InfoConsole("State: %i %s", pl.state, str_state);
});

static InitRoutine init([]() {
    pl_set_state.cmd->m_bHasCompletionCallback = true;
    pl_set_state.cmd->m_fnCompletionCallback   = cat_pl_set_state_completionCallback;
});
} // namespace playerlist
