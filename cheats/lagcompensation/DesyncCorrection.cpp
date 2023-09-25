#include "animation_system.h"
#include "LocalAnimFix.hpp"

void DesyncCorrection::Run(adjust_data* Record, player_t* Player)
{
	SetMode(Record, Player);

	if (Record->shot) {
		Record->side = RESOLVER_ON_SHOT;
		return;
	}

	if (Record->curMode != AIR)
		GetSide(Record, Player);


	switch (g_ctx.globals.missed_shots[Player->EntIndex()]) {
	case 0:
		break;
	case 1:// low delta
		g_cfg.player_list.types[Record->type].should_flip[Player->EntIndex()] = false;
		g_cfg.player_list.types[Record->type].low_delta[Player->EntIndex()] = true;
		break;
	case 2:// flipped full delta
		g_cfg.player_list.types[Record->type].should_flip[Player->EntIndex()] = true;
		g_cfg.player_list.types[Record->type].low_delta[Player->EntIndex()] = false;
		break;
	case 3:// flipped low delta
		g_cfg.player_list.types[Record->type].should_flip[Player->EntIndex()] = true;
		g_cfg.player_list.types[Record->type].low_delta[Player->EntIndex()] = true;
		break;
	}

	if (g_ctx.globals.missed_shots[Player->EntIndex()] > 3) {
		g_ctx.globals.missed_shots[Player->EntIndex()] = 0;
		g_cfg.player_list.types[Record->type].should_flip[Player->EntIndex()] = false;
		g_cfg.player_list.types[Record->type].low_delta[Player->EntIndex()] = false;
	}
	
	if (Record->type != LAYERS) {
		if (g_cfg.player_list.types[Record->type].should_flip[Player->EntIndex()])
			Record->desync_amount = -(Record->desync_amount);

		if (g_cfg.player_list.types[Record->type].low_delta[Player->EntIndex()])
			Record->desync_amount *= 0.5;
	}

	Player->get_animation_state()->m_flGoalFeetYaw = math::normalize_yaw(Player->m_angEyeAngles().y + Record->desync_amount);
}
void DesyncCorrection::SetMode(adjust_data* Record, player_t* Player)
{
	float speed = Record->velocity.Length2D();
	bool on_ground = Player->m_fFlags() & FL_ONGROUND && !Player->get_animation_state()->m_bInHitGroundAnimation;
	bool ducking = Player->get_animation_state()->m_fDuckAmount && Player->m_fFlags() & FL_ONGROUND && !Player->get_animation_state()->m_bInHitGroundAnimation;

	if (!on_ground)
		Record->curMode = AIR;
	else if (speed < 0.1f)
		Record->curMode = STANDING;
	else if (speed >= 0.1f)
	{
		Record->curMode = MOVING;	
	}
	else
		Record->curMode = FREESTANDING;
}
void DesyncCorrection::GetSide(adjust_data* Record, player_t* Player)
{
	if (Record->curMode == MOVING)
	{
		Record->type = LAYERS;
		const float left_delta = fabsf(Record->layers[6].m_flPlaybackRate - ResolverLayers[RightMatrix][6].m_flPlaybackRate);
		const float right_delta = fabsf(Record->layers[6].m_flPlaybackRate - ResolverLayers[LeftMatrix][6].m_flPlaybackRate);
		const float zero_delta = fabsf(Record->layers[6].m_flPlaybackRate - ResolverLayers[ZeroMatrix][6].m_flPlaybackRate);

		static float resolving_delta = 0.0f;
		bool should_use_low_angles = false;
		if (left_delta != zero_delta && right_delta != zero_delta)
		{
			const float low_left_delta = fabsf(Record->layers[6].m_flPlaybackRate - ResolverLayers[LowRightMatrix][6].m_flPlaybackRate);
			const float low_right_delta = fabsf(Record->layers[6].m_flPlaybackRate - ResolverLayers[LowLeftMatrix][6].m_flPlaybackRate);

			if ((low_left_delta >= zero_delta && low_left_delta >= left_delta) || (low_right_delta >= zero_delta && low_left_delta >= zero_delta))
				should_use_low_angles = true;

			if (float(left_delta * 1000.0) >= float(right_delta * 1000.0) && int(left_delta * 1000.0))
				resolving_delta = right_delta;
			else if (float(right_delta * 1000.0) >= float(left_delta * 1000.0) && int(right_delta * 1000.0))
				resolving_delta = left_delta;

			if (resolving_delta == 0.0f)
			{
				if (left_delta > right_delta)
					resolving_delta = right_delta;
				else if (right_delta > left_delta)
					resolving_delta = left_delta;
			}

			if (should_use_low_angles)
			{
				if (low_left_delta > low_right_delta && low_left_delta > left_delta)
					resolving_delta = low_right_delta;
				else if (low_right_delta > low_left_delta && low_right_delta > right_delta)
					resolving_delta = low_left_delta;
			}

			if (resolving_delta != 0.0f)
			{
				if (resolving_delta == left_delta)
					Record->desync_amount = 60.f;


				if (resolving_delta == right_delta)
					Record->desync_amount = -60.f;


				if (resolving_delta = low_left_delta)
					Record->desync_amount = 30.f;


				if (resolving_delta = low_right_delta)
					Record->desync_amount = -30.f;
			}
		}
	}
	else if (Record->curMode == STANDING)
	{
		auto m_side = false;
		auto trace = false;
		if (m_globals()->m_curtime - LockSide > 2.0f)
		{
			auto first_visible = util::visible(g_LocalAnimations->GetShootPosition(), Player->hitbox_position_matrix(HITBOX_HEAD, Record->m_Matricies[LeftMatrix].data()), Player, g_ctx.local());
			auto second_visible = util::visible(g_LocalAnimations->GetShootPosition(), Player->hitbox_position_matrix(HITBOX_HEAD, Record->m_Matricies[RightMatrix].data()), Player, g_ctx.local());

			if (first_visible != second_visible)
			{
				trace = true;
				m_side = second_visible;
				LockSide = m_globals()->m_curtime;
			}
			else
			{
				auto first_position = g_LocalAnimations->GetShootPosition().DistTo(Player->hitbox_position_matrix(HITBOX_HEAD, Record->m_Matricies[LeftMatrix].data()));
				auto second_position = g_LocalAnimations->GetShootPosition().DistTo(Player->hitbox_position_matrix(HITBOX_HEAD, Record->m_Matricies[RightMatrix].data()));

				if (fabsf(first_position - second_position) > 1.0f)
					m_side = first_position > second_position;
			}
		}
		else
			trace = true;

		if (m_side)
		{
			Record->type = trace ? TRACE : DIRECTIONAL;
			Record->desync_amount = 60.f;
		}
		else
		{
			Record->type = trace ? TRACE : DIRECTIONAL;
			Record->desync_amount = -60.f;
		}
	}
}