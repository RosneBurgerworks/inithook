/*
 * targethelper.cpp
 *
 *  Created on: Oct 16, 2016
 *      Author: nullifiedcat
 */

#include "common.hpp"
#include "hacks/Backtrack.hpp"

/*
 * Targeting priorities:
 * passive bullet vacc medic
 * zoomed snipers ALWAYS
 * medics
 * snipers
 * spies
 */

/* Assuming given entity is a valid target range 0 to 100 */
int GetScoreForEntity(CachedEntity *entity)
{
    if (!entity)
        return 0;
    if (entity->m_Type() == ENTITY_BUILDING)
    {
        if (entity->m_iClassID() == CL_CLASS(CObjectSentrygun))
        {
            if (entity->m_flDistance() < 400.0f)
                return 120;
            else if (entity->m_flDistance() < 1100.0f)
                return 60;
            return 30;
        }
        return 0;
    }
    bool resistant = IsPlayerResistantToCurrentClass(entity, false);
    bool zoomed    = HasCondition<TFCond_Zoomed>(entity);
    bool kritz     = IsPlayerCritBoosted(entity);
    bool special   = false;
    int total      = 0;
    switch (CE_INT(entity, netvar.iClass))
    {
    case tf_sniper:
        total += 25;
        if (zoomed)
        {
            total += 50;
        }
        special = true;
        break;
    case tf_medic:
        if (resistant)
        {
            total += 50;
            special = true;
            break;
        }
        else
        {
            int weapon_idx = CE_INT(entity, netvar.hActiveWeapon) & 0xFFF;
            if (IDX_GOOD(weapon_idx))
            {
                CachedEntity *weapon = ENTITY(weapon_idx);
                if (!CE_INVALID(weapon) && weapon->m_iClassID() == CL_CLASS(CWeaponMedigun) && weapon)
                {
                    int charge_amt = (int) floor(CE_FLOAT(weapon, netvar.m_flChargeLevel) * 100);
                    if (charge_amt >= 100)
                    {
                        total += 100;
                        special = true;
                    }
                    else if (charge_amt > 0)
                    {
                        total += charge_amt / 2;
                        special = true;
                    }
                }
            }
        }
        break;
    case tf_spy:
        total += 10;
        break;
    case tf_soldier:
        if (HasCondition<TFCond_BlastJumping>(entity))
            total += 30;
        break;
    }
    if (!special)
    {
        if (resistant)
        {
            total += 50;
        }
        if (kritz)
        {
            total += 99;
        }
        if (entity->m_flDistance() != 0)
        {
            int distance_factor = (4096 / entity->m_flDistance()) * 4;
            total += distance_factor;
            if (entity->m_iHealth() != 0)
            {
                int health_factor = (450 / entity->m_iHealth()) * 4;
                if (health_factor > 30)
                    health_factor = 30;
                total += health_factor;
            }
            if (entity->m_flDistance() <= 400)
                total += 50;
        }
    }
    if (total > 99)
        total = 99;
    if (playerlist::AccessData(entity).state == playerlist::EState::RAGE || playerlist::AccessData(entity).state == playerlist::EState::ANTIBOT)
        total += 999;
    if (IsSentryBuster(entity))
        total = 0;
    return total;
}
