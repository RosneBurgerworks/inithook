#include "HookedMethods.hpp"
#include "crits.hpp"

namespace hooked_methods
{

DEFINE_HOOKED_METHOD(RunCommand, void, IPrediction *prediction, IClientEntity *entity, CUserCmd *usercmd, IMoveHelper *move)
{
    original::RunCommand(prediction, entity, usercmd, move);

    if (CE_GOOD(LOCAL_E) && CE_GOOD(LOCAL_W) && entity && entity->entindex() == g_pLocalPlayer->entity_idx && usercmd && usercmd->command_number)
        criticals::fixBucket(RAW_ENT(LOCAL_W), usercmd);
}

} // namespace hooked_methods
