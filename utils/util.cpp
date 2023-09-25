// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: http://www.viva64.com

#include "util.hpp"
#include "../cheats/ragebot/aim.h"
#include "..\cheats\visuals\player_esp.h"
#include "..\cheats\lagcompensation\animation_system.h"
#include "..\cheats\misc\misc.h"
#include "../cheats/prediction/EnginePrediction.h"
#include <thread>

#define INRANGE(x, a, b) (x >= a && x <= b)  //-V1003
#define GETBITS(x) (INRANGE((x & (~0x20)),'A','F') ? ((x & (~0x20)) - 'A' + 0xA) : (INRANGE(x, '0', '9') ? x - '0' : 0)) //-V1003
#define GETBYTE(x) (GETBITS(x[0]) << 4 | GETBITS(x[1]))

// https://gamesense.pub/fl1pp.1/l3g5nd

namespace util
{
	int epoch_time()
	{
		return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
	}

	uintptr_t find_pattern(const char* module_name, const char* pattern, const char* mask)
	{
		MODULEINFO module_info = {};
		K32GetModuleInformation(GetCurrentProcess(), GetModuleHandleA(module_name), &module_info, sizeof(MODULEINFO));
		const auto address = reinterpret_cast<std::uint8_t*>(module_info.lpBaseOfDll);
		const auto size = module_info.SizeOfImage;
		std::vector < std::pair < std::uint8_t, bool>> signature;
		for (auto i = 0u; mask[i]; i++)
			signature.emplace_back(std::make_pair(pattern[i], mask[i] == 'x'));
		auto ret = std::search(address, address + size, signature.begin(), signature.end(),
			[](std::uint8_t curr, std::pair<std::uint8_t, bool> curr_pattern)
			{
				return (!curr_pattern.second) || curr == curr_pattern.first;
			});
		return ret == address + size ? 0 : std::uintptr_t(ret);
	}

	uint64_t FindSignature(const char* szModule, const char* szSignature)
	{
		MODULEINFO modInfo;
		GetModuleInformation(GetCurrentProcess(), GetModuleHandle(szModule), &modInfo, sizeof(MODULEINFO));

		uintptr_t startAddress = (DWORD)modInfo.lpBaseOfDll; //-V101 //-V220
		uintptr_t endAddress = startAddress + modInfo.SizeOfImage;

		const char* pat = szSignature;
		uintptr_t firstMatch = 0;

		for (auto pCur = startAddress; pCur < endAddress; pCur++)
		{
			if (!*pat)
				return firstMatch;

			if (*(PBYTE)pat == '\?' || *(BYTE*)pCur == GETBYTE(pat))
			{
				if (!firstMatch)
					firstMatch = pCur;

				if (!pat[2])
					return firstMatch;

				if (*(PWORD)pat == '\?\?' || *(PBYTE)pat != '\?')
					pat += 3;
				else
					pat += 2;
			}
			else
			{
				pat = szSignature;
				firstMatch = 0;
			}
		}

		return 0;
	}

	bool visible(const Vector& start, const Vector& end, entity_t* entity, player_t* from)
	{
		trace_t trace;

		Ray_t ray;
		ray.Init(start, end);

		CTraceFilter filter;
		filter.pSkip = from;

		g_ctx.globals.autowalling = true;
		m_trace()->TraceRay(ray, MASK_SHOT_HULL | CONTENTS_HITBOX, &filter, &trace);
		g_ctx.globals.autowalling = false;

		return trace.hit_entity == entity || trace.fraction == 1.0f; //-V550
	}

	bool is_button_down(int code)
	{
		if (code <= KEY_NONE || code >= KEY_MAX)
			return false;

		if (!m_engine()->IsActiveApp())
			return false;

		if (m_engine()->Con_IsVisible())
			return false;

		static auto cl_mouseenable = m_cvar()->FindVar(crypt_str("cl_mouseenable"));

		if (!cl_mouseenable->GetBool())
			return false;

		return m_inputsys()->IsButtonDown((ButtonCode_t)code);
	}

	
	void movement_fix_new(Vector& wish_angle, CUserCmd* m_pcmd)
	{

		Vector Zero(0.0f, 0.0f, 0.0f);

		Vector wish_forward, wish_right, wish_up, cmd_forward, cmd_right, cmd_up;

		auto viewangles = m_pcmd->m_viewangles;
		auto movedata = Vector(m_pcmd->m_forwardmove, m_pcmd->m_sidemove, m_pcmd->m_upmove);
		viewangles.Normalize();

		if (!(g_ctx.local()->m_fFlags() & FL_ONGROUND) && viewangles.z != 0.f)
			movedata.y = 0.f;

		math::angle_vectors(wish_angle, &wish_forward, &wish_right, &wish_up);
		math::angle_vectors(viewangles, &cmd_forward, &cmd_right, &cmd_up);

		const auto v8 = sqrtf(wish_forward.x * wish_forward.x + wish_forward.y * wish_forward.y),
			v10 = sqrtf(wish_right.x * wish_right.x + wish_right.y * wish_right.y),
			v12 = sqrtf(wish_up.z * wish_up.z);

		Vector wish_forward_norm(1.0f / v8 * wish_forward.x, 1.0f / v8 * wish_forward.y, 0.f),
			wish_right_norm(1.0f / v10 * wish_right.x, 1.0f / v10 * wish_right.y, 0.f),
			wish_up_norm(0.f, 0.f, 1.0f / v12 * wish_up.z);

		const auto v14 = sqrtf(cmd_forward.x * cmd_forward.x + cmd_forward.y * cmd_forward.y),
			v16 = sqrtf(cmd_right.x * cmd_right.x + cmd_right.y * cmd_right.y),
			v18 = sqrtf(cmd_up.z * cmd_up.z);

		Vector cmd_forward_norm(1.0f / v14 * cmd_forward.x, 1.0f / v14 * cmd_forward.y, 1.0f / v14 * 0.0f),
			cmd_right_norm(1.0f / v16 * cmd_right.x, 1.0f / v16 * cmd_right.y, 1.0f / v16 * 0.0f),
			cmd_up_norm(0.f, 0.f, 1.0f / v18 * cmd_up.z);

		const auto v22 = wish_forward_norm.x * movedata.x,
			v26 = wish_forward_norm.y * movedata.x,
			v28 = wish_forward_norm.z * movedata.x,
			v24 = wish_right_norm.x * movedata.y,
			v23 = wish_right_norm.y * movedata.y,
			v25 = wish_right_norm.z * movedata.y,
			v30 = wish_up_norm.x * movedata.z,
			v27 = wish_up_norm.z * movedata.z,
			v29 = wish_up_norm.y * movedata.z;

		Vector correct_movement = Zero;

		correct_movement.x = cmd_forward_norm.x * v24 + cmd_forward_norm.y * v23 + cmd_forward_norm.z * v25
			+ (cmd_forward_norm.x * v22 + cmd_forward_norm.y * v26 + cmd_forward_norm.z * v28)
			+ (cmd_forward_norm.y * v30 + cmd_forward_norm.x * v29 + cmd_forward_norm.z * v27);

		correct_movement.y = cmd_right_norm.x * v24 + cmd_right_norm.y * v23 + cmd_right_norm.z * v25
			+ (cmd_right_norm.x * v22 + cmd_right_norm.y * v26 + cmd_right_norm.z * v28)
			+ (cmd_right_norm.x * v29 + cmd_right_norm.y * v30 + cmd_right_norm.z * v27);

		correct_movement.z = cmd_up_norm.x * v23 + cmd_up_norm.y * v24 + cmd_up_norm.z * v25
			+ (cmd_up_norm.x * v26 + cmd_up_norm.y * v22 + cmd_up_norm.z * v28)
			+ (cmd_up_norm.x * v30 + cmd_up_norm.y * v29 + cmd_up_norm.z * v27);

		correct_movement.x = math::clamp(correct_movement.x, -450.f, 450.f);
		correct_movement.y = math::clamp(correct_movement.y, -450.f, 450.f);
		correct_movement.z = math::clamp(correct_movement.z, -320.f, 320.f);

		m_pcmd->m_forwardmove = correct_movement.x;
		m_pcmd->m_sidemove = correct_movement.y;
		m_pcmd->m_upmove = correct_movement.z;
	}

	void MovementFix(Vector& wish_angle, CUserCmd* m_pcmd)
	{
		Vector PureForward, PureRight, PureUp, CurrForward, CurrRight, CurrUp;
		math::angle_vectors(wish_angle, &PureForward, &PureRight, &PureUp);
		math::angle_vectors(m_pcmd->m_viewangles, &CurrForward, &CurrRight, &CurrUp);

		PureForward[2] = PureRight[2] = CurrForward[2] = CurrRight[2] = 0.f;

		auto VectorNormalize = [](Vector& vec)->float {
			float radius = sqrtf(vec.x * vec.x + vec.y * vec.y + vec.z * vec.z);
			float iradius = 1.f / (radius + FLT_EPSILON);

			vec.x *= iradius;
			vec.y *= iradius;
			vec.z *= iradius;

			return radius;
		};
		VectorNormalize(PureForward);
		VectorNormalize(PureRight);
		VectorNormalize(CurrForward);
		VectorNormalize(CurrRight);
		Vector PureWishDir;
		for (auto i = 0u; i < 2; i++)
			PureWishDir[i] = PureForward[i] * m_pcmd->m_forwardmove + PureRight[i] * m_pcmd->m_sidemove;
		PureWishDir[2] = 0.f;

		Vector CurrWishDir;
		for (auto i = 0u; i < 2; i++)
			CurrWishDir[i] = CurrForward[i] * m_pcmd->m_forwardmove + CurrRight[i] * m_pcmd->m_sidemove;
		CurrWishDir[2] = 0.f;

		if (PureWishDir != CurrWishDir) {
			m_pcmd->m_forwardmove = (PureWishDir.x * CurrRight.y - CurrRight.x * PureWishDir.y) / (CurrRight.y * CurrForward.x - CurrRight.x * CurrForward.y);
			m_pcmd->m_sidemove = (PureWishDir.y * CurrForward.x - CurrForward.y * PureWishDir.x) / (CurrRight.y * CurrForward.x - CurrRight.x * CurrForward.y);
		}
	}


	void FixMove(CUserCmd* cmd, Vector& wanted_move, bool a7)
	{
		// Declare and initialize variables
		float move_magnitude = 0.0f;
		float forwardY = 0.0f, forwardZ = 0.0f, forwardX = 0.0f;
		float rightY = 0.0f, rightZ = 0.0f, rightX = 0.0f;
		float upZ = 1.0f;

		// Copy the input wanted_move and view angles
		Vector inputAngles = wanted_move;
		Vector viewAngles = cmd->m_viewangles;

		// Check if wanted_move is the same as the view angles
		if (wanted_move == viewAngles)
			return;

		// Calculate the magnitude of movement
		move_magnitude = std::sqrt((cmd->m_forwardmove * cmd->m_forwardmove) + (cmd->m_sidemove * cmd->m_sidemove));

		// If there's no movement, return
		if (move_magnitude == 0.0f)
			return;

		// Check if the local player's move type is 8 (LADDER) or 9 (NOCLIP)
		if (g_ctx.local()->get_move_type() == 8 || g_ctx.local()->get_move_type() == 9)
			return;

		// Calculate forward, right, and up vectors based on input angles
		Vector forwardVector, rightVector, upVector;
		math::angle_vectors(inputAngles, &forwardVector, &rightVector, &upVector);

		// Extract components of the forward vector
		forwardY = forwardVector.y;
		forwardZ = forwardVector.z;

		// Set upZ to 1.0 if forwardVector.z is zero, otherwise calculate forwardX, forwardY, and forwardXNormalized
		if (forwardVector.z != 0.0f) {
			forwardZ = 0.0f;
			float forwardVectorLength2D = forwardVector.Length2D();
			if (forwardVectorLength2D >= 0.00000011920929f) {
				forwardY = forwardVector.y * (1.0f / forwardVectorLength2D);
				forwardX = forwardVector.x * (1.0f / forwardVectorLength2D);
			}
			else {
				forwardY = 0.0f;
				forwardX = 0.0f;
			}
		}

		// Extract components of the right vector
		rightY = rightVector.y;
		rightZ = rightVector.z;

		// Set rightX and rightZ to components of the right vector
		if (rightVector.z != 0.0) {
			rightZ = 0.0f;
			float rightVectorLength2D = rightVector.Length2D();
			if (rightVectorLength2D < 0.00000011920929f) {
				rightY = 0.0f;
				rightX = 0.0f;
			}
			else {
				rightY = rightVector.y * (1.0f / rightVectorLength2D);
				rightX = rightVector.x * (1.0f / rightVectorLength2D);
			}
		}

		// Set upZ to 0.0 if upVector.z is small
		if (upVector.z < 0.00000011920929f)
			upZ = 0.0f;

		// Handle special case for cmd->m_forward_move if viewAngles.z is 180.0 and a7 is false
		if (viewAngles.z == 180.0 && !a7)
			cmd->m_forwardmove = std::abs(cmd->m_forwardmove);

		// Calculate vectors based on view angles
		math::angle_vectors(viewAngles, &rightVector, &forwardVector, &upVector);

		// Extract components of the right and forward vectors
		rightZ = rightVector.z;
		if (rightVector.z == 0.0f) {
			rightY = rightVector.y;
			rightX = rightVector.x;
		}
		else {
			rightZ = 0.0f;
			float rightVectorLength2D = rightVector.Length2D();
			if (rightVectorLength2D < 0.00000011920929f) {
				rightY = 0.0f;
				rightX = 0.0f;
			}
			else {
				rightX = rightVector.x * (1.0f / rightVectorLength2D);
				rightY = rightVector.y * (1.0f / rightVectorLength2D);
			}
		}

		float forwardZView = forwardVector.z;
		float forwardYView;
		float forwardXView;
		if (forwardVector.z == 0.0f) {
			 forwardYView = forwardVector.y;
			 forwardXView = forwardVector.x;
		}
		else {
			forwardZView = 0.0f;
			float forwardVectorLength2D = forwardVector.Length2D();
			if (forwardVectorLength2D < 0.00000011920929f) {
				 forwardYView = 0.0f;
				 forwardXView = 0.0f;
			}
			else {
				 forwardXView = forwardVector.x * (1.0f / forwardVectorLength2D);
				 forwardYView = forwardVector.y * (1.0f / forwardVectorLength2D);
			}
		}

		// Set upZ to 0.0 if upVector.z is small
		if (upVector.z < 0.00000011920929f)
			upZ = 0.0f;

		// Calculate new movement values for cmd
		float rightMove = rightY * cmd->m_sidemove;
		float forwardMove = forwardX * cmd->m_forwardmove;
		float upMove = upZ * cmd->m_upmove;

		cmd->m_forwardmove = ((((rightMove * rightY) + (rightX * rightX)) + (rightZ * forwardYView)) + (((upMove * rightY) + (forwardMove * rightX)) + (rightMove * forwardZView))) + (((upMove * rightY) + (upMove * rightX)) + (upZ * forwardZView));
		cmd->m_sidemove = ((((rightMove * forwardY) + (rightX * forwardXView)) + (rightZ * forwardZView)) + (((upMove * forwardY) + (forwardMove * forwardXView)) + (rightMove * forwardZView))) + (((upMove * forwardY) + (upMove * forwardXView)) + (upZ * forwardZView));
		cmd->m_upmove = ((((rightMove * 0.0f) + (rightX * 0.0f)) + (rightZ * upZ)) + (((upMove * 0.0f) + (forwardMove * 0.0f)) + (rightMove * upZ))) + (((upMove * 0.0f) + (upMove * 0.0f)) + (upZ * upZ));
	}

	void movement_fix(Vector& wish_angle, CUserCmd* m_pcmd)
	{
		Vector view_fwd, view_right, view_up, cmd_fwd, cmd_right, cmd_up;
		auto viewangles = m_pcmd->m_viewangles;
		viewangles.Normalized();

		math::angle_vectors(wish_angle, &view_fwd, &view_right, &view_up);
		math::angle_vectors(viewangles, &cmd_fwd, &cmd_right, &cmd_up);

		float v8 = sqrtf((view_fwd.x * view_fwd.x) + (view_fwd.y * view_fwd.y));
		float v10 = sqrtf((view_right.x * view_right.x) + (view_right.y * view_right.y));
		float v12 = sqrtf(view_up.z * view_up.z);

		Vector norm_view_fwd((1.f / v8) * view_fwd.x, (1.f / v8) * view_fwd.y, 0.f);
		Vector norm_view_right((1.f / v10) * view_right.x, (1.f / v10) * view_right.y, 0.f);
		Vector norm_view_up(0.f, 0.f, (1.f / v12) * view_up.z);

		float v14 = sqrtf((cmd_fwd.x * cmd_fwd.x) + (cmd_fwd.y * cmd_fwd.y));
		float v16 = sqrtf((cmd_right.x * cmd_right.x) + (cmd_right.y * cmd_right.y));
		float v18 = sqrtf(cmd_up.z * cmd_up.z);

		Vector norm_cmd_fwd((1.f / v14) * cmd_fwd.x, (1.f / v14) * cmd_fwd.y, 0.f);
		Vector norm_cmd_right((1.f / v16) * cmd_right.x, (1.f / v16) * cmd_right.y, 0.f);
		Vector norm_cmd_up(0.f, 0.f, (1.f / v18) * cmd_up.z);

		float v22 = norm_view_fwd.x * m_pcmd->m_forwardmove;
		float v26 = norm_view_fwd.y * m_pcmd->m_forwardmove;
		float v28 = norm_view_fwd.z * m_pcmd->m_forwardmove;
		float v24 = norm_view_right.x * m_pcmd->m_sidemove;
		float v23 = norm_view_right.y * m_pcmd->m_sidemove;
		float v25 = norm_view_right.z * m_pcmd->m_sidemove;
		float v30 = norm_view_up.x * m_pcmd->m_upmove;
		float v27 = norm_view_up.z * m_pcmd->m_upmove;
		float v29 = norm_view_up.y * m_pcmd->m_upmove;

		m_pcmd->m_forwardmove = ((((norm_cmd_fwd.x * v24) + (norm_cmd_fwd.y * v23)) + (norm_cmd_fwd.z * v25))
			+ (((norm_cmd_fwd.x * v22) + (norm_cmd_fwd.y * v26)) + (norm_cmd_fwd.z * v28)))
			+ (((norm_cmd_fwd.y * v30) + (norm_cmd_fwd.x * v29)) + (norm_cmd_fwd.z * v27));

		m_pcmd->m_sidemove = ((((norm_cmd_right.x * v24) + (norm_cmd_right.y * v23)) + (norm_cmd_right.z * v25))
			+ (((norm_cmd_right.x * v22) + (norm_cmd_right.y * v26)) + (norm_cmd_right.z * v28)))
			+ (((norm_cmd_right.x * v29) + (norm_cmd_right.y * v30)) + (norm_cmd_right.z * v27));

		m_pcmd->m_upmove = ((((norm_cmd_up.x * v23) + (norm_cmd_up.y * v24)) + (norm_cmd_up.z * v25))
			+ (((norm_cmd_up.x * v26) + (norm_cmd_up.y * v22)) + (norm_cmd_up.z * v28)))
			+ (((norm_cmd_up.x * v30) + (norm_cmd_up.y * v29)) + (norm_cmd_up.z * v27));

		static auto cl_forwardspeed = m_cvar()->FindVar(crypt_str("cl_forwardspeed"));
		static auto cl_sidespeed = m_cvar()->FindVar(crypt_str("cl_sidespeed"));
		static auto cl_upspeed = m_cvar()->FindVar(crypt_str("cl_upspeed"));

		m_pcmd->m_forwardmove = math::clamp(m_pcmd->m_forwardmove, -cl_forwardspeed->GetFloat(), cl_forwardspeed->GetFloat());

		m_pcmd->m_sidemove = math::clamp(m_pcmd->m_sidemove, -cl_sidespeed->GetFloat(), cl_sidespeed->GetFloat());
		m_pcmd->m_upmove = math::clamp(m_pcmd->m_upmove, -cl_upspeed->GetFloat(), cl_upspeed->GetFloat());

		m_pcmd->m_upmove = sin(DEG2RAD(m_pcmd->m_viewangles.z)) * sin(DEG2RAD(m_pcmd->m_viewangles.x)) * m_pcmd->m_sidemove;
		m_pcmd->m_forwardmove = cos(DEG2RAD(m_pcmd->m_viewangles.z)) * m_pcmd->m_forwardmove + sin(DEG2RAD(m_pcmd->m_viewangles.x)) * sin(DEG2RAD(m_pcmd->m_viewangles.z)) * m_pcmd->m_sidemove;
	}

	unsigned int find_in_datamap(datamap_t* map, const char* name)
	{
		while (map)
		{
			for (auto i = 0; i < map->dataNumFields; ++i)
			{
				if (!map->dataDesc[i].fieldName)
					continue;

				if (!strcmp(name, map->dataDesc[i].fieldName))
					return map->dataDesc[i].fieldOffset;

				if (map->dataDesc[i].fieldType == FIELD_EMBEDDED)
				{
					if (map->dataDesc[i].td)
					{
						unsigned int offset;

						if (offset = find_in_datamap(map->dataDesc[i].td, name))
							return offset;
					}
				}
			}

			map = map->baseMap;
		}

		return 0;
	}

	bool get_bbox(entity_t* e, Box& box, bool player_esp)
	{
		auto collideable = e->GetCollideable();
		auto m_rgflCoordinateFrame = e->m_rgflCoordinateFrame();

		auto min = collideable->OBBMins();
		auto max = collideable->OBBMaxs();

		Vector points[8] =
		{
			Vector(min.x, min.y, min.z),
			Vector(min.x, max.y, min.z),
			Vector(max.x, max.y, min.z),
			Vector(max.x, min.y, min.z),
			Vector(max.x, max.y, max.z),
			Vector(min.x, max.y, max.z),
			Vector(min.x, min.y, max.z),
			Vector(max.x, min.y, max.z)
		};

		Vector pointsTransformed[8];

		for (auto i = 0; i < 8; i++)
			math::vector_transform(points[i], m_rgflCoordinateFrame, pointsTransformed[i]);

		Vector pos = e->GetAbsOrigin();
		Vector flb;
		Vector brt;
		Vector blb;
		Vector frt;
		Vector frb;
		Vector brb;
		Vector blt;
		Vector flt;

		auto bFlb = math::world_to_screen(pointsTransformed[3], flb);
		auto bBrt = math::world_to_screen(pointsTransformed[5], brt);
		auto bBlb = math::world_to_screen(pointsTransformed[0], blb);
		auto bFrt = math::world_to_screen(pointsTransformed[4], frt);
		auto bFrb = math::world_to_screen(pointsTransformed[2], frb);
		auto bBrb = math::world_to_screen(pointsTransformed[1], brb);
		auto bBlt = math::world_to_screen(pointsTransformed[6], blt);
		auto bFlt = math::world_to_screen(pointsTransformed[7], flt);

		if (!bFlb && !bBrt && !bBlb && !bFrt && !bFrb && !bBrb && !bBlt && !bFlt)
			return false;

		Vector arr[8] =
		{
			flb,
			brt,
			blb,
			frt,
			frb,
			brb,
			blt,
			flt
		};

		auto left = flb.x;
		auto top = flb.y;
		auto right = flb.x;
		auto bottom = flb.y;

		for (auto i = 1; i < 8; i++)
		{
			if (left > arr[i].x)
				left = arr[i].x;
			if (top < arr[i].y)
				top = arr[i].y;
			if (right < arr[i].x)
				right = arr[i].x;
			if (bottom > arr[i].y)
				bottom = arr[i].y;
		}

		box.x = left;
		box.y = bottom;
		box.w = right - left;
		box.h = top - bottom;

		return true;
	}

	void trace_line(Vector& start, Vector& end, unsigned int mask, CTraceFilter* filter, CGameTrace* tr)
	{
		Ray_t ray;
		ray.Init(start, end);

		m_trace()->TraceRay(ray, mask, filter, tr);
	}

	void clip_trace_to_players(IClientEntity* e, const Vector& start, const Vector& end, unsigned int mask, CTraceFilter* filter, CGameTrace* tr)
	{
		Vector mins = e->GetCollideable()->OBBMins(), maxs = e->GetCollideable()->OBBMaxs();

		Vector dir(end - start);
		dir.Normalize();

		Vector
			center = (maxs + mins) / 2,
			pos(center + e->GetAbsOrigin());

		Vector to = pos - start;
		float range_along = dir.Dot(to);

		float range;
		if (range_along < 0.f)
			range = -to.Length();

		else if (range_along > dir.Length())
			range = -(pos - end).Length();

		else {
			auto ray(pos - (dir * range_along + start));
			range = ray.Length();
		}

		if (range <= 60.f) {
			trace_t trace;

			Ray_t ray;
			ray.Init(start, end);

			m_trace()->ClipRayToEntity(ray, mask, e, &trace);

			if (tr->fraction > trace.fraction)
				*tr = trace;
		}
	}

	void RotateMovement(CUserCmd* cmd, float yaw)
	{
		Vector viewangles;
		m_engine()->GetViewAngles(viewangles);

		float rotation = DEG2RAD(viewangles.y - yaw);

		float cos_rot = cos(rotation);
		float sin_rot = sin(rotation);

		float new_forwardmove = cos_rot * cmd->m_forwardmove - sin_rot * cmd->m_sidemove;
		float new_sidemove = sin_rot * cmd->m_forwardmove + cos_rot * cmd->m_sidemove;

		cmd->m_forwardmove = new_forwardmove;
		cmd->m_sidemove = new_sidemove;
	}

	void color_modulate(float color[3], IMaterial* material)
	{
		auto found = false;
		auto var = material->FindVar(crypt_str("$envmaptint"), &found);

		if (found)
			var->set_vec_value(color[0], color[1], color[2]);

		m_renderview()->SetColorModulation(color[0], color[1], color[2]);
	}

	bool get_backtrack_matrix(player_t* e, matrix3x4_t* matrix)
	{
		if (!g_cfg.ragebot.enable)
			return false;

		auto nci = m_engine()->GetNetChannelInfo();

		if (!nci)
			return false;

		auto i = e->EntIndex();

		if (i < 1 || i > 64)
			return false;

		auto records = &player_records[i]; //-V826

		if (records->size() < 2)
			return false;

		for (auto record = records->rbegin(); record != records->rend(); ++record)
		{
			if (!record->valid())
				continue;

			if (record->velocity.Length2D() < 0.1f)
				continue;

			if (record->origin.DistTo(e->GetAbsOrigin()) < 1.0f)
				return false;

			auto curtime = m_globals()->m_curtime;
			auto range = 0.2f;

			if (g_ctx.local()->is_alive())
				curtime = TICKS_TO_TIME(g_ctx.globals.fixed_tickbase);

			auto next_record = record + 1;
			auto end = next_record == records->rend();

			auto next = end ? e->GetAbsOrigin() : next_record->origin;
			auto time_next = end ? e->m_flSimulationTime() : next_record->simulation_time;

			auto correct = nci->GetLatency(FLOW_OUTGOING) + nci->GetLatency(FLOW_INCOMING) + util::get_interpolation();
			auto time_delta = time_next - record->simulation_time;

			auto add = end ? range : time_delta;
			auto deadtime = record->simulation_time + correct + add;
			auto delta = deadtime - curtime;

			auto mul = 1.0f / add;
			auto lerp = math::lerp(next, record->origin, math::clamp(delta * mul, 0.0f, 1.0f));

			matrix3x4_t result[MAXSTUDIOBONES];
			memcpy(result, record->m_Matricies[MatrixBoneSide::MiddleMatrix].data(), MAXSTUDIOBONES * sizeof(matrix3x4_t));

			for (auto j = 0; j < MAXSTUDIOBONES; j++)
			{
				auto matrix_delta = math::matrix_get_origin(record->m_Matricies[MatrixBoneSide::MiddleMatrix].data()[j]) - record->origin;
				math::matrix_set_origin(matrix_delta + lerp, result[j]);
			}

			memcpy(matrix, result, MAXSTUDIOBONES * sizeof(matrix3x4_t));
			return true;
		}

		return false;
	}

	void create_state(c_baseplayeranimationstate* state, player_t* e)
	{
		using Fn = void(__thiscall*)(c_baseplayeranimationstate*, player_t*);
		static auto fn = reinterpret_cast <Fn> (util::FindSignature(crypt_str("client.dll"), crypt_str("55 8B EC 56 8B F1 B9 ? ? ? ? C7 46")));

		fn(state, e);
	}

	void create_state1(C_CSGOPlayerAnimationState* state, player_t* e)
	{
		using Fn = void(__thiscall*)(C_CSGOPlayerAnimationState*, player_t*);
		static auto fn = reinterpret_cast <Fn> (util::FindSignature(crypt_str("client.dll"), crypt_str("55 8B EC 56 8B F1 B9 ? ? ? ? C7 46")));

		fn(state, e);
	}

	void update_state(c_baseplayeranimationstate* state, const Vector& angles)
	{
		using Fn = void(__vectorcall*)(void*, void*, float, float, float, void*);
		static auto fn = reinterpret_cast <Fn> (util::FindSignature(crypt_str("client.dll"), crypt_str("55 8B EC 83 E4 F8 83 EC 18 56 57 8B F9 F3 0F 11 54 24")));

		fn(state, nullptr, 0.0f, angles[1], angles[0], nullptr);
	}

	void update_state1(C_CSGOPlayerAnimationState* state, const Vector& angles)
	{
		using Fn = void(__vectorcall*)(void*, void*, float, float, float, void*);
		static auto fn = reinterpret_cast <Fn> (util::FindSignature(crypt_str("client.dll"), crypt_str("55 8B EC 83 E4 F8 83 EC 18 56 57 8B F9 F3 0F 11 54 24")));

		fn(state, nullptr, 0.0f, angles[1], angles[0], nullptr);
	}

	void reset_state(c_baseplayeranimationstate* state)
	{
		using Fn = void(__thiscall*)(c_baseplayeranimationstate*);
		static auto fn = reinterpret_cast <Fn> (util::FindSignature(crypt_str("client.dll"), crypt_str("56 6A 01 68 ? ? ? ? 8B F1")));

		fn(state);
	}

	void copy_command(CUserCmd* cmd, int tickbase_shift)
	{
		/*static auto cl_forwardspeed = m_cvar()->FindVar(crypt_str("cl_forwardspeed"));
		static auto cl_sidespeed = m_cvar()->FindVar(crypt_str("cl_sidespeed"));

		if (key_binds::get().get_key_bind_state(18))
			cmd->m_sidemove = abs(cmd->m_sidemove) > 10.f ? copysignf(450.f, cmd->m_sidemove) : 0.f;
		else
		{
			if (g_cfg.ragebot.instant_double_tap)
			{
				Vector vMove(cmd->m_forwardmove, cmd->m_sidemove, cmd->m_upmove);
				float flSpeed = sqrt(vMove.x * vMove.x + vMove.y * vMove.y), flYaw;
				Vector vMove2;
				math::VectorAngles(vMove, vMove2);
				vMove2.Normalize();
				flYaw = DEG2RAD(cmd->m_viewangles.y - g_ctx.globals.original.y + vMove2.y);
				cmd->m_forwardmove = cos(flYaw) * flSpeed;
				cmd->m_sidemove = sin(flYaw) * flSpeed;
			}
			else if (g_cfg.ragebot.instant_double_tap && key_binds::get().get_key_bind_state(18))
				cmd->m_sidemove = abs(cmd->m_sidemove) > 15.f ? copysignf(450.f, cmd->m_sidemove) : 0.f;
			else
			{
				cmd->m_forwardmove = 0.0f;
				cmd->m_sidemove = 0.0f;
			}
		}*/

		auto commands_to_add = 0;

		do {
			auto sequence_number = commands_to_add + cmd->m_command_number;

			auto command = m_input()->GetUserCmd(sequence_number);
			auto verified_command = m_input()->GetVerifiedUserCmd(sequence_number);

			memcpy(command, cmd, sizeof(CUserCmd));

			if (command->m_tickcount != INT_MAX && m_clientstate()->iDeltaTick > 0)
				m_prediction()->Update(m_clientstate()->iDeltaTick, true, m_clientstate()->nLastCommandAck, m_clientstate()->nLastOutgoingCommand + m_clientstate()->iChokedCommands);

			command->m_command_number = sequence_number;
			command->m_predicted = command->m_tickcount != INT_MAX;

			if (m_clientstate()->pNetChannel && (commands_to_add + 1) < tickbase_shift) {
				++m_clientstate()->pNetChannel->m_nChokedPackets;
				++m_clientstate()->pNetChannel->m_nOutSequenceNr;
				++m_clientstate()->iChokedCommands;
			}

			auto v64 = (g_ctx.local()->m_flDuckAmount() * 0.34f) + 1.0f;
			auto v47 = 1.0f / (v64 - g_ctx.local()->m_flDuckAmount());

			command->m_forwardmove = command->m_forwardmove * v47;
			command->m_sidemove = v47 * command->m_sidemove;

			if (command->m_sidemove > 450.f)
				command->m_sidemove = 450.f;
			else if (command->m_sidemove < -450.f)
				command->m_sidemove = -450.f;

			if (command->m_forwardmove > 450.f)
				command->m_forwardmove = 450.f;
			else if (command->m_forwardmove < -450.f)
				command->m_forwardmove = -450.f;

			movement_fix(command->m_viewangles, command);

			math::normalize_angles(command->m_viewangles);

			memcpy(&verified_command->m_cmd, command, sizeof(CUserCmd));
			verified_command->m_crc = command->GetChecksum();

			++commands_to_add;
		} while (commands_to_add != tickbase_shift);

		*(int*)((uintptr_t)m_prediction() + 0xC) = -1;
		*(int*)((uintptr_t)m_prediction() + 0x1C) = 0;

		//*(bool*)((uintptr_t)m_prediction() + 0x24) = true;
		//*(int*)((uintptr_t)m_prediction() + 0x1C) = 0;
	}

	float get_interpolation()
	{
		float updaterate = m_cvar()->FindVar("cl_updaterate")->GetFloat();
		auto minupdate = m_cvar()->FindVar("sv_minupdaterate");
		auto maxupdate = m_cvar()->FindVar("sv_maxupdaterate");

		if (minupdate && maxupdate)
			updaterate = maxupdate->GetFloat();

		float ratio = m_cvar()->FindVar("cl_interp_ratio")->GetFloat();

		if (ratio == 0)
			ratio = 1.0f;

		float lerp = m_cvar()->FindVar("cl_interp")->GetFloat();
		auto cmin = m_cvar()->FindVar("sv_client_min_interp_ratio");
		auto cmax = m_cvar()->FindVar("sv_client_max_interp_ratio");

		if (cmin && cmax && cmin->GetFloat() != 1)
			ratio = math::clamp(ratio, cmin->GetFloat(), cmax->GetFloat());

		return max(lerp, ratio / updaterate);
	}

	bool is_valid_hitgroup(int index)
	{
		/*if ((index >= HITGROUP_HEAD && index <= HITGROUP_RIGHTLEG) || index == HITGROUP_GEAR)
			return true;

		return false;*/
		return true;
	}

	bool is_breakable_entity(IClientEntity* e)
	{
		if (!e || !e->EntIndex())
			return false;
		static auto is_breakable = util::FindSignature(crypt_str("client.dll"), crypt_str("55 8B EC 51 56 8B F1 85 F6 74 68"));

		auto take_damage = *(uintptr_t*)((uintptr_t)is_breakable + 0x26);
		auto backup = *(uint8_t*)((uintptr_t)e + take_damage);

		auto client_class = e->GetClientClass();
		auto network_name = client_class->m_pNetworkName;

		if (!strcmp(network_name, crypt_str("CBreakableSurface")))
			*(uint8_t*)((uintptr_t)e + take_damage) = DAMAGE_YES;
		else if (!strcmp(network_name, crypt_str("CBaseDoor")) || !strcmp(network_name, crypt_str("CDynamicProp")))
			*(uint8_t*)((uintptr_t)e + take_damage) = DAMAGE_NO;

		using Fn = bool(__thiscall*)(IClientEntity*);
		auto result = ((Fn)is_breakable)(e);

		*(uint8_t*)((uintptr_t)e + take_damage) = backup;
		return result;
	}

	int get_hitbox_by_hitgroup(int index)
	{
		switch (index)
		{
		case HITGROUP_HEAD:
			return HITBOX_HEAD;
		case HITGROUP_CHEST:
			return HITBOX_CHEST;
		case HITGROUP_STOMACH:
			return HITBOX_STOMACH;
		case HITGROUP_LEFTARM:
			return HITBOX_LEFT_HAND;
		case HITGROUP_RIGHTARM:
			return HITBOX_RIGHT_HAND;
		case HITGROUP_LEFTLEG:
			return HITBOX_RIGHT_CALF;
		case HITGROUP_RIGHTLEG:
			return HITBOX_LEFT_CALF;
		default:
			return HITBOX_PELVIS;
		}
	}
}