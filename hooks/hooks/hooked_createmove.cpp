// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: http://www.viva64.com

#include "..\hooks.hpp"
#include "..\..\cheats\ragebot\antiaim.h"
#include "..\..\cheats\visuals\other_esp.h"
#include "..\..\cheats\misc\fakelag.h"
#include "..\..\cheats\misc\prediction_system.h"
#include "..\..\cheats\ragebot\aim.h"
#include "..\..\cheats\legitbot\legitbot.h"
#include "..\..\cheats\misc\bunnyhop.h"
#include "..\..\cheats\misc\airstrafe.h"
#include "..\..\cheats\misc\spammers.h"
#include "..\..\cheats\fakewalk\slowwalk.h"
#include "..\..\cheats\misc\misc.h"
#include "..\..\cheats\misc\logs.h"
#include "..\..\cheats\visuals\GrenadePrediction.h"
#include "..\..\cheats\ragebot\knifebot.h"
#include "..\..\cheats\ragebot\zeusbot.h"
#include "..\..\cheats\lagcompensation\local_animations.h"
#include "..\..\cheats\lagcompensation\animation_system.h"
#include "..\..\cheats\tickbase shift\tickbase_shift.h"
#include "../../cheats/lagcompensation/LocalAnimFix.hpp"
#include "../../cheats/lagcompensation/AnimSync/LagComp.hpp"
#include "../../cheats/ragebot/aim.h"
#include "../../cheats/prediction/EnginePrediction.h"
#include "../../cheats/prediction/Networking.h"
#include "../../cheats/tickbase shift/TickbaseManipulation.h"
#include "../../cheats/misc/AutoPeek.h"
using CreateMove_t = void(__thiscall*)(IBaseClientDLL*, int, float, bool);

void __stdcall CreateMove(int sequence_number, float input_sample_frametime, bool active, bool& bSendPacket)
{
	static auto original_fn = hooks::client_hook->get_func_address <CreateMove_t>(22);
	original_fn(m_client(), sequence_number, input_sample_frametime, active);
	g_ctx.local((player_t*)m_entitylist()->GetClientEntity(m_engine()->GetLocalPlayer()), true);

	CUserCmd* m_pcmd = m_input()->GetUserCmd(sequence_number);
	CVerifiedUserCmd* verified = m_input()->GetVerifiedUserCmd(sequence_number);

	g_ctx.globals.in_createmove = false;


	if (!verified || !g_Networking->setup_packet(sequence_number, &bSendPacket))
		return original_fn(m_client(), sequence_number, input_sample_frametime, active);

	if (original_fn)
	{
		m_prediction()->SetLocalViewAngles(m_pcmd->m_viewangles);
		m_engine()->SetViewAngles(m_pcmd->m_viewangles);
	}

	if (!g_ctx.available())
		return;

	g_Misc->rank_reveal();
	g_Spammers->clan_tag();
	g_Misc->ChatSpammer();

	if (!g_ctx.local()->is_alive()) //-V807
		return;

	g_ctx.globals.weapon = g_ctx.local()->m_hActiveWeapon().Get();

	if (!g_ctx.globals.weapon)
		return;

	g_ctx.globals.in_createmove = true;
	g_ctx.set_command(m_pcmd);

	/*CreateMove branch without aimbot*/
	if (g_ctx.globals.isshifting)
	{
		g_Networking->start_move(m_pcmd, g_ctx.send_packet);

		g_ctx.globals.wish_angle = m_pcmd->m_viewangles;
		auto wish_angle = m_pcmd->m_viewangles;

		if (g_cfg.misc.bunnyhop)
			g_Bunnyhop->create_move();

		g_Misc->fast_stop(m_pcmd, wish_angle.y);
		g_Misc->SlideWalk(m_pcmd);
		g_Misc->NoDuck(m_pcmd);
		g_Misc->AutoCrouch(m_pcmd);

		g_GrenadePrediction->Tick(m_pcmd->m_buttons);

		if (g_cfg.misc.crouch_in_air && !(g_ctx.local()->m_fFlags() & FL_ONGROUND))
			m_pcmd->m_buttons |= IN_DUCK;

		/* update prediction */
		g_EnginePrediction->UpdatePrediction();
		/* run main cheat features here */
		g_EnginePrediction->StartPrediction();
		{
			g_ctx.globals.inaccuracy = g_EnginePrediction->GetInaccuracy();
			g_ctx.globals.spread = g_EnginePrediction->GetSpread();
			/* setup shoot position */
			g_LocalAnimations->CopyPlayerAnimationData(false);
			g_LocalAnimations->SetupShootPosition();
			if (g_cfg.misc.airstrafe)
				g_Airstrafe->create_move(m_pcmd, wish_angle.y);

			if (key_binds::get().get_key_bind_state(19) && g_EnginePrediction->GetUnpredictedData()->m_nFlags & FL_ONGROUND && !(g_ctx.local()->m_fFlags() & FL_ONGROUND)) //-V807
				m_pcmd->m_buttons |= IN_JUMP;

			if (key_binds::get().get_key_bind_state(21))
				g_Slowwalk->create_move(m_pcmd);

			if (g_cfg.ragebot.enable && !g_ctx.globals.weapon->is_non_aim() && g_EnginePrediction->GetUnpredictedData()->m_nFlags & FL_ONGROUND && g_ctx.local()->m_fFlags() & FL_ONGROUND)
				g_Slowwalk->create_move(m_pcmd, 0.95f + 0.003125f * (16 - m_clientstate()->iChokedCommands));


			g_Fakelag->Createmove();

			g_ctx.globals.aimbot_working = false;
			g_ctx.globals.revolver_working = false;


			g_Ragebot->think(m_pcmd);

			g_LegitBot->createmove(m_pcmd);

			g_Misc->ax();

			g_AntiAim->desync_angle = 0.0f;
			g_AntiAim->create_move(m_pcmd);

			g_Networking->packet_cycle(m_pcmd, g_ctx.send_packet);

			//g_Misc->automatic_peek(m_pcmd, g_ctx.globals.wish_angle.y);
			//g_Misc->AutoPeek(m_pcmd);
			g_AutoPeek->Run(m_pcmd);
			g_Networking->process_dt_aimcheck(m_pcmd);

		}
		g_EnginePrediction->FinishPrediction();

		if (g_ctx.globals.loaded_script)
			for (auto current : c_lua::get().hooks.getHooks(crypt_str("on_createmove")))
				current.func();

		if (g_cfg.misc.anti_untrusted)
			math::normalize_angles(m_pcmd->m_viewangles);
		else
			m_pcmd->m_viewangles.y = math::normalize_yaw(m_pcmd->m_viewangles.y);

		if (!(g_ctx.local()->m_fFlags() & FL_ONGROUND) && m_pcmd->m_viewangles.z != 0.f)
			m_pcmd->m_viewangles.z = 0.f;

		util::movement_fix_new(wish_angle, m_pcmd);

		g_Networking->process_packets(m_pcmd);

		if (g_ctx.send_packet && g_ctx.globals.should_send_packet)
			g_ctx.globals.should_send_packet = false;

		g_Misc->buybot();

		if (g_ctx.send_packet && !g_ctx.globals.should_send_packet && (!g_ctx.globals.should_choke_packet || (!tickbase::get().hide_shots_enabled && !g_ctx.globals.double_tap_fire)))
		{
			g_LocalAnimations->temp_fakeangle = m_pcmd->m_viewangles;
			math::normalize_angles(g_LocalAnimations->temp_fakeangle);
			math::clamp_angles(g_LocalAnimations->temp_fakeangle);
		}
		g_LocalAnimations->OnCreateMove();

		g_Networking->finish_packet(m_pcmd, verified, bSendPacket, g_ctx.globals.isshifting);


		C_LagComp::get().FinishLagCompensation();
		return;
	}


	/*CreateMove branch with aimbot*/
	g_Networking->start_move(m_pcmd, g_ctx.send_packet);

	g_ctx.globals.wish_angle = m_pcmd->m_viewangles;
	auto wish_angle = m_pcmd->m_viewangles;

	if (g_cfg.misc.bunnyhop)
		g_Bunnyhop->create_move();

	g_Misc->fast_stop(m_pcmd, wish_angle.y);
	g_Misc->SlideWalk(m_pcmd);
	g_Misc->NoDuck(m_pcmd);
	g_Misc->AutoCrouch(m_pcmd);

	g_GrenadePrediction->Tick(m_pcmd->m_buttons);

	if (g_cfg.misc.crouch_in_air && !(g_ctx.local()->m_fFlags() & FL_ONGROUND))
		m_pcmd->m_buttons |= IN_DUCK;

	/* update prediction */
	g_EnginePrediction->UpdatePrediction();
	/* run main cheat features here */
	g_EnginePrediction->StartPrediction();
	{
		g_ctx.globals.inaccuracy = g_EnginePrediction->GetInaccuracy();
		g_ctx.globals.spread = g_EnginePrediction->GetSpread();
		/* setup shoot position */
		g_LocalAnimations->CopyPlayerAnimationData(false);
		g_LocalAnimations->SetupShootPosition();
		if (g_cfg.misc.airstrafe)
			g_Airstrafe->create_move(m_pcmd , wish_angle.y);

		if (key_binds::get().get_key_bind_state(19) && g_EnginePrediction->GetUnpredictedData()->m_nFlags & FL_ONGROUND && !(g_ctx.local()->m_fFlags() & FL_ONGROUND)) //-V807
			m_pcmd->m_buttons |= IN_JUMP;

		if (key_binds::get().get_key_bind_state(21))
			g_Slowwalk->create_move(m_pcmd);

		if (g_cfg.ragebot.enable && !g_ctx.globals.weapon->is_non_aim() && g_EnginePrediction->GetUnpredictedData()->m_nFlags & FL_ONGROUND && g_ctx.local()->m_fFlags() & FL_ONGROUND)
			g_Slowwalk->create_move(m_pcmd, 0.95f + 0.003125f * (16 - m_clientstate()->iChokedCommands));

		
		g_Fakelag->Createmove();

		g_ctx.globals.aimbot_working = false;
		g_ctx.globals.revolver_working = false;

		g_Ragebot->think(m_pcmd);
		g_Ragebot->AutoRevolver(m_pcmd);
		g_LegitBot->createmove(m_pcmd);
		//g_Zeusbot->run(m_pcmd);
		//g_Knifebot->run(m_pcmd);

		g_Misc->ax();

		g_AntiAim->desync_angle = 0.0f;
		g_AntiAim->create_move(m_pcmd);

		g_Networking->packet_cycle(m_pcmd, g_ctx.send_packet);

		g_Misc->automatic_peek(m_pcmd, g_ctx.globals.wish_angle.y);

		g_Networking->process_dt_aimcheck(m_pcmd);
		
	}
	g_EnginePrediction->FinishPrediction();

	if (g_ctx.globals.loaded_script)
		for (auto current : c_lua::get().hooks.getHooks(crypt_str("on_createmove")))
			current.func();

	if (g_cfg.misc.anti_untrusted)
		math::normalize_angles(m_pcmd->m_viewangles);
	else
		m_pcmd->m_viewangles.y = math::normalize_yaw(m_pcmd->m_viewangles.y);
	
	if (!(g_ctx.local()->m_fFlags() & FL_ONGROUND) && m_pcmd->m_viewangles.z != 0.f)
		m_pcmd->m_viewangles.z = 0.f;

	util::movement_fix_new(wish_angle, m_pcmd);

	g_Networking->process_packets(m_pcmd);

	if (g_ctx.send_packet && g_ctx.globals.should_send_packet)
		g_ctx.globals.should_send_packet = false;

	g_Misc->buybot();

	if (g_ctx.send_packet && !g_ctx.globals.should_send_packet && (!g_ctx.globals.should_choke_packet || (!tickbase::get().hide_shots_enabled && !g_ctx.globals.double_tap_fire)))
	{
		g_LocalAnimations->temp_fakeangle = m_pcmd->m_viewangles;
		math::normalize_angles(g_LocalAnimations->temp_fakeangle);
		math::clamp_angles(g_LocalAnimations->temp_fakeangle);
	}
	g_LocalAnimations->OnCreateMove();

	g_Networking->finish_packet(m_pcmd, verified, bSendPacket, g_ctx.globals.isshifting);
	C_LagComp::get().FinishLagCompensation();
	return;
}
__declspec(naked) void __stdcall hooks::hooked_createmove_naked(int sequence_number, float input_sample_frametime, bool active)
{
	__asm
	{
		push ebx
		push esp
		push dword ptr[esp + 8 + 8 + 4]
		push dword ptr[esp + 12 + 8]
		push[esp + 16 + 4]
		call CreateMove
		pop ebx
		retn 12
	}
}