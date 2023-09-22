#include "LagComp.hpp"
#include "../../ragebot/aim.h"
#include "../LocalAnimFix.hpp"

void C_PlayerAnimations::SimulatePlayerAnimations(player_t* e, LagRecord_t* record, PlayerData_t* player_data)
{
	AnimState_s* animstate = e->GetAnimState();

	if (!animstate)
		return;

	/* Setup data and get previous record */
	LagRecord_t* m_PrevRecord = g_LagComp->SetupData(e, record, player_data);

	/* Copy data from previous or current record depends on situation */
	CopyRecordData(e, record, m_PrevRecord, animstate);

	/* Determine player's velocity */
	record->m_vecVelocity = DeterminePlayerVelocity(e, record, m_PrevRecord, animstate);

	if (record->m_bFirstAfterDormant)
		HandleDormancyLeaving(e, record, animstate);


	CGameGlobals_t m_Globals;
	CPlayersGlobals_t m_PlayerGlobals;

	m_Globals.CaptureData();
	m_PlayerGlobals.CaptureData(e);


	/* Invalidate EFlags */
	e->m_iEFlags() &= ~(EFL_DIRTY_ABSVELOCITY | EFL_DIRTY_ABSTRANSFORM);

	/* Update collision bounds */
	SetupCollision(e, record);

	/* Simulate legit player */
	if (record->m_nSimulationTicks <= 1 || !m_PrevRecord)
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
		e->set_abs_origin(record->m_vecOrigin);

		/* Set velociy */
		e->m_vecVelocity() = record->m_vecVelocity;
		e->m_vecAbsVelocity() = record->m_vecVelocity;

		/* Update animations */
		UpdatePlayerAnimations(e, record, animstate);
	}
	else
	{
		/* Simulate player activity ( jump and land ) */
		SimulatePlayerActivity(e, record, m_PrevRecord);

		/* Compute activity playback ( jump and land ) */
		record->m_flActivityPlayback = ComputeActivityPlayback(e, record);

		for (int iSimulationTick = 1; iSimulationTick <= record->m_nSimulationTicks; iSimulationTick++)
		{
			//auto simulated_time = m_PrevRecord->simulation_time + TICKS_TO_TIME(i);

			/* Determine simulation time and tick */
			float flSimulationTime = m_PrevRecord->m_flSimulationTime + TICKS_TO_TIME(iSimulationTick);
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
			e->m_flDuckAmount() = interpolate(m_PrevRecord->m_flDuckAmount, record->m_flDuckAmount, iSimulationTick, record->m_nSimulationTicks);
			e->m_flLowerBodyYawTarget() = m_PrevRecord->m_flLowerBodyYaw;
			e->m_angEyeAngles() = m_PrevRecord->m_angEyeAngles;

			/* Simulate origin */
			e->m_vecOrigin() = interpolate(m_PrevRecord->m_vecOrigin, record->m_vecOrigin, iSimulationTick, record->m_nSimulationTicks);
			e->set_abs_origin(e->m_vecOrigin());

			/* Activity simulation */
			if (flSimulationTime < record->m_flSimulationTime)
			{

				/* Simulate shoot */
				if (record->m_bDidShot)
				{
					if (iCurrentSimulationTick < record->m_nShotTick)
						e->m_flThirdpersonRecoil() = m_PrevRecord->m_flThirdPersonRecoil;
					else
					{
						e->m_angEyeAngles() = record->m_angEyeAngles;
						e->m_flLowerBodyYawTarget() = record->m_flLowerBodyYaw;
						e->m_flThirdpersonRecoil() = record->m_flThirdPersonRecoil;
					}
				}

				if (record->m_nActivityType != EPlayerActivity::NoActivity)
				{
					if (iCurrentSimulationTick == record->m_nActivityTick)
					{
						/* Compute current layer */
						int nLayer = ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL;
						if (record->m_nActivityType == EPlayerActivity::Land)
							nLayer = ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB;

						/* C_CSGOPlayerAnimationState::SetLayerSequence */
						e->get_animlayers()[nLayer].m_flCycle = 0.0f;
						e->get_animlayers()[nLayer].m_flWeight = 0.0f;
						e->get_animlayers()[nLayer].m_flPlaybackRate = record->m_flActivityPlayback;

						/* Force player's ground state */
						if (record->m_nActivityType == EPlayerActivity::Jump)
							e->m_fFlags() &= ~FL_ONGROUND;
						else if (record->m_nActivityType == EPlayerActivity::Land)
							e->m_fFlags() |= FL_ONGROUND;
					}
					else if (iCurrentSimulationTick < record->m_nActivityTick)
					{
						/* Force player's ground state */
						if (record->m_nActivityType == EPlayerActivity::Jump)
							e->m_fFlags() |= FL_ONGROUND;
						else if (record->m_nActivityType == EPlayerActivity::Land)
							e->m_fFlags() &= ~FL_ONGROUND;
					}
				}
			}
			else /* Set the latest networked data for the latest simulation tick */
			{
				e->m_fFlags() = record->m_nFlags;
				e->m_flDuckAmount() = record->m_flDuckAmount;
				e->m_flLowerBodyYawTarget() = record->m_flLowerBodyYaw;
				e->m_angEyeAngles() = record->m_angEyeAngles;
			}

			/* Set velocity */
			Vector& vecVelocity = e->m_vecVelocity();
			vecVelocity.x = interpolate(m_PrevRecord->m_vecVelocity.x, record->m_vecVelocity.x, iSimulationTick, record->m_nSimulationTicks);
			vecVelocity.y = interpolate(m_PrevRecord->m_vecVelocity.y, record->m_vecVelocity.y, iSimulationTick, record->m_nSimulationTicks);
			e->m_vecAbsVelocity() = vecVelocity;

			/* Update animations */
			UpdatePlayerAnimations(e, record, animstate);
		}
	}
	/* Reset animation layers */
	std::memcpy(e->get_animlayers(), record->m_Layers.data(), sizeof(AnimationLayer) * 13);

	/* Reset player's origin */
	e->set_abs_origin(m_PlayerGlobals.m_vecAbsOrigin);

	/* Save additional record data */
	record->m_angAbsAngles = Vector(0.0f, animstate->m_flFootYaw, 0.0f);
	//{
	//	float flAimMatrixWidthRange = math::lerp(std::clamp(e->GetAnimState()->m_flSpeedAsPortionOfWalkTopSpeed, 0.0f, 1.0f), 1.0f, math::lerp(e->GetAnimState()->m_flWalkToRunTransition, 0.8f, 0.5f)); //-V807
	//	if (e->GetAnimState()->m_flAnimDuckAmount > 0)
	//		flAimMatrixWidthRange = math::lerp(e->GetAnimState()->m_flAnimDuckAmount * std::clamp(e->GetAnimState()->m_flSpeedAsPortionOfCrouchTopSpeed, 0.0f, 1.0f), flAimMatrixWidthRange, 0.5f);

	//	record->m_flDesyncDelta = flAimMatrixWidthRange * e->GetAnimState()->m_flAimYawMax;
	//}

	/* Save record's poses */
	std::memcpy(record->m_PoseParameters.data(), e->m_flPoseParameter().data(), sizeof(float) * record->m_PoseParameters.size());

	if (g_cfg.ragebot.enable_resolver)
	{
		/*simple temporary resolver*/
		AimPlayer* data = &g_Ragebot->m_players[e->EntIndex() - 1];

		record->m_flDesyncDelta = e->get_max_desync_delta();

		if (data->m_missed_shots > 0)
			record->m_flDesyncDelta = -(e->get_max_desync_delta());

		if (data->m_missed_shots > 1)
			data->m_missed_shots = 0;

		e->GetAnimState()->m_flFootYaw = math::normalize_yaw(record->m_flEyeYaw + record->m_flDesyncDelta);
	}

	/* Setup bones on this record */
	{
		/* Setup visual matrix */
		SetupPlayerMatrix(e, record, record->m_Matricies[VisualMatrix].data(), EMatrixFlags::Interpolated | EMatrixFlags::VisualAdjustment);
		std::memcpy(player_data->m_aCachedMatrix.data(), record->m_Matricies[VisualMatrix].data(), sizeof(matrix3x4_t) * MAXSTUDIOBONES);

		/* Setup rage matrix */
		if (g_cfg.ragebot.enable)
			SetupPlayerMatrix(e, record, record->m_Matricies[MiddleMatrix].data(), EMatrixFlags::BoneUsedByHitbox);

		/* Generate safe aimbot points */
		GenerateSafePoints(e, record);
	}

	m_PlayerGlobals.AdjustData(e);
	m_Globals.AdjustData();
}
bool C_PlayerAnimations::CopyCachedMatrix(player_t* pPlayer, matrix3x4_t* aMatrix, int nBoneCount)
{
	PlayerData_t* m_PlayerData = &g_LagComp->GetPlayerData(pPlayer);

	std::memcpy(aMatrix, m_PlayerData->m_aCachedMatrix.data(), sizeof(matrix3x4_t) * nBoneCount);
	return true;
}
void C_PlayerAnimations::InterpolateMatricies()
{
	g_LocalAnimations->InterpolateMatricies();
	for (int nPlayerID = 1; nPlayerID <= 64; nPlayerID++)
	{
		player_t* pPlayer = static_cast<player_t*>(m_entitylist()->GetClientEntity(nPlayerID));
		if (!pPlayer || !pPlayer->is_player() || pPlayer->m_iTeamNum() == g_ctx.local()->m_iTeamNum() || pPlayer == g_ctx.local() || pPlayer->IsDormant() || !pPlayer->is_alive())
			continue;

		PlayerData_t* m_PlayerData = &g_LagComp->GetPlayerData(pPlayer);
		if (!m_PlayerData)
			continue;

		// get bone count
		int nBoneCount = pPlayer->m_CachedBoneData().Count();
		if (nBoneCount > MAXSTUDIOBONES)
			nBoneCount = MAXSTUDIOBONES;

		// re-pos matrix
		g_PlayerAnimations->TransformateMatrix(pPlayer, m_PlayerData);

		// copy the entire matrix
		std::memcpy(pPlayer->m_CachedBoneData().Base(), m_PlayerData->m_aCachedMatrix.data(), sizeof(matrix3x4_t) * nBoneCount);

		// build attachments
		std::memcpy(pPlayer->GetBoneAccessor().GetBoneArrayForWrite(), m_PlayerData->m_aCachedMatrix.data(), sizeof(matrix3x4_t) * nBoneCount);
		pPlayer->SetupBones_AttachmentHelper();
		std::memcpy(pPlayer->GetBoneAccessor().GetBoneArrayForWrite(), m_PlayerData->m_aCachedMatrix.data(), sizeof(matrix3x4_t) * nBoneCount);
	}
}

void C_PlayerAnimations::TransformateMatrix(player_t* pPlayer, PlayerData_t* m_PlayerData)
{
	Vector vecOriginDelta = pPlayer->GetAbsOrigin() - m_PlayerData->m_vecLastOrigin;
	for (auto& Matrix : m_PlayerData->m_aCachedMatrix)
	{
		Matrix[0][3] += vecOriginDelta.x;
		Matrix[1][3] += vecOriginDelta.y;
		Matrix[2][3] += vecOriginDelta.z;
	}
	m_PlayerData->m_vecLastOrigin = pPlayer->GetAbsOrigin();
}
Vector C_PlayerAnimations::DeterminePlayerVelocity(player_t* pPlayer, LagRecord_t* m_LagRecord, LagRecord_t* m_PrevRecord, AnimState_s* m_AnimationState)
{

	auto pCombatWeapon = pPlayer->m_hActiveWeapon();
	auto pCombatWeaponData = pCombatWeapon->get_csweapon_info();

	auto m_flMaxSpeed = pCombatWeapon && pCombatWeaponData ? std::max<float>((pPlayer->m_bIsScoped() ? pCombatWeaponData->flMaxPlayerSpeedAlt : pCombatWeaponData->flMaxPlayerSpeed), 0.001f) : 260.0f;;

	/* Prepare data once */
	if (!m_PrevRecord)
	{
		const float flVelLength = m_LagRecord->m_vecVelocity.Length();
		if (flVelLength > m_flMaxSpeed)
			m_LagRecord->m_vecVelocity *= m_flMaxSpeed / flVelLength;

		return m_LagRecord->m_vecVelocity;
	}

	/* Define const */
	//const float flMaxSpeed = SDK::EngineData::m_ConvarList[CheatConvarList::MaxSpeed]->GetFloat();


	/* Get animation layers */
	const AnimationLayer* m_AliveLoop = &m_LagRecord->m_Layers[ANIMATION_LAYER_ALIVELOOP];
	const AnimationLayer* m_PrevAliveLoop = &m_PrevRecord->m_Layers[ANIMATION_LAYER_ALIVELOOP];

	const AnimationLayer* m_Movement = &m_LagRecord->m_Layers[ANIMATION_LAYER_MOVEMENT_MOVE];
	const AnimationLayer* m_PrevMovement = &m_PrevRecord->m_Layers[ANIMATION_LAYER_MOVEMENT_MOVE];
	const AnimationLayer* m_Landing = &m_LagRecord->m_Layers[ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB];
	const AnimationLayer* m_PrevLanding = &m_PrevRecord->m_Layers[ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB];

	/* Recalculate velocity using origin delta */
	m_LagRecord->m_vecVelocity = (m_LagRecord->m_vecOrigin - m_PrevRecord->m_vecOrigin) * (1.0f / TICKS_TO_TIME(m_LagRecord->m_flSimulationTime));


	/* Check PlaybackRate */
	if (m_Movement->m_flPlaybackRate < 0.00001f)
		m_LagRecord->m_vecVelocity.x = m_LagRecord->m_vecVelocity.y = 0.0f;
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
								m_LagRecord->m_flAnimationVelocity = m_LagRecord->m_vecVelocity.Length2D();
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
						if (m_LagRecord->m_Layers[ANIMATION_LAYER_MOVEMENT_STRAFECHANGE].m_nSequence == m_PrevRecord->m_Layers[ANIMATION_LAYER_MOVEMENT_STRAFECHANGE].m_nSequence)
						{



							/* Check move sequence */
							if (m_Movement->m_nSequence == m_PrevMovement->m_nSequence)
							{
								/* Check land sequence */
								if (m_Landing->m_nSequence == m_PrevLanding->m_nSequence)
								{



									/* Check stand sequence */
									if (m_LagRecord->m_Layers[ANIMATION_LAYER_ADJUST].m_nSequence == m_PrevRecord->m_Layers[ANIMATION_LAYER_ADJUST].m_nSequence)
									{
										/* Check Flags */
										if (m_LagRecord->m_nFlags & FL_ONGROUND)
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
		const float flModifier = m_LagRecord->m_flAnimationVelocity / m_LagRecord->m_vecVelocity.Length2D();
		m_LagRecord->m_vecVelocity.x *= flModifier;



		m_LagRecord->m_vecVelocity.y *= flModifier;
	}

	/* Prepare data once */
	const float flVelLength = m_LagRecord->m_vecVelocity.Length();




	/* Clamp velocity if its out bounds */
	if (flVelLength > m_flMaxSpeed)
		m_LagRecord->m_vecVelocity *= m_flMaxSpeed / flVelLength;

	return m_LagRecord->m_vecVelocity;
}



void C_PlayerAnimations::CopyRecordData(player_t* pPlayer, LagRecord_t* m_LagRecord, LagRecord_t* m_PrevRecord, AnimState_s* m_AnimationState)
{
	LagRecord_t* m_ForceRecord = m_LagRecord;
	if (m_PrevRecord)
		m_ForceRecord = m_PrevRecord;

	AnimationLayer* m_StrafeLayer = &m_ForceRecord->m_Layers[7];
	m_AnimationState->m_flStrafeChangeCycle = m_StrafeLayer->m_flCycle;
	m_AnimationState->m_flStrafeChangeWeight = m_StrafeLayer->m_flWeight;
	m_AnimationState->m_nStrafeSequence = m_StrafeLayer->m_nSequence;
	m_AnimationState->m_flPrimaryCycle = m_ForceRecord->m_Layers[6].m_flCycle;
	m_AnimationState->m_flMoveWeight = m_ForceRecord->m_Layers[6].m_flWeight;
	m_AnimationState->m_flAccelerationWeight = m_ForceRecord->m_Layers[12].m_flWeight;
	std::memcpy(pPlayer->get_animlayers(), m_ForceRecord->m_Layers.data(), sizeof(AnimationLayer) * 13);
}
int C_PlayerAnimations::DetermineAnimationCycle(player_t* pPlayer, LagRecord_t* m_LagRecord, LagRecord_t* m_PrevRecord)
{
	/* Get animation layers */
	const AnimationLayer* m_AliveLoop = &m_LagRecord->m_Layers[ANIMATION_LAYER_ALIVELOOP];
	const AnimationLayer* m_PrevAliveLoop = &m_PrevRecord->m_Layers[ANIMATION_LAYER_ALIVELOOP];

	/* Get ticks animated on the server ( by default it's simtime delta ) */
	int nTicksAnimated = m_LagRecord->m_nSimulationTicks;
	if (m_AliveLoop->m_flPlaybackRate == m_PrevAliveLoop->m_flPlaybackRate)
		nTicksAnimated = (m_AliveLoop->m_flCycle - m_PrevAliveLoop->m_flCycle) / (m_AliveLoop->m_flPlaybackRate * m_globals()->m_intervalpertick);
	else
		nTicksAnimated = ((((m_AliveLoop->m_flCycle / m_AliveLoop->m_flPlaybackRate) + ((1.0f - m_PrevAliveLoop->m_flCycle) / m_PrevAliveLoop->m_flPlaybackRate)) / m_globals()->m_intervalpertick));

	return min(max(nTicksAnimated, m_LagRecord->m_nSimulationTicks), 17);
}
void C_PlayerAnimations::HandleDormancyLeaving(player_t* pPlayer, LagRecord_t* m_Record, AnimState_s* m_AnimationState)
{
	/* Get animation layers */
	const AnimationLayer* m_JumpingLayer = &m_Record->m_Layers[ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL];
	const AnimationLayer* m_LandingLayer = &m_Record->m_Layers[ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB];

	/* Final m_flLastUpdateTime */
	float m_flLastUpdateTime = m_Record->m_flSimulationTime - m_globals()->m_intervalpertick;

	/* Fix animation state timing */
	if (m_Record->m_nFlags & FL_ONGROUND) /* On ground */
	{
		/* Use information from landing */
		int nActivity = pPlayer->sequence_activity(m_LandingLayer->m_nSequence);
		if (nActivity == ACT_CSGO_LAND_HEAVY || nActivity == ACT_CSGO_LAND_LIGHT)
		{
			/* Compute land duration */
			float flLandDuration = m_LandingLayer->m_flCycle / m_LandingLayer->m_flPlaybackRate;

			/* Check landing time */
			float flLandingTime = m_Record->m_flSimulationTime - flLandDuration;
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
			float flJumpingTime = m_Record->m_flSimulationTime - flDurationInAir;
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

void C_PlayerAnimations::SetupCollision(player_t* pPlayer, LagRecord_t* m_LagRecord)
{

	ICollideable* m_Collideable = pPlayer->GetCollideable();
	if (!m_Collideable)
		return;

	pPlayer->UpdateCollisionBounds();
	m_LagRecord->m_vecMins = m_Collideable->OBBMins();
	m_LagRecord->m_vecMaxs = m_Collideable->OBBMaxs();
}

void C_PlayerAnimations::UpdatePlayerAnimations(player_t* pPlayer, LagRecord_t* m_LagRecord, AnimState_s* m_AnimationState)
{
	/* Bypass this check https://github.com/perilouswithadollarsign/cstrike15_src/blob/f82112a2388b841d72cb62ca48ab1846dfcc11c8/game/shared/cstrike15/csgo_playeranimstate.cpp#L266 */
	m_AnimationState->m_nLastUpdateFrame = 0;
	if (m_AnimationState->m_flLastUpdateTime == m_globals()->m_curtime)
		m_AnimationState->m_flLastUpdateTime = m_globals()->m_curtime + m_globals()->m_intervalpertick;

	/* Force animation state player and previous weapon */
	m_AnimationState->m_pBaseEntity = pPlayer;
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

void C_PlayerAnimations::SimulatePlayerActivity(player_t* pPlayer, LagRecord_t* m_LagRecord, LagRecord_t* m_PrevRecord)
{
	/* Get animation layers */
	const AnimationLayer* m_JumpingLayer = &m_LagRecord->m_Layers[ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL];
	const AnimationLayer* m_LandingLayer = &m_LagRecord->m_Layers[ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB];
	const AnimationLayer* m_PrevJumpingLayer = &m_PrevRecord->m_Layers[ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL];
	const AnimationLayer* m_PrevLandingLayer = &m_PrevRecord->m_Layers[ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB];

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
					m_LagRecord->m_nActivityTick = TIME_TO_TICKS(m_LagRecord->m_flSimulationTime - m_LagRecord->m_flDurationInAir) + 1;
					m_LagRecord->m_nActivityType = EPlayerActivity::Jump;
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
					m_LagRecord->m_nActivityTick = TIME_TO_TICKS(m_LagRecord->m_flSimulationTime - flLandDuration) + 1;
					m_LagRecord->m_nActivityType = EPlayerActivity::Land;

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

float C_PlayerAnimations::ComputeActivityPlayback(player_t* pPlayer, LagRecord_t* m_Record)
{
	/* Get animation layers */
	AnimationLayer* m_JumpingLayer = &m_Record->m_Layers[ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL];
	AnimationLayer* m_LandingLayer = &m_Record->m_Layers[ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB];

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
void C_PlayerAnimations::SetupBones(player_t* pPlayer, int nBoneMask, matrix3x4_t* aMatrix)
{
	/* rebuild setup bones later */
	g_ctx.globals.setuping_bones = true;
	pPlayer->SetupBones(aMatrix, MAXSTUDIOBONES, nBoneMask, 0.0f);
	g_ctx.globals.setuping_bones = false;
}
void C_PlayerAnimations::SetupPlayerMatrix(player_t* pPlayer, LagRecord_t* m_Record, matrix3x4_t* Matrix, int nFlags)
{
	/* Reset layers */
	std::memcpy(pPlayer->get_animlayers(), m_Record->m_Layers.data(), sizeof(AnimationLayer) * 13);

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
	int nSimulationTick = TIME_TO_TICKS(m_Record->m_flSimulationTime);
	m_globals()->m_curtime = m_Record->m_flSimulationTime;
	m_globals()->m_realtime = m_Record->m_flSimulationTime;
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
		pPlayer->set_abs_origin(m_Record->m_vecOrigin);

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
	std::memcpy(pPlayer->get_animlayers(), m_Record->m_Layers.data(), sizeof(AnimationLayer) * 13);

	/* Restore game's globals */
	Globals.AdjustData();
}
float C_PlayerAnimations::BuildFootYaw(player_t* pPlayer, LagRecord_t* m_LagRecord)
{
	AnimState_s* m_AnimationState = pPlayer->GetAnimState();
	if (!m_AnimationState)
		return 0.0f;


	float flAimMatrixWidthRange = math::lerp(std::clamp(m_AnimationState->m_flSpeedAsPortionOfWalkTopSpeed, 0.f, 1.f), 1.0f, math::lerp(m_AnimationState->m_flWalkToRunTransition, 0.8f, 0.5f));
	if (m_AnimationState->m_flAnimDuckAmount > 0)
		flAimMatrixWidthRange = math::lerp(m_AnimationState->m_flAnimDuckAmount * std::clamp(m_AnimationState->m_flSpeedAsPortionOfCrouchTopSpeed, 0.0f, 1.0f), flAimMatrixWidthRange, 0.5f);

	float flTempYawMax = m_AnimationState->m_flAimYawMax * flAimMatrixWidthRange;
	float flTempYawMin = m_AnimationState->m_flAimYawMin * flAimMatrixWidthRange;

	float flFootYaw = math::normalize_yaw(m_LagRecord->m_flEyeYaw);
	float flEyeFootDelta = math::AngleDiff(m_LagRecord->m_angEyeAngles.y, flFootYaw);
	if (flEyeFootDelta > flTempYawMax)
		flFootYaw = m_LagRecord->m_angEyeAngles.y - fabs(flTempYawMax);
	else if (flEyeFootDelta < flTempYawMin)
		flFootYaw = m_LagRecord->m_angEyeAngles.y + fabs(flTempYawMin);

	if (m_AnimationState->m_bOnGround)
	{
		if (m_AnimationState->m_flVelocityLengthXY > 0.1f || m_AnimationState->m_flVelocityLengthZ > 100.0f)
			flFootYaw = math::ApproachAngle(m_LagRecord->m_flEyeYaw, flFootYaw, m_globals()->m_intervalpertick * (30.0f + (20.0f * m_AnimationState->m_flWalkToRunTransition)));
		else
			flFootYaw = math::ApproachAngle(m_LagRecord->m_flLowerBodyYaw, flFootYaw, m_globals()->m_intervalpertick * 100.f);
	}

	return math::normalize_yaw(flFootYaw);
}
void C_PlayerAnimations::GenerateSafePoints(player_t* pPlayer, LagRecord_t* m_LagRecord)
{
	auto GetYawRotation = [&](int ESafeSied) -> float
	{
		float flOldEyeYaw = m_LagRecord->m_flEyeYaw;
		// set eye yaw
		float flEyeRotation = m_LagRecord->m_angEyeAngles.y;
		switch (ESafeSied)
		{
		case LeftMatrix: m_LagRecord->m_flEyeYaw = math::normalize_yaw(flEyeRotation - 58.0f); break;
		case ZeroMatrix: m_LagRecord->m_flEyeYaw = math::normalize_yaw(flEyeRotation); break;
		case RightMatrix: m_LagRecord->m_flEyeYaw = math::normalize_yaw(flEyeRotation + 58.0f); break;
		case LowLeftMatrix: m_LagRecord->m_flEyeYaw = math::normalize_yaw(flEyeRotation - 29.0f); break;
		case LowRightMatrix: m_LagRecord->m_flEyeYaw = math::normalize_yaw(flEyeRotation + 29.0f); break;
		}

		// generate foot yaw
		float flFootYaw = BuildFootYaw(pPlayer, m_LagRecord);

		// restore eye yaw                                   
		m_LagRecord->m_flEyeYaw = flOldEyeYaw;

		// return result
		return flFootYaw;
	};
	// point building func
	auto BuildSafePoint = [&](int ESafeSied)
	{
		// save animation data
		std::array < AnimationLayer, 13 > m_Layers;
		std::array < float, 24 > m_PoseParameters;
		AnimState_s m_AnimationState;
		// copy data
		std::memcpy(m_Layers.data(), pPlayer->get_animlayers(), sizeof(AnimationLayer) * 13);
		std::memcpy(m_PoseParameters.data(), pPlayer->m_flPoseParameter().data(), sizeof(float) * 24);
		std::memcpy(&m_AnimationState, pPlayer->GetAnimState(), sizeof(AnimState_s));

		// set foot yaw
		pPlayer->GetAnimState()->m_flFootYaw = GetYawRotation(ESafeSied);

		// update player animations
		UpdatePlayerAnimations(pPlayer, m_LagRecord, pPlayer->GetAnimState());

		// get matrix
		matrix3x4_t* aMatrix = nullptr;
		switch (ESafeSied)
		{
		case LeftMatrix: aMatrix = m_LagRecord->m_Matricies[LeftMatrix].data(); break;
		case ZeroMatrix: aMatrix = m_LagRecord->m_Matricies[ZeroMatrix].data(); break;
		case RightMatrix: aMatrix = m_LagRecord->m_Matricies[RightMatrix].data(); break;
		case LowLeftMatrix: aMatrix = m_LagRecord->m_Matricies[LowLeftMatrix].data(); break;
		case LowRightMatrix: aMatrix = m_LagRecord->m_Matricies[LowRightMatrix].data(); break;
		}

		// setup bones
		SetupPlayerMatrix(pPlayer, m_LagRecord, aMatrix, EMatrixFlags::BoneUsedByHitbox);

		// restore data
		std::memcpy(pPlayer->get_animlayers(), m_Layers.data(), sizeof(AnimationLayer) * 13);
		std::memcpy(pPlayer->m_flPoseParameter().data(), m_PoseParameters.data(), sizeof(float) * 24);
		std::memcpy(pPlayer->GetAnimState(), &m_AnimationState, sizeof(AnimState_s));
	};
	// check conditions
	if (!g_ctx.local()->is_alive() || !g_cfg.ragebot.enable)
		return;

	// build safe points
	BuildSafePoint(LeftMatrix);
	BuildSafePoint(RightMatrix);
	//BuildSafePoint(3);
	//BuildSafePoint(4);
	BuildSafePoint(ZeroMatrix);
}
