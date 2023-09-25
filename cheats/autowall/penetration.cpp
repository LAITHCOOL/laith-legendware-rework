#include "penetration.h"
#include "../lagcompensation/LocalAnimFix.hpp"
void UTIL_ClipTraceToPlayers(const Vector& start, const Vector& end, uint32_t mask, ITraceFilter* filter, CGameTrace* tr, float range) {

	static auto clip_trace_to_players  = util::FindSignature(crypt_str("client.dll"), crypt_str("53 8B DC 83 EC 08 83 E4 F0 83 C4 04 55 8B 6B 04 89 6C 24 04 8B EC 81 EC D8 ? ? ? 0F 57 C9"));
	_asm
	{
		mov eax, filter
		lea ecx, tr
		push ecx
		push eax
		push mask
		lea edx, end
		lea ecx, start
		call clip_trace_to_players
		add esp, 0xC
	}
}

float penetration::scale(player_t* player, float damage, float armor_ratio, int hitgroup) {
	bool  has_heavy_armor;
	int   armor;
	float heavy_ratio, bonus_ratio, ratio, new_damage, head_scale, body_scale;

	static auto is_armored = [](player_t* player, int armor, int hitgroup) {
		// the player has no armor.
		if (armor <= 0)
			return false;

		// if the hitgroup is head and the player has a helment, return true.
		// otherwise only return true if the hitgroup is not generic / legs / gear.
		if (hitgroup == HITGROUP_HEAD && player->m_bHasHelmet())
			return true;

		else if (hitgroup >= HITGROUP_CHEST && hitgroup <= HITGROUP_RIGHTARM)
			return true;

		return false;
	};
	static auto mp_damage_scale_ct_head = m_cvar()->FindVar(crypt_str("mp_damage_scale_ct_head"));
	static auto mp_damage_scale_t_head = m_cvar()->FindVar(crypt_str("mp_damage_scale_t_head"));

	static auto mp_damage_scale_ct_body = m_cvar()->FindVar(crypt_str("mp_damage_scale_ct_body"));
	static auto mp_damage_scale_t_body = m_cvar()->FindVar(crypt_str("mp_damage_scale_t_body"));

	head_scale = player->m_iTeamNum() == 3 ? mp_damage_scale_ct_head->GetFloat() : mp_damage_scale_t_head->GetFloat();
	body_scale = player->m_iTeamNum() == 3 ? mp_damage_scale_ct_body->GetFloat() : mp_damage_scale_t_body->GetFloat();

	// check if the player has heavy armor, this is only really used in operation stuff.
	has_heavy_armor = player->m_bHasHeavyArmor();

	if (has_heavy_armor)
		head_scale *= 0.5f;

	// scale damage based on hitgroup.
	switch (hitgroup) {
	case HITGROUP_HEAD:
		damage *= 4.f * head_scale;
		break;
	case HITGROUP_CHEST:
	case HITGROUP_LEFTARM:
	case HITGROUP_RIGHTARM:
	case HITGROUP_GEAR:
		damage *= body_scale;
		break;
	case HITGROUP_STOMACH:
		damage *= 1.25f * body_scale;
		break;

	case HITGROUP_LEFTLEG:
	case HITGROUP_RIGHTLEG:
		damage *= 0.75f * body_scale;
		break;

	default:
		break;
	}

	// grab amount of player armor.
	armor = player->m_ArmorValue();

	// check if the ent is armored and scale damage based on armor.
	if (is_armored(player, armor, hitgroup)) {
		heavy_ratio = 1.f;
		bonus_ratio = 0.5f;
		ratio = armor_ratio * 0.5f;

		// player has heavy armor.
		if (has_heavy_armor)
		{
			// calculate ratio values.
			bonus_ratio = 0.33f;
			ratio = armor_ratio * 0.25f;
			heavy_ratio = 0.33f;

			// calculate new damage.
			new_damage = (damage * ratio) * 0.85f;
		}

		// no heavy armor, do normal damage calculation.
		else
			new_damage = damage * ratio;

		if (((damage - new_damage) * (heavy_ratio * bonus_ratio)) > armor)
			new_damage = damage - (armor / bonus_ratio);

		damage = new_damage;
	}

	return std::floor(damage);
}

bool penetration::TraceToExit(const Vector& start, const Vector& dir, Vector& out, CGameTrace* enter_trace, CGameTrace* exit_trace) {
	float  dist{};
	Vector new_end;
	int    contents, first_contents{};

	// max pen distance is 90 units.
	while (dist <= 90.f) {
		// step forward a bit.
		dist += 4.f;

		// set out pos.
		out = start + (dir * dist);

		if (!first_contents)
			first_contents = m_trace()->GetPointContents(out, MASK_SHOT_HULL | CONTENTS_HITBOX, nullptr);

		contents = m_trace()->GetPointContents(out, MASK_SHOT_HULL | CONTENTS_HITBOX, nullptr);

		if ((contents & MASK_SHOT_HULL) && (!(contents & CONTENTS_HITBOX) || (contents == first_contents)))
			continue;

		// move end pos a bit for tracing.
		new_end = out - (dir * 4.f);

		// do first trace.
		m_trace()->TraceRay(Ray_t(out, new_end), MASK_SHOT_HULL | CONTENTS_HITBOX, nullptr, exit_trace);

		static auto sv_clip_penetration_traces_to_players = m_cvar()->FindVar(crypt_str("sv_clip_penetration_traces_to_players"));;
		// note - dex; this is some new stuff added sometime around late 2017 ( 10.31.2017 update? ).
		if (sv_clip_penetration_traces_to_players->GetInt())
			UTIL_ClipTraceToPlayers(out, new_end, MASK_SHOT_HULL | CONTENTS_HITBOX, nullptr, exit_trace, -60.f);
		static auto trace_filter_simple = util::FindSignature(crypt_str("client.dll"), crypt_str("55 8B EC 83 E4 F0 83 EC 7C 56 52")) + 0x3D;
		// we hit an ent's hitbox, do another trace.
		if (exit_trace->startsolid && (exit_trace->surface.flags & SURF_HITBOX)) {
			uint32_t filter_[4] =
			{
				*(uint32_t*)(trace_filter_simple),
				(uint32_t)exit_trace->hit_entity,
				0,
				0
			};

			m_trace()->TraceRay(Ray_t(out, start), MASK_SHOT_HULL, (CTraceFilter*)&filter_, exit_trace);

			if (exit_trace->DidHit() && !exit_trace->startsolid) {
				out = exit_trace->endpos;
				return true;
			}

			continue;
		}

		if (!exit_trace->DidHit() || exit_trace->startsolid) {
			if (util::is_breakable_entity(enter_trace->hit_entity)) {
				*exit_trace = *enter_trace;
				exit_trace->endpos = start + dir;
				return true;
			}

			continue;
		}

		if ((exit_trace->surface.flags & SURF_NODRAW)) {
			// note - dex; ok, when this happens the game seems to not ignore world?
			if (util::is_breakable_entity(exit_trace->hit_entity) && util::is_breakable_entity(enter_trace->hit_entity)) {
				out = exit_trace->endpos;
				return true;
			}

			if (!(enter_trace->surface.flags & SURF_NODRAW))
				continue;
		}

		if (exit_trace->plane.normal.Dot(dir) <= 1.f) {
			out -= (dir * (exit_trace->fraction * 4.f));
			return true;
		}
	}

	return false;
}

void penetration::ClipTraceToPlayer(const Vector& start, const Vector& end, uint32_t mask, CGameTrace* tr, player_t* player, float min) {
	Vector     pos, to, dir, on_ray;
	float      len, range_along, range;
	Ray_t        ray;
	CGameTrace new_trace;

	// reference: https://github.com/alliedmodders/hl2sdk/blob/3957adff10fe20d38a62fa8c018340bf2618742b/game/shared/util_shared.h#L381

	// set some local vars.
	pos = player->GetAbsOrigin() + ((player->m_vecMins() + player->m_vecMaxs()) * 0.5f);
	to = pos - start;
	dir = start - end;
	len = dir.Normalize();

	range_along = dir.Dot(to);

	// off start point.
	if (range_along < 0.f)
		range = -(to).Length();

	// off end point.
	else if (range_along > len)
		range = -(pos - end).Length();

	// within ray bounds.
	else {
		on_ray = start + (dir * range_along);
		range = (pos - on_ray).Length();
	}

	if ( /*min <= range &&*/ range <= 60.f) {
		// clip to player.
		m_trace()->ClipRayToEntity(Ray_t(start, end), mask, player, &new_trace);

		if (tr->fraction > new_trace.fraction)
			*tr = new_trace;
	}
}

bool penetration::run(PenetrationInput_t* in, PenetrationOutput_t* out, bool use_custom_shoot_pos) {
	int			  pen{ 4 }, enter_material, exit_material;
	float		  damage, penetration, penetration_mod, player_damage, remaining, trace_len{}, total_pen_mod, damage_mod, modifier, damage_lost;
	surfacedata_t* enter_surface, * exit_surface;
	bool		  nodraw, grate;
	Vector		  start, dir, end, pen_end;
	CGameTrace	  trace, exit_trace;
	weapon_t* weapon;
	weapon_info_t* weapon_info;

	// if we are tracing from our local player perspective.
	if (in->m_from == g_ctx.local()) {
		weapon = g_ctx.globals.weapon;
		weapon_info = g_ctx.globals.weapon->get_csweapon_info();
		start = use_custom_shoot_pos ? in->m_custom_shoot_pos : g_LocalAnimations->GetShootPosition();
	}

	// not local player.
	else {
		weapon = in->m_from->m_hActiveWeapon().Get();
		if (!weapon)
			return false;

		// get weapon info.
		weapon_info = weapon->get_csweapon_info();
		if (!weapon_info)
			return false;

		start = use_custom_shoot_pos ? in->m_custom_shoot_pos : in->m_from->get_shoot_position();
	}

	// get some weapon data.
	damage = (float)weapon_info->iDamage;
	penetration = weapon_info->flPenetration;

	// used later in calculations.
	penetration_mod = max(0.f, (3.f / penetration) * 1.25f);

	// get direction to end point.
	dir = (in->m_pos - start).Normalized();
	static auto trace_filter_simple = util::FindSignature(crypt_str("client.dll"), crypt_str("55 8B EC 83 E4 F0 83 EC 7C 56 52")) + 0x3D;
	uint32_t filter_[4] =
	{
		*(uint32_t*)(trace_filter_simple),
		(uint32_t)in->m_from,
		0,
		0
	};

	g_ctx.globals.autowalling = true;

	while (damage > 0.f) {

		// -------- SimulateFireBullet --------
		// 
		// calculating remaining len.
		remaining = weapon_info->flRange - trace_len;

		// set trace end.
		end = start + (dir * remaining);

		// setup ray and trace.
		// TODO; use UTIL_TraceLineIgnoreTwoEntities?

		m_trace()->TraceRay(Ray_t(start, end), MASK_SHOT_HULL | CONTENTS_HITBOX, (CTraceFilter*)&filter_, &trace);

		// we didn't hit anything.
		if (trace.fraction == 1.f) {
			g_ctx.globals.autowalling = false;
			return false;
		}

		// check for player hitboxes extending outside their collision bounds.
		// if no target is passed we clip the trace to a specific player, otherwise we clip the trace to any player.
		if (in->m_target)
			ClipTraceToPlayer(start, end + (dir * 40.f), MASK_SHOT_HULL | CONTENTS_HITBOX, &trace, in->m_target, -60.f);
		else
			UTIL_ClipTraceToPlayers(start, end + (dir * 40.f), MASK_SHOT_HULL | CONTENTS_HITBOX, (CTraceFilter*)&filter_, &trace, -60.f);

		// calculate damage based on the distance the bullet traveled.
		trace_len += trace.fraction * remaining;
		damage *= std::pow(weapon_info->flRangeModifier, trace_len / 500.f);

		// if a target was passed.
		if (in->m_target) {

			// validate that we hit the target we aimed for.
			if (trace.hit_entity && trace.hit_entity == in->m_target && util::is_valid_hitgroup(trace.hitgroup)) {
				int group = (weapon->m_iItemDefinitionIndex() == WEAPON_TASER) ? HITGROUP_GENERIC : trace.hitgroup;

				// scale damage based on the hitgroup we hit.
				player_damage = scale(in->m_target, damage, weapon_info->flArmorRatio, group);

				// set result data for when we hit a player.
				out->m_pen = pen != 4;
				out->m_hitgroup = group;
				out->m_damage = player_damage;
				out->m_target = in->m_target;

				// non-penetrate damage.
				if (pen == 4) {
					g_ctx.globals.autowalling = false;
					return player_damage >= in->m_damage;
				}

				// penetration damage.
				g_ctx.globals.autowalling = false;
				return player_damage >= in->m_damage_pen;
			}
		}

		// no target was passed, check for any player hit or just get final damage done.
		else {
			out->m_pen = pen != 4;

			// todo - dex; team checks / other checks / etc.
			if (trace.hit_entity && ((player_t*)trace.hit_entity)->is_player() && util::is_valid_hitgroup(trace.hitgroup)) {
				int group = (weapon->m_iItemDefinitionIndex() == WEAPON_TASER) ? HITGROUP_GENERIC : trace.hitgroup;

				player_damage = scale((player_t*)trace.hit_entity, damage, weapon_info->flArmorRatio, group);

				// set result data for when we hit a player.
				out->m_hitgroup = group;
				out->m_damage = player_damage;
				out->m_target = (player_t*)trace.hit_entity;

				// non-penetrate damage.
				if (pen == 4) {
					g_ctx.globals.autowalling = false;
					return player_damage >= in->m_damage;
				}

				// penetration damage.
				g_ctx.globals.autowalling = false;
				return player_damage >= in->m_damage_pen;
			}

			// if we've reached here then we didn't hit a player yet, set damage and hitgroup.
			out->m_damage = damage;
		}

		// don't run pen code if it's not wanted.
		if (!in->m_can_pen) {
			g_ctx.globals.autowalling = false;
			return false;
		}

		// -------- HandleBulletPenetration --------
		// 
		// get surface at entry point.
		enter_surface = m_physsurface()->GetSurfaceData(trace.surface.surfaceProps);

		// this happens when we're too far away from a surface and can penetrate walls or the surface's pen modifier is too low.
		if ((trace_len > 3000.f && penetration) || enter_surface->game.flPenetrationModifier < 0.1f) {
			g_ctx.globals.autowalling = false;
			return false;
		}

		// store data about surface flags / contents.
		nodraw = (trace.surface.flags & SURF_NODRAW);
		grate = (trace.contents & CONTENTS_GRATE);

		// get material at entry point.
		enter_material = enter_surface->game.material;

		// note - dex; some extra stuff the game does.
		if (!pen && !nodraw && !grate && enter_material != CHAR_TEX_GRATE && enter_material != CHAR_TEX_GLASS)
		{
			g_ctx.globals.autowalling = false;
			return false;
		}

		// no more pen.
		if (penetration <= 0.f || pen <= 0) {
			g_ctx.globals.autowalling = false;
			return false;
		}

		// try to penetrate object.
		if (!TraceToExit(trace.endpos, dir, pen_end, &trace, &exit_trace)) { // CRASHHH
			if (!(m_trace()->GetPointContents(pen_end, MASK_SHOT_HULL) & MASK_SHOT_HULL)) {
				g_ctx.globals.autowalling = false;
				return false;
			}
		}

		// get surface / material at exit point.
		exit_surface = m_physsurface()->GetSurfaceData(exit_trace.surface.surfaceProps);
		exit_material = exit_surface->game.material;

		// todo - dex; check for CHAR_TEX_FLESH and ff_damage_bullet_penetration / ff_damage_reduction_bullets convars?
		//             also need to check !isbasecombatweapon too.
		if (enter_material == CHAR_TEX_GRATE || enter_material == CHAR_TEX_GLASS) {
			total_pen_mod = 3.f;
			damage_mod = 0.05f;
		}

		else if (nodraw || grate) {
			total_pen_mod = 1.f;
			damage_mod = 0.16f;
		}

		else {
			total_pen_mod = (enter_surface->game.flPenetrationModifier + exit_surface->game.flPenetrationModifier) * 0.5f;
			damage_mod = 0.16f;
		}

		// thin metals, wood and plastic get a penetration bonus.
		if (enter_material == exit_material) {
			if (exit_material == CHAR_TEX_CARDBOARD || exit_material == CHAR_TEX_WOOD)
				total_pen_mod = 3.f;

			else if (exit_material == CHAR_TEX_PLASTIC)
				total_pen_mod = 2.f;
		}

		// set some local vars.
		trace_len = (exit_trace.endpos - trace.endpos).LengthSqr();
		modifier = fmaxf(1.f / total_pen_mod, 0.f);

		// this calculates how much damage we've lost depending on thickness of the wall, our penetration, damage, and the modifiers set earlier
		damage_lost = fmaxf(((modifier * trace_len) / 24.f) + ((damage * damage_mod) + (fmaxf(3.75 / penetration, 0.f) * 3.f * modifier)), 0.f);

		// subtract from damage.
		damage -= max(0.f, damage_lost);
		if (damage < 1.f)
		{
			g_ctx.globals.autowalling = false;
			return false;
		}

		// set new start pos for successive trace.
		start = exit_trace.endpos;

		// decrement pen.
		--pen;

		// if we reached till here that mean our target is not visible or they are behind a wall.
		out->m_visible = false;
	}

	g_ctx.globals.autowalling = false;
	return false;
}