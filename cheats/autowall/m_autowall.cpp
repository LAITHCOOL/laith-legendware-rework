#include "../autowall/m_autowall.h"

namespace m_gAutowall
{
	bool c_auto_wall::IsBreakableEntity(IClientEntity* e)
	{
		if (!e || !e->EntIndex())
			return false;

		static auto is_breakable = util::FindSignature(crypt_str("client.dll"), crypt_str("55 8B EC 51 56 8B F1 85 F6 74 68"));

		auto take_damage = *(uintptr_t*)((uintptr_t)is_breakable + 0x26);
		auto backup = *(uint8_t*)((uintptr_t)e + take_damage);

		auto client_class = e->GetClientClass();
		auto network_name = client_class->m_pNetworkName;

		if (!strcmp(network_name, "CBreakableSurface"))
			*(uint8_t*)((uintptr_t)e + take_damage) = DAMAGE_YES;
		else if (!strcmp(network_name, "CBaseDoor") || !strcmp(network_name, "CDynamicProp"))
			*(uint8_t*)((uintptr_t)e + take_damage) = DAMAGE_NO;

		using Fn = bool(__thiscall*)(IClientEntity*);
		auto result = ((Fn)is_breakable)(e);


		*(uint8_t*)((uintptr_t)e + take_damage) = backup;
		return result;
	}

	bool c_auto_wall::trace_to_exit(
		const Vector& src, const Vector& dir,
		const trace_t& enter_trace, trace_t& exit_trace
	) const
	{
		float dist{};
		e_mask first_contents{};

		constexpr auto k_step_size = 4.f;
		constexpr auto k_max_dist = 90.f;

		while (dist <= k_max_dist)
		{
			dist += k_step_size;

			const auto out = src + (dir * dist);

			const auto cur_contents = m_trace()->GetPointContents(out, e_mask::shot_player);

			if (!first_contents)
				first_contents = cur_contents;

			if (cur_contents & e_mask::shot_hull
				&& (!(cur_contents & e_mask::contents_hitbox) || cur_contents == first_contents))
				continue;

			m_trace()->TraceRay({ out, out - dir * k_step_size }, e_mask::shot_player, nullptr, &exit_trace);

			if (exit_trace.startsolid
				&& exit_trace.surface.flags & e_mask::surf_hitbox)
			{
				CTraceFilter trace_filter{ exit_trace.hit_entity, 0 };

				m_trace()->TraceRay(
					{ out, src }, e_mask::shot_hull,
					reinterpret_cast<CTraceFilter*>(&trace_filter), &exit_trace
				);

				if (exit_trace.DidHit()
					&& !exit_trace.startsolid)
					return true;

				continue;
			}

			if (!exit_trace.DidHit()
				|| exit_trace.startsolid)
			{
				if (enter_trace.hit_entity
					&& enter_trace.hit_entity->EntIndex()
					&& IsBreakableEntity(enter_trace.hit_entity) enter_trace.hit_entity->breakable())
				{
					exit_trace = enter_trace;
					exit_trace.endpos = src + dir;

					return true;
				}

				continue;
			}

			if (exit_trace.surface.m_flags & e_mask::surf_nodraw)
			{
				if (exit_trace.m_hit_entity->breakable()
					&& enter_trace.m_hit_entity->breakable())
					return true;

				if (!(enter_trace.m_surface.m_flags & e_mask::surf_nodraw))
					continue;
			}

			if (exit_trace.m_plane.m_normal.dot(dir) <= 1.f)
				return true;
		}

		return false;
	}

	void c_auto_wall::clip_trace_to_player(
		const Vector& src, const Vector& dst, trace_t& trace,
		player_t* const player, const valve::should_hit_fn_t& should_hit_fn
	) const
	{
		if (should_hit_fn
			&& !should_hit_fn(player, e_mask::shot_player))
			return;

		const auto pos = player->origin() + (player->obb_min() + player->obb_max()) * 0.5f;
		const auto to = pos - src;

		auto dir = src - dst;
		const auto len = dir.normalize();
		const auto range_along = dir.dot(to);

		const auto range =
			range_along < 0.f ? -(to).length()
			: range_along > len ? -(pos - dst).length()
			: (pos - (src + dir * range_along)).length();

		if (range > 60.f)
			return;

		trace_t new_trace{};

		m_trace()->clip_ray_to_entity({ src, dst }, e_mask::shot_player, player, &new_trace);

		if (new_trace.m_fraction > trace.m_fraction)
			return;

		trace = new_trace;
	}

	bool c_auto_wall::handle_bullet_penetration(
		player_t* const shooter, const weapon_info_t* const wpn_data,
		const trace_t& enter_trace, Vector& src, const Vector& dir, int& pen_count,
		float& cur_dmg, const float pen_modifier
	) const
	{
		if (pen_count <= 0
			|| wpn_data->m_penetration <= 0.f)
			return false;

		trace_t exit_trace{};

		if (!trace_to_exit(enter_trace.m_end_pos, dir, enter_trace, exit_trace)
			&& !(m_trace()->point_contents(enter_trace.m_end_pos, e_mask::shot_hull) & e_mask::shot_hull))
			return false;

		auto final_dmg_modifier = 0.16f;
		float combined_pen_modifier{};

		const auto exit_surface_data = valve::g_surface_data->find(exit_trace.m_surface.m_surface_props);
		const auto enter_surface_data = valve::g_surface_data->find(enter_trace.m_surface.m_surface_props);

		if (enter_surface_data->m_game.m_material == 'G'
			|| enter_surface_data->m_game.m_material == 'Y')
		{
			final_dmg_modifier = 0.05f;
			combined_pen_modifier = 3.f;
		}
		else if (enter_trace.m_contents & e_mask::contents_grate
			|| enter_trace.m_surface.m_flags & e_mask::surf_nodraw)
		{
			final_dmg_modifier = 0.16f;
			combined_pen_modifier = 1.f;
		}
		else if (enter_trace.m_hit_entity
			&& g_context->cvars().m_ff_dmg_reduction_bullets->get_float() == 0.f
			&& enter_surface_data->m_game.m_material == 'F'
			&& enter_trace.m_hit_entity->team() == shooter->team())
		{
			const auto dmg_bullet_pen = g_context->cvars().m_ff_dmg_bullet_pen->get_float();
			if (dmg_bullet_pen == 0.f)
				return false;

			combined_pen_modifier = dmg_bullet_pen;
			final_dmg_modifier = 0.16f;
		}
		else
		{
			combined_pen_modifier = (
				enter_surface_data->m_game.m_penetration_modifier
				+ exit_surface_data->m_game.m_penetration_modifier
				) * 0.5f;

			final_dmg_modifier = 0.16f;
		}

		if (enter_surface_data->m_game.m_material == exit_surface_data->m_game.m_material)
		{
			if (exit_surface_data->m_game.m_material == 'U'
				|| exit_surface_data->m_game.m_material == 'W')
				combined_pen_modifier = 3.f;
			else if (exit_surface_data->m_game.m_material == 'L')
				combined_pen_modifier = 2.f;
		}

		const auto modifier = std::max(1.f / combined_pen_modifier, 0.f);
		const auto pen_dist = (exit_trace.m_end_pos - enter_trace.m_end_pos).length();

		const auto lost_dmg =
			cur_dmg * final_dmg_modifier
			+ pen_modifier * (modifier * 3.f)
			+ ((pen_dist * pen_dist) * modifier) / 24.f;

		if (lost_dmg > cur_dmg)
			return false;

		if (lost_dmg > 0.f)
			cur_dmg -= lost_dmg;

		if (cur_dmg < 1.f)
			return false;

		--pen_count;

		src = exit_trace.m_end_pos;

		return true;
	}

	void c_auto_wall::scale_dmg(
		player_t* const player, float& dmg,
		const float armor_ratio, const float headshot_mult, const int hitgroup
	) const
	{
		const auto has_heavy_armor = player->has_heavy_armor();

		switch (hitgroup)
		{
		case 1:
			dmg *= headshot_mult;

			if (has_heavy_armor)
				dmg *= 0.5f;

			break;
		case 3: dmg *= 1.25f; break;
		case 6:
		case 7:
			dmg *= 0.75f;

			break;
		}

		const auto armor_value = player->armor_value();
		if (!armor_value
			|| hitgroup < 0
			|| hitgroup > 5
			|| (hitgroup == 1 && !player->has_helmet()))
			return;

		auto heavy_ratio = 1.f, bonus_ratio = 0.5f, ratio = armor_ratio * 0.5f;

		if (has_heavy_armor)
		{
			ratio *= 0.2f;
			heavy_ratio = 0.25f;
			bonus_ratio = 0.33f;
		}

		auto dmg_to_hp = dmg * ratio;

		if (((dmg - dmg_to_hp) * (bonus_ratio * heavy_ratio)) > armor_value)
			dmg -= armor_value / bonus_ratio;
		else
			dmg = dmg_to_hp;
	}

	pen_data_t c_auto_wall::fire_bullet(
		player_t* const shooter, player_t* const target,
		const weapon_info_t* const wpn_data, const bool is_taser, Vector src, const Vector& dst
	) const
	{
		const auto pen_modifier = std::max((3.f / wpn_data->m_penetration) * 1.25f, 0.f);

		float cur_dist{};

		pen_data_t data{};

		data.m_remaining_pen = 4;

		auto cur_dmg = static_cast<float>(wpn_data->m_dmg);

		auto dir = dst - src;

		dir.normalize();

		trace_t trace{};
		valve::trace_filter_skip_two_entities_t trace_filter{};

		player_t* last_hit_player{};

		auto max_dist = wpn_data->m_range;

		while (cur_dmg > 0.f)
		{
			max_dist -= cur_dist;

			const auto cur_dst = src + dir * max_dist;

			trace_filter.m_ignore_entity0 = shooter;
			trace_filter.m_ignore_entity1 = last_hit_player;

			m_trace()->TraceRay(
				{ src, cur_dst }, e_mask::shot_player,
				reinterpret_cast<valve::trace_filter_t*>(&trace_filter), &trace
			);

			if (target)
				clip_trace_to_player(src, cur_dst + dir * 40.f, trace, target, trace_filter.m_should_hit_fn);

			if (trace.m_fraction == 1.f)
				break;

			cur_dist += trace.m_fraction * max_dist;
			cur_dmg *= std::pow(wpn_data->m_range_modifier, cur_dist / 500.f);

			if (trace.m_hit_entity)
			{
				const auto is_player = trace.m_hit_entity->is_player();
				if ((trace.m_hit_entity == target || (is_player && trace.m_hit_entity->team() != shooter->team()))
					&& ((trace.m_hitgroup >= 1 && trace.m_hitgroup <= 7) || trace.m_hitgroup == 10))
				{
					data.m_hit_player = static_cast<player_t*>(trace.m_hit_entity);
					data.m_hitbox = trace.m_hitbox;
					data.m_hitgroup = trace.m_hitgroup;

					if (is_taser)
						data.m_hitgroup = 0;

					scale_dmg(data.m_hit_player, cur_dmg, wpn_data->m_armor_ratio, wpn_data->m_headshot_multiplier, data.m_hitgroup);

					data.m_dmg = static_cast<int>(cur_dmg);

					return data;
				}

				last_hit_player =
					is_player ? static_cast<player_t*>(trace.m_hit_entity) : nullptr;
			}
			else
				last_hit_player = nullptr;

			if (is_taser
				|| (cur_dist > 3000.f && wpn_data->m_penetration > 0.f))
				break;

			const auto enter_surface = valve::g_surface_data->find(trace.m_surface.m_surface_props);
			if (enter_surface->m_game.m_penetration_modifier < 0.1f
				|| !handle_bullet_penetration(shooter, wpn_data, trace, src, dir, data.m_remaining_pen, cur_dmg, pen_modifier))
				break;

			data.did_penetrate = true;
		}

		return data;
	}

	pen_data_t c_auto_wall::fire_emulated(
		player_t* const shooter, player_t* const target, Vector src, const Vector& dst
	) const
	{
		static const auto wpn_data = []()
		{
			weapon_info_t wpn_data{};

			wpn_data.m_dmg = 115;
			wpn_data.m_range = 8192.f;
			wpn_data.m_penetration = 2.5f;
			wpn_data.m_range_modifier = 0.99f;
			wpn_data.m_armor_ratio = 1.95f;

			return wpn_data;
		}();

		const auto pen_modifier = std::max((3.f / wpn_data.m_penetration) * 1.25f, 0.f);

		float cur_dist{};

		pen_data_t data{};

		data.m_remaining_pen = 4;

		auto cur_dmg = static_cast<float>(wpn_data.m_dmg);

		auto dir = dst - src;

		const auto max_dist = dir.normalize();

		trace_t trace{};
		valve::trace_filter_skip_two_entities_t trace_filter{};

		player_t* last_hit_player{};

		while (cur_dmg > 0.f)
		{
			const auto dist_remaining = wpn_data.m_range - cur_dist;

			const auto cur_dst = src + dir * dist_remaining;

			trace_filter.m_ignore_entity0 = shooter;
			trace_filter.m_ignore_entity1 = last_hit_player;

			m_trace()->TraceRay(
				{ src, cur_dst }, e_mask::shot_player,
				reinterpret_cast<valve::trace_filter_t*>(&trace_filter), &trace
			);

			if (target)
				clip_trace_to_player(src, cur_dst + dir * 40.f, trace, target, trace_filter.m_should_hit_fn);

			if (trace.m_fraction == 1.f
				|| (trace.m_end_pos - src).length() > max_dist)
				break;

			cur_dist += trace.m_fraction * dist_remaining;
			cur_dmg *= std::pow(wpn_data.m_range_modifier, cur_dist / 500.f);

			if (trace.m_hit_entity)
			{
				const auto is_player = trace.m_hit_entity->is_player();
				if (trace.m_hit_entity == target)
				{
					data.m_hit_player = static_cast<player_t*>(trace.m_hit_entity);
					data.m_hitbox = trace.m_hitbox;
					data.m_hitgroup = trace.m_hitgroup;
					data.m_dmg = static_cast<int>(cur_dmg);

					return data;
				}

				last_hit_player =
					is_player ? static_cast<player_t*>(trace.m_hit_entity) : nullptr;
			}
			else
				last_hit_player = nullptr;

			if (cur_dist > 3000.f
				&& wpn_data.m_penetration > 0.f)
				break;

			const auto enter_surface = valve::g_surface_data->find(trace.m_surface.m_surface_props);
			if (enter_surface->m_game.m_penetration_modifier < 0.1f
				|| !handle_bullet_penetration(shooter, &wpn_data, trace, src, dir, data.m_remaining_pen, cur_dmg, pen_modifier))
				break;
		}

		return data;
	}
}