/* This is free and unencumbered software released into the public domain.
 * Anyone is free to copy, modify, publish, use, compile, sell, or
 * distribute this software, either in source code form or as a compiled
 * binary, for any purpose, commercial or non-commercial, and by any
 * means.
 *
 * In jurisdictions that recognize copyright laws, the author or authors
 * of this software dedicate any and all copyright interest in the
 * software to the public domain. We make this dedication for the benefit
 * of the public at large and to the detriment of our heirs and
 * successors. We intend this dedication to be an overt act of
 * relinquishment in perpetuity of all present and future rights to this
 * software under copyright law.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */
#include "common.hpp"

static settings::Boolean pb("party-bypass", "true");

static CatCommand identify("print_steamid", "Prints your SteamID", []() {
    logging::InfoConsole("%u", g_ISteamUser->GetSteamID().GetAccountID());
});

static void toggle(bool on)
{
    static BytePatch patches[] = {
        {
            e8call_direct(gSignatures.GetClientSignature(
            "55 89 E5 57 56 53 83 EC 1C 8B 5D ? 8B 75 ? 8B 7D ? 89 1C 24 89 74 24 ? 89 7C 24 ? E8 ? ? ? ? 84 C0") + 29),
            std::experimental::make_array<uint8_t>(0x31, 0xC0, 0x40, 0xC3)
        },
        {
            gSignatures.GetClientSignature,
            "55 89 E5 57 56 53 83 EC ? 8B 45 0C 8B 5D 08 8B 55 10 89 45 ? 8B 43",
            0x00, std::experimental::make_array<uint8_t>(0x31, 0xC0, 0x40, 0xC3)
        }
    };
    for (auto & patch : patches)
    {
        if (on)
            patch.Patch();
        else
            patch.Shutdown();
    }
}

static InitRoutine init([]() {
    pb.installChangeCallback(toggle);
    if (*pb)
        toggle(true);
});
