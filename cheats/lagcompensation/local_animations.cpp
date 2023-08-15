// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: http://www.viva64.com

#include "local_animations.h"
#include "..\misc\misc.h"
#include "..\misc\logs.h"
#include "..\misc\prediction_system.h"
#include "../prediction/EnginePrediction.h"
#include "../tickbase shift/tickbase_shift.h"

void local_animations::run(CUserCmd* m_pcmd)
{
	// data from last createmove tick.
	auto data = &this->anim_data[(m_pcmd->m_command_number - 1) % MULTIPLAYER_BACKUP];

	// there's no command left to fix animation, abort fixing animation.
	if (!data->anim_cmd)
		return;

	// backup thirdperson recoil.
	const auto backup_thirdperson_recoil = g_ctx.local()->m_flThirdpersonRecoil();

	// set our thirdperson recoil to scaled aimpunchangle.
	g_ctx.local()->m_flThirdpersonRecoil() = g_ctx.local()->m_aimPunchAngleScaled().x;

	// updates animation-related for localplayer.
	this->update_local_animations(data);
	this->update_fake_animations(data);

	// set our data to the unmodified one.
	g_ctx.local()->m_flThirdpersonRecoil() = backup_thirdperson_recoil;
}

void local_animations::do_animation_event(Local_data::Anim_data* data)
{
	if (g_AntiAim->freeze_check)
	{
		this->m_movetype = MOVETYPE_NONE;
		this->m_flags = FL_ONGROUND;
	}

	AnimationLayer* pLandOrClimbLayer = &g_ctx.local()->get_animlayers()[ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB];
	if (!pLandOrClimbLayer)
		return;

	AnimationLayer* pJumpOrFallLayer = &g_ctx.local()->get_animlayers()[ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL];
	if (!pJumpOrFallLayer)
		return;

	if (this->m_movetype != MOVETYPE_LADDER && g_ctx.local()->get_move_type() == MOVETYPE_LADDER)
		g_ctx.local()->get_animation_state1()->set_layer_sequence(pLandOrClimbLayer, ACT_CSGO_CLIMB_LADDER);
	else if (this->m_movetype == MOVETYPE_LADDER && g_ctx.local()->get_move_type() != MOVETYPE_LADDER)
		g_ctx.local()->get_animation_state1()->set_layer_sequence(pJumpOrFallLayer, ACT_CSGO_FALL);
	else
	{
		if (g_ctx.local()->m_fFlags() & FL_ONGROUND)
		{
			if (!(this->m_flags & FL_ONGROUND))
				g_ctx.local()->get_animation_state1()->set_layer_sequence(pLandOrClimbLayer, g_ctx.local()->get_animation_state1()->m_flDurationInAir > 1.0f ? ACT_CSGO_LAND_HEAVY : ACT_CSGO_LAND_LIGHT);
		}
		else if (this->m_flags & FL_ONGROUND)
		{
			if (g_ctx.local()->m_vecVelocity().z > 0.0f)
				g_ctx.local()->get_animation_state1()->set_layer_sequence(pJumpOrFallLayer, ACT_CSGO_JUMP);
			else
				g_ctx.local()->get_animation_state1()->set_layer_sequence(pJumpOrFallLayer, ACT_CSGO_FALL);
		}
	}

	this->m_movetype = g_ctx.local()->get_move_type();
	this->m_flags = g_EnginePrediction->GetUnpredictedData()->m_nFlags;
}

std::array< AnimationLayer, 13 > local_animations::get_animlayers()
{
	std::array< AnimationLayer, 13 > aOutput;

	g_ctx.local()->copy_animlayers(aOutput.data());
	std::memcpy(&aOutput.at(ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL), &this->m_real_layers[ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL], sizeof(AnimationLayer));
	std::memcpy(&aOutput.at(ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB), &this->m_real_layers[ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB], sizeof(AnimationLayer));
	std::memcpy(&aOutput.at(ANIMATION_LAYER_ALIVELOOP), &this->m_real_layers[ANIMATION_LAYER_ALIVELOOP], sizeof(AnimationLayer));
	std::memcpy(&aOutput.at(ANIMATION_LAYER_LEAN), &this->m_real_layers[ANIMATION_LAYER_LEAN], sizeof(AnimationLayer));
	std::memcpy(&aOutput.at(ANIMATION_LAYER_MOVEMENT_MOVE), &this->m_real_layers[ANIMATION_LAYER_MOVEMENT_MOVE], sizeof(AnimationLayer));
	std::memcpy(&aOutput.at(ANIMATION_LAYER_MOVEMENT_STRAFECHANGE), &this->m_real_layers[ANIMATION_LAYER_MOVEMENT_STRAFECHANGE], sizeof(AnimationLayer));

	return aOutput;
}

void local_animations::store_animations_data(CUserCmd* cmd, bool& send_packet)
{
	if (!cmd)
		return;

	// data to be used in animations.
	auto data = &this->anim_data[(cmd->m_command_number % MULTIPLAYER_BACKUP)];

	// variety of command-related saved data.
	data->anim_cmd = cmd;
	data->tickbase = g_ctx.local()->m_nTickBase();
	data->send_packet = send_packet;
}

void local_animations::update_fake_animations(Local_data::Anim_data* data)
{
	// data used for animation.
	bool allocate = (!this->local_data.fake_animstate || this->local_data.fake_animstate->m_pBasePlayer != g_ctx.local()),
		change = (!allocate) && (g_ctx.local()->GetRefEHandle().ToLong() != this->m_fake_handle),
		reset = (!allocate && !change) && (g_ctx.local()->m_flSpawnTime() != this->m_fake_spawntime);

	// if the animstate didn't exist and the handle is not our entity then we free the animstate.
	if (change)
		this->local_data.fake_animstate = nullptr;

	// reset if animstate is not allocated or something is changed or our spawntime is changed.
	if (reset)
	{
		allocate = true;
		this->m_fake_spawntime = g_ctx.local()->m_flSpawnTime();
	}

	// if we already allocate it then we set the data for animation.
	if (allocate)
	{
		auto* state = reinterpret_cast<C_CSGOPlayerAnimationState*>(m_memalloc()->Alloc(sizeof(C_CSGOPlayerAnimationState)));

		if (state)
			util::create_state1(state, g_ctx.local());

		this->m_fake_handle = g_ctx.local()->GetRefEHandle().ToLong();
		this->m_fake_spawntime = g_ctx.local()->m_flSpawnTime();

		this->local_data.fake_animstate = state;
	}

	// code for viewangles fix in the future.
	static bool lock_viewangles = false;
	static Vector target_angle = data->anim_cmd->m_viewangles;
	static Vector non_shot_target_angle = data->anim_cmd->m_viewangles;

	// did we shoot at this command?.
	if (g_ctx.globals.shot_command == data->anim_cmd->m_command_number)
	{
		lock_viewangles = true;
		target_angle = data->anim_cmd->m_viewangles;
	}

	// we not shooting at this command :(.
	if (!lock_viewangles)
		target_angle = data->anim_cmd->m_viewangles;

	// invalidate prior animations.
	if (this->local_data.fake_animstate->m_nLastUpdateFrame == m_globals()->m_framecount)
		this->local_data.fake_animstate->m_nLastUpdateFrame = m_globals()->m_framecount - 1;

	// same as above.
	if (this->local_data.fake_animstate->m_flLastUpdateTime == m_globals()->m_curtime)
		this->local_data.fake_animstate->m_flLastUpdateTime = m_globals()->m_curtime - m_globals()->m_intervalpertick;

	// if everything is fine and we are choking packet then run our desync animfix code.
	if (data->send_packet)
	{
		// copy our pose parameter like a real man.
		AnimationLayer layers[13];
		float pose_parameter[24];

		// copy our animation data like a strong man.
		g_ctx.local()->copy_animlayers(layers);
		g_ctx.local()->copy_poseparameter(pose_parameter);

		// if our animstate owner is us then we need to change the base entity to our player.
		this->local_data.fake_animstate->m_pBasePlayer = g_ctx.local();

		// update our animstate to make it shows fake angles.
		util::update_state1(this->local_data.fake_animstate, target_angle);

		// now we setup the bones after everything is done.
		g_ctx.local()->setup_bones_rebuilt(g_ctx.globals.local.m_fake_matrix , BONE_USED_BY_ANYTHING);

		// do we activate visualize lag?.
		this->local_data.visualize_lag = g_cfg.player.visualize_lag;

		// if we not activate it yet we interpolate our fake matrix.
		if (!this->local_data.visualize_lag)
		{
			// loop through all of the arbitrary.
			for (auto& i : g_ctx.globals.local.m_fake_matrix)
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

void local_animations::update_local_animations(Local_data::Anim_data* data)
{
	// restore data.
	const auto m_curtime = m_globals()->m_curtime;
	const auto m_realtime = m_globals()->m_realtime;
	const auto m_absoluteframetime = m_globals()->m_absoluteframetime;
	const auto m_frametime = m_globals()->m_frametime;
	const auto m_interpolation_amount = m_globals()->m_interpolation_amount;
	const auto m_tickcount = m_globals()->m_tickcount;
	const auto m_framecount = m_globals()->m_framecount;

	// code for viewangles fix in the future.
	static bool lock_viewangles = false;
	static Vector target_angle = data->anim_cmd->m_viewangles;
	static Vector non_shot_target_angle = data->anim_cmd->m_viewangles;

	// did we shoot at this command?.
	if (g_ctx.globals.shot_command == data->anim_cmd->m_command_number)
	{
		lock_viewangles = true;
		target_angle = data->anim_cmd->m_viewangles;
	}

	// we not shooting at this command :(.
	if (!lock_viewangles)
		target_angle = data->anim_cmd->m_viewangles;

	g_ctx.local()->m_fFlags() = g_EnginePrediction->GetUnpredictedData()->m_nFlags;

	// if our spawntime didn't match our entity spawntime or the entity of the animstate holder is not us?.
	if (g_ctx.local()->m_flSpawnTime() != this->m_real_spawntime || g_ctx.local()->get_animation_state1()->m_pBasePlayer != g_ctx.local())
	{
		this->m_flags = g_ctx.local()->m_fFlags();
		this->m_movetype = g_ctx.local()->get_move_type();

		this->m_real_spawntime = g_ctx.local()->m_flSpawnTime();
	}

	g_ctx.local()->set_animlayers(this->get_animlayers().data());

	// thing for restore later.
	const auto iFlags = g_ctx.local()->m_fFlags();
	const auto flLowerBodyYaw = g_ctx.local()->m_flLowerBodyYawTarget();
	const auto flDuckSpeed = g_ctx.local()->m_flDuckSpeed();
	const auto flDuckAmount = g_ctx.local()->m_flDuckAmount();
	const auto vecThirdPersonViewAngles = g_ctx.local()->m_thirdPersonViewAngles();
	const auto vecAbsVelocity = g_ctx.local()->m_vecAbsVelocity();

	// match our global variables with local variables.
	m_globals()->m_curtime = TICKS_TO_TIME(data->anim_cmd->m_tickcount);
	m_globals()->m_realtime = TICKS_TO_TIME(data->anim_cmd->m_tickcount);
	m_globals()->m_absoluteframetime = m_globals()->m_intervalpertick;
	m_globals()->m_frametime = m_globals()->m_intervalpertick;
	m_globals()->m_tickcount = data->anim_cmd->m_tickcount;
	m_globals()->m_framecount = data->anim_cmd->m_tickcount;
	m_globals()->m_interpolation_amount = 0.0f;

	g_ctx.local()->get_animation_state1()->m_flPrimaryCycle = this->m_real_layers[ANIMATION_LAYER_MOVEMENT_MOVE].m_flCycle;
	g_ctx.local()->get_animation_state1()->m_flMoveWeight = this->m_real_layers[ANIMATION_LAYER_MOVEMENT_MOVE].m_flWeight;
	g_ctx.local()->get_animation_state1()->m_nStrafeSequence = this->m_real_layers[ANIMATION_LAYER_MOVEMENT_STRAFECHANGE].m_nSequence;
	g_ctx.local()->get_animation_state1()->m_flStrafeChangeCycle = this->m_real_layers[ANIMATION_LAYER_MOVEMENT_STRAFECHANGE].m_flCycle;
	g_ctx.local()->get_animation_state1()->m_flStrafeChangeWeight = this->m_real_layers[ANIMATION_LAYER_MOVEMENT_STRAFECHANGE].m_flWeight;
	g_ctx.local()->get_animation_state1()->m_flAccelerationWeight = this->m_real_layers[ANIMATION_LAYER_LEAN].m_flWeight;

	// set our render angles to our viewangles and to fixed viewangles later ( hide shots exploit and double tap exploit so it's a compensate to make our animfix shows hide shot ).
	g_ctx.local()->m_vecAbsVelocity() = g_ctx.local()->m_vecVelocity();
	g_ctx.local()->m_thirdPersonViewAngles() = (((tickbase::get().hide_shots_key) && !g_ctx.globals.fakeducking && lock_viewangles) ? non_shot_target_angle : target_angle);

	// allows re-animating at the same tick.
	if (g_ctx.local()->get_animation_state1()->m_nLastUpdateFrame == m_globals()->m_framecount)
		g_ctx.local()->get_animation_state1()->m_nLastUpdateFrame = m_globals()->m_framecount - 1;

	// same as above.
	if (g_ctx.local()->get_animation_state1()->m_flLastUpdateTime == m_globals()->m_curtime)
		g_ctx.local()->get_animation_state1()->m_flLastUpdateTime = m_globals()->m_curtime - m_globals()->m_intervalpertick;

	this->do_animation_event(data);
	for (int i = NULL; i < 13; i++)
		g_ctx.local()->get_animlayers()[i].m_pOwner = g_ctx.local();

	bool bClientSideAnimation = g_ctx.local()->m_bClientSideAnimation();
	g_ctx.local()->m_bClientSideAnimation() = true;

	g_ctx.globals.updating_animation = true;
	g_ctx.local()->update_clientside_animation();
	g_ctx.globals.updating_animation = false;

	g_ctx.local()->m_bClientSideAnimation() = bClientSideAnimation;

	g_ctx.local()->copy_poseparameter(this->m_real_poses);
	g_ctx.local()->copy_animlayers(this->m_real_layers);

	g_ctx.local()->m_fFlags() = iFlags;
	g_ctx.local()->m_flDuckSpeed() = flDuckSpeed;
	g_ctx.local()->m_flDuckAmount() = flDuckAmount;
	g_ctx.local()->m_flLowerBodyYawTarget() = flLowerBodyYaw;
	g_ctx.local()->m_thirdPersonViewAngles() = vecThirdPersonViewAngles;
	g_ctx.local()->m_vecAbsVelocity() = vecAbsVelocity;

	m_globals()->m_curtime = m_curtime;
	m_globals()->m_realtime = m_realtime;
	m_globals()->m_absoluteframetime = m_absoluteframetime;
	m_globals()->m_frametime = m_frametime;
	m_globals()->m_tickcount = m_tickcount;
	m_globals()->m_framecount = m_framecount;
	m_globals()->m_interpolation_amount = m_interpolation_amount;

	if (data->send_packet)
	{
		// if we not lock our viewangles then we set our non_shot_target_angle to current viewangles to make our localplayer shows accurate shootpos.
		if (!lock_viewangles)
			non_shot_target_angle = data->anim_cmd->m_viewangles;

		// save off real data.
		this->m_real_angles = g_ctx.local()->get_animation_state1()->m_flFootYaw;

		// rebuilt bones and interpolate our origin with our matrix origin..
		g_ctx.globals.local.m_real_matrix_ret = g_ctx.local()->setup_bones_rebuilt(g_ctx.globals.local.m_real_matrix, BONE_USED_BY_ANYTHING);

		g_ctx.local()->set_animlayers(this->get_animlayers().data());
		g_ctx.local()->set_poseparameter(this->m_real_poses);

		for (int i = NULL; i < MAXSTUDIOBONES; i++)
			g_ctx.globals.local_origin[i] = g_ctx.local()->GetAbsOrigin() - g_ctx.globals.local.m_real_matrix[i].GetOrigin();

		// we passed through all of it now we don't need to lock viewangles for future.
		lock_viewangles = false;
	}
}

void local_animations::setup_shoot_position(CUserCmd* m_pcmd)
{
	const auto old_body_pitch = g_ctx.local()->m_flPoseParameter()[12];

	g_ctx.local()->m_flPoseParameter()[12] = (m_pcmd->m_viewangles.x + 89.0f) / 178.0f;
	g_ctx.local()->setup_bones_rebuilt(nullptr, BONE_USED_BY_HITBOX);
	g_ctx.local()->m_flPoseParameter()[12] = old_body_pitch;

	g_ctx.globals.eye_pos = g_ctx.local()->get_shoot_position();
}

void local_animations::on_update_clientside_animation(player_t* player)
{
	// backup for our data.
	const auto backup_bones = player->m_BoneAccessor().m_pBones;

	// interpolate our real matrix.
	for (int i = NULL; i < MAXSTUDIOBONES; i++)
		g_ctx.globals.local.m_real_matrix[i].SetOrigin(player->GetAbsOrigin() - g_ctx.globals.local_origin[i]);

	// set our cached bone to our matrix.
	std::memcpy(player->m_CachedBoneData().Base(), g_ctx.globals.local.m_real_matrix, player->m_CachedBoneData().Count() * sizeof(matrix3x4_t));

	// set our accessor bones to our matrix.
	player->m_BoneAccessor().m_pBones = g_ctx.globals.local.m_real_matrix;
	player->attachment_helper();
	player->m_BoneAccessor().m_pBones = backup_bones;
}

void local_animations::get_cached_matrix(player_t* player, matrix3x4_t* matrix)
{
	if (g_ctx.globals.local.m_real_matrix_ret)
		std::memcpy(matrix, g_ctx.globals.local.m_real_matrix, player->m_CachedBoneData().Count() * sizeof(matrix3x4_t));
}

void local_animations::reset_data()
{
	std::memset(g_ctx.globals.local_origin, 0, sizeof(g_ctx.globals.local_origin));
	std::memset(g_ctx.globals.local.m_fake_matrix, 0, sizeof(g_ctx.globals.local.m_fake_matrix));
	std::memset(g_ctx.globals.local.m_real_matrix, 0, sizeof(g_ctx.globals.local.m_real_matrix));
	std::memset(this->m_real_layers, 0, sizeof(this->m_real_layers));
	std::memset(this->m_real_poses, 0, sizeof(this->m_real_poses));
	std::memset(this->anim_data, 0, sizeof(this->anim_data));

	g_ctx.globals.local.m_real_matrix_ret = false;

	this->local_data.fake_animstate = nullptr;
	this->local_data.visualize_lag = false;

	this->m_fake_handle = NULL;
	this->m_flags = 0;
	this->m_movetype = 0;
	this->m_real_angles = 0;
	this->m_real_spawntime = this->m_fake_spawntime = 0.0f;
}