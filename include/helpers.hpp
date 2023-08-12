/*
 * helpers.h
 *
 *  Created on: Oct 8, 2016
 *      Author: nullifiedcat
 */

#pragma once

class CachedEntity;
class IClientEntity;
class ConVar;
class ConCommand;
class CUserCmd;
class CCommand;
struct player_info_s;
class Vector;

class ICvar;
void SetCVarInterface(ICvar *iface);

#define PI 3.14159265358979323846f
#define RADPI 57.295779513082f
//#define DEG2RAD(x) (float)(x) * (float)(PI / 180.0f)

#include <enums.hpp>
#include <conditions.hpp>
#include <entitycache.hpp>
#include <core/logging.hpp>

#include <string>
#include <sstream>
#include <vector>
#include <mutex>
#include <random>
#include <set>

#include <core/sdk.hpp>

#include "ProxyFnHook.hpp"

#define TICK_INTERVAL (g_GlobalVars->interval_per_tick)
#define TIME_TO_TICKS(dt) ((int) (0.5f + (float) (dt) / TICK_INTERVAL))
#define TICKS_TO_TIME(t) (TICK_INTERVAL * (t))
#define ROUND_TO_TICKS(t) (TICK_INTERVAL * TIME_TO_TICKS(t))
// Validate a UserCmd
#define VERIFIED_CMD_SIZE 90

// typedef void ( *FnCommandCallback_t )( const CCommand &command );

template <typename Iter, typename RandomGenerator> Iter select_randomly(Iter start, Iter end, RandomGenerator &g)
{
    std::uniform_int_distribution<> dis(0, std::distance(start, end) - 1);
    std::advance(start, dis(g));
    return start;
}

template <typename Iter> Iter select_randomly(Iter start, Iter end)
{
    static std::random_device rd;
    static std::mt19937 gen(rd());
    return select_randomly(start, end, gen);
}

// TODO split this shit

std::vector<ConVar *> &RegisteredVarsList();
std::vector<ConCommand *> &RegisteredCommandsList();

void BeginConVars();
void EndConVars();

// Calling source engine functions would get me more accurate result at cost of
// speed, so idk.
// TODO?

bool IsPlayerInvulnerable(CachedEntity *player);
bool IsPlayerCritBoosted(CachedEntity *player);
bool IsPlayerInvisible(CachedEntity *player);
bool IsPlayerDisguised(CachedEntity *player);
bool IsPlayerResistantToCurrentClass(CachedEntity *player, bool smallres = false);
bool IsPlayerResistantToCurrentWeapon(CachedEntity *player, bool smallres = false);

const char *GetBuildingName(CachedEntity *ent);
Vector GetBuildingPosition(CachedEntity *ent);
bool IsBuildingVisible(CachedEntity *ent);

ConVar *CreateConVar(std::string name, std::string value, std::string help);
ConCommand *CreateConCommand(const char *name, FnCommandCallback_t callback, const char *help);

powerup_type GetPowerupOnPlayer(CachedEntity *player);
// GetHitbox() is being called really frequently.
// It's better if it won't create a new object each time it gets called.
// So it returns a success state, and the values are stored in out reference.
bool GetHitbox(CachedEntity *entity, int hb, Vector &out);
weaponmode GetWeaponMode();
weaponmode GetWeaponMode(CachedEntity *ent);

void FixMovement(CUserCmd &cmd, Vector &viewangles);
void VectorAngles(Vector &forward, Vector &angles);
void AngleVectors2(const QAngle &angles, Vector *forward);
void AngleVectors3(const QAngle &angles, Vector *forward, Vector *right, Vector *up);
extern std::mutex trace_lock;
bool IsEntityVisible(CachedEntity *entity, int hb);
bool IsEntityVectorVisible(CachedEntity *entity, Vector endpos, bool use_weapon_offset = false, unsigned int mask = MASK_SHOT_HULL, trace_t *trace = nullptr);
bool VisCheckEntFromEnt(CachedEntity *startEnt, CachedEntity *endEnt);
bool VisCheckEntFromEntVector(Vector startVector, CachedEntity *startEnt, CachedEntity *endEnt);
Vector VischeckCorner(CachedEntity *player, CachedEntity *target, float maxdist, bool checkWalkable);
std::pair<Vector, Vector> VischeckWall(CachedEntity *player, CachedEntity *target, float maxdist, bool checkWalkable);
float vectorMax(Vector i);
Vector vectorAbs(Vector i);
bool canReachVector(Vector loc, Vector dest = { 0, 0, 0 });

bool LineIntersectsBox(Vector &bmin, Vector &bmax, Vector &lmin, Vector &lmax);

float DistToSqr(CachedEntity *entity);
void fClampAngle(Vector &qaAng);
// const char* MakeInfoString(IClientEntity* player);
bool GetProjectileData(CachedEntity *weapon, float &speed, float &gravity, float &start_velocity);
bool IsVectorVisible(const Vector &a, const Vector &b, bool enviroment_only = false, unsigned mask = MASK_SHOT_HULL);
// A Special function for navparser to check if a Vector is visible.
bool IsVectorVisibleNavigation(Vector a, Vector b, unsigned int mask = MASK_SHOT_HULL);
Vector GetForwardVector(Vector origin, Vector viewangles, float distance);
Vector GetForwardVector(float distance);
CachedEntity *getClosestNonlocalEntity(Vector vec);
bool IsSentryBuster(CachedEntity *ent);
// TODO move that to weaponid.h
bool HasWeapon(CachedEntity *ent, int wantedId);
bool IsAmbassador(CachedEntity *ent);
bool AmbassadorCanHeadshot();

// Convert a TF2 handle into an IDX -> ENTITY(IDX)
int HandleToIDX(int handle);

inline const char *teamname(int team)
{
    if (team == 2)
        return "RED";
    else if (team == 3)
        return "BLU";
    return "SPEC";
}
extern const std::string classes[10];
inline const char *classname(int clazz)
{
    if (clazz > 0 && clazz < 10)
    {
        return classes[clazz - 1].c_str();
    }
    return "Unknown";
}

void PrintChat(const char *fmt, ...);
void TriggerNameChange(const char *name);
void ChangeName(std::string name);

void WhatIAmLookingAt(int *result_eindex, Vector *result_pos);
Vector getShootPos(Vector angle);

inline Vector GetAimAtAngles(Vector origin, Vector target, CachedEntity *punch_correct = nullptr)
{
    Vector angles, tr;
    tr = (target - origin);
    VectorAngles(tr, angles);
    // Apply punchangle correction
    if (punch_correct)
        angles -= CE_VECTOR(punch_correct, netvar.vecPunchAngle);
    fClampAngle(angles);
    return angles;
}

void AimAt(Vector origin, Vector target, CUserCmd *cmd);
void AimAtHitbox(CachedEntity *ent, int hitbox, CUserCmd *cmd);
bool IsProjectileCrit(CachedEntity *ent);
void FastStop();

QAngle VectorToQAngle(Vector in);
Vector QAngleToVector(QAngle in);

bool CanHeadshot();
bool CanShoot();

char GetUpperChar(ButtonCode_t button);
char GetChar(ButtonCode_t button);

bool IsEntityVisiblePenetration(CachedEntity *entity, int hb);

// void RunEnginePrediction(IClientEntity* ent, CUserCmd *ucmd = NULL);
// void StartPrediction(CUserCmd* cmd);
// void EndPrediction();

float RandFloatRange(float min, float max);
int UniformRandomInt(int min, int max);

Vector CalcAngle(Vector src, Vector dst);
void MakeVector(Vector ang, Vector &out);
float GetFov(Vector ang, Vector src, Vector dst);

void ReplaceString(std::string &input, const std::string &what, const std::string &with_what);
size_t ReplaceString(char *str, size_t str_capacity, const char *what, const char *with_what);
size_t ReplaceSpecials(char *str);
void ReplaceSpecials(std::string &str);
std::size_t RemoveChars(char *str, const char *chars, std::size_t len = ~0U);
void ReplaceChars(char *str, const char *what, const char *with_what);

void StringToLower(char *dest, const char *src);
void StringToUpper(char *dest, const char *src);

char *CStringDuplicate(const char *str);

std::pair<float, float> ComputeMove(const Vector &a, const Vector &b);
void WalkTo(const Vector &vector);

std::string GetLevelName();

void format_internal(std::stringstream &stream);
template <typename T, typename... Targs> void format_internal(std::stringstream &stream, T value, Targs... args)
{
    stream << value;
    format_internal(stream, args...);
}
template <typename... Args> std::string format(const Args &... args)
{
    std::stringstream stream;
    format_internal(stream, args...);
    return stream.str();
}
CSteamID CSteamIDFrom32(uint32_t id32);

extern const std::string classes[10];
extern const char *powerups[POWERUP_COUNT];

int SharedRandomInt(unsigned iseed, const char *sharedname, int iMinVal, int iMaxVal, int additionalSeed /*=0*/);
float ATTRIB_HOOK_FLOAT(float base_value, const char *search_string, IClientEntity *ent, void *buffer, bool is_global_const_string);

std::unique_ptr<char[]> format_cstr(const char *fmt, ...);

bool isRapidFire(IClientEntity *wep);
int getWeaponByID(CachedEntity *player, int weaponid);
bool HookNetvar(std::vector<std::string> path, ProxyFnHook &hook, RecvVarProxyFn function);
