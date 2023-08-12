/*
  Created by Jenny White on 29.04.18.
  Copyright (c) 2018 nullworks. All rights reserved.
*/

#include <settings/Bool.hpp>
#include "HookedMethods.hpp"
#include "MiscTemporary.hpp"
#include "votelogger.hpp"

static settings::Boolean die_if_vac{ "shutdown.die-if-vac", "false" };
static settings::String custom_disconnect_reason{ "shutdown.disconnect-reason", "" };
static settings::Boolean autoabandon{ "misc.auto-abandon", "false" };

settings::Boolean abandon_on_mm_fail{"shutdown.abandon-on-mm-fail", "false"};

namespace hooked_methods
{
DEFINE_HOOKED_METHOD(Shutdown, void, INetChannel *this_, const char *reason)
{
    g_Settings.bInvalid = true;
    need_name_change = true;
    votelogger::Reset();
    logging::Info("Disconnect: %s", reason);
    if (strstr(reason, "banned") || (strstr(reason, "Generic_Kicked") && tfmm::isMMBanned()))
    {
        if (*die_if_vac)
        {
            logging::Info("VAC/Matchmaking banned");
            std::_Exit(EXIT_SUCCESS);
        }
    }
    if (strstr(reason, "Ad-hoc") || strstr(reason, "shutting down")) {
        if (*abandon_on_mm_fail) {
            logging::Info("MM Fail: Ad-hoc or server shutting down, abandoning.");
            g_IEngine->ClientCmd_Unrestricted("killserver");
            tfmm::abandon();
        }
    }
#if ENABLE_IPC
    ipc::UpdateServerAddress(true);
#endif
    if (isHackActive() && (custom_disconnect_reason.toString().size() > 3) && strstr(reason, "user"))
        original::Shutdown(this_, custom_disconnect_reason.toString().c_str());
    else
        original::Shutdown(this_, reason);
    if (autoabandon && !ignoredc)
        tfmm::disconnectAndAbandon();

    ignoredc = false;
}
} // namespace hooked_methods
