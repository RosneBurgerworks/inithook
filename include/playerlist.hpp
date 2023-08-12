/*
 * playerlist.hpp
 *
 *  Created on: Apr 11, 2017
 *      Author: nullifiedcat
 */

#pragma once

#include "common.hpp"

namespace playerlist
{

enum class EState
{
    STATE_FIRST = 0,
    DEFAULT     = STATE_FIRST,
    FRIEND,
    RAGE,
    IPC,
    CAT,
    PARTY,
    ANTIBOT,
    STATE_LAST = ANTIBOT
};

#if ENABLE_VISUALS
extern rgba_t Colors[];
static_assert(sizeof(rgba_t) == sizeof(float) * 4, "player list is going to be incompatible with no visual version");
#endif

extern const std::string names[];
extern const char *const pszNames[];
extern const std::array<std::pair<EState, size_t>, 5> arrGUIStates;

struct userdata
{
    EState state{ EState::DEFAULT };
    // For whitelisting private auth from betray
    bool can_betray{ true };
#if ENABLE_VISUALS
    rgba_t color{ 0, 0, 0, 0 };
#else
    char color[16]{};
#endif
};

extern std::unordered_map<uint32, userdata> data;
void Load();

constexpr bool IsFriendly(EState state)
{
    return state != EState::RAGE && state != EState::ANTIBOT && state != EState::DEFAULT && state != EState::CAT;
}

#if ENABLE_VISUALS
rgba_t Color(uint32 steamid);
rgba_t Color(CachedEntity *player);
#endif

userdata &AccessData(uint32 steamid);
userdata &AccessData(CachedEntity *player);
bool IsDefault(uint32 steamid);
bool IsDefault(CachedEntity *player);
bool IsFriend(uint32 steamid);
bool IsFriend(CachedEntity *player);
bool ChangeState(uint32 steamid, EState state, bool force = false);
bool ChangeState(CachedEntity *entity, EState state, bool force = false);
} // namespace playerlist
