/*
  Created by Jenny White on 29.04.18.
  Copyright (c) 2018 nullworks. All rights reserved.
*/

#include <settings/Bool.hpp>
#include <hacks/visual/Thirdperson.hpp>
#include "HookedMethods.hpp"
#include "AntiAntiAim.hpp"
#include "MiscTemporary.hpp"

static settings::Float nightmode{ "visual.night-mode", "0" };
static settings::Rgba nightmode_color{ "visual.night-mode.color", "000000FF" };
static settings::Boolean no_shake{ "visual.no-shake", "true" };
static settings::Boolean override_textures{ "visual.override-textures", "false" };
static settings::String override_textures_texture{ "visual.override-textures.custom-texture", "dev/dev_measuregeneric01b" };

static float old_nightmode{ 0.0f };
static rgba_t old_nightmode_color{ 0, 0, 0, 0 };

static bool update_override_textures = false;
std::vector<std::string> dont_override_strings = { "glass", "door", "water", "tools", "player" };
std::vector<std::string> nodraw_strings        = { "decal", "overlay", "hay" };

namespace hooked_methods
{

DEFINE_HOOKED_METHOD(FrameStageNotify, void, void *this_, ClientFrameStage_t stage)
{
    if (!isHackActive())
        return original::FrameStageNotify(this_, stage);

    PROF_SECTION(FrameStageNotify_TOTAL);


    if (update_override_textures)
    {
        if (override_textures)
        {
            for (MaterialHandle_t i = g_IMaterialSystem->FirstMaterial(); i != g_IMaterialSystem->InvalidMaterial(); i = g_IMaterialSystem->NextMaterial(i))
            {
                IMaterial *pMaterial = g_IMaterialSystem->GetMaterial(i);
                if (!pMaterial)
                    continue;

                auto name = std::string(pMaterial->GetTextureGroupName());
                auto path = std::string(pMaterial->GetName());

                // Ensure world mat
                if (name.find("World") == std::string::npos)
                    continue;
                // Don't override this stuff
                bool good = true;
                for (auto &entry : dont_override_strings)
                    if (path.find(entry) != path.npos)
                    {
                        good = false;
                    }
                // Don't draw this stuff
                for (auto &entry : nodraw_strings)
                    if (path.find(entry) != path.npos)
                    {
                        pMaterial->SetMaterialVarFlag(MATERIAL_VAR_NO_DRAW, true);
                        good = false;
                    }
                if (!good)
                    continue;

                if (!pMaterial->GetMaterialVarFlag(MATERIAL_VAR_NO_DRAW))
                {
                    auto *kv = new KeyValues(pMaterial->GetShaderName());
                    kv->SetString("$basetexture", (*override_textures_texture).c_str());
                    kv->SetString("$basetexturetransform", "center .5 .5 scale 6 6 rotate 0 translate 0 0");
                    kv->SetString("$surfaceprop", "concrete");
                    pMaterial->SetShaderAndParams(kv);
                }
            }
        }
        update_override_textures = false;
    }

    if (old_nightmode != *nightmode || old_nightmode_color != *nightmode_color)
    {
        static ConVar *r_DrawSpecificStaticProp = g_ICvar->FindVar("r_DrawSpecificStaticProp");
        if (!r_DrawSpecificStaticProp)
        {
            r_DrawSpecificStaticProp = g_ICvar->FindVar("r_DrawSpecificStaticProp");
            return;
        }
        r_DrawSpecificStaticProp->SetValue(0);

        for (MaterialHandle_t i = g_IMaterialSystem->FirstMaterial(); i != g_IMaterialSystem->InvalidMaterial(); i = g_IMaterialSystem->NextMaterial(i))
        {
            IMaterial *pMaterial = g_IMaterialSystem->GetMaterial(i);

            if (!pMaterial)
                continue;
            if (strstr(pMaterial->GetTextureGroupName(), "World") || strstr(pMaterial->GetTextureGroupName(), "StaticProp"))
            {
                if (float(nightmode) > 0.0f)
                {
                    rgba_t draw_color = colors::Fade(colors::white, *nightmode_color, (*nightmode / 100.0f) * (PI / 2), 1.0f);

                    float r, g, b, a;
                    pMaterial->GetColorModulation(&r, &g, &b);
                    a = pMaterial->GetAlphaModulation();
                    if (r != draw_color.r || g != draw_color.g || b != draw_color.b)
                        pMaterial->ColorModulate(draw_color.r, draw_color.g, draw_color.b);
                    if (a != draw_color.a)
                        pMaterial->AlphaModulate((*nightmode_color).a);
                }
                else
                    pMaterial->ColorModulate(1.0f, 1.0f, 1.0f);
            }
        }
        old_nightmode       = *nightmode;
        old_nightmode_color = *nightmode_color;
    }

    if (!g_IEngine->IsInGame())
        g_Settings.bInvalid = true;
    {
        PROF_SECTION(FSN_antiantiaim);
        hacks::shared::anti_anti_aim::frameStageNotify(stage);
    }
    if (isHackActive() && !g_Settings.bInvalid && stage == FRAME_RENDER_START)
    {
        if (nolerp)
        {
            for (int i = 1; i <= g_IEngine->GetMaxClients(); i++)
            {
                CachedEntity *ent = ENTITY(i);
                if (CE_GOOD(ent) && ent->m_bAlivePlayer())
                {
                    re::C_BaseEntity::AddEffects(RAW_ENT(ent), EF_NOINTERP);
                    re::C_BaseEntity::SetAbsOrigin(RAW_ENT(ent), ent->m_vecOrigin());
                }
            }
        }
        IF_GAME(IsTF())
        {
            if (no_shake && CE_GOOD(LOCAL_E) && LOCAL_E->m_bAlivePlayer())
            {
                NET_VECTOR(RAW_ENT(LOCAL_E), netvar.vecPunchAngle)    = { 0.0f, 0.0f, 0.0f };
                NET_VECTOR(RAW_ENT(LOCAL_E), netvar.vecPunchAngleVel) = { 0.0f, 0.0f, 0.0f };
            }
        }
        hacks::tf::thirdperson::frameStageNotify();
    }
    return original::FrameStageNotify(this_, stage);
}

// Reset nightmode on level init so it applys again
static InitRoutine init([]() {
    override_textures.installChangeCallback([](bool after) { update_override_textures = true; });
    override_textures_texture.installChangeCallback([](std::string after) { update_override_textures = true; });
    EC::Register(
        EC::LevelInit,
        []() {
            old_nightmode            = { 0.0f };
            old_nightmode_color      = { 0, 0, 0, 0 };
            update_override_textures = true;
        },
        "fsn_levelinit");
});

} // namespace hooked_methods
