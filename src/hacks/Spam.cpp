/*
 * Spam.cpp
 *
 *  Created on: Jan 21, 2017
 *      Author: nullifiedcat
 */

#include <hacks/Spam.hpp>
#include <settings/Bool.hpp>
#include "common.hpp"
#include "MiscTemporary.hpp"

static settings::Int spam_source{ "spam.source", "0" };
static settings::Boolean random_order{ "spam.random", "0" };
static settings::String filename{ "spam.filename", "spam.txt" };
static settings::Int spam_delay{ "spam.delay", "800" };
static settings::Int voicecommand_spam{ "spam.voicecommand", "0" };
static settings::Boolean team_only{ "spam.teamchat", "false" };

static settings::Boolean ipc_coordination{ "spam.ipc-coordination", "false"};
static settings::Boolean dynamic_delay{ "spam.ipc-coordination.dynamic-delay", "true" };

namespace hacks::shared::spam
{

static int last_index;

std::chrono::time_point<std::chrono::system_clock> last_spam_point{};

static size_t current_index{0};
static size_t current_ipc_vec_index{0};
TextFile file{};

const std::string teams[] = { "RED", "BLU" };

// FUCK enum class.
// It doesn't have bitwise operators by default!! WTF!! static_cast<int>(REEE)!

enum class QueryFlags
{
    ZERO        = 0,
    TEAMMATES   = (1 << 0),
    ENEMIES     = (1 << 1),
    STATIC      = (1 << 2),
    ALIVE       = (1 << 3),
    DEAD        = (1 << 4),
    LOCALPLAYER = (1 << 5)
};

enum class QueryFlagsClass
{
    SCOUT    = (1 << 0),
    SNIPER   = (1 << 1),
    SOLDIER  = (1 << 2),
    DEMOMAN  = (1 << 3),
    MEDIC    = (1 << 4),
    HEAVY    = (1 << 5),
    PYRO     = (1 << 6),
    SPY      = (1 << 7),
    ENGINEER = (1 << 8)
};

struct Query
{
    int flags{ 0 };
    int flags_class{ 0 };
};

static int current_static_index{ 0 };
static Query static_query{};

bool PlayerPassesQuery(Query query, int idx)
{
    player_info_s pinfo;
    if (idx == g_IEngine->GetLocalPlayer())
    {
        if (!(query.flags & static_cast<int>(QueryFlags::LOCALPLAYER)))
            return false;
    }
    if (!g_IEngine->GetPlayerInfo(idx, &pinfo))
        return false;
    CachedEntity *player = ENTITY(idx);
    if (!RAW_ENT(player))
        return false;
    int teammate = !player->m_bEnemy();
    bool alive   = !CE_BYTE(player, netvar.iLifeState);
    int clazzBit = (1 << CE_INT(player, netvar.iClass) - 1);
    if (static_cast<int>(query.flags_class))
    {
        if (!(clazzBit & static_cast<int>(query.flags_class)))
            return false;
    }
    if (query.flags & (static_cast<int>(QueryFlags::TEAMMATES) | static_cast<int>(QueryFlags::ENEMIES)))
    {
        if (!teammate && !(query.flags & static_cast<int>(QueryFlags::ENEMIES)))
            return false;
        if (teammate && !(query.flags & static_cast<int>(QueryFlags::TEAMMATES)))
            return false;
    }
    if (query.flags & (static_cast<int>(QueryFlags::ALIVE) | static_cast<int>(QueryFlags::DEAD)))
    {
        if (!alive && !(query.flags & static_cast<int>(QueryFlags::DEAD)))
            return false;
        if (alive && !(query.flags & static_cast<int>(QueryFlags::ALIVE)))
            return false;
    }
    return true;
}

Query QueryFromSubstring(const std::string &string)
{
    Query result{};
    bool read = true;
    for (auto it = string.begin(); read && *it; it++)
    {
        if (*it == '%')
            read = false;
        if (read)
        {
            switch (*it)
            {
            case 's':
                result.flags |= static_cast<int>(QueryFlags::STATIC);
                break;
            case 'a':
                result.flags |= static_cast<int>(QueryFlags::ALIVE);
                break;
            case 'd':
                result.flags |= static_cast<int>(QueryFlags::DEAD);
                break;
            case 't':
                result.flags |= static_cast<int>(QueryFlags::TEAMMATES);
                break;
            case 'e':
                result.flags |= static_cast<int>(QueryFlags::ENEMIES);
                break;
            case 'l':
                result.flags |= static_cast<int>(QueryFlags::LOCALPLAYER);
                break;
            case '1':
                result.flags_class |= static_cast<int>(QueryFlagsClass::SCOUT);
                break;
            case '2':
                result.flags_class |= static_cast<int>(QueryFlagsClass::SOLDIER);
                break;
            case '3':
                result.flags_class |= static_cast<int>(QueryFlagsClass::PYRO);
                break;
            case '4':
                result.flags_class |= static_cast<int>(QueryFlagsClass::DEMOMAN);
                break;
            case '5':
                result.flags_class |= static_cast<int>(QueryFlagsClass::HEAVY);
                break;
            case '6':
                result.flags_class |= static_cast<int>(QueryFlagsClass::ENGINEER);
                break;
            case '7':
                result.flags_class |= static_cast<int>(QueryFlagsClass::MEDIC);
                break;
            case '8':
                result.flags_class |= static_cast<int>(QueryFlagsClass::SNIPER);
                break;
            case '9':
                result.flags_class |= static_cast<int>(QueryFlagsClass::SPY);
                break;
            }
        }
    }
    return result;
}

int QueryPlayer(Query query)
{
    if (query.flags & static_cast<int>(QueryFlags::STATIC))
    {
        if (current_static_index && (query.flags & static_query.flags) == static_query.flags && (query.flags_class & static_query.flags_class) == static_query.flags_class)
        {
            if (PlayerPassesQuery(query, current_static_index))
            {
                return current_static_index;
            }
        }
    }
    std::vector<int> candidates{};
    int index_result = 0;
    for (int i = 1; i <= g_IEngine->GetMaxClients(); i++)
    {
        if (PlayerPassesQuery(query, i))
        {
            candidates.push_back(i);
        }
    }
    if (candidates.size())
    {
        index_result = candidates.at(rand() % candidates.size());
    }
    if (query.flags & static_cast<int>(QueryFlags::STATIC))
    {
        current_static_index     = index_result;
        static_query.flags       = query.flags;
        static_query.flags_class = query.flags_class;
    }
    return index_result;
}

bool SubstituteQueries(std::string &input)
{
    size_t index = input.find("%query:");
    while (index != std::string::npos)
    {
        std::string sub = input.substr(index + 7);
        size_t closing  = sub.find("%");
        Query q         = QueryFromSubstring(sub);
        int p           = QueryPlayer(q);
        if (!p)
            return false;
        player_info_s pinfo;
        if (!g_IEngine->GetPlayerInfo(p, &pinfo))
            return false;
        std::string name = std::string(pinfo.name);
        input.replace(index, 8 + closing, name);
        index = input.find("%query:", index + name.size());
    }
    return true;
}

void SubstituteNamesteal(std::string &input)
{
    size_t index = input.find("%namesteal_target%");
    if (index != std::string::npos)
        ReplaceString(input, "%namesteal_target%", target_name);
}

void SubstituteLines(std::string &input)
{
    size_t index = input.find("%lines%");
    if (index != std::string::npos)
    {
        int len      = input.length() - 7;
        int leftover = 127 - len;

        std::string replace;
        while (replace.length() < leftover)
            replace += "\n";

        ReplaceString(input, "%lines%", replace);
    }
}

bool FormatSpamMessage(std::string &message)
{
    ReplaceSpecials(message);
    bool team       = g_pLocalPlayer->team - 2;
    bool enemy_team = !team;
    IF_GAME(IsTF2())
    {
        ReplaceString(message, "%myteam%", teams[team]);
        ReplaceString(message, "%enemyteam%", teams[enemy_team]);
    }
    if (!SubstituteQueries(message))
        return false;
    SubstituteNamesteal(message);
    SubstituteLines(message); /* Last so we always add perfect amount of newlines */
    return true;
}

CatCommand say_lines("say_lines", "Say with newlines (\\n)", [](const CCommand &args) {
    std::string message = "%lines%" + std::string(args.ArgS());
    SubstituteLines(message);
    chat_stack::Say(message);
});

CatCommand say_format("say_format", "Say with formatting", [](const CCommand &args) {
    std::string message = std::string(args.ArgS());
    FormatSpamMessage(message);
    chat_stack::Say(message);
});

#if ENABLE_IPC

static bool isLeader(re::CTFPartyClient *pc)
{
    int num_members = pc->GetNumMembers();
    if (num_members <= 1)
        return true;

    CSteamID id;
    pc->GetCurrentPartyLeader(id);

    return id == g_ISteamUser->GetSteamID();
}

static std::vector<int> ipc_party_vec;
static void RefreshIPCVector()
{
    if (!ipc::peer)
        return;
    auto pc = re::CTFPartyClient::GTFPartyClient();
    if (!pc)
        return;
    if (!isLeader(pc))
        return;

    ipc_party_vec.clear();
    auto m = ipc::peer->memory;
    for (auto member_id : pc->GetPartySteamIDs())
    {
        for (unsigned i = 0; i < cat_ipc::max_peers; ++i)
        {
            /* Not a peer */
            if (m->peer_data[i].free)
                continue;
            /* Not the correct member */
            if (m->peer_user_data[i].friendid != member_id)
                continue;
            ipc_party_vec.emplace_back(i);
            break;
        }
    }
}

static int getDelayIPC()
{
    if (!dynamic_delay || ipc_party_vec.empty())
        return *spam_delay;
    /* 8 seconds is enough to spam for a long time without ratelimit */
    return std::floor(8000 / ipc_party_vec.size());
}

#endif

void createMove()
{
    IF_GAME(IsTF2())
    {
        if (voicecommand_spam)
        {
            static Timer last_voice_spam;
            if (last_voice_spam.test_and_set(4000))
            {
                switch (*voicecommand_spam)
                {
                case 1: // RANDOM
                    g_IEngine->ServerCmd(format("voicemenu ", UniformRandomInt(0, 2), " ", UniformRandomInt(0, 8)).c_str());
                    break;
                case 2: // MEDIC
                    g_IEngine->ServerCmd("voicemenu 0 0");
                    break;
                case 3: // THANKS
                    g_IEngine->ServerCmd("voicemenu 0 1");
                    break;
                case 4: // Go Go Go!
                    g_IEngine->ServerCmd("voicemenu 0 2");
                    break;
                case 5: // Move up!
                    g_IEngine->ServerCmd("voicemenu 0 3");
                    break;
                case 6: // Go left!
                    g_IEngine->ServerCmd("voicemenu 0 4");
                    break;
                case 7: // Go right!
                    g_IEngine->ServerCmd("voicemenu 0 5");
                    break;
                case 8: // Yes!
                    g_IEngine->ServerCmd("voicemenu 0 6");
                    break;
                case 9: // No!
                    g_IEngine->ServerCmd("voicemenu 0 7");
                    break;
                case 10: // Incoming!
                    g_IEngine->ServerCmd("voicemenu 1 0");
                    break;
                case 11: // Spy!
                    g_IEngine->ServerCmd("voicemenu 1 1");
                    break;
                case 12: // Sentry Ahead!
                    g_IEngine->ServerCmd("voicemenu 1 2");
                    break;
                case 13: // Need Teleporter Here!
                    g_IEngine->ServerCmd("voicemenu 1 3");
                    break;
                case 14: // Need Dispenser Here!
                    g_IEngine->ServerCmd("voicemenu 1 4");
                    break;
                case 15: // Need Sentry Here!
                    g_IEngine->ServerCmd("voicemenu 1 5");
                    break;
                case 16: // Activate Charge!
                    g_IEngine->ServerCmd("voicemenu 1 6");
                    break;
                case 17: // Help!
                    g_IEngine->ServerCmd("voicemenu 2 0");
                    break;
                case 18: // Battle Cry!
                    g_IEngine->ServerCmd("voicemenu 2 1");
                    break;
                case 19: // Cheers!
                    g_IEngine->ServerCmd("voicemenu 2 2");
                    break;
                case 20: // Jeers!
                    g_IEngine->ServerCmd("voicemenu 2 3");
                    break;
                case 21: // Positive!
                    g_IEngine->ServerCmd("voicemenu 2 4");
                    break;
                case 22: // Negative!
                    g_IEngine->ServerCmd("voicemenu 2 5");
                    break;
                case 23: // Nice shot!
                    g_IEngine->ServerCmd("voicemenu 2 6");
                    break;
                case 24: // Nice job!
                    g_IEngine->ServerCmd("voicemenu 2 7");
                }
            }
        }
    }

    if (!spam_source)
        return;

    const std::vector<std::string> *source = nullptr;
    switch (*spam_source)
    {
    case 1:
        source = &file.lines;
        break;
    case 2:
        source = &builtin_default;
        break;
    case 3:
        source = &builtin_lennyfaces;
        break;
    case 4:
        source = &builtin_blanks;
        break;
    case 5:
        source = &builtin_nonecore;
        break;
    case 6:
        source = &builtin_lmaobox;
        break;
    case 7:
        source = &builtin_lithium;
        break;
    default:
        return;
    }
    if (!source || !source->size())
        return;
#if ENABLE_IPC
    if (ipc_coordination && ipc::peer)
    {
        auto pc = re::CTFPartyClient::GTFPartyClient();
        if (!pc)
            return;
        /* Don't do anything in empty party */
        if (pc->GetNumMembers() <= 1)
            return;

        /* Make sure we is the leader */
        if (isLeader(pc))
        {
            if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - last_spam_point).count() > int(getDelayIPC()))
            {
                /* We are not ready yet */
                if (ipc_party_vec.empty())
                {
                    logging::InfoConsole("Spam.cpp: ipc coordination vector is empty!");
                    RefreshIPCVector();
                    return;
                }
                /* Reset peer index */
                if (current_ipc_vec_index > ipc_party_vec.size())
                    current_ipc_vec_index = 0;
                /* Reset spam line */
                if (current_index >= source->size())
                    current_index = 0;
                /* Randomization */
                if (random_order && source->size() > 1)
                {
                    current_index = UniformRandomInt(0, source->size() - 1);
                    while (current_index == last_index)
                        current_index = UniformRandomInt(0, source->size() - 1);
                }
                last_index             = current_index;
                std::string spamString = source->at(current_index);
                if (ipc_party_vec[current_ipc_vec_index] == ipc::peer->client_id)
                {
                    if (FormatSpamMessage(spamString))
                        chat_stack::Say(spamString, *team_only);
                }
                else
                {
                    std::string command = "cat_say_format " + spamString;

                    /* Send it to ipc server */
                    if (command.length() >= 63)
                        ipc::peer->SendMessage(0, ipc_party_vec[current_ipc_vec_index], ipc::commands::execute_client_cmd_long, command.c_str(), command.length() + 1);
                    else
                        ipc::peer->SendMessage(command.c_str(), ipc_party_vec[current_ipc_vec_index], ipc::commands::execute_client_cmd, 0, 0);
                }
                current_ipc_vec_index++;
                last_spam_point = std::chrono::system_clock::now();
            }
        }
    }
#endif
    if (!ipc_coordination && std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - last_spam_point).count() > int(spam_delay))
    {
        if (chat_stack::stack.empty())
        {
            if (current_index >= source->size())
                current_index = 0;
            if (random_order && source->size() > 1)
            {
                current_index = UniformRandomInt(0, source->size() - 1);
                while (current_index == last_index)
                {
                    current_index = UniformRandomInt(0, source->size() - 1);
                }
            }
            last_index             = current_index;
            std::string spamString = (*source)[current_index];
            if (FormatSpamMessage(spamString))
                chat_stack::Say(spamString, *team_only);
            current_index++;
        }
        last_spam_point = std::chrono::system_clock::now();
    }
}

void reloadSpamFile()
{
    file.Load(*filename);
}

bool isActive()
{
    return bool(spam_source);
}

void init()
{
    spam_source.installChangeCallback([](int after) { file.Load(*filename); });
    filename.installChangeCallback([](std::string after) { file.TryLoad(after); });
    reloadSpamFile();
}

const std::vector<std::string> builtin_default    = { "Cathook - more fun than a ball of yarn!", "GNU/Linux is the best OS!", "Visit https://github.com/nullworks/cathook for more information!", "Cathook - Free and Open-Source tf2 cheat!", "Cathook - ca(n)t stop me meow!" };
const std::vector<std::string> builtin_lennyfaces = { "( ͡° ͜ʖ ͡°)", "( ͡°( ͡° ͜ʖ( ͡° ͜ʖ ͡°)ʖ ͡°) ͡°)", "ʕ•ᴥ•ʔ", "(▀̿Ĺ̯▀̿ ̿)", "( ͡°╭͜ʖ╮͡° )", "(ง'̀-'́)ง", "(◕‿◕✿)", "༼ つ  ͡° ͜ʖ ͡° ༽つ" };
const std::vector<std::string> builtin_blanks     = { "\e"
    "\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n"
    "\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n"
    "\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n"
    "\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n"
    "\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n"
    "\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n"
    "\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n"
};

const std::vector<std::string> builtin_nonecore = { "NULL CORE - REDUCE YOUR RISK OF BEING OWNED!", "NULL CORE - WAY TO THE TOP!", "NULL CORE - BEST TF2 CHEAT!", "NULL CORE - NOW WITH BLACKJACK AND HOOKERS!", "NULL CORE - BUTTHURT IN 10 SECONDS FLAT!", "NULL CORE - WHOLE SERVER OBSERVING!", "NULL CORE - GET BACK TO PWNING!", "NULL CORE - WHEN PVP IS TOO HARDCORE!", "NULL CORE - CAN CAUSE KIDS TO RAGE!", "NULL CORE - F2P NOOBS WILL BE 100% NERFED!" };
const std::vector<std::string> builtin_lmaobox  = { "GET GOOD, GET LMAOBOX!", "LMAOBOX - WAY TO THE TOP", "WWW.LMAOBOX.NET - BEST FREE TF2 HACK!" };
const std::vector<std::string> builtin_lithium  = { "CHECK OUT www.YouTube.com/c/DurRud FOR MORE INFORMATION!", "PWNING AIMBOTS WITH OP ANTI-AIMS SINCE 2015 - LITHIUMCHEAT", "STOP GETTING MAD AND STABILIZE YOUR MOOD WITH LITHIUMCHEAT!", "SAVE YOUR MONEY AND GET LITHIUMCHEAT! IT IS FREE!", "GOT ROLLED BY LITHIUM? HEY, THAT MEANS IT'S TIME TO GET LITHIUMCHEAT!!" };

#if ENABLE_IPC

class PartyListener : public IGameEventListener2
{
    virtual void FireGameEvent(IGameEvent *event)
    {
        if (ipc_coordination && !strcmp(event->GetName(), "party_updated"))
            RefreshIPCVector();
    }
};

static PartyListener party_listener{};

/* IPC coordination is a expensive task, only run when if we need to */
static void register_ipc_coordination(bool enable)
{
    if (enable)
    {
        EC::Register(EC::LevelInit, RefreshIPCVector, "spam_ipc_levelinit");
        EC::Register(EC::LevelShutdown, RefreshIPCVector, "spam_ipc_levelshutdown");
        EC::Register(EC::Shutdown, RefreshIPCVector, "spam_ipc_shutdown");
        g_IEventManager2->AddListener(&party_listener, "party_updated", false);
    }
    else
    {
        EC::Unregister(EC::LevelInit, "spam_ipc_levelinit");
        EC::Unregister(EC::LevelShutdown, "spam_ipc_levelshutdown");
        EC::Unregister(EC::Shutdown, "spam_ipc_shutdown");
        g_IEventManager2->RemoveListener(&party_listener);
    }
}

#endif

static InitRoutine EC([]() {
    EC::Register(EC::CreateMove, createMove, "spam", EC::average);
#if ENABLE_IPC
    ipc_coordination.installChangeCallback([](bool new_val) { register_ipc_coordination(new_val); });
#endif
    init();
});
} // namespace hacks::shared::spam

static CatCommand reload("spam_reload", "Reload spam file", hacks::shared::spam::reloadSpamFile);
