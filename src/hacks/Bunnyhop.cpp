/*
 * HBunnyhop.cpp
 *
 *  Created on: Oct 6, 2016
 *      Author: nullifiedcat
 */

#include <settings/Bool.hpp>
#include "common.hpp"

static settings::Boolean enable{ "bunnyhop.enable", "false" };

namespace hacks::shared::bunnyhop
{

static void CreateMove()
{
    if (!enable || !CE_GOOD(LOCAL_E) || !LOCAL_E->m_bAlivePlayer() || CE_BAD(LOCAL_W))
        return;

    bool ground      = CE_INT(g_pLocalPlayer->entity, netvar.iFlags) & (1 << 0);
    static bool jump = false;

    if (current_user_cmd->buttons & IN_JUMP)
    {
        if (!jump && !ground)
            current_user_cmd->buttons &= ~IN_JUMP;
        else if (jump)
            jump = false;
    }
    else if (!jump)
        jump = true;
}

static InitRoutine EC([]() { EC::Register(EC::CreateMove_NoEnginePred, CreateMove, "cm_unnyhop"); });
} // namespace hacks::shared::bunnyhop
