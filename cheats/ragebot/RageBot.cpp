// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: http://www.viva64.com


//pasting progress: 1-paste the ragebot [DONE]
//                  2-paste the autowall [DONE]
//                  3-paste menu and config elemnts [done]
//                  4-paste shots reg system [done]
//                  5-paste knifebot
//                  6-paste hit_chams [PENDING]
#include "aim.h"
#include "..\misc\misc.h"
#include "..\misc\logs.h"
#include "..\misc\prediction_system.h"
#include "..\autowall\penetration.h"
#include "..\fakewalk\slowwalk.h"
#include "..\lagcompensation\local_animations.h"
#include "..\visuals\hitchams.h"
#include "../prediction/Networking.h"
#include "../prediction/EnginePrediction.h"
#include "ray_tracer.hpp"
#include "../visuals/hitchams.h"
#include "shots.h"



void aimbot::AutoStop(CUserCmd* m_pcmd)
{
	if (!this->m_stop)
		return;

	m_stop = false;

	if (!g_ctx.globals.weapon->m_iItemDefinitionIndex() == WEAPON_TASER || !g_cfg.ragebot.weapon[g_ctx.globals.current_weapon].autostop)
		return;

	if (g_ctx.globals.slowwalking && !g_cfg.misc.slowwalk)
		return;

	if (!(g_ctx.local()->m_fFlags() & FL_ONGROUND && g_EnginePrediction->GetUnpredictedData()->m_nFlags & FL_ONGROUND)) //-V807
		return;

	if (g_ctx.globals.weapon->is_empty())
		return;

	auto animlayer = g_ctx.local()->get_animlayers()[1];

	if (animlayer.m_nSequence)
	{
		auto activity = g_ctx.local()->sequence_activity(animlayer.m_nSequence);

		if (activity == ACT_CSGO_RELOAD && animlayer.m_flWeight > 0.0f)
			return;
	}

	auto weapon_info = g_ctx.globals.weapon->get_csweapon_info();

	if (!weapon_info)
		return;

	auto max_speed = 0.33f * g_ctx.local()->GetMaxPlayerSpeed();

	if (g_ctx.local()->m_vecVelocity().Length2D() < max_speed)
		g_Slowwalk->create_move(m_pcmd);
	else
	{
		Vector direction;
		Vector real_view;

		math::vector_angles(g_ctx.local()->m_vecVelocity(), direction);
		m_engine()->GetViewAngles(real_view);

		direction.y = real_view.y - direction.y;

		Vector forward;
		math::angle_vectors(direction, forward);

		static auto cl_forwardspeed = m_cvar()->FindVar(crypt_str("cl_forwardspeed"))->GetFloat();
		static auto cl_sidespeed = m_cvar()->FindVar(crypt_str("cl_sidespeed"))->GetFloat();

		auto negative_forward_speed = -cl_forwardspeed;
		auto negative_side_speed = -cl_sidespeed;

		auto negative_forward_direction = forward * negative_forward_speed;
		auto negative_side_direction = forward * negative_side_speed;

		m_pcmd->m_forwardmove = negative_forward_direction.x;
		m_pcmd->m_sidemove = negative_side_direction.y;
	}

	g_EnginePrediction->RePredict();
}

void aimbot::AutoRevolver(CUserCmd* m_pcmd)
{
	if (!g_cfg.ragebot.enable)
		return;

	if (!m_engine()->IsActiveApp())
		return;

	if (g_ctx.globals.weapon->m_iItemDefinitionIndex() != WEAPON_REVOLVER)
		return;

	if (m_pcmd->m_buttons & IN_ATTACK)
		return;

	m_pcmd->m_buttons &= ~IN_ATTACK2;

	static auto r8cock_time = 0.0f;
	auto server_time = TICKS_TO_TIME(g_ctx.globals.backup_tickbase);

	if (g_ctx.globals.weapon->can_fire(false))
	{
		if (r8cock_time <= server_time) //-V807
		{
			if (g_ctx.globals.weapon->m_flNextSecondaryAttack() <= server_time)
				r8cock_time = server_time + TICKS_TO_TIME(13);
			else
				m_pcmd->m_buttons |= IN_ATTACK2;
		}
		else
			m_pcmd->m_buttons |= IN_ATTACK;
	}
	else
	{
		r8cock_time = server_time + TICKS_TO_TIME(13);
		m_pcmd->m_buttons &= ~IN_ATTACK;
	}

	g_ctx.globals.revolver_working = true;
}

void aimbot::AdjustRevolverData(int commandnumber, int buttons)
{
	auto weapon = g_ctx.local()->m_hActiveWeapon().Get();

	if (weapon)
	{
		static float tickbase_records[MULTIPLAYER_BACKUP];
		static bool in_attack[MULTIPLAYER_BACKUP];
		static bool can_shoot[MULTIPLAYER_BACKUP];

		tickbase_records[commandnumber % MULTIPLAYER_BACKUP] = g_ctx.local()->m_nTickBase();
		in_attack[commandnumber % MULTIPLAYER_BACKUP] = buttons & IN_ATTACK || buttons & IN_ATTACK2;
		can_shoot[commandnumber % MULTIPLAYER_BACKUP] = weapon->can_fire(false);

		if (weapon->m_iItemDefinitionIndex() == WEAPON_REVOLVER)
		{
			auto postpone_fire_ready_time = FLT_MAX;

			if ((int)g_Networking->tickrate() >> 1 > 1)
			{
				auto command_number = commandnumber - 1;
				auto shoot_number = 0;

				for (auto i = 1; i < (int)g_Networking->tickrate() >> 1; ++i)
				{
					shoot_number = command_number;

					if (!in_attack[command_number % MULTIPLAYER_BACKUP] || !can_shoot[command_number % MULTIPLAYER_BACKUP])
						break;

					--command_number;
				}

				if (shoot_number)
				{
					auto tick = 1 - (int)(-0.03348f / m_globals()->m_intervalpertick);

					if (commandnumber - shoot_number >= tick)
						postpone_fire_ready_time = TICKS_TO_TIME(tickbase_records[(tick + shoot_number) % MULTIPLAYER_BACKUP]) + 0.2f;
				}
			}

			weapon->m_flPostponeFireReadyTime() = postpone_fire_ready_time;
		}
	}
}

void AimPlayer::OnNetUpdate(player_t* player) {
	if (!g_cfg.ragebot.enable || !player->is_alive() || player->m_iTeamNum() == g_ctx.local()->m_iTeamNum()) {
		this->reset();

		return;
	}

	if (player->IsDormant()) {
		m_type = 0;
		m_side = 0;
		m_last_side_hit = 0;
		m_resolve_strenght = 0.f;
		m_last_resolve_strenght_hit = 0.f;
	}

	// update player_t ptr.
	//m_player = player;
}

void AimPlayer::OnRoundStart(player_t* player) {
	//m_player = player;
	m_shots = 0;
	m_missed_shots = 0;
	m_type = 0;
	m_side = 0;
	m_type_before_bruteforce = 0;
	m_side_before_bruteforce = 0;
	m_last_side_hit = 0;
	m_resolve_strenght = 0.f;
	m_resolve_strenght_before_bruteforce = 0.f;
	m_last_resolve_strenght_hit = 0.f;

	if (!m_hitboxes.empty())
		m_hitboxes.clear();

	// IMPORTANT: DO NOT CLEAR LAST HIT SHIT.
}

void AimPlayer::SetupHitboxes(adjust_data* record, bool history) {
	if (!record)
		return;

	// reset hitboxes.
	if (!m_hitboxes.empty())
		m_hitboxes.clear();

	if (g_ctx.globals.weapon->m_iItemDefinitionIndex() == WEAPON_TASER) {
		// hitboxes for the zeus.
		m_hitboxes.push_back({ HITBOX_STOMACH, HitscanMode::PREFER });

		return;
	}

	auto only = false;
	AimPlayer* data = & g_Ragebot->m_players[record->i];

	// only, always.
	if (g_cfg.ragebot.weapon[g_ctx.globals.current_weapon].always_body_aim.at(ALWAYS_BAIM_ALWAYS) || key_binds::get().get_key_bind_state(22) || (g_cfg.ragebot.weapon[g_ctx.globals.current_weapon].max_misses && data->m_missed_shots >= g_cfg.ragebot.weapon[g_ctx.globals.current_weapon].max_misses_amount)) {
		only = true;
		m_hitboxes.push_back({ HITBOX_CHEST, HitscanMode::PREFER });
		m_hitboxes.push_back({ HITBOX_STOMACH, HitscanMode::PREFER });
		m_hitboxes.push_back({ HITBOX_PELVIS, HitscanMode::PREFER });
	}

	// only, health.
	if (g_cfg.ragebot.weapon[g_ctx.globals.current_weapon].always_body_aim.at(ALWAYS_BAIM_HEATH_BELOW) && record->player->m_iHealth() <= g_cfg.ragebot.weapon[g_ctx.globals.current_weapon].always_baim_if_hp_below) {
		only = true;
		m_hitboxes.push_back({ HITBOX_CHEST, HitscanMode::PREFER });
		m_hitboxes.push_back({ HITBOX_STOMACH, HitscanMode::PREFER });
		m_hitboxes.push_back({ HITBOX_PELVIS, HitscanMode::PREFER });
	}

	// only, in air.
	if (g_cfg.ragebot.weapon[g_ctx.globals.current_weapon].always_body_aim.at(ALWAYS_BAIM_AIR) && !(record->flags & FL_ONGROUND)) {
		only = true;
		m_hitboxes.push_back({ HITBOX_CHEST, HitscanMode::PREFER });
		m_hitboxes.push_back({ HITBOX_STOMACH, HitscanMode::PREFER });
		m_hitboxes.push_back({ HITBOX_PELVIS, HitscanMode::PREFER });
	}

	// only baim conditions have been met.
	// do not insert more hitboxes.
	if (only)
		return;

	// prefer, always.
	if (g_cfg.ragebot.weapon[g_ctx.globals.current_weapon].prefer_body_aim.at(PREFER_BAIM_ALWAYS)) {
		m_hitboxes.push_back({ HITBOX_CHEST, HitscanMode::PREFER });
		m_hitboxes.push_back({ HITBOX_STOMACH, HitscanMode::PREFER });
		m_hitboxes.push_back({ HITBOX_PELVIS, HitscanMode::PREFER });
	}

	// prefer, lethal.
	if (g_cfg.ragebot.weapon[g_ctx.globals.current_weapon].prefer_body_aim.at(PREFER_BAIM_LETHAL)) {
		m_hitboxes.push_back({ HITBOX_CHEST, HitscanMode::LETHAL });
		m_hitboxes.push_back({ HITBOX_STOMACH, HitscanMode::LETHAL });
		m_hitboxes.push_back({ HITBOX_PELVIS, HitscanMode::LETHAL });
	}

	// prefer, in air.
	if (g_cfg.ragebot.weapon[g_ctx.globals.current_weapon].prefer_body_aim.at(PREFER_BAIM_AIR) && !(record->flags & FL_ONGROUND)) {
		m_hitboxes.push_back({ HITBOX_CHEST, HitscanMode::PREFER });
		m_hitboxes.push_back({ HITBOX_STOMACH, HitscanMode::PREFER });
		m_hitboxes.push_back({ HITBOX_PELVIS, HitscanMode::PREFER });
	}

	bool ignore_limbs = record->velocity.Length2D() > 71.f && g_cfg.ragebot.weapon[g_ctx.globals.current_weapon].ignore_limbs;

	if (g_cfg.ragebot.weapon[g_ctx.globals.current_weapon].hitboxes.at(0)) {
		if (g_cfg.ragebot.weapon[g_ctx.globals.current_weapon].prefer_safe_points) {
			m_hitboxes.push_back({ HITBOX_HEAD, HitscanMode::PREFER_SAFEPOINT, true });
		}

		m_hitboxes.push_back({ HITBOX_HEAD, g_cfg.ragebot.weapon[g_ctx.globals.current_weapon].preferred_hitbox == 1 ? HitscanMode::PREFER : HitscanMode::NORMAL, false });
	}

	if (g_cfg.ragebot.weapon[g_ctx.globals.current_weapon].hitboxes.at(1))
		m_hitboxes.push_back({ HITBOX_UPPER_CHEST, HitscanMode::NORMAL, false });

	if (g_cfg.ragebot.weapon[g_ctx.globals.current_weapon].hitboxes.at(2))
		m_hitboxes.push_back({ HITBOX_CHEST, HitscanMode::NORMAL, false });

	if (g_cfg.ragebot.weapon[g_ctx.globals.current_weapon].hitboxes.at(3)) {
		if (g_cfg.ragebot.weapon[g_ctx.globals.current_weapon].prefer_safe_points) {
			m_hitboxes.push_back({ HITBOX_LOWER_CHEST, HitscanMode::PREFER_SAFEPOINT, true });
		}

		m_hitboxes.push_back({ HITBOX_LOWER_CHEST, HitscanMode::NORMAL, true });
	}

	if (g_cfg.ragebot.weapon[g_ctx.globals.current_weapon].hitboxes.at(4)) {
		if (g_cfg.ragebot.weapon[g_ctx.globals.current_weapon].prefer_safe_points) {
			m_hitboxes.push_back({ HITBOX_STOMACH, HitscanMode::PREFER_SAFEPOINT, true });
		}

		m_hitboxes.push_back({ HITBOX_STOMACH, g_cfg.ragebot.weapon[g_ctx.globals.current_weapon].preferred_hitbox == 3 ? HitscanMode::PREFER : HitscanMode::NORMAL, false });
	}

	if (g_cfg.ragebot.weapon[g_ctx.globals.current_weapon].hitboxes.at(5)) {
		if (g_cfg.ragebot.weapon[g_ctx.globals.current_weapon].prefer_safe_points) {
			m_hitboxes.push_back({ HITBOX_PELVIS, HitscanMode::PREFER_SAFEPOINT, true });
		}

		m_hitboxes.push_back({ HITBOX_PELVIS, g_cfg.ragebot.weapon[g_ctx.globals.current_weapon].preferred_hitbox == 2 ? HitscanMode::PREFER : HitscanMode::NORMAL, false });
	}

	if (g_cfg.ragebot.weapon[g_ctx.globals.current_weapon].hitboxes.at(6) && !ignore_limbs)
	{
		m_hitboxes.push_back({ HITBOX_RIGHT_UPPER_ARM, HitscanMode::NORMAL, false });
		m_hitboxes.push_back({ HITBOX_LEFT_UPPER_ARM, HitscanMode::NORMAL, false });
	}

	if (g_cfg.ragebot.weapon[g_ctx.globals.current_weapon].hitboxes.at(7))
	{
		m_hitboxes.push_back({ HITBOX_RIGHT_THIGH, HitscanMode::NORMAL, false });
		m_hitboxes.push_back({ HITBOX_LEFT_THIGH, HitscanMode::NORMAL, false });

		m_hitboxes.push_back({ HITBOX_RIGHT_CALF, HitscanMode::NORMAL, false });
		m_hitboxes.push_back({ HITBOX_LEFT_CALF, HitscanMode::NORMAL, false });
	}

	if (g_cfg.ragebot.weapon[g_ctx.globals.current_weapon].hitboxes.at(8) && !ignore_limbs)
	{
		m_hitboxes.push_back({ HITBOX_RIGHT_FOOT, HitscanMode::NORMAL, false });
		m_hitboxes.push_back({ HITBOX_LEFT_FOOT, HitscanMode::NORMAL, false });
	}
}

void aimbot::init() {
	// clear globals.
	g_ctx.globals.aimbot_working = false;
	g_ctx.globals.aimbot_shooting = false;
	g_ctx.globals.revolver_working = false;

	// clear old targets.
	if (!m_targets.empty())
		m_targets.clear();

	m_target = nullptr;
	m_point = HitscanPoint_t{};
	//this->m_working = false;
	if (!m_debug_points.empty())
		m_debug_points.clear();

	m_angle = ZERO;
	m_damage = 0.f;
	m_record = nullptr;
	m_safe = false;
	m_stop = false;
	m_hitbox = -1;

	m_best_dist = FLT_MAX;
	m_best_fov = 180.f + 1.f;
	m_best_damage = 0.f;
	m_best_hp = 100 + 1;
	m_best_lag = FLT_MAX;
	m_best_height = FLT_MAX;

	//this->m_current_matrix = nullptr;
}

static bool compare_records(const optimized_adjust_data& first, const optimized_adjust_data& second)
{
	auto first_pitch = math::normalize_pitch(first.angles.x);
	auto second_pitch = math::normalize_pitch(second.angles.x);

	if (first.shot)
		return first.shot > second.shot;
	else if (second.shot)
		return second.shot > first.shot;
	else
		return first.simulation_time > second.simulation_time;
}

adjust_data* aimbot::get_record(std::deque <adjust_data>* records)
{
	for (auto i = 0; i < records->size(); ++i)
	{
		auto record = &records->at(i);

		if (!record->valid())
			continue;

		return record;
	}

	return nullptr;
}

adjust_data* aimbot::get_record_history(std::deque <adjust_data>* records)
{
	//if (records->size() < 2)
	//	return nullptr;

	//if (records->empty())
	//	return nullptr;

	//adjust_data* lastRecord = &records->back();

	//if (lastRecord->valid())
	//	return lastRecord;
	//else
	//	return nullptr;


	std::deque <optimized_adjust_data> optimized_records; //-V826

	for (int i = 1; i < records->size(); ++i) // red
	{
		auto record = &records->at(i);
		optimized_adjust_data optimized_record;

		optimized_record.i = i;
		optimized_record.player = record->player;
		optimized_record.simulation_time = record->simulation_time;
		optimized_record.duck_amount = record->duck_amount;
		optimized_record.angles = record->angles;
		optimized_record.origin = record->origin;
		optimized_record.shot = record->shot;

		optimized_records.emplace_back(optimized_record);
	}

	if (optimized_records.size() < 2)
		return nullptr;

	std::sort(optimized_records.begin(), optimized_records.end(), compare_records);

	for (auto& optimized_record : optimized_records)
	{
		auto record = &records->at(optimized_record.i);

		if (!record->valid())
			continue;

		return record;
	}
	return nullptr;
}

adjust_data* aimbot::get_oldest_record(std::deque <adjust_data>* records)
{
	if (records->size() < 2)
		return nullptr;

	if (records->empty())
		return nullptr;

	for (auto record = records->rbegin(); record != records->rend(); ++record)
	{
		if (!record->valid())
			continue;

		return &(*record);
	}

	return nullptr;
}

adjust_data* aimbot::get_first_record(std::deque<adjust_data>* records)
{
	if (records->empty())
		return nullptr;

	for (auto record = records->begin(); record != records->end(); ++record)
	{
		if (!record->valid())
			continue;

		return &(*record);
	}

	return nullptr;
}


bool aimbot::is_peeking_enemy(float ticks_to_stop, bool aim)
{
	// data to be used later.
	bool done = false;

	// this is the code if we can run it on our aimbot. (autostop on peek).
	if (aim)
	{
		if (g_ctx.globals.weapon->m_iItemDefinitionIndex() == WEAPON_TASER)
			return done;

		if (!g_cfg.ragebot.weapon[g_ctx.globals.current_weapon].autostop_modifiers[AUTOSTOP_EARLY])
			return done;
	}

	// there are no target, don't run the code.
	if (this->m_targets.empty())
		return done;

	// this is our predicted eye pos to see if we can hit our enemy in our next tick.
	auto predicted_eye_pos = g_ctx.globals.eye_pos + g_EnginePrediction->GetUnpredictedData()->m_vecVelocity * m_globals()->m_intervalpertick * ticks_to_stop;

	for (const auto& t : this->m_targets)
	{
		if (!t->m_player)
			continue;

		// reset uh?.
		done = false;
		auto records = &player_records[t->m_player->EntIndex()]; //-V826
		if (records->empty())
			continue;
		auto first_record = get_first_record(records);

		if (!first_record || !first_record->valid() || first_record->invalid)
			continue;

		// data to be used later.
		const auto backup_origin = first_record->player->m_vecOrigin();
		const auto backup_abs_origin = first_record->player->GetAbsOrigin();
		const auto backup_abs_angles = first_record->player->GetAbsAngles();
		const auto backup_obb_mins = first_record->player->m_vecMins();
		const auto backup_obb_maxs = first_record->player->m_vecMaxs();
		const auto backup_cache = first_record->player->m_CachedBoneData().Base();

		auto restore = [&]() -> void {
			first_record->player->m_vecOrigin() = backup_origin;
			first_record->player->set_abs_origin(backup_abs_origin);
			first_record->player->set_abs_angles(backup_abs_angles);
			first_record->player->UpdateCollisionBounds();
			first_record->player->SetCollisionBounds(backup_obb_mins, backup_obb_maxs);
			std::memcpy(first_record->player->m_CachedBoneData().Base(), backup_cache, first_record->player->m_CachedBoneData().Count() * sizeof(matrix3x4_t));
		};

		penetration::PenetrationInput_t in;

		// this is the input.
		in.m_damage = 1.f;
		in.m_damage_pen = 1.f;
		in.m_can_pen = true;
		in.m_target = first_record->player;
		in.m_from = g_ctx.local();
		in.m_pos = first_record->player->hitbox_position_matrix(HITBOX_STOMACH, first_record->m_Matricies[MiddleMatrix].data());
		in.m_custom_shoot_pos = predicted_eye_pos;

		penetration::PenetrationOutput_t out;

		// set our enemy stuff to the current record.
		first_record->player->m_vecOrigin() = first_record->origin;
		first_record->player->set_abs_origin(first_record->origin);
		first_record->player->set_abs_angles(first_record->abs_angles);
		first_record->player->UpdateCollisionBounds();
		first_record->player->SetCollisionBounds(first_record->mins, first_record->maxs);
		std::memcpy(first_record->player->m_CachedBoneData().Base(), first_record->m_Matricies[MiddleMatrix].data(), sizeof(first_record->m_Matricies[MiddleMatrix].data()));

		// run our p100 penetration code.
		if (penetration::get().run(&in, &out, true)) {
			// restore things back.
			restore();

			// no damage :(
			if (out.m_damage < 1)
				continue;

			// damn, we done.
			done = true;
		}
		else
			// restore things back.
			restore();

		// we found a target we can shoot at and deal damage? fuck yeah.
		if (done)
			break;
	}

	return done;
}

void aimbot::think(CUserCmd* m_pcmd) {
	// do all startup routines.
	this->init();

	// we have no aimbot enabled.
	if (!g_cfg.ragebot.enable)
		return;

	// no grenades or bomb.
	if (g_ctx.globals.weapon->is_non_aim(false))
		return;

	// no point in aimbotting if we cannot fire this tick.
	if (!g_ctx.globals.weapon->can_fire(true))
		return;

	// setup bones for all valid targets.
	for (auto i = 0; i < m_globals()->m_maxclients; ++i)
	{
		auto player = (player_t*)m_entitylist()->GetClientEntity(i);

		if (!player || player == g_ctx.local())
			continue;

		if (!IsValidTarget(player))
			continue;

		AimPlayer* data = &this->m_players[i];

		data->m_player = player;

		if (!data)
			continue;

		this->m_targets.push_back(data);
	}

	// run knifebot.
	if (g_ctx.globals.weapon->is_knife()) {
		//knife(m_pcmd);

		return;
	}

	g_Ragebot->AutoRevolver(m_pcmd);

	// auto-stop if we about to peek this guy.
	if (!this->m_stop && this->is_peeking_enemy(math::clamp(g_EnginePrediction->GetUnpredictedData()->m_vecVelocity.Length2D() / g_ctx.local()->GetMaxPlayerSpeed() * 3.0f, 0.0f, 4.0f), true)) {
		this->m_stop = true;
	}


	// scan available targets... if we even have any.
	this->find(m_pcmd);

	// finally set data when shooting.
	this->apply(m_pcmd);
}

void aimbot::find(CUserCmd* m_pcmd) {
	struct BestTarget_t { player_t* player; AimPlayer* target; HitscanPoint_t point; float damage; float min_damage; adjust_data* record; int hitbox; bool safe; float damage_compare; };

	HitscanPoint_t       tmp_point;
	float        tmp_damage;
	float		 tmp_min_damage;
	BestTarget_t best;
	best.player = nullptr;
	best.target = nullptr;
	best.damage = -1.f;
	best.point = HitscanPoint_t{};
	best.record = nullptr;
	best.hitbox = -1;
	best.safe = false;
	best.damage_compare = -1;

	adjust_data* best_record = nullptr;

	if (this->m_targets.empty())
		return;

	// iterate all targets.
	for (const auto& t : this->m_targets) {
		if (!t->m_player)
			continue;


		auto records = &player_records[t->m_player->EntIndex()]; //-V826
		if (records->empty())
			continue;

		//auto ideal = get_record(records);
		auto ideal = get_first_record(records);
		//auto ideal = &records->front();

		if (ideal->valid())
		{
			t->SetupHitboxes(ideal, false);
			if (!t->m_hitboxes.empty())
			{
				// try to select best record as target.
				if (t->GetBestAimPosition(tmp_point, tmp_damage, best.hitbox, best.safe, ideal, tmp_min_damage)) {
					if (SelectTarget(ideal, tmp_point, tmp_damage)) {
						if (tmp_damage >= best.damage_compare) {
							best.damage_compare = tmp_damage;
							// if we made it so far, set shit. 
							best.player = t->m_player;
							best.point = tmp_point;
							best.damage = tmp_damage;
							best.record = ideal;
							best.min_damage = tmp_min_damage;
							best.target = t;
						}

					}
				}
			}

		}

		//auto last = get_record_history(records);
		auto last = get_oldest_record(records);

		if (last->valid())
		{
			t->SetupHitboxes(last, true);
			if (!t->m_hitboxes.empty())
			{
				// rip something went wrong..
				if (t->GetBestAimPosition(tmp_point, tmp_damage, best.hitbox, best.safe, last, tmp_min_damage)) {
					if (SelectTarget(last, tmp_point, tmp_damage))
					{
						if (tmp_damage > best.damage_compare) {
							best.damage_compare = tmp_damage;
							// if we made it so far, set shit.
							best.player = t->m_player;
							best.point = tmp_point;
							best.damage = tmp_damage;
							best.record = last;
							best.min_damage = tmp_min_damage;
							best.target = t;
						}

					}
				}
			}
		}

		if (best.record) {
			if (best.record == last)
				best.record->m_fDidBacktrack = true;
			else if (best.record == ideal)
				best.record->m_fDidBacktrack = false;
		}

		// we found a target we can shoot at and deal damage? fuck yeah. (THIS IS TEMPORARY TILL WE REPLACE THE TARGET SELECTION)
		if (best.damage > 0.f && best.player && best.record && best.target) {
			break;
		}
	}

	// verify our target and set needed data.
	if (best.player && best.record && best.target) {
		// calculate aim angle.
		math::vector_angles(best.point.point - g_ctx.globals.eye_pos, m_angle);

		// set member vars.
		m_target = best.player;
		m_point = best.point;
		m_damage = best.damage;
		m_record = best.record;
		m_safe = best.safe;
		m_hitbox = best.hitbox;

		m_current_matrix = best.record->m_Matricies[MiddleMatrix].data();

		if (!m_target || m_target->IsDormant() || m_record->dormant || !m_current_matrix || !m_damage || !(m_damage >= best.min_damage || (m_damage <= best.min_damage && m_damage >= m_target->m_iHealth())))
			return;



		int nHitChance = g_ctx.globals.weapon->m_iItemDefinitionIndex() == WEAPON_TASER ? 50 : g_cfg.ragebot.weapon[g_ctx.globals.current_weapon].hitchance_amount;
		//float flCalculatedHitchance = this->CheckHitchance(m_target, m_angle, m_record, best.hitbox);

		bool flHitchanceValid = g_hit_chance->can_hit(m_record, g_ctx.globals.weapon, m_angle, best.hitbox);

		// autostop between shot (default).
		if (!this->m_stop)
		{
			if (g_ctx.globals.weapon->m_iItemDefinitionIndex() == WEAPON_TASER || g_cfg.ragebot.weapon[g_ctx.globals.current_weapon].autostop_modifiers[AUTOSTOP_BETWEEN_SHOT])
				this->m_stop = true;
			// autostop hitchance fail.
			else if (g_ctx.globals.weapon->m_iItemDefinitionIndex() != WEAPON_TASER && g_cfg.ragebot.weapon[g_ctx.globals.current_weapon].autostop_modifiers[AUTOSTOP_HITCHANCE_FAIL] && !flHitchanceValid)
				this->m_stop = true;


		}

		this->AutoStop(m_pcmd);

		// if we can scope.
		bool can_scope = !g_ctx.globals.weapon->m_zoomLevel() && g_ctx.globals.weapon->IsZoomable();
		if (can_scope)
		{
			// hitchance fail.
			if (!flHitchanceValid)
			{
				m_pcmd->m_buttons |= IN_ATTACK2;
				g_EnginePrediction->RePredict();
				return;
			}
		}

		if (flHitchanceValid)
		{

			if (g_cfg.ragebot.autoshoot)
			{
				m_pcmd->m_buttons |= IN_ATTACK;
				g_Ragebot->m_firing = true;

			}
		}
	}
}

bool aimbot::HasMaximumAccuracy()
{
	if (g_ctx.globals.weapon->is_non_aim(true))
		return false;

	if (g_ctx.local()->m_flVelocityModifier() < 1.0f)
		return false;

	float flDefaultInaccuracy = 0.0f;
	if (g_ctx.local()->m_fFlags() & FL_DUCKING)
		flDefaultInaccuracy = g_ctx.globals.scoped ? g_ctx.globals.weapon->get_csweapon_info()->flInaccuracyCrouchAlt : g_ctx.globals.weapon->get_csweapon_info()->flInaccuracyCrouch;
	else
		flDefaultInaccuracy = g_ctx.globals.scoped ? g_ctx.globals.weapon->get_csweapon_info()->flInaccuracyStandAlt : g_ctx.globals.weapon->get_csweapon_info()->flInaccuracyStand;

	return g_ctx.globals.inaccuracy - flDefaultInaccuracy < 0.0001f;
}

float aimbot::CheckHitchance(player_t* player, const Vector& angle, adjust_data* record, int hitbox) {
	//note - nico; you might wonder why I changed HITCHANCE_MAX:
	//I made it require more seeds (while not double tapping) to hit because it ensures better accuracy
	//while double tapping/using double tap it requires less seeds now, so we might shoot the 2nd shot more often <.<

	if (this->HasMaximumAccuracy())
		return 100.0f;

	constexpr int   SEED_MAX = 255;

	Vector start = g_ctx.globals.eye_pos, end, fwd, right, up, dir, wep_spread;
	float inaccuracy = g_ctx.globals.inaccuracy;
	float spread = g_ctx.globals.spread;
	float hitchance = g_ctx.globals.weapon->m_iItemDefinitionIndex() == WEAPON_TASER ? 50 : g_cfg.ragebot.weapon[g_ctx.globals.current_weapon].hitchance_amount;

	size_t total_hits = 0;

	// get needed directional vectors.
	math::angle_vectors(angle, &fwd, &right, &up);

	g_ctx.globals.weapon->update_accuracy_penality();

	// iterate all possible seeds.
	for (int i = 0; i <= SEED_MAX; ++i) {
		// get spread.
		wep_spread = g_ctx.globals.weapon->calculate_spread(i, inaccuracy, spread);

		// get spread direction.
		dir = (fwd + (right * wep_spread.x) + (up * wep_spread.y)).Normalized();

		// get end of trace.
		end = start + (dir * g_ctx.globals.weapon->get_csweapon_info()->flRange);

		// check if we hit a valid player / hitgroup on the player and increment total hits.
		if (this->CanHit(start, end, record, hitbox))
			++total_hits;

		// we made it.
		if ((SEED_MAX - i + total_hits) < (hitchance * 2.55f))
			return total_hits / 2.55f;
	}

	return total_hits / 2.55f;
}

bool AimPlayer::SetupHitboxPoints(adjust_data* record, matrix3x4_t* bones, int index, std::vector< HitscanPoint_t >& points) {
	// reset points.
	if (!points.empty())
		points.clear();

	if (g_ctx.globals.weapon->m_iItemDefinitionIndex() == WEAPON_TASER)
	{
		points.push_back({ record->player->hitbox_position_matrix(index, bones), index, true });
		return true;
	}

	const model_t* model = record->player->GetModel();
	if (!model)
		return false;

	studiohdr_t* hdr = m_modelinfo()->GetStudioModel(model);
	if (!hdr)
		return false;

	mstudiohitboxset_t* set = hdr->pHitboxSet(record->player->m_nHitboxSet());
	if (!set)
		return false;

	mstudiobbox_t* bbox = set->pHitbox(index);
	if (!bbox)
		return false;

	// get hitbox scales.
	float head_scale = g_cfg.ragebot.weapon[g_ctx.globals.current_weapon].head_scale;
	float body_scale = g_cfg.ragebot.weapon[g_ctx.globals.current_weapon].body_scale;

	// compute raw center point.
	Vector center = (bbox->bbmin + bbox->bbmax) / 2.f;
	Vector transformed_center = center;
	math::vector_transform(transformed_center, bones[bbox->bone], transformed_center);

	// calculate dynamic scale.
	if (!g_cfg.ragebot.weapon[g_ctx.globals.current_weapon].static_point_scale)
	{
		float spread = g_ctx.globals.inaccuracy + g_ctx.globals.spread;
		float distance = transformed_center.DistTo(g_ctx.globals.eye_pos);

		distance /= math::fast_sin(DEG2RAD(90.f - RAD2DEG(spread)));
		spread = math::fast_sin(spread);

		// get radius and set spread.
		float radius = max(bbox->radius - distance * spread, 0.f);
		head_scale = body_scale = math::clamp(radius / bbox->radius, 0.f, 1.f);
	}

	// these indexes represent boxes.
	if (bbox->radius <= 0.f)
	{
		// references: 
		//      https://developer.valvesoftware.com/wiki/Rotation_Tutorial
		//      CBaseAnimating::GetHitboxBonePosition
		//      CBaseAnimating::DrawServerHitboxes

		// convert rotation angle to a matrix.
		matrix3x4_t rot_matrix = math::angle_matrix(bbox->rotation);

		// apply the rotation to the entity input space (local).
		matrix3x4_t matrix;
		math::concat_transforms(bones[bbox->bone], rot_matrix, matrix);

		// extract origin from matrix.
		Vector origin = matrix.GetOrigin();

		// the feet hiboxes have a side, heel and the toe.
		if (index == HITBOX_RIGHT_FOOT || index == HITBOX_LEFT_FOOT) {
			float d1 = (bbox->bbmin.z - center.z) * 0.875f;

			// invert.
			if (index == HITBOX_LEFT_FOOT)
				d1 *= -1.f;

			// side is more optimal then center.
			points.emplace_back(HitscanPoint_t{ Vector(center.x, center.y, center.z + d1), index, false });

			if (g_cfg.ragebot.weapon[g_ctx.globals.current_weapon].multipoint_hitboxes.at(4))
			{
				// get point offset relative to center point
				// and factor in hitbox scale.
				float d2 = (bbox->bbmin.x - center.x) * body_scale;
				float d3 = (bbox->bbmax.x - center.x) * body_scale;

				// heel.
				points.emplace_back(HitscanPoint_t{ Vector(center.x + d2, center.y, center.z), index, false });

				// toe.
				points.emplace_back(HitscanPoint_t{ Vector(center.x + d3, center.y, center.z), index, false });
			}
		}

		// nothing to do here we are done.
		if (points.empty())
			return false;

		// rotate our bbox points by their correct angle
		// and convert our points to world space.
		for (HitscanPoint_t& p : points)
		{
			// VectorRotate.
			// rotate point by angle stored in matrix.
			p.point = { p.point.Dot(matrix[0]), p.point.Dot(matrix[1]), p.point.Dot(matrix[2]) };

			// transform point to world space.
			p.point += origin;
		}
	}
	else {
		points.emplace_back(HitscanPoint_t{ transformed_center, index, true });

		if (index >= HITBOX_RIGHT_THIGH)
			return true;

		float point_scale = 0.2f;

		if (index == HITBOX_HEAD) {
			if (!g_cfg.ragebot.weapon[g_ctx.globals.current_weapon].multipoint_hitboxes.at(0))
				return true;

			point_scale = head_scale;
		}

		if (index == HITBOX_LOWER_CHEST || index == HITBOX_CHEST) {
			if (!g_cfg.ragebot.weapon[g_ctx.globals.current_weapon].multipoint_hitboxes.at(2))
				return true;

			point_scale = body_scale;
		}

		if (index == HITBOX_PELVIS || index == HITBOX_STOMACH) {
			if (!g_cfg.ragebot.weapon[g_ctx.globals.current_weapon].multipoint_hitboxes.at(3))
				return true;

			point_scale = body_scale;
		}

		Vector min, max;
		math::vector_transform(bbox->bbmin, bones[bbox->bone], min);
		math::vector_transform(bbox->bbmax, bones[bbox->bone], max);

		RayTracer::Hitbox box(min, max, bbox->radius);
		RayTracer::Trace trace;

		Vector delta = (transformed_center - g_ctx.globals.eye_pos).Normalized();
		Vector max_min = (max - min).Normalized();
		Vector cr = max_min.Cross(delta);

		Vector delta_angle;
		math::vector_angles(delta, delta_angle);

		Vector right, up;
		if (index == HITBOX_HEAD) {
			Vector cr_angle;
			math::vector_angles(cr, cr_angle);
			cr_angle.to_vectors(right, up);
			cr_angle.z = delta_angle.x;

			Vector _up = up, _right = right, _cr = cr;
			cr = _right;
			right = _cr;
		}
		else
			math::VectorVectors(delta, up, right);

		if (index == HITBOX_HEAD) {
			Vector middle = (right.Normalized() + up.Normalized()) * 0.5f;
			Vector middle2 = (right.Normalized() - up.Normalized()) * 0.5f;

			RayTracer::TraceFromCenter(RayTracer::Ray(g_ctx.globals.eye_pos, transformed_center + (middle * 1000.f)), box, trace, RayTracer::Flags_RETURNEND);
			points.push_back(HitscanPoint_t{ trace.m_traceEnd, index, true });

			RayTracer::TraceFromCenter(RayTracer::Ray(g_ctx.globals.eye_pos, transformed_center - (middle2 * 1000.f)), box, trace, RayTracer::Flags_RETURNEND);
			points.push_back(HitscanPoint_t{ trace.m_traceEnd, index, true });

			RayTracer::TraceFromCenter(RayTracer::Ray(g_ctx.globals.eye_pos, transformed_center + (up * 1000.f)), box, trace, RayTracer::Flags_RETURNEND);
			points.push_back(HitscanPoint_t{ trace.m_traceEnd, index, false });

			RayTracer::TraceFromCenter(RayTracer::Ray(g_ctx.globals.eye_pos, transformed_center - (up * 1000.f)), box, trace, RayTracer::Flags_RETURNEND);
			points.push_back(HitscanPoint_t{ trace.m_traceEnd, index, false });
		}
		else {
			RayTracer::TraceFromCenter(RayTracer::Ray(g_ctx.globals.eye_pos, transformed_center + (up * 1000.f)), box, trace, RayTracer::Flags_RETURNEND);
			points.push_back(HitscanPoint_t{ trace.m_traceEnd, index, false });

			RayTracer::TraceFromCenter(RayTracer::Ray(g_ctx.globals.eye_pos, transformed_center - (up * 1000.f)), box, trace, RayTracer::Flags_RETURNEND);
			points.push_back(HitscanPoint_t{ trace.m_traceEnd, index, false });
		}

		// nothing left to do here.
		if (points.empty())
			return false;

		for (size_t i = 1; i < points.size(); ++i) {
			Vector delta_center = points[i].point - transformed_center;
			points[i].point = transformed_center + delta_center * point_scale;
		}
	}

	return true;
}

bool aimbot::IsSafePoint(adjust_data* LagRecord, Vector vecStartPosition, Vector vecEndPosition, int iHitbox)
{
	if (!this->CanHit(vecStartPosition, vecEndPosition, LagRecord, iHitbox, true, LagRecord->m_Matricies[LeftMatrix].data()))
		return false;

	if (!this->CanHit(vecStartPosition, vecEndPosition, LagRecord, iHitbox, true, LagRecord->m_Matricies[RightMatrix].data()))
		return false;

	return true;
}
bool aimbot::bTraceMeantForHitbox(const Vector& vecEyePosition, const Vector& vecEnd, int iHitbox, adjust_data* pRecord)
{
	// Initialize our trace data & information
	trace_t traceData = trace_t();
	Ray_t traceRay = Ray_t(vecEyePosition, vecEnd);

	// trace a ray to the entity
	m_trace()->ClipRayToCollideable(traceRay, MASK_SHOT, pRecord->player->GetCollideable(), &traceData);

	// check if trace did hit the desired hitbox and not other
	// example: aiming for head -> its behind his chest -> return chest == head
	return traceData.hitbox == iHitbox;
}
bool AimPlayer::GetBestAimPosition(HitscanPoint_t& point, float& damage, int& hitbox, bool& safe, adjust_data* record, float& tmp_min_damage) {
	bool                  done, pen;
	float                 dmg, pendmg;
	HitscanData_t         scan;
	std::vector< HitscanPoint_t > points;

	// get player hp.
	int hp = min(100, record->player->m_iHealth());
	auto force_safe_points = key_binds::get().get_key_bind_state(3) || g_cfg.ragebot.weapon[g_ctx.globals.current_weapon].max_misses && this->m_missed_shots >= g_cfg.ragebot.weapon[g_ctx.globals.current_weapon].max_misses_amount;

	m_matrix = record->m_Matricies[MiddleMatrix].data();

	if (!m_matrix)
		return false;

	if (g_ctx.globals.weapon->m_iItemDefinitionIndex() == WEAPON_TASER) {
		dmg = pendmg = hp;
		pen = true;
	}
	else {
		if (g_cfg.ragebot.weapon[g_ctx.globals.current_weapon].minimum_visible_damage > 100)
			dmg = hp + g_cfg.ragebot.weapon[g_ctx.globals.current_weapon].minimum_visible_damage - 100;
		else
			dmg = math::clamp(g_cfg.ragebot.weapon[g_ctx.globals.current_weapon].minimum_visible_damage, 1, hp);

		if (g_cfg.ragebot.weapon[g_ctx.globals.current_weapon].minimum_damage > 100)
			pendmg = hp + g_cfg.ragebot.weapon[g_ctx.globals.current_weapon].minimum_damage - 100;
		else
			pendmg = math::clamp(g_cfg.ragebot.weapon[g_ctx.globals.current_weapon].minimum_damage, 1, hp);

		if (key_binds::get().get_key_bind_state(4 + g_ctx.globals.current_weapon))
		{
			if (g_cfg.ragebot.weapon[g_ctx.globals.current_weapon].minimum_override_damage > 100)
				dmg = pendmg = hp + g_cfg.ragebot.weapon[g_ctx.globals.current_weapon].minimum_override_damage - 100;
			else
				dmg = pendmg = math::clamp(g_cfg.ragebot.weapon[g_ctx.globals.current_weapon].minimum_override_damage, 1, hp);
		}

		pen = g_cfg.ragebot.autowall;
	}

	// backup player
	const auto backup_origin = record->player->m_vecOrigin();
	const auto backup_abs_origin = record->player->GetAbsOrigin();
	const auto backup_abs_angles = record->player->GetAbsAngles();
	const auto backup_obb_mins = record->player->m_vecMins();
	const auto backup_obb_maxs = record->player->m_vecMaxs();
	const auto backup_cache = record->player->m_CachedBoneData().Base();

	auto restore = [&]() -> void {
		record->player->m_vecOrigin() = backup_origin;
		record->player->set_abs_origin(backup_abs_origin);
		record->player->set_abs_angles(backup_abs_angles);
		record->player->UpdateCollisionBounds();
		record->player->SetCollisionBounds(backup_obb_mins, backup_obb_maxs);
		std::memcpy(record->player->m_CachedBoneData().Base(), backup_cache, record->player->m_CachedBoneData().Count() * sizeof(matrix3x4_t));
	};

	// iterate hitboxes.
	for (const auto& it : m_hitboxes) {
		done = false;

		// setup points on hitbox.
		if (!SetupHitboxPoints(record, m_matrix, it.m_index, points))
			continue;



		// iterate points on hitbox.
		for (const auto& point : points) {

			if (g_cfg.player.show_multi_points)
				g_Ragebot->m_debug_points.emplace_back(point);

			penetration::PenetrationInput_t in;

			in.m_damage = dmg;
			in.m_damage_pen = pendmg;
			in.m_can_pen = pen;
			in.m_target = record->player;
			in.m_from = g_ctx.local();
			in.m_pos = point.point;

			penetration::PenetrationOutput_t out;

			//credits:- https://www.unknowncheats.me/forum/counterstrike-global-offensive/596435-prevent-bodyaim-head.html
			/*if (!g_Ragebot->bTraceMeantForHitbox(g_ctx.globals.eye_pos, point.point, it.m_index, record))
				continue;*/

			// code for safepoint matrix, the point should (!) be a safe point.
			bool safe =  g_Ragebot->IsSafePoint(record, g_ctx.globals.eye_pos, point.point, it.m_index);

			// don't run the code if our user want to force the safepoint and the target is not safe to hit.
			if (force_safe_points && !safe)
				continue;

			// setup trace data
			record->player->m_vecOrigin() = record->origin;
			record->player->set_abs_origin(record->origin);
			record->player->set_abs_angles(record->abs_angles);
			record->player->UpdateCollisionBounds();
			record->player->SetCollisionBounds(record->mins, record->maxs);
			std::memcpy(record->player->m_CachedBoneData().Base(), m_matrix, sizeof(m_matrix));

			// we can hit p!
			if (penetration::get().run(&in, &out)) {
				tmp_min_damage = dmg;

				// restore trace data
				restore();
				
				// nope we did not hit head..
				if (it.m_index == HITBOX_HEAD && out.m_hitgroup != HITGROUP_HEAD)
					continue;

				// prefered hitbox, just stop now.
				if (it.m_mode == HitscanMode::PREFER)
					done = true;

				// this hitbox requires lethality to get selected, if that is the case.
				// we are done, stop now.
				else if (it.m_mode == HitscanMode::LETHAL && out.m_damage >= record->player->m_iHealth())
					done = true;

				// always prefer safe points if we want to.
				else if (it.m_mode == HitscanMode::PREFER_SAFEPOINT && safe)
					done = true;

				// this hitbox has normal selection, it needs to have more damage.
				else if (it.m_mode == HitscanMode::NORMAL) {
					// we did more damage.
					if (out.m_damage > scan.m_damage) {
						if (! g_Ragebot->m_stop && g_ctx.globals.weapon->m_iItemDefinitionIndex() != WEAPON_TASER)
						{
							if (g_cfg.ragebot.weapon[g_ctx.globals.current_weapon].autostop_modifiers[AUTOSTOP_CENTER] && point.center)
								 g_Ragebot->m_stop = true;
							else if (g_cfg.ragebot.weapon[g_ctx.globals.current_weapon].autostop_modifiers[AUTOSTOP_LETHAL] && out.m_damage < record->player->m_iHealth())
								 g_Ragebot->m_stop = true;
						}

						// save new best data.
						scan.m_damage = out.m_damage;
						scan.m_point = point;
						scan.m_hitbox = it.m_index;
						scan.m_safepoint = it.m_safepoint;

						// if the first point is lethal
						// screw the other ones.
						if (point.point == points.front().point && out.m_damage >= record->player->m_iHealth())
							break;
					}
				}

				// we found a preferred / lethal hitbox.
				if (done) {
					if (!g_Ragebot->m_stop && g_ctx.globals.weapon->m_iItemDefinitionIndex() != WEAPON_TASER && g_cfg.ragebot.weapon[g_ctx.globals.current_weapon].autostop_modifiers[AUTOSTOP_CENTER] && point.center)
						g_Ragebot->m_stop = true;
					if (!g_Ragebot->m_stop && g_ctx.globals.weapon->m_iItemDefinitionIndex() != WEAPON_TASER && g_cfg.ragebot.weapon[g_ctx.globals.current_weapon].autostop_modifiers[AUTOSTOP_LETHAL] && out.m_damage < record->player->m_iHealth())
						g_Ragebot->m_stop = true;

					// save new best data.
					scan.m_damage = out.m_damage;
					scan.m_point = point;
					scan.m_hitbox = it.m_index;
					scan.m_safepoint = it.m_mode == HitscanMode::PREFER && it.m_safepoint;
					scan.m_mode = it.m_mode;




					g_Ragebot->m_working = true;

					break;
				}
				else
					g_Ragebot->m_working = false;
			}
			else
			{
				// restore trace data.
				restore();
			}
		}

		// ghetto break out of outer loop.
		if (done)
			break;
	}

	// we found something that we can damage.
	// set out vars.
	if (scan.m_damage > 0.f)
	{
		point = scan.m_point;
		damage = scan.m_damage;
		hitbox = scan.m_hitbox;
		safe = scan.m_safepoint;
		g_Ragebot->m_current_matrix = m_matrix;
		
		return true;
	}

	return false;
}

bool aimbot::SelectTarget(adjust_data* record, const HitscanPoint_t& point, float damage) {
	Vector engine_angles;
	float dist, fov, height;
	int   hp;

	m_engine()->GetViewAngles(engine_angles);

	// fov check.
	if (g_cfg.ragebot.field_of_view > 0) {
		// if out of fov, retn false.

		if (math::get_fov(engine_angles, math::calculate_angle(g_ctx.globals.eye_pos, point.point)) > g_cfg.ragebot.field_of_view)
			return false;
	}

	switch (g_cfg.ragebot.weapon[g_ctx.globals.current_weapon].selection_type) {

		// distance.
	case 0:
		dist = g_ctx.globals.eye_pos.DistTo(record->origin);

		if (dist < m_best_dist) {
			m_best_dist = dist;
			return true;
		}

		break;

		// crosshair.
	case 1:
		fov = math::get_fov(engine_angles, math::calculate_angle(g_ctx.globals.eye_pos, point.point));

		if (fov < m_best_fov) {
			m_best_fov = fov;
			return true;
		}

		break;

		// damage.
	case 2:
		if (damage > m_best_damage) {
			m_best_damage = damage;
			return true;
		}

		break;

		// lowest hp.
	case 3:
		// fix for retarded servers?
		hp = min(100, record->player->m_iHealth());

		if (hp < m_best_hp) {
			m_best_hp = hp;
			return true;
		}

		break;

		// least lag.
	case 4:
		if (record->m_nSimulationTicks < m_best_lag) {
			m_best_lag = record->m_nSimulationTicks;
			return true;
		}

		break;

		// height.
	case 5:
		height = record->origin.z - g_ctx.local()->m_vecOrigin().z;

		if (height < m_best_height) {
			m_best_height = height;
			return true;
		}

		break;

	default:
		return false;
	}

	return false;
}

void aimbot::apply(CUserCmd* m_pcmd) {
	if (!(m_pcmd->m_buttons & IN_ATTACK))
		return;

	if (this->m_target)
	{
		if (this->m_record)
			m_pcmd->m_tickcount = TIME_TO_TICKS(m_record->simulation_time + util::get_interpolation());

		m_pcmd->m_viewangles = this->m_angle;

		if (!g_cfg.ragebot.silent_aim)
			m_engine()->SetViewAngles(this->m_angle);

		//if (g_cfg.player.lag_hitbox && this->m_record)
			//hit_chams::get().add_matrix(this->m_record);

		shots::get().register_shot(this->m_target, g_ctx.globals.eye_pos, this->m_record, m_globals()->m_tickcount, this->m_point, this->m_safe);
	}
	if (g_Misc->IsFiring())
		g_ctx.globals.aimbot_shooting = true;

	g_ctx.globals.revolver_working = false;
	g_ctx.globals.last_aimbot_shot = m_globals()->m_tickcount;
	g_ctx.globals.shot_command = m_pcmd->m_command_number;
}


bool aimbot::CanHit(Vector start, Vector end, adjust_data* record, int box, bool in_shot, matrix3x4_t* bones)
{
	if (!record || !record->player)
		return false;

	// backup player
	const auto backup_origin = record->player->m_vecOrigin();
	const auto backup_abs_origin = record->player->GetAbsOrigin();
	const auto backup_abs_angles = record->player->GetAbsAngles();
	const auto backup_obb_mins = record->player->m_vecMins();
	const auto backup_obb_maxs = record->player->m_vecMaxs();
	const auto backup_cache = record->player->m_CachedBoneData().Base();

	// always try to use our aimbot matrix first.
	auto matrix = this->m_current_matrix;

	// this is basically for using a custom matrix.
	if (in_shot)
		matrix = bones;

	if (!matrix)
		return false;

	const model_t* model = record->player->GetModel();
	if (!model)
		return false;

	studiohdr_t* hdr = m_modelinfo()->GetStudioModel(model);
	if (!hdr)
		return false;

	mstudiohitboxset_t* set = hdr->pHitboxSet(record->player->m_nHitboxSet());
	if (!set)
		return false;

	mstudiobbox_t* bbox = set->pHitbox(box);
	if (!bbox)
		return false;

	Vector min, max;
	const auto IsCapsule = bbox->radius != -1.f;

	if (IsCapsule) {
		math::vector_transform(bbox->bbmin, matrix[bbox->bone], min);
		math::vector_transform(bbox->bbmax, matrix[bbox->bone], max);
		const auto dist = math::segment_to_segment(start, end, min, max);

		if (dist < bbox->radius) {
			return true;
		}
	}
	else {
		CGameTrace tr;

		// setup trace data
		record->player->m_vecOrigin() = record->origin;
		record->player->set_abs_origin(record->origin);
		record->player->set_abs_angles(record->abs_angles);
		record->player->UpdateCollisionBounds();
		record->player->SetCollisionBounds(record->mins, record->maxs);
		std::memcpy(record->player->m_CachedBoneData().Base(), matrix, sizeof(matrix));

		// setup ray and trace.
		m_trace()->ClipRayToEntity(Ray_t(start, end), MASK_SHOT, record->player, &tr);

		record->player->m_vecOrigin() = backup_origin;
		record->player->set_abs_origin(backup_abs_origin);
		record->player->set_abs_angles(backup_abs_angles);
		record->player->UpdateCollisionBounds();
		record->player->SetCollisionBounds(backup_obb_mins, backup_obb_maxs);
		std::memcpy(record->player->m_CachedBoneData().Base(), backup_cache, record->player->m_CachedBoneData().Count() * sizeof(matrix3x4_t));

		// check if we hit a valid player_t / hitgroup on the player_t and increment total hits.
		if (tr.hit_entity == record->player && util::is_valid_hitgroup(tr.hitgroup))
			return true;
	}

	return false;
}






void hit_chance::build_seed_table() {
	constexpr float pi_2 = 2.0f * (float)M_PI;
	for (size_t i = 0; i < 256; ++i) {
		math::random_seed(i);

		const float rand_a = math::random_float(0.0f, 1.0f);
		const float rand_pi_a = math::random_float(0.0f, pi_2);
		const float rand_b = math::random_float(0.0f, 1.0f);
		const float rand_pi_b = math::random_float(0.0f, pi_2);

		hit_chance_records[i] = {
			{  rand_a, rand_b                                 },
			{  std::cos(rand_pi_a), std::sin(rand_pi_a)     },
			{  std::cos(rand_pi_b), std::sin(rand_pi_b)     }
		};
	}
}

bool hit_chance::intersects_bb_hitbox(Vector start, Vector delta, Vector min, Vector max) {
	float d1, d2, f;
	auto start_solid = true;
	auto t1 = -1.0, t2 = 1.0;

	const float _start[3] = { start.x, start.y, start.z };
	const float _delta[3] = { delta.x, delta.y, delta.z };
	const float mins[3] = { min.x, min.y, min.z };
	const float maxs[3] = { max.x, max.y, max.z };

	for (auto i = 0; i < 6; ++i) {
		if (i >= 3) {
			const auto j = (i - 3);

			d1 = _start[j] - maxs[j];
			d2 = d1 + _delta[j];
		}
		else {
			d1 = -_start[i] + mins[i];
			d2 = d1 - _delta[i];
		}

		if (d1 > 0 && d2 > 0) {
			start_solid = false;
			return false;
		}

		if (d1 <= 0 && d2 <= 0)
			continue;

		if (d1 > 0)
			start_solid = false;

		if (d1 > d2) {
			f = d1;
			if (f < 0)
				f = 0;

			f /= d1 - d2;
			if (f > t1)
				t1 = f;
		}
		else {
			f = d1 / (d1 - d2);
			if (f < t2)
				t2 = f;
		}
	}

	return start_solid || (t1 < t2&& t1 >= 0.0f);
}

bool __vectorcall hit_chance::intersects_hitbox(Vector eye_pos, Vector end_pos, Vector min, Vector max, float radius) {
	auto dist = math::segment_to_segment(eye_pos, end_pos, min, max);

	return (dist < radius);
}

std::vector<hit_chance::hitbox_data_t> hit_chance::get_hitbox_data(adjust_data* log, int hitbox) {
	std::vector<hitbox_data_t> hitbox_data;
	auto target = static_cast<player_t*>(m_entitylist()->GetClientEntity(log->player->EntIndex()));

	const auto model = target->GetClientRenderable()->GetModel();

	if (!model)
		return {};


	auto hdr = m_modelinfo()->GetStudioModel(model);

	if (!hdr)
		return {};

	auto set = hdr->pHitboxSet(log->player->m_nHitboxSet());

	if (!set)
		return {};

	//we use 128 bones that not proper use aim matrix there
	auto bone_matrix = log->m_Matricies[MatrixBoneSide::MiddleMatrix].data();

	Vector min, max;

	if (hitbox == -1) {
		for (int i = 0; i < set->numhitboxes; ++i) {
			const auto box = set->pHitbox(i);

			if (!box)
				continue;

			float radius = box->radius;
			const auto is_capsule = radius != -1.f;

			if (is_capsule) {
				math::vector_transform(box->bbmin, bone_matrix[box->bone], min);
				math::vector_transform(box->bbmax, bone_matrix[box->bone], max);
			}
			else {
				math::vector_transform(math::vector_rotate(box->bbmin, box->rotation), bone_matrix[box->bone], min);
				math::vector_transform(math::vector_rotate(box->bbmax, box->rotation), bone_matrix[box->bone], max);
				radius = min.DistTo(max);
			}

			hitbox_data.emplace_back(hitbox_data_t{ min, max, radius, box, box->bone, box->rotation });
		}
	}
	else {
		const auto box = set->pHitbox(hitbox);

		if (!box)
			return {};

		float radius = box->radius;
		const auto is_capsule = radius != -1.f;

		if (is_capsule) {
			math::vector_transform(box->bbmin, bone_matrix[box->bone], min);
			math::vector_transform(box->bbmax, bone_matrix[box->bone], max);
		}
		else {
			math::vector_transform(math::vector_rotate(box->bbmin, box->rotation), bone_matrix[box->bone], min);
			math::vector_transform(math::vector_rotate(box->bbmax, box->rotation), bone_matrix[box->bone], max);
			radius = min.DistTo(max);
		}

		hitbox_data.emplace_back(hitbox_data_t{ min, max, radius, box, box->bone, box->rotation });
	}

	return hitbox_data;
}

Vector hit_chance::get_spread_direction(weapon_t* weapon, Vector angles, int seed) {
	if (!weapon)
		return Vector();

	const int   rnsd = (seed & 0xFF);
	const auto* data = &hit_chance_records[rnsd];

	if (!data)
		return Vector();

	float rand_a = data->random[0];
	float rand_b = data->random[1];

	if (weapon->m_iItemDefinitionIndex() == WEAPON_NEGEV) {
		auto weapon_info = weapon ? g_ctx.globals.weapon->get_csweapon_info() : nullptr;

		if (weapon_info && weapon_info->iRecoilSeed < 3) {
			rand_a = 1.0f - std::pow(rand_a, static_cast<float>(3 - weapon_info->iRecoilSeed + 1));
			rand_b = 1.0f - std::pow(rand_b, static_cast<float>(3 - weapon_info->iRecoilSeed + 1));
		}
	}

	//must write from predition
	const float rand_inaccuracy = rand_a * g_ctx.globals.inaccuracy;
	const float rand_spread = rand_b * g_ctx.globals.spread;

	const float spread_x = data->inaccuracy[0] * rand_inaccuracy + data->spread[0] * rand_spread;
	const float spread_y = data->inaccuracy[1] * rand_inaccuracy + data->spread[1] * rand_spread;

	Vector forward, right, up;
	math::angle_vectors(angles, &forward, &right, &up);

	return forward + right * spread_x + up * spread_y;
}

bool hit_chance::can_intersect_hitbox(const Vector start, const Vector end, Vector spread_dir, adjust_data* log, int hitbox)
{
	const auto hitbox_data = get_hitbox_data(log, hitbox);

	if (hitbox_data.empty())
		return false;

	auto intersected = false;
	Vector delta;
	Vector start_scaled;

	for (const auto& it : hitbox_data) {
		const auto is_capsule = it.m_radius != -1.f;
		if (!is_capsule) {
			math::vector_i_transform(start, log->m_Matricies[MatrixBoneSide::MiddleMatrix].data()[it.m_bone], start_scaled);
			math::vector_i_rotate(spread_dir * 8192.f, log->m_Matricies[MatrixBoneSide::MiddleMatrix].data()[it.m_bone], delta);
			if (intersects_bb_hitbox(start_scaled, delta, it.m_min, it.m_max)) {
				intersected = true;
				break; //note - AkatsukiSun: cannot hit more than one hitbox.
			}
		}
		else if (intersects_hitbox(start, end, it.m_min, it.m_max, it.m_radius)) {
			intersected = true;
			break;//note - AkatsukiSun: cannot hit more than one hitbox.
		}
		else {
			intersected = false;
			break;
		}
	}

	return intersected;
}

bool hit_chance::can_hit(adjust_data* log, weapon_t* weapon, Vector angles, int hitbox) {

	auto target = static_cast<player_t*>(m_entitylist()->GetClientEntity(log->player->EntIndex()));

	if (!target || !weapon)
		return false;

	auto weapon_info = weapon ? g_ctx.globals.weapon->get_csweapon_info() : nullptr;

	if (!weapon_info)
		return false;

	if (g_ctx.globals.isshifting)
		return true;

	build_seed_table();

	if ((weapon->m_iItemDefinitionIndex() == WEAPON_SSG08 || weapon->m_iItemDefinitionIndex() == WEAPON_REVOLVER) && !(g_ctx.local()->m_fFlags() & FL_ONGROUND)) {
		if ((g_ctx.globals.inaccuracy < 0.009f)) {
			return true;
		}
	}

	const Vector eye_pos = g_ctx.local()->GetEyePosition();
	Vector start_scaled = { };
	const auto hitchance_cfg = g_cfg.ragebot.weapon[g_ctx.globals.current_weapon].hitchance_amount;
	const int hits_needed = (hitchance_cfg * 256) / 100;
	int hits = 0;

	for (int i = 0; i < 256; ++i) {

		Vector spread_dir = get_spread_direction(weapon, angles, i);
		Vector end_pos = eye_pos + (spread_dir * 8192.f);

		if (can_intersect_hitbox(eye_pos, end_pos, spread_dir, log, hitbox))
			hits++;

		if (hits >= hits_needed)
			return true;
	}

	return false;
}