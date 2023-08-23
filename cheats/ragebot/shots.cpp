// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: http://www.viva64.com

#include "shots.h"
#include "..\misc\logs.h"
#include "..\visuals\other_esp.h"
#include "../prediction/Networking.h"

void shots::register_shot(player_t* target, Vector eye_pos, adjust_data* record, int fire_tick, HitscanPoint_t point, bool safe)
{
	auto shot = &this->m_shots.emplace_back();

	shot->target = target;
	shot->record = record;
	shot->fire_tick = fire_tick;
	shot->shot_info.safe = safe;
	shot->shot_info.point = point;
	shot->eye_pos = eye_pos;
	shot->distance = eye_pos.DistTo(point.point);

	if (target)
	{
		//AimPlayer* data = & g_Ragebot->m_players[target->EntIndex()];
		++g_AimPlayerData->m_shots;
	}
}

void shots::on_fsn()
{
	auto current_shot = this->m_shots.end();
	auto latency = g_Networking->latency + 1.0f;

	for (auto& shot = this->m_shots.begin(); shot != this->m_shots.end(); ++shot)
	{
		if (shot->end)
		{
			current_shot = shot;
			break;
		}
		else if (shot->impacts && m_globals()->m_tickcount - 1 > shot->event_fire_tick)
		{
			current_shot = shot;
			current_shot->end = true;
			break;
		}
		else if (m_globals()->m_tickcount - TIME_TO_TICKS(latency) > shot->fire_tick)
		{
			current_shot = shot;
			current_shot->end = true;
			current_shot->latency = true;
			break;
		}
	}

	if (current_shot == this->m_shots.end())
		return;

	/*auto data = &g_Ragebot->m_players[current_shot->target->EntIndex()];

	if (!data)
		return;*/

	if (!current_shot->latency)
	{
		current_shot->shot_info.should_log = true;

		if (!current_shot->hurt_player)
		{
			if (current_shot->impact_hit_player)
			{
				g_ctx.globals.missed_shots[current_shot->target->EntIndex()]++;

				if (g_cfg.misc.events_to_log[EVENTLOG_HIT])
					eventlogs::get().add(crypt_str("missed shot due to resolver"), true, Color(255, 100, 100));
			}
			else if (g_cfg.misc.events_to_log[EVENTLOG_HIT])
			{
				if (current_shot->occlusion)
					eventlogs::get().add(crypt_str("missed shot due to occlusion"), true, Color(255, 115, 0));
				else
					eventlogs::get().add(crypt_str("missed shot due to spread"), true, Color(255, 210, 70));
			}
		}
	}

	if (current_shot->shot_info.should_log)
		current_shot->shot_info.should_log = false;

	this->m_shots.erase(current_shot);
}

void shots::on_impact(Vector impactpos)
{
	if (impactpos.IsZero())
		return;

	shot_record* current_shot = nullptr;

	for (auto& shot : this->m_shots)
	{
		if (!shot.start)
			continue;

		if (shot.end)
			continue;

		current_shot = &shot;
		break;
	}

	if (!current_shot)
		return;

	if (!current_shot->target->valid(true))
		return;

	trace_t trace, trace_zero, trace_first, trace_second;
	m_trace()->ClipRayToEntity(Ray_t(current_shot->eye_pos, impactpos), MASK_SHOT_HULL | CONTENTS_HITBOX, current_shot->target, &trace);

	if (!current_shot->record->bot && current_shot->shot_info.safe)
	{
		const auto backup_bone_cache = current_shot->target->m_CachedBoneData().Base();

		std::memcpy(current_shot->target->m_CachedBoneData().Base(), current_shot->record->m_Matricies[LeftMatrix].data(), current_shot->target->m_CachedBoneData().Count() * sizeof(matrix3x4_t)); //-V807
		m_trace()->ClipRayToEntity(Ray_t(current_shot->eye_pos, impactpos), MASK_SHOT_HULL | CONTENTS_HITBOX, current_shot->target, &trace_first);

		std::memcpy(current_shot->target->m_CachedBoneData().Base(), current_shot->record->m_Matricies[RightMatrix].data(), current_shot->target->m_CachedBoneData().Count() * sizeof(matrix3x4_t)); //-V807
		m_trace()->ClipRayToEntity(Ray_t(current_shot->eye_pos, impactpos), MASK_SHOT_HULL | CONTENTS_HITBOX, current_shot->target, &trace_second);

		std::memcpy(current_shot->target->m_CachedBoneData().Base(), current_shot->record->m_Matricies[ZeroMatrix].data(), current_shot->target->m_CachedBoneData().Count() * sizeof(matrix3x4_t)); //-V807
		m_trace()->ClipRayToEntity(Ray_t(current_shot->eye_pos, impactpos), MASK_SHOT_HULL | CONTENTS_HITBOX, current_shot->target, &trace_zero);

		std::memcpy(current_shot->target->m_CachedBoneData().Base(), backup_bone_cache, current_shot->target->m_CachedBoneData().Count() * sizeof(matrix3x4_t)); //-V807
	}

	auto hit = trace.hit_entity == current_shot->target;

	if (!current_shot->record->bot && current_shot->shot_info.safe)
		hit = hit && trace_zero.hit_entity == current_shot->target && trace_first.hit_entity == current_shot->target && trace_second.hit_entity == current_shot->target;

	if (hit)
		current_shot->impact_hit_player = true;
	else if (current_shot->eye_pos.DistTo(impactpos) < current_shot->distance)
		current_shot->occlusion = true;
	else
		current_shot->occlusion = false;

	current_shot->impacts = true;
}

void shots::on_weapon_fire()
{
	shot_record* current_shot = nullptr;

	for (auto& shot : this->m_shots)
	{
		if (shot.start)
		{
			shot.end = true;
			continue;
		}

		if (shot.end)
			continue;

		current_shot = &shot;
		break;
	}

	if (!current_shot)
		return;

	current_shot->start = true;
	current_shot->event_fire_tick = m_globals()->m_tickcount;
}

bool weapon_is_aim(const std::string& weapon)
{
	return weapon.find(crypt_str("decoy")) == std::string::npos && weapon.find(crypt_str("flashbang")) == std::string::npos &&
		weapon.find(crypt_str("hegrenade")) == std::string::npos && weapon.find(crypt_str("inferno")) == std::string::npos &&
		weapon.find(crypt_str("molotov")) == std::string::npos && weapon.find(crypt_str("smokegrenade")) == std::string::npos;
}

void shots::on_player_hurt(IGameEvent* event, int user_id)
{
	auto entity = (player_t*)m_entitylist()->GetClientEntity(user_id);
	auto damage = event->GetInt(crypt_str("dmg_health"));
	auto hitgroup = event->GetInt(crypt_str("hitgroup"));

	std::string weapon = event->GetString(crypt_str("weapon"));

	auto headshot = hitgroup == HITGROUP_HEAD;
	auto health = entity->m_iHealth() - damage;

	if (health <= 0 && headshot)
		otheresp::get().hitmarker.hurt_color = Color::Red;
	else if (health <= 0)
		otheresp::get().hitmarker.hurt_color = Color::Yellow;
	else
		otheresp::get().hitmarker.hurt_color = Color::White;

	otheresp::get().damage_marker[user_id].hurt_color = headshot ? Color::Red : Color::White;

	shot_record* current_shot = nullptr;

	for (auto& shot : this->m_shots)
	{
		if (!shot.start)
			continue;

		if (shot.end)
			continue;

		current_shot = &shot;
		break;
	}

	if (weapon_is_aim(weapon) && current_shot)
	{
		otheresp::get().hitmarker.hurt_time = m_globals()->m_curtime;
		otheresp::get().hitmarker.point = entity->hitbox_position_matrix(util::get_hitbox_by_hitgroup(hitgroup), current_shot && entity == current_shot->target ? current_shot->record->m_Matricies[MiddleMatrix].data() : entity->m_CachedBoneData().Base());
		Color result;

		if (hitgroup == HITGROUP_HEAD)
			result = Color::Red;
		else if (hitgroup == HITGROUP_CHEST)
			result = Color::Yellow;
		else
			result = Color::White;

		otheresp::get().damage_marker[user_id] = otheresp::Damage_marker
		{
			entity->hitbox_position_matrix(util::get_hitbox_by_hitgroup(hitgroup), current_shot && entity == current_shot->target ? current_shot->record->m_Matricies[MiddleMatrix].data() : entity->m_CachedBoneData().Base()),
			m_globals()->m_curtime,
			result,
			damage,
			hitgroup
		};
	}

	if (!current_shot)
		return;

	current_shot->end = true;
	current_shot->hurt_player = true;
}

void shots::clear_stored_data()
{
	if (!this->m_shots.empty())
		this->m_shots.clear();
}