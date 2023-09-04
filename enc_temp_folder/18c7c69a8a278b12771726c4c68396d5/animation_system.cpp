#include "animation_system.h"
#include "..\misc\misc.h"
#include "..\misc\logs.h"
#include "../../utils/threadmanager.hpp"
#include "../MultiThread/Multithread.hpp"
#include "AnimSync/LagComp.hpp"
std::deque <adjust_data> player_records[65];
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

	if (!g_Lagcompensation->valid(i, e))
		return;

	else if (stage == FRAME_RENDER_START)
		g_Lagcompensation->FixPvs(e);

	else if (stage == FRAME_NET_UPDATE_END) {

		player_records[i].emplace_front(adjust_data(e));

		auto records = &player_records[e->EntIndex()]; //-V826

		if (records->empty())
			return;

		adjust_data* previous_record = nullptr;

		if (records->size() >= 2)
			previous_record = &records->at(1);

		adjust_data* record = &records->front();

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
					e->m_flSimulationTime() = player_records[i].front().simulation_time;
					e->m_flOldSimulationTime() = player_records[i].front().old_simtime;
					update = false;
				}
			}
		}

		if (update)
		{
			g_Lagcompensation->ProccessShiftingPlayers(e, record, previous_record);
			if (!player_records[i].empty())
				g_Lagcompensation->SimulatePlayerAnimations(e, record, previous_record);
		}

		while (player_records[i].size() > g_Networking->tickrate())
			player_records[i].pop_back();
	}
}

void lagcompensation::ProccessShiftingPlayers(player_t* e, adjust_data* record, adjust_data* previous_record)
{
	if (previous_record && record)
	{
		if (!e || !e->is_alive())
			return;

		/* Check tickbase exploits */
		if (previous_record->simulation_time > record->simulation_time)
			HandleTickbaseShift(e, record);

		/* Invalidate records for defensive and break lc */
		if (record->simulation_time <= record->m_flExploitTime)
		{
			/* Don't care at all about this cases if we have anti-exploit */
			record->invalid = true;
			record->m_bHasBrokenLC = true;
		}

		/* handle break lagcompensation by high speed and fakelags */
		if (previous_record->m_bRestoreData)
		{
			if ((record->origin - previous_record->origin).Length2DSqr() > 4096.0f)
			{
				record->m_bHasBrokenLC = true;
				return CleanPlayer(e, record);
			}
		}

		/* Determine simulation ticks */
		/* Another code for tickbase shift */
		if (record->simulation_time < previous_record->simulation_time)
		{
			record->invalid = true;
			record->m_bHasBrokenLC = true;

			if (previous_record->m_bRestoreData)
				record->m_nSimulationTicks = TIME_TO_TICKS(record->old_simtime - previous_record->old_simtime);
			else
				record->m_nSimulationTicks = previous_record->m_nSimulationTicks;
		}
		else
		{
			if (previous_record->m_bRestoreData)
				record->m_nSimulationTicks = TIME_TO_TICKS(record->simulation_time - previous_record->simulation_time);
			else
				record->m_nSimulationTicks = TIME_TO_TICKS(record->simulation_time - record->old_simtime);
		}
	}
}

void lagcompensation::fsn(ClientFrameStage_t stage)
{
	if (!g_cfg.ragebot.enable)
		return;

	std::vector<EntityJobDataStruct> jobDataVec(m_globals()->m_maxclients);

	// Prepare the job data
	for (int i = 0; i < m_globals()->m_maxclients; i++)
	{
		EntityJobDataStruct jobData;
		jobData.index = i;
		jobData.stage = stage;
		jobDataVec[i] = jobData;
	}

	// Enqueue the jobs 
	for (auto& jobData : jobDataVec)
	{
		Threading::QueueJobRef(ProcessEntityJob, &jobData);
	}

	// Wait for all the jobs to finish 
	Threading::FinishQueue();
}


void lagcompensation::HandleTickbaseShift(player_t* pPlayer, adjust_data* record)
{
	record->m_flExploitTime = pPlayer->m_flSimulationTime();
	//CleanPlayer(pPlayer, record);
}
void lagcompensation::CleanPlayer(player_t* pPlayer, adjust_data* record)
{
	if (!pPlayer)
		return;

	auto m_LagRecords = &player_records[pPlayer->EntIndex()];
	if (!m_LagRecords)
		return;
	m_LagRecords->clear();
	//m_LagRecords->invalid = true;
	//m_LagRecords->reset();
}

Vector lagcompensation::DeterminePlayerVelocity(player_t* pPlayer, adjust_data* m_LagRecord, adjust_data* m_PrevRecord, C_CSGOPlayerAnimationState* m_AnimationState)
{
	
	auto pCombatWeapon = pPlayer->m_hActiveWeapon();
	auto pCombatWeaponData = pCombatWeapon->get_csweapon_info();
	auto m_flMaxSpeed = pCombatWeapon && pCombatWeaponData ? std::max<float>((pPlayer->m_bIsScoped() ? pCombatWeaponData->flMaxPlayerSpeedAlt : pCombatWeaponData->flMaxPlayerSpeed), 0.001f) : 260.0f;;

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
						if (m_AnimationState->m_pWeapon == pPlayer->m_hActiveWeapon())
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
											if (pPlayer->m_flDuckAmount() >= 1.f)
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


void lagcompensation::RecalculateVelocity(player_t* pPlayer, adjust_data* LagRecord, adjust_data* PreviousRecord, C_CSGOPlayerAnimationState* animstate)
{
	Vector vecVelocity = LagRecord->velocity;
	float_t flWeightSpeed = LagRecord->layers[ANIMATION_LAYER_MOVEMENT_MOVE].m_flWeight;

	static auto sv_gravity = m_cvar()->FindVar("sv_gravity");
	if (LagRecord->flags & FL_ONGROUND
		&& LagRecord->layers[ANIMATION_LAYER_ALIVELOOP].m_flWeight > 0.0f
		&& LagRecord->layers[ANIMATION_LAYER_ALIVELOOP].m_flWeight < 1.0f)
	{
		if (float_t flValue = (1.0f - LagRecord->layers[ANIMATION_LAYER_ALIVELOOP].m_flWeight) * 0.35f; flValue > 0.0f && flValue < 1.0f)
			LagRecord->m_flAnimationVelocity = flValue + 0.55f;
		else
			LagRecord->m_flAnimationVelocity = -1.f;
	}

	if (PreviousRecord)
	{
		//	calculate new velocity based on (new_origin - old_origin) / (new_time - old_time) formula.
		if (LagRecord->m_nSimulationTicks > 1 && LagRecord->m_nSimulationTicks <= 20)
			vecVelocity = (LagRecord->origin - PreviousRecord->origin) * (1.0f / TICKS_TO_TIME(LagRecord->m_nSimulationTicks));

		if (abs(vecVelocity.x) < 0.001f)
			vecVelocity.x = 0.0f;
		if (abs(vecVelocity.y) < 0.001f)
			vecVelocity.y = 0.0f;
		if (abs(vecVelocity.z) < 0.001f)
			vecVelocity.z = 0.0f;

		if (_fdtest(&vecVelocity.x) > 0
			|| _fdtest(&vecVelocity.y) > 0
			|| _fdtest(&vecVelocity.z) > 0)
			vecVelocity.Zero();

		float_t flCurrentDirection = RAD2DEG(atan2f(vecVelocity.y, vecVelocity.x));
		float_t flPreviousDirection = RAD2DEG(atan2f(PreviousRecord->velocity.y, PreviousRecord->velocity.x));

		if (LagRecord->flags & FL_ONGROUND
			&& vecVelocity.Length2D() >= 0.1f
			&& abs(math::normalize_yaw(flCurrentDirection - flPreviousDirection)) < 1.0f
			&& abs(LagRecord->duck_amount - PreviousRecord->duck_amount) <= 0.0f
			&& LagRecord->layers[ANIMATION_LAYER_MOVEMENT_MOVE].m_flPlaybackRate > PreviousRecord->layers[ANIMATION_LAYER_MOVEMENT_MOVE].m_flPlaybackRate
			&& flWeightSpeed > PreviousRecord->layers[ANIMATION_LAYER_MOVEMENT_MOVE].m_flWeight)
		{
			if (flWeightSpeed <= 0.7f && flWeightSpeed > 0.0f)
			{
				if (LagRecord->layers[ANIMATION_LAYER_MOVEMENT_MOVE].m_flPlaybackRate == 0.0f)
					vecVelocity.Zero();
				else
				{
					if (vecVelocity.Length2D() != 0.0f)
					{
						float_t flMaxSpeedMultiply = 1;
						if (LagRecord->flags & 6)
							flMaxSpeedMultiply = 0.34f;
						else if (pPlayer->m_bIsWalking())
							flMaxSpeedMultiply = 0.52f;

						vecVelocity.x = (vecVelocity.x / vecVelocity.Length2D()) * (flWeightSpeed * (pPlayer->GetMaxPlayerSpeed() * flMaxSpeedMultiply));
						vecVelocity.y = (vecVelocity.y / vecVelocity.Length2D()) * (flWeightSpeed * (pPlayer->GetMaxPlayerSpeed() * flMaxSpeedMultiply));
					}
				}
			}
		}

		if (LagRecord->flags & FL_ONGROUND && vecVelocity.Length2D() > 0.1f && LagRecord->m_nSimulationTicks > 1)
		{
			if (LagRecord->m_flAnimationVelocity > 0)
			{
				if (pPlayer->m_hActiveWeapon().Get())
				{
					vecVelocity.x *= (LagRecord->m_flAnimationVelocity * pPlayer->GetMaxPlayerSpeed()) / vecVelocity.Length2D();
					vecVelocity.y *= (LagRecord->m_flAnimationVelocity * pPlayer->GetMaxPlayerSpeed()) / vecVelocity.Length2D();
				}
			}
		}

		if (!(LagRecord->flags & FL_ONGROUND))
		{
			vecVelocity = (LagRecord->origin - PreviousRecord->origin) * (1.0f / TICKS_TO_TIME(LagRecord->m_nSimulationTicks));

			float_t flWeight = 1.0f - LagRecord->layers[ANIMATION_LAYER_ALIVELOOP].m_flWeight;
			if (flWeight > 0.0f)
			{
				float_t flPreviousRate = PreviousRecord->layers[ANIMATION_LAYER_ALIVELOOP].m_flPlaybackRate;
				float_t flCurrentRate = LagRecord->layers[ANIMATION_LAYER_ALIVELOOP].m_flPlaybackRate;

				if (flPreviousRate == flCurrentRate)
				{
					int32_t iPreviousSequence = PreviousRecord->layers[ANIMATION_LAYER_ALIVELOOP].m_nSequence;
					int32_t iCurrentSequence = LagRecord->layers[ANIMATION_LAYER_ALIVELOOP].m_nSequence;

					if (iPreviousSequence == iCurrentSequence)
					{
						float_t flSpeedNormalized = (flWeight / 2.8571432f) + 0.55f;
						if (flSpeedNormalized > 0.0f)
						{
							float_t flSpeed = flSpeedNormalized * pPlayer->GetMaxPlayerSpeed();
							if (flSpeed > 0.0f)
								if (vecVelocity.Length2D() > 0.0f)
									vecVelocity = (vecVelocity / vecVelocity.Length()) * flSpeed;
						}
					}
				}

				vecVelocity.z -= sv_gravity->GetFloat() * 0.5f * TICKS_TO_TIME(LagRecord->m_nSimulationTicks);
			}
			else
				vecVelocity.z = 0.0f;
		}
	}
	else
	{
		float_t flLastUpdateTime = LagRecord->simulation_time - m_globals()->m_intervalpertick;
		if (pPlayer->m_fFlags() & FL_ONGROUND)
		{
			animstate->m_bLanding = false;
			animstate->m_bOnGround = true;

			float_t flLandTime = 0.0f;
			if (LagRecord->layers[ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB].m_flCycle > 0.0f &&
				LagRecord->layers[ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB].m_flPlaybackRate > 0.0f)
			{
				int32_t iLandActivity = pPlayer->sequence_activity(LagRecord->layers[ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB].m_nSequence);
				if (iLandActivity == ACT_CSGO_LAND_LIGHT || iLandActivity == ACT_CSGO_LAND_HEAVY)
				{
					flLandTime = LagRecord->layers[ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB].m_flCycle / LagRecord->layers[ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB].m_flPlaybackRate;
					if (flLandTime > 0.0f)
						flLastUpdateTime = LagRecord->simulation_time - flLandTime;
				}
			}

			LagRecord->velocity.z = 0.0f;
		}
		else
		{
			float_t flJumpTime = 0.0f;
			if (LagRecord->layers[ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL].m_flCycle > 0.0f &&
				LagRecord->layers[ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL].m_flPlaybackRate > 0.0f)
			{
				int32_t iJumpActivity = pPlayer->sequence_activity(LagRecord->layers[ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL].m_nSequence);
				if (iJumpActivity == ACT_CSGO_JUMP)
				{
					flJumpTime = LagRecord->layers[ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL].m_flCycle / LagRecord->layers[ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL].m_flPlaybackRate;
					if (flJumpTime > 0.0f)
						flLastUpdateTime = LagRecord->simulation_time - flJumpTime;
				}
			}

			animstate->m_bOnGround = false;
			animstate->m_flDurationInAir = flJumpTime - m_globals()->m_intervalpertick;
		}

		if (LagRecord->layers[ANIMATION_LAYER_MOVEMENT_MOVE].m_flPlaybackRate < 0.00001f)
			LagRecord->velocity.Zero();
		else
		{
			if (flWeightSpeed > 0.0f && flWeightSpeed < 0.95f)
			{
				float_t flMaxSpeed = pPlayer->GetMaxPlayerSpeed();
				if (vecVelocity.Length() > 0.0f)
				{
					float_t flMaxSpeedMultiply = 1.0f;
					if (pPlayer->m_fFlags() & 6)
						flMaxSpeedMultiply = 0.34f;
					else if (pPlayer->m_bIsWalking())
						flMaxSpeedMultiply = 0.52f;

					LagRecord->velocity = (LagRecord->velocity / pPlayer->m_vecVelocity().Length()) * (flWeightSpeed * (flMaxSpeed * flMaxSpeedMultiply));
				}
			}
		}

		animstate->m_flLastUpdateTime = flLastUpdateTime;
	}

	pPlayer->m_vecVelocity() = vecVelocity;
	pPlayer->m_vecAbsVelocity() = pPlayer->m_vecVelocity();

	if (pPlayer->m_fFlags() & FL_ONGROUND && pPlayer->m_vecVelocity().Length() > 0.0f && flWeightSpeed <= 0.0f)
		pPlayer->m_vecVelocity().Zero();
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

template < class T >
static T interpolate(const T& current, const T& target, const int progress, const int maximum)
{
	return current + (((target - current) / maximum) * progress);
}

void lagcompensation::setup_matrix(player_t* e, const int& matrix , adjust_data* record)
{
	switch (matrix)
	{
	case MiddleMatrix:
		e->setup_bones_latest(record->m_Matricies[MiddleMatrix].data(), false);
		break;
	case LeftMatrix:
		e->setup_bones_latest(record->m_Matricies[LeftMatrix].data(), true);
		break;
	case RightMatrix:
		e->setup_bones_latest(record->m_Matricies[RightMatrix].data(), true);
		break;
	case LowLeftMatrix:
		e->setup_bones_latest(record->m_Matricies[LowLeftMatrix].data(), true);
		break;
	case LowRightMatrix:
		e->setup_bones_latest(record->m_Matricies[LowRightMatrix].data(), true);
		break;
	case ZeroMatrix:
		e->setup_bones_latest(record->m_Matricies[ZeroMatrix].data(), true);
		break;
	}
}
void lagcompensation::HandleDormancyLeaving(player_t* pPlayer, adjust_data* m_Record, C_CSGOPlayerAnimationState* m_AnimationState)
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
				m_AnimationState->m_bLanding = true;
				m_AnimationState->m_flDuckAdditional = 0.0f;
			}
			else if (flLandingTime - m_globals()->m_intervalpertick == m_flLastUpdateTime)
			{
				m_AnimationState->m_bOnGround = false;
				m_AnimationState->m_bLanding = false;
				m_AnimationState->m_flDuckAdditional = 0.0f;
			}

			/* Determine duration in air */
			float flDurationInAir = (m_JumpingLayer->m_flCycle - m_LandingLayer->m_flCycle);
			if (flDurationInAir < 0.0f)
				flDurationInAir += 1.0f;

			/* Set time in air */
			m_AnimationState->m_flDurationInAir = flDurationInAir / m_JumpingLayer->m_flPlaybackRate;

			/* Check bounds.*/
			/* There's two conditions to let this data be useful: */
			/* It's useful if player has landed after the latest client animation update */
			/* It's useful if player has landed before the previous tick */
			if (flLandingTime < m_flLastUpdateTime && flLandingTime > m_AnimationState->m_flLastUpdateTime)
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
			if (flJumpingTime < m_flLastUpdateTime && flJumpingTime > m_AnimationState->m_flLastUpdateTime)
				m_flLastUpdateTime = flJumpingTime;

			/* Set time in air */
			m_AnimationState->m_flDurationInAir = flDurationInAir - m_globals()->m_intervalpertick;

			/* Disable landing */
			m_AnimationState->m_bLanding = false;
		}
	}

	/* Set m_flLastUpdateTime */
	m_AnimationState->m_flLastUpdateTime = m_flLastUpdateTime;
}

void lagcompensation::SetupCollision(player_t* pPlayer, adjust_data* m_LagRecord)
{

	ICollideable* m_Collideable = pPlayer->GetCollideable();
	if (!m_Collideable)
		return;

	m_LagRecord->mins = m_Collideable->OBBMins();
	m_LagRecord->maxs = m_Collideable->OBBMaxs();
}

float lagcompensation::BuildFootYaw(player_t* pPlayer, adjust_data* m_LagRecord)
{
	return player_resolver->b_yaw(pPlayer , pPlayer->m_angEyeAngles().y , 5);
}
void lagcompensation::UpdatePlayerAnimations(player_t* pPlayer, adjust_data* m_LagRecord, C_CSGOPlayerAnimationState* m_AnimationState)
{

	/* Bypass this check https://github.com/perilouswithadollarsign/cstrike15_src/blob/f82112a2388b841d72cb62ca48ab1846dfcc11c8/game/shared/cstrike15/csgo_playeranimstate.cpp#L266 */
	m_AnimationState->m_nLastUpdateFrame = 0;
	if (m_AnimationState->m_flLastUpdateTime == m_globals()->m_curtime)
		m_AnimationState->m_flLastUpdateTime = m_globals()->m_curtime + m_globals()->m_intervalpertick;

	/* Force animation state player and previous weapon */
	m_AnimationState->m_pBasePlayer = pPlayer;
	m_AnimationState->m_pWeaponLast = pPlayer->m_hActiveWeapon();

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



void lagcompensation::SetupData(player_t* pPlayer, adjust_data* m_Record , adjust_data* m_PrevRecord)
{
	/* Determine simulation ticks with anim cycle */
	if (!m_PrevRecord || m_Record->bot)
	{
		m_Record->m_nSimulationTicks = 1;
		return;
	}
	m_Record->m_nSimulationTicks = DetermineAnimationCycle(pPlayer, m_Record, m_PrevRecord);

	if (TIME_TO_TICKS(m_Record->simulation_time - m_PrevRecord->simulation_time) > 17)
		m_PrevRecord = nullptr;


	/*if (!m_PrevRecord)
	{
		m_Record->m_nSimulationTicks = 1;
		return;
	}
	auto ticks_chocked = 1;
	auto simulation_ticks = TIME_TO_TICKS(pPlayer->m_flSimulationTime() - m_PrevRecord->simulation_time);

	if (simulation_ticks > 0 && simulation_ticks < 17)
		ticks_chocked = simulation_ticks;

	m_Record->m_nSimulationTicks = ticks_chocked;

	*/
}
int lagcompensation::DetermineAnimationCycle(player_t* pPlayer, adjust_data* m_LagRecord, adjust_data* m_PrevRecord)
{
	/* Get animation layers */
	const AnimationLayer m_AliveLoop = m_LagRecord->layers[ANIMATION_LAYER_ALIVELOOP];
	const AnimationLayer m_PrevAliveLoop = m_PrevRecord->layers[ANIMATION_LAYER_ALIVELOOP];

	/* Get ticks animated on the server ( by default it's simtime delta ) */
	int nTicksAnimated = m_LagRecord->m_nSimulationTicks;
	if (m_AliveLoop.m_flPlaybackRate == m_PrevAliveLoop.m_flPlaybackRate)
		nTicksAnimated = (m_AliveLoop.m_flCycle - m_PrevAliveLoop.m_flCycle) / (m_AliveLoop.m_flPlaybackRate * m_globals()->m_intervalpertick);
	else
		nTicksAnimated = ((((m_AliveLoop.m_flCycle / m_AliveLoop.m_flPlaybackRate) + ((1.0f - m_PrevAliveLoop.m_flCycle) / m_PrevAliveLoop.m_flPlaybackRate)) / m_globals()->m_intervalpertick));

	return min(max(nTicksAnimated, m_LagRecord->m_nSimulationTicks), 17);
}
void lagcompensation::SimulatePlayerActivity(player_t* pPlayer, adjust_data* m_LagRecord, adjust_data* m_PrevRecord)
{
	/* Get animation layers */
	const AnimationLayer* m_JumpingLayer = &m_LagRecord->layers[ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL];
	const AnimationLayer* m_LandingLayer = &m_LagRecord->layers[ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB];
	const AnimationLayer* m_PrevJumpingLayer = &m_PrevRecord->layers[ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL];
	const AnimationLayer* m_PrevLandingLayer = &m_PrevRecord->layers[ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB];

	/* Detect jump/land, collect its data, rebuild time in air */
	const int nJumpingActivity = pPlayer->sequence_activity(m_JumpingLayer->m_nSequence);
	const int nLandingActivity = pPlayer->sequence_activity(m_LandingLayer->m_nSequence);

	/* Collect jump data */
	if (nJumpingActivity == ACT_CSGO_JUMP)
	{
		/* check duration bounds */
		if (m_JumpingLayer->m_flWeight > 0.0f && m_JumpingLayer->m_flPlaybackRate > 0.0f)
		{
			/* check cycle changed */
			if (m_JumpingLayer->m_flCycle < m_PrevJumpingLayer->m_flCycle)
			{
				m_LagRecord->m_flDurationInAir = m_JumpingLayer->m_flCycle / m_JumpingLayer->m_flPlaybackRate;
				if (m_LagRecord->m_flDurationInAir > 0.0f)
				{
					m_LagRecord->m_nActivityTick = TIME_TO_TICKS(m_LagRecord->simulation_time - m_LagRecord->m_flDurationInAir) + 1;
					m_LagRecord->m_nActivityType = EPlayerActivityC::CJump;
				}
			}
		}
	}

	/* Collect land data */
	if (nLandingActivity == ACT_CSGO_LAND_LIGHT || nLandingActivity == ACT_CSGO_LAND_HEAVY)
	{
		/* weight changing everytime on activity switch in this layer */
		if (m_LandingLayer->m_flWeight > 0.0f && m_PrevLandingLayer->m_flWeight <= 0.0f)
		{
			/* check cycle changed */
			if (m_LandingLayer->m_flCycle > m_PrevLandingLayer->m_flCycle)
			{
				float flLandDuration = m_LandingLayer->m_flCycle / m_LandingLayer->m_flPlaybackRate;
				if (flLandDuration > 0.0f)
				{
					m_LagRecord->m_nActivityTick = TIME_TO_TICKS(m_LagRecord->simulation_time - flLandDuration) + 1;
					m_LagRecord->m_nActivityType = EPlayerActivityC::CLand;

					/* Determine duration in air */
					float flDurationInAir = (m_JumpingLayer->m_flCycle - m_LandingLayer->m_flCycle);
					if (flDurationInAir < 0.0f)
						flDurationInAir += 1.0f;

					/* Set time in air */
					m_LagRecord->m_flDurationInAir = flDurationInAir / m_JumpingLayer->m_flPlaybackRate;
				}
			}
		}
	}
}
float lagcompensation::ComputeActivityPlayback(player_t* pPlayer, adjust_data* m_Record)
{
	/* Get animation layers */
	AnimationLayer* m_JumpingLayer = &m_Record->layers[ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL];
	AnimationLayer* m_LandingLayer = &m_Record->layers[ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB];

	/* Determine playback */
	float flActivityPlayback = 0.0f;
	switch (m_Record->m_nActivityType)
	{
	case EPlayerActivity::Jump:
	{
		flActivityPlayback = pPlayer->GetLayerSequenceCycleRate(m_JumpingLayer, m_JumpingLayer->m_nSequence);
	}
	break;

	case EPlayerActivity::Land:
	{
		flActivityPlayback = pPlayer->GetLayerSequenceCycleRate(m_LandingLayer, m_LandingLayer->m_nSequence);
	}
	break;
	}

	return flActivityPlayback;
}
int lagcompensation::DetermineSimulationTicks(player_t* player, adjust_data* record, adjust_data* previous_record)
{

	if (!previous_record || !record || !player) {
		return 1;
	}

	auto sim_ticks = TIME_TO_TICKS(record->simulation_time - previous_record->simulation_time);

	if (sim_ticks - 1 > 31 || previous_record->simulation_time == 0.f) {
		sim_ticks = 1;
	}
	auto cur_cycle = record->layers[11].m_flCycle;
	auto prev_rate = previous_record->layers[11].m_flPlaybackRate;

	if (prev_rate > 0.f && record->layers[11].m_flPlaybackRate > 0.f) {
		auto prev_cycle = previous_record->layers[11].m_flCycle;
		sim_ticks = 0;

		if (prev_cycle > cur_cycle)
			cur_cycle += 1.f;

		while (cur_cycle > prev_cycle) {
			const auto last_cmds = sim_ticks;

			const auto next_rate = m_globals()->m_intervalpertick * prev_rate;
			prev_cycle += m_globals()->m_intervalpertick * prev_rate;

			if (prev_cycle >= 1.f)
				prev_rate = record->layers[11].m_flPlaybackRate;

			++sim_ticks;

			if (prev_cycle > cur_cycle && (prev_cycle - cur_cycle) > (next_rate * 0.5f))
				sim_ticks = last_cmds;
		}
	}
	return std::clamp(sim_ticks, 0, 17);
}

void lagcompensation::SetBones(matrix3x4_t* matrix, player_t* m_pEntity)
{
	m_CachedMatrixRetr[m_pEntity->EntIndex()] = m_pEntity->setup_bones_latest(matrix , false);
	m_pEntity->setup_bones_latest(matrix, false);
	for (int i = NULL; i < MAXSTUDIOBONES; i++)
		m_BoneOrigins[m_pEntity->EntIndex()][i] = m_pEntity->GetAbsOrigin() - matrix[i].GetOrigin();

	std::memcpy(m_CachedMatrix[m_pEntity->EntIndex()], matrix, m_pEntity->m_CachedBoneData().Count() * sizeof(matrix3x4_t));
}

void lagcompensation::CopyCachedMatrix(player_t* player, matrix3x4_t* matrix)
{
	if (m_CachedMatrixRetr[player->EntIndex()])
		std::memcpy(matrix, m_CachedMatrix[player->EntIndex()], player->m_CachedBoneData().Count() * sizeof(matrix3x4_t));
}

void lagcompensation::OnUpdateClientSideAnimations(player_t* player)
{
	for (int i = NULL; i < MAXSTUDIOBONES; i++)
		m_CachedMatrix[player->EntIndex()][i].SetOrigin(player->GetAbsOrigin() - m_BoneOrigins[player->EntIndex()][i]);

	std::memcpy(player->m_CachedBoneData().Base(),m_CachedMatrix[player->EntIndex()], player->m_CachedBoneData().Count() * sizeof(matrix3x4_t));
	std::memcpy(player->m_BoneAccessor().GetBoneArrayForWrite(), m_CachedMatrix[player->EntIndex()], player->m_CachedBoneData().Count() * sizeof(matrix3x4_t));

	return player->attachment_helper();
}

void lagcompensation::SimulatePlayerAnimations(player_t* e, adjust_data* record, adjust_data* previous_record)
{
	auto animstate = e->get_animation_state1();

	if (!animstate || !record)
		return;

	if (previous_record) {
		animstate->m_flPrimaryCycle = previous_record->layers[6].m_flCycle;
		animstate->m_flMoveWeight = previous_record->layers[6].m_flWeight;

		const auto& layer7 = previous_record->layers[7];

		animstate->m_flStrafeChangeWeight = layer7.m_flWeight;
		animstate->m_nStrafeSequence = layer7.m_nSequence;
		animstate->m_flStrafeChangeCycle = layer7.m_flCycle;
		animstate->m_flAccelerationWeight = previous_record->layers[12].m_flWeight;

		memcpy(e->get_animlayers(), previous_record->layers, e->animlayer_count() * sizeof(AnimationLayer));
	}
	else {
		if (record->flags & FL_ONGROUND) {
			animstate->m_bOnGround = true;
			animstate->m_bLanding = false;
		}

		animstate->m_flDurationInAir = 0.f;
		animstate->m_flPrimaryCycle = record->layers[6].m_flCycle;
		animstate->m_flMoveWeight = record->layers[6].m_flWeight;
		animstate->m_flStrafeChangeWeight = record->layers[7].m_flWeight;
		animstate->m_nStrafeSequence = record->layers[7].m_nSequence;
		animstate->m_flStrafeChangeCycle = record->layers[7].m_flCycle;
		animstate->m_flAccelerationWeight = record->layers[12].m_flWeight;

		e->m_flPoseParameter()[6] = 0.f;
		memcpy(e->get_animlayers(), record->layers, e->animlayer_count() * sizeof(AnimationLayer));

		auto walk_run_transition = record->velocity.Length2D() <= 135.f ? 0.f : 1.f;

		animstate->m_flWalkToRunTransition = walk_run_transition;
		animstate->m_flLastUpdateTime = record->simulation_time - m_globals()->m_intervalpertick;
	}


	player_info_t player_info;

	if (!m_engine()->GetPlayerInfo(e->EntIndex(), &player_info))
		return;

	GameGlobals_t m_Globals;
	PlayersGlobals_t m_PlayerGlobals;

	/*Handle shots*/
	weapon_t* pWeapon = e->m_hActiveWeapon();
	if (pWeapon)
	{
		if (pWeapon->m_fLastShotTime() <= e->m_flSimulationTime() && pWeapon->m_fLastShotTime() > e->m_flOldSimulationTime())
		{
			record->shot = true;
			record->shot_tick = TIME_TO_TICKS(pWeapon->m_fLastShotTime());
		}
	}

	m_Globals.CaptureData();
	m_PlayerGlobals.CaptureData(e);

	//SetupData(e, record, previous_record);
	record->m_nSimulationTicks = DetermineSimulationTicks(e, record, previous_record);

	/* Determine player's velocity */
    //record->velocity = DeterminePlayerVelocity(e, record, previous_record, animstate);
	RecalculateVelocity(e, record, previous_record, animstate);
	e->invalidate_physics_recursive(VELOCITY_CHANGED);

	/*Handle dormancy*/
	if (is_dormant[e->EntIndex()])
	{
		is_dormant[e->EntIndex()] = false;
		HandleDormancyLeaving(e, record, animstate);
	}

	/* Invalidate EFlags */
	e->m_iEFlags() &= ~(EFL_DIRTY_ABSVELOCITY | EFL_DIRTY_ABSTRANSFORM);

	if (e->m_fFlags() & FL_ONGROUND && e->m_vecVelocity().Length() > 0.0f && record->layers[6].m_flWeight <= 0.0f)
		record->velocity.Zero();

	/* Update collision bounds */
	SetupCollision(e, record);

	C_CSGOPlayerAnimationState state;
	memcpy(&state, animstate, sizeof(C_CSGOPlayerAnimationState));

	/* Simulate legit player */
	if (record->m_nSimulationTicks <= 1 || !previous_record)
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

		/* C_BaseEntity::SetAbsOrigin */
		e->set_abs_origin(record->origin);

		/* Set velociy */
		e->m_vecVelocity() = record->velocity;
		e->m_vecAbsVelocity() = record->velocity;

		/* Update animations */
		UpdatePlayerAnimations(e, record, animstate);
	}
	else if (previous_record && record->m_nSimulationTicks > 1)
	{
		/* Simulate player activity ( jump and land ) */
		SimulatePlayerActivity(e, record, previous_record);

		/* Compute activity playback ( jump and land ) */
		record->m_flActivityPlayback = ComputeActivityPlayback(e, record);

		for (int iSimulationTick = 1; iSimulationTick <= record->m_nSimulationTicks; iSimulationTick++)
		{
			/* Determine simulation time and tick */
			float flSimulationTime = previous_record->simulation_time + TICKS_TO_TIME(iSimulationTick);
			int iCurrentSimulationTick = TIME_TO_TICKS(flSimulationTime);

			/* Setup game's global data */
			m_globals()->m_curtime = flSimulationTime;
			m_globals()->m_realtime = flSimulationTime;
			m_globals()->m_frametime = m_globals()->m_intervalpertick;
			m_globals()->m_absoluteframetime = m_globals()->m_intervalpertick;
			m_globals()->m_framecount = iCurrentSimulationTick;
			m_globals()->m_tickcount = iCurrentSimulationTick;
			m_globals()->m_interpolation_amount = 0.0f;

			/* Set player data */

			// fix retard abobus animation
			if (auto duck_amount_per_tick = (e->m_flDuckAmount() - previous_record->duck_amount) / record->m_nSimulationTicks)
				e->m_flDuckAmount() = previous_record->duck_amount + duck_amount_per_tick * (float)iSimulationTick;

			//e->m_flDuckAmount() = interpolate(previous_record->duck_amount, record->duck_amount, iSimulationTick, record->m_nSimulationTicks);
			e->m_flLowerBodyYawTarget() = previous_record->lby;
			e->m_angEyeAngles() = previous_record->angles;

			/* Simulate origin */
			e->m_vecOrigin() = interpolate(previous_record->origin, record->origin, iSimulationTick, record->m_nSimulationTicks);
			e->set_abs_origin(e->m_vecOrigin());

			/* Activity simulation */
			if (flSimulationTime < record->simulation_time)
			{
				/* Simulate shoot */
				if (record->shot)
				{
					if (iCurrentSimulationTick < record->shot_tick)
						e->m_flThirdpersonRecoil() = previous_record->m_flThirdPersonRecoil;
					else
					{
						auto prev_lby = previous_record->lby;
						auto cur_lby = record->lby;

						if (prev_lby != cur_lby)
						{
							auto v40 = record->m_nSimulationTicks - iSimulationTick;
							bool use_new_lby = true;

							if (record->lby_diff < 1.f)
								use_new_lby = v40 == 0;
							else
								use_new_lby = v40 < 2;

							if (!use_new_lby)
								cur_lby = prev_lby;

							e->m_flLowerBodyYawTarget() = cur_lby;
						}

						e->m_angEyeAngles() = record->angles;
						e->m_flThirdpersonRecoil() = record->m_flThirdPersonRecoil;
					}
				}

				if (record->m_nActivityType != EPlayerActivityC::CNoActivity)
				{
					if (iCurrentSimulationTick == record->m_nActivityTick)
					{
						/* Compute current layer */
						int nLayer = ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL;
						if (record->m_nActivityType == EPlayerActivityC::CLand)
							nLayer = ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB;

						/* C_CSGOPlayerAnimationState::SetLayerSequence */
						e->get_animlayers()[nLayer].m_flCycle = 0.0f;
						e->get_animlayers()[nLayer].m_flWeight = 0.0f;
						e->get_animlayers()[nLayer].m_flPlaybackRate = record->m_flActivityPlayback;

						/* Force player's ground state */
						if (record->m_nActivityType == EPlayerActivityC::CJump)
							e->m_fFlags() &= ~FL_ONGROUND;
						else if (record->m_nActivityType == EPlayerActivityC::CLand)
							e->m_fFlags() |= FL_ONGROUND;
					}
					else if (iCurrentSimulationTick < record->m_nActivityTick)
					{
						/* Force player's ground state */
						if (record->m_nActivityType == EPlayerActivityC::CJump)
							e->m_fFlags() |= FL_ONGROUND;
						else if (record->m_nActivityType == EPlayerActivityC::CLand)
							e->m_fFlags() &= ~FL_ONGROUND;
					}
				}
			}
			else /* Set the latest networked data for the latest simulation tick */
			{
				e->m_fFlags() = record->flags;
				e->m_flDuckAmount() = record->duck_amount;
				e->m_flLowerBodyYawTarget() = record->lby;
				e->m_angEyeAngles() = record->angles;
			}

			/* Set velocity */
			Vector& vecVelocity = e->m_vecVelocity();
			vecVelocity.x = interpolate(previous_record->velocity.x, record->velocity.x, iSimulationTick, record->m_nSimulationTicks);
			vecVelocity.y = interpolate(previous_record->velocity.y, record->velocity.y, iSimulationTick, record->m_nSimulationTicks);
			e->m_vecAbsVelocity() = vecVelocity;
			e->m_vecVelocity() = vecVelocity;

			/* Update animations */
			UpdatePlayerAnimations(e, record, animstate);
		}
	}

	/* Reset animation layers */
	std::memcpy(e->get_animlayers(), record->layers, sizeof(AnimationLayer) * 13);

	/* Reset player's origin */
	e->set_abs_origin(m_PlayerGlobals.m_vecAbsOrigin);

	previous_goal_feet_yaw[e->EntIndex()] = animstate->m_flFootYaw;
	if (/*!record->bot && g_ctx.local()->is_alive() &&*/ e->m_iTeamNum() != g_ctx.local()->m_iTeamNum() && !g_cfg.legitbot.enabled)
	{
		player_resolver[e->EntIndex()].initialize_yaw(e, record, previous_record);

		// Define the delta angles array
		const float deltaAngles[] = { 0.0f, -58.0f, 58.0f, -29.0f, 29.0f, 0.0f };

		// Loop through each delta angle
		for (int i = 1; i <= 5; i++) {
			float deltaAngle = deltaAngles[i];

			// Update animstate for the current delta angle
			animstate->m_flFootYaw = math::normalize_yaw(e->m_angEyeAngles().y + deltaAngle);
			UpdatePlayerAnimations(e, record, animstate);

			//setup bones 
			e->setup_bones_latest(record->m_Matricies[i].data(), true);

			// copy layer data to use it in in the resolver
			memcpy(player_resolver[e->EntIndex()].resolver_layers[i], e->get_animlayers(), e->animlayer_count() * sizeof(AnimationLayer));

			// restore data
			memcpy(e->get_animlayers(), record->layers, e->animlayer_count() * sizeof(AnimationLayer));
			memcpy(animstate, &state, sizeof(C_CSGOPlayerAnimationState));
		}

		player_resolver[e->EntIndex()].initialize(e, record, previous_goal_feet_yaw[e->EntIndex()], e->m_angEyeAngles().x, previous_record);
		player_resolver[e->EntIndex()].resolve_desync();
	}

	UpdatePlayerAnimations(e, record, animstate);

	//e->setup_bones_latest(record->m_Matricies[MiddleMatrix].data(), false);
	SetBones(record->m_Matricies[MiddleMatrix].data(), e);
	//memcpy(e->m_CachedBoneData().Base(), record->m_Matricies[MiddleMatrix].data(), e->m_CachedBoneData().Count() * sizeof(matrix3x4_t));

	if (previous_record)
		memcpy(player_resolver[e->EntIndex()].previous_layers, previous_record->layers, e->animlayer_count() * sizeof(AnimationLayer));

	e->m_flLowerBodyYawTarget() = m_PlayerGlobals.m_flLowerBodyYawTarget;
	e->m_flDuckAmount() = m_PlayerGlobals.m_flDuckAmount;
	e->m_iEFlags() = m_PlayerGlobals.m_iEFlags;
	e->m_fFlags() = m_PlayerGlobals.m_fFlags;

	//m_PlayerGlobals.AdjustData(e);
	m_Globals.AdjustData();

	return e->invalidate_physics_recursive(ANIMATION_CHANGED | ANGLES_CHANGED);
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