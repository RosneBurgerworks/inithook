/*
 * entityhitboxcache.cpp
 *
 *  Created on: Dec 23, 2016
 *      Author: nullifiedcat
 */

#include <settings/Int.hpp>
#include "common.hpp"

namespace hitbox_cache
{

EntityHitboxCache::EntityHitboxCache() : parent_ref(&entity_cache::Get(((unsigned) this - (unsigned) &hitbox_cache::array) / sizeof(EntityHitboxCache)))
{
    Reset();
}

int EntityHitboxCache::GetNumHitboxes()
{
    if (!m_bInit)
        Init();
    if (!m_bSuccess)
        return 0;
    return m_nNumHitboxes;
}

// WHY ARE WE PUTTING EMPTY CONSRTRUCTORS EVERYWHERE
// N
// I
// G
// G
// E
// R

EntityHitboxCache::~EntityHitboxCache()
{
}

void EntityHitboxCache::InvalidateCache()
{
    bones_setup = false;
    for (int i = 0; i < CACHE_MAX_HITBOXES; i++)
    {
        m_CacheValidationFlags[i]    = false;
        m_VisCheckValidationFlags[i] = false;
    }
    m_bInit    = false;
    m_bSuccess = false;
}

void EntityHitboxCache::Update()
{
    InvalidateCache();
}

void EntityHitboxCache::Init()
{
    model_t *model;
    studiohdr_t *shdr;
    mstudiohitboxset_t *set;

    m_bInit = true;
    if (CE_BAD(parent_ref))
        return;
    model = (model_t *) RAW_ENT(parent_ref)->GetModel();
    if (!model)
        return;
    if (!m_bModelSet || model != m_pLastModel)
    {
        shdr = g_IModelInfo->GetStudiomodel(model);
        if (!shdr)
            return;
        set = shdr->pHitboxSet(CE_INT(parent_ref, netvar.iHitboxSet));
        if (!dynamic_cast<mstudiohitboxset_t *>(set))
            return;
        m_pLastModel   = model;
        m_nNumHitboxes = 0;
        if (set)
        {
            m_nNumHitboxes = set->numhitboxes;
        }
        if (m_nNumHitboxes > CACHE_MAX_HITBOXES)
            m_nNumHitboxes = CACHE_MAX_HITBOXES;
        m_bModelSet = true;
    }
    m_bSuccess = true;
}

bool EntityHitboxCache::VisibilityCheck(int id)
{
    CachedHitbox *hitbox;

    if (!m_bInit)
        Init();
    if (id < 0 || id >= m_nNumHitboxes)
        return false;
    if (!m_bSuccess)
        return false;
    if (m_VisCheckValidationFlags[id])
        return m_VisCheck[id];
    // TODO corners
    // YEA MORE THAN A FUCKING TODO U FAT CUNT
    hitbox = GetHitbox(id);
    if (!hitbox)
        return false;
    // THIS PROBABLY WONT FUCKING HELP BUT FUCK IT WHY NOT!
    m_VisCheck[id]                = (IsEntityVectorVisible(parent_ref, hitbox->center, true, g_pLocalPlayer->weapon_mode != weapon_melee ? MASK_SHOT : MASK_SHOT_HULL));
    m_VisCheckValidationFlags[id] = true;
    return m_VisCheck[id];
}

// APPARENTLY using 0 makes it p 1234712309472319054273098547320954237509875 (at least for me) ! !  ! TODO: Needs someone else to test
// NA NEVERMIND FUCK IT LETS USE 2 BECAUSE I SAID SO
static settings::Int setupbones_time{ "source.setupbones-time", "2" };
// NOPE NOPE NOPE BONE CACHE SETTING IS FUCKING GONE YOU PIECE OF SHIT

// "so did you add kill davids pc feature?"
static settings::Boolean killdavidspcfeature{ "source.setupbones.killdavidspcfeature", "false" };

// STOP PUTTING USELESS MUTEX'S EVERYWHERE NULLCAT!!!

matrix3x4_t *EntityHitboxCache::GetBones()
{
    static float bones_setup_time = 0.0f;
    switch (*setupbones_time)
    {
    case 0:
        bones_setup_time = 0.0f;
        break;
    case 1:
        bones_setup_time = g_GlobalVars->curtime;
        break;
    case 2:
        if (CE_GOOD(LOCAL_E))
            bones_setup_time = g_GlobalVars->interval_per_tick * CE_INT(LOCAL_E, netvar.nTickBase);
        break;
    case 3:
        if (CE_GOOD(parent_ref))
            bones_setup_time = CE_FLOAT(parent_ref, netvar.m_flSimulationTime);
    }
    if (!bones_setup || killdavidspcfeature /* trolling complete, back to hq */)
    {
        if (g_Settings.is_create_move)
        {
            PROF_SECTION(bone_setup);
            // ALSO, WHAT IN THE NAME OF GOD IS 0x7FF00 !?!?!?!?!?!?!?!? WHY IS THIS USED INSTEAD OF BONE_USED_BY_HITBOX ENUM BUILT INTO THE SDK >?!?!?!?!?!? !?!?!?!? !? !?!?!
            // WHATEVER, ALL (PUBLIC) CHEATS USE THIS RANDOM SIGNED BINARY PIECE OF SHIT, SO THEY CANT BE WRONG (RIGHT!?!?!?!)
            bones_setup = RAW_ENT(parent_ref)->SetupBones(bones, MAXSTUDIOBONES, 0x7FF00, bones_setup_time);
        }
    }
    return bones;
}

void EntityHitboxCache::Reset()
{
    // FUCK YOU! THIS IS FUCKING GROSS!!! sizeof(bool) ARE YOU FUCKING WITH ME ?!?!?!?
    memset(m_VisCheck, 0, sizeof(bool) * CACHE_MAX_HITBOXES);
    memset(m_VisCheckValidationFlags, 0, sizeof(bool) * CACHE_MAX_HITBOXES);
    memset(m_CacheValidationFlags, 0, sizeof(bool) * CACHE_MAX_HITBOXES);
    memset(m_CacheInternal, 0, sizeof(CachedHitbox) * CACHE_MAX_HITBOXES);
    memset(&bones, 0, sizeof(matrix3x4_t) * 128);
    m_nNumHitboxes = 0;
    m_bInit        = false;
    m_bModelSet    = false;
    m_bSuccess     = false;
    m_pLastModel   = nullptr;
    bones_setup    = false;
}

CachedHitbox *EntityHitboxCache::GetHitbox(int id)
{
    if (m_CacheValidationFlags[id])
        return &m_CacheInternal[id];
    mstudiobbox_t *box;

    if (!m_bInit)
        Init();
    if (id < 0 || id >= m_nNumHitboxes)
        return nullptr;
    if (!m_bSuccess)
        return nullptr;
    if (CE_BAD(parent_ref))
        return nullptr;
    auto model = (const model_t *) RAW_ENT(parent_ref)->GetModel();
    if (!model)
        return nullptr;
    auto shdr = g_IModelInfo->GetStudiomodel(model);
    if (!shdr)
        return nullptr;
    auto set = shdr->pHitboxSet(CE_INT(parent_ref, netvar.iHitboxSet));
    if (!dynamic_cast<mstudiohitboxset_t *>(set))
        return nullptr;
    box = set->pHitbox(id);
    if (!box)
        return nullptr;
    if (box->bone < 0 || box->bone >= MAXSTUDIOBONES)
        return nullptr;
    VectorTransform(box->bbmin, GetBones()[box->bone], m_CacheInternal[id].min);
    VectorTransform(box->bbmax, GetBones()[box->bone], m_CacheInternal[id].max);
    m_CacheInternal[id].bbox   = box;
    m_CacheInternal[id].center = (m_CacheInternal[id].min + m_CacheInternal[id].max) / 2;
    m_CacheValidationFlags[id] = true;
    return &m_CacheInternal[id];
}

EntityHitboxCache array[MAX_ENTITIES]{};

void Update()
{
}

void Invalidate()
{
}
} // namespace hitbox_cache
