/*
  Created by Jenny White on 29.04.18.
  Copyright (c) 2018 nullworks. All rights reserved.
*/

#include <chatlog.hpp>
#include <MiscTemporary.hpp>
#include <settings/Bool.hpp>
#include "HookedMethods.hpp"
#include "CatBot.hpp"
#include "nospread.hpp"
#include "PlayerTools.hpp"

static settings::Boolean clean_chat{ "chat.clean", "false" };
static settings::Boolean clean_chat_empty{ "chat.clean.empty", "false" };
static settings::Boolean clean_global{ "chat.censor.global-chat", "false" };
static settings::Boolean dispatch_log{ "debug.log-dispatch-user-msg", "false" };
static settings::Boolean chat_filter{ "chat.censor.enable", "false" };
static settings::Boolean chat_filter_namechange{ "chat.censor.namechange", "true" };
static settings::String chat_filter_words{ "chat.censor.words", "skid;script;cheat;hak;hac;f1;f2;hax;ban;bot;report;vote;kick;kcik;kik;no;me;not;spam;chat;boat;avatar;picture;pfp" };
static settings::Boolean chat_filter_class{ "chat.censor.class", "true" };
static settings::Boolean chat_filter_friends{ "chat.censor.friends", "false" };
/* Note the whitespace character in set */
static settings::String chat_filter_remove_chars{ "chat.censor.remove_chars", "- .,:" };
static settings::String chat_filter_find_chars{ "chat.censor.find_chars", "1430657" };
static settings::String chat_filter_repl_chars{ "chat.censor.replacement_chars", "iaeogst" };
/* >0 - find this amount of name characters in message
 *  0 - disable
 * <0 - use whole name */
static settings::Int chat_filter_name_length{ "chat.censor.name_part_length", "4" };
#if ENABLE_NULL_GRAPHICS
static settings::Boolean anti_autobalance{ "cat-bot.anti-autobalance", "true" };
#else
static settings::Boolean anti_autobalance{ "cat-bot.anti-autobalance", "false" };
#endif
static settings::Boolean anti_autobalance_safety{ "cat-bot.anti-autobalance.safety", "true" };
/* Will re-join on death if we can without adhock, and the teams are unbalanced, shitty way to prevent being moved after death */
static settings::Boolean anti_autobalance_aggressive{ "cat-bot.anti-autobalance.aggresive", "false" };

extern settings::Boolean abandon_on_mm_fail;

static Timer sendmsg{}, gitgud{};

// Using repeated char causes crash on some systems. Suboptimal solution.
const static char *clear = "\e"
    "\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n"
    "\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n"
    "\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n"
    "\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n"
    "\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n"
    "\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n"
    "\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n";
static char *lastname, *lastfilter;

namespace hooked_methods
{

static char previous_address[64];
static Timer reset_it{};
static Timer wait_timer{};
static void Paint()
{
    if (!wait_timer.test_and_set(1000))
        return;
    INetChannel *server = (INetChannel *) g_IEngine->GetNetChannelInfo();
    if (server)
        reset_it.update();
    if (reset_it.test_and_set(20000))
    {
        anti_balance_attempts = 0;
        previous_address[0]   = 0;
    }
}
static bool HasNamePart(const char *tgt, const char *iname, int count)
{
    char name[32], *p = name, c;

    int len = std::strlen(iname);
    if (!len)
        return false;

    std::memcpy(name, iname, len + 1);
    if (count < 0 || len < count)
        count = len;

    while ((c = p[count]))
    {
        p[count] = 0;
        if (std::strstr(tgt, p))
        {
            logging::Info("Censor reason: message contained part of your name %s", p);
            return true;
        }
        p[count] = c;
        ++p;
    }
    return false;
}
static const char *GetActiveClassName()
{
    switch (g_pLocalPlayer->clazz)
    {
    case tf_scout:
        return "scout";
    case tf_soldier:
        return "soldier";
    case tf_pyro:
        return "pyro";
    case tf_demoman:
        return "demo";
    case tf_engineer:
        return "engi";
    case tf_heavy:
        return "heavy";
    case tf_medic:
        return "med";
    case tf_sniper:
        return "sniper";
    case tf_spy:
        return "spy";
    }
    return nullptr;
}

static void CensorChat(int m_IDX, const char *event, const char *name, const char *message)
{
    if (m_IDX == LOCAL_E->m_IDX)
    {
        if (*chat_filter_namechange && !std::strncmp(event, "#TF_Name_Change", 15))
            chat_stack::Say(clear, false);
        return;
    }
    if (std::strncmp(event, "TF_Chat", 7))
        return;
    if (!chat_filter_friends)
    {
        player_info_s info{};
        if (g_IEngine->GetPlayerInfo(m_IDX, &info))
        {
            if (!player_tools::shouldTargetSteamId(info.friendsID))
                return;
        }
    }

    player_info_s info;
    g_IEngine->GetPlayerInfo(LOCAL_E->m_IDX, &info);

    std::size_t msg_len     = std::strlen(message);
    char *processed_message = new char[msg_len + 1];
    StringToLower(processed_message, message);

    const char *filter_chars = (*chat_filter_remove_chars).c_str(),
        *find_chars = (*chat_filter_find_chars).c_str(),
        *repl_chars = (*chat_filter_repl_chars).c_str();

    RemoveChars(processed_message, filter_chars, msg_len);
    ReplaceChars(processed_message, find_chars, repl_chars);
    logging::Info("Processed message: %s", processed_message);
    bool matches = false;
    if (*chat_filter_class)
    {
        const char *clazz = GetActiveClassName();
        matches           = clazz && std::strstr(processed_message, clazz);
        if (matches)
            logging::Info("Censor reason: message contained name of your active class: %s", clazz);
    }
    if (!matches && (*chat_filter_words).size())
    {
        char *filter_words = new char[(*chat_filter_words).size() + 1];
        StringToLower(filter_words, (*chat_filter_words).c_str());
        RemoveChars(filter_words, filter_chars, (*chat_filter_words).size());
        ReplaceChars(filter_words, find_chars, repl_chars);

        char *tok_tmp, *cur_tok = strtok_r(filter_words, ";", &tok_tmp);
        for (;!matches && cur_tok; cur_tok = strtok_r(nullptr, ";", &tok_tmp))
        {
            matches = std::strstr(processed_message, cur_tok);
            if (matches)
                logging::Info("Censor reason: message contained censored word: %s", cur_tok);
        }
        delete[] filter_words;
    }
    if (!matches && *chat_filter_name_length)
    {
        StringToLower(info.name, info.name);
        RemoveChars(info.name, filter_chars);
        ReplaceChars(info.name, find_chars, repl_chars);
        matches = HasNamePart(processed_message, info.name, *chat_filter_name_length);
    }
    if (matches)
    {
        chat_stack::Say(clear, !*clean_global);
#if ENABLE_VISUALS
        if (lastname)
        {
            delete[] lastname;
            delete[] lastfilter;
        }
        lastfilter = CStringDuplicate(message);
        lastname   = CStringDuplicate(name);
        gitgud.update();
#endif
    }
    delete[] processed_message;
}

class PlayerDeathListener : public IGameEventListener2
{
public:
    void FireGameEvent(IGameEvent *event) override
    {
        if (!anti_autobalance_aggressive)
            return;

        // We died :(
        if (g_IEngine->GetPlayerForUserID(event->GetInt("userid")) == g_IEngine->GetLocalPlayer())
        {
            int team_players       = 0;
            int enemy_team_players = 0;

            for (int i = 1; i < g_GlobalVars->maxClients; ++i)
            {
                player_info_s info{};
                if (!g_IEngine->GetPlayerInfo(i, &info) || !info.friendsID)
                    continue;
                if (g_pPlayerResource->GetTeam(i) == g_pLocalPlayer->team)
                    team_players++;
                else
                    enemy_team_players++;
            }

            // Re-join, we can be randomly moved by gc
            if (team_players > enemy_team_players && (!anti_autobalance_safety || anti_balance_attempts < 2))
            {
                ignoredc = true;
                logging::Info("Rejoin: anti autobalance");
                g_IEngine->ClientCmd_Unrestricted("killserver;wait 10;cat_mm_join");
            }
        }
    }
};

static PlayerDeathListener death_listener;

static InitRoutine Autobalance([]() {
  EC::Register(EC::Paint, Paint, "paint_autobalance", EC::average);
  g_IEventManager2->AddListener(&death_listener, "player_death", false);
});

DEFINE_HOOKED_METHOD(DispatchUserMessage, bool, void *this_, int type, bf_read &buf)
{
    int s, i, j;
    char c, *p;

    if (!isHackActive())
        return original::DispatchUserMessage(this_, type, buf);

    const char *buf_data = reinterpret_cast<const char *>(buf.m_pData);
    s = buf.GetNumBytesLeft();
    /* Delayed print of name and message, censored by chat_filter
     * TO DO: Document type 47
     */
#if ENABLE_VISUALS
    if (lastname && type != 47 && gitgud.test_and_set(300))
    {
        PrintChat("\x07%06X%s\x01: %s", 0xe05938, lastname, lastfilter);
        delete[] lastname;
        delete[] lastfilter;
        lastname = lastfilter = nullptr;
    }
#endif
    // We should bail out
    if (!hacks::tf2::nospread::DispatchUserMessage(&buf, type))
        return true;
    switch (type)
    {
    case 12:
        if (hacks::shared::catbot::anti_motd && hacks::shared::catbot::catbotmode)
        {
            if (std::strstr(buf_data, "class_"))
                return false;
        }
        break;
    case 5:
    {
        /* [21:03:10] MESSAGE 5, DATA = [ 03 23 54 46 5F 43 6F 6D 70 65 74 69 74 69 76 65 5F 47 61 6D 65 4F 76 65 72 00 2D 36 32 35 32 30 00 00 00 00 ] strings listed below
         * [21:03:10] #TF_Competitive_GameOver
         * [21:03:10] -62520
         */
        if (abandon_on_mm_fail && std::strstr(buf_data, "#TF_Competitive_GameOver"))
        {
            logging::InfoConsole("MM Fail: GC is likely dead, received \"safe to leave\" message");
            g_IEngine->ClientCmd_Unrestricted("killserver");
            tfmm::abandon();
            break;
        }

        if (!anti_autobalance || s <= 35 || CE_BAD(LOCAL_E))
            break;

        INetChannel *server = (INetChannel *) g_IEngine->GetNetChannelInfo();
        if (!server)
            break;

        if (!std::strstr(buf_data, "TeamChangeP"))
            break;

        auto current_address = server->GetAddress();
        if (std::strncmp(previous_address, current_address, sizeof(previous_address)))
        {
            std::strncpy(previous_address, current_address, sizeof(previous_address));
            anti_balance_attempts = 0;
        }
        if (!anti_autobalance_safety || anti_balance_attempts < 2)
        {
            ignoredc = true;
            logging::Info("Rejoin: anti autobalance");
            g_IEngine->ClientCmd_Unrestricted("killserver;wait 10;cat_mm_join");
        }
        else
            re::CTFPartyClient::GTFPartyClient()->SendPartyChat("Will be autobalanced in 3 seconds");

        anti_balance_attempts++;
        break;
    }
    case 4:
        if (s >= 256 || CE_BAD(LOCAL_E))
            break;

        /* First byte is player ENT index
         * Second byte is unindentified (equals to 0x01)
         */
        const char *event = buf_data + 2, *name = event + std::strlen(event) + 1,
            *message = name + std::strlen(name) + 1;

        if (*clean_chat)
        {
            std::size_t prev_len = std::strlen(message),
                len = RemoveChars(const_cast<char *>(message), "\r\n\e", prev_len);

            buf.m_nDataBytes -= prev_len - len;
            buf.m_nDataBits  -= (prev_len - len) << 3;
            /* Empty message, ignore if configured to do so */
            if (!len && *clean_chat_empty)
                return true;
        }
        if (*chat_filter)
            CensorChat(buf_data[0], event, name, message);

        chatlog::LogMessage(buf_data[0], message);
        break;
    }
    if (dispatch_log)
    {
        char *m = new char[4096];
        if (s >= 1024)
            s = 1024;

        for (i = 0; i < s; ++i)
            std::snprintf(&m[i * 3], 4, "%02X ", int(buf_data[i]) & 0xFF);

        logging::Info("MESSAGE %d, DATA = [ %s] strings listed below", type, m);
        j = 0;
        for (i = 0; i < s; ++i)
        {
            if (!(m[j++] = buf_data[i]))
            {
                logging::Info("%s", m);
                j = 0;
            }
        }
        delete[] m;
    }
    votelogger::dispatchUserMessage(buf, type);
    return original::DispatchUserMessage(this_, type, buf);
}
} // namespace hooked_methods
