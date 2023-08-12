/*
  Created by Jenny White on 29.04.18.
  Copyright (c) 2018 nullworks. All rights reserved.
*/

#pragma once

#include <settings/Bool.hpp>
#include "common.hpp"
#include "DetourHook.hpp"

// This is a temporary file to put code that needs moving/refactoring in.
extern bool *bSendPackets;
extern bool ignoreKeys;
extern std::array<int, 32> bruteint;
extern std::array<Timer, 32> timers;

extern bool firstcm;
extern bool ignoredc;
extern bool do_fakelag;

extern bool calculated_can_shoot;
extern float prevflow;
extern int prevflowticks;
extern int anti_balance_attempts;
extern int namesteal_target_id;
#if ENABLE_VISUALS
extern int spectator_target;
#endif

extern float backup_lerp;

extern std::string target_name;
extern std::string current_server_name;

void DoSlowAim(Vector &input_angle, float speed = 5);

extern settings::Boolean clean_screenshots;
extern settings::Boolean nolerp;
extern settings::Boolean no_zoom;
extern settings::Boolean disable_visuals;
extern settings::Int fakelag_amount;
extern int stored_buttons;
#if ENABLE_VISUALS
extern bool freecam_is_toggled;
#endif
typedef void (*CL_SendMove_t)();
extern DetourHook cl_warp_sendmovedetour;
extern DetourHook cl_nospread_sendmovedetour;