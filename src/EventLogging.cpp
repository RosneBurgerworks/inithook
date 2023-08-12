#include "common.hpp"
#include "helpers.hpp"

static settings::Boolean event_hurt{ "log.hurt", "false" };
static settings::Boolean event_connect{ "log.joining", "false" };
static settings::Boolean event_activate{ "log.connect", "false" };
static settings::Boolean event_disconnect{ "log.disconnect", "false" };
static settings::Boolean event_team{ "log.team", "false" };
static settings::Boolean event_death{ "log.death", "false" };
static settings::Boolean event_spawn{ "log.spawn", "false" };
static settings::Boolean event_changeclass{ "log.changeclass", "false" };

class PlayerConnectClientListener : public IGameEventListener2
{
public:
    void FireGameEvent(IGameEvent *event) override
    {
        if (!event_connect)
            return;

        player_info_s info{};
        int entity = g_IEngine->GetPlayerForUserID(event->GetInt("userid"));
        if (g_IEngine->GetPlayerInfo(entity, &info))
        {
            logging::InfoConsole("%s [U:1:%u] is joining", info.name, info.friendsID);
#if ENABLE_VISUALS
            PrintChat("\x07%06X%s [U:1:%u]\x01 is joining", 0xa06ba0, info.name, info.friendsID, 0x914e65);
#endif
        }
    }
};

class PlayerActivateListener : public IGameEventListener2
{
public:
    void FireGameEvent(IGameEvent *event) override
    {
        if (!event_activate)
            return;

        player_info_s info{};
        int entity = g_IEngine->GetPlayerForUserID(event->GetInt("userid"));
        if (g_IEngine->GetPlayerInfo(entity, &info))
        {
            logging::InfoConsole("%s [U:1:%u] connected", info.name, info.friendsID);
#if ENABLE_VISUALS
            PrintChat("\x07%06X%s [U:1:%u]\x01 connected", 0xa06ba0, info.name, info.friendsID);
#endif
        }
    }
};

class PlayerDisconnectListener : public IGameEventListener2
{
public:
    void FireGameEvent(IGameEvent *event) override
    {
        if (!event_disconnect)
            return;

        player_info_s info{};
        int entity = g_IEngine->GetPlayerForUserID(event->GetInt("userid"));
        if (g_IEngine->GetPlayerInfo(entity, &info))
        {
            logging::InfoConsole("%s [U:1:%u] is disconnected", info.name, info.friendsID);
#if ENABLE_VISUALS
            PrintChat("\x07%06X%s [U:1:%u]\x01 is disconnected", 0xa06ba0, info.name, info.friendsID, 0x914e65);
#endif
        }
    }
};

class PlayerTeamListener : public IGameEventListener2
{
public:
    void FireGameEvent(IGameEvent *event) override
    {
        if (!event_team || event->GetBool("disconnect"))
            return;

        player_info_s info{};
        int entity = g_IEngine->GetPlayerForUserID(event->GetInt("userid"));
        if (g_IEngine->GetPlayerInfo(entity, &info))
        {
            int oteam           = event->GetInt("oldteam");
            int nteam           = event->GetInt("team");
            const char *oteam_s = teamname(oteam);
            const char *nteam_s = teamname(nteam);

            logging::InfoConsole("%s [U:1:%u] changed team (%s -> %s)", info.name, info.friendsID, oteam_s, nteam_s);
#if ENABLE_VISUALS
            PrintChat("\x07%06X%s [U:1:%u]\x01 changed team (\x07%06X%s\x01 -> \x07%06X%s\x01)", 0xa06ba0, info.name, info.friendsID, colors::chat::team(oteam), oteam_s, colors::chat::team(nteam), nteam_s);
#endif
        }
    }
};

class PlayerHurtListener : public IGameEventListener2
{
public:
    void FireGameEvent(IGameEvent *event) override
    {
        if (!event_hurt)
            return;

        player_info_s kinfo{}, vinfo{};

        int health = event->GetInt("health");

        int attacker_ent = g_IEngine->GetPlayerForUserID(event->GetInt("attacker"));
        int victim_ent   = g_IEngine->GetPlayerForUserID(event->GetInt("userid"));
        if (!g_IEngine->GetPlayerInfo(victim_ent, &vinfo) || !g_IEngine->GetPlayerInfo(attacker_ent, &kinfo))
        {
            return;
        }
        logging::InfoConsole("%s [U:1:%u] hurt %s [U:1:%u] down to %d hp", kinfo.name, kinfo.friendsID, vinfo.name, vinfo.friendsID, health);
#if ENABLE_VISUALS
        CachedEntity *vic = ENTITY(g_IEngine->GetPlayerForUserID(victim_ent));
        CachedEntity *att = ENTITY(g_IEngine->GetPlayerForUserID(attacker_ent));

        if (vic == nullptr || att == nullptr || RAW_ENT(vic) == nullptr || RAW_ENT(att) == nullptr)
            return;

        PrintChat("\x07%06X%s [U:1:%u]\x01 hurt \x07%06X%s [U:1:%u]\x01 down to \x07%06X%d\x01hp", colors::chat::team(att->m_iTeam()), kinfo.name, kinfo.friendsID, colors::chat::team(vic->m_iTeam()), vinfo.name, vinfo.friendsID, 0x2aaf18, health);
#endif
    }
};

class PlayerDeathListener : public IGameEventListener2
{
public:
    void FireGameEvent(IGameEvent *event) override
    {
        if (!event_death)
            return;

        player_info_s kinfo{}, vinfo{};
        int attacker_ent = g_IEngine->GetPlayerForUserID(event->GetInt("attacker"));
        int victim_ent   = g_IEngine->GetPlayerForUserID(event->GetInt("userid"));
        if (!g_IEngine->GetPlayerInfo(victim_ent, &vinfo) || !g_IEngine->GetPlayerInfo(attacker_ent, &kinfo))
        {
            return;
        }
        logging::InfoConsole("%s [U:1:%u] killed %s [U:1:%u]", kinfo.name, kinfo.friendsID, vinfo.name, vinfo.friendsID);
#if ENABLE_VISUALS
        CachedEntity *vic = ENTITY(g_IEngine->GetPlayerForUserID(victim_ent));
        CachedEntity *att = ENTITY(g_IEngine->GetPlayerForUserID(attacker_ent));

        if (vic == nullptr || att == nullptr || RAW_ENT(vic) == nullptr || RAW_ENT(att) == nullptr)
            return;

        PrintChat("\x07%06X%s [U:1:%u]\x01 killed \x07%06X%s [U:1:%u]\x01", colors::chat::team(att->m_iTeam()), kinfo.name, kinfo.friendsID, colors::chat::team(vic->m_iTeam()), vinfo.name, vinfo.friendsID);
#endif
    }
};

class PlayerSpawnListener : public IGameEventListener2
{
public:
    void FireGameEvent(IGameEvent *event) override
    {
        if (!event_spawn)
            return;

        player_info_s info{};
        int id = event->GetInt("userid");
        if (!g_IEngine->GetPlayerInfo(g_IEngine->GetPlayerForUserID(id), &info))
            return;

        logging::InfoConsole("%s [U:1:%u] (re)spawned", info.name, info.friendsID);
#if ENABLE_VISUALS
        CachedEntity *player = ENTITY(g_IEngine->GetPlayerForUserID(id));
        if (player == nullptr || RAW_ENT(player) == nullptr)
            return;

        PrintChat("\x07%06X%s [U:1:%u]\x01 (re)spawned", colors::chat::team(player->m_iTeam()), info.name, info.friendsID);
#endif
    }
};

class PlayerChangeClassListener : public IGameEventListener2
{
public:
    void FireGameEvent(IGameEvent *event) override
    {
        if (!event_changeclass)
            return;

        player_info_s info{};
        int id = event->GetInt("userid");
        if (!g_IEngine->GetPlayerInfo(g_IEngine->GetPlayerForUserID(id), &info))
            return;

        logging::InfoConsole("%s [U:1:%u] changed to %s", info.name, info.friendsID, classname(event->GetInt("class")));
#if ENABLE_VISUALS
        CachedEntity *player = ENTITY(g_IEngine->GetPlayerForUserID(id));
        if (player == nullptr || RAW_ENT(player) == nullptr)
            return;

        PrintChat("\x07%06X%s [U:1:%u]\x01 changed to \x07%06X%s\x01", colors::chat::team(player->m_iTeam()), info.name, info.friendsID, 0xa06ba0, classname(event->GetInt("class")));
#endif
    }
};

static PlayerConnectClientListener playerConnectClientListener;
static PlayerActivateListener playerActivateListener;
static PlayerDisconnectListener playerDisconnectListener;
static PlayerTeamListener playerTeamListener;
static PlayerHurtListener playerHurtListener;
static PlayerDeathListener playerDeathListener;
static PlayerSpawnListener playerSpawnListener;
static PlayerChangeClassListener playerChangeClassListener;

static InitRoutine init([]() {
    /* And another thing... You're ugly. */
    g_IEventManager2->AddListener(&playerConnectClientListener, "player_connect_client", false);
    g_IEventManager2->AddListener(&playerActivateListener, "player_activate", false);
    g_IEventManager2->AddListener(&playerDisconnectListener, "player_disconnect", false);
    g_IEventManager2->AddListener(&playerTeamListener, "player_team", false);
    g_IEventManager2->AddListener(&playerHurtListener, "player_hurt", false);
    g_IEventManager2->AddListener(&playerDeathListener, "player_death", false);
    g_IEventManager2->AddListener(&playerSpawnListener, "player_spawn", false);
    g_IEventManager2->AddListener(&playerChangeClassListener, "player_changeclass", false);
});
