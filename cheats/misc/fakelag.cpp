#include "..\ragebot\aim.h"
#include "fakelag.h"
#include "misc.h"
#include "prediction_system.h"
#include "logs.h"
#include "../tickbase shift/tickbase_shift.h"
#include "../prediction/EnginePrediction.h"
void fakelag::Fakelag(CUserCmd* m_pcmd)
{
	if (g_cfg.antiaim.fakelag && !g_ctx.globals.exploits)
	{
		static auto force_choke = false;

		if (force_choke)
		{
			force_choke = false;
			g_ctx.send_packet = false;
			return;
		}

		if (g_ctx.local()->m_fFlags() & FL_ONGROUND && !(g_EnginePrediction->GetUnpredictedData()->m_nFlags & FL_ONGROUND))
		{
			force_choke = true;
			g_ctx.send_packet = false;
			return;
		}
	}

	static auto fluctuate_ticks = 0;
	static auto switch_ticks = false;
	static auto random_factor = min(rand() % 16 + 1, g_cfg.antiaim.triggers_fakelag_amount);

	auto choked = m_clientstate()->iChokedCommands;
	auto flags = g_EnginePrediction->GetUnpredictedData()->m_nFlags;
	auto velocity = g_EnginePrediction->GetUnpredictedData()->m_vecVelocity.Length();
	auto velocity2d = g_EnginePrediction->GetUnpredictedData()->m_vecVelocity.Length2D();

	auto max_speed = 260.0f;
	auto weapon_info = g_ctx.globals.weapon->get_csweapon_info();

	if (weapon_info)
		max_speed = g_ctx.globals.scoped ? weapon_info->flMaxPlayerSpeedAlt : weapon_info->flMaxPlayerSpeed;

	switch (g_cfg.antiaim.fakelag_type)
	{
	case 0:
		max_choke = g_cfg.antiaim.triggers_fakelag_amount;
		break;
	case 1:
		max_choke = random_factor;
		break;
	case 2:
		if (velocity2d >= 5.0f)
		{
			auto dynamic_factor = std::ceilf(64.0f / (velocity2d * m_globals()->m_intervalpertick));

			if (dynamic_factor > 14)
				dynamic_factor = g_cfg.antiaim.triggers_fakelag_amount;

			max_choke = dynamic_factor;
		}
		else
			max_choke = g_cfg.antiaim.triggers_fakelag_amount;
		break;
	case 3:
		max_choke = fluctuate_ticks;
		break;
	}

	if (m_gamerules()->m_bIsValveDS())
		max_choke = m_engine()->IsVoiceRecording() ? 1 : min(max_choke, 6);



	if (g_ctx.local()->m_fFlags() & FL_ONGROUND && g_EnginePrediction->GetUnpredictedData()->m_nFlags & FL_ONGROUND && !m_gamerules()->m_bIsValveDS() && key_binds::get().get_key_bind_state(20))
	{
		max_choke = 14;

		if (choked < max_choke)
			g_ctx.send_packet = false;
		else
			g_ctx.send_packet = true;
	}
	else
	{
		if (g_cfg.ragebot.enable && g_ctx.globals.current_weapon != -1 && !g_ctx.globals.exploits && g_cfg.antiaim.fakelag && g_cfg.antiaim.fakelag_enablers[FAKELAG_PEEK] && g_cfg.antiaim.triggers_fakelag_amount > 6 && !started_peeking && velocity >= 5.0f)
		{
			if ( g_Ragebot->is_peeking_enemy((float)g_cfg.antiaim.triggers_fakelag_amount * 0.5f))
			{
				random_factor = min(rand() % 16 + 1, g_cfg.antiaim.triggers_fakelag_amount);
				switch_ticks = !switch_ticks;
				fluctuate_ticks = switch_ticks ? g_cfg.antiaim.triggers_fakelag_amount : max(g_cfg.antiaim.triggers_fakelag_amount - 2, 1);

				g_ctx.send_packet = true;
				this->started_peeking = true;

				return;
			}
		}

		if (!g_ctx.globals.exploits && g_cfg.antiaim.fakelag && g_cfg.antiaim.fakelag_enablers[FAKELAG_PEEK] && started_peeking)
		{
			if (choked < max_choke)
				g_ctx.send_packet = false;
			else
			{
				started_peeking = false;

				random_factor = min(rand() % 14 + 1, g_cfg.antiaim.triggers_fakelag_amount);
				switch_ticks = !switch_ticks;
				fluctuate_ticks = switch_ticks ? g_cfg.antiaim.triggers_fakelag_amount : max(g_cfg.antiaim.triggers_fakelag_amount - 2, 1);

				g_ctx.send_packet = true;
			}
		}
		else if (!g_ctx.globals.exploits && g_cfg.antiaim.fakelag && velocity >= 5.0f && g_ctx.globals.slowwalking && g_cfg.antiaim.fakelag_enablers[FAKELAG_SLOW_WALK])
		{
			if (choked < max_choke)
				g_ctx.send_packet = false;
			else
			{
				started_peeking = false;

				random_factor = min(rand() % 14 + 1, g_cfg.antiaim.triggers_fakelag_amount);
				switch_ticks = !switch_ticks;
				fluctuate_ticks = switch_ticks ? g_cfg.antiaim.triggers_fakelag_amount : max(g_cfg.antiaim.triggers_fakelag_amount - 2, 1);

				g_ctx.send_packet = true;
			}
		}
		else if (!g_ctx.globals.exploits && g_cfg.antiaim.fakelag && velocity >= 5.0f && !g_ctx.globals.slowwalking && g_ctx.local()->m_fFlags() & FL_ONGROUND && g_EnginePrediction->GetUnpredictedData()->m_nFlags & FL_ONGROUND && g_cfg.antiaim.fakelag_enablers[FAKELAG_MOVE])
		{
			if (choked < max_choke)
				g_ctx.send_packet = false;
			else
			{
				started_peeking = false;

				random_factor = min(rand() % 14 + 1, g_cfg.antiaim.triggers_fakelag_amount);
				switch_ticks = !switch_ticks;
				fluctuate_ticks = switch_ticks ? g_cfg.antiaim.triggers_fakelag_amount : max(g_cfg.antiaim.triggers_fakelag_amount - 2, 1);

				g_ctx.send_packet = true;
			}
		}
		else if (!g_ctx.globals.exploits && g_cfg.antiaim.fakelag && !g_ctx.globals.slowwalking && !(g_ctx.local()->m_fFlags() & FL_ONGROUND && g_EnginePrediction->GetUnpredictedData()->m_nFlags & FL_ONGROUND) && g_cfg.antiaim.fakelag_enablers[FAKELAG_AIR])
		{
			if (choked < max_choke)
				g_ctx.send_packet = false;
			else
			{
				started_peeking = false;

				random_factor = min(rand() % 14 + 1, g_cfg.antiaim.triggers_fakelag_amount);
				switch_ticks = !switch_ticks;
				fluctuate_ticks = switch_ticks ? g_cfg.antiaim.triggers_fakelag_amount : max(g_cfg.antiaim.triggers_fakelag_amount - 2, 1);

				g_ctx.send_packet = true;
			}
		}
		else if (!g_ctx.globals.exploits && g_cfg.antiaim.fakelag)
		{
			max_choke = g_cfg.antiaim.fakelag_amount;

			if (m_gamerules()->m_bIsValveDS())
				max_choke = min(max_choke, 6);

			if (choked < max_choke)
				g_ctx.send_packet = false;
			else
			{
				started_peeking = false;

				random_factor = min(rand() % 14 + 1, g_cfg.antiaim.fakelag_amount);
				switch_ticks = !switch_ticks;
				fluctuate_ticks = switch_ticks ? g_cfg.antiaim.fakelag_amount : max(g_cfg.antiaim.fakelag_amount - 2, 1);

				g_ctx.send_packet = true;
			}
		}


		else if (g_ctx.globals.exploits)
		{
			if (g_ctx.globals.isshifting && g_cfg.ragebot.defensive_doubletap)
			{
				condition = false;
				started_peeking = false;

				if (choked < 13)
					g_ctx.send_packet = false;
				else
					g_ctx.send_packet = true;
			}
			else
			{
				condition = true;
				started_peeking = false;

				if (choked < 2)
					g_ctx.send_packet = false;
				else
					g_ctx.send_packet = true;
			}
		}
		else
			condition = true;
	}
}

void fakelag::Createmove()
{
	if (FakelagCondition(g_ctx.get_command()))
		return;

	Fakelag(g_ctx.get_command());

	if (!m_gamerules()->m_bIsValveDS() && m_clientstate()->iChokedCommands <= 16)
	{
		static auto Fn = util::FindSignature(crypt_str("engine.dll"), crypt_str("B8 ? ? ? ? 3B F0 0F 4F F0 89 5D FC")) + 0x1;
		DWORD old = 0;

		VirtualProtect((void*)Fn, sizeof(uint32_t), PAGE_EXECUTE_READWRITE, &old);
		*(uint32_t*)Fn = 17;
		VirtualProtect((void*)Fn, sizeof(uint32_t), old, &old);
	}
}

bool fakelag::FakelagCondition(CUserCmd* m_pcmd)
{
	condition = false;

	if (g_ctx.local()->m_bGunGameImmunity() || g_ctx.local()->m_fFlags() & FL_FROZEN)
		condition = true;

	if (g_AntiAim->freeze_check && !tickbase::get().double_tap_enabled && !tickbase::get().hide_shots_enabled)
		condition = true;

	return condition;
}