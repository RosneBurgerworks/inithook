/*
 * CatBot.hpp
 *
 *  Created on: Dec 30, 2017
 *      Author: nullifiedcat
 */

#pragma once

#include "common.hpp"

namespace hacks::shared::catbot
{
extern settings::Boolean catbotmode;
extern settings::Boolean anti_motd;

#if ENABLE_IPC
void update_ipc_data(ipc::user_data_s &data);
#endif
} // namespace hacks::shared::catbot
