#include <tuple>
#include "LocalAnimFix.hpp"

void C_LocalAnimations::OnCreateMove(CUserCmd* m_pCmd)
{
	// g_LocalAnimations->StoreAnimationRecord(m_pCmd);
	if (!g_ctx.send_packet)
		return;

	std::tuple < float, float, float, float, float, int, int > m_Globals = std::make_tuple
	(
		m_globals()->m_curtime,
		m_globals()->m_realtime,
		m_globals()->m_frametime,
		m_globals()->m_absoluteframetime,
		m_globals()->m_interpolation_amount,

		m_globals()->m_framecount,
		m_globals()->m_tickcount
	);

	this->m_LocalData.m_nSimulationTicks = m_clientstate()->iChokedCommands + 1;
	std::tuple < Vector, Vector, float, float, Vector, Vector, Vector, Vector, int, int, int, float, float > m_Data = std::make_tuple
	(
		g_ctx.local()->m_angVisualAngles(),
		g_ctx.local()->m_angEyeAngles(),
		g_ctx.local()->m_flDuckAmount(),
		g_ctx.local()->m_flDuckSpeed(),
		g_ctx.local()->GetAbsOrigin(),
		g_ctx.local()->m_vecOrigin(),
		g_ctx.local()->m_vecAbsVelocity(),
		g_ctx.local()->m_vecVelocity(),
		g_ctx.local()->m_iEFlags(),
		g_ctx.local()->m_fFlags(),
		g_ctx.local()->GetMoveType(),
		g_ctx.local()->m_flThirdpersonRecoil(),
		g_ctx.local()->m_flLowerBodyYawTarget()
	);

	/* set localplayer entity's flags */
	g_ctx.local()->m_iEFlags() &= ~(EFL_DIRTY_ABSVELOCITY | EFL_DIRTY_ABSTRANSFORM);

	/* remove interpolation amount */
	m_globals()->m_interpolation_amount = 0.0f;

	/* shot data */
	std::tuple < Vector, bool > m_ShotData = std::make_tuple < Vector, bool >(Vector(0, 0, 0), false);

	/* copy data */
	// g_LocalAnimations->CopyPlayerAnimationData(false);

	/* UpdatePlayerAnimations */
	for (int nSimulationTick = 1; nSimulationTick <= m_LocalData.m_nSimulationTicks; nSimulationTick++)
	{
		/* determine the tickbase and set globals to it */
		int nTickBase = g_ctx.local()->m_nTickBase() - m_LocalData.m_nSimulationTicks + nSimulationTick;
		m_globals()->m_curtime = TICKS_TO_TIME(nTickBase);
		m_globals()->m_realtime = TICKS_TO_TIME(nTickBase);
		m_globals()->m_frametime = m_globals()->m_intervalpertick;
		m_globals()->m_absoluteframetime = m_globals()->m_intervalpertick;
		m_globals()->m_framecount = nTickBase;
		m_globals()->m_tickcount = nTickBase;

		AnimationRecord_t* m_Record = &m_LocalData.m_AnimRecords[(m_pCmd->m_command_number - m_LocalData.m_nSimulationTicks + nSimulationTick) % MULTIPLAYER_BACKUP];
		if (m_Record)
		{
			/* set player data from the animation record */
			g_ctx.local()->m_flThirdpersonRecoil() = m_Record->m_angAimPunch.x * m_cvar()->FindVar("weapon_recoil_scale")->GetFloat();
			g_ctx.local()->m_vecVelocity() = m_Record->m_vecVelocity;
			g_ctx.local()->m_vecAbsVelocity() = m_Record->m_vecVelocity;
			g_ctx.local()->m_flDuckAmount() = m_Record->m_flDuckAmount;
			g_ctx.local()->m_flDuckSpeed() = m_Record->m_flDuckSpeed;
			g_ctx.local()->m_angVisualAngles() = m_Record->m_angRealAngles;
			g_ctx.local()->m_angEyeAngles() = m_Record->m_angRealAngles;
			g_ctx.local()->m_fFlags() = m_Record->m_nFlags;
			g_ctx.local()->GetMoveType() = m_Record->m_nMoveType;

			/* fix localplayer strafe and sequences */
			//// g_LocalAnimations->SimulateStrafe(m_Record->m_nButtons);
			//// g_LocalAnimations->DoAnimationEvent(m_Record->m_nButtons);

			/* set shot angle */
			if (nSimulationTick == m_LocalData.m_nSimulationTicks)
			{
				if (std::get < 1 >(m_ShotData))
				{
					g_ctx.local()->m_angVisualAngles() = std::get < 0 >(m_ShotData);
					g_ctx.local()->m_angEyeAngles() = std::get < 0 >(m_ShotData);
				}
			}
			else
			{
				if (m_Record->m_bIsShooting)
				{
					std::get < 0 >(m_ShotData) = m_Record->m_angRealAngles;
					std::get < 1 >(m_ShotData) = true;
				}
			}
		}

		/* Fix framecount and time */
		g_ctx.local()->get_animation_state1()->m_iLastClientSideAnimationUpdateFramecount = 0;
		g_ctx.local()->get_animation_state1()->m_iLastClientSideAnimationUpdateFramecount = m_globals()->m_curtime - m_globals()->m_intervalpertick;

		/* set player and weapon */
		g_ctx.local()->get_animation_state1()->m_pBaseEntity = g_ctx.local();
		g_ctx.local()->get_animation_state1()->m_pActiveWeapon = g_ctx.local()->m_hActiveWeapon();

		/* force client-side animation */
		bool m_bClientSideAnimation = g_ctx.local()->m_bClientSideAnimation();
		g_ctx.local()->m_bClientSideAnimation() = true;

		/* update localplayer animations */
		g_ctx.globals.updating_animation = true;
		g_ctx.local()->update_clientside_animation();
		g_ctx.globals.updating_animation = false;

		/* restore client-side animation */
		g_ctx.local()->m_bClientSideAnimation() = m_bClientSideAnimation;
	}

	/* copy layers */
	std::memcpy(m_LocalData.m_Real.m_Layers.data(), g_ctx.local()->get_animlayers(), sizeof(AnimationLayer) * 13);
	std::memcpy(m_LocalData.m_Real.m_PoseParameters.data(), g_ctx.local()->m_flPoseParameter().data(), sizeof(float) * MAXSTUDIOPOSEPARAM);

	g_ctx.local()->set_abs_origin(m_LocalData.m_vecAbsOrigin);
	//if ( !g_Globals->m_Packet.m_bSkipMatrix )
	//// g_LocalAnimations->SetupPlayerBones(m_LocalData.m_Real.m_Matrix.data(), BONE_USED_BY_ANYTHING);

	// setup bones
	g_ctx.globals.setuping_bones = true;
	// now we setup the bones after everything is done.
	g_ctx.local()->setup_bones_local(m_LocalData.m_Real.m_Matrix.data(), MAXSTUDIOBONES, BONE_USED_BY_ANYTHING, 0);
	g_ctx.globals.setuping_bones = false;

	// g_LocalAnimations->UpdateDesyncAnimations(m_pCmd);

	/* restore globals */
	m_globals()->m_curtime = std::get < 0 >(m_Globals);
	m_globals()->m_realtime = std::get < 1 >(m_Globals);
	m_globals()->m_frametime = std::get < 2 >(m_Globals);
	m_globals()->m_absoluteframetime = std::get < 3 >(m_Globals);
	m_globals()->m_interpolation_amount = std::get < 4 >(m_Globals);
	m_globals()->m_framecount = std::get < 5 >(m_Globals);
	m_globals()->m_tickcount = std::get < 6 >(m_Globals);

	/* restore changed localplayer's data */
	g_ctx.local()->m_angVisualAngles() = std::get < 0 >(m_Data);
	g_ctx.local()->m_angEyeAngles() = std::get < 1 >(m_Data);
	g_ctx.local()->m_flDuckAmount() = std::get < 2 >(m_Data);
	g_ctx.local()->m_flDuckSpeed() = std::get < 3 >(m_Data);
	g_ctx.local()->m_vecOrigin() = std::get < 5 >(m_Data);
	g_ctx.local()->m_vecAbsVelocity() = std::get < 6 >(m_Data);
	g_ctx.local()->m_vecVelocity() = std::get < 7 >(m_Data);
	g_ctx.local()->m_iEFlags() = std::get < 8 >(m_Data);
	g_ctx.local()->m_fFlags() = std::get < 9 >(m_Data);
	g_ctx.local()->GetMoveType() = std::get < 10 >(m_Data);
	g_ctx.local()->m_flThirdpersonRecoil() = std::get < 11 >(m_Data);
	g_ctx.local()->m_flLowerBodyYawTarget() = std::get < 12 >(m_Data);
}
void C_LocalAnimations::CopyPlayerAnimationData(bool bFake)
{
	std::array < AnimationLayer, 13 > m_Layers = m_LocalData.m_Real.m_Layers;
	if (bFake)
		m_Layers = m_LocalData.m_Fake.m_Layers;

	std::memcpy
	(
		&g_ctx.local()->get_animlayers()[ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL],
		&m_Layers[ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL],
		sizeof(AnimationLayer)
	);
	std::memcpy
	(
		&g_ctx.local()->get_animlayers()[ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB],
		&m_Layers[ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB],
		sizeof(AnimationLayer)
	);
	std::memcpy
	(
		&g_ctx.local()->get_animlayers()[ANIMATION_LAYER_ALIVELOOP],
		&m_Layers[ANIMATION_LAYER_ALIVELOOP],
		sizeof(AnimationLayer)
	);
}
void C_LocalAnimations::UpdateDesyncAnimations(CUserCmd* m_pCmd)
{
	C_CSGOPlayerAnimationState m_AnimationState;
	std::memcpy(&m_AnimationState, g_ctx.local()->get_animation_state1(), sizeof(C_CSGOPlayerAnimationState));

	std::memcpy
	(
		&g_ctx.local()->get_animlayers()[ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL],
		&m_LocalData.m_Fake.m_Layers[ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL],
		sizeof(AnimationLayer)
	);
	std::memcpy
	(
		&g_ctx.local()->get_animlayers()[ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB],
		&m_LocalData.m_Fake.m_Layers[ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB],
		sizeof(AnimationLayer)
	);
	std::memcpy
	(
		&g_ctx.local()->get_animlayers()[ANIMATION_LAYER_ALIVELOOP],
		&m_LocalData.m_Fake.m_Layers[ANIMATION_LAYER_ALIVELOOP],
		sizeof(AnimationLayer)
	);

	std::memcpy(g_ctx.local()->get_animation_state1(), &m_LocalData.m_Fake.m_AnimationState, sizeof(C_CSGOPlayerAnimationState));
	std::memcpy(g_ctx.local()->m_flPoseParameter().data(), m_LocalData.m_Fake.m_PoseParameters.data(), sizeof(float) * MAXSTUDIOPOSEPARAM);

	std::tuple < Vector, bool > m_ShotData = std::make_tuple < Vector, bool >(Vector(0, 0, 0), false);

	/* UpdatePlayerAnimations */
	for (int nSimulationTick = 1; nSimulationTick <= m_LocalData.m_nSimulationTicks; nSimulationTick++)
	{
		/* determine the tickbase and set globals to it */
		int nTickBase = g_ctx.local()->m_nTickBase() - m_LocalData.m_nSimulationTicks + nSimulationTick;
		m_globals()->m_curtime = TICKS_TO_TIME(nTickBase);
		m_globals()->m_realtime = TICKS_TO_TIME(nTickBase);
		m_globals()->m_frametime = m_globals()->m_intervalpertick;
		m_globals()->m_absoluteframetime = m_globals()->m_intervalpertick;
		m_globals()->m_framecount = nTickBase;
		m_globals()->m_tickcount = nTickBase;

		AnimationRecord_t* m_Record = &m_LocalData.m_AnimRecords[(m_pCmd->m_command_number - m_LocalData.m_nSimulationTicks + nSimulationTick) % MULTIPLAYER_BACKUP];
		if (m_Record)
		{
			/* set player data from the animation record */
			g_ctx.local()->m_flThirdpersonRecoil() = m_Record->m_angAimPunch.x * m_cvar()->FindVar("weapon_recoil_scale")->GetFloat();
			g_ctx.local()->m_vecVelocity() = m_Record->m_vecVelocity;
			g_ctx.local()->m_vecAbsVelocity() = m_Record->m_vecVelocity;
			g_ctx.local()->m_flDuckAmount() = m_Record->m_flDuckAmount;
			g_ctx.local()->m_flDuckSpeed() = m_Record->m_flDuckSpeed;
			g_ctx.local()->m_angVisualAngles() = m_Record->m_angFakeAngles;
			g_ctx.local()->m_angEyeAngles() = m_Record->m_angFakeAngles;
			g_ctx.local()->m_fFlags() = m_Record->m_nFlags;
			g_ctx.local()->GetMoveType() = m_Record->m_nMoveType;

			/* fix localplayer strafe and sequences */
			//// g_LocalAnimations->SimulateStrafe(m_Record->m_nButtons);
			//// g_LocalAnimations->DoAnimationEvent(m_Record->m_nButtons, true);

			/* set shot angle */
			if (nSimulationTick == m_LocalData.m_nSimulationTicks)
			{
				if (std::get < 1 >(m_ShotData))
				{
					g_ctx.local()->m_angVisualAngles() = std::get < 0 >(m_ShotData);
					g_ctx.local()->m_angEyeAngles() = std::get < 0 >(m_ShotData);
				}
			}
			else
			{
				if (m_Record->m_bIsShooting)
				{
					std::get < 0 >(m_ShotData) = m_Record->m_angRealAngles;
					std::get < 1 >(m_ShotData) = true;
				}
			}
		}

		/* Fix framecount */
		g_ctx.local()->get_animation_state1()->m_iLastClientSideAnimationUpdateFramecount = 0;

		/* set player and weapon */
		g_ctx.local()->get_animation_state1()->m_pBaseEntity = g_ctx.local();
		g_ctx.local()->get_animation_state1()->m_pActiveWeapon = g_ctx.local()->m_hActiveWeapon();

		/* force client-side animation */
		bool m_bClientSideAnimation = g_ctx.local()->m_bClientSideAnimation();
		g_ctx.local()->m_bClientSideAnimation() = true;

		/* update localplayer animations */
		g_ctx.globals.updating_animation = true;
		g_ctx.local()->update_clientside_animation();
		g_ctx.globals.updating_animation = false;

		/* restore client-side animation */
		g_ctx.local()->m_bClientSideAnimation() = m_bClientSideAnimation;
	}

	std::memcpy(&m_LocalData.m_Fake.m_AnimationState, g_ctx.local()->get_animation_state1(), sizeof(C_CSGOPlayerAnimationState));
	std::memcpy(m_LocalData.m_Fake.m_Layers.data(), g_ctx.local()->get_animlayers(), sizeof(AnimationLayer) * 13);
	std::memcpy(m_LocalData.m_Fake.m_PoseParameters.data(), g_ctx.local()->m_flPoseParameter().data(), sizeof(float) * MAXSTUDIOPOSEPARAM);

	std::memcpy
	(
		&g_ctx.local()->get_animlayers()[ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL],
		&m_LocalData.m_Real.m_Layers[ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL],
		sizeof(AnimationLayer)
	);
	std::memcpy
	(
		&g_ctx.local()->get_animlayers()[ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB],
		&m_LocalData.m_Real.m_Layers[ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB],
		sizeof(AnimationLayer)
	);

	g_ctx.local()->m_flPoseParameter()[1] = m_LocalData.m_Real.m_PoseParameters[1];
	std::memcpy(&g_ctx.local()->get_animlayers()[7], &m_LocalData.m_Real.m_Layers[7], sizeof(AnimationLayer));

	m_LocalData.m_flYawDelta = std::roundf(math::AngleDiff(math::normalize_yaw(g_ctx.local()->get_animation_state1()->m_flGoalFeetYaw), math::normalize_yaw(m_AnimationState.m_flGoalFeetYaw)));

	//// g_LocalAnimations->SetupPlayerBones(m_LocalData.m_Fake.m_Matrix.data(), BONE_USED_BY_ANYTHING);

	// setup bones
	g_ctx.globals.setuping_bones = true;
	// now we setup the bones after everything is done.
	g_ctx.local()->setup_bones_local(m_LocalData.m_Fake.m_Matrix.data(), MAXSTUDIOBONES, BONE_USED_BY_ANYTHING, 0);
	g_ctx.local()->setup_bones_local(g_ctx.globals.fake_matrix, MAXSTUDIOBONES, BONE_USED_BY_ANYTHING, 0);
	g_ctx.globals.setuping_bones = false;

	std::memcpy(g_ctx.local()->get_animation_state1(), &m_AnimationState, sizeof(C_CSGOPlayerAnimationState));
	std::memcpy(g_ctx.local()->get_animlayers(), m_LocalData.m_Real.m_Layers.data(), sizeof(AnimationLayer) * 13);
	std::memcpy(g_ctx.local()->m_flPoseParameter().data(), m_LocalData.m_Real.m_PoseParameters.data(), sizeof(float) * MAXSTUDIOPOSEPARAM);
}
//void C_LocalAnimations::SimulateStrafe(int nButtons)
//{
//	Vector vecForward;
//	Vector vecRight;
//	Vector vecUp;
//
//	math::AngleVectorsAnims(QAngle(0, g_ctx.local()->get_animation_state1()->m_flGoalFeetYaw, 0), vecForward, vecRight, vecUp);
//	vecRight.NormalizeInPlace();
//
//	float flVelToRightDot = math::dot_product(g_ctx.local()->get_animation_state1()->m_vLastVelocity, vecRight);
//	float flVelToForwardDot = math::dot_product(g_ctx.local()->get_animation_state1()->m_vLastVelocity, vecForward);
//
//	bool bMoveRight = (nButtons & (IN_MOVERIGHT)) != 0;
//	bool bMoveLeft = (nButtons & (IN_MOVELEFT)) != 0;
//	bool bMoveForward = (nButtons & (IN_FORWARD)) != 0;
//	bool bMoveBackward = (nButtons & (IN_BACK)) != 0;
//
//	bool bStrafeRight = (g_ctx.local()->get_animation_state1()->m_flSpeedAsPortionOfWalkTopSpeed >= 0.73f && bMoveRight && !bMoveLeft && flVelToRightDot < -0.63f);
//	bool bStrafeLeft = (g_ctx.local()->get_animation_state1()->m_flSpeedAsPortionOfWalkTopSpeed >= 0.73f && bMoveLeft && !bMoveRight && flVelToRightDot > 0.63f);
//	bool bStrafeForward = (g_ctx.local()->get_animation_state1()->m_flSpeedAsPortionOfWalkTopSpeed >= 0.65f && bMoveForward && !bMoveBackward && flVelToForwardDot < -0.55f);
//	bool bStrafeBackward = (g_ctx.local()->get_animation_state1()->m_flSpeedAsPortionOfWalkTopSpeed >= 0.65f && bMoveBackward && !bMoveForward && flVelToForwardDot > 0.55f);
//
//	g_ctx.local()->m_bStrafing() = (bStrafeRight || bStrafeLeft || bStrafeForward || bStrafeBackward);
//}
void C_LocalAnimations::DoAnimationEvent(int nButtons, bool bIsFakeAnimations)
{
	AnimationLayer* pLandOrClimbLayer = &g_ctx.local()->get_animlayers()[ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB];
	if (!pLandOrClimbLayer)
		return;

	AnimationLayer* pJumpOrFallLayer = &g_ctx.local()->get_animlayers()[ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL];
	if (!pJumpOrFallLayer)
		return;

	int nCurrentFlags = m_LocalData.m_Real.m_nFlags;
	if (bIsFakeAnimations)
		nCurrentFlags = m_LocalData.m_Fake.m_nFlags;

	int nCurrentMoveType = m_LocalData.m_Real.m_nMoveType;
	if (bIsFakeAnimations)
		nCurrentMoveType = m_LocalData.m_Fake.m_nMoveType;

	if (nCurrentMoveType != MOVETYPE_LADDER && g_ctx.local()->GetMoveType() == MOVETYPE_LADDER)
		g_ctx.local()->get_animation_state1()->SetLayerSequence(pLandOrClimbLayer, ACT_CSGO_CLIMB_LADDER);
	else if (nCurrentMoveType == MOVETYPE_LADDER && g_ctx.local()->GetMoveType() != MOVETYPE_LADDER)
		g_ctx.local()->get_animation_state1()->SetLayerSequence(pJumpOrFallLayer, ACT_CSGO_FALL);
	else
	{
		if (g_ctx.local()->m_fFlags() & FL_ONGROUND)
		{
			if (!(nCurrentFlags & FL_ONGROUND))
				g_ctx.local()->get_animation_state1()->SetLayerSequence
				(
					pLandOrClimbLayer,
					g_ctx.local()->get_animation_state1()->m_flInAirTime() > 1.0f ? ACT_CSGO_LAND_HEAVY : ACT_CSGO_LAND_LIGHT
				);
		}
		else if (nCurrentFlags & FL_ONGROUND)
		{
			if (g_ctx.local()->m_vecVelocity().z > 0.0f)
				g_ctx.local()->get_animation_state1()->SetLayerSequence(pJumpOrFallLayer, ACT_CSGO_JUMP);
			else
				g_ctx.local()->get_animation_state1()->SetLayerSequence(pJumpOrFallLayer, ACT_CSGO_FALL);
		}
	}

	if (bIsFakeAnimations)
	{
		m_LocalData.m_Fake.m_nMoveType = g_ctx.local()->GetMoveType();
		m_LocalData.m_Fake.m_nFlags = g_ctx.local()->m_fFlags();
	}
	else
	{
		m_LocalData.m_Real.m_nMoveType = g_ctx.local()->GetMoveType();
		m_LocalData.m_Real.m_nFlags = g_ctx.local()->m_fFlags();
	}
}



void C_LocalAnimations::StoreAnimationRecord(CUserCmd* m_pCmd)
{
	AnimationRecord_t m_AnimRecord;

	// store record data
	m_AnimRecord.m_nFlags = m_LocalData.m_nFlags;
	m_AnimRecord.m_vecOrigin = g_ctx.local()->m_vecOrigin();
	m_AnimRecord.m_vecVelocity = g_ctx.local()->m_vecVelocity();
	m_AnimRecord.m_flDuckAmount = g_ctx.local()->m_flDuckAmount();
	m_AnimRecord.m_flDuckSpeed = g_ctx.local()->m_flDuckSpeed();
	m_AnimRecord.m_angRealAngles = m_pCmd->m_viewangles;
	m_AnimRecord.m_angFakeAngles = g_ctx.globals.m_angFakeAngles;
	m_AnimRecord.m_angAimPunch = g_ctx.local()->m_aimPunchAngle();
	m_AnimRecord.m_nButtons = m_pCmd->m_buttons;
	m_AnimRecord.m_nMoveType = g_ctx.local()->GetMoveType();

	weapon_t* pWeapon = g_ctx.local()->m_hActiveWeapon();
	if (pWeapon)
	{
		if (pWeapon->is_grenade())
		{
			if (!pWeapon->m_bPinPulled() && pWeapon->m_fThrowTime() > 0.0f)
				m_AnimRecord.m_bIsShooting = true;
		}
		else
		{
			if (m_pCmd->m_buttons & IN_ATTACK)
				m_AnimRecord.m_bIsShooting = true;

			if (pWeapon->is_knife())
				if ((m_pCmd->m_buttons & IN_ATTACK) || (m_pCmd->m_buttons & IN_ATTACK2))
					m_AnimRecord.m_bIsShooting = true;
		}
	}

	if (m_AnimRecord.m_bIsShooting)
		m_AnimRecord.m_angFakeAngles = m_pCmd->m_viewangles;
	m_AnimRecord.m_angFakeAngles.z = 0.0f;

	/* proper roll aa display */
	m_LocalData.m_AnimRecords[m_pCmd->m_command_number % MULTIPLAYER_BACKUP] = m_AnimRecord;
}
void C_LocalAnimations::BeforePrediction()
{
	m_LocalData.m_nFlags = g_ctx.local()->m_fFlags();
	m_LocalData.m_vecAbsOrigin = g_ctx.local()->GetAbsOrigin();

	if (m_LocalData.m_flSpawnTime != g_ctx.local()->m_flSpawnTime())
	{
		std::memcpy(&m_LocalData.m_Fake.m_AnimationState, g_ctx.local()->get_animation_state1(), sizeof(C_CSGOPlayerAnimationState));
		std::memcpy(m_LocalData.m_Fake.m_Layers.data(), g_ctx.local()->get_animlayers(), sizeof(AnimationLayer) * 13);
		std::memcpy(m_LocalData.m_Fake.m_PoseParameters.data(), g_ctx.local()->m_flPoseParameter().data(), sizeof(float) * MAXSTUDIOPOSEPARAM);
	}
	m_LocalData.m_flSpawnTime = g_ctx.local()->m_flSpawnTime();
}
void C_LocalAnimations::SetupShootPosition(CUserCmd* m_pCmd)
{
	/* fix view offset */
	Vector vecViewOffset = g_ctx.local()->m_vecViewOffset();
	if (vecViewOffset.z <= 46.05f)
		vecViewOffset.z = 46.0f;
	else if (vecViewOffset.z > 64.0f)
		vecViewOffset.z = 64.0f;

	/* calculate default shoot position */
	m_LocalData.m_vecShootPosition = g_ctx.local()->m_vecOrigin() + vecViewOffset;

	/* backup data */
	std::tuple < Vector, Vector > m_Backup = std::make_tuple(g_ctx.local()->GetAbsOrigin(), g_ctx.local()->m_angEyeAngles());

	/* force LocalPlayer data */
	g_ctx.local()->set_abs_origin(g_ctx.local()->m_vecOrigin());
	g_ctx.local()->m_angEyeAngles() = m_pCmd->m_viewangles;

	/* normalize angles */
	math::normalize_angles(m_pCmd->m_viewangles);
	math::clamp_angles(m_pCmd->m_viewangles);

	/* modify eye position rebuild */
	{
		/* should we modify eye pos */
		bool bModifyEyePosition = false;

		/* modify eye pos on land */
		static int Flags = g_ctx.local()->m_fFlags();
		if (Flags != g_ctx.local()->m_fFlags())
		{
			if (!(Flags & FL_ONGROUND) && (g_ctx.local()->m_fFlags() & FL_ONGROUND))
				bModifyEyePosition = true;

			Flags = g_ctx.local()->m_fFlags();
		}

		/* modify eye pos on duck */
		if (g_ctx.local()->m_flDuckAmount() != 0.0f)
			bModifyEyePosition = true;

		/* modify eye pos on FD */
		if (g_ctx.globals.fakeducking)
			bModifyEyePosition = true;

		/* modify LocalPlayer's EyePosition */
		if (bModifyEyePosition)
		{
			/* store old body pitch */
			const float m_flOldBodyPitch = g_ctx.local()->m_flPoseParameter()[12];

			/* determine m_flThirdpersonRecoil */
			const float m_flThirdpersonRecoil = g_ctx.local()->m_aimPunchAngle().x * m_cvar()->FindVar("weapon_recoil_scale")->GetFloat();

			/* set body pitch */
			g_ctx.local()->m_flPoseParameter()[12] = std::clamp(math::AngleDiff(math::AngleNormalizeAnims(m_flThirdpersonRecoil), 0.0f), 0.0f, 1.0f);

			/* build matrix */
			//// g_LocalAnimations->SetupPlayerBones(m_LocalData.m_Shoot.m_Matrix.data(), BONE_USED_BY_HITBOX);

			// setup bones
			g_ctx.globals.setuping_bones = true;
			// now we setup the bones after everything is done.
			g_ctx.local()->setup_bones_local(m_LocalData.m_Shoot.m_Matrix.data(), MAXSTUDIOBONES, BONE_USED_BY_HITBOX, 0);
			g_ctx.globals.setuping_bones = false;

			/* reset body pitch */
			g_ctx.local()->m_flPoseParameter()[12] = m_flOldBodyPitch;

			/* C_CSGOPlayerAnimationState::ModifyEyePosition rebuild */
			// g_LocalAnimations->ModifyEyePosition(m_LocalData.m_vecShootPosition, m_LocalData.m_Shoot.m_Matrix.data());
		}
	}

	/* restore LocalPlayer data */
	g_ctx.local()->set_abs_origin(std::get < 0 >(m_Backup));
	g_ctx.local()->m_angEyeAngles() = std::get < 1 >(m_Backup);
}
void C_LocalAnimations::SetupPlayerBones(matrix3x4_t* aMatrix, int nMask)
{
	// save globals
	std::tuple < float, float, float, float, float, int, int > m_Globals = std::make_tuple
	(
		// backup globals
		m_globals()->m_curtime,
		m_globals()->m_realtime,
		m_globals()->m_frametime,
		m_globals()->m_absoluteframetime,
		m_globals()->m_interpolation_amount,

		// backup frame count and tick count
		m_globals()->m_framecount,
		m_globals()->m_tickcount
	);

	// save player data
	std::tuple < int, int, int, int, int, bool > m_PlayerData = std::make_tuple
	(
		g_ctx.local()->m_nLastSkipFramecount(),
		g_ctx.local()->m_fEffects(),
		g_ctx.local()->m_nClientEffects(),
		g_ctx.local()->m_nOcclusionFrame(),
		g_ctx.local()->m_nOcclusionMask(),
		g_ctx.local()->m_bJiggleBones()
	);

	// backup animation layers
	std::array < AnimationLayer, 13 > m_Layers;
	std::memcpy(m_Layers.data(), g_ctx.local()->get_animlayers(), sizeof(AnimationLayer) * 13);

	/* set owners */
	for (int nLayer = 0; nLayer < 13; nLayer++)
	{
		AnimationLayer* m_Layer = &g_ctx.local()->get_animlayers()[nLayer];
		if (!m_Layer)
			continue;

		m_Layer->m_pOwner = g_ctx.local();
	}

	// get simulation time
	float flSimulationTime = TICKS_TO_TIME(m_globals()->m_tickcount);

	// setup globals
	m_globals()->m_curtime = flSimulationTime;
	m_globals()->m_realtime = flSimulationTime;
	m_globals()->m_frametime = m_globals()->m_intervalpertick;
	m_globals()->m_absoluteframetime = m_globals()->m_intervalpertick;
	m_globals()->m_interpolation_amount = 0.0f;
	//m_globals()->m_tickcount = g_Networking->GetServerTick();

	// fix skipanimframe ( part 1 )
	m_globals()->m_framecount = INT_MAX;

	// invalidate bone cache
	g_ctx.local()->invalidate_bone_cache();

	// disable ugly lean animation
	g_ctx.local()->get_animlayers()[ANIMATION_LAYER_LEAN].m_flWeight = 0.0f;

	// disable bones jiggle
	g_ctx.local()->m_bJiggleBones() = false;

	// force client effects
	g_ctx.local()->m_nClientEffects() |= 2; // disable ik

	// force effects to disable interp
	g_ctx.local()->m_fEffects() |= EF_NOINTERP;

	// fix PVS occlusion
	g_ctx.local()->m_nOcclusionFrame() = -1;
	g_ctx.local()->m_nOcclusionMask() &= ~2;

	// fix skipanimframe ( part 2 )
	g_ctx.local()->m_nLastSkipFramecount() = 0;

	// setup bones
	g_ctx.globals.setuping_bones = true;
	// now we setup the bones after everything is done.
	g_ctx.local()->setup_bones_local(aMatrix, MAXSTUDIOBONES, nMask, m_globals()->m_curtime);
	g_ctx.globals.setuping_bones = false;

	// restore animation layers
	std::memcpy(g_ctx.local()->get_animlayers(), m_Layers.data(), sizeof(AnimationLayer) * 13);

	// restore player data
	g_ctx.local()->m_nLastSkipFramecount() = std::get < 0 >(m_PlayerData);
	g_ctx.local()->m_fEffects() = std::get < 1 >(m_PlayerData);
	g_ctx.local()->m_nClientEffects() = std::get < 2 >(m_PlayerData);
	g_ctx.local()->m_nOcclusionFrame() = std::get < 3 >(m_PlayerData);
	g_ctx.local()->m_nOcclusionMask() = std::get < 4 >(m_PlayerData);
	g_ctx.local()->m_bJiggleBones() = std::get < 5 >(m_PlayerData);

	// restore globals
	m_globals()->m_curtime = std::get < 0 >(m_Globals);
	m_globals()->m_realtime = std::get < 1 >(m_Globals);
	m_globals()->m_frametime = std::get < 2 >(m_Globals);
	m_globals()->m_absoluteframetime = std::get < 3 >(m_Globals);
	m_globals()->m_interpolation_amount = std::get < 4 >(m_Globals);

	// restore frame count and tick count
	m_globals()->m_framecount = std::get < 5 >(m_Globals);
	m_globals()->m_tickcount = std::get < 6 >(m_Globals);
}
void C_LocalAnimations::ModifyEyePosition(Vector& vecInputEyePos, matrix3x4_t* aMatrix)
{
	Vector vecHeadPos = Vector
	(
		aMatrix[8][0][3],
		aMatrix[8][1][3],
		aMatrix[8][2][3] + 1.7f
	);

	if (vecHeadPos.z > vecInputEyePos.z)
		return;

	float flLerp = math::RemapValClamped(abs(vecInputEyePos.z - vecHeadPos.z),
		4.0f,
		10.0f,
		0.0f, 1.0f);

	vecInputEyePos.z = math::lerp(flLerp, vecInputEyePos.z, vecHeadPos.z);
}
void C_LocalAnimations::InterpolateMatricies()
{
	// correct matrix
	// g_LocalAnimations->TransformateMatricies();

	// copy bones
	std::memcpy(g_ctx.local()->m_CachedBoneData().Base(), m_LocalData.m_Real.m_Matrix.data(), sizeof(matrix3x4_t) * g_ctx.local()->m_CachedBoneData().Count());
	std::memcpy(g_ctx.local()->GetBoneAccessor().GetBoneArrayForWrite(), m_LocalData.m_Real.m_Matrix.data(), sizeof(matrix3x4_t) * g_ctx.local()->m_CachedBoneData().Count());

	// SetupBones_AttachmentHelper
	return g_ctx.local()->SetupBones_AttachmentHelper();
}
void C_LocalAnimations::TransformateMatricies()
{
	Vector vecOriginDelta = g_ctx.local()->GetAbsOrigin() - m_LocalData.m_Real.m_vecMatrixOrigin;
	for (auto& Matrix : m_LocalData.m_Real.m_Matrix)
	{
		Matrix[0][3] += vecOriginDelta.x;
		Matrix[1][3] += vecOriginDelta.y;
		Matrix[2][3] += vecOriginDelta.z;
	}

	for (auto& Matrix : m_LocalData.m_Fake.m_Matrix)
	{
		Matrix[0][3] += vecOriginDelta.x;
		Matrix[1][3] += vecOriginDelta.y;
		Matrix[2][3] += vecOriginDelta.z;
	}

	m_LocalData.m_Real.m_vecMatrixOrigin = g_ctx.local()->GetAbsOrigin();
}
bool C_LocalAnimations::CopyCachedMatrix(matrix3x4_t* aInMatrix, int nBoneCount)
{
	//std::memcpy(aInMatrix, m_LocalData.m_Real.m_Matrix.data(), sizeof(matrix3x4_t) * nBoneCount);

	return m_LocalData.m_Real.m_Matrix.data();
	//return true;
}
void C_LocalAnimations::CleanSnapshots()
{
	*(float*)((DWORD)(g_ctx.local()) + 0x9B24) = 1.0f;
	*(float*)((DWORD)(g_ctx.local()) + 0xCF74) = 1.0f;
}
void C_LocalAnimations::ResetData()
{
	m_LocalData.m_nFlags = 0;
	m_LocalData.m_nSimulationTicks = 0;
	m_LocalData.m_flSpawnTime = 0.0f;
	m_LocalData.m_flYawDelta = 0.0f;
	m_LocalData.m_AnimRecords = { };
	m_LocalData.m_vecShootPosition = Vector(0, 0, 0);

	m_LocalData.m_Real.m_nMoveType = 0;
	m_LocalData.m_Real.m_nFlags = 0;
	m_LocalData.m_Real.m_Layers = { };
	m_LocalData.m_Real.m_PoseParameters = { };
	m_LocalData.m_Real.m_vecMatrixOrigin = Vector(0, 0, 0);
	m_LocalData.m_Real.m_Matrix = { };

	m_LocalData.m_Fake.m_nMoveType = 0;
	m_LocalData.m_Fake.m_nFlags = 0;
	m_LocalData.m_Fake.m_Layers = { };
	m_LocalData.m_Fake.m_CleanLayers = { };
	m_LocalData.m_Fake.m_PoseParameters = { };
	m_LocalData.m_Fake.m_vecMatrixOrigin = Vector(0, 0, 0);
	m_LocalData.m_Fake.m_Matrix = { };

	m_LocalData.m_Shoot.m_Matrix = { };
	m_LocalData.m_Shoot.m_Layers = { };
	m_LocalData.m_Shoot.m_PoseParameters = { };
}


void C_LocalAnimations::OnUPD_ClientSideAnims(player_t* local)
{
	float pose_parameter[24];
	std::memcpy(pose_parameter, g_ctx.local()->m_flPoseParameter().data(), 24 * sizeof(float));

	// interpolate our real matrix.
	for (int i = 0; i < 128; i++)
		m_LocalData.m_Real.m_Matrix.data()[i].SetOrigin(g_ctx.local()->GetAbsOrigin() - m_LocalData.m_Real.m_vecMatrixOrigin[i]);

	local->force_bone_rebuild();

	std::memcpy(g_ctx.local()->m_flPoseParameter().data(), m_LocalData.m_Real.m_PoseParameters.data(), 24 * sizeof(float));
	local->set_abs_angles(Vector(0, m_LocalData.m_flYawDelta, 0));
	std::memcpy(g_ctx.local()->get_animlayers(), m_LocalData.m_Shoot.m_Layers.data(), g_ctx.local()->animlayer_count() * sizeof(AnimationLayer));

	// set our cached bone to our matrix.
	std::memcpy(g_ctx.local()->m_CachedBoneData().Base(), m_LocalData.m_Real.m_Matrix.data(), g_ctx.local()->m_CachedBoneData().Count() * sizeof(matrix3x4_t));

	local->setup_bones_rebuilt(m_LocalData.m_Real.m_Matrix.data(), 0xFFF00);
	std::memcpy(g_ctx.local()->m_flPoseParameter().data(), pose_parameter, 24 * sizeof(float));
}