/*
 * sconvars.cpp
 *
 *  Created on: May 1, 2017
 *      Author: nullifiedcat
 */

#include "sconvars.hpp"

namespace sconvar
{

std::vector<SpoofedConVar *> convars;

SpoofedConVar::SpoofedConVar(ConVar *var) : original(var)
{
    int flags        = var->m_nFlags;
    const char *name = var->m_pszName;
    asprintf(&original_new_name, "q_%s", name);
    if (g_ICvar->FindVar(original_new_name))
        return;

    original_m_pszName = const_cast<char *>(var->m_pszName);
    original_m_nFlags = var->m_nFlags;

    var->m_pszName = original_new_name;
    var->m_nFlags  = 0;
    ConVar *svar   = new ConVar(name, var->m_pszDefaultValue, flags, var->m_pszHelpString, var->m_bHasMin, var->m_fMinVal, var->m_bHasMax, var->m_fMaxVal, var->m_fnChangeCallback);
    g_ICvar->RegisterConCommand(svar);
    spoof = svar;
}

SpoofedConVar::~SpoofedConVar()
{
    original->m_pszName = original_m_pszName;
    original->m_nFlags = original_m_nFlags;
    /* TO DO: Remove spoofed cvar */
    free(original_new_name);
}

CatCommand spoof_convar("spoof", "Spoof ConVar", [](const CCommand &args) {
    if (args.ArgC() < 2)
    {
        logging::Info("Invalid call");
        return;
    }
    ConVar *var = g_ICvar->FindVar(args.Arg(1));
    if (!var)
    {
        logging::Info("Not found");
        return;
    }
    convars.push_back(new SpoofedConVar(var));
});
} // namespace sconvar
