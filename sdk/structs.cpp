// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: http://www.viva64.com

#include <array>
#include "structs.hpp"
#include "..\cheats\misc\misc.h"
#include "..\cheats\misc\logs.h"
#include "..\cheats\lagcompensation\local_animations.h"
#include "..\cheats\lagcompensation\setup_bones.h"
#include "../cheats/prediction/Networking.h"


Vector& player_t::get_render_angles1()
{
	if (!this)
		return ZERO;

	static auto deadflag = netvars::get().get_offset(crypt_str("CBasePlayer"), crypt_str("deadflag"));
	return *(Vector*)(uintptr_t(this) + (deadflag + 0x4));
}

Vector& player_t::get_abs_angles()
{
	return call_virtual<Vector& (__thiscall*)(void*)>(this, 11)(this);
}

Vector& player_t::get_abs_origin()
{
	if (!this) //-V704
		return ZERO;

	return call_virtual<Vector& (__thiscall*)(void*)>(this, 10)(this);
}

float& player_t::m_flCollisionChangeTime()
{
	return *(float*)((DWORD)(this) + 0x9924);
}

float& player_t::m_flCollisionChangeOrigin()
{
	return *(float*)((DWORD)(this) + 0x9920);
}

void player_t::force_bone_rebuild()
{
	m_BoneAccessor().m_WritableBones = m_BoneAccessor().m_ReadableBones = 0;

	*(int*)(uintptr_t(this) + 0x2924) = 0xFF7FFFFF;
	*(int*)(uintptr_t(this) + 0x2690) = 0;
}


bool player_t::BuildLocalBones(int matrix)
{
	if (!this)
		return false;

	if (IsDormant())
		return false;

	static auto r_jiggle_bones = m_cvar()->FindVar("r_jiggle_bones");

	const auto penis = r_jiggle_bones->GetInt();
	const auto old_origin = get_abs_origin();
	const auto clientsideanim = m_bClientSideAnimation();

	r_jiggle_bones->SetValue(0);

	float bk = FLT_MAX;

	auto previous_weapon = get_animation_state() ? get_animation_state()->m_pLastBoneSetupWeapon : nullptr;

	if (previous_weapon)
		get_animation_state()->m_pLastBoneSetupWeapon = get_animation_state()->m_pActiveWeapon;

	if (this != g_ctx.local()) {
		set_abs_origin(m_vecOrigin());
	}
	else
	{
		if (get_animation_state() != nullptr && get_animation_state()->m_velocity < 0.1f && get_animation_state()->m_bOnGround && sequence_activity(get_animlayers()[3].m_nSequence) == 979)
		{
			bk = get_animlayers()[3].m_flWeight;

			get_animlayers()[3].m_flWeight = 0.f;
		}
	}

	const auto v20 = *(int*)(uintptr_t(this) + 0xA28);
	const auto v19 = *(int*)(uintptr_t(this) + 0xA30);
	const auto v22 = *(uint8_t*)(uintptr_t(this) + 0x68);
	const auto v62 = *(uint8_t*)(uintptr_t(this) + 0x274);

	*(uint8_t*)(uintptr_t(this) + 0x274) = 0;

	const auto effects = m_fEffects();
	m_fEffects() |= 8;

	*(int*)(uintptr_t(this) + 0xA68) = 0;
	*(int*)(uintptr_t(this) + 0xA28) &= ~10u;
	*(int*)(uintptr_t(this) + 0xA30) = 0;

	*(unsigned short*)(uintptr_t(this) + 0x68) |= 2;

	auto realtime_backup = m_globals()->m_realtime;
	auto curtime = m_globals()->m_curtime;
	auto frametime = m_globals()->m_frametime;
	auto absoluteframetime = m_globals()->m_absoluteframetime;
	auto framecount = m_globals()->m_framecount;
	auto tickcount = m_globals()->m_tickcount;
	auto interpolation_amount = m_globals()->m_interpolation_amount;

	float time = (this != g_ctx.local() ? m_flSimulationTime() : TICKS_TO_TIME(m_clientstate()->m_iServerTick));
	int ticks = TIME_TO_TICKS(time);

	m_globals()->m_curtime = time;
	m_globals()->m_realtime = time;
	m_globals()->m_frametime = m_globals()->m_intervalpertick;
	m_globals()->m_absoluteframetime = m_globals()->m_intervalpertick;
	m_globals()->m_framecount = ticks;
	m_globals()->m_tickcount = ticks;
	m_globals()->m_interpolation_amount = 0.f;

	const float weight12 = animlayer_count() < 12 ? -999 : get_animlayers()[12].m_flWeight;
	if (g_ctx.local() != this)
		get_animlayers()[12].m_flWeight = 0.f;

	m_globals()->m_framecount = -999;

	const auto v26 = *(int*)(uintptr_t(this) + 0x2670);

	*(int*)(uintptr_t(this) + 0x2670) = 0;
	g_ctx.globals.setuping_bones = true;

	m_bClientSideAnimation() = false;
	const bool result = SetupBones(nullptr, -1, matrix, time);
	m_bClientSideAnimation() = clientsideanim;

	g_ctx.globals.setuping_bones = false;

	*(int*)(uintptr_t(this) + 0x2670) = v26;

	*(int*)(uintptr_t(this) + 0xA28) = v20;
	*(int*)(uintptr_t(this) + 0xA30) = v19;
	*(unsigned short*)(uintptr_t(this) + 0x68) = v22;

	m_fEffects() = effects;

	*(uint8_t*)(uintptr_t(this) + 0x274) = v62;

	r_jiggle_bones->SetValue(penis);

	if (weight12 > -1)
		get_animlayers()[12].m_flWeight = weight12;

	m_globals()->m_realtime = realtime_backup;
	m_globals()->m_curtime = curtime;
	m_globals()->m_frametime = frametime;
	m_globals()->m_absoluteframetime = absoluteframetime;
	m_globals()->m_framecount = framecount;
	m_globals()->m_tickcount = tickcount;
	m_globals()->m_interpolation_amount = interpolation_amount;

	if (this != g_ctx.local())
		set_abs_origin(old_origin);
	else
	{
		if (bk < FLT_MAX)
			get_animlayers()[3].m_flWeight = bk;
	}

	if (previous_weapon)
		get_animation_state()->m_pLastBoneSetupWeapon = previous_weapon;

	return result;
}



bool player_t::setup_bones_local(matrix3x4_t* pBoneToWorldOut, int nMaxBones, int boneMask, float currentTime, bool record)
{
	if (!this)
		return false;

	void* pRenderable = reinterpret_cast<void*>(uintptr_t(this) + 0x4);

	if (!pRenderable)
		return false;

	std::array < AnimationLayer, 13 > aAnimationLayers;

	const auto backup_bone_array = m_BoneAccessor().GetBoneArrayForWrite();
	float_t flCurTime = m_globals()->m_curtime;
	float_t flRealTime = m_globals()->m_realtime;
	float_t flFrameTime = m_globals()->m_frametime;
	float_t flAbsFrameTime = m_globals()->m_absoluteframetime;
	int32_t iFrameCount = m_globals()->m_framecount;
	int32_t iTickCount = m_globals()->m_tickcount;
	float_t flInterpolation = m_globals()->m_interpolation_amount;

	m_globals()->m_curtime = m_flSimulationTime();
	m_globals()->m_realtime = m_flSimulationTime();
	m_globals()->m_frametime = m_globals()->m_intervalpertick;
	m_globals()->m_absoluteframetime = m_globals()->m_intervalpertick;
	m_globals()->m_framecount = TIME_TO_TICKS(m_flSimulationTime());
	m_globals()->m_tickcount = TIME_TO_TICKS(m_flSimulationTime());
	m_globals()->m_interpolation_amount = 0.0f;

	auto iOcclusionFramecount = m_iOcclusionFramecount();
	auto iOcclusionFlags = m_iOcclusionFlags();
	auto nLastSkipFramecount = m_nLastSkipFramecount();
	auto iEffects = m_fEffects();
	auto pInverseKinematics = m_pInverseKinematics();
	auto bMaintainSequenceTransition = m_bMaintainSequenceTransition();
	auto vecAbsOrigin = GetAbsOrigin();

	std::memcpy(aAnimationLayers.data(), get_animlayers(), sizeof(AnimationLayer) * 13);

	invalidate_bone_cache();
	m_BoneAccessor().m_ReadableBones = m_BoneAccessor().m_WritableBones = 0;

	if (get_animation_state1())
		get_animation_state1()->m_pWeaponLast = get_animation_state1()->m_pWeapon;

	g_ctx.globals.setuping_bones = true;

	m_iOcclusionFramecount() = 0;
	m_iOcclusionFlags() = 0;
	m_nLastSkipFramecount() = 0;

	if (this != g_ctx.local())
		set_abs_origin(m_vecOrigin());

	m_fEffects() |= 8;
	m_pInverseKinematics() = nullptr;
	m_bMaintainSequenceTransition() = false;

	get_animlayers()[ANIMATION_LAYER_LEAN].m_flWeight = 0.0f;
	if (this == g_ctx.local())
	{
		if (sequence_activity(get_animlayers()[ANIMATION_LAYER_ADJUST].m_nSequence) == ACT_CSGO_IDLE_TURN_BALANCEADJUST)
		{
			get_animlayers()[ANIMATION_LAYER_ADJUST].m_flCycle = 0.0f;
			get_animlayers()[ANIMATION_LAYER_ADJUST].m_flWeight = 0.0f;
		}
	}

	typedef bool(__thiscall* Fn)(void*, matrix3x4_t*, int, int, float);

	if (record)
		m_BoneAccessor().SetBoneArrayForWrite(pBoneToWorldOut);

	if (this == g_ctx.local())
		g_ctx.globals.m_allow_update_real_bones = true;

	auto res = call_virtual<Fn>(pRenderable, 13)(pRenderable, record ? nullptr : pBoneToWorldOut, nMaxBones, boneMask, currentTime);

	if (this == g_ctx.local())
		g_ctx.globals.m_allow_update_real_bones = false;

	if (record)
		m_BoneAccessor().SetBoneArrayForWrite(backup_bone_array);

	m_bMaintainSequenceTransition() = false;
	m_pInverseKinematics() = pInverseKinematics;
	m_fEffects() = iEffects;
	m_nLastSkipFramecount() = nLastSkipFramecount;
	m_iOcclusionFlags() = iOcclusionFlags;
	m_iOcclusionFramecount() = iOcclusionFramecount;

	g_ctx.globals.setuping_bones = false;

	if (this != g_ctx.local())
		set_abs_origin(vecAbsOrigin);

	std::memcpy(get_animlayers(), aAnimationLayers.data(), sizeof(AnimationLayer) * 13);

	m_globals()->m_curtime = flCurTime;
	m_globals()->m_realtime = flRealTime;
	m_globals()->m_frametime = flFrameTime;
	m_globals()->m_absoluteframetime = flAbsFrameTime;
	m_globals()->m_framecount = iFrameCount;
	m_globals()->m_tickcount = iTickCount;
	m_globals()->m_interpolation_amount = flInterpolation;

	return res;
}
void C_CSGOPlayerAnimationState::set_layer_sequence(AnimationLayer* animlayer, int activity)
{
	if (!animlayer || !activity)
		return;

	int sequence = this->select_sequence_from_activity_modifier(activity);
	if (sequence < 2)
		return;

	animlayer->m_nSequence = sequence;

	if (m_pBasePlayer && sequence && animlayer)
		animlayer->m_flPlaybackRate = m_pBasePlayer->GetLayerSequenceCycleRate(animlayer, sequence);

	animlayer->m_flCycle = animlayer->m_flWeight = 0.0f;
}

int C_CSGOPlayerAnimationState::select_sequence_from_activity_modifier(int activity)
{
	bool is_player_ducked = m_flAnimDuckAmount > 0.55f;
	bool is_player_running = m_flSpeedAsPortionOfWalkTopSpeed > 0.25f;

	int layer_sequence = -1;
	switch (activity)
	{
	case ACT_CSGO_JUMP:
	{
		layer_sequence = 15 + int(is_player_running);
		if (is_player_ducked)
			layer_sequence = 17 + int(is_player_running);
	}
	break;
	case ACT_CSGO_ALIVE_LOOP:
	{
		layer_sequence = 8;
		if (m_pWeaponLast != m_pWeapon)
			layer_sequence = 9;
	}
	break;
	case ACT_CSGO_IDLE_ADJUST_STOPPEDMOVING:
	{
		layer_sequence = 6;
	}
	break;
	case ACT_CSGO_FALL:
	{
		layer_sequence = 14;
	}
	break;
	case ACT_CSGO_IDLE_TURN_BALANCEADJUST:
	{
		layer_sequence = 4;
	}
	break;
	case ACT_CSGO_LAND_LIGHT:
	{
		layer_sequence = 20;
		if (is_player_running)
			layer_sequence = 22;

		if (is_player_ducked)
		{
			layer_sequence = 21;
			if (is_player_running)
				layer_sequence = 19;
		}
	}
	break;
	case ACT_CSGO_LAND_HEAVY:
	{
		layer_sequence = 23;
		if (is_player_ducked)
			layer_sequence = 24;
	}
	break;
	case ACT_CSGO_CLIMB_LADDER:
	{
		layer_sequence = 13;
	}
	break;
	default: break;
	}

	return layer_sequence;
}

void C_CSGOPlayerAnimationState::increment_layer_cycle(AnimationLayer* layer, bool is_loop)
{
	float new_cycle = (layer->m_flPlaybackRate * this->m_flLastUpdateIncrement) + layer->m_flCycle;
	if (!is_loop && new_cycle >= 1.0f)
		new_cycle = 0.999f;

	new_cycle -= (int)(new_cycle);
	if (new_cycle < 0.0f)
		new_cycle += 1.0f;

	if (new_cycle > 1.0f)
		new_cycle -= 1.0f;

	layer->m_flCycle = new_cycle;
}

bool C_CSGOPlayerAnimationState::is_layer_sequence_finished(AnimationLayer* layer, float time)
{
	return (layer->m_flPlaybackRate * time) + layer->m_flCycle >= 1.0f;
}

void C_CSGOPlayerAnimationState::set_layer_cycle(AnimationLayer* animlayer, float cycle)
{
	if (animlayer)
		animlayer->m_flCycle = cycle;
}

void C_CSGOPlayerAnimationState::set_layer_rate(AnimationLayer* animlayer, float rate)
{
	if (animlayer)
		animlayer->m_flPlaybackRate = rate;
}

void C_CSGOPlayerAnimationState::set_layer_weight(AnimationLayer* animlayer, float weight)
{
	if (animlayer)
		animlayer->m_flWeight = weight;
}


void AnimState_s::IncrementLayerCycle(AnimationLayer* Layer, bool bIsLoop)
{
	float_t flNewCycle = (Layer->m_flPlaybackRate * this->m_flLastUpdateIncrement) + Layer->m_flCycle;
	if (!bIsLoop && flNewCycle >= 1.0f)
		flNewCycle = 0.999f;

	flNewCycle -= (int32_t)(flNewCycle);
	if (flNewCycle < 0.0f)
		flNewCycle += 1.0f;

	if (flNewCycle > 1.0f)
		flNewCycle -= 1.0f;

	Layer->m_flCycle = flNewCycle;
}
bool AnimState_s::IsLayerSequenceFinished(AnimationLayer* Layer, float_t flTime)
{
	return (Layer->m_flPlaybackRate * flTime) + Layer->m_flCycle >= 1.0f;
}
void AnimState_s::SetLayerCycle(AnimationLayer* pAnimationLayer, float_t flCycle)
{
	if (pAnimationLayer)
		pAnimationLayer->m_flCycle = flCycle;
}
void AnimState_s::SetLayerRate(AnimationLayer* pAnimationLayer, float_t flRate)
{
	if (pAnimationLayer)
		pAnimationLayer->m_flPlaybackRate = flRate;
}
void AnimState_s::SetLayerWeight(AnimationLayer* pAnimationLayer, float_t flWeight)
{
	if (pAnimationLayer)
		pAnimationLayer->m_flWeight = flWeight;
}
void AnimState_s::SetLayerWeightRate(AnimationLayer* pAnimationLayer, float_t flPrevious)
{
	if (pAnimationLayer)
		pAnimationLayer->m_flWeightDeltaRate = (pAnimationLayer->m_flWeight - flPrevious) / m_flLastUpdateIncrement;
}
void AnimState_s::SetLayerSequence(AnimationLayer* pAnimationLayer, int iActivity)
{
	int32_t iSequence = this->SelectSequenceFromActivityModifier(iActivity);
	if (iSequence < 2)
		return;

	pAnimationLayer->m_nSequence = iSequence;
	pAnimationLayer->m_flPlaybackRate = m_pBaseEntity->GetLayerSequenceCycleRate(pAnimationLayer, iSequence);
	pAnimationLayer->m_flCycle = pAnimationLayer->m_flWeight = 0.0f;
}
int AnimState_s::SelectSequenceFromActivityModifier(int iActivity)
{
	bool bIsPlayerDucked = m_flAnimDuckAmount > 0.55f;
	bool bIsPlayerRunning = m_flSpeedAsPortionOfWalkTopSpeed > 0.25f;

	int iLayerSequence = -1;
	switch (iActivity)
	{
	case ACT_CSGO_JUMP:
	{
		iLayerSequence = 15 + static_cast <int>(bIsPlayerRunning);
		if (bIsPlayerDucked)
			iLayerSequence = 17 + static_cast <int>(bIsPlayerRunning);
	}
	break;

	case ACT_CSGO_ALIVE_LOOP:
	{
		iLayerSequence = 9;
		if (m_pWeaponLast != m_pWeapon)
			iLayerSequence = 8;
	}
	break;

	case ACT_CSGO_IDLE_ADJUST_STOPPEDMOVING:
	{
		iLayerSequence = 6;
	}
	break;

	case ACT_CSGO_FALL:
	{
		iLayerSequence = 14;
	}
	break;

	case ACT_CSGO_IDLE_TURN_BALANCEADJUST:
	{
		iLayerSequence = 4;
	}
	break;

	case ACT_CSGO_LAND_LIGHT:
	{
		iLayerSequence = 20;
		if (bIsPlayerRunning)
			iLayerSequence = 22;

		if (bIsPlayerDucked)
		{
			iLayerSequence = 21;
			if (bIsPlayerRunning)
				iLayerSequence = 19;
		}
	}
	break;

	case ACT_CSGO_LAND_HEAVY:
	{
		iLayerSequence = 23;
		if (bIsPlayerDucked)
			iLayerSequence = 24;
	}
	break;

	case ACT_CSGO_CLIMB_LADDER:
	{
		iLayerSequence = 13;
	}
	break;
	default: break;
	}

	return iLayerSequence;
}

void player_t::copy_animlayers(AnimationLayer* layers)
{
	if (!this)
		return;

	std::memcpy(layers, this->get_animlayers(), this->animlayer_count() * sizeof(AnimationLayer));
}

void player_t::copy_poseparameter(float* poses)
{
	if (!this)
		return;

	std::memcpy(poses, this->m_flPoseParameter().data(), 24 * sizeof(float));
}

void player_t::set_poseparameter(float* poses)
{
	if (!this)
		return;

	std::memcpy(this->m_flPoseParameter().data(), poses, 24 * sizeof(float));
}

void player_t::set_animlayers(AnimationLayer* layers)
{
	if (!this)
		return;

	std::memcpy(this->get_animlayers(), layers, this->animlayer_count() * sizeof(AnimationLayer));
}

void player_t::attachment_helper()
{
	if (!this)
		return;

	using fn = void(__thiscall*)(player_t*, CStudioHdr*);
	
	static fn attachement_helper = (fn)g_ctx.globals.attachment_helper;
	attachement_helper(this, this->m_pStudioHdr());
};

int* player_t::m_nButtons()
{
	static std::uintptr_t m_nButtons = util::find_in_datamap(GetPredDescMap(), crypt_str("m_nButtons"));
	return (int*)((std::uintptr_t)this + m_nButtons);
}

int& player_t::m_afButtonLast()
{
	static std::uintptr_t m_afButtonLast = util::find_in_datamap(GetPredDescMap(), crypt_str("m_afButtonLast"));
	return *(int*)((std::uintptr_t)this + m_afButtonLast);
}

int& player_t::m_afButtonPressed()
{
	static std::uintptr_t m_afButtonPressed = util::find_in_datamap(GetPredDescMap(), crypt_str("m_afButtonPressed"));
	return *(int*)((std::uintptr_t)this + m_afButtonPressed);
}

int& player_t::m_afButtonReleased()
{
	static std::uintptr_t m_afButtonReleased = util::find_in_datamap(GetPredDescMap(), crypt_str("m_afButtonReleased"));
	return *(int*)((std::uintptr_t)this + m_afButtonReleased);
}

int* player_t::m_nNextThinkTick1()
{
	static int m_nNextThinkTick = netvars::get().get_offset(crypt_str("CBasePlayer"), crypt_str("m_nNextThinkTick"));
	return (int*)((uintptr_t)this + m_nNextThinkTick);
}


void player_t::unknown_function()
{
	if (!this)
		return;
	uint64_t unknown_function1 = util::FindSignature(crypt_str("client.dll"), crypt_str("55 8B EC 56 57 8B F9 8B B7 ? ? ? ? 8B C6 C1 E8"));
	static auto fn = reinterpret_cast<void(__thiscall*)(int)>(unknown_function1);
	fn(0);
}

/////////////////////////////////

datamap_t* entity_t::GetPredDescMap()
{
	typedef datamap_t* (__thiscall* Fn)(void*);
	return call_virtual< Fn >(this, g_ctx.indexes.at(16))(this);
}
Vector player_t::GetEyePosition()
{
	return GetOrigin() + m_vecViewOffset();
}

bool player_t::CanSeePlayer(player_t* player, const Vector& pos)
{
	CGameTrace tr;
	Ray_t ray;
	CTraceFilter filter;
	filter.pSkip = this;

	ray.Init(get_shoot_position(), pos);
	m_trace()->TraceRay(ray, MASK_SHOT | CONTENTS_GRATE, &filter, &tr);

	return tr.hit_entity == player || tr.fraction > 0.97f;
}
CUserCmd*& player_t::m_pCurrentCommand()
{
	static auto currentCommand = *(uint32_t*)(util::FindSignature(crypt_str("client.dll"), crypt_str("89 BE ? ? ? ? E8 ? ? ? ? 85 FF")) + 0x2);
	return *(CUserCmd**)(uintptr_t(this) + currentCommand);
}
float player_t::GetMaxPlayerSpeed()
{
	weapon_t* pWeapon = this->m_hActiveWeapon().Get();

	if (pWeapon)
	{
		weapon_info_t* pWeaponData = pWeapon->get_csweapon_info();

		if (pWeaponData)
			return this->m_bIsScoped() ? pWeaponData->flMaxPlayerSpeedAlt : pWeaponData->flMaxPlayerSpeed;
	}

	return 260.0f;
}


void player_t::RunPreThink()
{
	static auto PhysicsRunThink = reinterpret_cast<bool(__thiscall*)(void*, int)>(util::FindSignature(crypt_str("client.dll"), crypt_str("55 8B EC 83 EC 10 53 56 57 8B F9 8B 87 ? ? ? ? C1 E8 16")));

	if (!PhysicsRunThink(this, 0))
		return;

	// Called every frame to let game rules do any specific think logic for the player
	// FIXME:  Do we need to set up a client side version of the gamerules???
	// g_pGameRules->PlayerThink( player );

	PreThink();
}

//-----------------------------------------------------------------------------
// Purpose: Runs the PLAYER's thinking code if time.  There is some play in the exact time the think
//  function will be called, because it is called before any movement is done
//  in a frame.  Not used for pushmove objects, because they must be exact.
//  Returns false if the entity removed itself.
void player_t::RunThink()
{
	static auto SetNextThink = reinterpret_cast<void(__thiscall*)(int)>(util::FindSignature(crypt_str("client.dll"), crypt_str("55 8B EC 56 57 8B F9 8B B7 ? ? ? ? 8B C6")));

	int thinktick = this->m_nNextThinkTick();
	if (thinktick <= 0 || thinktick > this->m_nTickBase()) //here need player->GetNextThinkTick() < player->m_nTickBase 
		return;

	//this->m_nNextThinkTick() = -1;
	SetNextThink(0);
	this->Think();
}
bool entity_t::is_player()
{
	if (!this) //-V704
		return false;

	auto client_class = GetClientClass();

	if (!client_class)
		return false;

	return client_class->m_ClassID == CCSPlayer;
}

void entity_t::set_model_index(int index)
{
	if (!this) //-V704
		return;

	using Fn = void(__thiscall*)(PVOID, int);
	return call_virtual<Fn>(this, g_ctx.indexes.at(7))(this, index);
}



bool entity_t::IsBreakableEntity()
{

	static uint64_t is_breakable = util::FindSignature(crypt_str("client.dll"), crypt_str("55 8B EC 51 56 8B F1 85 F6 74 68"));

	if (!this || !this->GetClientClass())
		return false;

	const auto szObjectName = this->GetClientClass()->m_pNetworkName;
	if (szObjectName[0] == 'C')
	{
		if (szObjectName[7] == 't' || szObjectName[7] == 'b')
			return true;
	}

	return ((bool(__thiscall*)(entity_t*))(is_breakable))(this);
}

void entity_t::set_abs_angles(const Vector& angle)
{
	if (!this) //-V704
		return;

	using Fn = void(__thiscall*)(void*, const Vector&);
	static auto fn = reinterpret_cast<Fn>(util::FindSignature(crypt_str("client.dll"), crypt_str("55 8B EC 83 E4 F8 83 EC 64 53 56 57 8B F1 E8")));

	return fn(this, angle);
}

void entity_t::set_abs_origin(const Vector& origin)
{
	if (!this) //-V704
		return;

	using Fn = void(__thiscall*)(void*, const Vector&);
	static auto fn = reinterpret_cast<Fn>(util::FindSignature(crypt_str("client.dll"), crypt_str("55 8B EC 83 E4 F8 51 53 56 57 8B F1 E8")));

	return fn(this, origin);
}
using GetShotgunSpread_t = void(__stdcall*)(int, int, int, float*, float*);
Vector weapon_t::calculate_spread(int seed, float inaccuracy, float spread, bool revolver2) {
	weapon_info_t* wep_info;
	int        item_def_index;
	float      recoil_index, r1, r2, r3, r4, s1, c1, s2, c2;

	// if we have no bullets, we have no spread.
	wep_info = get_csweapon_info();
	if (!wep_info || !wep_info->iBullets)
		return ZERO;

	// get some data for later.
	item_def_index = m_iItemDefinitionIndex();
	recoil_index = m_flRecoilSeed();
	static auto weapon_accuracy_shotgun_spread_patterns  = m_cvar()->FindVar(crypt_str("weapon_accuracy_shotgun_spread_patterns"));
	static auto get_shotgun_spread = util::FindSignature(crypt_str("client.dll"), crypt_str("55 8B EC 83 EC 10 56 8B 75 08 8D")); // GetShotgunSpread()
	// generate needed floats.
	r1 = std::get<0>(g_Networking->computed_seeds[seed]);
	r2 = std::get<1>(g_Networking->computed_seeds[seed]);

	if (weapon_accuracy_shotgun_spread_patterns->GetInt() > 0)
		((GetShotgunSpread_t)get_shotgun_spread)(item_def_index, 0, 0 + wep_info->iBullets * recoil_index, &r4, &r3);
	else {
		r3 = std::get<0>(g_Networking->computed_seeds[seed]);
		r4 = std::get<1>(g_Networking->computed_seeds[seed]);
	}

	// revolver secondary spread.
	if (item_def_index == WEAPON_REVOLVER && revolver2) {
		r1 = 1.f - (r1 * r1);
		r3 = 1.f - (r3 * r3);
	}

	// negev spread.
	else if (item_def_index == WEAPON_NEGEV && recoil_index < 3.f) {
		for (int i = 3; i > recoil_index; --i) {
			r1 *= r1;
			r3 *= r3;
		}

		r1 = 1.f - r1;
		r3 = 1.f - r3;
	}

	// get needed sine / cosine values.
	c1 = std::cos(r2);
	c2 = std::cos(r4);
	s1 = std::sin(r2);
	s2 = std::sin(r4);

	// calculate spread vector.
	return {
		(c1 * (r1 * inaccuracy)) + (c2 * (r3 * spread)),
		(s1 * (r1 * inaccuracy)) + (s2 * (r3 * spread)),
		0.f
	};
}
weapon_info_t* weapon_t::get_csweapon_info()
{
	if (!this) //-V704
		return nullptr;

	if (!m_weaponsys())
		return nullptr;

	return m_weaponsys()->GetWpnData(this->m_iItemDefinitionIndex());
}

float weapon_t::get_inaccuracy()
{
	if (!this)
		return 0.0f;

	return call_virtual<float(__thiscall*)(void*)>(this, 483)(this);
}

float weapon_t::get_spread()
{
	if (!this)
		return 0.0f;

	return call_virtual<float(__thiscall*)(void*)>(this, 453)(this);
}

void player_t::SetAsPredictionPlayer()
{
	static auto player = (**((player_t***)((void*)((DWORD)(util::FindSignature(("client.dll"), ("89 35 ? ? ? ? F3 0F 10 48 20"))) + 0x2))));
	player = this;
}

void player_t::UnsetAsPredictionPlayer()
{
	static auto player = (**((player_t***)((void*)((DWORD)(util::FindSignature(("client.dll"), ("89 35 ? ? ? ? F3 0F 10 48 20"))) + 0x2))));
	player = NULL;
}

typedescription_t* player_t::GetDatamapEntry(datamap_t* pDatamap, const char* szName)
{
	while (pDatamap)
	{
		for (int i = 0; i < pDatamap->dataNumFields; i++)
		{
			if (strcmp(szName, pDatamap->dataDesc[i].fieldName)) //-V526
				continue;

			return &pDatamap->dataDesc[i];
		}

		pDatamap = pDatamap->baseMap;
	}

	return NULL;
}

void weapon_t::update_accuracy_penality()
{
	if (!this) //-V704
		return;

	call_virtual<void(__thiscall*)(void*)>(this, g_ctx.indexes.at(11))(this);
}

bool weapon_t::is_empty()
{
	if (!this) //-V704
		return true;

	return m_iClip1() <= 0;
}

bool weapon_t::can_fire(bool check_revolver)
{
	if (!this) //-V704
		return false;

	if (!is_non_aim() && is_empty())
		return false;

	auto owner = (player_t*)m_entitylist()->GetClientEntityFromHandle(m_hOwnerEntity());

	if (owner == g_ctx.local() && g_AntiAim->freeze_check)
		return false;

	if (!owner->valid(false))
		return false;

	if (owner->m_bIsDefusing())
		return false;

	auto server_time = TICKS_TO_TIME(g_ctx.globals.fixed_tickbase);

	if (server_time < m_flNextPrimaryAttack())
		return false;

	if (server_time < owner->m_flNextAttack())
		return false;

	if (check_revolver && m_iItemDefinitionIndex() == WEAPON_REVOLVER && m_flPostponeFireReadyTime() >= server_time)
		return false;

	return true;
}

int weapon_t::get_weapon_group(bool rage)
{
	if (!this) //-V704
		return 0;

	if (rage)
	{
		if (m_iItemDefinitionIndex() == WEAPON_REVOLVER || m_iItemDefinitionIndex() == WEAPON_DEAGLE)
			return 0;
		else if (is_pistol())
			return 1;
		else if (is_smg())
			return 2;
		else if (is_rifle())
			return 3;
		else if (m_iItemDefinitionIndex() == WEAPON_SCAR20 || m_iItemDefinitionIndex() == WEAPON_G3SG1)
			return 4;
		else if (m_iItemDefinitionIndex() == WEAPON_SSG08)
			return 5;
		else if (m_iItemDefinitionIndex() == WEAPON_AWP)
			return 6;
		else if (is_shotgun())
			return 7;
	}
	else
	{
		if (m_iItemDefinitionIndex() == WEAPON_DEAGLE)
			return 0;
		if (is_pistol())
			return 1;
		else if (is_rifle())
			return 2;
		else if (is_smg())
			return 3;
		else if (is_sniper())
			return 4;
		else if (is_shotgun())
			return 5;
	}

	return 0;
}

bool weapon_t::IsGun()
{
	if (!this)
		return false;

	int id = this->m_iItemDefinitionIndex();

	if (!id)
		return false;

	if (id == WEAPON_KNIFE && id == WEAPON_HEGRENADE && id == WEAPON_DECOY && id == WEAPON_MOLOTOV && id == WEAPON_C4
		&& id == WEAPON_FLASHBANG && id == WEAPON_SMOKEGRENADE || id == WEAPON_KNIFE_T)
		return false;
	else
		return true;
}

bool weapon_t::is_rifle()
{
	if (!this) //-V704
		return false;

	int weapon_id = m_iItemDefinitionIndex();

	return weapon_id == WEAPON_AK47 || weapon_id == WEAPON_M4A1 || weapon_id == WEAPON_M4A1_SILENCER || weapon_id == WEAPON_GALILAR ||
		weapon_id == WEAPON_FAMAS || weapon_id == WEAPON_AUG || weapon_id == WEAPON_SG556;
}

bool weapon_t::is_smg()
{
	if (!this) //-V704
		return false;

	int weapon_id = m_iItemDefinitionIndex();

	return weapon_id == WEAPON_MAC10 || weapon_id == WEAPON_MP7 || weapon_id == WEAPON_MP9 || weapon_id == WEAPON_P90 ||
		weapon_id == WEAPON_BIZON || weapon_id == WEAPON_UMP45 || weapon_id == WEAPON_MP5SD;
}

bool weapon_t::is_shotgun()
{
	if (!this) //-V704
		return false;

	int weapon_id = m_iItemDefinitionIndex();

	return weapon_id == WEAPON_XM1014 || weapon_id == WEAPON_NOVA || weapon_id == WEAPON_SAWEDOFF || weapon_id == WEAPON_MAG7 || weapon_id == WEAPON_M249 || weapon_id == WEAPON_NEGEV;
}

bool weapon_t::is_pistol()
{
	if (!this) //-V704
		return false;

	int weapon_id = m_iItemDefinitionIndex();

	return weapon_id == WEAPON_DEAGLE || weapon_id == WEAPON_ELITE || weapon_id == WEAPON_FIVESEVEN || weapon_id == WEAPON_P250 ||
		weapon_id == WEAPON_GLOCK || weapon_id == WEAPON_HKP2000 || weapon_id == WEAPON_CZ75A || weapon_id == WEAPON_USP_SILENCER || weapon_id == WEAPON_TEC9 || weapon_id == WEAPON_REVOLVER;
}

bool weapon_t::is_sniper()
{
	if (!this) //-V704
		return false;

	int weapon_id = m_iItemDefinitionIndex();

	return weapon_id == WEAPON_AWP || weapon_id == WEAPON_SCAR20 || weapon_id == WEAPON_G3SG1 || weapon_id == WEAPON_SSG08;
}

bool weapon_t::is_grenade()
{
	if (!this) //-V704
		return false;

	int idx = m_iItemDefinitionIndex();

	return idx == WEAPON_FLASHBANG || idx == WEAPON_HEGRENADE || idx == WEAPON_SMOKEGRENADE || idx == WEAPON_MOLOTOV || idx == WEAPON_DECOY || idx == WEAPON_INCGRENADE;
}

bool weapon_t::is_knife()
{
	if (!this) //-V704
		return false;

	int idx = m_iItemDefinitionIndex();

	return idx == WEAPON_KNIFE || idx == WEAPON_BAYONET || idx == WEAPON_KNIFE_BUTTERFLY || idx == WEAPON_KNIFE_FALCHION
		|| idx == WEAPON_KNIFE_FLIP || idx == WEAPON_KNIFE_GUT || idx == WEAPON_KNIFE_KARAMBIT || idx == WEAPON_KNIFE_M9_BAYONET
		|| idx == WEAPON_KNIFE_PUSH || idx == WEAPON_KNIFE_SURVIVAL_BOWIE || idx == WEAPON_KNIFE_T || idx == WEAPON_KNIFE_TACTICAL
		|| idx == WEAPON_KNIFEGG || idx == WEAPON_KNIFE_GHOST || idx == WEAPON_KNIFE_GYPSY_JACKKNIFE || idx == WEAPON_KNIFE_STILETTO
		|| idx == WEAPON_KNIFE_URSUS || idx == WEAPON_KNIFE_WIDOWMAKER || idx == WEAPON_KNIFE_CSS || idx == WEAPON_KNIFE_CANIS
		|| idx == WEAPON_KNIFE_CORD || idx == WEAPON_KNIFE_OUTDOOR || idx == WEAPON_KNIFE_SKELETON;
}

bool weapon_t::is_non_aim(bool disable_knife)
{
	if (!this) //-V704
		return true;

	auto idx = m_iItemDefinitionIndex();

	if (idx == WEAPON_C4 || idx == WEAPON_HEALTHSHOT)
		return true;

	if (disable_knife)
		if (is_knife())
			return true;

	if (is_grenade())
		return true;

	return false;
}

bool weapon_t::can_double_tap()
{
	if (!this) //-V704
		return false;

	if (is_non_aim())
		return false;

	auto idx = m_iItemDefinitionIndex();

	if (idx == WEAPON_TASER || idx == WEAPON_REVOLVER || idx == WEAPON_SSG08 || idx == WEAPON_AWP || idx == WEAPON_XM1014 || idx == WEAPON_NOVA || idx == WEAPON_SAWEDOFF || idx == WEAPON_MAG7)
		return false;

	return true;
}

int weapon_t::get_max_tickbase_shift()
{
	if (!can_double_tap())
		return m_gamerules()->m_bIsValveDS() ? 6 : 16;

	auto idx = m_iItemDefinitionIndex();
	auto max_tickbase_shift = 0;

	switch (idx)
	{
	case WEAPON_M249:
	case WEAPON_MAC10:
	case WEAPON_P90:
	case WEAPON_MP5SD:
	case WEAPON_NEGEV:
	case WEAPON_MP9:
		max_tickbase_shift = 5;
		break;
	case WEAPON_ELITE:
	case WEAPON_UMP45:
	case WEAPON_BIZON:
	case WEAPON_TEC9:
	case WEAPON_MP7:
		max_tickbase_shift = 6;
		break;
	case WEAPON_AK47:
	case WEAPON_AUG:
	case WEAPON_FAMAS:
	case WEAPON_GALILAR:
	case WEAPON_M4A1:
	case WEAPON_M4A1_SILENCER:
	case WEAPON_CZ75A:
		max_tickbase_shift = 7;
		break;
	case WEAPON_FIVESEVEN:
	case WEAPON_GLOCK:
	case WEAPON_P250:
	case WEAPON_SG556:
		max_tickbase_shift = 8;
		break;
	case WEAPON_HKP2000:
	case WEAPON_USP_SILENCER:
		max_tickbase_shift = 9;
		break;
	case WEAPON_DEAGLE:
		max_tickbase_shift = 13;
		break;
	case WEAPON_G3SG1:
	case WEAPON_SCAR20:
		max_tickbase_shift = max_tickbase_shift = g_cfg.lua.override_shift;
		break;
	}

	if (m_gamerules()->m_bIsValveDS())
		max_tickbase_shift = min(max_tickbase_shift, 6);

	return max_tickbase_shift;
}

char* weapon_t::get_icon()
{
	if (!this) //-V704
		return " ";

	switch (m_iItemDefinitionIndex())
	{
	case WEAPON_BAYONET:
		return "1";
	case WEAPON_KNIFE_SURVIVAL_BOWIE:
		return "7";
	case WEAPON_KNIFE_BUTTERFLY:
		return "8";
	case WEAPON_KNIFE_FALCHION:
		return "0";
	case WEAPON_KNIFE_FLIP:
		return "2";
	case WEAPON_KNIFE_GUT:
		return "3";
	case WEAPON_KNIFE_KARAMBIT:
		return "4";
	case WEAPON_KNIFE_M9_BAYONET:
		return "5";
	case WEAPON_KNIFE_TACTICAL:
		return "6";
	case WEAPON_KNIFE_PUSH:
		return "]";
	case WEAPON_DEAGLE:
		return "A";
	case WEAPON_ELITE:
		return "B";
	case WEAPON_FIVESEVEN:
		return "C";
	case WEAPON_GLOCK:
		return "D";
	case WEAPON_HKP2000:
		return "E";
	case WEAPON_P250:
		return "F";
	case WEAPON_USP_SILENCER:
		return "G";
	case WEAPON_TEC9:
		return "H";
	case WEAPON_REVOLVER:
		return "J";
	case WEAPON_MAC10:
		return "K";
	case WEAPON_UMP45:
		return "L";
	case WEAPON_BIZON:
		return "M";
	case WEAPON_MP7:
		return "N";
	case WEAPON_MP9:
		return "O";
	case WEAPON_P90:
		return "P";
	case WEAPON_GALILAR:
		return "Q";
	case WEAPON_FAMAS:
		return "R";
	case WEAPON_M4A1_SILENCER:
		return "T";
	case WEAPON_M4A1:
		return "S";
	case WEAPON_AUG:
		return "U";
	case WEAPON_SG556:
		return "V";
	case WEAPON_AK47:
		return "W";
	case WEAPON_G3SG1:
		return "X";
	case WEAPON_SCAR20:
		return "Y";
	case WEAPON_AWP:
		return "Z";
	case WEAPON_SSG08:
		return "a";
	case WEAPON_XM1014:
		return "b";
	case WEAPON_SAWEDOFF:
		return "c";
	case WEAPON_MAG7:
		return "d";
	case WEAPON_NOVA:
		return "e";
	case WEAPON_NEGEV:
		return "f";
	case WEAPON_M249:
		return "g";
	case WEAPON_TASER:
		return "h";
	case WEAPON_FLASHBANG:
		return "i";
	case WEAPON_HEGRENADE:
		return "j";
	case WEAPON_SMOKEGRENADE:
		return "k";
	case WEAPON_MOLOTOV:
		return "l";
	case WEAPON_DECOY:
		return "m";
	case WEAPON_INCGRENADE:
		return "n";
	case WEAPON_C4:
		return "o";
	case WEAPON_CZ75A:
		return "I";
	default:
		return " ";
	}
}

std::string weapon_t::get_name()
{
	if (!this) //-V704
		return " ";

	switch (m_iItemDefinitionIndex())
	{
	case WEAPON_KNIFE:
		return "KNIFE";
	case WEAPON_KNIFE_T:
		return "KNIFE";
	case WEAPON_BAYONET:
		return "KNIFE";
	case WEAPON_KNIFE_SURVIVAL_BOWIE:
		return "KNIFE";
	case WEAPON_KNIFE_BUTTERFLY:
		return "KNIFE";
	case WEAPON_KNIFE_FALCHION:
		return "KNIFE";
	case WEAPON_KNIFE_FLIP:
		return "KNIFE";
	case WEAPON_KNIFE_GUT:
		return "KNIFE";
	case WEAPON_KNIFE_KARAMBIT:
		return "KNIFE";
	case WEAPON_KNIFE_M9_BAYONET:
		return "KNIFE";
	case WEAPON_KNIFE_TACTICAL:
		return "KNIFE";
	case WEAPON_KNIFE_PUSH:
		return "KNIFE";
	case WEAPON_DEAGLE:
		return "DEAGLE";
	case WEAPON_ELITE:
		return "DUAL BERETTAS";
	case WEAPON_FIVESEVEN:
		return "FIVE-SEVEN";
	case WEAPON_GLOCK:
		return "GLOCK 18";
	case WEAPON_HKP2000:
		return "P2000";
	case WEAPON_P250:
		return "P250";
	case WEAPON_USP_SILENCER:
		return "USP-S";
	case WEAPON_TEC9:
		return "TEC-9";
	case WEAPON_REVOLVER:
		return "REVOLVER";
	case WEAPON_MAC10:
		return "MAC-10";
	case WEAPON_UMP45:
		return "UMP-45";
	case WEAPON_BIZON:
		return "PP-BIZON";
	case WEAPON_MP7:
		return "MP7";
	case WEAPON_MP9:
		return "MP9";
	case WEAPON_P90:
		return "P90";
	case WEAPON_GALILAR:
		return "GALIL AR";
	case WEAPON_FAMAS:
		return "FAMAS";
	case WEAPON_M4A1_SILENCER:
		return "M4A1-S";
	case WEAPON_M4A1:
		return "M4A4";
	case WEAPON_AUG:
		return "AUG";
	case WEAPON_SG556:
		return "SG 553";
	case WEAPON_AK47:
		return "AK-47";
	case WEAPON_G3SG1:
		return "G3SG1";
	case WEAPON_SCAR20:
		return "SCAR-20";
	case WEAPON_AWP:
		return "AWP";
	case WEAPON_SSG08:
		return "SSG 08";
	case WEAPON_XM1014:
		return "XM1014";
	case WEAPON_SAWEDOFF:
		return "SAWED-OFF";
	case WEAPON_MAG7:
		return "MAG-7";
	case WEAPON_NOVA:
		return "NOVA";
	case WEAPON_NEGEV:
		return "NEGEV";
	case WEAPON_M249:
		return "M249";
	case WEAPON_TASER:
		return "ZEUS X27";
	case WEAPON_FLASHBANG:
		return "FLASHBANG";
	case WEAPON_HEGRENADE:
		return "HE GRENADE";
	case WEAPON_SMOKEGRENADE:
		return "SMOKE";
	case WEAPON_MOLOTOV:
		return "MOLOTOV";
	case WEAPON_DECOY:
		return "DECOY";
	case WEAPON_INCGRENADE:
		return "INCENDIARY";
	case WEAPON_C4:
		return "C4";
	case WEAPON_CZ75A:
		return "CZ75-AUTO";
	default:
		return " ";
	}
}

std::array <float, 24>& entity_t::m_flPoseParameter()
{
	static auto _m_flPoseParameter = netvars::get().get_offset(crypt_str("CBaseAnimating"), crypt_str("m_flPoseParameter"));
	return *(std::array <float, 24>*)((uintptr_t)this + _m_flPoseParameter);
}

Vector player_t::get_shoot_position()
{
	auto result = ZERO;

	if (!this)
		return result;


	call_virtual<Vector& (__thiscall*)(void*, Vector*)>(this, 169)(this, &result);

	return result;
}

bool player_t::is_alive()
{
	if (!this) //-V704
		return false;

	if (m_iTeamNum() != 2 && m_iTeamNum() != 3)
		return false;

	if (m_lifeState() != LIFE_ALIVE)
		return false;

	return true;
}

bool player_t::m_bDuckUntilOnGround()
{
	return *(bool*)((DWORD)(this) + 0x10478);
}

int	player_t::get_move_type()
{
	if (!this) //-V704
		return 0;

	return *(int*)((uintptr_t)this + 0x25C);
}

int player_t::get_hitbox_bone_id(int hitbox_id)
{
	if (!this) //-V704
		return -1;

	auto hdr = m_modelinfo()->GetStudioModel(GetModel());

	if (!hdr)
		return -1;

	auto hitbox_set = hdr->pHitboxSet(m_nHitboxSet());

	if (!hitbox_set)
		return -1;

	auto hitbox = hitbox_set->pHitbox(hitbox_id);

	if (!hitbox)
		return -1;

	return hitbox->bone;
}

Vector player_t::hitbox_position(int hitbox_id)
{
	if (!this) //-V704
		return ZERO;

	auto hdr = m_modelinfo()->GetStudioModel(GetModel());

	if (!hdr)
		return ZERO;

	auto hitbox_set = hdr->pHitboxSet(m_nHitboxSet());

	if (!hitbox_set)
		return ZERO;

	auto hitbox = hitbox_set->pHitbox(hitbox_id);

	if (!hitbox)
		return ZERO;

	Vector min, max;

	math::vector_transform(hitbox->bbmin, m_CachedBoneData().Base()[hitbox->bone], min);
	math::vector_transform(hitbox->bbmax, m_CachedBoneData().Base()[hitbox->bone], max);

	return (min + max) * 0.5f;
}

bool player_t::is_on_ground()
{
	return this->m_hGroundEntity().IsValid() && this->m_hGroundEntity().Get();
}

Vector player_t::hitbox_position_matrix(int hitbox_id, matrix3x4_t matrix[MAXSTUDIOBONES])
{
	if (!this) //-V704
		return ZERO;

	auto hdr = m_modelinfo()->GetStudioModel(GetModel());

	if (!hdr)
		return ZERO;

	auto hitbox_set = hdr->pHitboxSet(m_nHitboxSet());

	if (!hitbox_set)
		return ZERO;

	auto hitbox = hitbox_set->pHitbox(hitbox_id);

	if (!hitbox)
		return ZERO;

	Vector min, max;

	math::vector_transform(hitbox->bbmin, matrix[hitbox->bone], min);
	math::vector_transform(hitbox->bbmax, matrix[hitbox->bone], max);

	return (min + max) * 0.5f;
}

CUtlVector <matrix3x4_t>& player_t::m_CachedBoneData()
{
	return *(CUtlVector <matrix3x4_t>*)(uintptr_t(this) + 0x2914);
}

CBoneAccessor& player_t::m_BoneAccessor()
{
	static auto m_nForceBone = netvars::get().get_offset(crypt_str("CBaseAnimating"), crypt_str("m_nForceBone"));
	static auto BoneAccessor = m_nForceBone + 0x1C;

	return *(CBoneAccessor*)((uintptr_t)this + BoneAccessor);
}

void player_t::invalidate_bone_cache()
{
	if (!this) //-V704
		return;

	m_flLastBoneSetupTime() = -FLT_MAX;
	m_iMostRecentModelBoneCounter() = UINT_MAX;
}

void player_t::set_abs_velocity(const Vector& velocity)
{
	if (!this) //-V704
		return;

	using Fn = void(__thiscall*)(void*, const Vector&);
	static auto fn = reinterpret_cast<Fn>(util::FindSignature(crypt_str("client.dll"), crypt_str("55 8B EC 83 E4 F8 83 EC 0C 53 56 57 8B 7D 08 8B F1 F3")));

	return fn(this, velocity);
}

Vector player_t::get_render_angles()
{
	if (!this) //-V704
		return ZERO;

	return *(Vector*)((uintptr_t)this + 0x31D8);
}

void player_t::set_render_angles(const Vector& angles)
{
	if (!this) //-V704
		return;

	*(Vector*)((uintptr_t)this + 0x31D8) = angles;
}
Vector player_t::m_aimPunchAngleScaled()
{
	if (!this)
		return ZERO;

	static auto weapon_recoil_scale = m_cvar()->FindVar(crypt_str("weapon_recoil_scale"));
	const auto m_aim_punch_angle = m_aimPunchAngle();

	return m_aim_punch_angle * weapon_recoil_scale->GetFloat();
}
void player_t::update_clientside_animation()
{
	if (!this || !get_animation_state() || m_clientstate()->iDeltaTick == -1) //check
		return;// Repeat

	//if (get_animation_state()->m_iLastClientSideAnimationUpdateFramecount == m_globals()->m_framecount)  // check source cs
	//    get_animation_state()->m_iLastClientSideAnimationUpdateFramecount = m_globals()->m_framecount - 1; // check source cs

	if (this == g_ctx.local()) {
		m_flThirdpersonRecoil() = m_aimPunchAngleScaled().x;
	}
	else {
		m_iEFlags() &= ~(EFL_DIRTY_ABSVELOCITY | EFL_DIRTY_ABSTRANSFORM); // set the flag
	}

	if (get_animation_state()->m_flLastClientSideAnimationUpdateTime == m_globals()->m_curtime) // enable brain
		get_animation_state()->m_flLastClientSideAnimationUpdateTime = m_globals()->m_curtime + TICKS_TO_TIME(1); // enable brain

	g_ctx.globals.updating_animation = true; // include update player animation
	this->m_bClientSideAnimation() = true;   // include update client side animation

	auto previous_weapon = get_animation_state() ? get_animation_state()->m_pLastBoneSetupWeapon : nullptr;

	if (previous_weapon)
		get_animation_state()->m_pLastBoneSetupWeapon = get_animation_state()->m_pActiveWeapon;

	using Fn = void(__thiscall*)(void*);
	call_virtual<Fn>(this, g_ctx.indexes.at(13))(this); // call to index

	g_ctx.globals.updating_animation = false; //turn off
}

uint32_t& player_t::m_iMostRecentModelBoneCounter()
{
	static auto invalidate_bone_cache = util::FindSignature(crypt_str("client.dll"), crypt_str("80 3D ?? ?? ?? ?? ?? 74 16 A1 ?? ?? ?? ?? 48 C7 81"));
	static auto most_recent_model_bone_counter = *(uintptr_t*)(invalidate_bone_cache + 0x1B);

	return *(uint32_t*)((uintptr_t)this + most_recent_model_bone_counter);
}



void player_t::SetCollisionBounds(const Vector& OBBMins, const Vector& OBBMaxs)
{
	const auto Collideable = GetCollideable();
	if (!Collideable)
		return;

	static const auto nSetCollisionBounds = reinterpret_cast<void(__thiscall*)(ICollideable*, const Vector&, const Vector&)>(util::FindSignature(crypt_str("client.dll"), 
		crypt_str("53 8B DC 83 EC 08 83 E4 F8 83 C4 04 55 8B 6B 04 89 6C 24 04 8B EC 83 EC 18 56 57 8B 7B 08 8B D1 8B 4B 0C")));

	nSetCollisionBounds(Collideable, OBBMins , OBBMaxs);
}

void player_t::UpdateCollisionBounds()
{

	typedef void(__thiscall* oUpdateCollisionBounds)(PVOID);
	return call_virtual< oUpdateCollisionBounds >(this, 340)(this);
}

float& player_t::m_flLastBoneSetupTime()
{
	static auto invalidate_bone_cache = util::FindSignature(crypt_str("client.dll"), crypt_str("80 3D ?? ?? ?? ?? ?? 74 16 A1 ?? ?? ?? ?? 48 C7 81"));
	static auto last_bone_setup_time = *(uintptr_t*)(invalidate_bone_cache + 0x11);

	return *(float*)((uintptr_t)this + last_bone_setup_time);
}

void player_t::select_item(const char* string, int sub_type)
{
	static auto select_item_fn = reinterpret_cast <void(__thiscall*)(void*, const char*, int)> (util::FindSignature(crypt_str("client.dll"), crypt_str("55 8B EC 56 8B F1 ? ? ? 85 C9 74 71 8B 06")));
	select_item_fn(this, string, sub_type);
}

bool player_t::using_standard_weapons_in_vechile()
{
	static auto using_standard_weapons_in_vechile_fn = reinterpret_cast <bool(__thiscall*)(void*)> (util::FindSignature(crypt_str("client.dll"), crypt_str("56 57 8B F9 8B 97 ? ? ? ? 83 FA FF 74 41")));
	return using_standard_weapons_in_vechile_fn(this);
}

bool player_t::physics_run_think(int index)
{
	static auto physics_run_think_fn = reinterpret_cast <bool(__thiscall*)(void*, int)> (util::FindSignature(crypt_str("client.dll"), crypt_str("55 8B EC 83 EC 10 53 56 57 8B F9 8B 87 ? ? ? ? C1 E8 16")));
	return physics_run_think_fn(this, index);
}

VarMapping_t* player_t::var_mapping()
{
	return reinterpret_cast<VarMapping_t*>((DWORD)this + 0x24);
}

CUserCmd*& player_t::current_command()
{
	static auto current_command = *reinterpret_cast<uint32_t*> (util::FindSignature(crypt_str("client.dll"), crypt_str("89 BE ? ? ? ? E8 ? ? ? ? 85 FF")) + 2);
	return *reinterpret_cast<CUserCmd**> (reinterpret_cast<uintptr_t> (this) + current_command);
}


void  player_t::SetLocalVAngles(Vector& angle)
{
	using fnSetVAngles = void(__thiscall*)(void*, Vector&);
	call_virtual<fnSetVAngles>(this, 372)(this, angle);

}

bool player_t::valid(bool check_team, bool check_dormant)
{
	if (!this) //-V704
		return false;

	if (!g_ctx.local())
		return false;

	if (!is_player())
		return false;

	if (!is_alive())
		return false;

	if (IsDormant() && check_dormant)
		return false;

	if (check_team && g_ctx.local()->m_iTeamNum() == m_iTeamNum())
		return false;

	return true;
}

int player_t::animlayer_count()
{
	if (!this) //-V704
		return 0;

	return *(int*)((DWORD)this + 0x299C);
}

AnimationLayer* player_t::get_animlayers()
{
	return *(AnimationLayer**)((DWORD)this + 0x2990);
}


int player_t::sequence_activity(int sequence)
{
	if (!this) //-V704
		return 0;

	auto hdr = m_modelinfo()->GetStudioModel(this->GetModel());

	if (!hdr)
		return 0;

	static auto sequence_activity = reinterpret_cast<int(__fastcall*)(void*, studiohdr_t*, int)>(util::FindSignature(crypt_str("client.dll"), crypt_str("55 8B EC 53 8B 5D 08 56 8B F1 83")));
	return sequence_activity(this, hdr, sequence);
}

c_baseplayeranimationstate* player_t::get_animation_state()
{
	return *reinterpret_cast<c_baseplayeranimationstate**>(reinterpret_cast<void*>(uintptr_t(this) + 0x9960));
}

AnimState_s* player_t::GetAnimState()
{
	return *reinterpret_cast<AnimState_s**>(reinterpret_cast<void*>(uintptr_t(this) + 0x9960));
}


//        

C_CSGOPlayerAnimationState* player_t::get_animation_state1()
{
	return *reinterpret_cast<C_CSGOPlayerAnimationState**>(reinterpret_cast<void*>(uintptr_t(this) + 0x9960));
}

CStudioHdr* player_t::m_pStudioHdr()
{
	static auto studio_hdr = util::FindSignature(crypt_str("client.dll"), crypt_str("8B B7 ?? ?? ?? ?? 89 74 24 20"));
	return *(CStudioHdr**)((uintptr_t)this + *(uintptr_t*)(studio_hdr + 0x2) + 0x4);
}

CStudioHdr* player_t::GetStudioHdr()
{
	return *(CStudioHdr**)((DWORD)(this) + 0x2950);
}

void player_t::SetupBones_AttachmentHelper()
{
	static auto pSetupBones_AttachmentHelper = (void*)(util::FindSignature("client.dll", "55 8B EC 83 EC 48 53 8B 5D 08 89 4D F4"));
	return ((void(__thiscall*)(void*, void*))(pSetupBones_AttachmentHelper))(this, this->GetStudioHdr());
}
std::uintptr_t player_t::renderable_nem() {
	return reinterpret_cast<std::uintptr_t>(this) + sizeof(std::uintptr_t);
}
 bool player_t::setup_bones_nem(matrix3x4_t* const bones, const int max_bones,const int mask, const float time
) {
	using fn_t = bool(__thiscall*)(
		const std::uintptr_t, matrix3x4_t* const,
		const int, const int, const float
		);
	return (*reinterpret_cast<fn_t**>(renderable_nem()))[13u](
		renderable_nem(), bones, max_bones, mask, time
		);
}




/*bool player_t::setup_bones(player_t* const player,matrix3x4_t& bones, const float time, const int flags)
{
	struct backup_t
	{
		__forceinline constexpr backup_t() = default;

		__forceinline backup_t(valve::c_player* const player)
			: m_cur_time{ valve::g_global_vars->m_cur_time },
			m_frame_time{ valve::g_global_vars->m_frame_time },
			m_frame_count{ valve::g_global_vars->m_frame_count },
			m_occlusion_frame{ player->occlusion_frame() },
			m_ent_client_flags{ player->ent_client_flags() },
			m_ik_context{ player->ik_context() }, m_effects{ player->effects() },
			m_occlusion_flags{ player->occlusion_flags() } {}

		float					m_cur_time{}, m_frame_time{};
		int						m_frame_count{}, m_occlusion_frame{};

		std::uint8_t			m_ent_client_flags{};
		valve::ik_context_t* m_ik_context{};

		std::size_t				m_effects{}, m_occlusion_flags{};
	} backup{ player };

	g_context->force_bone_mask() = flags & 4;

	valve::g_global_vars->m_cur_time = time;
	valve::g_global_vars->m_frame_time = valve::g_global_vars->m_interval_per_tick;

	if (flags & 8)
	{
		player->effects() |= 8u;
		player->occlusion_flags() &= ~0xau;
		player->occlusion_frame() = 0;
	}

	if (flags & 4)
	{
		player->ik_context() = nullptr;
		player->ent_client_flags() |= 2u;
	}

	if (flags & 2)
		player->last_setup_bones_frame() = 0;

	if (flags & 1)
	{
		player->mdl_bone_counter() = 0ul;
		player->last_setup_bones_time() = std::numeric_limits< float >::lowest();

		auto& bone_accessor = player->bone_accessor();

		bone_accessor.m_writable_bones = bone_accessor.m_readable_bones = 0;
	}

	auto& jiggle_bones = g_context->cvars().m_r_jiggle_bones;

	const auto backup_jiggle_bones = jiggle_bones->get_bool();

	jiggle_bones->set_int(false);

	g_context->allow_setup_bones() = true;
	const auto ret = player->setup_bones_nem(bones.data(), 256, (((flags >> 4) & 1) << 9) + 0xffd00, time);
	const auto ret = player->setup_bones(bones.data(), bones, (((flags >> 4) & 1) << 9) + 0xffd00, time);
	g_context->allow_setup_bones() = false;

	jiggle_bones->set_int(backup_jiggle_bones);

	if (flags & 8)
	{
		player->effects() = backup.m_effects;
		player->occlusion_flags() = backup.m_occlusion_flags;
		player->occlusion_frame() = backup.m_occlusion_frame;
	}

	if (flags & 4)
	{
		player->ik_context() = backup.m_ik_context;
		player->ent_client_flags() = backup.m_ent_client_flags;
	}

	valve::g_global_vars->m_cur_time = backup.m_cur_time;
	valve::g_global_vars->m_frame_time = backup.m_frame_time;
	valve::g_global_vars->m_frame_count = backup.m_frame_count;

	if (!(flags & 4))
		return ret;

	const auto mdl_data = player->mdl_data();
	if (!mdl_data
		|| !mdl_data->m_studio_hdr)
		return ret;

	const auto hitbox_set = mdl_data->m_studio_hdr->hitbox_set(player->hitbox_set_index());
	if (!hitbox_set)
		return ret;

	for (int i{}; i < hitbox_set->m_hitboxes_count; ++i)
	{
		const auto hitbox = hitbox_set->hitbox(i);
		if (!hitbox
			|| hitbox->m_radius >= 0.f)
			continue;

		mat3x4_t rot_mat{};
		g_context->addresses().m_angle_matrix(hitbox->m_rotation, rot_mat);

		math::concat_transforms(
			bones[hitbox->m_bone], rot_mat, bones.at(hitbox->m_bone)
		);
	}

	return ret;
}*/
bool player_t::setup_bones_rebuilt(matrix3x4_t* matrix, int mask)
{
	if (!this)
		return false;
	auto setuped = false;
	std::array < AnimationLayer, 13 > animation_layers;
	float curtime = m_globals()->m_curtime;
	float realtime = m_globals()->m_realtime;
	float absoluteframetime = m_globals()->m_absoluteframetime;
	float frametime = m_globals()->m_frametime;
	float framecount = m_globals()->m_framecount;
	float tickcount = m_globals()->m_tickcount;
	float interpolation_amount = m_globals()->m_interpolation_amount;
	m_globals()->m_curtime = this->m_flSimulationTime();
	m_globals()->m_realtime = this->m_flSimulationTime();
	m_globals()->m_frametime = m_globals()->m_intervalpertick;
	m_globals()->m_absoluteframetime = m_globals()->m_intervalpertick;
	m_globals()->m_framecount = TIME_TO_TICKS(this->m_flSimulationTime());
	m_globals()->m_tickcount = TIME_TO_TICKS(this->m_flSimulationTime());
	m_globals()->m_interpolation_amount = 0.0f;
	const LPVOID inverse_kinematics = *(LPVOID*)(uintptr_t(this) + 0x2670);
	const int client_effects = *(uint8_t*)(uintptr_t(this) + 0x68);
	const int last_skip_framecount = *(int*)(uintptr_t(this) + 0xA68);
	const int occlusion_mask = *(int*)(uintptr_t(this) + 0xA28);
	const int occlusion_frame = *(int*)(uintptr_t(this) + 0xA30);
	const int effects = this->m_fEffects();
	const bool maintain_sequence_transition = *(bool*)(uintptr_t(this) + 0x9F0);
	const Vector absoluteorigin = this->GetAbsOrigin();
	std::memcpy(animation_layers.data(), this->get_animlayers(), sizeof(AnimationLayer) * 13);
	this->invalidate_bone_cache();
	this->m_BoneAccessor().m_ReadableBones = NULL;
	this->m_BoneAccessor().m_WritableBones = NULL;
	auto animstate = this->get_animation_state();
	auto previous_weapon = animstate ? animstate->m_pLastActiveWeapon : nullptr;
	if (previous_weapon)
		animstate->m_pLastActiveWeapon = animstate->m_pActiveWeapon;
	*(int*)(uintptr_t(this) + 0xA30) = 0;
	*(int*)(uintptr_t(this) + 0xA28) = 0;
	*(int*)(uintptr_t(this) + 0xA68) = 0;
	if (this != g_ctx.local())
		this->set_abs_origin(this->m_vecOrigin());
	this->m_fEffects() |= 8;
	*(uint8_t*)(uintptr_t(this) + 0x68) |= 2;
	*(LPVOID*)(uintptr_t(this) + 0x2670) = nullptr;
	*(bool*)(uintptr_t(this) + 0x2930) = false;
	*(bool*)(uintptr_t(this) + 0x9F0) = false;
	if (mask == BONE_USED_BY_HITBOX)
		this->get_animlayers()[3].m_pOwner = NULL;
	else if (this == g_ctx.local())
	{
		this->get_animlayers()[12].m_flWeight = 0.0f;
		if (this->sequence_activity(this->get_animlayers()[3].m_nSequence) == ACT_CSGO_IDLE_TURN_BALANCEADJUST)
		{
			this->get_animlayers()[3].m_flCycle = 0.0f;
			this->get_animlayers()[3].m_flWeight = 0.0f;
		}
	}
	g_ctx.globals.setuping_bones = true;
	setuped = SetupBones(matrix, MAXSTUDIOBONES, mask, this->m_flSimulationTime());
	g_ctx.globals.setuping_bones = false;
	*(bool*)(uintptr_t(this) + 0x9F0) = maintain_sequence_transition;
	*(LPVOID*)(uintptr_t(this) + 0x2670) = inverse_kinematics;
	*(uint8_t*)(uintptr_t(this) + 0x68) = client_effects;
	this->m_fEffects() = effects;
	*(int*)(uintptr_t(this) + 0xA68) = last_skip_framecount;
	*(int*)(uintptr_t(this) + 0xA30) = occlusion_frame;
	*(int*)(uintptr_t(this) + 0xA28) = occlusion_mask;
	if (this != g_ctx.local())
		set_abs_origin(absoluteorigin);
	std::memcpy(this->get_animlayers(), animation_layers.data(), sizeof(AnimationLayer) * 13);
	m_globals()->m_curtime = curtime;
	m_globals()->m_realtime = realtime;
	m_globals()->m_absoluteframetime = absoluteframetime;
	m_globals()->m_frametime = frametime;
	m_globals()->m_framecount = framecount;
	m_globals()->m_tickcount = tickcount;
	m_globals()->m_interpolation_amount = interpolation_amount;

	return setuped;
}


bool player_t::setup_bones1(matrix3x4_t* pBoneToWorldOut, bool safe_matrix)
{
	if (!this)
		return false;

	AnimationLayer backup_layers[13];

	const float flCurTime = m_globals()->m_curtime;
	const float flRealTime = m_globals()->m_realtime;
	const float flFrameTime = m_globals()->m_frametime;
	const float flAbsFrameTime = m_globals()->m_absoluteframetime;
	const int iFrameCount = m_globals()->m_framecount;
	const int iTickCount = m_globals()->m_tickcount;
	const float flInterpolation = m_globals()->m_interpolation_amount;

	m_globals()->m_curtime = m_globals()->m_realtime = this->m_flSimulationTime();
	m_globals()->m_frametime = m_globals()->m_absoluteframetime = m_globals()->m_intervalpertick;
	m_globals()->m_framecount = INT_MAX;
	m_globals()->m_tickcount = TIME_TO_TICKS(m_globals()->m_realtime);
	m_globals()->m_interpolation_amount = 0.f;

	const uint32_t iClientEffects = this->m_nClientEffects();
	const uint32_t iOcclusionFramecount = this->m_iOcclusionFramecount();
	const uint32_t iOcclusionFlags = this->m_iOcclusionFlags();
	const uint32_t nLastSkipFramecount = this->m_nLastSkipFramecount();
	const uint32_t iEffects = this->m_fEffects();
	const bool bMaintainSequenceTransition = this->m_bMaintainSequenceTransition();
	const Vector vecAbsOrigin = this->GetAbsOrigin();

	int iMask = BONE_USED_BY_ANYTHING;
	if (safe_matrix)
		iMask = BONE_USED_BY_HITBOX;

	this->copy_animlayers(backup_layers);
	this->invalidate_bone_cache();
	this->m_BoneAccessor().m_ReadableBones = this->m_BoneAccessor().m_WritableBones = NULL;

	if (this->get_animation_state())
		this->get_animation_state()->m_pLastActiveWeapon = this->get_animation_state()->m_pActiveWeapon;

	this->m_iOcclusionFramecount() = this->m_iOcclusionFlags() = this->m_nLastSkipFramecount() = NULL;

	if (this != g_ctx.local())
		this->set_abs_origin(this->m_vecOrigin());

	this->m_fEffects() |= 8;
	this->m_nClientEffects() |= 2;
	this->m_bMaintainSequenceTransition() = false;

	this->get_animlayers()[ANIMATION_LAYER_LEAN].m_flWeight = 0.0f;
	if (safe_matrix)
		this->get_animlayers()[ANIMATION_LAYER_ADJUST].m_pOwner = NULL;
	else if (this == g_ctx.local())
	{
		if (this->sequence_activity(this->get_animlayers()[ANIMATION_LAYER_ADJUST].m_nSequence) == ACT_CSGO_IDLE_TURN_BALANCEADJUST)
		{
			this->get_animlayers()[ANIMATION_LAYER_ADJUST].m_flCycle = 0.0f;
			this->get_animlayers()[ANIMATION_LAYER_ADJUST].m_flWeight = 0.0f;
		}
	}

	g_ctx.globals.setuping_bones = true;
	auto res = this->SetupBones(pBoneToWorldOut, MAXSTUDIOBONES, iMask, 0.f);
	g_ctx.globals.setuping_bones = false;

	this->m_bMaintainSequenceTransition() = bMaintainSequenceTransition;
	this->m_nClientEffects() = iClientEffects;
	this->m_fEffects() = iEffects;
	this->m_nLastSkipFramecount() = nLastSkipFramecount;
	this->m_iOcclusionFlags() = iOcclusionFlags;
	this->m_iOcclusionFramecount() = iOcclusionFramecount;

	if (this != g_ctx.local())
		this->set_abs_origin(vecAbsOrigin);

	this->set_animlayers(backup_layers);

	m_globals()->m_curtime = flCurTime;
	m_globals()->m_realtime = flRealTime;
	m_globals()->m_frametime = flFrameTime;
	m_globals()->m_absoluteframetime = flAbsFrameTime;
	m_globals()->m_framecount = iFrameCount;
	m_globals()->m_tickcount = iTickCount;
	m_globals()->m_interpolation_amount = flInterpolation;

	return res;
}

uint32_t& player_t::m_fEffects()
{
	static auto m_fEffects = util::find_in_datamap(GetPredDescMap(), crypt_str("m_fEffects"));
	return *(uint32_t*)(uintptr_t(this) + m_fEffects);
}

uint32_t& player_t::m_iEFlags()
{
	static auto m_iEFlags = util::find_in_datamap(GetPredDescMap(), crypt_str("m_iEFlags"));
	return *(uint32_t*)(uintptr_t(this) + m_iEFlags);
}

float& player_t::m_surfaceFriction()
{
	static auto m_surfaceFriction = util::find_in_datamap(GetPredDescMap(), crypt_str("m_surfaceFriction"));
	return *(float*)(uintptr_t(this) + m_surfaceFriction);
}

Vector& player_t::m_vecAbsVelocity()
{
	if (!this) //-V704
		return ZERO;

	static auto m_vecAbsVelocity = util::find_in_datamap(GetPredDescMap(), crypt_str("m_vecAbsVelocity"));
	return *(Vector*)(uintptr_t(this) + m_vecAbsVelocity);
}

float player_t::get_max_desync_delta()
{
	if (!this) //-V704
		return 0.0f;

	auto animstate = get_animation_state();

	if (!animstate)
		return 0.0f;

	auto speedfactor = math::clamp(animstate->m_flFeetSpeedForwardsOrSideWays, 0.0f, 1.0f);
	auto avg_speedfactor = (animstate->m_flStopToFullRunningFraction * -0.3f - 0.2f) * speedfactor + 1.0f;

	auto duck_amount = animstate->m_fDuckAmount;

	if (duck_amount) //-V550
	{
		auto max_velocity = math::clamp(animstate->m_flFeetSpeedUnknownForwardOrSideways, 0.0f, 1.0f);
		auto duck_speed = duck_amount * max_velocity;

		avg_speedfactor += duck_speed * (0.5f - avg_speedfactor);
	}

	return animstate->yaw_desync_adjustment() * avg_speedfactor;
}


float player_t::GetDsyncDelta()
{
	if (!this) //-V704
		return 0.0f;

	auto animstate = GetAnimState();

	if (!animstate)
		return 0.0f;

	float flAimMatrixWidthRange = math::lerp(std::clamp(this->GetAnimState()->m_flSpeedAsPortionOfWalkTopSpeed, 0.0f, 1.0f), 1.0f, math::lerp(this->GetAnimState()->m_flWalkToRunTransition, 0.8f, 0.5f)); //-V807
	if (this->GetAnimState()->m_flAnimDuckAmount > 0)
		flAimMatrixWidthRange = math::lerp(this->GetAnimState()->m_flAnimDuckAmount * std::clamp(this->GetAnimState()->m_flSpeedAsPortionOfCrouchTopSpeed, 0.0f, 1.0f), flAimMatrixWidthRange, 0.5f);

	float dsy = flAimMatrixWidthRange * this->GetAnimState()->m_flAimYawMax;

	return dsy;
}

void player_t::invalidate_physics_recursive(int change_flags)
{
	static auto m_uInvalidatePhysics = util::FindSignature(crypt_str("client.dll"), crypt_str("55 8B EC 83 E4 F8 83 EC 0C 53 8B 5D 08 8B C3 56 83 E0 04"));
	reinterpret_cast <void(__thiscall*)(void*, int)> (m_uInvalidatePhysics)(this, change_flags);
}

float& viewmodel_t::m_flCycle()
{
	static auto m_flCycle = util::find_in_datamap(GetPredDescMap(), crypt_str("m_flCycle"));
	return *(float*)(uintptr_t(this) + m_flCycle);
}

float& viewmodel_t::m_flAnimTime()
{
	static auto m_flAnimTime = util::find_in_datamap(GetPredDescMap(), crypt_str("m_flAnimTime"));
	return *(float*)(uintptr_t(this) + m_flAnimTime);
}

void viewmodel_t::SendViewModelMatchingSequence(int sequence)
{
	using Fn = void(__thiscall*)(void*, int);
	call_virtual <Fn>(this, g_ctx.indexes.at(14))(this, sequence);
}

void CHudChat::chat_print(const char* fmt, ...)
{
	char msg[1024];
	va_list args;
	va_start(args, fmt);
	vsnprintf(msg, 1024, fmt, args);
	call_virtual <void(__cdecl*)(void*, int, int, const char*, ...)>(this, g_ctx.indexes.at(15))(this, 0, 0, fmt);
	va_end(args);
}