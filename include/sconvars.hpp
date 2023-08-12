/*
 * sconvars.hpp
 *
 *  Created on: May 1, 2017
 *      Author: nullifiedcat
 */

#pragma once

#include "common.hpp"

/*
 * HECK off F1ssi0N
 * I won't make NETWORK HOOKS to deal with this SHIT
 */

namespace sconvar
{

class SpoofedConVar
{
public:
    SpoofedConVar(ConVar *var);
    ~SpoofedConVar();

public:
    ConVar *original;
    ConVar *spoof;
private:
    char *original_new_name, *original_m_pszName;
    int original_m_nFlags;
};
} // namespace sconvar
