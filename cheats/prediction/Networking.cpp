// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: http://www.viva64.com

#include "networking.h"
#include "..\misc\misc.h"
#include "..\ragebot\aim.h"
#include "../tickbase shift/tickbase_shift.h"
#include "../prediction/EnginePrediction.h"
#define M_PI 3.14159265358979323846f
#define M_PI_2 M_PI * 2
void networking::start_move(CUserCmd* m_pcmd, bool& bSendPacket)
{
	if (hooks::menu_open && g_ctx.globals.focused_on_input)
	{
		m_pcmd->m_buttons = 0;
		m_pcmd->m_forwardmove = 0.0f;
		m_pcmd->m_sidemove = 0.0f;
		m_pcmd->m_upmove = 0.0f;
	}


	if (hooks::menu_open)
	{
		m_pcmd->m_buttons &= ~IN_ATTACK;
		m_pcmd->m_buttons &= ~IN_ATTACK2;
	}

	if (m_pcmd->m_buttons & IN_ATTACK2 && g_cfg.ragebot.enable && g_ctx.globals.weapon->m_iItemDefinitionIndex() == WEAPON_REVOLVER)
		m_pcmd->m_buttons &= ~IN_ATTACK2;


	g_ctx.send_packet = true;
	g_ctx.globals.tickbase_shift = 0;
	g_ctx.globals.double_tap_fire = false;
	g_ctx.globals.force_send_packet = false;
	g_ctx.globals.exploits = tickbase::get().double_tap_key || tickbase::get().hide_shots_key;
	g_ctx.globals.current_weapon = g_ctx.globals.weapon->get_weapon_group(g_cfg.ragebot.enable);
	g_ctx.globals.slowwalking = false;
	g_ctx.globals.original_viewangles = m_pcmd->m_viewangles;
	g_ctx.globals.original_forwardmove = m_pcmd->m_forwardmove;
	g_ctx.globals.original_sidemove = m_pcmd->m_sidemove;

	g_ctx.globals.backup_tickbase = g_ctx.local()->m_nTickBase();

	if (g_ctx.globals.tickbase_shift)
		g_ctx.globals.fixed_tickbase = g_ctx.local()->m_nTickBase() - g_ctx.globals.tickbase_shift;
	else
		g_ctx.globals.fixed_tickbase = g_ctx.globals.backup_tickbase;

	if (hooks::menu_open)
	{
		m_pcmd->m_buttons &= ~IN_ATTACK;
		m_pcmd->m_buttons &= ~IN_ATTACK2;
	}

	if (m_pcmd->m_buttons & IN_ATTACK2 && g_cfg.ragebot.enable && g_ctx.globals.weapon->m_iItemDefinitionIndex() == WEAPON_REVOLVER)
		m_pcmd->m_buttons &= ~IN_ATTACK2;


	if (g_ctx.globals.isshifting)
	{
		m_pcmd->m_buttons &= ~(IN_ATTACK | IN_ATTACK2);
	}

	if (g_cfg.ragebot.enable && !g_ctx.globals.weapon->can_fire(true))
	{
		if (m_pcmd->m_buttons & IN_ATTACK && !g_ctx.globals.weapon->is_non_aim() && g_ctx.globals.weapon->m_iItemDefinitionIndex() != WEAPON_REVOLVER)
			m_pcmd->m_buttons &= ~IN_ATTACK;
		else if ((m_pcmd->m_buttons & IN_ATTACK || m_pcmd->m_buttons & IN_ATTACK2) && (g_ctx.globals.weapon->is_knife() || g_ctx.globals.weapon->m_iItemDefinitionIndex() == WEAPON_REVOLVER) && g_ctx.globals.weapon->m_iItemDefinitionIndex() != WEAPON_HEALTHSHOT)
		{
			if (m_pcmd->m_buttons & IN_ATTACK)
				m_pcmd->m_buttons &= ~IN_ATTACK;

			if (m_pcmd->m_buttons & IN_ATTACK2)
				m_pcmd->m_buttons &= ~IN_ATTACK2;
		}
	}

	if (m_pcmd->m_buttons & IN_FORWARD && m_pcmd->m_buttons & IN_BACK)
	{
		m_pcmd->m_buttons &= ~IN_FORWARD;
		m_pcmd->m_buttons &= ~IN_BACK;
	}

	if (m_pcmd->m_buttons & IN_MOVELEFT && m_pcmd->m_buttons & IN_MOVERIGHT)
	{
		m_pcmd->m_buttons &= ~IN_MOVELEFT;
		m_pcmd->m_buttons &= ~IN_MOVERIGHT;
	}



	g_AntiAim->breaking_lby = false;

	g_Misc->movement.in_forward = m_pcmd->m_buttons & IN_FORWARD;
	g_Misc->movement.in_back = m_pcmd->m_buttons & IN_BACK;
	g_Misc->movement.in_right = m_pcmd->m_buttons & IN_RIGHT;
	g_Misc->movement.in_left = m_pcmd->m_buttons & IN_LEFT;
	g_Misc->movement.in_moveright = m_pcmd->m_buttons & IN_MOVERIGHT;
	g_Misc->movement.in_moveleft = m_pcmd->m_buttons & IN_MOVELEFT;
	g_Misc->movement.in_jump = m_pcmd->m_buttons & IN_JUMP;
}

void networking::process_dt_aimcheck(CUserCmd* m_pcmd)
{
	if (!g_ctx.globals.weapon->is_non_aim())
	{
		auto double_tap_aim_check = false;

		if (m_pcmd->m_buttons & IN_ATTACK && g_ctx.globals.double_tap_aim_check)
		{
			double_tap_aim_check = true;
			g_ctx.globals.double_tap_aim_check = false;
		}

		auto revolver_shoot = g_ctx.globals.weapon->m_iItemDefinitionIndex() == WEAPON_REVOLVER && !g_ctx.globals.revolver_working && (m_pcmd->m_buttons & IN_ATTACK || m_pcmd->m_buttons & IN_ATTACK2);

		if (!double_tap_aim_check && m_pcmd->m_buttons & IN_ATTACK && g_ctx.globals.weapon->m_iItemDefinitionIndex() != WEAPON_REVOLVER || revolver_shoot)
			g_ctx.globals.double_tap_aim = false;
	}

	if (m_globals()->m_tickcount - g_ctx.globals.last_aimbot_shot > 16) //-V807
	{
		g_ctx.globals.double_tap_aim = false;
		g_ctx.globals.double_tap_aim_check = false;
	}
}

void networking::packet_cycle(CUserCmd* m_pcmd, bool& bSendPacket)
{
	if (m_gamerules()->m_bIsValveDS() && m_clientstate()->iChokedCommands >= 6) //-V648
	{
		g_ctx.send_packet = true;
		g_Fakelag->started_peeking = false;
	}
	else if (!m_gamerules()->m_bIsValveDS() && m_clientstate()->iChokedCommands >= 16) //-V648
	{
		g_ctx.send_packet = true;
		g_Fakelag->started_peeking = false;
	}

	if (g_ctx.globals.should_send_packet)
	{
		g_ctx.globals.force_send_packet = true;
		g_ctx.send_packet = true;
		g_Fakelag->started_peeking = false;
	}

	if (g_ctx.globals.should_choke_packet)
	{
		g_ctx.globals.should_choke_packet = false;
		g_ctx.globals.should_send_packet = true;
		g_ctx.send_packet = false;
	}
	auto cmd = m_pcmd;
	if (!g_ctx.globals.weapon->is_non_aim())
	{
		auto double_tap_aim_check = false;

		if (cmd->m_buttons & IN_ATTACK && g_ctx.globals.double_tap_aim_check)
		{
			double_tap_aim_check = true;
			g_ctx.globals.double_tap_aim_check = false;
		}

		auto revolver_shoot = g_ctx.globals.weapon->m_iItemDefinitionIndex() == WEAPON_REVOLVER && !g_ctx.globals.revolver_working && (cmd->m_buttons & IN_ATTACK || cmd->m_buttons & IN_ATTACK2);

		if (cmd->m_buttons & IN_ATTACK && g_ctx.globals.weapon->m_iItemDefinitionIndex() != WEAPON_REVOLVER || revolver_shoot)
		{
			static auto weapon_recoil_scale = m_cvar()->FindVar(crypt_str("weapon_recoil_scale"));

			if (g_cfg.ragebot.enable)
				cmd->m_viewangles -= g_ctx.local()->m_aimPunchAngle() * weapon_recoil_scale->GetFloat();

			if (!g_ctx.globals.fakeducking)
			{
				g_ctx.globals.force_send_packet = true;
				g_ctx.globals.should_choke_packet = true;
				g_ctx.send_packet = true;
				g_Fakelag->started_peeking = false;
			}

			g_ctx.globals.last_eye_pos = g_ctx.globals.eye_pos;
			g_ctx.globals.shot_command = cmd->m_command_number;

			if (!double_tap_aim_check)
				g_ctx.globals.double_tap_aim = false;
		}
	}
	else if (g_ctx.globals.weapon->is_knife() && (cmd->m_buttons & IN_ATTACK || cmd->m_buttons & IN_ATTACK2))
	{
		if (!g_ctx.globals.fakeducking) {
			g_ctx.globals.force_send_packet = true;
			g_ctx.globals.should_choke_packet = true;
			g_ctx.send_packet = true;
			g_Fakelag->started_peeking = false;
		}

		g_ctx.globals.shot_command = cmd->m_command_number;
	}

	if (g_ctx.globals.fakeducking)
		g_ctx.globals.force_send_packet = g_ctx.send_packet;

	tickbase::get().DoubleTap(m_pcmd);
	tickbase::get().double_tap_deffensive(m_pcmd);
	tickbase::get().HideShots(m_pcmd);

	if (g_ctx.globals.isshifting)
		m_pcmd->m_buttons &= ~(IN_ATTACK | IN_ATTACK2);
}

void networking::process_packets(CUserCmd* m_pcmd)
{
	static auto previous_ticks_allowed = g_ctx.globals.ticks_allowed;

	if (g_ctx.send_packet && m_clientstate()->pNetChannel)//
	{
		auto choked_packets = m_clientstate()->pNetChannel->m_nChokedPackets;

		if (choked_packets >= 0)
		{
			auto ticks_allowed = g_ctx.globals.ticks_allowed;
			auto command_number = m_pcmd->m_command_number - choked_packets;

			do
			{
				auto command = &m_input()->m_pCommands[m_pcmd->m_command_number - MULTIPLAYER_BACKUP * (command_number / MULTIPLAYER_BACKUP) - choked_packets];

				if (!command || command->m_tickcount > m_globals()->m_tickcount + 72)
				{
					if (--ticks_allowed < 0)
						ticks_allowed = 0;

					g_ctx.globals.ticks_allowed = ticks_allowed;
				}

				++command_number;
				--choked_packets;
			} while (choked_packets >= 0);
		}
	}//

	if (g_ctx.globals.ticks_allowed > 17)
		g_ctx.globals.ticks_allowed = math::clamp(g_ctx.globals.ticks_allowed - 1, 0, 17);

	if (previous_ticks_allowed && !g_ctx.globals.ticks_allowed)
		g_ctx.globals.ticks_choke = 16;

	previous_ticks_allowed = g_ctx.globals.ticks_allowed;

	if (g_ctx.globals.ticks_choke)
	{
		g_ctx.send_packet = g_ctx.globals.force_send_packet;
		--g_ctx.globals.ticks_choke;
	}


	if (g_ctx.globals.ticks_choke)
	{
		g_ctx.send_packet = g_ctx.globals.force_send_packet;
		--g_ctx.globals.ticks_choke;
	}

	auto& correct = g_ctx.globals.data.emplace_front();

	correct.command_number = m_pcmd->m_command_number;
	correct.choked_commands = m_clientstate()->iChokedCommands + 1;
	correct.tickcount = m_globals()->m_tickcount;

	if (g_ctx.send_packet)
		g_ctx.globals.choked_number.clear();
	else
		g_ctx.globals.choked_number.emplace_back(correct.command_number);

	while (g_ctx.globals.data.size() > (int)(2.0f / m_globals()->m_intervalpertick))
		g_ctx.globals.data.pop_back();

	auto& out = g_ctx.globals.commands.emplace_back();

	out.is_outgoing = g_ctx.send_packet;
	out.is_used = false;
	out.command_number = m_pcmd->m_command_number;
	out.previous_command_number = 0;

	while (g_ctx.globals.commands.size() > (int)(1.0f / m_globals()->m_intervalpertick))
		g_ctx.globals.commands.pop_front();

	if (!g_ctx.send_packet && !m_gamerules()->m_bIsValveDS())//
	{
		auto net_channel = m_clientstate()->pNetChannel;

		if (net_channel->m_nChokedPackets > 0 && !(net_channel->m_nChokedPackets % 4))
		{
			auto backup_choke = net_channel->m_nChokedPackets;
			net_channel->m_nChokedPackets = 0;

			net_channel->send_datagram();
			--net_channel->m_nOutSequenceNr;

			net_channel->m_nChokedPackets = backup_choke;
		}
	}//
}

void networking::finish_packet(CUserCmd* m_pcmd, CVerifiedUserCmd* verified, bool& bSendPacket, bool shifting)
{
	if (!shifting)
	{
		verified->m_cmd = *m_pcmd; // This should be apparently only called if we are not shifting ticks????? 
		verified->m_crc = m_pcmd->GetChecksum();; // This should be apparently only called if we are not shifting ticks?????
	}
	
	g_ctx.globals.in_createmove = false;
	bSendPacket = g_ctx.send_packet;
}

bool networking::setup_packet(int sequence_number, bool* pbSendPacket)
{
	CUserCmd* m_pcmd = m_input()->GetUserCmd(sequence_number);

	if (!m_pcmd || !m_pcmd->m_command_number)
		return false;

	if (!g_ctx.local()->is_alive())
		return false;

	//g_ctx.send_packet = pbSendPacket;
	return true;
}

int networking::framerate()
{
	static auto framerate = 0.0f;
	framerate = 0.9f * framerate + 0.1f * m_globals()->m_absoluteframetime;

	if (framerate <= 0.0f)
		framerate = 1.0f;

	return (int)(1.0f / framerate);
}


void networking::on_packetend(CClientState* client_state)
{
	if (!g_ctx.available())
		return;

	if (!g_ctx.local()->is_alive())
		return;

	if (*(int*)((uintptr_t)client_state + 0x164) != *(int*)((uintptr_t)client_state + 0x16C))
		return;

	//g_Networking->restore_netvar_data(m_clientstate()->nLastCommandAck);
}

int networking::ping()
{
	if (!g_ctx.available())
		return 0;

	auto latency = m_engine()->IsPlayingDemo() ? 0.0f : this->flow_outgoing;
	static 	float updaterate = m_cvar()->FindVar("cl_updaterate")->GetFloat();
	if (latency)
		latency -= 0.5f / updaterate;

	return (int)(max(0.0f, latency) * 1000.0f);
}

float networking::tickrate()
{
	if (!m_engine()->IsConnected())
		return 0.0f;

	return (1.0f / m_globals()->m_intervalpertick);
}

int networking::server_tick()
{
	int extra_choke = 0;

	if (g_ctx.globals.fakeducking)
		extra_choke = 14 - m_clientstate()->iChokedCommands;

	return m_globals()->m_tickcount + TIME_TO_TICKS(this->latency) + extra_choke;
}

void networking::start_network()
{
	if (!g_ctx.available())
		return;

	if (m_engine()->GetNetChannelInfo())
	{
		this->flow_outgoing = m_engine()->GetNetChannelInfo()->GetLatency(FLOW_OUTGOING);
		this->flow_incoming = m_engine()->GetNetChannelInfo()->GetLatency(FLOW_INCOMING);

		this->average_outgoing = m_engine()->GetNetChannelInfo()->GetAvgLatency(FLOW_OUTGOING);
		this->average_incoming = m_engine()->GetNetChannelInfo()->GetAvgLatency(FLOW_INCOMING);

		this->latency = this->flow_outgoing + this->flow_incoming;
	}
}

void networking::process_interpolation(ClientFrameStage_t Stage, bool bPostFrame)
{
	if (Stage != ClientFrameStage_t::FRAME_RENDER_START)
		return;

	if (!g_ctx.available() || !g_ctx.local()->is_alive())
		return;

	if (!bPostFrame)
	{
		this->final_predicted_tick = g_ctx.local()->m_nFinalPredictedTick();
		this->interp = m_globals()->m_interpolation_amount;

		g_ctx.local()->m_nFinalPredictedTick() = g_ctx.local()->m_nTickBase();

		if (g_ctx.globals.isshifting)
			m_globals()->m_interpolation_amount = 0.0f;

		return;
	}

	g_ctx.local()->m_nFinalPredictedTick() = this->final_predicted_tick;
	m_globals()->m_interpolation_amount = 0.0f;
}

void networking::reset_data()
{
	this->latency = 0.f;
	this->flow_incoming = 0.f;
	this->flow_outgoing = 0.f;
	this->average_outgoing = 0.f;
	this->average_incoming = 0.f;
	this->interp = 0.f;

	this->final_predicted_tick = 0;

	//std::memset(this->compress_data, 0, sizeof(this->compress_data));
}

void networking::build_seed_table()
{
	for (auto i = 0; i <= 255; i++) {
		math::random_seed(i + 1);

		const auto pi_seed = math::random_float(0.f, M_PI_2);

		computed_seeds.emplace_back(math::random_float(0.f, 1.f), pi_seed);
	}
}