/* Created on 16 Feb. 2021 by aUniqueUser */
#include "common.hpp"
#include "hack.hpp"
#include "MiscTemporary.hpp"
#include "PlayerTools.hpp"
#include <unordered_set>

namespace hacks::tf2::votekicks
{

static settings::Boolean enabled{ "votekicks.enabled", "false" };
/* 0 - Smart, 1 - Random, 2 - Sequential */
static settings::Int mode{ "votekicks.mode", "0" };
/* Should we only vote to maintain majority ? */
static settings::Boolean maintain_majority_only{ "votekicks.maintain-majority-only", "false" };
/* Time between calling a vote in milliseconds */
static settings::Int timer{ "votekicks.timer", "1000" };
/* Minimum amount of team members to start a vote */
static settings::Int min_team_size{ "votekicks.min-team-size", "4" };
/* Only kick rage or antibot playerlist states */
static settings::Boolean rage_only{ "votekicks.rage-only", "false" };
/* Only vote F2P players */
static settings::Boolean f2p_only{ "votekicks.f2p-only", "false" };

/* Priority settings */
static settings::Boolean prioritize_rage{ "votekicks.prioritize.rage", "true" };
static settings::Boolean prioritize_namesteal{ "votekicks.prioritize.namesteal", "true" };
static settings::Boolean prioritize_previously_kicked{ "votekicks.prioritize.previous", "true" };
/* If highest score target >= this rvar (& gt 0), make them highest priority to kick */
static settings::Int prioritize_highest_score{ "votekicks.prioritize.highest-score", "0" };

std::unordered_set<uint32> previously_kicked;

static int GetKickScore(int uid)
{
    player_info_s i{};
    int idx = g_IEngine->GetPlayerForUserID(uid);
    if (!g_IEngine->GetPlayerInfo(idx, &i))
        return 0;

    uid = 0;
    if (prioritize_namesteal && namesteal_target_id == i.friendsID)
        uid += 500;
    if (prioritize_previously_kicked && previously_kicked.find(i.friendsID) != previously_kicked.end())
        uid += 500;
    if (prioritize_rage)
    {
        auto &pl = playerlist::AccessData(i.friendsID);
        if (pl.state == playerlist::EState::RAGE || pl.state == playerlist::EState::ANTIBOT)
            uid += 1000;
    }

    uid += g_pPlayerResource->GetScore(idx);
    return uid;
}

static bool NeedToVote()
{
    player_info_s local_info{};
    int friends = 0;
    int foes    = 0;

    /* https://www.youtube.com/watch?v=TIEO20NVIBM */
    if (CE_BAD(LOCAL_E) || !g_IEngine->GetPlayerInfo(LOCAL_E->m_IDX, &local_info))
        return false;
    for (int i = 1; i < g_GlobalVars->maxClients; ++i)
    {
        player_info_s info{};
        if (!g_IEngine->GetPlayerInfo(i, &info) || !info.friendsID)
            continue;
        if (g_pPlayerResource->GetTeam(i) != g_pLocalPlayer->team)
            continue;
        if (!player_tools::shouldTargetSteamId(info.friendsID))
            friends++;
        else
            foes++;
    }
    return foes >= friends;
}

static void CreateMove()
{
    static Timer votekicks_timer;
    if (!votekicks_timer.test_and_set(*timer))
        return;
    if (maintain_majority_only && !NeedToVote())
        return;

    player_info_s local_info{};
    std::vector<int> targets;
    std::vector<int> scores;
    int teamSize = 0;

    if (CE_BAD(LOCAL_E) || !g_IEngine->GetPlayerInfo(LOCAL_E->m_IDX, &local_info))
        return;
    for (int i = 1; i < g_GlobalVars->maxClients; ++i)
    {
        player_info_s info{};
        if (!g_IEngine->GetPlayerInfo(i, &info) || !info.friendsID)
            continue;
        if (g_pPlayerResource->GetTeam(i) != g_pLocalPlayer->team)
            continue;
        teamSize++;

        if (info.friendsID == local_info.friendsID)
            continue;
        if (!player_tools::shouldTargetSteamId(info.friendsID))
            continue;
        auto &pl = playerlist::AccessData(info.friendsID);
        if (rage_only && (pl.state != playerlist::EState::RAGE && pl.state != playerlist::EState::ANTIBOT))
            continue;
        if (f2p_only && (!g_ISteamFriends->RequestUserInformation(CSteamIDFrom32(info.friendsID), true) && g_ISteamFriends->GetFriendSteamLevel(CSteamIDFrom32(info.friendsID)) > 0))
            continue;

        targets.push_back(info.userID);
        scores.push_back(g_pPlayerResource->GetScore(g_IEngine->GetPlayerForUserID(info.userID)));
    }
    if (targets.empty() || scores.empty() || teamSize <= *min_team_size)
        return;

    int target;
    auto score_iterator = std::max_element(scores.begin(), scores.end());

    switch (*mode)
    {
    case 0:
        // Smart mode - Sort by kick score
        std::sort(targets.begin(), targets.end(), [](int a, int b) { return GetKickScore(a) > GetKickScore(b); });
        target = (*prioritize_highest_score && *score_iterator >= *prioritize_highest_score) ? targets[std::distance(scores.begin(), score_iterator)] : targets[0];
        break;
    case 1:
        // Random
        target = targets[UniformRandomInt(0, targets.size() - 1)];
        break;
    case 2:
        // Sequential
        target = targets[0];
        break;
    }

    player_info_s info{};
    if (!g_IEngine->GetPlayerInfo(g_IEngine->GetPlayerForUserID(target), &info))
        return;
    hack::ExecuteCommand("callvote kick \"" + std::to_string(target) + " cheating\"");
}

static CatCommand debugKickScore("debug_kickscore", "Prints kick score for each player", []() {
    player_info_s info{};
    if (!g_IEngine->IsInGame())
        return;
    for (int i = 1; i < g_GlobalVars->maxClients; ++i)
    {
        if (!g_IEngine->GetPlayerInfo(i, &info) || !info.friendsID)
            continue;
        logging::InfoConsole("%d %u %s: %d", i, info.friendsID, info.name, GetKickScore(info.userID));
    }
});

static void register_votekicks(bool enable)
{
    if (enable)
        EC::Register(EC::CreateMove, CreateMove, "cm_votekicks");
    else
        EC::Unregister(EC::CreateMove, "cm_votekicks");
}

static InitRoutine init([]() {
    enabled.installChangeCallback([](bool new_val) { register_votekicks(new_val); });
    if (*enabled)
        register_votekicks(true);
});

} // namespace hacks::tf2::votekicks