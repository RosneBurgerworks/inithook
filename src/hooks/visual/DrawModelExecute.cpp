/*
    This file is part of Cathook.

    Cathook is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Cathook is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Cathook. If not, see <https://www.gnu.org/licenses/>.
*/

// Codeowners: aUniqueUser

#include <PlayerTools.hpp>
#include "common.hpp"
#include "HookedMethods.hpp"
#include "MiscTemporary.hpp"
#include "Backtrack.hpp"
#include "EffectGlow.hpp"
#include "Aimbot.hpp"

/* World visual rvars */
static settings::Boolean no_arms{ "remove.arms", "false" };
static settings::Boolean no_hats{ "remove.hats", "false" };
static settings::Boolean blend_zoom{ "zoom.blend", "false" };

static settings::Boolean enable{ "chams.enable", "false" };
static settings::Boolean render_original{ "chams.original", "false" };

/* Cham target rvars */
static settings::Boolean health{ "chams.health", "false" };
static settings::Boolean teammates{ "chams.show.teammates", "false" };
static settings::Boolean disguised{ "chams.show.disguised", "true" };
static settings::Boolean players{ "chams.show.players", "true" };
static settings::Boolean medkits{ "chams.show.medkits", "false" };
static settings::Boolean intel{ "chams.show.intel", "true" };
static settings::Boolean ammobox{ "chams.show.ammoboxes", "false" };
static settings::Boolean buildings{ "chams.show.buildings", "true" };
static settings::Boolean stickies{ "chams.show.stickies", "true" };
static settings::Boolean stickies_local{ "chams.show.stickies.local", "true" };
static settings::Boolean pipes{ "chams.show.pipes", "true" };
static settings::Boolean pipes_local{ "chams.show.pipes.local", "true" };
static settings::Boolean teammate_buildings{ "chams.show.teammate-buildings", "false" };
static settings::Boolean recursive{ "chams.recursive", "true" };
static settings::Boolean legit{ "chams.legit", "false" };
static settings::Boolean singlepass{ "chams.single-pass", "false" };
static settings::Boolean chamsself{ "chams.self", "true" };
static settings::Boolean weapons{ "chams.weapons", "false" };
static settings::Rgba weapons_base{ "chams.weapons.basecolor", "ffffffff" };
static settings::Rgba weapons_overlay{ "chams.weapons.overlaycolor", "ffffffff" };
static settings::Float cham_alpha{ "chams.alpha", "1" };
static settings::Boolean overlay_chams{ "chams.overlay", "false" };

/* Lighting rvars */
static settings::Boolean phong_enable{ "chams.phong", "true" };
static settings::Boolean halfambert{ "chams.halfambert", "true" };
static settings::Boolean phong_fresnelrange{ "chams.phonefresnelranges", "true" };
static settings::Int phong_boost{ "chams.phongboost", "2" };
static settings::Int additive{ "chams.additive", "1" };
static settings::Int pearlescent{ "chams.pearlescent", "8" };

static settings::Float phong_exponent{ "chams.phongexponent", "25" };
static settings::Float phong_fresnelrange_1{ "chams.phongfresnelranges.1", "0" };
static settings::Float phong_fresnelrange_2{ "chams.phongfresnelranges.2", "3" };
static settings::Float phong_fresnelrange_3{ "chams.phongfresnelranges.3", "15" };

static settings::Boolean rimlighting{ "chams.rimlighting", "true" };
static settings::Float rimlighting_boost{ "chams.rimlighting.boost", "1" };
static settings::Float rimlighting_exponent{ "chams.rimlighting.exponent", "4" };

/* Customization of envmap */
static settings::Boolean envmap{ "chams.envmap", "true" };
static settings::Boolean envmap_matt{ "chams.envmap.matt", "false" };
static settings::Float envmapfresnel{ "chams.envmapfresnel", "1" };
static settings::Boolean envmap_tint{ "chams.envmap.tint", "true" };
static settings::Float envmap_tint_red_r{ "chams.envmap.tint.red.r", "4" };
static settings::Float envmap_tint_red_g{ "chams.envmap.tint.red.g", "0" };
static settings::Float envmap_tint_red_b{ "chams.envmap.tint.red.b", "2" };
static settings::Float envmap_tint_blu_r{ "chams.envmap.tint.blu.r", "0" };
static settings::Float envmap_tint_blu_g{ "chams.envmap.tint.blu.g", "2" };
static settings::Float envmap_tint_blu_b{ "chams.envmap.tint.blu.b", "4" };
static settings::Float envmap_tint_weapons_r{ "chams.envmap.tint.weapons.r", "1" };
static settings::Float envmap_tint_weapons_g{ "chams.envmap.tint.weapons.g", "1" };
static settings::Float envmap_tint_weapons_b{ "chams.envmap.tint.weapons.b", "1" };

/* Overlay chams team highlight colors */
static settings::Rgba chams_overlay_color_blu{ "chams.overlay.overlaycolor.blu", "000000ff" };
static settings::Rgba chams_overlay_color_red{ "chams.overlay.overlaycolor.red", "000000ff" };

/* Seperate cham settings when ignorez */
static settings::Boolean novis{ "chams.novis", "true" };
static settings::Rgba novis_team_red{ "chams.novis.red", "ff8800ff" };
static settings::Rgba novis_team_blu{ "chams.novis.blu", "bc00ffff" };

/* Customization of novis envmap */
static settings::Float envmap_tint_red_r_novis{ "chams.novis.envmap.tint.red.r", "4" };
static settings::Float envmap_tint_red_g_novis{ "chams.novis.envmap.tint.red.g", "4" };
static settings::Float envmap_tint_red_b_novis{ "chams.novis.envmap.tint.red.b", "1" };
static settings::Float envmap_tint_blu_r_novis{ "chams.novis.envmap.tint.blu.r", "4" };
static settings::Float envmap_tint_blu_g_novis{ "chams.novis.envmap.tint.blu.g", "1" };
static settings::Float envmap_tint_blu_b_novis{ "chams.novis.envmap.tint.blu.b", "4" };

/* Overlay chams novis team highlight colors */
static settings::Rgba chams_overlay_color_blu_novis{ "chams.novis.overlay.overlaycolor.blu", "ff00ffff" };
static settings::Rgba chams_overlay_color_red_novis{ "chams.novis.overlay.overlaycolor.red", "ff0000ff" };

/* DME Nightmode */
static settings::Float nightmode{ "visual.dme-night-mode", "0" };
static settings::Rgba nightmode_color{ "visual.dme-night-mode.color", "000000FF" };

class Materials
{
public:
    CMaterialReference mat_dme_unlit;
    CMaterialReference mat_dme_lit;
    CMaterialReference mat_dme_unlit_overlay_base;
    CMaterialReference mat_dme_lit_overlay;

    // Sadly necessary ):
    CMaterialReference mat_dme_lit_fp;
    CMaterialReference mat_dme_unlit_overlay_base_fp;
    CMaterialReference mat_dme_lit_overlay_fp;

    void Shutdown()
    {
        mat_dme_unlit.Shutdown();
        mat_dme_lit.Shutdown();
        mat_dme_unlit_overlay_base.Shutdown();
        mat_dme_lit_overlay.Shutdown();

        mat_dme_lit_fp.Shutdown();
        mat_dme_unlit_overlay_base_fp.Shutdown();
        mat_dme_lit_overlay_fp.Shutdown();
    }
};

class ChamColors
{
public:
    float envmap_r, envmap_g, envmap_b;
    rgba_t rgba;

    rgba_t rgba_overlay = colors::empty;

    ChamColors(rgba_t col = colors::empty, float r = 1.0f, float g = 1.0f, float b = 1.0f)
    {
        rgba     = col;
        envmap_r = r;
        envmap_g = g;
        envmap_b = b;
    }
};

namespace hooked_methods
{
static bool init_mat = false;
static Materials mats;

template <typename T> void rvarCallback(T)
{
    init_mat = false;
}

class DrawEntry
{
public:
    int entidx;
    int parentidx;
    DrawEntry()
    {
    }
    DrawEntry(int own_idx, int parent_idx)
    {
        entidx    = own_idx;
        parentidx = parent_idx;
    }
};


static InitRoutine init_dme([]() {
    EC::Register(
        EC::LevelShutdown,
        []() {
            if (init_mat)
            {
                mats.Shutdown();
                init_mat = false;
            }
        },
        "dme_lvl_shutdown");

    halfambert.installChangeCallback(rvarCallback<bool>);
    additive.installChangeCallback(rvarCallback<int>);
    pearlescent.installChangeCallback(rvarCallback<int>);

    phong_enable.installChangeCallback(rvarCallback<bool>);
    phong_boost.installChangeCallback(rvarCallback<int>);
    phong_exponent.installChangeCallback(rvarCallback<float>);
    phong_fresnelrange.installChangeCallback(rvarCallback<bool>);
    phong_fresnelrange_1.installChangeCallback(rvarCallback<float>);
    phong_fresnelrange_2.installChangeCallback(rvarCallback<float>);
    phong_fresnelrange_3.installChangeCallback(rvarCallback<float>);

    rimlighting.installChangeCallback(rvarCallback<bool>);
    rimlighting_boost.installChangeCallback(rvarCallback<float>);
    rimlighting_exponent.installChangeCallback(rvarCallback<float>);

    envmap.installChangeCallback(rvarCallback<bool>);
    envmapfresnel.installChangeCallback(rvarCallback<float>);
    envmap_tint.installChangeCallback(rvarCallback<bool>);
    envmap_matt.installChangeCallback(rvarCallback<bool>);
});

// Purpose => Returns true if we should render provided internal entity
bool ShouldRenderChams(CachedEntity *ent)
{
    if (CE_BAD(LOCAL_E) || CE_BAD(ent))
        return false;
    if (!enable && !hacks::tf2::backtrack::chams)
        return false;
    if (ent->m_IDX == LOCAL_E->m_IDX)
        return *chamsself;
    if (ent->m_iClassID() == CL_CLASS(CCaptureFlag))
        return *intel;
    switch (ent->m_Type())
    {
    case ENTITY_BUILDING:
        if (!buildings)
            return false;
        if (!ent->m_bEnemy() && !(teammate_buildings || teammates))
            return false;
        if (ent->m_iHealth() == 0 || !ent->m_iHealth())
            return false;
        if (CE_BYTE(LOCAL_E, netvar.m_bCarryingObject) && ent->m_IDX == HandleToIDX(CE_INT(LOCAL_E, netvar.m_hCarriedObject)))
            return false;
        return true;
    case ENTITY_PLAYER:
        if (!players)
            return false;
        if (!disguised && IsPlayerDisguised(ent))
            return false;
        if (!teammates && !ent->m_bEnemy() && playerlist::IsDefault(ent))
            return false;
        if (CE_BYTE(ent, netvar.iLifeState))
            return false;
        return true;
    case ENTITY_PROJECTILE:
        if (ent->m_iClassID() == CL_CLASS(CTFGrenadePipebombProjectile))
            if (stickies || pipes)
            {
                if (CE_INT(ent, netvar.iPipeType) != 1)
                {
                    if (pipes)
                    {
                        if (pipes_local && chamsself)
                            if ((CE_INT(ent, netvar.hThrower) & 0xFFF) == g_pLocalPlayer->entity->m_IDX) // Check if the sticky is the players own
                                return true;
                        if (ent->m_bEnemy())
                            return true;
                    }
                    else
                        return false;
                }
                if (stickies_local && chamsself)
                    if ((CE_INT(ent, netvar.hThrower) & 0xFFF) == g_pLocalPlayer->entity->m_IDX) // Check if the sticky is the players own
                        return true;
                if (ent->m_bEnemy())
                    return true;
            }
        break;
    case ENTITY_GENERIC:
        switch (ent->m_ItemType())
        {
        case ITEM_HEALTH_LARGE:
        case ITEM_HEALTH_MEDIUM:
        case ITEM_HEALTH_SMALL:
            return *medkits;
        case ITEM_AMMO_LARGE:
        case ITEM_AMMO_MEDIUM:
        case ITEM_AMMO_SMALL:
            return *ammobox;
        default:
            break;
        }
        break;
    default:
        break;
    }
    return false;
}

// Purpose => Get ChamColors struct from internal entity
static ChamColors GetChamColors(IClientEntity *entity, bool ignorez)
{
    CachedEntity *ent = ENTITY(entity->entindex());

    if (CE_BAD(ent))
        return ChamColors(colors::white);
    if (ent->m_IDX == hacks::shared::aimbot::target_eid)
        return ChamColors(colors::target);
    if (re::C_BaseCombatWeapon::IsBaseCombatWeapon(entity))
    {
        IClientEntity *owner = re::C_TFWeaponBase::GetOwnerViaInterface(entity);
        if (owner)
            return GetChamColors(owner, ignorez);
    }
    switch (ent->m_Type())
    {
    case ENTITY_BUILDING:
        if (!ent->m_bEnemy() && !(teammates || teammate_buildings) && ent != LOCAL_E)
            return ChamColors();
        if (health)
            return ChamColors(colors::Health_dimgreen(ent->m_iHealth(), ent->m_iMaxHealth()));
        break;
    case ENTITY_PLAYER:
        if (!players)
            return ChamColors();
        if (health)
            return ChamColors(colors::Health_dimgreen(ent->m_iHealth(), ent->m_iMaxHealth()));
        break;
    default:
        break;
    }
    if (ent->m_Type() == ENTITY_PLAYER || ent->m_Type() == ENTITY_BUILDING || ent->m_Type() == ENTITY_PROJECTILE)
    {
        ChamColors result;

        if (ent->m_iTeam() == TEAM_BLU)
            result = ChamColors(colors::blu, *envmap_tint_blu_r, *envmap_tint_blu_g, *envmap_tint_blu_b);
        else if (ent->m_iTeam() == TEAM_RED)
            result = ChamColors(colors::red, *envmap_tint_red_r, *envmap_tint_red_g, *envmap_tint_red_b);
        if (novis && ignorez)
        {
            if (ent->m_iTeam() == TEAM_BLU)
                result = ChamColors(*novis_team_blu, *envmap_tint_blu_r_novis, *envmap_tint_blu_g_novis, *envmap_tint_blu_b_novis);
            else if (ent->m_iTeam() == TEAM_RED)
                result = ChamColors(*novis_team_red, *envmap_tint_red_r_novis, *envmap_tint_red_g_novis, *envmap_tint_red_b_novis);
        }
        if (ent->m_Type() == ENTITY_PLAYER)
        {
            if (IsPlayerInvulnerable(ent))
            {
                if (ent->m_iTeam() == TEAM_BLU)
                    result = ChamColors(colors::blu_u, *envmap_tint_blu_r, *envmap_tint_blu_g, *envmap_tint_blu_b);
                else if (ent->m_iTeam() == TEAM_RED)
                    result = ChamColors(colors::red_u, *envmap_tint_red_r, *envmap_tint_red_g, *envmap_tint_red_b);
            }
            if (HasCondition<TFCond_UberBulletResist>(ent))
            {
                if (ent->m_iTeam() == TEAM_BLU)
                    result = ChamColors(colors::blu_v, *envmap_tint_blu_r, *envmap_tint_blu_g, *envmap_tint_blu_b);
                else if (ent->m_iTeam() == TEAM_RED)
                    result = ChamColors(colors::red_v, *envmap_tint_red_r, *envmap_tint_red_g, *envmap_tint_red_b);
            }
        }
        auto o = player_tools::forceEspColor(ent);
        if (o.has_value() && !(!ent->m_bEnemy() && playerlist::AccessData(ent->player_info.friendsID).state == playerlist::EState::RAGE))
            result = ChamColors(*o);

        return result;
    }
    return ChamColors(colors::EntityF(ent));
}

// Purpose => Render entity attachments (weapons, hats)
void RenderAttachment(IClientEntity *entity, IClientEntity *attach, CMaterialReference &mat)
{
    if (attach->ShouldDraw())
    {
        if (entity->GetClientClass()->m_ClassID == RCC_PLAYER && re::C_BaseCombatWeapon::IsBaseCombatWeapon(attach))
        {
            // If separate weapon settings is used, apply them
            if (weapons)
            {
                // Backup original color
                rgba_t original;
                g_IVRenderView->GetColorModulation(original.rgba);
                g_IVRenderView->SetColorModulation(*weapons_base);

                // Setup material
                g_IVRenderView->SetBlend((*weapons_base).a);
                if (mat && envmap)
                    mat->FindVar("$envmaptint", nullptr)->SetVecValue(*envmap_tint_weapons_r, *envmap_tint_weapons_g, *envmap_tint_weapons_b);

                // Render
                attach->DrawModel(1);

                if (overlay_chams)
                {
                    // Setup material
                    g_IVRenderView->SetColorModulation(*weapons_overlay);
                    g_IVRenderView->SetBlend((*weapons_overlay).a);
                    if (mat && envmap)
                        mat->FindVar("$envmaptint", nullptr)->SetVecValue(*envmap_tint_weapons_r, *envmap_tint_weapons_g, *envmap_tint_weapons_b);

                    // Render
                    attach->DrawModel(1);
                }

                // Reset it!
                g_IVRenderView->SetColorModulation(original.rgba);
            }
            else
            {
                attach->DrawModel(1);
            }
        }
        else
            attach->DrawModel(1);
    }
}

// Locked from drawing
static bool chams_attachment_drawing = false;

// Purpose => Render overriden model and and attachments
void RenderChamsRecursive(IClientEntity *entity, CMaterialReference &mat, IVModelRender *this_, const DrawModelState_t &state, const ModelRenderInfo_t &info, matrix3x4_t *bone)
{
    if (!enable)
        return;
    original::DrawModelExecute(this_, state, info, bone);

    if (!*recursive)
        return;

    IClientEntity *attach;
    int passes = 0;

    attach = g_IEntityList->GetClientEntity(*(int *) ((uintptr_t) entity + netvar.m_Collision - 24) & 0xFFF);
    while (attach && passes++ < 32)
    {
        chams_attachment_drawing = true;
        RenderAttachment(entity, attach, mat);
        chams_attachment_drawing = false;
        attach                   = g_IEntityList->GetClientEntity(*(int *) ((uintptr_t) attach + netvar.m_Collision - 20) & 0xFFF);
    }
}

// Purpose => Apply and render chams according to settings
void ApplyChams(ChamColors colors, bool recurse, bool render_original, bool overlay, bool ignorez, bool wireframe, bool firstperson, IClientEntity *entity, IVModelRender *this_, const DrawModelState_t &state, const ModelRenderInfo_t &info, matrix3x4_t *bone)
{
    static auto &mat = firstperson ? overlay ? mats.mat_dme_unlit_overlay_base_fp : mats.mat_dme_lit_fp : overlay ? mats.mat_dme_unlit_overlay_base : mats.mat_dme_lit;
    if (render_original)
        recurse ? RenderChamsRecursive(entity, mat, this_, state, info, bone) : original::DrawModelExecute(this_, state, info, bone);

    // Setup material
    g_IVRenderView->SetColorModulation(colors.rgba);
    g_IVRenderView->SetBlend((colors.rgba).a);
    mat->AlphaModulate((colors.rgba).a);
    if (envmap && envmap_tint)
        mat->FindVar("$envmaptint", nullptr)->SetVecValue(colors.envmap_r, colors.envmap_g, colors.envmap_b);

    // Setup wireframe and ignorez using material vars
    mat->SetMaterialVarFlag(MATERIAL_VAR_IGNOREZ, ignorez);
    mat->SetMaterialVarFlag(MATERIAL_VAR_WIREFRAME, wireframe);

    // Override
    g_IVModelRender->ForcedMaterialOverride(mat);

    // Apply our new material
    recurse ? RenderChamsRecursive(entity, mat, this_, state, info, bone) : original::DrawModelExecute(this_, state, info, bone);
    if (overlay)
    {
        // Use white if no color was supplied
        if (colors.rgba_overlay == colors::empty && entity && IDX_GOOD(entity->entindex()))
        {
            CachedEntity *ent = ENTITY(entity->entindex());
            if (ent->m_Type() != ENTITY_PLAYER && ent->m_Type() != ENTITY_PROJECTILE && ent->m_Type() != ENTITY_BUILDING)
                colors.rgba_overlay = colors::white;
            else
                colors.rgba_overlay = ent->m_iTeam() == TEAM_RED ? *chams_overlay_color_red : ent->m_iTeam() == TEAM_BLU ? *chams_overlay_color_blu : colors::white;
        }
        // Setup material
        g_IVRenderView->SetColorModulation(colors.rgba_overlay);
        g_IVRenderView->SetBlend((colors.rgba_overlay).a);

        static auto &mat_overlay = mats.mat_dme_lit_overlay;
        if (envmap && envmap_tint)
            mat_overlay->FindVar("$envmaptint", nullptr)->SetVecValue(colors.envmap_r, colors.envmap_g, colors.envmap_b);

        mat_overlay->SetMaterialVarFlag(MATERIAL_VAR_IGNOREZ, ignorez);
        mat_overlay->AlphaModulate((colors.rgba_overlay).a);

        // Override and apply
        g_IVModelRender->ForcedMaterialOverride(mat_overlay);
        recurse ? RenderChamsRecursive(entity, mat, this_, state, info, bone) : original::DrawModelExecute(this_, state, info, bone);
    }
}

DEFINE_HOOKED_METHOD(DrawModelExecute, void, IVModelRender *this_, const DrawModelState_t &state, const ModelRenderInfo_t &info, matrix3x4_t *bone)
{
    if (!isHackActive() || effect_glow::g_EffectGlow.drawing || chams_attachment_drawing || (*clean_screenshots && g_IEngine->IsTakingScreenshot()) || CE_BAD(LOCAL_E) || (!enable && !no_hats && !no_arms && !blend_zoom && float(nightmode) == 0.0f && !(hacks::tf2::backtrack::chams && hacks::tf2::backtrack::isBacktrackEnabled)))
        return original::DrawModelExecute(this_, state, info, bone);

    PROF_SECTION(DrawModelExecute);

    if (!init_mat)
    {
        const char *cubemap_str = *envmap_matt ? "effects/saxxy/saxxy_gold" : "env_cubemap";
        {
            auto *kv = new KeyValues("UnlitGeneric");
            kv->SetString("$basetexture", "white");
            mats.mat_dme_unlit.Init("__cathook_dme_chams_unlit", kv);
        }
        KeyValues *kv_vertex_lit = nullptr;
        {
            auto *kv = new KeyValues("VertexLitGeneric");
            kv->SetString("$basetexture", "white");
            kv->SetString("$bumpmap", "water/tfwater001_normal");
            kv->SetString("$lightwarptexture", "models/player/pyro/pyro_lightwarp");
            kv->SetBool("$halfambert", *halfambert);
            kv->SetBool("$phong", *phong_enable);
            kv->SetFloat("$phongexponent", *phong_exponent);
            kv->SetFloat("$phongboost", *phong_boost);
            if (phong_fresnelrange)
            {
                char buffer[100];
                snprintf(buffer, 100, "[%.2f %.2f %.2f]", *phong_fresnelrange_1, *phong_fresnelrange_2, *phong_fresnelrange_3);
                kv->SetString("$phongfresnelranges", buffer);
            }
            if (envmap)
            {
                kv->SetString("$envmap", cubemap_str);
                kv->SetFloat("$envmapfresnel", *envmapfresnel);
                kv->SetString("$envmapfresnelminmaxexp", "[0.01 1 2]");
                kv->SetInt("$normalmapalphaenvmapmask", 1);
                kv->SetInt("$selfillum", 1);
                if (envmap_tint)
                    kv->SetString("$envmaptint", "[1 1 1]");
            }
            kv->SetBool("$rimlight", *rimlighting);
            kv->SetFloat("$rimlightexponent", *rimlighting_exponent);
            kv->SetFloat("$rimlightboost", *phong_boost);
            kv_vertex_lit = kv->MakeCopy();
            mats.mat_dme_lit.Init("__cathook_dme_chams_lit", kv);
        }
        {
            auto *kv = new KeyValues("UnlitGeneric");
            kv->SetString("$basetexture", "white");
            mats.mat_dme_unlit_overlay_base.Init("__cathook_dme_chams_lit_overlay_base", kv);
        }
        {
            auto *kv = kv_vertex_lit;
            kv->SetInt("$additive", *additive);
            kv->SetInt("$pearlescent", *pearlescent);
            kv->SetBool("$flat", false);
            mats.mat_dme_lit_overlay.Init("__cathook_dme_chams_lit_overlay", kv);
        }
        KeyValues *kv_vertex_lit_fp = nullptr;
        {
            auto *kv = new KeyValues("VertexLitGeneric");
            kv->SetString("$basetexture", "white");
            kv->SetString("$bumpmap", "water/tfwater001_normal");
            kv->SetString("$lightwarptexture", "models/player/pyro/pyro_lightwarp");
            kv->SetBool("$halfambert", *halfambert);
            kv->SetBool("$phong", *phong_enable);
            kv->SetFloat("$phongexponent", *phong_exponent);
            kv->SetFloat("$phongboost", *phong_boost);
            if (phong_fresnelrange)
            {
                char buffer[100];
                snprintf(buffer, 100, "[%.2f %.2f %.2f]", *phong_fresnelrange_1, *phong_fresnelrange_2, *phong_fresnelrange_3);
                kv->SetString("$phongfresnelranges", buffer);
            }
            if (envmap)
            {
                kv->SetString("$envmap", cubemap_str);
                kv->SetFloat("$envmapfresnel", *envmapfresnel);
                kv->SetString("$envmapfresnelminmaxexp", "[0.01 1 2]");
                kv->SetInt("$normalmapalphaenvmapmask", 1);
                kv->SetInt("$selfillum", 1);
                if (envmap_tint)
                    kv->SetString("$envmaptint", "[1 1 1]");
            }
            kv->SetBool("$rimlight", *rimlighting);
            kv->SetFloat("$rimlightexponent", *rimlighting_exponent);
            kv->SetFloat("$rimlightboost", *phong_boost);
            kv_vertex_lit_fp = kv->MakeCopy();
            mats.mat_dme_lit_fp.Init("__cathook_dme_chams_lit_fp", kv);
        }
        {
            auto *kv = new KeyValues("UnlitGeneric");
            kv->SetString("$basetexture", "white");
            mats.mat_dme_unlit_overlay_base_fp.Init("__cathook_dme_chams_lit_overlay_base_fp", kv);
        }
        {
            auto *kv = kv_vertex_lit_fp;
            kv->SetInt("$additive", *additive);
            kv->SetInt("$pearlescent", *pearlescent);
            kv->SetBool("$flat", false);
            mats.mat_dme_lit_overlay_fp.Init("__cathook_dme_chams_lit_overlay_fp", kv);
        }
        init_mat = true;
    }

    if (info.pModel)
    {
        const char *name = g_IModelInfo->GetModelName(info.pModel);
        if (name)
        {
            std::string sname = name;
            if (no_hats && sname.find("player/items") != std::string::npos)
                return;
            if (no_arms && sname.find("arms") != std::string::npos && sname.find("yeti") == std::string::npos)
                return;

            // Night mode
            if (float(nightmode) > 0.0f && sname.find("prop") != std::string::npos)
            {
                rgba_t draw_color = colors::Fade(colors::white, *nightmode_color, (*nightmode / 100.0f) * (PI / 2), 1.0f);
                rgba_t original_color;

                g_IVRenderView->GetColorModulation(original_color);
                original_color.a = g_IVRenderView->GetBlend();

                g_IVRenderView->SetColorModulation(draw_color);
                g_IVRenderView->SetBlend(draw_color.a);

                // Draw with the night mode color
                original::DrawModelExecute(this_, state, info, bone);

                // Reset it!
                g_IVModelRender->ForcedMaterialOverride(nullptr);
                g_IVRenderView->SetColorModulation(original_color);
                g_IVRenderView->SetBlend(original_color.a);
                return;
            }

            // Player, entity and backtrack chams
            if (IDX_GOOD(info.entity_index))
            {
                // Get the internal entity from the index
                CachedEntity *ent = ENTITY(info.entity_index);
                if (ShouldRenderChams(ent))
                {
                    // Get original to restore to later
                    rgba_t original_color;
                    g_IVRenderView->GetColorModulation(original_color);
                    original_color.a = g_IVRenderView->GetBlend();

                    // Player and entity chams
                    if (enable)
                    {
                        // First time has ignorez, 2nd time not
                        for (int i = 1; i >= 0; i--)
                        {
                            if (i && legit)
                                continue;
                            if (!i && singlepass)
                                continue;

                            // Setup colors
                            auto colors   = GetChamColors(RAW_ENT(ent), i);
                            colors.rgba.a = *cham_alpha;

                            // Apply chams according to user settings
                            ApplyChams(colors, *recursive, *render_original, *overlay_chams, i, false, false, RAW_ENT(ent), this_, state, info, bone);
                        }
                    }
                    else
                        original::DrawModelExecute(this_, state, info, bone);
                    // Backtrack chams
                    using namespace hacks::tf2;
                    if (backtrack::chams && backtrack::isBacktrackEnabled)
                    {
                        // TODO: Allow for a fade between the entity's color and a specified color, it would look cool but i'm lazy
                        if (ent->m_bAlivePlayer())
                        {
                            // Get ticks
                            auto good_ticks = backtrack::getGoodTicks(info.entity_index);
                            if (!good_ticks.empty())
                            {
                                // Setup chams according to user settings
                                ChamColors backtrack_colors;
                                backtrack_colors.rgba         = *backtrack::chams_color;
                                backtrack_colors.rgba_overlay = *backtrack::chams_color_overlay;
                                backtrack_colors.envmap_r     = *backtrack::chams_envmap_tint_r;
                                backtrack_colors.envmap_g     = *backtrack::chams_envmap_tint_g;
                                backtrack_colors.envmap_b     = *backtrack::chams_envmap_tint_b;

                                for (int i = 0; i <= *backtrack::chams_ticks; ++i)
                                {
                                    // Can't draw more than we have
                                    if (i >= good_ticks.size())
                                        break;
                                    if (!good_ticks[i].bones.empty())
                                        ApplyChams(backtrack_colors, false, false, *backtrack::chams_overlay, false, *backtrack::chams_wireframe, false, RAW_ENT(ent), this_, state, info, &good_ticks[i].bones[0]);
                                }
                            }
                        }
                    }
                    // Reset it!
                    g_IVModelRender->ForcedMaterialOverride(nullptr);
                    g_IVRenderView->SetColorModulation(original_color);
                    g_IVRenderView->SetBlend(original_color.a);
                    return;
                }
            }
        }
    }
    IClientUnknown *unk = info.pRenderable->GetIClientUnknown();
    if (unk)
    {
        IClientEntity *ent = unk->GetIClientEntity();
        if (ent)
            if (ent->entindex() == spectator_target)
                return;
    }
    // Don't do it when we are trying to enforce backtrack chams
    if (!hacks::tf2::backtrack::isDrawing)
        return original::DrawModelExecute(this_, state, info, bone);
} // namespace hooked_methods
} // namespace hooked_methods
