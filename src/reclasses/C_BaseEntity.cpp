/*
 * C_BaseEntity.cpp
 *
 *  Created on: Mar 20, 2021
 *      Author: aUniqueUser
 */

#include "reclasses.hpp"
#include "copypasted/CSignature.h"
#include "e8call.hpp"

uintptr_t estimateAbsVelocity_addr;

void re::C_BaseEntity::EstimateAbsVelocity(IClientEntity *self, Vector &out)
{
    typedef void (*EstimateAbsVelocity_t)(IClientEntity *, Vector &);
    static uintptr_t addr = gSignatures.GetClientSignature("55 89 E5 56 53 83 EC 20 8B 5D ? 8B 75 ? E8 ? ? ? ? 39 D8");
    static EstimateAbsVelocity_t EstimateAbsVelocity_fn = EstimateAbsVelocity_t(addr);
    EstimateAbsVelocity_fn(self, out);
}
