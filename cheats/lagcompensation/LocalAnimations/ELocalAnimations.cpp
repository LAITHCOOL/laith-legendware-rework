                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                #include "ELocalAnimations.h"
#include "../../cheats/tickbase shift/tickbase_shift.h"

void EC_LocalAnimations::Instance(CUserCmd* cmd)
{
	float_t flCurtime = m_globals()->m_curtime;
	float_t flRealTime = m_globals()->m_realtime;
	float_t flAbsFrameTime = m_globals()->m_absoluteframetime;
	float_t flFrameTime = m_globals()->m_frametime;
	float_t flInterpolationAmount = m_globals()->m_interpolation_amount;
	float_t iTickCount = m_globals()->m_tickcount;
	float_t iFrameCount = m_globals()->m_framecount;


	if (g_ctx.local()->m_flSpawnTime() != m_LocalData.m_flSpawnTime)
	{
		m_LocalData.m_iFlags[0] = m_LocalData.m_iFlags[1] = g_ctx.local()->m_fFlags();
		m_LocalData.m_iMoveType[0] = m_LocalData.m_iMoveType[1] = g_ctx.local()->GetMoveType();
		m_LocalData.m_flSpawnTime = g_ctx.local()->m_flSpawnTime();

		std::memcpy(&m_LocalData.m_FakeAnimationState, g_ctx.local()->GetAnimState(), sizeof(AnimState_s));
		std::memcpy(m_LocalData.m_FakeAnimationLayers.data(), g_ctx.local()->get_animlayers(), sizeof(AnimationLayer) * 13);
		std::memcpy(m_LocalData.m_FakePoseParameters.data(), g_ctx.local()->m_flPoseParameter().data(), sizeof(float_t) * 24);
	}

	std::memcpy(g_ctx.local()->get_animlayers(), GetAnimationLayers().data(), sizeof(AnimationLayer) * 13);

	if (!g_ctx.send_packet)
	{
		if (cmd->m_buttons & IN_ATTACK)
		{
			bool bIsRevolver = false;
			if (g_ctx.local()->m_hActiveWeapon().Get())
				bIsRevolver = g_ctx.local()->m_hActiveWeapon().Get()->m_iItemDefinitionIndex() == WEAPON_REVOLVER;

			auto check_fire = g_ctx.local()->m_hActiveWeapon().Get();

			if (g_ctx.globals.shifting_command_number != cmd->m_command_number || tickbase::get().double_tap_key)
			{
				if (check_fire->can_fire(bIsRevolver))
				{
					if (!g_ctx.send_packet)
					{
						m_LocalData.m_bDidShotAtChokeCycle = true;
						m_LocalData.m_angShotChokedAngle = cmd->m_viewangles;
					}
					
				}
			}
		}
	}

	int32_t iFlags = g_ctx.local()->m_fFlags();
	float_t flLowerBodyYaw = g_ctx.local()->m_flLowerBodyYawTarget();
	float_t flDuckSpeed = g_ctx.local()->m_flDuckSpeed();
	float_t flDuckAmount = g_ctx.local()->m_flDuckAmount();
	Vector angVisualAngles = g_ctx.local()->m_angVisualAngles();

	m_globals()->m_curtime = TICKS_TO_TIME(cmd->m_tickcount);
	m_globals()->m_realtime = TICKS_TO_TIME(cmd->m_tickcount);
	m_globals()->m_absoluteframetime = m_globals()->m_intervalpertick;
	m_globals()->m_frametime = m_globals()->m_intervalpertick;
	m_globals()->m_tickcount = cmd->m_tickcount;
	m_globals()->m_framecount = cmd->m_tickcount;
	m_globals()->m_interpolation_amount = 0.0f;

	g_ctx.local()->GetAnimState()->m_flPrimaryCycle = m_LocalData.m_AnimationLayers[ANIMATION_LAYER_MOVEMENT_MOVE].m_flCycle;
	g_ctx.local()->GetAnimState()->m_flMoveWeight = m_LocalData.m_AnimationLayers[ANIMATION_LAYER_MOVEMENT_MOVE].m_flWeight;
	g_ctx.local()->GetAnimState()->m_nStrafeSequence = m_LocalData.m_AnimationLayers[ANIMATION_LAYER_MOVEMENT_STRAFECHANGE].m_nSequence;
	g_ctx.local()->GetAnimState()->m_flStrafeChangeCycle = m_LocalData.m_AnimationLayers[ANIMATION_LAYER_MOVEMENT_STRAFECHANGE].m_flCycle;
	g_ctx.local()->GetAnimState()->m_flStrafeChangeWeight = m_LocalData.m_AnimationLayers[ANIMATION_LAYER_MOVEMENT_STRAFECHANGE].m_flWeight;
	g_ctx.local()->GetAnimState()->m_flAccelerationWeight = m_LocalData.m_AnimationLayers[ANIMATION_LAYER_LEAN].m_flWeight;

	g_ctx.local()->m_vecAbsVelocity() = g_ctx.local()->m_vecVelocity();
	g_ctx.local()->m_angVisualAngles() = cmd->m_viewangles;
	static auto weapon_recoil_scale = m_cvar()->FindVar(crypt_str("weapon_recoil_scale"));
	g_ctx.local()->m_flThirdpersonRecoil() = g_ctx.local()->m_aimPunchAngle().x * weapon_recoil_scale->GetFloat();

	if (m_LocalData.m_bDidShotAtChokeCycle)
		if (g_ctx.send_packet)
			g_ctx.local()->m_angVisualAngles() = m_LocalData.m_angShotChokedAngle;

	g_ctx.local()->m_angVisualAngles().z = 0.0f;
	g_ctx.local()->m_flLowerBodyYawTarget() = m_LocalData.m_flLowerBodyYaw;
	if (g_ctx.local()->m_fFlags() & FL_FROZEN || m_gamerules()->IsFreezeTime())
		g_ctx.local()->m_flLowerBodyYawTarget() = flLowerBodyYaw;

	if (g_ctx.local()->GetAnimState()->m_nLastUpdateFrame > m_globals()->m_framecount - 1)
		g_ctx.local()->GetAnimState()->m_nLastUpdateFrame = m_globals()->m_framecount - 1;

	this->DoAnimationEvent(0);
	for (int iLayer = 0; iLayer < 13; iLayer++)
		g_ctx.local()->get_animlayers()[iLayer].m_pOwner = g_ctx.local();

	bool bClientSideAnimation = g_ctx.local()->m_bClientSideAnimation();
	g_ctx.local()->m_bClientSideAnimation() = true;

	g_ctx.globals.updating_animation = true;
	g_ctx.local()->update_clientside_animation();
	g_ctx.globals.updating_animation = false;

	g_ctx.local()->m_bClientSideAnimation() = bClientSideAnimation;

	std::memcpy(m_LocalData.m_PoseParameters.data(), g_ctx.local()->m_flPoseParameter().data(), sizeof(float_t) * 24);
	std::memcpy(m_LocalData.m_AnimationLayers.data(), g_ctx.local()->get_animlayers(), sizeof(AnimationLayer) * 13);

	if (g_ctx.local()->GetAnimState()->m_flVelocityLengthXY > 0.1f || fabs(g_ctx.local()->GetAnimState()->m_flVelocityLengthZ) > 100.0f)
	{
		m_LocalData.m_flNextLowerBodyYawUpdateTime = flCurtime + 0.22f;
		if (m_LocalData.m_flLowerBodyYaw != math::normalize_yaw(cmd->m_viewangles.y))
			m_LocalData.m_flLowerBodyYaw = g_ctx.local()->m_flLowerBodyYawTarget() = math::normalize_yaw(g_ctx.local()->GetAnimState()->m_flEyeYaw);
	}
	else if (flCurtime > m_LocalData.m_flNextLowerBodyYawUpdateTime)
	{
		float_t flAngleDifference = math::AngleDiff(math::normalize_yaw(g_ctx.local()->GetAnimState()->m_flFootYaw), math::normalize_yaw(cmd->m_viewangles.y));
		if (fabsf(flAngleDifference) > 35.0f)
		{
			m_LocalData.m_flNextLowerBodyYawUpdateTime = flCurtime + 1.1f;
			if (m_LocalData.m_flLowerBodyYaw != math::normalize_yaw(cmd->m_viewangles.y))
				m_LocalData.m_flLowerBodyYaw = g_ctx.local()->m_flLowerBodyYawTarget() = math::normalize_yaw(cmd->m_viewangles.y);
		}
	}

	g_ctx.local()->m_fFlags() = iFlags;
	g_ctx.local()->m_flDuckAmount() = flDuckAmount;
	g_ctx.local()->m_flDuckSpeed() = flDuckSpeed;
	g_ctx.local()->m_flLowerBodyYawTarget() = flLowerBodyYaw;
	g_ctx.local()->m_angVisualAngles() = angVisualAngles;

	if (g_ctx.send_packet)
	{
		AnimState_s AnimationState;
		std::memcpy(&AnimationState, g_ctx.local()->GetAnimState(), sizeof(AnimState_s));

		bool bShouldSetupMatrix = true;
		if (tickbase::get().hide_shots_key)
			if (g_ctx.globals.shifting_command_number == cmd->m_command_number)
				bShouldSetupMatrix = false;

		if (bShouldSetupMatrix)
			g_ctx.local()->setup_bones_latest(m_LocalData.m_aMainBones.data(),false);
		//g_BoneManager->BuildMatrix(g_ctx.local(), m_LocalData.m_aMainBones.data(), false);
		// 
		// десинк лееры
		std::memcpy(g_ctx.local()->get_animlayers(), GetFakeAnimationLayers().data(), sizeof(AnimationLayer) * 13);
		std::memcpy(g_ctx.local()->GetAnimState(), &m_LocalData.m_FakeAnimationState, sizeof(AnimState_s));
		std::memcpy(g_ctx.local()->m_flPoseParameter().data(), m_LocalData.m_FakePoseParameters.data(), sizeof(float_t) * MAXSTUDIOPOSEPARAM);

		// выставляем данные с ласт апдейта
		g_ctx.local()->GetAnimState()->m_flPrimaryCycle = m_LocalData.m_FakeAnimationLayers[ANIMATION_LAYER_MOVEMENT_MOVE].m_flCycle;
		g_ctx.local()->GetAnimState()->m_flMoveWeight = m_LocalData.m_FakeAnimationLayers[ANIMATION_LAYER_MOVEMENT_MOVE].m_flWeight;
		g_ctx.local()->GetAnimState()->m_nStrafeSequence = m_LocalData.m_FakeAnimationLayers[ANIMATION_LAYER_MOVEMENT_STRAFECHANGE].m_nSequence;
		g_ctx.local()->GetAnimState()->m_flStrafeChangeCycle = m_LocalData.m_FakeAnimationLayers[ANIMATION_LAYER_MOVEMENT_STRAFECHANGE].m_flCycle;
		g_ctx.local()->GetAnimState()->m_flStrafeChangeWeight = m_LocalData.m_FakeAnimationLayers[ANIMATION_LAYER_MOVEMENT_STRAFECHANGE].m_flWeight;
		g_ctx.local()->GetAnimState()->m_flAccelerationWeight = m_LocalData.m_FakeAnimationLayers[ANIMATION_LAYER_LEAN].m_flWeight;

		// обновляем десинк
		int32_t iSimulationTicks = m_clientstate()->iChokedCommands + 1;
		for (int32_t iSimulationTick = 1; iSimulationTick <= iSimulationTicks; iSimulationTick++)
		{
			int32_t iTickCount = cmd->m_tickcount - (iSimulationTicks - iSimulationTick);
			m_globals()->m_curtime = TICKS_TO_TIME(iTickCount);
			m_globals()->m_realtime = TICKS_TO_TIME(iTickCount);
			m_globals()->m_absoluteframetime = m_globals()->m_intervalpertick;
			m_globals()->m_frametime = m_globals()->m_intervalpertick;
			m_globals()->m_tickcount = iTickCount;
			m_globals()->m_framecount = iTickCount;

			g_ctx.local()->m_vecAbsVelocity() = g_ctx.local()->m_vecVelocity();
			g_ctx.local()->m_flThirdpersonRecoil() = g_ctx.local()->m_aimPunchAngle().x * weapon_recoil_scale->GetFloat();

			g_ctx.local()->m_angVisualAngles() = m_angViewAngles;
			if ((iSimulationTicks - iSimulationTick) < 1)
			{
				if (m_LocalData.m_bDidShotAtChokeCycle)
					g_ctx.local()->m_angVisualAngles() = m_LocalData.m_angShotChokedAngle;

				g_ctx.local()->m_angVisualAngles().z = 0.0f;
			}

			if (g_ctx.local()->GetAnimState()->m_nLastUpdateFrame == m_globals()->m_frametime)
				g_ctx.local()->GetAnimState()->m_nLastUpdateFrame = m_globals()->m_frametime - 1;

			this->DoAnimationEvent(1);
			for (int iLayer = 0; iLayer < 13; iLayer++)
				g_ctx.local()->get_animlayers()[iLayer].m_pOwner = g_ctx.local();

			bool bClientSideAnimation = g_ctx.local()->m_bClientSideAnimation();
			g_ctx.local()->m_bClientSideAnimation() = true;

			g_ctx.globals.updating_animation = true;
			g_ctx.local()->update_clientside_animation();
			g_ctx.globals.updating_animation = false;

			g_ctx.local()->m_bClientSideAnimation() = bClientSideAnimation;
		}

		// build desync matrix
		//g_BoneManager->BuildMatrix(g_ctx.local(), m_LocalData.m_aDesyncBones.data(), false);
		g_ctx.local()->setup_bones_latest(m_LocalData.m_aDesyncBones.data(), false);

		// copy lag matrix
		std::memcpy(m_LocalData.m_aLagBones.data(), m_LocalData.m_aDesyncBones.data(), sizeof(matrix3x4_t) * MAXSTUDIOBONES);

		// сэйивим десинк лееры
		std::memcpy(&m_LocalData.m_FakeAnimationState, g_ctx.local()->GetAnimState(), sizeof(AnimState_s));
		std::memcpy(m_LocalData.m_FakeAnimationLayers.data(), g_ctx.local()->get_animlayers(), sizeof(AnimationLayer) * 13);
		std::memcpy(m_LocalData.m_FakePoseParameters.data(), g_ctx.local()->m_flPoseParameter().data(), sizeof(float_t) * 24);

		// ресторим лееры
		std::memcpy(g_ctx.local()->get_animlayers(), GetAnimationLayers().data(), sizeof(AnimationLayer) * 13);
		std::memcpy(g_ctx.local()->GetAnimState(), &AnimationState, sizeof(AnimState_s));
		std::memcpy(g_ctx.local()->m_flPoseParameter().data(), m_LocalData.m_PoseParameters.data(), sizeof(float_t) * 24);

		// муваем матрицы
		for (int i = 0; i < MAXSTUDIOBONES; i++)
			m_LocalData.m_vecBoneOrigins[i] = g_ctx.local()->GetAbsOrigin() - m_LocalData.m_aMainBones[i].GetOrigin();

		for (int i = 0; i < MAXSTUDIOBONES; i++)
			m_LocalData.m_vecFakeBoneOrigins[i] = g_ctx.local()->GetAbsOrigin() - m_LocalData.m_aDesyncBones[i].GetOrigin();

		// резет углов
		m_LocalData.m_bDidShotAtChokeCycle = false;
		m_LocalData.m_angShotChokedAngle = Vector(0, 0, 0);
	}

	g_ctx.local()->m_fFlags() = iFlags;
	g_ctx.local()->m_flDuckAmount() = flDuckAmount;
	g_ctx.local()->m_flDuckSpeed() = flDuckSpeed;
	g_ctx.local()->m_flLowerBodyYawTarget() = flLowerBodyYaw;
	g_ctx.local()->m_angVisualAngles() = angVisualAngles;

	// ресторим глобалсы
	m_globals()->m_curtime = flCurtime;
	m_globals()->m_realtime = flRealTime;
	m_globals()->m_absoluteframetime = flAbsFrameTime;
	m_globals()->m_frametime = flFrameTime;
	m_globals()->m_tickcount = iTickCount;
	m_globals()->m_framecount = iFrameCount;
	m_globals()->m_interpolation_amount = flInterpolationAmount;
}

void EC_LocalAnimations::SetupShootPosition(CUserCmd* cmd)
{
	float_t flOldBodyPitch = g_ctx.local()->m_flPoseParameter()[12];

	g_ctx.local()->m_flPoseParameter()[12] = (cmd->m_viewangles.x + 89.0f) / 178.0f;
	//g_BoneManager->BuildMatrix(g_ctx.local(), NULL, true);
	g_ctx.local()->setup_bones_latest(nullptr, true);
	g_ctx.local()->m_flPoseParameter()[12] = flOldBodyPitch;

	m_LocalData.m_vecShootPosition = g_ctx.local()->get_shoot_position();
}

bool EC_LocalAnimations::GetCachedMatrix(matrix3x4_t* aMatrix)
{
	std::memcpy(aMatrix, m_LocalData.m_aMainBones.data(), sizeof(matrix3x4_t) * g_ctx.local()->m_CachedBoneData().Count());
	return true;
}

std::array < matrix3x4_t, MAXSTUDIOBONES > EC_LocalAnimations::GetDesyncMatrix()
{
	return m_LocalData.m_aDesyncBones;
}

std::array < matrix3x4_t, MAXSTUDIOBONES > EC_LocalAnimations::GetLagMatrix()
{
	return m_LocalData.m_aLagBones;
}

void EC_LocalAnimations::DoAnimationEvent(int type)
{
	if (m_gamerules()->IsFreezeTime() || (g_ctx.local()->m_fFlags() & FL_FROZEN))
	{
		m_LocalData.m_iMoveType[type] = MOVETYPE_NONE;
		m_LocalData.m_iFlags[type] = FL_ONGROUND;
	}

	AnimationLayer* pLandOrClimbLayer = &g_ctx.local()->get_animlayers()[ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB];
	if (!pLandOrClimbLayer)
		return;

	AnimationLayer* pJumpOrFallLayer = &g_ctx.local()->get_animlayers()[ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL];
	if (!pJumpOrFallLayer)
		return;

	if (m_LocalData.m_iMoveType[type] != MOVETYPE_LADDER && g_ctx.local()->GetMoveType() == MOVETYPE_LADDER)
		g_ctx.local()->GetAnimState()->SetLayerSequence(pLandOrClimbLayer, ACT_CSGO_CLIMB_LADDER);
	else if (m_LocalData.m_iMoveType[type] == MOVETYPE_LADDER && g_ctx.local()->GetMoveType() != MOVETYPE_LADDER)
		g_ctx.local()->GetAnimState()->SetLayerSequence(pJumpOrFallLayer, ACT_CSGO_FALL);
	else
	{
		if (g_ctx.local()->m_fFlags() & FL_ONGROUND)
		{
			if (!(m_LocalData.m_iFlags[type] & FL_ONGROUND))
				g_ctx.local()->GetAnimState()->SetLayerSequence(pLandOrClimbLayer, g_ctx.local()->GetAnimState()->m_flDurationInAir > 1.0f && type == 0 ? ACT_CSGO_LAND_HEAVY : ACT_CSGO_LAND_LIGHT);
		}
		else if (m_LocalData.m_iFlags[type] & FL_ONGROUND)
		{
			if (g_ctx.local()->m_vecVelocity().z > 0.0f)
				g_ctx.local()->GetAnimState()->SetLayerSequence(pJumpOrFallLayer, ACT_CSGO_JUMP);
			else
				g_ctx.local()->GetAnimState()->SetLayerSequence(pJumpOrFallLayer, ACT_CSGO_FALL);
		}
	}

	m_LocalData.m_iMoveType[type] = g_ctx.local()->GetMoveType();
	m_LocalData.m_iFlags[type] = g_ctx.local()->m_fFlags();
}

void EC_LocalAnimations::OnUpdateClientSideAnimation()
{
	for (int i = 0; i < MAXSTUDIOBONES; i++)
		m_LocalData.m_aMainBones[i].SetOrigin(g_ctx.local()->GetAbsOrigin() - m_LocalData.m_vecBoneOrigins[i]);

	for (int i = 0; i < MAXSTUDIOBONES; i++)
		m_LocalData.m_aDesyncBones[i].SetOrigin(g_ctx.local()->GetAbsOrigin() - m_LocalData.m_vecFakeBoneOrigins[i]);

	std::memcpy(g_ctx.local()->m_CachedBoneData().Base(), m_LocalData.m_aMainBones.data(), sizeof(matrix3x4_t) * g_ctx.local()->m_CachedBoneData().Count());
	std::memcpy(g_ctx.local()->GetBoneAccessor().GetBoneArrayForWrite(), m_LocalData.m_aMainBones.data(), sizeof(matrix3x4_t) * g_ctx.local()->m_CachedBoneData().Count());

	return g_ctx.local()->SetupBones_AttachmentHelper();
}

std::array< AnimationLayer, 13 > EC_LocalAnimations::GetAnimationLayers()
{
	std::array< AnimationLayer, 13 > aOutput;

	std::memcpy(aOutput.data(), g_ctx.local()->get_animlayers(), sizeof(AnimationLayer) * 13);
	std::memcpy(&aOutput.at(ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL), &m_LocalData.m_AnimationLayers.at(ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL), sizeof(AnimationLayer));
	std::memcpy(&aOutput.at(ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB), &m_LocalData.m_AnimationLayers.at(ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB), sizeof(AnimationLayer));
	std::memcpy(&aOutput.at(ANIMATION_LAYER_ALIVELOOP), &m_LocalData.m_AnimationLayers.at(ANIMATION_LAYER_ALIVELOOP), sizeof(AnimationLayer));
	std::memcpy(&aOutput.at(ANIMATION_LAYER_LEAN), &m_LocalData.m_AnimationLayers.at(ANIMATION_LAYER_LEAN), sizeof(AnimationLayer));
	std::memcpy(&aOutput.at(ANIMATION_LAYER_MOVEMENT_MOVE), &m_LocalData.m_AnimationLayers.at(ANIMATION_LAYER_MOVEMENT_MOVE), sizeof(AnimationLayer));
	std::memcpy(&aOutput.at(ANIMATION_LAYER_MOVEMENT_STRAFECHANGE), &m_LocalData.m_AnimationLayers.at(ANIMATION_LAYER_MOVEMENT_STRAFECHANGE), sizeof(AnimationLayer));

	return aOutput;
}

std::array< AnimationLayer, 13 > EC_LocalAnimations::GetFakeAnimationLayers()
{
	std::array< AnimationLayer, 13 > aOutput;

	std::memcpy(aOutput.data(), g_ctx.local()->get_animlayers(), sizeof(AnimationLayer) * 13);
	std::memcpy(&aOutput.at(ANIMATION_LAYER_MOVEMENT_MOVE), &m_LocalData.m_FakeAnimationLayers.at(ANIMATION_LAYER_MOVEMENT_MOVE), sizeof(AnimationLayer));
	std::memcpy(&aOutput.at(ANIMATION_LAYER_MOVEMENT_STRAFECHANGE), &m_LocalData.m_FakeAnimationLayers.at(ANIMATION_LAYER_MOVEMENT_STRAFECHANGE), sizeof(AnimationLayer));
	std::memcpy(&aOutput.at(ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL), &m_LocalData.m_FakeAnimationLayers.at(ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL), sizeof(AnimationLayer));
	std::memcpy(&aOutput.at(ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB), &m_LocalData.m_FakeAnimationLayers.at(ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB), sizeof(AnimationLayer));
	std::memcpy(&aOutput.at(ANIMATION_LAYER_ALIVELOOP), &m_LocalData.m_FakeAnimationLayers.at(ANIMATION_LAYER_ALIVELOOP), sizeof(AnimationLayer));
	std::memcpy(&aOutput.at(ANIMATION_LAYER_LEAN), &m_LocalData.m_FakeAnimationLayers.at(ANIMATION_LAYER_LEAN), sizeof(AnimationLayer));

	return aOutput;
}

void EC_LocalAnimations::ResetData()
{
	m_LocalData.m_aDesyncBones = { };
	m_LocalData.m_aMainBones = { };

	m_LocalData.m_vecNetworkedOrigin = Vector(0, 0, 0);
	m_LocalData.m_angShotChokedAngle = Vector(0, 0, 0);
	m_LocalData.m_vecBoneOrigins.fill(Vector(0, 0, 0));
	m_LocalData.m_vecFakeBoneOrigins.fill(Vector(0, 0, 0));

	m_LocalData.m_bDidShotAtChokeCycle = false;

	m_LocalData.m_AnimationLayers.fill(AnimationLayer());
	m_LocalData.m_FakeAnimationLayers.fill(AnimationLayer());

	m_LocalData.m_PoseParameters.fill(0.0f);
	m_LocalData.m_FakePoseParameters.fill(0.0f);

	m_LocalData.m_flShotTime = 0.0f;
	m_LocalData.m_angForcedAngles = Vector(0, 0, 0);

	m_LocalData.m_flLowerBodyYaw = 0.0f;
	m_LocalData.m_flNextLowerBodyYawUpdateTime = 0.0f;
	m_LocalData.m_flSpawnTime = 0.0f;

	m_LocalData.m_iFlags[0] = m_LocalData.m_iFlags[0] = 0;
	m_LocalData.m_iMoveType[0] = m_LocalData.m_iMoveType[1] = 0;
}                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   