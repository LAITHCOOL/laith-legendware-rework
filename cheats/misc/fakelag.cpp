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
			g_ctx.send_packet = true;
			return;
		}

		if (g_EnginePrediction->GetUnpredictedData()->m_nFlags & FL_ONGROUND && !(g_ctx.local()->m_fFlags() & FL_ONGROUND))
		{
			force_choke = true;
			g_ctx.send_packet = false;
			return;
		}
	}

	static auto fluctuate_ticks = 0;
	static auto switch_ticks = false;
	static auto random_factor = min(rand() % 16 + 1, g_cfg.antiaim.triggers_fakelag_amount);

	auto choked = m_clientstate()->iChokedCommands; //-V807
	auto flags = g_EnginePrediction->GetUnpredictedData()->m_nFlags; //-V807
	auto velocity = g_EnginePrediction->GetUnpredictedData()->m_vecVelocity.Length(); //-V807
	auto velocity2d = g_EnginePrediction->GetUnpredictedData()->m_vecVelocity.Length2D();

	auto max_speed = g_ctx.local()->GetMaxPlayerSpeed();

	auto reloading = false;

	if (g_cfg.antiaim.fakelag_enablers[FAKELAG_ON_RELOAD])
	{
		auto animlayer = g_ctx.local()->get_animlayers()[ANIMATION_LAYER_WEAPON_ACTION];

		if (animlayer.m_nSequence)
		{
			auto activity = g_ctx.local()->sequence_activity(animlayer.m_nSequence);
			reloading = activity == ACT_CSGO_RELOAD && animlayer.m_flWeight;
		}
	}

	switch (g_cfg.antiaim.fakelag_type)
	{
	case 0:
		this->max_choke = g_cfg.antiaim.triggers_fakelag_amount;
		break;
	case 1:
		this->max_choke = random_factor;
		break;
	case 2:
		if (velocity2d >= 5.0f)
		{
			auto dynamic_factor = std::ceilf(64.0f / (velocity2d * m_globals()->m_intervalpertick));

			if (dynamic_factor > 16)
				dynamic_factor = g_cfg.antiaim.triggers_fakelag_amount;

			this->max_choke = dynamic_factor;
		}
		else
			this->max_choke = g_cfg.antiaim.triggers_fakelag_amount;
		break;
	case 3:
		this->max_choke = fluctuate_ticks;
		break;
	}

	if (m_gamerules()->m_bIsValveDS()) //-V807
		this->max_choke = m_engine()->IsVoiceRecording() ? 1 : min(this->max_choke, 6);

	if (g_EnginePrediction->GetUnpredictedData()->m_nFlags & FL_ONGROUND && g_ctx.local()->m_fFlags() & FL_ONGROUND && !m_gamerules()->m_bIsValveDS() && key_binds::get().get_key_bind_state(20)) //-V807
	{
		this->max_choke = 14;

		if (choked < this->max_choke)
			g_ctx.send_packet = false;
		else
			g_ctx.send_packet = true;
	}
	else
	{
		if (g_cfg.ragebot.enable && g_ctx.globals.current_weapon != -1 && !g_ctx.globals.exploits && g_cfg.antiaim.fakelag && g_cfg.antiaim.fakelag_enablers[FAKELAG_ON_PEEK] && g_cfg.antiaim.triggers_fakelag_amount > 6 && !this->started_peeking && velocity >= 5.0f)
		{
			if (g_Ragebot->is_peeking_enemy((float)g_cfg.antiaim.triggers_fakelag_amount * 0.5f))
			{
				random_factor = min(rand() % 16 + 1, g_cfg.antiaim.triggers_fakelag_amount);
				switch_ticks = !switch_ticks;
				fluctuate_ticks = switch_ticks ? g_cfg.antiaim.triggers_fakelag_amount : max(g_cfg.antiaim.triggers_fakelag_amount - 2, 1);

				g_ctx.send_packet = true;
				this->started_peeking = true;

				return;
			}
		}

		if (!g_ctx.globals.exploits && g_cfg.antiaim.fakelag && !(g_EnginePrediction->GetUnpredictedData()->m_nFlags & FL_ONGROUND && g_ctx.local()->m_fFlags() & FL_ONGROUND) && g_cfg.antiaim.fakelag_enablers[FAKELAG_IN_AIR])
		{
			if (choked < this->max_choke)
				g_ctx.send_packet = false;
			else
			{
				this->started_peeking = false;

				random_factor = min(rand() % 16 + 1, g_cfg.antiaim.triggers_fakelag_amount);
				switch_ticks = !switch_ticks;
				fluctuate_ticks = switch_ticks ? g_cfg.antiaim.triggers_fakelag_amount : max(g_cfg.antiaim.triggers_fakelag_amount - 2, 1);

				g_ctx.send_packet = true;
			}
		}
		else if (!g_ctx.globals.exploits && g_cfg.antiaim.fakelag && (g_EnginePrediction->GetUnpredictedData()->m_nFlags & FL_ONGROUND && g_ctx.local()->m_fFlags() & FL_ONGROUND) && g_cfg.antiaim.fakelag_enablers[FAKELAG_ON_LAND])
		{
			if (choked < this->max_choke)
				g_ctx.send_packet = false;
			else
			{
				this->started_peeking = false;

				random_factor = min(rand() % 16 + 1, g_cfg.antiaim.triggers_fakelag_amount);
				switch_ticks = !switch_ticks;
				fluctuate_ticks = switch_ticks ? g_cfg.antiaim.triggers_fakelag_amount : max(g_cfg.antiaim.triggers_fakelag_amount - 2, 1);

				g_ctx.send_packet = true;
			}
		}
		else if (!g_ctx.globals.exploits && g_cfg.antiaim.fakelag && g_cfg.antiaim.fakelag_enablers[FAKELAG_ON_PEEK] && this->started_peeking)
		{
			if (choked < this->max_choke)
				g_ctx.send_packet = false;
			else
			{
				this->started_peeking = false;

				random_factor = min(rand() % 16 + 1, g_cfg.antiaim.triggers_fakelag_amount);
				switch_ticks = !switch_ticks;
				fluctuate_ticks = switch_ticks ? g_cfg.antiaim.triggers_fakelag_amount : max(g_cfg.antiaim.triggers_fakelag_amount - 2, 1);

				g_ctx.send_packet = true;
			}
		}
		else if (!g_ctx.globals.exploits && g_cfg.antiaim.fakelag && g_cfg.antiaim.fakelag_enablers[FAKELAG_ON_SHOT] && (m_pcmd->m_buttons & IN_ATTACK) && g_ctx.globals.weapon->can_fire(true))
		{
			if (choked < this->max_choke)
				g_ctx.send_packet = false;
			else
			{
				this->started_peeking = false;

				random_factor = min(rand() % 16 + 1, g_cfg.antiaim.triggers_fakelag_amount);
				switch_ticks = !switch_ticks;
				fluctuate_ticks = switch_ticks ? g_cfg.antiaim.triggers_fakelag_amount : max(g_cfg.antiaim.triggers_fakelag_amount - 2, 1);

				g_ctx.send_packet = true;
			}
		}
		else if (!g_ctx.globals.exploits && g_cfg.antiaim.fakelag && g_cfg.antiaim.fakelag_enablers[FAKELAG_ON_RELOAD] && reloading)
		{
			if (choked < this->max_choke)
				g_ctx.send_packet = false;
			else
			{
				this->started_peeking = false;

				random_factor = min(rand() % 16 + 1, g_cfg.antiaim.triggers_fakelag_amount);
				switch_ticks = !switch_ticks;
				fluctuate_ticks = switch_ticks ? g_cfg.antiaim.triggers_fakelag_amount : max(g_cfg.antiaim.triggers_fakelag_amount - 2, 1);

				g_ctx.send_packet = true;
			}
		}
		else if (!g_ctx.globals.exploits && g_cfg.antiaim.fakelag && g_cfg.antiaim.fakelag_enablers[FAKELAG_ON_VELOCITY_CHANGE] && abs(velocity - g_ctx.local()->m_vecVelocity().Length()) > 5.0f)
		{
			if (choked < this->max_choke)
				g_ctx.send_packet = false;
			else
			{
				this->started_peeking = false;

				random_factor = min(rand() % 16 + 1, g_cfg.antiaim.triggers_fakelag_amount);
				switch_ticks = !switch_ticks;
				fluctuate_ticks = switch_ticks ? g_cfg.antiaim.triggers_fakelag_amount : max(g_cfg.antiaim.triggers_fakelag_amount - 2, 1);

				g_ctx.send_packet = true;
			}
		}
		else if (!g_ctx.globals.exploits && g_cfg.antiaim.fakelag)
		{
			this->max_choke = g_cfg.antiaim.fakelag_amount;

			if (m_gamerules()->m_bIsValveDS())
				this->max_choke = min(this->max_choke, 6);

			if (choked < this->max_choke)
				g_ctx.send_packet = false;
			else
			{
				this->started_peeking = false;

				random_factor = min(rand() % 16 + 1, g_cfg.antiaim.fakelag_amount);
				switch_ticks = !switch_ticks;
				fluctuate_ticks = switch_ticks ? g_cfg.antiaim.fakelag_amount : max(g_cfg.antiaim.fakelag_amount - 2, 1);

				g_ctx.send_packet = true;
			}
		}
		else if (g_ctx.globals.exploits || !g_AntiAim->condition(m_pcmd, false) && (g_AntiAim->type == ANTIAIM_LEGIT || g_cfg.antiaim.type[g_AntiAim->type].desync)) //-V648
		{
			this->condition = true;
			this->started_peeking = false;
			int target_choke = 1;

			if (g_cfg.ragebot.fakelag_exploits > target_choke)
				target_choke = g_cfg.ragebot.fakelag_exploits;
				

			if (choked < target_choke)
				g_ctx.send_packet = false;
			else
				g_ctx.send_packet = true;
		}
		else
			this->condition = true;
	}

	if (this->force_ticks_allowed)
		return;

	return this->ForceTicksAllowedForProcessing();
}

void fakelag::ForceTicksAllowedForProcessing()
{
	g_ctx.send_packet = false;
	if (m_clientstate()->iChokedCommands < 14)
		return;

	this->force_ticks_allowed = true;
}

void fakelag::Createmove()
{
	if (FakelagCondition(g_ctx.get_command()))
		return;

	Fakelag(g_ctx.get_command());

	/*if (!m_gamerules()->m_bIsValveDS() && m_clientstate()->iChokedCommands <= 16)
	{
		static auto Fn = util::FindSignature(crypt_str("engine.dll"), crypt_str("B8 ? ? ? ? 3B F0 0F 4F F0 89 5D FC")) + 0x1;
		DWORD old = 0;

		VirtualProtect((void*)Fn, sizeof(uint32_t), PAGE_EXECUTE_READWRITE, &old);
		*(uint32_t*)Fn = 17;
		VirtualProtect((void*)Fn, sizeof(uint32_t), old, &old);
	}*/
}
void fakelag::SetMoveChokeClampLimit()
{
	unsigned long protect = 0;
	static auto sendmove_choke_clamp = util::FindSignature(crypt_str("engine.dll"), crypt_str("B8 ? ? ? ? 3B F0 0F 4F F0 89 5D FC")) + 0x1;;
	VirtualProtect((void*)sendmove_choke_clamp, 4, PAGE_EXECUTE_READWRITE, &protect);
	*(uint32_t*)sendmove_choke_clamp = 62;
	VirtualProtect((void*)sendmove_choke_clamp, 4, protect, &protect);
}
bool fakelag::FakelagCondition(CUserCmd* m_pcmd)
{
	this->condition = false;

	if (g_ctx.local()->m_bGunGameImmunity() || g_ctx.local()->m_fFlags() & FL_FROZEN)
		this->condition = true;

	if (g_AntiAim->freeze_check)
		this->condition = true;

	return this->condition;
}