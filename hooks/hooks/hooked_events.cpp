// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: http://www.viva64.com

#include "..\hooks.hpp"
#include "..\..\cheats\ragebot\aim.h"
#include "..\..\cheats\misc\logs.h"
#include "..\..\cheats\visuals\other_esp.h"
#include "..\..\cheats\visuals\bullet_tracers.h"
#include "..\..\cheats\visuals\player_esp.h"
#include "..\..\cheats\ragebot\antiaim.h"
#include "..\..\cheats\visuals\nightmode.h"
#include "..\..\cheats\misc\misc.h"
#include "..\..\cheats\visuals\dormant_esp.h"
#include "..\..\cheats\lagcompensation\animation_system.h"
#include "../../cheats/misc/ShotSystem.hpp"
#include "../../cheats/lagcompensation/AnimSync/LagComp.hpp"
#include "../../cheats/ragebot/shots.h"


void C_HookedEvents::FireGameEvent(IGameEvent* event)
{
	auto event_name = event->GetName();


	//g_ShotHandling.GameEvent(event);

	if (g_ctx.globals.loaded_script)
	{
		for (auto& script : c_lua::get().scripts)
		{
			auto script_id = c_lua::get().get_script_id(script);

			if (c_lua::get().events.find(script_id) == c_lua::get().events.end())
				continue;

			if (c_lua::get().events[script_id].find(event_name) == c_lua::get().events[script_id].end())
				continue;

			c_lua::get().events[script_id][event_name](event);
		}
	}

	if (!strcmp(event->GetName(), crypt_str("player_footstep")))
	{
		auto userid = m_engine()->GetPlayerForUserID(event->GetInt(crypt_str("userid")));
		auto e = static_cast<player_t*>(m_entitylist()->GetClientEntity(userid));

		if (e->valid(false, false))
		{
			auto type = ENEMY;

			if (e == g_ctx.local())
				type = LOCAL;
			else if (e->m_iTeamNum() == g_ctx.local()->m_iTeamNum())
				type = TEAM;

			if (g_cfg.player.type[type].footsteps)
			{
				static auto model_index = m_modelinfo()->GetModelIndex(crypt_str("sprites/physbeam.vmt"));

				if (g_ctx.globals.should_update_beam_index)
					model_index = m_modelinfo()->GetModelIndex(crypt_str("sprites/physbeam.vmt"));

				BeamInfo_t info;

				info.m_nType = TE_BEAMRINGPOINT;
				info.m_pszModelName = crypt_str("sprites/physbeam.vmt");
				info.m_nModelIndex = model_index;
				info.m_nHaloIndex = -1;
				info.m_flHaloScale = 3.0f;
				info.m_flLife = 2.0f;
				info.m_flWidth = (float)g_cfg.player.type[type].thickness;
				info.m_flFadeLength = 1.0f;
				info.m_flAmplitude = 0.0f;
				info.m_flRed = (float)g_cfg.player.type[type].footsteps_color.r();
				info.m_flGreen = (float)g_cfg.player.type[type].footsteps_color.g();
				info.m_flBlue = (float)g_cfg.player.type[type].footsteps_color.b();
				info.m_flBrightness = (float)g_cfg.player.type[type].footsteps_color.a();
				info.m_flSpeed = 0.0f;
				info.m_nStartFrame = 0.0f;
				info.m_flFrameRate = 60.0f;
				info.m_nSegments = -1;
				info.m_nFlags = FBEAM_FADEOUT;
				info.m_vecCenter = e->GetAbsOrigin() + Vector(0.0f, 0.0f, 5.0f);
				info.m_flStartRadius = 5.0f;
				info.m_flEndRadius = (float)g_cfg.player.type[type].radius;
				info.m_bRenderable = true;

				auto beam_draw = m_viewrenderbeams()->CreateBeamRingPoint(info);

				if (beam_draw)
					m_viewrenderbeams()->DrawBeam(beam_draw);
			}
		}

	}
	else if (!strcmp(event_name, crypt_str("weapon_fire")))
	{
		auto user_id = event->GetInt(crypt_str("userid"));
		auto user = m_engine()->GetPlayerForUserID(user_id);

		if (user == m_engine()->GetLocalPlayer())
			shots::get().on_weapon_fire();
	}
	else if (!strcmp(event_name, crypt_str("bullet_impact")))
	{
		auto user_id = event->GetInt(crypt_str("userid"));
		auto user = m_engine()->GetPlayerForUserID(user_id); //-V807

		if (user == m_engine()->GetLocalPlayer())
		{
			Vector position(event->GetFloat(crypt_str("x")), event->GetFloat(crypt_str("y")), event->GetFloat(crypt_str("z")));

			if (g_cfg.esp.server_bullet_impacts)
				m_debugoverlay()->BoxOverlay(position, Vector(-2.0f, -2.0f, -2.0f), Vector(2.0f, 2.0f, 2.0f), QAngle(0.0f, 0.0f, 0.0f), g_cfg.esp.server_bullet_impacts_color.r(), g_cfg.esp.server_bullet_impacts_color.g(), g_cfg.esp.server_bullet_impacts_color.b(), g_cfg.esp.server_bullet_impacts_color.a(), 4.0f);

			shots::get().on_impact(position);
		}
	}
	else if (!strcmp(event_name, crypt_str("player_death")))
	{
		auto attacker = event->GetInt(crypt_str("attacker"));
		auto user = event->GetInt(crypt_str("userid"));

		auto attacker_id = m_engine()->GetPlayerForUserID(attacker); //-V807
		auto user_id = m_engine()->GetPlayerForUserID(user);

		player_t* pPlayer = static_cast<player_t*>(m_entitylist()->GetClientEntity(user_id));
		
		if (pPlayer)	
			C_LagComp::get().CleanPlayer(pPlayer);

		if (g_ctx.local()->is_alive() && attacker_id == m_engine()->GetLocalPlayer() && user_id != m_engine()->GetLocalPlayer())
		{
			auto entity = static_cast<player_t*>(m_entitylist()->GetClientEntity(user_id));

			if (g_cfg.player.enable && g_cfg.esp.kill_effect)
				g_ctx.local()->m_flHealthShotBoostExpirationTime() = m_globals()->m_curtime + g_cfg.esp.kill_effect_duration;

			if (g_cfg.esp.killsound)
			{
				auto headshot = event->GetBool(crypt_str("headshot"));

				switch (g_ctx.globals.kills)
				{
				case 0:
					m_surface()->PlaySound_(headshot ? crypt_str("kill2.wav") : crypt_str("kill1.wav"));
					break;
				case 1:
					m_surface()->PlaySound_(crypt_str("kill3.wav"));
					break;
				case 2:
					m_surface()->PlaySound_(crypt_str("kill4.wav"));
					break;
				case 3:
					m_surface()->PlaySound_(crypt_str("kill5.wav"));
					break;
				case 4:
					m_surface()->PlaySound_(crypt_str("kill6.wav"));
					break;
				case 5:
					m_surface()->PlaySound_(crypt_str("kill7.wav"));
					break;
				case 6:
					m_surface()->PlaySound_(crypt_str("kill8.wav"));
					break;
				case 7:
					m_surface()->PlaySound_(crypt_str("kill9.wav"));
					break;
				case 8:
					m_surface()->PlaySound_(crypt_str("kill10.wav"));
					break;
				default:
					m_surface()->PlaySound_(crypt_str("kill11.wav"));
					break;
				}
			}

			g_ctx.globals.kills++;

			std::string weapon_name = event->GetString(crypt_str("weapon"));
			auto weapon = g_ctx.local()->m_hActiveWeapon().Get();

			if (weapon && weapon_name.find(crypt_str("knife")) != std::string::npos)
				SkinChanger::overrideHudIcon(event);
		}
	}
	else if (!strcmp(event_name, crypt_str("player_hurt")))
	{
		auto attacker = event->GetInt(crypt_str("attacker"));
		auto user = event->GetInt(crypt_str("userid"));

		auto attacker_id = m_engine()->GetPlayerForUserID(attacker);
		auto user_id = m_engine()->GetPlayerForUserID(user);

		if (attacker_id == m_engine()->GetLocalPlayer() && user_id != m_engine()->GetLocalPlayer())
		{
			if (g_cfg.esp.hitsound && g_cfg.player.enable)
			{
				switch (g_cfg.esp.hitsound)
				{
				case 1:
					m_surface()->PlaySound_(crypt_str("metallic.wav"));
					break;
				case 2:
					m_surface()->PlaySound_(crypt_str("cod.wav"));
					break;
				case 3:
					m_surface()->PlaySound_(crypt_str("bubble.wav"));
					break;
				case 4:
					m_surface()->PlaySound_(crypt_str("stapler.wav"));
					break;
				case 5:
					m_surface()->PlaySound_(crypt_str("bell.wav"));
					break;
				case 6:
					m_surface()->PlaySound_(crypt_str("flick.wav"));
					break;
				case 7:
					m_surface()->PlaySound_(crypt_str("blaster.wav"));
					break;
				}
			}

			auto entity = static_cast<player_t*>(m_entitylist()->GetClientEntity(user_id));

			std::string weapon = event->GetString(crypt_str("weapon"));
			auto damage = event->GetInt(crypt_str("dmg_health"));
			auto hitgroup = event->GetInt(crypt_str("hitgroup"));

			static auto get_hitbox_by_hitgroup = [](int hitgroup) -> int
			{
				switch (hitgroup)
				{
				case HITGROUP_HEAD:
					return HITBOX_HEAD;
				case HITGROUP_CHEST:
					return HITBOX_CHEST;
				case HITGROUP_STOMACH:
					return HITBOX_STOMACH;
				case HITGROUP_LEFTARM:
					return HITBOX_LEFT_HAND;
				case HITGROUP_RIGHTARM:
					return HITBOX_RIGHT_HAND;
				case HITGROUP_LEFTLEG:
					return HITBOX_RIGHT_CALF;
				case HITGROUP_RIGHTLEG:
					return HITBOX_LEFT_CALF;
				default:
					return HITBOX_PELVIS;
				}
			};

			static auto get_hitbox_name = [](int hitbox) -> std::string
			{
				switch (hitbox)
				{
				case HITBOX_HEAD:
					return crypt_str("Head");
				case HITBOX_CHEST:
					return crypt_str("Chest");
				case HITBOX_STOMACH:
					return crypt_str("Stomach");
				case HITBOX_PELVIS:
					return crypt_str("Pelvis");
				case HITBOX_RIGHT_UPPER_ARM:
				case HITBOX_RIGHT_FOREARM:
				case HITBOX_RIGHT_HAND:
					return crypt_str("Left arm");
				case HITBOX_LEFT_UPPER_ARM:
				case HITBOX_LEFT_FOREARM:
				case HITBOX_LEFT_HAND:
					return crypt_str("Right arm");
				case HITBOX_RIGHT_THIGH:
				case HITBOX_RIGHT_CALF:
					return crypt_str("Left leg");
				case HITBOX_LEFT_THIGH:
				case HITBOX_LEFT_CALF:
					return crypt_str("Right leg");
				case HITBOX_RIGHT_FOOT:
					return crypt_str("Left foot");
				case HITBOX_LEFT_FOOT:
					return crypt_str("Right foot");
				}
			};

			shots::get().on_player_hurt(event, user_id);
		}
	}
	else if (!strcmp(event_name, crypt_str("round_start")))
	{
		if (!g_cfg.misc.show_default_log && eventlogs::get().last_log && g_cfg.misc.log_output[EVENTLOG_OUTPUT_CONSOLE])
		{
			eventlogs::get().last_log = false;
			m_cvar()->ConsolePrintf(crypt_str("-------------------------\n-------------------------\n-------------------------\n"));
		}

		for (auto i = 1; i < m_globals()->m_maxclients; i++)
		{
			g_ctx.globals.fired_shots[i] = 0;
			//g_ctx.globals.missed_shots[i] = 0;
			g_ctx.globals.missed_shots_spread[i] = 0;
			lagcompensation::get().is_dormant[i] = false;
			lagcompensation::get().player_resolver[i].reset();
			playeresp::get().esp_alpha_fade[i] = 0.0f;
			playeresp::get().health[i] = 100;
			c_dormant_esp::get().m_cSoundPlayers[i].reset();


			auto player = (player_t*)m_entitylist()->GetClientEntity(i);

			if (!player || player == g_ctx.local())
				continue;

		
			g_AimPlayerData->OnRoundStart(player);
		}

		g_AntiAim->freeze_check = true;
		g_ctx.globals.bomb_timer_enable = true;
		g_ctx.globals.should_buy = 2;
		g_ctx.globals.should_clear_death_notices = true;
		g_ctx.globals.should_update_playerresource = true;
		g_ctx.globals.should_update_gamerules = true;
		g_ctx.globals.kills = 0;
	}
	else if (!strcmp(event_name, crypt_str("round_freeze_end")))
		g_AntiAim->freeze_check = false;
	else if (!strcmp(event_name, crypt_str("bomb_defused")))
		g_ctx.globals.bomb_timer_enable = false;

	if ((g_cfg.esp.bullet_tracer || g_cfg.esp.enemy_bullet_tracer) && g_cfg.player.enable)
		bullettracers::get().events(event);

	eventlogs::get().events(event);
}

int C_HookedEvents::GetEventDebugID(void)
{
	return EVENT_DEBUG_ID_INIT;
}

void C_HookedEvents::RegisterSelf()
{
	m_iDebugId = EVENT_DEBUG_ID_INIT;
	auto eventmanager = m_eventmanager();

	eventmanager->AddListener(this, crypt_str("player_footstep"), false);
	eventmanager->AddListener(this, crypt_str("player_hurt"), false);
	eventmanager->AddListener(this, crypt_str("player_death"), false);
	eventmanager->AddListener(this, crypt_str("weapon_fire"), false);
	eventmanager->AddListener(this, crypt_str("item_purchase"), false);
	eventmanager->AddListener(this, crypt_str("bullet_impact"), false);
	eventmanager->AddListener(this, crypt_str("round_start"), false);
	eventmanager->AddListener(this, crypt_str("round_freeze_end"), false);
	eventmanager->AddListener(this, crypt_str("bomb_defused"), false);
	eventmanager->AddListener(this, crypt_str("bomb_begindefuse"), false);
	eventmanager->AddListener(this, crypt_str("bomb_beginplant"), false);

	g_ctx.globals.events.emplace_back(crypt_str("player_footstep"));
	g_ctx.globals.events.emplace_back(crypt_str("player_hurt"));
	g_ctx.globals.events.emplace_back(crypt_str("player_death"));
	g_ctx.globals.events.emplace_back(crypt_str("weapon_fire"));
	g_ctx.globals.events.emplace_back(crypt_str("item_purchase"));
	g_ctx.globals.events.emplace_back(crypt_str("bullet_impact"));
	g_ctx.globals.events.emplace_back(crypt_str("round_start"));
	g_ctx.globals.events.emplace_back(crypt_str("round_freeze_end"));
	g_ctx.globals.events.emplace_back(crypt_str("bomb_defused"));
	g_ctx.globals.events.emplace_back(crypt_str("bomb_begindefuse"));
	g_ctx.globals.events.emplace_back(crypt_str("bomb_beginplant"));
}

void C_HookedEvents::RemoveSelf()
{
	m_eventmanager()->RemoveListener(this);
}