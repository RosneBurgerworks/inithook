#include "common.hpp"

namespace hacks::tf2::antipazer
{

static settings::Boolean enabled("anti-pazer.enable", "false");

uint32 pazer_trusted[] = {
    1378752,    // Overheal_
    179077661,  // sAven
    97733808,   // Uncle Dane
    155691357,  // h2overdrive
    67033233,   // raspy
    32604711,   // camp3r
    221123406,  // jon
    1011439610, // wgetJane
    43645661,   // pazer
};

static bool isPazerFound()
{
    auto lobby = CTFLobbyShared::GetLobby();
    if (!lobby)
        return false;

    int members, pending, i;
    CTFLobbyPlayer *player;

    members = lobby->GetNumMembers();
    pending = lobby->GetNumPendingPlayers();
    for (i = 0; i < members; ++i)
    {
        player = lobby->GetPlayer(i);
        if (!player)
            continue;
        if (std::find(std::begin(pazer_trusted), std::end(pazer_trusted), player->GetID().GetAccountID()) != std::end(pazer_trusted))
            return true;
    }
    for (i = 0; i < pending; ++i)
    {
        player = lobby->GetPendingPlayer(i);
        if (!player)
            continue;
        if (std::find(std::begin(pazer_trusted), std::end(pazer_trusted), player->GetID().GetAccountID()) != std::end(pazer_trusted))
            return true;
    }
    return false;
}

static Timer pazer_timer;
static void Paint()
{
    if (pazer_timer.test_and_set(5000))
    {
        if (isPazerFound())
        {
            g_IEngine->ClientCmd_Unrestricted("killserver");
            tfmm::abandon();
        }
    }
}

static void register_anti_pazer(bool enable)
{
    if (enable)
        EC::Register(EC::Paint, Paint, "paint_antipazer");
    else
        EC::Unregister(EC::Paint, "paint_antipazer");
}

static InitRoutine init([]() {
    enabled.installChangeCallback([](bool new_val) {
        register_anti_pazer(new_val);
    });
    if (*enabled)
        register_anti_pazer(true);
});

} // namespace hacks::tf2::antipazer