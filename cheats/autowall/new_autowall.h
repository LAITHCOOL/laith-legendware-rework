#pragma once
#include "../../includes.hpp"

struct SimulateBulletObject_t
{
	Vector vecPosition = { };
	Vector vecDirection = { };
	trace_t enterTrace = { };
	float flCurrentDamage = 0.0f;
	int iPenetrateCount = 0;
};

// @credits: outlassn
class c_newautowall
{
	/* @section: get */
	/// @param[in] vecPoint another player's point to get damage on
	/// @param[out] pDataOut [optional] simulated fire bullet data output
	/// @returns: damage at given point from given player shoot position
	float GetDamage(player_t* pAttacker, const Vector& vecPoint, SimulateBulletObject_t* pDataOut = nullptr);
	/// scales given damage by various dependent factors
	/// @param[in,out] pflDamageToScale damage value that being scaled
	void ScaleDamage(const int iHitGroup, player_t* pCSPlayer, const float flWeaponArmorRatio, const float flWeaponHeadShotMultiplier, float* pflDamageToScale);
	/// simulates fire bullet to penetrate up to 4 walls
	/// @returns: true if simulated bullet hit the player, false otherwise
	bool SimulateFireBullet(player_t* pAttacker, weapon_t* pWeapon, SimulateBulletObject_t& data);

	/* @section: main */
	/// find exact penetration exit
	/// @returns: true if successfully got penetration exit for current object, false otherwise
	bool TraceToExit(const trace_t& enterTrace, trace_t& exitTrace, const Vector& vecPosition, const Vector& vecDirection, const IHandleEntity* pClipPlayer);
	/// process bullet penetration to count penetrated objects it hits
	/// @returns: true if bullet stopped and we should stop processing penetration, false otherwise
	bool HandleBulletPenetration(player_t* pLocal, const weapon_info_t* pWeaponData, const surfacedata_t* pEnterSurfaceData, SimulateBulletObject_t& data);
}
