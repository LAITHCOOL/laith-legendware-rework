// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: http://www.viva64.com

#include "local_animations.h"
#include "..\misc\misc.h"
#include "..\misc\logs.h"
#include "..\misc\prediction_system.h"

void local_animations::run(CUserCmd* m_pcmd)
{
	// sanity checks, not more and not much.
	if (!g_ctx.local()->is_alive() || !g_ctx.local()->get_animation_state1())
		return;

	// this code is important!!! ( for me ).
	// fix for animations.

	// backup thirdperson recoil.
	const auto backup_thirdperson_recoil = g_ctx.local()->m_flThirdpersonRecoil();

	run_animations(m_pcmd);

	// set our thirdperson recoil to scaled aimpunchangle.
	g_ctx.local()->m_flThirdpersonRecoil() = g_ctx.local()->m_aimPunchAngleScaled().x;

	// set our data to the unmodified one.
	g_ctx.local()->m_flThirdpersonRecoil() = backup_thirdperson_recoil;
}

void local_animations::run_animations(CUserCmd* cmd)
{
	const auto movestate = g_ctx.local()->m_iMoveState();
	const auto iswalking = g_ctx.local()->m_bIsWalking();
	auto data = &anim_data[(cmd->m_command_number - 1) % MULTIPLAYER_BACKUP];

	g_ctx.local()->m_iMoveState() = 0;
	g_ctx.local()->m_bIsWalking() = false;

	auto m_forward = cmd->m_buttons & IN_FORWARD;
	auto m_back = cmd->m_buttons & IN_BACK;
	auto m_right = cmd->m_buttons & IN_MOVERIGHT;
	auto m_left = cmd->m_buttons & IN_MOVELEFT;
	auto m_walking = cmd->m_buttons & IN_SPEED;

	bool m_walk_state = m_walking ? true : false;

	if (cmd->m_buttons & IN_DUCK || g_ctx.local()->m_bDucking() || g_ctx.local()->m_fFlags() & FL_DUCKING)
		m_walk_state = false;
	else if (m_walking)
	{
		float m_max_speed = g_ctx.local()->m_flMaxSpeed() * 0.52f;

		if (m_max_speed + 25.f > g_ctx.local()->m_vecVelocity().Length())
			g_ctx.local()->m_bIsWalking() = true;
	}

	auto move_buttons_pressed = cmd->m_buttons & (IN_FORWARD | IN_BACK | IN_MOVELEFT | IN_MOVERIGHT | IN_RUN);

	bool holding_forward_and_back;
	bool holding_right_and_left;

	if (!m_forward)
		holding_forward_and_back = false;
	else
		holding_forward_and_back = m_back;

	if (!m_right)
		holding_right_and_left = false;
	else
		holding_right_and_left = m_left;

	if (move_buttons_pressed)
	{
		if (holding_forward_and_back)
		{
			if (holding_right_and_left)
				g_ctx.local()->m_iMoveState() = 0;
			else if (m_right || m_left)
				g_ctx.local()->m_iMoveState() = 2;
			else
				g_ctx.local()->m_iMoveState() = 0;
		}
		else
		{
			if (holding_forward_and_back)
				g_ctx.local()->m_iMoveState() = 0;
			else if (m_back || m_forward)
				g_ctx.local()->m_iMoveState() = 2;
			else
				g_ctx.local()->m_iMoveState() = 0;
		}
	}

	if (g_ctx.local()->m_iMoveState() == 2 && m_walk_state)
		g_ctx.local()->m_iMoveState() = 1;


	// updates animation-related for localplayer.
	update_local_animations_2(data);
	update_fake_animations(data);

	g_ctx.local()->m_iMoveState() = movestate;
	g_ctx.local()->m_bIsWalking() = iswalking;
}

void local_animations::do_animation_event()
{
	if (antiaim::get().freeze_check)
	{
		m_movetype = MOVETYPE_NONE;
		m_flags = FL_ONGROUND;
	}

	AnimationLayer* pLandOrClimbLayer = &g_ctx.local()->get_animlayers()[ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB];
	if (!pLandOrClimbLayer)
		return;

	AnimationLayer* pJumpOrFallLayer = &g_ctx.local()->get_animlayers()[ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL];
	if (!pJumpOrFallLayer)
		return;

	if (m_movetype != MOVETYPE_LADDER && g_ctx.local()->get_move_type() == MOVETYPE_LADDER)
		g_ctx.local()->get_animation_state1()->SetLayerSequence(pLandOrClimbLayer, ACT_CSGO_CLIMB_LADDER);
	else if (m_movetype == MOVETYPE_LADDER && g_ctx.local()->get_move_type() != MOVETYPE_LADDER)
		g_ctx.local()->get_animation_state1()->SetLayerSequence(pJumpOrFallLayer, ACT_CSGO_FALL);
	else
	{
		if (g_ctx.local()->m_fFlags() & FL_ONGROUND)
		{
			if (!(m_flags & FL_ONGROUND))
				g_ctx.local()->get_animation_state1()->SetLayerSequence(pLandOrClimbLayer, g_ctx.local()->get_animation_state1()->m_flInAirTime() > 1.0f ? ACT_CSGO_LAND_HEAVY : ACT_CSGO_LAND_LIGHT);
		}
		else if (m_flags & FL_ONGROUND)
		{
			if (g_ctx.local()->m_vecVelocity().z > 0.0f)
				g_ctx.local()->get_animation_state1()->SetLayerSequence(pJumpOrFallLayer, ACT_CSGO_JUMP);
			else
				g_ctx.local()->get_animation_state1()->SetLayerSequence(pJumpOrFallLayer, ACT_CSGO_FALL);
		}
	}

	m_movetype = g_ctx.local()->get_move_type();
	m_flags = g_ctx.local()->m_fFlags();
}

std::array< AnimationLayer, 13 > local_animations::get_animlayers()
{
	std::array< AnimationLayer, 13 > aOutput;

	g_ctx.local()->copy_animlayers(aOutput.data());
	std::memcpy(&aOutput.at(ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL), &m_real_layers[ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL], sizeof(AnimationLayer));
	std::memcpy(&aOutput.at(ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB), &m_real_layers[ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB], sizeof(AnimationLayer));
	std::memcpy(&aOutput.at(ANIMATION_LAYER_ALIVELOOP), &m_real_layers[ANIMATION_LAYER_ALIVELOOP], sizeof(AnimationLayer));
	std::memcpy(&aOutput.at(ANIMATION_LAYER_LEAN), &m_real_layers[ANIMATION_LAYER_LEAN], sizeof(AnimationLayer));
	std::memcpy(&aOutput.at(ANIMATION_LAYER_MOVEMENT_MOVE), &m_real_layers[ANIMATION_LAYER_MOVEMENT_MOVE], sizeof(AnimationLayer));
	std::memcpy(&aOutput.at(ANIMATION_LAYER_MOVEMENT_STRAFECHANGE), &m_real_layers[ANIMATION_LAYER_MOVEMENT_STRAFECHANGE], sizeof(AnimationLayer));

	return aOutput;
}

void local_animations::store_animations_data(CUserCmd* cmd, bool& send_packet)
{
	if (!cmd)
		return;

	// data to be used in animations.
	auto data = &anim_data[(cmd->m_command_number % MULTIPLAYER_BACKUP)];

	// variety of command-related saved data.
	data->send_packet = send_packet;
	data->m_command_number = cmd->m_command_number;
	data->m_viewangles = cmd->m_viewangles;
	data->m_nTickBase = g_ctx.globals.fixed_tickbase;
}

void local_animations::update_fake_animations(Local_data::Anim_data* data)
{
	// data used for animation.
	bool allocate = (!local_data.animstate || local_data.animstate->m_pBaseEntity != g_ctx.local()),
		change = (!allocate) && (g_ctx.local()->GetRefEHandle().ToLong() != m_fake_handle),
		reset = (!allocate && !change) && (g_ctx.local()->m_flSpawnTime() != m_fake_spawntime);

	// if the animstate didn't exist and the handle is not our entity then we free the animstate.
	if (change)
		local_data.animstate = nullptr;

	// reset if animstate is not allocated or something is changed or our spawntime is changed.
	if (reset)
	{
		allocate = true;
		m_fake_spawntime = g_ctx.local()->m_flSpawnTime();
	}

	// if we already allocate it then we set the data for animation.
	if (allocate)
	{
		auto* state = reinterpret_cast<C_CSGOPlayerAnimationState*>(m_memalloc()->Alloc(sizeof(C_CSGOPlayerAnimationState)));

		if (state)
			util::create_state1(state, g_ctx.local());

		m_fake_handle = g_ctx.local()->GetRefEHandle().ToLong();
		m_fake_spawntime = g_ctx.local()->m_flSpawnTime();

		local_data.animstate = state;
	}

	// code for viewangles fix in the future.
	static bool lock_viewangles = false;
	static Vector target_angle = data->m_viewangles;
	static Vector non_shot_target_angle = data->m_viewangles;

	// did we shoot at this command?.
	if (g_ctx.globals.shot_command == data->m_command_number)
	{
		lock_viewangles = true;
		target_angle = data->m_viewangles;
	}

	// we not shooting at this command :(.
	if (!lock_viewangles)
		target_angle = data->m_viewangles;

	// invalidate prior animations.
	if (local_data.animstate->m_iLastClientSideAnimationUpdateFramecount == m_globals()->m_framecount) {
		local_data.animstate->m_iLastClientSideAnimationUpdateFramecount = m_globals()->m_framecount - 1;
	}

	// same as above.
	if (local_data.animstate->m_flLastClientSideAnimationUpdateTime == m_globals()->m_curtime) {
		local_data.animstate->m_flLastClientSideAnimationUpdateTime = m_globals()->m_curtime - m_globals()->m_intervalpertick;
	}

	// if everything is fine and we are choking packet then run our desync animfix code.
	if (data->send_packet)
	{
		// copy our pose parameter like a real man.
		AnimationLayer layers[13], fake_layers[13];
		float pose_parameter[24], fake_pose_parameter[24];

		// copy our animation data like a strong man.
		g_ctx.local()->copy_animlayers(layers);
		g_ctx.local()->copy_poseparameter(pose_parameter);

		// if our animstate owner is us then we need to change the base entity to our player.
		local_data.animstate->m_pBaseEntity = g_ctx.local();

		// update our animstate to make it shows fake angles.
		util::update_state1(local_data.animstate, target_angle);

		g_ctx.local()->copy_animlayers(fake_layers);
		g_ctx.local()->copy_poseparameter(fake_pose_parameter);

		local_data.animstate->m_bInHitGroundAnimation = false;
		local_data.animstate->m_fLandingDuckAdditiveSomething = 0.0f;
		local_data.animstate->m_flHeadHeightOrOffsetFromHittingGroundAnimation = 1.0f;

		g_ctx.local()->set_poseparameter(fake_pose_parameter);
		g_ctx.local()->set_animlayers(fake_layers);

		// now we setup the bones after everything is done.
		g_ctx.local()->setup_bones_local(g_ctx.globals.fake_matrix, MAXSTUDIOBONES, BONE_USED_BY_ANYTHING, m_globals()->m_curtime);

		// do we activate visualize lag?.
		local_data.visualize_lag = g_cfg.player.visualize_lag;

		// if we not activate it yet we interpolate our fake matrix.
		if (!local_data.visualize_lag)
		{
			// loop through all of the arbitrary.
			for (auto& i : g_ctx.globals.fake_matrix)
			{
				i[0][3] -= g_ctx.local()->GetRenderOrigin().x; //-V807
				i[1][3] -= g_ctx.local()->GetRenderOrigin().y;
				i[2][3] -= g_ctx.local()->GetRenderOrigin().z;
			}
		}

		// set our poseparameter and our animation layer.
		g_ctx.local()->set_poseparameter(pose_parameter);
		g_ctx.local()->set_animlayers(layers);

		// did the we passed the choke packet and run the entire thing? Then we reset our locked viewangles.
		lock_viewangles = false;
	}
}
#include "../tickbase shift/tickbase_shift.h"
void local_animations::update_local_animations(Local_data::Anim_data* data)
{
	// code for viewangles fix in the future.
	static bool lock_viewangles = false;
	static Vector target_angle = data->m_viewangles;
	static Vector non_shot_target_angle = data->m_viewangles;

	// did we shoot at this command?.
	if (g_ctx.globals.shot_command == data->m_command_number)
	{
		lock_viewangles = true;
		target_angle = data->m_viewangles;
	}

	// we not shooting at this command :(.
	if (!lock_viewangles)
		target_angle = data->m_viewangles;

	// if our spawntime didn't match our entity spawntime or the entity of the animstate holder is not us?.
	if (g_ctx.local()->m_flSpawnTime() != m_real_spawntime || g_ctx.local()->get_animation_state1()->m_pBaseEntity != g_ctx.local())
	{
		g_ctx.local()->copy_poseparameter(m_real_poses);
		g_ctx.local()->copy_animlayers(m_real_layers);

		m_movetype = g_ctx.local()->get_move_type();
		m_flags = g_ctx.local()->m_fFlags();

		// set our spawntime to our entity spawntime.
		m_real_spawntime = g_ctx.local()->m_flSpawnTime();
	}

	g_ctx.local()->set_animlayers(get_animlayers().data());

	// thing for restore later.
	const int iPreviousFramecount = m_globals()->m_framecount;
	const float flPreviousRealtime = m_globals()->m_realtime;
	const float flPreviousCurtime = m_globals()->m_curtime;
	const float flPreviousFrametime = m_globals()->m_frametime;
	const float flPreviousAbsoluteFrametime = m_globals()->m_absoluteframetime;
	const float flPreviousInterpolationAmount = m_globals()->m_interpolation_amount;
	const int iPreviousTickCount = m_globals()->m_tickcount;

	const float flTime = TICKS_TO_TIME(g_ctx.globals.fixed_tickbase);
	const float flThiccTime = (flTime / m_globals()->m_intervalpertick) + .5f;

	int iFlags = g_ctx.local()->m_fFlags();
	float flLowerBodyYaw = g_ctx.local()->m_flLowerBodyYawTarget();
	float flDuckSpeed = g_ctx.local()->m_flDuckSpeed();
	float flDuckAmount = g_ctx.local()->m_flDuckAmount();

	// match our global variables with local variables.
	m_globals()->m_realtime = flTime;
	m_globals()->m_curtime = flTime;
	m_globals()->m_frametime = m_globals()->m_intervalpertick;
	m_globals()->m_absoluteframetime = m_globals()->m_intervalpertick;
	m_globals()->m_framecount = flThiccTime;
	m_globals()->m_tickcount = flThiccTime;
	m_globals()->m_interpolation_amount = 0.f;

	if (!data->send_packet)
	{

		// layers fix.
		g_ctx.local()->get_animation_state1()->m_flFeetCycle = m_real_layers[ANIMATION_LAYER_MOVEMENT_MOVE].m_flCycle;
		g_ctx.local()->get_animation_state1()->m_flFeetYawRate = m_real_layers[ANIMATION_LAYER_MOVEMENT_MOVE].m_flWeight;
		g_ctx.local()->get_animation_state1()->m_flStrafeChangeCycle = m_real_layers[ANIMATION_LAYER_MOVEMENT_STRAFECHANGE].m_flCycle;
		g_ctx.local()->get_animation_state1()->m_iStrafeSequence = m_real_layers[ANIMATION_LAYER_MOVEMENT_STRAFECHANGE].m_nSequence;
		g_ctx.local()->get_animation_state1()->m_flStrafeChangeWeight = m_real_layers[ANIMATION_LAYER_MOVEMENT_STRAFECHANGE].m_flWeight;
		g_ctx.local()->get_animation_state1()->m_flAccelerationWeight = m_real_layers[ANIMATION_LAYER_LEAN].m_flWeight;

		// setup our feetyawrate.
		g_ctx.local()->get_animation_state1()->m_flFeetYawRate = 0;

		// set our render angles to our viewangles and to fixed viewangles later ( hide shots exploit and double tap exploit so it's a compensate to make our animfix shows hide shot ).
		g_ctx.local()->m_vecAbsVelocity() = g_ctx.local()->m_vecVelocity();
		g_ctx.local()->get_render_angles1() = (((tickbase::get().hide_shots_key) && !g_ctx.globals.fakeducking && lock_viewangles) ? non_shot_target_angle : target_angle);

		// allows re-animating at the same tick.
		if (g_ctx.local()->get_animation_state1()->m_iLastClientSideAnimationUpdateFramecount == m_globals()->m_framecount)
			g_ctx.local()->get_animation_state1()->m_iLastClientSideAnimationUpdateFramecount = m_globals()->m_framecount - 1;

		// same as above.
		if (g_ctx.local()->get_animation_state1()->m_flLastClientSideAnimationUpdateTime == m_globals()->m_curtime)
			g_ctx.local()->get_animation_state1()->m_flLastClientSideAnimationUpdateTime = m_globals()->m_curtime - m_globals()->m_intervalpertick;

		do_animation_event();
		for (int iLayer = 0; iLayer < 13; iLayer++)
			g_ctx.local()->get_animlayers()[iLayer].m_pOwner = g_ctx.local();

		// update our clientside animations.
		const auto bClientSideAnimation = g_ctx.local()->m_bClientSideAnimation();

		g_ctx.local()->m_bClientSideAnimation() = true;

		g_ctx.globals.m_updating_real_animations = true;
		g_ctx.local()->update_clientside_animation();
		g_ctx.globals.m_updating_real_animations = false;

		g_ctx.local()->m_bClientSideAnimation() = bClientSideAnimation;

		g_ctx.local()->copy_poseparameter(m_real_poses);
		g_ctx.local()->copy_animlayers(m_real_layers);

		g_ctx.local()->m_fFlags() = iFlags;
		g_ctx.local()->m_flLowerBodyYawTarget() = flLowerBodyYaw;
		g_ctx.local()->m_flDuckSpeed() = flDuckSpeed;
		g_ctx.local()->m_flDuckAmount() = flDuckAmount;


		// if we not lock our viewangles then we set our non_shot_target_angle to current viewangles to make our localplayer shows accurate shootpos.
		if (!lock_viewangles)
			non_shot_target_angle = data->m_viewangles;

		// save off real data.
		local_data.real_animstate = g_ctx.local()->get_animation_state1();
		m_real_angles = g_ctx.local()->get_animation_state1()->m_flGoalFeetYaw;

		// rebuilt bones and interpolate our origin with our matrix origin..
		g_ctx.globals.m_real_matrix_ret = g_ctx.local()->setup_bones_local(g_ctx.globals.m_real_matrix, 128, BONE_USED_BY_ANYTHING, 0);

		g_ctx.local()->set_animlayers(get_animlayers().data());
		g_ctx.local()->set_animation_state(local_data.real_animstate);
		g_ctx.local()->set_poseparameter(m_real_poses);

		for (int i = 0; i < 128; i++)
			g_ctx.globals.local_origin[i] = g_ctx.local()->GetAbsOrigin() - g_ctx.globals.m_real_matrix[i].GetOrigin();

		// we passed through all of it now we don't need to lock viewangles for future.
		lock_viewangles = false;
	}
	

	m_globals()->m_realtime = flPreviousRealtime;
	m_globals()->m_curtime = flPreviousCurtime;
	m_globals()->m_frametime = flPreviousFrametime;
	m_globals()->m_absoluteframetime = flPreviousAbsoluteFrametime;
	m_globals()->m_interpolation_amount = flPreviousInterpolationAmount;
	m_globals()->m_framecount = iPreviousFramecount;
	m_globals()->m_tickcount = iPreviousTickCount;
}

inline float anglemod1(float a)
{
	a = (360.f / 65536) * ((int)(a * (65536.f / 360.0f)) & 65535);
	return a;
}

float ApproachAngle1(float target, float value, float speed)
{
	target = anglemod1(target);
	value = anglemod1(value);

	float delta = target - value;

	if (speed < 0)
		speed = -speed;

	if (delta < -180)
		delta += 360;
	else if (delta > 180)
		delta -= 360;

	if (delta > speed)
		value += speed;
	else if (delta < -speed)
		value -= speed;
	else
		value = target;

	return value;
}

void local_animations::update_local_animations_2(Local_data::Anim_data* local_data)
{
	static bool lock_viewangles = false;
	static Vector target_angle = local_data->m_viewangles;
	static Vector non_shot_target_angle = local_data->m_viewangles;

	g_ctx.local()->get_animlayers()[3].m_flWeight = 0.0f;
	g_ctx.local()->get_animlayers()[3].m_flCycle = 0.0f;

	if (g_ctx.globals.shot_command == local_data->m_command_number)
	{
		lock_viewangles = true;
		target_angle = local_data->m_viewangles;
	}

	if (!lock_viewangles)
		target_angle = local_data->m_viewangles;

	auto animstate = g_ctx.local()->get_animation_state1();

	if (!animstate || !g_ctx.local()->is_alive() || g_ctx.local()->IsDormant())
	{
		m_real_spawntime = 0;
		return;
	}

	g_ctx.local()->m_flThirdpersonRecoil() = g_ctx.local()->m_aimPunchAngleScaled().x;

	AnimationLayer old_anim_layers[13];
	std::memcpy(old_anim_layers, g_ctx.local()->get_animlayers(), g_ctx.local()->animlayer_count() * sizeof(AnimationLayer));

	if (g_ctx.local()->m_flSpawnTime() != m_real_spawntime || animstate->m_pBaseEntity != g_ctx.local())
	{
		g_ctx.globals.updating_animation = true;
		g_ctx.local()->update_clientside_animation();
		g_ctx.globals.updating_animation = false;

		std::memcpy(g_ctx.local()->get_animlayers(), old_anim_layers, g_ctx.local()->animlayer_count() * sizeof(AnimationLayer));
		m_real_spawntime = g_ctx.local()->m_flSpawnTime();
	}


	// thing for restore later.
	const int iPreviousFramecount = m_globals()->m_framecount;
	const float flPreviousRealtime = m_globals()->m_realtime;
	const float flPreviousCurtime = m_globals()->m_curtime;
	const float flPreviousFrametime = m_globals()->m_frametime;
	const float flPreviousAbsoluteFrametime = m_globals()->m_absoluteframetime;
	const float flPreviousInterpolationAmount = m_globals()->m_interpolation_amount;
	const int iPreviousTickCount = m_globals()->m_tickcount;



	const auto old_render_angles = g_ctx.local()->get_render_angles1();
	const auto old_pose_params = g_ctx.local()->m_flPoseParameter();




	animstate->m_flUpdateTimeDelta = fmaxf(m_globals()->m_curtime - animstate->m_flLastClientSideAnimationUpdateTime, 0.f);

	g_ctx.local()->get_render_angles1() = ((tickbase::get().hide_shots_key) && !g_ctx.globals.fakeducking && lock_viewangles) ? non_shot_target_angle : target_angle;

	if (animstate->m_pActiveWeapon != animstate->m_pLastActiveWeapon)
	{
		for (int i = 0; i < 13; i++)
		{
			AnimationLayer pLayer = g_ctx.local()->get_animlayers()[i];

			pLayer.m_pStudioHdr = NULL;
			pLayer.m_nDispatchSequence = -1;
			pLayer.m_nDispatchSequence_2 = -1;
		}
	}

	float flNewDuckAmount;
	if (animstate)
	{
		flNewDuckAmount = math::clamp(g_ctx.local()->m_flDuckAmount() + animstate->m_fLandingDuckAdditiveSomething, 0.0f, 1.0f);
		flNewDuckAmount = ApproachAngle1(flNewDuckAmount, animstate->m_fDuckAmount, animstate->m_flUpdateTimeDelta * 6.0f);
		flNewDuckAmount = math::clamp(flNewDuckAmount, 0.0f, 1.0f);
	}
	animstate->m_fDuckAmount = flNewDuckAmount;


	const float flTime = TICKS_TO_TIME(g_ctx.globals.fixed_tickbase);
	const float flThiccTime = (flTime / m_globals()->m_intervalpertick) + .5f;
	// match our global variables with local variables.
	m_globals()->m_realtime = flTime;
	m_globals()->m_curtime = flTime;
	m_globals()->m_frametime = m_globals()->m_intervalpertick;
	m_globals()->m_absoluteframetime = m_globals()->m_intervalpertick;
	m_globals()->m_framecount = flThiccTime;
	m_globals()->m_tickcount = flThiccTime;
	m_globals()->m_interpolation_amount = 0.f;


	g_ctx.local()->get_animlayers()->m_nSequence = 0;
	g_ctx.local()->get_animlayers()->m_flPlaybackRate = 0.0f;
	g_ctx.local()->get_animlayers()->m_flCycle = 0.0f;

	if (g_ctx.local()->get_animlayers()->m_flCycle != 0.0f)
	{
		g_ctx.local()->get_animlayers()->m_flCycle = 0.0f;
		g_ctx.local()->invalidate_physics_recursive(ANIMATION_CHANGED);
	}

	animstate->m_flFeetYawRate = 0;

	g_ctx.globals.updating_animation = true;
	g_ctx.local()->update_clientside_animation();
	g_ctx.globals.updating_animation = false;

	g_ctx.local()->force_bone_rebuild();
	//g_ctx.local()->setup_bones_local(g_ctx.globals.m_real_matrix, 128, BONE_USED_BY_ANYTHING, 0);
	//g_ctx.local()->BuildLocalBones();

	g_ctx.local()->setup_bones_rebuilt(g_ctx.globals.m_real_matrix, 0xFFF00);

	g_ctx.local()->get_animation_state1()->m_flFeetCycle = old_anim_layers[6].m_flCycle;
	g_ctx.local()->get_animation_state1()->m_flFeetYawRate = old_anim_layers[6].m_flWeight;
	g_ctx.local()->get_animation_state1()->m_flStrafeChangeCycle = old_anim_layers[7].m_flCycle;
	g_ctx.local()->get_animation_state1()->m_iStrafeSequence = old_anim_layers[7].m_nSequence;
	g_ctx.local()->get_animation_state1()->m_flStrafeChangeWeight = old_anim_layers[7].m_flWeight;
	g_ctx.local()->get_animation_state1()->m_flAccelerationWeight = old_anim_layers[12].m_flWeight;

	for (int i = 0; i < 13; ++i)
	{
		AnimationLayer layer = g_ctx.local()->get_animlayers()[i];
		if (!layer.m_nSequence && layer.m_pOwner && layer.m_flWeight != 0.0f)
		{
			((player_t*)layer.m_pOwner)->invalidate_physics_recursive(BOUNDS_CHANGED);
			layer.m_flWeight = 0.0f;
		}
	}

	if (local_data->send_packet == false)
	{
		std::memcpy(g_ctx.local()->get_animlayers(), old_anim_layers, g_ctx.local()->animlayer_count() * sizeof(AnimationLayer));
		return;
	}

	if (!lock_viewangles)
		non_shot_target_angle = local_data->m_viewangles;

	memcpy(m_real_poses, g_ctx.local()->m_flPoseParameter().data(), 24 * sizeof(float));
	m_real_angles = g_ctx.local()->get_animation_state1()->m_flGoalFeetYaw;
	memcpy(m_real_layers, g_ctx.local()->get_animlayers(), g_ctx.local()->animlayer_count() * sizeof(AnimationLayer));

	g_ctx.local()->m_flPoseParameter() = old_pose_params;
	memcpy(g_ctx.local()->get_animlayers(), old_anim_layers, g_ctx.local()->animlayer_count() * sizeof(AnimationLayer));
	g_ctx.local()->get_render_angles1() = old_render_angles;


	for (int i = 0; i < 128; i++)
		g_ctx.globals.local_origin[i] = g_ctx.local()->GetAbsOrigin() - g_ctx.globals.m_real_matrix[i].GetOrigin();

	lock_viewangles = false;


	m_globals()->m_realtime = flPreviousRealtime;
	m_globals()->m_curtime = flPreviousCurtime;
	m_globals()->m_frametime = flPreviousFrametime;
	m_globals()->m_absoluteframetime = flPreviousAbsoluteFrametime;
	m_globals()->m_interpolation_amount = flPreviousInterpolationAmount;
	m_globals()->m_framecount = iPreviousFramecount;
	m_globals()->m_tickcount = iPreviousTickCount;

}

void local_animations::setup_shoot_position(CUserCmd* m_pcmd)
{
	float flOldBodyPitch = g_ctx.local()->m_flPoseParameter()[12];

	g_ctx.local()->m_flPoseParameter()[12] = (m_pcmd->m_viewangles.x + 89.0f) / 178.0f;
	g_ctx.local()->setup_bones_local(nullptr, -1, BONE_USED_BY_HITBOX, m_globals()->m_curtime);
	g_ctx.local()->m_flPoseParameter()[12] = flOldBodyPitch;

	g_ctx.globals.eye_pos = g_ctx.local()->get_shoot_position();
}

void local_animations::build_local_bones(player_t* local)
{
	// backup for our data.
	const auto backup_bones = g_ctx.local()->m_BoneAccessor().m_pBones;

	// interpolate our real matrix.
	for (int i = 0; i < 128; i++)
		g_ctx.globals.m_real_matrix[i].SetOrigin(g_ctx.local()->GetAbsOrigin() - g_ctx.globals.local_origin[i]);

	local->force_bone_rebuild();

	std::memcpy(g_ctx.local()->m_flPoseParameter().data(), m_real_poses, 24 * sizeof(float));
	local->set_abs_angles(Vector(0, m_real_angles, 0));
	std::memcpy(g_ctx.local()->get_animlayers(), m_real_layers, g_ctx.local()->animlayer_count() * sizeof(AnimationLayer));

	// set our cached bone to our matrix.
	std::memcpy(g_ctx.local()->m_CachedBoneData().Base(), g_ctx.globals.m_real_matrix, g_ctx.local()->m_CachedBoneData().Count() * sizeof(matrix3x4_t));

	// set our accessor bones to our matrix.
	g_ctx.local()->m_BoneAccessor().m_pBones = g_ctx.globals.m_real_matrix;
	g_ctx.local()->attachment_helper();
	g_ctx.local()->m_BoneAccessor().m_pBones = backup_bones;
}


void local_animations::OnUPD_ClientSideAnims(player_t* local)
{
	float pose_parameter[24];
	std::memcpy(pose_parameter, g_ctx.local()->m_flPoseParameter().data(), 24 * sizeof(float));

	// interpolate our real matrix.
	for (int i = 0; i < 128; i++)
		g_ctx.globals.m_real_matrix[i].SetOrigin(g_ctx.local()->GetAbsOrigin() - g_ctx.globals.local_origin[i]);

	local->force_bone_rebuild();

	std::memcpy(g_ctx.local()->m_flPoseParameter().data(), m_real_poses, 24 * sizeof(float));
	local->set_abs_angles(Vector(0, m_real_angles, 0));
	std::memcpy(g_ctx.local()->get_animlayers(), m_real_layers, g_ctx.local()->animlayer_count() * sizeof(AnimationLayer));

	// set our cached bone to our matrix.
	std::memcpy(g_ctx.local()->m_CachedBoneData().Base(), g_ctx.globals.m_real_matrix, g_ctx.local()->m_CachedBoneData().Count() * sizeof(matrix3x4_t));

	local->setup_bones_rebuilt(g_ctx.globals.m_real_matrix , 0xFFF00);
	std::memcpy(g_ctx.local()->m_flPoseParameter().data(), pose_parameter, 24 * sizeof(float));
}

bool local_animations::cached_matrix(matrix3x4_t* aMatrix)
{
	std::memcpy(aMatrix, g_ctx.globals.m_real_matrix, g_ctx.local()->m_CachedBoneData().Count() * sizeof(matrix3x4_t));
	return true;
}