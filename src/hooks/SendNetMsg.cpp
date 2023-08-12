/*
  Created by Jenny White on 29.04.18.
  Copyright (c) 2018 nullworks. All rights reserved.
*/

#include <settings/Int.hpp>
#include "HookedMethods.hpp"
#include "Warp.hpp"
#include "nospread.hpp"

static settings::Int newlines_msg{ "chat.prefix-newlines", "0" };
static settings::Boolean log_sent{ "debug.log-sent-chat", "false" };

namespace hooked_methods
{
DEFINE_HOOKED_METHOD(SendNetMsg, bool, INetChannel *this_, INetMessage &msg, bool force_reliable, bool voice)
{
    if (!isHackActive())
        return original::SendNetMsg(this_, msg, force_reliable, voice);
    size_t say_idx, say_team_idx;
    int offset;
    std::string newlines{};
    NET_StringCmd stringcmd;

    // Do we have to force reliable state?
    if (hacks::tf2::nospread::SendNetMessage(&msg))
        force_reliable = true;
        // Don't use warp with nospread
    else
        hacks::tf2::warp::SendNetMessage(msg);

    // net_StringCmd
    if (msg.GetType() == 4 && (newlines_msg))
    {
        std::string str(msg.ToString());
        say_idx      = str.find("net_StringCmd: \"say \"");
        say_team_idx = str.find("net_StringCmd: \"say_team \"");
        if (!say_idx || !say_team_idx)
        {
            offset = say_idx ? 26 : 21;
            if (*newlines_msg > 0)
            {
                // TODO move out? update in a value change callback?
                newlines = std::string(*newlines_msg, '\n');
                str.insert(offset, newlines);
            }
            str = str.substr(16, str.length() - 17);
            stringcmd.m_szCommand = str.c_str();
            return original::SendNetMsg(this_, stringcmd, force_reliable, voice);
        }
    }
    static ConVar *sv_player_usercommand_timeout = g_ICvar->FindVar("sv_player_usercommand_timeout");
    static float lastcmd                         = 0.0f;
    if (lastcmd > g_GlobalVars->absoluteframetime)
    {
        lastcmd = g_GlobalVars->absoluteframetime;
    }
    if (log_sent && msg.GetType() != 3 && msg.GetType() != 9)
    {
        unsigned char buf[4096];
        std::string bytes = "";

        logging::Info("=> %s [%i] %s", msg.GetName(), msg.GetType(), msg.ToString());
        bf_write buffer("cathook_debug_buffer", buf, 4096);
        logging::Info("Writing %i", msg.WriteToBuffer(buffer));
        for (int i = 0; i < buffer.GetNumBytesWritten(); i++)
            bytes += format((unsigned short) buf[i], ' ');
        logging::Info("%i bytes => %s", buffer.GetNumBytesWritten(), bytes.c_str());
    }
    bool ret_val = original::SendNetMsg(this_, msg, force_reliable, voice);
    hacks::tf2::nospread::SendNetMessagePost();
    return ret_val;
}
} // namespace hooked_methods
