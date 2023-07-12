#include "animation_system.h"
#include "..\misc\misc.h"
#include "..\misc\logs.h"
#include "../../utils/threadmanager.hpp"
#include "../MultiThread/Multithread.hpp"
std::deque <adjust_data> player_records[65];

enum ADVANCED_ACTIVITY : int
{
	ACTIVITY_NONE = 0,
	ACTIVITY_JUMP,
	ACTIVITY_LAND
};//

float lagcompensation::GetAngle(player_t* player) {
	return math::normalize_yaw(player->m_angEyeAngles().y);
}

void lagcompensation::apply_interpolation_flags(player_t* e)
{
	auto map = e->var_mapping();
	if (map == nullptr)
		return;
	for (auto j = 0; j < map->m_nInterpolatedEntries; j++)
		map->m_Entries[j].m_bNeedsToInterpolate = false;
}

//mxvement
struct EntityJobDataStruct
{
	int index;
	ClientFrameStage_t stage;
};


void ProcessEntityJob(EntityJobDataStruct* EntityJobData)
{
	int i = EntityJobData->index;
	ClientFrameStage_t stage = EntityJobData->stage;

	auto e = static_cast<player_t*>(m_entitylist()->GetClientEntity(i));

	if (e == g_ctx.local())
		return;

	if (!lagcompensation::get().valid(i, e))
		return;

	auto time_delta = abs(TIME_TO_TICKS(e->m_flSimulationTime()) - m_globals()->m_tickcount);

	if (time_delta > 1.0f / m_globals()->m_intervalpertick)
		return;

	auto update = player_records[i].empty() || e->m_flSimulationTime() != e->m_flOldSimulationTime();
	if (update && !player_records[i].empty())
	{
		auto server_tick = m_clientstate()->m_iServerTick - i % m_globals()->m_timestamprandomizewindow;
		auto current_tick = server_tick - server_tick % m_globals()->m_timestampnetworkingbase;

		if (TIME_TO_TICKS(e->m_flOldSimulationTime()) < current_tick && TIME_TO_TICKS(e->m_flSimulationTime()) == current_tick)
		{
			auto layer = &e->get_animlayers()[11];
			auto previous_layer = &player_records[i].front().layers[11];

			if (layer->m_flCycle == previous_layer->m_flCycle) //-V550
			{
				e->m_flSimulationTime() = e->m_flOldSimulationTime();
				update = false;
			}
		}
	}

	switch (stage)
	{
	case FRAME_NET_UPDATE_POSTDATAUPDATE_END:
		lagcompensation::get().apply_interpolation_flags(e);
		break;
	case FRAME_NET_UPDATE_END:
		if (update)
		{
			if (!player_records[i].empty() && (e->m_vecOrigin() - player_records[i].front().origin).LengthSqr() > 4096.0f) {
				for (auto& record : player_records[i])
					record.invalid = true;
			}

			player_records[i].emplace_front(adjust_data(e));
			lagcompensation::get().update_player_animations(e);

			while (player_records[i].size() > 32)
				player_records[i].pop_back();
		}
		break;
	case FRAME_RENDER_START:
		e->set_abs_origin(e->m_vecOrigin());
		lagcompensation::get().FixPvs(e);
		break;
	}
}

void lagcompensation::fsn(ClientFrameStage_t stage)
{
	if (!g_cfg.ragebot.enable)
		return;

	
	std::vector<EntityJobDataStruct> jobDataVec(m_globals()->m_maxclients - 1);

	// Prepare the job data
	for (int i = 1; i < m_globals()->m_maxclients; i++)
	{
		EntityJobDataStruct jobData;
		jobData.index = i;
		jobData.stage = stage;
		jobDataVec[i - 1] = jobData;
	}

	// Enqueue the jobs 
	for (auto& jobData : jobDataVec)
	{
		Threading::QueueJobRef(ProcessEntityJob, &jobData);
	}

	// Wait for all the jobs to finish 
	Threading::FinishQueue();
}


//void lagcompensation::fsn(ClientFrameStage_t stage)
//{
//	if (!g_cfg.ragebot.enable)
//		return;
//
//	for (int i = 1; i < m_globals()->m_maxclients; i++) //-V807
//	{
//		auto e = static_cast<player_t*>(m_entitylist()->GetClientEntity(i));
//
//		if (e == g_ctx.local())
//			continue;
//
//		if (!valid(i, e))
//			continue;
//
//		auto time_delta = abs(TIME_TO_TICKS(e->m_flSimulationTime()) - m_globals()->m_tickcount);
//
//		if (time_delta > 1.0f / m_globals()->m_intervalpertick)
//			continue;
//
//		auto update = player_records[i].empty() || e->m_flSimulationTime() != e->m_flOldSimulationTime();
//		if (update && !player_records[i].empty())
//		{
//			auto server_tick = m_clientstate()->m_iServerTick - i % m_globals()->m_timestamprandomizewindow;
//			auto current_tick = server_tick - server_tick % m_globals()->m_timestampnetworkingbase;
//
//			if (TIME_TO_TICKS(e->m_flOldSimulationTime()) < current_tick && TIME_TO_TICKS(e->m_flSimulationTime()) == current_tick)
//			{
//				auto layer = &e->get_animlayers()[11];
//				auto previous_layer = &player_records[i].front().layers[11];
//
//				if (layer->m_flCycle == previous_layer->m_flCycle) //-V550
//				{
//					e->m_flSimulationTime() = e->m_flOldSimulationTime();
//					update = false;
//				}
//			}
//		}
//
//		switch (stage)
//		{
//		case FRAME_NET_UPDATE_POSTDATAUPDATE_END:
//			apply_interpolation_flags(e);
//			break;
//		case FRAME_NET_UPDATE_END:
//			if (update)
//			{
//				if (!player_records[i].empty() && (e->m_vecOrigin() - player_records[i].front().origin).LengthSqr() > 4096.0f) {
//					for (auto& record : player_records[i])
//						record.invalid = true;
//				}
//
//				player_records[i].emplace_front(adjust_data(e));
//				update_player_animations(e);
//
//				while (player_records[i].size() > 32)
//					player_records[i].pop_back();
//			}
//			break;
//		case FRAME_RENDER_START:
//			e->set_abs_origin(e->m_vecOrigin());
//			FixPvs();
//			break;
//		}
//
//	}
//}


Vector lagcompensation::DeterminePlayerVelocity(player_t* pPlayer, adjust_data* m_LagRecord, adjust_data* m_PrevRecord, c_baseplayeranimationstate* m_AnimationState)
{
	float m_flMaxSpeed = 260.0f;
	auto pCombatWeapon = pPlayer->m_hActiveWeapon();
	auto pCombatWeaponData = pCombatWeapon->get_csweapon_info();

	if (!pCombatWeaponData || !pCombatWeapon)
		m_flMaxSpeed = 260.0f;
	else
	{
		if (pCombatWeapon->m_weaponMode() == 0)
			m_flMaxSpeed = pCombatWeaponData->flMaxPlayerSpeed;
		else if (pCombatWeapon->m_weaponMode() == 1)
			m_flMaxSpeed = pCombatWeaponData->flMaxPlayerSpeedAlt;
	}

	/* Prepare data once */
	if (!m_PrevRecord)
	{
		const float flVelLength = m_LagRecord->velocity.Length();
		if (flVelLength > m_flMaxSpeed)
			m_LagRecord->velocity *= m_flMaxSpeed / flVelLength;

		return m_LagRecord->velocity;
	}

	/* Define const */
	//const float flMaxSpeed = SDK::EngineData::m_ConvarList[CheatConvarList::MaxSpeed]->GetFloat();


	/* Get animation layers */
	const AnimationLayer* m_AliveLoop = &m_LagRecord->layers[ANIMATION_LAYER_ALIVELOOP];
	const AnimationLayer* m_PrevAliveLoop = &m_PrevRecord->layers[ANIMATION_LAYER_ALIVELOOP];

	const AnimationLayer* m_Movement = &m_LagRecord->layers[ANIMATION_LAYER_MOVEMENT_MOVE];
	const AnimationLayer* m_PrevMovement = &m_PrevRecord->layers[ANIMATION_LAYER_MOVEMENT_MOVE];
	const AnimationLayer* m_Landing = &m_LagRecord->layers[ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB];
	const AnimationLayer* m_PrevLanding = &m_PrevRecord->layers[ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB];

	/* Recalculate velocity using origin delta */
	m_LagRecord->velocity = (m_LagRecord->origin - m_PrevRecord->origin) * (1.0f / TICKS_TO_TIME(m_LagRecord->simulation_time));


	/* Check PlaybackRate */
	if (m_Movement->m_flPlaybackRate < 0.00001f)
		m_LagRecord->velocity.x = m_LagRecord->velocity.y = 0.0f;
	else
	{
		/* Compute velocity using m_flSpeedAsPortionOfRunTopSpeed */
		float flWeight = m_AliveLoop->m_flWeight;
		if (flWeight < 1.0f)
		{

			/* Check PlaybackRate */
			if (m_AliveLoop->m_flPlaybackRate == m_PrevAliveLoop->m_flPlaybackRate)
			{
				/* Check Sequence */
				if (m_AliveLoop->m_nSequence == m_PrevAliveLoop->m_nSequence)
				{

					/* Very important cycle check */
					if (m_AliveLoop->m_flCycle > m_PrevAliveLoop->m_flCycle)
					{
						/* Check weapon */
						if (m_AnimationState->m_pActiveWeapon == pPlayer->m_hActiveWeapon())
						{
							/* Get m_flSpeedAsPortionOfRunTopSpeed */
							float m_flSpeedAsPortionOfRunTopSpeed = ((1.0f - flWeight) / 2.8571432f) + 0.55f;


							/* Check m_flSpeedAsPortionOfRunTopSpeed bounds ( from 55% to 90% from the speed ) */
							if (m_flSpeedAsPortionOfRunTopSpeed > 0.55f && m_flSpeedAsPortionOfRunTopSpeed < 0.9f)
							{

								/* Compute velocity */
								m_LagRecord->m_flAnimationVelocity = m_flSpeedAsPortionOfRunTopSpeed * m_flMaxSpeed;
								m_LagRecord->m_nVelocityMode = EFixedVelocity::AliveLoopLayer;
							}
							else if (m_flSpeedAsPortionOfRunTopSpeed > 0.9f)
							{

								

								/* Compute velocity */
								m_LagRecord->m_flAnimationVelocity = m_LagRecord->velocity.Length2D();
							}
						}
					}
				}
			}
		}

		/* Compute velocity using Movement ( 6 ) weight  */
		if (m_LagRecord->m_flAnimationVelocity <= 0.0f)
		{

			

			/* Check Weight bounds from 10% to 90% from the speed */
			float flWeight = m_Movement->m_flWeight;
			if (flWeight > 0.1f && flWeight < 0.9f)
			{
				/* Skip on land */
				if (m_Landing->m_flWeight <= 0.0f)
				{

					

					/* Check Accelerate */
					if (flWeight > m_PrevMovement->m_flWeight)
					{
						/* Skip on direction switch */
						if (m_LagRecord->layers[ANIMATION_LAYER_MOVEMENT_STRAFECHANGE].m_nSequence == m_PrevRecord->layers[ANIMATION_LAYER_MOVEMENT_STRAFECHANGE].m_nSequence)
						{

							

							/* Check move sequence */
							if (m_Movement->m_nSequence == m_PrevMovement->m_nSequence)
							{
								/* Check land sequence */
								if (m_Landing->m_nSequence == m_PrevLanding->m_nSequence)
								{

									

									/* Check stand sequence */
									if (m_LagRecord->layers[ANIMATION_LAYER_ADJUST].m_nSequence == m_PrevRecord->layers[ANIMATION_LAYER_ADJUST].m_nSequence)
									{
										/* Check Flags */
										if (m_LagRecord->flags & FL_ONGROUND)
										{

											

											/* Compute MaxSpeed modifier */
											float flSpeedModifier = 1.0f;
											if (m_LagRecord->flags & FL_DUCKING)
												flSpeedModifier = 0.34f;
											else if (pPlayer->m_bIsWalking())
												flSpeedModifier = 0.52f;


											

											/* Compute Velocity ( THIS CODE ONLY WORKS IN DUCK AND WALK ) */
											if (flSpeedModifier < 1.0f)
											{
												m_LagRecord->m_flAnimationVelocity = (flWeight * (m_flMaxSpeed * flSpeedModifier));
												m_LagRecord->m_nVelocityMode = EFixedVelocity::MovementLayer;
											}
										}
									}
								}
							}
						}
					}

					

				}
			}
		}
	}

	/* Compute velocity from m_Record->m_flAnimationVelocity floating point */
	if (m_LagRecord->m_flAnimationVelocity > 0.0f)
	{
		const float flModifier = m_LagRecord->m_flAnimationVelocity / m_LagRecord->velocity.Length2D();
		m_LagRecord->velocity.x *= flModifier;

		

		m_LagRecord->velocity.y *= flModifier;
	}

	/* Prepare data once */
	const float flVelLength = m_LagRecord->velocity.Length();


	

	/* Clamp velocity if its out bounds */
	if (flVelLength > m_flMaxSpeed)
		m_LagRecord->velocity *= m_flMaxSpeed / flVelLength;

	return m_LagRecord->velocity;
}


void lagcompensation::Extrapolate(player_t* pEntity, Vector& vecOrigin, Vector& vecVelocity, int& fFlags, bool bOnGround) {
	Vector                start, end, normal;
	CGameTrace            trace;
	Ray_t                 ray;
	CTraceFilterWorldOnly filter;

	// define trace start.
	start = vecOrigin;

	// move trace end one tick into the future using predicted velocity.
	end = start + (vecVelocity * m_globals()->m_intervalpertick);

	// trace.
	ray.Init(start, end, pEntity->GetCollideable()->OBBMins(), pEntity->GetCollideable()->OBBMaxs());

	g_ctx.globals.autowalling = true;
	m_trace()->TraceRay(ray, CONTENTS_SOLID, &filter, &trace);
	g_ctx.globals.autowalling = false;

	// we hit shit
	// we need to fix shit.
	if (trace.fraction != 1.f) {

		// fix sliding on planes.
		for (int i = 0; i < 2; ++i) {
			vecVelocity -= trace.plane.normal * vecVelocity.Dot(trace.plane.normal);

			float adjust = vecVelocity.Dot(trace.plane.normal);
			if (adjust < 0.f)
				vecVelocity -= (trace.plane.normal * adjust);

			start = trace.endpos;
			end = start + (vecVelocity * (m_globals()->m_intervalpertick * (1.f - trace.fraction)));

			ray.Init(start, end, pEntity->GetCollideable()->OBBMins(), pEntity->GetCollideable()->OBBMaxs());

			g_ctx.globals.autowalling = true;
			m_trace()->TraceRay(ray, CONTENTS_SOLID, &filter, &trace);
			g_ctx.globals.autowalling = false;

			if (trace.fraction == 1.f)
				break;
		}
	}

	// set new final origin.
	start = end = vecOrigin = trace.endpos;

	// move endpos 2 units down.
	// this way we can check if we are in/on the ground.
	end.z -= 2.f;

	// trace.
	ray.Init(start, end, pEntity->GetCollideable()->OBBMins(), pEntity->GetCollideable()->OBBMaxs());

	g_ctx.globals.autowalling = true;
	m_trace()->TraceRay(ray, CONTENTS_SOLID, &filter, &trace);
	g_ctx.globals.autowalling = false;

	// strip onground flag.
	fFlags &= ~FL_ONGROUND;

	// add back onground flag if we are onground.
	if (trace.fraction != 1.f && trace.plane.normal.z > 0.7f)
		fFlags |= FL_ONGROUND;
}

bool lagcompensation::valid(int i, player_t* e)
{
	if (!g_cfg.ragebot.enable || !e->valid(false))
	{
		if (!e->is_alive())
		{
			is_dormant[i] = false;
			player_resolver[i].reset();
			//player_resolver[i].reset_resolver();

			g_ctx.globals.fired_shots[i] = 0;
			g_ctx.globals.missed_shots[i] = 0;


		}
		else if (e->IsDormant())
			is_dormant[i] = true;

		player_records[i].clear();
		return false;
	}

	return true;
}

float Bias(float x, float biasAmt)
{
	// WARNING: not thread safe
	static float lastAmt = -1;
	static float lastExponent = 0;
	if (lastAmt != biasAmt)
	{
		lastExponent = log(biasAmt) * -1.4427f; // (-1.4427 = 1 / log(0.5))
	}
	return pow(x, lastExponent);
}


template < class T >
static T interpolate(const T& current, const T& target, const int progress, const int maximum)
{
	return current + ((target - current) / maximum) * progress;
}

static auto ticksToTime(int ticks) noexcept { return static_cast<float>(ticks * m_globals()->m_intervalpertick); }



void lagcompensation::setup_matrix(player_t* e, AnimationLayer* layers, const int& matrix , adjust_data* record)
{
	e->invalidate_physics_recursive(8);

	AnimationLayer backup_layers[13];
	memcpy(backup_layers, e->get_animlayers(), e->animlayer_count() * sizeof(AnimationLayer));

	memcpy(e->get_animlayers(), layers, e->animlayer_count() * sizeof(AnimationLayer));

	switch (matrix)
	{
	case MAIN:
		e->setup_bones_rebuilt(record->m_Matricies[MatrixBoneSide::MiddleMatrix].data(), BONE_USED_BY_ANYTHING);
		break;
	case FIRST:
		e->setup_bones_rebuilt(record->m_Matricies[MatrixBoneSide::LeftMatrix].data(), BONE_USED_BY_HITBOX);
		break;
	case SECOND:
		e->setup_bones_rebuilt(record->m_Matricies[MatrixBoneSide::RightMatrix].data(), BONE_USED_BY_HITBOX);
		break;
	case LOW_FIRST:
		e->setup_bones_rebuilt(record->m_Matricies[MatrixBoneSide::LowLeftMatrix].data(), BONE_USED_BY_HITBOX);
		break;
	case LOW_SECOND:
		e->setup_bones_rebuilt(record->m_Matricies[MatrixBoneSide::LowRightMatrix].data(), BONE_USED_BY_HITBOX);
		break;
	}

	memcpy(e->get_animlayers(), backup_layers, e->animlayer_count() * sizeof(AnimationLayer));
}
void lagcompensation::HandleDormancyLeaving(player_t* pPlayer, adjust_data* m_Record, c_baseplayeranimationstate* m_AnimationState)
{
	/* Get animation layers */
	const AnimationLayer* m_JumpingLayer = &m_Record->layers[ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL];
	const AnimationLayer* m_LandingLayer = &m_Record->layers[ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB];

	/* Final m_flLastUpdateTime */
	float m_flLastUpdateTime = m_Record->simulation_time - m_globals()->m_intervalpertick;

	/* Fix animation state timing */
	if (m_Record->flags & FL_ONGROUND) /* On ground */
	{
		/* Use information from landing */
		int nActivity = pPlayer->sequence_activity(m_LandingLayer->m_nSequence);
		if (nActivity == ACT_CSGO_LAND_HEAVY || nActivity == ACT_CSGO_LAND_LIGHT)
		{
			/* Compute land duration */
			float flLandDuration = m_LandingLayer->m_flCycle / m_LandingLayer->m_flPlaybackRate;

			/* Check landing time */
			float flLandingTime = m_Record->simulation_time - flLandDuration;
			if (flLandingTime == m_flLastUpdateTime)
			{
				m_AnimationState->m_bOnGround = true;
				m_AnimationState->m_bInHitGroundAnimation = true;
				m_AnimationState->m_fLandingDuckAdditiveSomething = 0.0f;
			}
			else if (flLandingTime - m_globals()->m_intervalpertick == m_flLastUpdateTime)
			{
				m_AnimationState->m_bOnGround = false;
				m_AnimationState->m_bInHitGroundAnimation = false;
				m_AnimationState->m_fLandingDuckAdditiveSomething = 0.0f;
			}

			/* Determine duration in air */
			float flDurationInAir = (m_JumpingLayer->m_flCycle - m_LandingLayer->m_flCycle);
			if (flDurationInAir < 0.0f)
				flDurationInAir += 1.0f;

			/* Set time in air */
			m_AnimationState->time_since_in_air() = flDurationInAir / m_JumpingLayer->m_flPlaybackRate;

			/* Check bounds.*/
			/* There's two conditions to let this data be useful: */
			/* It's useful if player has landed after the latest client animation update */
			/* It's useful if player has landed before the previous tick */
			if (flLandingTime < m_flLastUpdateTime && flLandingTime > m_AnimationState->m_flLastClientSideAnimationUpdateTime)
				m_flLastUpdateTime = flLandingTime;
		}
	}
	else /* In air */
	{
		/* Use information from jumping */
		int nActivity = pPlayer->sequence_activity(m_JumpingLayer->m_nSequence);
		if (nActivity == ACT_CSGO_JUMP)
		{
			/* Compute duration in air */
			float flDurationInAir = m_JumpingLayer->m_flCycle / m_JumpingLayer->m_flPlaybackRate;

			/* Check landing time */
			float flJumpingTime = m_Record->simulation_time - flDurationInAir;
			if (flJumpingTime <= m_flLastUpdateTime)
				m_AnimationState->m_bOnGround = false;
			else if (flJumpingTime - m_globals()->m_intervalpertick)
				m_AnimationState->m_bOnGround = true;

			/* Check bounds.*/
			/* There's two conditions to let this data be useful: */
			/* It's useful if player has jumped after the latest client animation update */
			/* It's useful if player has jumped before the previous tick */
			if (flJumpingTime < m_flLastUpdateTime && flJumpingTime > m_AnimationState->m_flLastClientSideAnimationUpdateTime)
				m_flLastUpdateTime = flJumpingTime;

			/* Set time in air */
			m_AnimationState->time_since_in_air() = flDurationInAir - m_globals()->m_intervalpertick;

			/* Disable landing */
			m_AnimationState->m_bInHitGroundAnimation = false;
		}
	}

	/* Set m_flLastUpdateTime */
	m_AnimationState->m_flLastClientSideAnimationUpdateTime = m_flLastUpdateTime;
}

void lagcompensation::SetupCollision(player_t* pPlayer, adjust_data* m_LagRecord)
{
	ICollideable* m_Collideable = pPlayer->GetCollideable();
	if (!m_Collideable)
		return;
	
	pPlayer->UpdateCollisionBounds();// sig = ( "client.dll" ), _S( "56 57 8B F9 8B 0D ? ? ? ? F6 87 04 01" );
	pPlayer->SetCollisionBounds(m_LagRecord->mins, m_LagRecord->maxs);
}

void lagcompensation::SetupBones(player_t* pPlayer, int nBoneMask, matrix3x4_t* aMatrix)
{
	/* rebuild setup bones later */
	g_ctx.globals.setuping_bones = true;
	//pPlayer->SetupBones(aMatrix, MAXSTUDIOBONES, nBoneMask, 0.0f);
	//e->setup_bones_rebuilt(record->m_Matricies[MiddleMatrix].data(), BONE_USED_BY_HITBOX);
	pPlayer->setup_bones_rebuilt(aMatrix, nBoneMask);
	g_ctx.globals.setuping_bones = false;
}
void lagcompensation::SetupPlayerMatrix(player_t* pPlayer, adjust_data* m_Record, matrix3x4_t* Matrix, int nFlags)
{
	/* Reset layers */
	std::memcpy(pPlayer->get_animlayers(), m_Record->layers, sizeof(AnimationLayer) * 13);

	/* Store game's globals */
	GameGlobals_t Globals;
	Globals.CaptureData();

	/* Store player's data */
	std::tuple < int, int, int, int, int, bool, Vector > m_PlayerData = std::make_tuple
	(
		pPlayer->m_nLastSkipFramecount(),
		pPlayer->m_fEffects(),
		pPlayer->m_nClientEffects(),
		pPlayer->m_nOcclusionFrame(),
		pPlayer->m_nOcclusionMask(),
		pPlayer->m_bJiggleBones(),
		pPlayer->GetAbsOrigin()
	);

	/* Force game's globals */
	int nSimulationTick = TIME_TO_TICKS(m_Record->simulation_time);
	m_globals()->m_curtime = m_Record->simulation_time;
	m_globals()->m_realtime = m_Record->simulation_time;
	m_globals()->m_frametime = m_globals()->m_intervalpertick;
	m_globals()->m_absoluteframetime = m_globals()->m_intervalpertick;
	m_globals()->m_tickcount = nSimulationTick;
	m_globals()->m_framecount = INT_MAX; /* ShouldSkipAnimationFrame fix */
	m_globals()->m_interpolation_amount = 0.0f;

	/* Force it https://github.com/perilouswithadollarsign/cstrike15_src/blob/f82112a2388b841d72cb62ca48ab1846dfcc11c8/game/client/c_baseanimating.cpp#L3102 */
	pPlayer->invalidate_bone_cache();

	/* Force the owner of animation layers */
	for (int iLayer = 0; iLayer < 13; iLayer++)
	{
		AnimationLayer* m_Layer = &pPlayer->get_animlayers()[iLayer];
		if (!m_Layer)
			continue;

		m_Layer->m_pOwner = pPlayer;
	}

	/* Disable ACT_CSGO_IDLE_TURN_BALANCEADJUST animation */
	if (nFlags & EMatrixFlags::VisualAdjustment)
	{
		pPlayer->get_animlayers()[ANIMATION_LAYER_LEAN].m_flWeight = 0.0f;
		if (pPlayer->sequence_activity(pPlayer->get_animlayers()[ANIMATION_LAYER_ADJUST].m_nSequence) == ACT_CSGO_IDLE_TURN_BALANCEADJUST)
		{
			pPlayer->get_animlayers()[ANIMATION_LAYER_ADJUST].m_flCycle = 0.0f;
			pPlayer->get_animlayers()[ANIMATION_LAYER_ADJUST].m_flWeight = 0.0f;
		}
	}

	/* Remove interpolation if required */
	if (!(nFlags & EMatrixFlags::Interpolated))
		pPlayer->set_abs_origin(m_Record->origin);

	/* Compute bone mask */
	int nBoneMask = BONE_USED_BY_ANYTHING;
	if (nFlags & EMatrixFlags::BoneUsedByHitbox)
		nBoneMask = BONE_USED_BY_HITBOX;

	/* Fix player's data */
	pPlayer->m_bJiggleBones() = false;
	pPlayer->m_nClientEffects() |= 2;
	pPlayer->m_fEffects() |= EF_NOINTERP;
	pPlayer->m_nOcclusionFrame() = -1;
	pPlayer->m_nOcclusionMask() &= ~2;
	pPlayer->m_nLastSkipFramecount() = 0;

	/* Setup bones */
	SetupBones(pPlayer, nBoneMask, Matrix);

	/* Restore player's data */
	pPlayer->m_nLastSkipFramecount() = std::get < 0 >(m_PlayerData);
	pPlayer->m_fEffects() = std::get < 1 >(m_PlayerData);
	pPlayer->m_nClientEffects() = std::get < 2 >(m_PlayerData);
	pPlayer->m_nOcclusionFrame() = std::get < 3 >(m_PlayerData);
	pPlayer->m_nOcclusionMask() = std::get < 4 >(m_PlayerData);
	pPlayer->m_bJiggleBones() = std::get < 5 >(m_PlayerData);
	pPlayer->set_abs_origin(std::get < 6 >(m_PlayerData));

	/* Reset layers */
	std::memcpy(pPlayer->get_animlayers(), m_Record->layers, sizeof(AnimationLayer) * 13);

	/* Restore game's globals */
	Globals.AdjustData();
}
float lagcompensation::BuildFootYaw(player_t* pPlayer, adjust_data* m_LagRecord)
{
	return player_resolver->b_yaw(pPlayer , pPlayer->m_angEyeAngles().y , 5);
}
void lagcompensation::UpdatePlayerAnimations(player_t* pPlayer, adjust_data* m_LagRecord, c_baseplayeranimationstate* m_AnimationState)
{

	/* Bypass this check https://github.com/perilouswithadollarsign/cstrike15_src/blob/f82112a2388b841d72cb62ca48ab1846dfcc11c8/game/shared/cstrike15/csgo_playeranimstate.cpp#L266 */
	m_AnimationState->m_iLastClientSideAnimationUpdateFramecount = 0;
	if (m_AnimationState->m_flLastClientSideAnimationUpdateTime == m_globals()->m_curtime)
		m_AnimationState->m_flLastClientSideAnimationUpdateTime = m_globals()->m_curtime + m_globals()->m_intervalpertick;

	/* Force animation state player and previous weapon */
	m_AnimationState->m_pBaseEntity = pPlayer;
	m_AnimationState->m_pLastActiveWeapon = pPlayer->m_hActiveWeapon();

	/* Force the owner of animation layers */
	for (int iLayer = 0; iLayer < 13; iLayer++)
	{
		AnimationLayer* m_Layer = &pPlayer->get_animlayers()[iLayer];
		if (!m_Layer)
			continue;

		m_Layer->m_pOwner = pPlayer;
	}
	bool bClientSideAnimation = pPlayer->m_bClientSideAnimation();
	pPlayer->m_bClientSideAnimation() = true;

	g_ctx.globals.updating_animation = true;
	pPlayer->update_clientside_animation();
	g_ctx.globals.updating_animation = false;
	pPlayer->m_bClientSideAnimation() = bClientSideAnimation;
}
void lagcompensation::GenerateSafePoints(player_t* pPlayer, adjust_data* m_LagRecord)
{
	auto GetYawRotation = [&](int nRotationSide) -> float
	{
		float flOldEyeYaw = pPlayer->m_angEyeAngles().y;

		// set eye yaw
		float flEyeRotation = pPlayer->m_angEyeAngles().y;

		float flRotation;
		switch (nRotationSide)
		{
		case -1: flRotation = -60.0f; break;
		case 0: flRotation = 0.0f; break;
		case 1: flRotation = +60.0f; break;
		}

		// generate foot yaw
		//float flFootYaw = BuildFootYaw(pPlayer, m_LagRecord);
		float flFootYaw = math::normalize_yaw(pPlayer->m_angEyeAngles().y + flRotation);

		// restore eye yaw                                   
		pPlayer->m_angEyeAngles().y = flOldEyeYaw;

		// return result
		return flFootYaw;
	};

	// point building func
	auto BuildSafePoint = [&](int nRotationSide)
	{
		// save animation data
		std::array < AnimationLayer, 13 > m_Layers;
		std::array < float, 24 > m_PoseParameters;
		c_baseplayeranimationstate m_AnimationState;
		// copy data
		std::memcpy(m_Layers.data(), pPlayer->get_animlayers(), sizeof(AnimationLayer) * 13);
		std::memcpy(m_PoseParameters.data(), pPlayer->m_flPoseParameter().data(), sizeof(float) * 24);
		std::memcpy(&m_AnimationState, pPlayer->get_animation_state(), sizeof(c_baseplayeranimationstate));

		// set foot yaw
		pPlayer->get_animation_state()->m_flGoalFeetYaw = GetYawRotation(nRotationSide);

		// update player animations
		UpdatePlayerAnimations(pPlayer, m_LagRecord, pPlayer->get_animation_state());

		// get matrix
		matrix3x4_t* aMatrix = nullptr;
		switch (nRotationSide)
		{
		case -1: aMatrix = m_LagRecord->m_Matricies[1].data(); break;
		case 0: aMatrix = m_LagRecord->m_Matricies[2].data(); break;
		case 1: aMatrix = m_LagRecord->m_Matricies[3].data(); break;
		}

		// setup bones
		SetupPlayerMatrix(pPlayer, m_LagRecord, aMatrix, EMatrixFlags::BoneUsedByHitbox);

		// restore data
		std::memcpy(pPlayer->get_animlayers(), m_Layers.data(), sizeof(AnimationLayer) * 13);
		std::memcpy(pPlayer->m_flPoseParameter().data(), m_PoseParameters.data(), sizeof(float) * 24);
		std::memcpy(pPlayer->get_animation_state(), &m_AnimationState, sizeof(c_baseplayeranimationstate));
	};

	// check conditions
	if (/*!record->bot &&*/ /*g_ctx.local()->is_alive() &&*/ pPlayer->m_iTeamNum() != g_ctx.local()->m_iTeamNum() && !g_cfg.legitbot.enabled && !g_ctx.local()->is_alive())
		return;

	// build safe points
	BuildSafePoint(-1);
	BuildSafePoint(0);
	BuildSafePoint(1);
}
void lagcompensation::update_player_animations(player_t* e)
{
	auto animstate = e->get_animation_state();

	if (!animstate)
		return;

	player_info_t player_info;

	if (!m_engine()->GetPlayerInfo(e->EntIndex(), &player_info))
		return;

	auto records = &player_records[e->EntIndex()]; //-V826

	if (records->empty())
		return;

	adjust_data* previous_record = nullptr;

	if (records->size() >= 2 && !records->at(1).invalid)
		previous_record = &records->at(1);

	auto record = &records->front();

	AnimationLayer animlayers[15];

	memcpy(animlayers, e->get_animlayers(), e->animlayer_count() * sizeof(AnimationLayer));
	memcpy(record->layers, animlayers, e->animlayer_count() * sizeof(AnimationLayer));

	/* Determine player's velocity */
    e->m_vecVelocity() = DeterminePlayerVelocity(e, record, previous_record, animstate);
	record->velocity = e->m_vecVelocity();

	/*Handle dormancy*/
	if (is_dormant[e->EntIndex()])
	{
		is_dormant[e->EntIndex()] = false;
		HandleDormancyLeaving(e, record, animstate);
	}

	GameGlobals_t m_Globals;
	m_Globals.CaptureData();

	/* Save player's data */
	std::tuple < Vector, Vector, Vector, int, int, float, float, float > m_Data = std::make_tuple
	(
		e->m_vecVelocity(),
		e->m_vecAbsVelocity(),
		e->GetAbsOrigin(),
		e->m_fFlags(),
		e->m_iEFlags(),
		e->m_flDuckAmount(),
		e->m_flLowerBodyYawTarget(),
		e->m_flThirdpersonRecoil()
	);

	/* Invalidate EFlags */
	e->m_iEFlags() &= ~(EFL_DIRTY_ABSVELOCITY | EFL_DIRTY_ABSTRANSFORM);

	/* Update collision bounds */
	SetupCollision(e, record);


	{
		/* Determine simulation tick */
		int nSimulationTick = TIME_TO_TICKS(e->m_flSimulationTime());

		/* Set global game's data */
		m_globals()->m_curtime = e->m_flSimulationTime();
		m_globals()->m_realtime = e->m_flSimulationTime();
		m_globals()->m_frametime = m_globals()->m_intervalpertick;
		m_globals()->m_absoluteframetime = m_globals()->m_intervalpertick;
		m_globals()->m_framecount = nSimulationTick;
		m_globals()->m_tickcount = nSimulationTick;
		m_globals()->m_interpolation_amount = 0.0f;
	}
	//auto ticks_chocked = 1;
	//int m_iChoked = TIME_TO_TICKS(e->m_flSimulationTime() - previous_record->simulation_time);

	//Vector vecPreviousOrigin = previous_record->origin;
	//int fPreviousFlags = previous_record->flags;

	//// extrapolate
	//for (int i = 0; i < m_iChoked; ++i)
	//{
	//	const float flSimulationTime = previous_record->simulation_time + TICKS_TO_TIME(i + 1);
	//	const float flLerp = 1.f - (record->simulation_time - flSimulationTime) / (record->simulation_time - record->simulation_time);

	//	e->m_flDuckAmount() = math::interpolate(previous_record->duck_amount, record->duck_amount, flLerp);

	//	if (m_iChoked - 1 == i) {
	//		e->m_vecVelocity() = record->velocity;
	//		e->m_fFlags() = record->flags;
	//	}
	//	else {
	//		Extrapolate(e, vecPreviousOrigin, e->m_vecVelocity(), e->m_fFlags(), fPreviousFlags & FL_ONGROUND);
	//		fPreviousFlags = e->m_fFlags();

	//		record->velocity = (record->origin - previous_record->origin) * (1.f / TICKS_TO_TIME(m_iChoked));
	//	}
	//}
	

	auto updated_animations = false;

	c_baseplayeranimationstate state;
	memcpy(&state, animstate, sizeof(c_baseplayeranimationstate));

	if (previous_record)
	{
		auto ticks_chocked = 1;
		auto player = e;
		memcpy(e->get_animlayers(), previous_record->layers, e->animlayer_count() * sizeof(AnimationLayer));

		auto simulation_ticks = TIME_TO_TICKS(e->m_flSimulationTime() - previous_record->simulation_time);

		if (simulation_ticks > 0 && simulation_ticks < 15)
			ticks_chocked = simulation_ticks;
		
		if (ticks_chocked > 1)
		{

			int iActivityTick = 0;
			int iActivityType = 0;

			if (animlayers[ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB].m_flWeight > 0.f && previous_record->layers[ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB].m_flWeight <= 0.f) {
				int iLandSequence = animlayers[ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB].m_nSequence;

				if (iLandSequence > 2) {
					int iLandActivity = e->sequence_activity(iLandSequence);

					if (iLandActivity == ACT_CSGO_LAND_LIGHT || iLandActivity == ACT_CSGO_LAND_HEAVY) {
						float flCurrentCycle = animlayers[ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB].m_flCycle;
						float flCurrentRate = animlayers[ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB].m_flPlaybackRate;

						if (flCurrentCycle > 0.f && flCurrentRate > 0.f) {
							float flLandTime = (flCurrentCycle / flCurrentRate);

							if (flLandTime > 0.f) {
								iActivityTick = TIME_TO_TICKS(e->m_flSimulationTime() - flLandTime) + 1;
								iActivityType = ACTIVITY_LAND;
							}
						}
					}
				}
			}

			if (animlayers[ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL].m_flCycle > 0.f && animlayers[ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL].m_flPlaybackRate > 0.f) {
				int iJumpSequence = animlayers[ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL].m_nSequence;

				if (iJumpSequence > 2) {
					int iJumpActivity = e->sequence_activity(iJumpSequence);

					if (iJumpActivity == ACT_CSGO_JUMP) {
						float flCurrentCycle = animlayers[ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL].m_flCycle;
						float flCurrentRate = animlayers[ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL].m_flPlaybackRate;

						if (flCurrentCycle > 0.f && flCurrentRate > 0.f) {
							float flJumpTime = (flCurrentCycle / flCurrentRate);

							if (flJumpTime > 0.f) {
								iActivityTick = TIME_TO_TICKS(e->m_flSimulationTime() - flJumpTime) + 1;
								iActivityType = ACTIVITY_JUMP;
							}
						}
					}
				}
			}

			for (auto i = 0; i < ticks_chocked; ++i)
			{
				auto simulated_time = previous_record->simulation_time + TICKS_TO_TIME(i);

				//// lerp duck amt.
				e->m_flDuckAmount() = interpolate(previous_record->duck_amount, e->m_flDuckAmount(), i, ticks_chocked);

				// lerp velocity.
				e->m_vecVelocity() = interpolate(previous_record->velocity, e->m_vecVelocity(), i, ticks_chocked);
				e->m_vecAbsVelocity() = e->m_vecVelocity();//e->m_vecAbsVelocity() = interpolate(previous_record->velocity, e->m_vecVelocity(), i, ticks_chocked);
				e->set_abs_origin(e->m_vecOrigin());

				int iCurrentSimulationTick = TIME_TO_TICKS(simulated_time);

				if (iActivityType > ACTIVITY_NONE) {
					bool bIsOnGround = e->m_fFlags() & FL_ONGROUND;

					if (iActivityType == ACTIVITY_JUMP) {
						if (iCurrentSimulationTick == iActivityTick - 1)
							bIsOnGround = true;
						else if (iCurrentSimulationTick == iActivityTick)
						{
							// reset animation layer.
							e->get_animlayers()[ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL].m_flCycle = 0.f;
							e->get_animlayers()[ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL].m_nSequence = animlayers[ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL].m_nSequence;
							e->get_animlayers()[ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL].m_flPlaybackRate = animlayers[ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL].m_flPlaybackRate;

							// reset player ground state.
							bIsOnGround = false;
						}

					}
					else if (iActivityType == ACTIVITY_LAND) {
						if (iCurrentSimulationTick == iActivityTick - 1)
							bIsOnGround = false;
						else if (iCurrentSimulationTick == iActivityTick)
						{
							// reset animation layer.
							e->get_animlayers()[ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB].m_flCycle = 0.f;
							e->get_animlayers()[ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB].m_nSequence = animlayers[ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB].m_nSequence;
							e->get_animlayers()[ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB].m_flPlaybackRate = animlayers[ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB].m_flPlaybackRate;

							// reset player ground state.
							bIsOnGround = true;
						}
					}

					if (bIsOnGround)
						e->m_fFlags() |= FL_ONGROUND;
					else
						e->m_fFlags() &= ~FL_ONGROUND;
				}



				auto simulated_ticks = TIME_TO_TICKS(simulated_time);


				/* Set global game's data */
				m_globals()->m_curtime = simulated_time;
				m_globals()->m_realtime = simulated_time;
				m_globals()->m_framecount = simulated_ticks;
				m_globals()->m_tickcount = simulated_ticks;
				m_globals()->m_interpolation_amount = 0.0f;

				// update player animations
				UpdatePlayerAnimations(e, record, e->get_animation_state());

				m_Globals.AdjustData();

				updated_animations = true;
			}
		}
	}



	if (!updated_animations)
	{
		e->m_vecAbsVelocity() = e->m_vecVelocity();
		// update player animations
		UpdatePlayerAnimations(e, record, e->get_animation_state());
	}

	memcpy(animstate, &state, sizeof(c_baseplayeranimationstate));
	//UpdatePlayerAnimations(e, record, animstate);
	if (/*!record->bot &&*/ /*g_ctx.local()->is_alive() &&*/ e->m_iTeamNum() != g_ctx.local()->m_iTeamNum() && !g_cfg.legitbot.enabled)
	{
		player_resolver[e->EntIndex()].initialize_yaw(e, record);

		/*rebuild setup velocity for more accurate rotations for the resolver and safepoints*/
		//auto eye = e->m_angEyeAngles().y;
		auto idx = e->EntIndex();
		float negative_full = record->left;
		float positive_full = record->right;
		float EyeYaw = record->Eye;
		float Middle = record->Middle;

		float negative_40 = negative_full * 0.5f;
		float positive_40 = positive_full * 0.5f;


		if (g_cfg.player_list.set_cs_low)
		{
			//gonna add resolver override later 
		}

		// --- main --- \\

		animstate->m_flGoalFeetYaw = previous_goal_feet_yaw[e->EntIndex()];
		g_ctx.globals.updating_animation = true;
		e->update_clientside_animation();
		g_ctx.globals.updating_animation = false;
		previous_goal_feet_yaw[e->EntIndex()] = animstate->m_flGoalFeetYaw;
		memcpy(animstate, &state, sizeof(c_baseplayeranimationstate));
		// ------ \\

		// --- none --- \\

		animstate->m_flGoalFeetYaw = Middle;
		g_ctx.globals.updating_animation = true;
		e->update_clientside_animation();
		g_ctx.globals.updating_animation = false;
		setup_matrix(e, animlayers, NONE , record);
		player_resolver[e->EntIndex()].resolver_goal_feet_yaw[0] = animstate->m_flGoalFeetYaw;
		memcpy(animstate, &state, sizeof(c_baseplayeranimationstate));
		memcpy(player_resolver[e->EntIndex()].resolver_layers[0], e->get_animlayers(), e->animlayer_count() * sizeof(AnimationLayer));
		memcpy(e->m_CachedBoneData().Base(), record->m_Matricies[MatrixBoneSide::MiddleMatrix].data(), e->m_CachedBoneData().Count() * sizeof(matrix3x4_t));
		// --- \\

		// --- 60 delta --- \\

		animstate->m_flGoalFeetYaw = EyeYaw + 60.0f;
		g_ctx.globals.updating_animation = true;
		e->update_clientside_animation();
		g_ctx.globals.updating_animation = false;
		setup_matrix(e, animlayers, FIRST, record);
		player_resolver[e->EntIndex()].resolver_goal_feet_yaw[1] = animstate->m_flGoalFeetYaw;
		memcpy(animstate, &state, sizeof(c_baseplayeranimationstate));
		memcpy(player_resolver[e->EntIndex()].resolver_layers[1], e->get_animlayers(), e->animlayer_count() * sizeof(AnimationLayer));
		memcpy(e->m_CachedBoneData().Base(), record->m_Matricies[MatrixBoneSide::LeftMatrix].data(), e->m_CachedBoneData().Count() * sizeof(matrix3x4_t));
		// --- \\




		// --- -60 delta --- \\

		animstate->m_flGoalFeetYaw = EyeYaw - 60.0f;
		g_ctx.globals.updating_animation = true;
		e->update_clientside_animation();
		g_ctx.globals.updating_animation = false;
		setup_matrix(e, animlayers, SECOND, record);
		player_resolver[e->EntIndex()].resolver_goal_feet_yaw[2] = animstate->m_flGoalFeetYaw;
		memcpy(animstate, &state, sizeof(c_baseplayeranimationstate));
		memcpy(player_resolver[e->EntIndex()].resolver_layers[2], e->get_animlayers(), e->animlayer_count() * sizeof(AnimationLayer));
		memcpy(e->m_CachedBoneData().Base(), record->m_Matricies[MatrixBoneSide::RightMatrix].data(), e->m_CachedBoneData().Count() * sizeof(matrix3x4_t));
		// --- \\



		// --- 40 --- \\

		animstate->m_flGoalFeetYaw = EyeYaw + 30;
		g_ctx.globals.updating_animation = true;
		e->update_clientside_animation();
		g_ctx.globals.updating_animation = false;
		setup_matrix(e, animlayers, LOW_FIRST, record);
		player_resolver[e->EntIndex()].resolver_goal_feet_yaw[3] = animstate->m_flGoalFeetYaw;
		memcpy(animstate, &state, sizeof(c_baseplayeranimationstate));
		memcpy(player_resolver[e->EntIndex()].resolver_layers[3], e->get_animlayers(), e->animlayer_count() * sizeof(AnimationLayer));
		memcpy(e->m_CachedBoneData().Base(), record->m_Matricies[MatrixBoneSide::LowLeftMatrix].data(), e->m_CachedBoneData().Count() * sizeof(matrix3x4_t));
		// --- \\




		// --- -40 delta --- \\

		animstate->m_flGoalFeetYaw = EyeYaw - 30;
		g_ctx.globals.updating_animation = true;
		e->update_clientside_animation();
		g_ctx.globals.updating_animation = false;
		setup_matrix(e, animlayers, LOW_SECOND, record);
		player_resolver[e->EntIndex()].resolver_goal_feet_yaw[4] = animstate->m_flGoalFeetYaw;
		memcpy(animstate, &state, sizeof(c_baseplayeranimationstate));
		memcpy(player_resolver[e->EntIndex()].resolver_layers[4], e->get_animlayers(), e->animlayer_count() * sizeof(AnimationLayer));
		memcpy(e->m_CachedBoneData().Base(), record->m_Matricies[MatrixBoneSide::LowRightMatrix].data(), e->m_CachedBoneData().Count() * sizeof(matrix3x4_t));
		// --- \\

		player_resolver[e->EntIndex()].initialize(e, record, previous_goal_feet_yaw[e->EntIndex()], e->m_angEyeAngles().x);
		player_resolver[e->EntIndex()].resolve_desync();

		//e->m_angEyeAngles().x = player_resolver[e->EntIndex()].resolve_pitch();
	}

	UpdatePlayerAnimations(e, record, animstate);


	setup_matrix(e, animlayers, MAIN, record);
	memcpy(e->m_CachedBoneData().Base(), record->m_Matricies[MatrixBoneSide::MiddleMatrix].data(), e->m_CachedBoneData().Count() * sizeof(matrix3x4_t));


		/* Restore player's data */
	e->m_vecVelocity() = std::get < 0 >(m_Data);
	e->m_vecAbsVelocity() = std::get < 1 >(m_Data);
	e->m_fFlags() = std::get < 3 >(m_Data);
	e->m_iEFlags() = std::get < 4 >(m_Data);
	e->m_flDuckAmount() = std::get < 5 >(m_Data);
	e->m_flLowerBodyYawTarget() = std::get < 6 >(m_Data);
	e->m_flThirdpersonRecoil() = std::get < 7 >(m_Data);

	m_Globals.AdjustData();

	memcpy(e->get_animlayers(), animlayers, e->animlayer_count() * sizeof(AnimationLayer));
	memcpy(player_resolver[e->EntIndex()].previous_layers, animlayers, e->animlayer_count() * sizeof(AnimationLayer));

	e->invalidate_physics_recursive(8);
	e->invalidate_bone_cache();

	if (e->m_flSimulationTime() < e->m_flOldSimulationTime())
		record->invalid = true;

	record->store_data(e, true);
}

void lagcompensation::FixPvs(player_t* pCurEntity)
{
	if (pCurEntity == g_ctx.local())
		return;

	if (!pCurEntity
		|| !pCurEntity->is_player()
		|| pCurEntity->EntIndex() == m_engine()->GetLocalPlayer())
		return;

	*reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(pCurEntity) + 0xA30) = m_globals()->m_framecount;
	*reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(pCurEntity) + 0xA28) = 0;
}