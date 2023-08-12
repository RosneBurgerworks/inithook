#include "common.hpp"
#include "settings/Bool.hpp"
#include "PlayerTools.hpp"
#include "DetourHook.hpp"
#include "Backtrack.hpp"

#if !ENABLE_NULL_GRAPHICS

namespace hacks::tf2::misc_aimbot
{

static settings::Boolean charge_aim{ "chargeaim.enable", "false" };
static settings::Button charge_key{ "chargeaim.key", "<null>" };

// The best entity for demoknight -- closest in FOV
std::pair<CachedEntity *, Vector> FindBestEnt(bool vischeck)
{
    float fov              = FLT_MAX;
    CachedEntity *best_ent = nullptr;
    Vector best_ent_box_pos;
    for (int i = 1; i <= g_IEngine->GetMaxClients(); i++)
    {
        CachedEntity *ent = ENTITY(i);
        if (CE_GOOD(ent) && ent->m_vecDormantOrigin() && ent->m_bAlivePlayer() && ent->m_bEnemy())
        {
            // I hate this code TwT
            Vector box;
            if (hacks::tf2::backtrack::isBacktrackEnabled)
            {
                auto data = hacks::tf2::backtrack::getClosestEntTick(ent, LOCAL_E->m_vecOrigin(), hacks::tf2::backtrack::defaultTickFilter);
                if (data)
                    box = data->hitboxes.at(head).center;
                else if (!GetHitbox(ent, head, box))
                    continue;
            }
            else if (!GetHitbox(ent, head, box))
                continue;

            float _fov_ = GetFov(g_pLocalPlayer->v_OrigViewangles, g_pLocalPlayer->v_Eye, box);
            if (_fov_ < fov && player_tools::shouldTarget(ent) && (!vischeck || ent->IsVisible()))
            {
                fov              = _fov_;
                best_ent         = ent;
                best_ent_box_pos = box;
            }
        }
    }
    return { best_ent, best_ent_box_pos };
}

static void CreateMove()
{
    if (!*charge_aim)
        return;
#if !ENABLE_NULL_GRAPHICS
    if (charge_key && !charge_key.isKeyDown())
        return;
#endif
    if (CE_BAD(LOCAL_E) || !LOCAL_E->m_bAlivePlayer() || CE_BAD(LOCAL_W))
        return;
    if (!HasCondition<TFCond_Charging>(LOCAL_E))
        return;
    auto bestent = FindBestEnt(true);
    if (bestent.second.IsValid())
    {
        auto angles                  = GetAimAtAngles(g_pLocalPlayer->v_Eye, bestent.second);
        angles.x                     = current_user_cmd->viewangles.x;
        current_user_cmd->viewangles = angles;
    }
}

DetourHook CAM_CapYaw_detour;
typedef float (*CAM_CapYaw_t)(IInput *, float);
// Fix client side limit being applied weirdly, Note that most of this is taken from the source leak directly
float CAM_CapYaw_Hook(IInput *this_, float fVal)
{
    if (CE_INVALID(LOCAL_E))
        return fVal;

    if (HasCondition<TFCond_Charging>(LOCAL_E))
    {
        float flChargeYawCap = re::CTFPlayerShared::CalculateChargeCap(re::CTFPlayerShared::GetPlayerShared(RAW_ENT(LOCAL_E)));

        // Our only change
        flChargeYawCap *= 2.5f;

        if (fVal > flChargeYawCap)
            return flChargeYawCap;
        else if (fVal < -flChargeYawCap)
            return -flChargeYawCap;
    }

    return fVal;
}

static InitRoutine init([]() {
    EC::Register(EC::CreateMove, CreateMove, "cm_chargeaim", EC::average);
    EC::Register(EC::CreateMoveWarp, CreateMove, "cmw_chargeaim", EC::average);

    static auto signature = gSignatures.GetClientSignature("55 89 E5 53 83 EC 14 E8 ? ? ? ? 85 C0 74 ? 8D 98 ? ? ? ? C7 44 24 ? 11 00 00 00");

    CAM_CapYaw_detour.Init(signature, (void *) CAM_CapYaw_Hook);
    EC::Register(
        EC::Shutdown, []() { CAM_CapYaw_detour.Shutdown(); }, "chargeaim_shutdown");
});

} // namespace hacks::tf2::misc_aimbot
#endif
