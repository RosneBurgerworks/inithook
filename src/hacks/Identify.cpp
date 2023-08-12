#include <common.hpp>

#define INVALID_VALUE 0xFFFF
/* Send identification requests */
static settings::Boolean send{ "identify.send", "false" };
/* Recognize other identification requests */
static settings::Boolean recognize{ "identify.recognize", "true" };
/* Reply to other identifications requests. Works with identify.send == false (passive mode) */
static settings::Boolean recognize_reply{ "identify.recognize.reply", "true" };
/* Useful for the funny lobby exploit */
static settings::Boolean rageIdentifiers{ "identify.mark-rage", "false" };
/* Force compatibility with upstream identify */
static settings::Boolean authUpstream{ "identify.upstream-compatibility", "true" };
/* Do not send auth when a player under ANTIBOT pl state is in the lobby */
static settings::Boolean anti_bot_avoidance{ "identify.antibot-avoidance", "true" };
/* should we log all drawline events to InfoConsole() ?*/
static settings::Boolean logDrawlines{ "identify.log-drawlines", "false" };
/* Delay between identification requests */
static settings::Int delay{ "identify.time", "5" };
/* 0xCA7 is standard Cathook identification value */
static settings::Float value{ "identify.value", "3239" };
/* 0xCA8 is standard Cathook identification reply */
static settings::Float valueReply{ "identify.reply_value", "3240" };
/* 34685 is private identify authorization  */
static settings::Float authMessage{ "identify.auth", "34685" };

constexpr float authMessageUpstream = 1234567.0f;

static Timer identify_timer{};

static void NotifyDrawline(float x_value, float y_value)
{
    if (anti_bot_avoidance)
    {
        auto lobby = CTFLobbyShared::GetLobby();
        if (!lobby)
            return;

        CTFLobbyPlayer *player;
        int members = lobby->GetNumMembers();
        for (int i = 0; i < members; ++i)
        {
            player = lobby->GetPlayer(i);
            if (!player)
                continue;
            CSteamID id = player->GetID();
            int id32    = id.GetAccountID();
            if (playerlist::AccessData(id32).state == playerlist::EState::ANTIBOT)
            {
                logging::InfoConsole("Refusing to send cl_drawline identification because [U:1:%u] -> %s [Anti-Bot] is in lobby.", id32, g_ISteamFriends->GetFriendPersonaName(id));
                return;
            }
        }
    }

    auto *kv = new KeyValues("cl_drawline");
    kv->SetInt("panel", 2);
    kv->SetInt("line", 0);
    kv->SetFloat("x", x_value);
    kv->SetFloat("y", y_value);
    g_IEngine->ServerCmdKeyValues(kv);
}

static bool should_reply = false;

void ProcessDrawLine(IGameEvent *kv)
{
    if (anti_bot_avoidance)
    {
        auto lobby = CTFLobbyShared::GetLobby();
        if (!lobby)
            return;

        CTFLobbyPlayer *player;
        int members = lobby->GetNumMembers();
        for (int i = 0; i < members; ++i)
        {
            player = lobby->GetPlayer(i);
            if (!player)
                continue;
            CSteamID id = player->GetID();
            int id32    = id.GetAccountID();
            if (playerlist::AccessData(id32).state == playerlist::EState::ANTIBOT)
                return;
        }
    }

    int player_idx;
    player_info_s info{};
    /* Player index 0 is invalid in all cases */
    player_idx = kv->GetInt("player", 0);
    if (!player_idx || !g_IEngine->GetPlayerInfo(player_idx, &info))
        return;

    float id           = kv->GetFloat("x");
    float message_type = kv->GetFloat("y");
    int panel_type     = kv->GetInt("panel");
    int line_type      = kv->GetInt("line");
    if (panel_type == 2 && line_type == 0 && (message_type == *authMessage || (authUpstream && message_type == authMessageUpstream)) && (id == *value || id == *valueReply))
    {
        using namespace playerlist;
        auto &state = AccessData(info.friendsID).state;
        if (playerlist::ChangeState(info.friendsID, rageIdentifiers ? playerlist::EState::RAGE : playerlist::EState::CAT))
        {
            logging::InfoConsole("Player [U:1:%u] %s identified by cl_drawline (%s)", info.friendsID, info.name, message_type == authMessageUpstream ? "Upstream" : "Private");
            // Don't count betray on private auth
            if (message_type == *authMessage)
                playerlist::AccessData(info.friendsID).can_betray = false;
#if ENABLE_VISUALS
            PrintChat("\x07%06X%s\x01 [U:1:%u] Marked as %s (cl_drawline auth)", 0xe05938, info.name, info.friendsID, rageIdentifiers ? "RAGE" : "CAT");
#endif
        }
        if (*recognize_reply && id == *value)
            should_reply = true;
    }
    if (logDrawlines)
        logging::InfoConsole("Player [U:1:%u] %s sent cl_drawline (panel_type %i, line_type %i, message_type %f, id %f)", info.friendsID, info.name, panel_type, line_type, message_type, id);
}

class DrawLineListener : public IGameEventListener2
{
    virtual void FireGameEvent(IGameEvent *event)
    {
        ProcessDrawLine(event);
    }
};
static DrawLineListener event_listener{};

static CatCommand send_identify("debug_identify", "Broadcast identification request", []() { NotifyDrawline(*value, *authMessage); });

static void cm()
{
    if (!identify_timer.test_and_set(*delay * 1000))
        return;
    if ((*send || should_reply) && !rageIdentifiers /* Never send drawline if we are using this feature */)
    {
        NotifyDrawline(*send && !should_reply ? *value : *valueReply, *authMessage);
        if (authUpstream && *authMessage != authMessageUpstream)
            NotifyDrawline(*send && !should_reply ? *value : *valueReply, authMessageUpstream);
        should_reply = false;
    }
}

static InitRoutine run_identify([]() {
    EC::Register(EC::CreateMove, cm, "identify_cm");
    g_IEventManager2->AddListener(&event_listener, "cl_drawline", false);

    EC::Register(EC::Shutdown, []() { g_IEventManager2->RemoveListener(&event_listener); }, "identify_shutdown");
});
