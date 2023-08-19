#include "TickbaseManipulation.h"
#include "../misc/misc.h"
#include "../prediction/EnginePrediction.h"
#include "../ragebot/aim.h"
#include "../../hooks/hooks.hpp"
void CTickBase::store(int tickbase, int cmd, int shift, bool restore, int cmd_diff)
{
	this->data.tickbase = tickbase;
	this->data.command_number = cmd;
	this->data.shift_amount = shift;
	this->data.restore_tickbase = restore;
	this->data.cmd_diff = cmd_diff;
}

void CTickBase::fix(int new_command_number, int& tickbase)
{
	auto d = this->data;
	if (d.command_number <= 0)
		return;

	if (d.command_number == new_command_number)
		tickbase = d.tickbase - d.shift_amount + m_globals()->m_simticksthisframe;

	if (d.restore_tickbase && d.command_number + d.cmd_diff == new_command_number)
		tickbase += d.shift_amount - m_globals()->m_simticksthisframe;
}

//void WriteUserCmd(void* buf, CUserCmd* in, CUserCmd* out) {
//	using WriteUserCmd_t = void(__fastcall*)(void*, CUserCmd*, CUserCmd*);
//	static auto Fn = (WriteUserCmd_t)util::FindSignature(crypt_str("client.dll"), crypt_str("55 8B EC 83 E4 F8 51 53 56 8B D9"));
//
//	
//	__asm {
//		mov     ecx, buf
//		mov     edx, in
//		push    out
//		call    Fn
//		add     esp, 4
//	}
//}

//using WriteUsercmdDeltaToBuffer_t = bool(__thiscall*)(void*, int, void*, int, int, bool);
//
//bool __fastcall hooks::ShiftCmdBuffer(int* new_commands, int* backup_commands, void* ecx, int slot, void* buf, int from, int to, bool real_cmd)
//{
//	static auto original_fn = client_hook->get_func_address <WriteUsercmdDeltaToBuffer_t>(24);
//
//	//static auto original = vtables[vtables_t::client].original<write_usercmd_fn>(_number(24));
//
//	auto new_from = -1;
//	auto shift_amount = g_Exploits->m_shift;
//
//	g_Tickbase->simulation_amt = 0;
//	g_Exploits->m_shift = 0;
//
//	auto commands = *new_commands;
//	auto shift_commands = std::clamp(commands + shift_amount, 1, 62);
//	*new_commands = shift_commands;
//	*backup_commands = 0;
//
//	auto next_cmd_nr = m_clientstate()->iChokedCommands + m_clientstate()->nLastOutgoingCommand + 1;
//	auto new_to = next_cmd_nr - commands + 1;
//	if (new_to <= next_cmd_nr) {
//		while (original_fn(ecx, slot, buf, new_from, new_to, true)) {
//			new_from = new_to++;
//
//			if (new_to > next_cmd_nr)
//				goto next_cmd;
//		}
//		return false;
//	}
//next_cmd:
//	if (real_cmd) {
//		auto net_channel = m_clientstate()->pNetChannel;
//		if (net_channel) {
//			++net_channel->m_nOutSequenceNr;
//			++net_channel->m_nChokedPackets;
//		}
//		++m_clientstate()->iChokedCommands;
//	}
//
//	*(int*)((uintptr_t)m_prediction() + 0x1C) = 0;
//	*(int*)((uintptr_t)m_prediction() + 0xC) = -1;
//
//	auto fake_cmd = m_input()->GetUserCmd(slot, new_from);
//	if (!fake_cmd)
//		return true;
//
//	CUserCmd to_cmd;
//	CUserCmd from_cmd;
//
//	from_cmd = *fake_cmd;
//	to_cmd = from_cmd;
//
//	++to_cmd.m_command_number;
//
//	if (real_cmd) {
//		for (int i = 0; i < shift_amount; ++i) {
//			m_prediction()->Update(m_clientstate()->iDeltaTick,
//				m_clientstate()->iDeltaTick > 0,
//				m_clientstate()->nLastCommandAck,
//				m_clientstate()->nLastOutgoingCommand + m_clientstate()->iChokedCommands);
//
//			to_cmd.m_buttons &= ~(IN_ATTACK | IN_ATTACK2);
//
//			auto new_cmd = m_input()->GetUserCmd(to_cmd.m_command_number);
//			auto verified_cmd = m_input()->GetVerifiedUserCmd(to_cmd.m_command_number);
//
//			std::memcpy(new_cmd, &to_cmd, sizeof(CUserCmd));
//			std::memcpy(&verified_cmd->m_cmd, &to_cmd, sizeof(CUserCmd));
//			verified_cmd->m_crc = new_cmd->GetChecksum();
//
//			WriteUserCmd(buf, &to_cmd, &from_cmd);
//			
//			if (i >= shift_amount - 1) {
//				/*auto& out = g_FakeLag.m_cmd.emplace_back();
//				out.m_outgoing = true;
//				out.m_used = false;
//				out.m_cmd = m_clientstate()->nLastOutgoingCommand + m_clientstate()->iChokedCommands + 1;
//				out.m_prev_cmd = 0;*/
//			}
//			else {
//				auto net_channel = m_clientstate()->pNetChannel;
//				if (net_channel) {
//					++net_channel->m_nOutSequenceNr;
//					++net_channel->m_nChokedPackets;
//				}
//				++m_clientstate()->iChokedCommands;
//			}
//
//			from_cmd = to_cmd;
//			++to_cmd.m_command_number;
//		}
//	}
//	else {
//		to_cmd.m_tickcount = INT_MAX;
//		do {
//			WriteUserCmd(buf, &to_cmd, &from_cmd);
//
//			++to_cmd.m_command_number;
//			shift_amount--;
//		} while (shift_amount > 0);
//	}
//
//	g_Tickbase->simulation_amt = shift_commands - commands + 1;
//}

void TickbaseManipulation::ShiftCmd(CUserCmd* cmd, int amount, bool buffer)
{
	if (m_charge_ticks < amount)
		return;

	if (!m_stop_movement)
		m_stop_movement = true;

	if (!m_teleportshift)
		m_teleportshift = true;

	if (buffer) {
		m_shift_tick = g_ctx.get_command()->m_command_number;
		m_shift = amount;
		m_break_lc = false;
		return;
	}

	int number = cmd->m_command_number;

	int cmd_cnt = number - 150 * ((number + 1) / 150) + 1;
	auto new_cmd = &m_input()->m_pCommands[cmd_cnt];

	auto netchan = m_clientstate()->pNetChannel;

	if (!new_cmd || !netchan)
		return;

	std::memcpy(new_cmd, cmd, sizeof(CUserCmd));

	new_cmd->m_command_number = cmd->m_command_number + 1;
	new_cmd->m_buttons &= ~(IN_ATTACK | IN_ATTACK2);

	auto ExtendMovement = [&](CUserCmd* cmd) {
		/*if ((g_cfg.rage.dt_options & 2) && !g_cfg.binds[ap_b].toggled)
			cmd->forwardmove = cmd->sidemove = 0.f;
		else*/
		util::movement_fix_new(g_ctx.globals.original_viewangles, cmd);
	};

	ExtendMovement(new_cmd);

	for (int i = 0; i < amount; ++i) {
		int shift_cmd_number = new_cmd->m_command_number + i;

		auto shift_cmd = m_input()->GetUserCmd(shift_cmd_number);
		auto shift_verified_cmd = m_input()->GetVerifiedUserCmd(shift_cmd_number);

		std::memcpy(shift_cmd, new_cmd, sizeof(CUserCmd));

		shift_cmd->m_command_number = shift_cmd_number;
		shift_cmd->m_predicted = shift_cmd->m_tickcount != INT_MAX;

		std::memcpy(shift_verified_cmd, shift_cmd, sizeof(CUserCmd));
		shift_verified_cmd->m_crc = shift_cmd->GetChecksum();

		++m_clientstate()->iChokedCommands;
		++netchan->m_nChokedPackets;
		++netchan->m_nOutSequenceNr;
	}

	*(uint8_t*)((uintptr_t)m_prediction() + 0x24) = 1; // m_bPreviousAckHadErrors 
	*(uint32_t*)((uintptr_t)m_prediction() + 0x1C) = 0; // m_nCommandsPredicted 
}

bool TickbaseManipulation::IsRecharging(CUserCmd* cmd)
{
	static int choke = 0;
	if (m_recharge) {
		if (choke) {
			g_ctx.send_packet = true;
			choke = 0;
		}
		else {
			if (cmd)
				cmd->InvalidatePackets();

			if (++m_charge_ticks >= recharge) {
				m_recharge = false;
				m_recharge_finish = true;
				m_stop_movement = true;
				if (cmd)
					g_Tickbase->store(g_ctx.local()->m_nTickBase(), cmd->m_command_number, m_charge_ticks, this->HSActive());
			}

			g_ctx.send_packet = m_charge_ticks >= recharge;

			m_shift = 0;
			m_toggle_lag = m_lag_shift = false;
			m_teleportshift = false;
		}
		return true;
	}
	else
		choke = m_clientstate()->iChokedCommands;

	return false;
}

//void TickbaseManipulation::BypassFakelagLimit()
//{
//	auto address = Patterns::send_move_addr.add(1).as<uint8_t*>();
//
//	uint32_t choke_clamp = 17;
//
//	DWORD old_protect = 0;
//	VirtualProtect((void*)address, sizeof(uint32_t), PAGE_EXECUTE_READWRITE, &old_protect);
//	*(uint32_t*)address = choke_clamp;
//	VirtualProtect((void*)address, sizeof(uint32_t), old_protect, &old_protect);
//}

void TickbaseManipulation::OnPrePredict()
{
	static bool toggle_hs = false;
	static bool toggle_dt = false;

	if (hs_active) {
		if (!toggle_hs) {
			m_hs_active = true;
			toggle_hs = true;
		}
	}
	else
		toggle_hs = false;

	if (dt_active) {
		if (!toggle_dt) {
			m_dt_active = true;
			toggle_dt = true;
		}
	}
	else
		toggle_dt = false;

	bool active = m_dt_enabled && m_dt_active || m_hs_enabled && m_hs_active;

	if (m_charge_ticks < recharge && active) {
		m_recharge = true;
		m_recharge_finish = false;
	}
}

void TickbaseManipulation::DoubleTap()
{
	static bool recharge = false;

	if (recharge) {
		recharge = false;
		m_charge_dt = true;
		this->ResetShift();
		return;
	}

	static int last_dt_tick = 0;

	if (m_charge_dt) {
		float shot_diff = std::abs(g_ctx.globals.weapon->m_fLastShotTime() - m_globals()->m_curtime);
		bool shot_finish = shot_diff >= 0.45f;
		if (shot_finish && !g_ctx.globals.aimbot_working) {
			if (!dt_active)
				m_teleport = false;
			else
				m_teleport = true;

			m_dt_bullet = 0;
			m_charge_dt = false;
			m_dt_active = true;
		}
		else if (g_ctx.get_command()->m_buttons & IN_ATTACK) {
			m_dt_bullet++;

			last_dt_tick = m_globals()->m_tickcount;
		}

		m_shift_tick = 0;
		return;
	}

	if (g_ctx.globals.fakeducking) {
		m_dt_enabled = false;

		if (!m_dt_off) {
			m_stop_movement = true;

			if (m_teleport) {
				//this->ShiftCmd(g_ctx.get_command(), dt_shift);
				g_ctx.globals.shift_ticks = dt_shift;
				m_teleport = false;
			}

			m_dt_off = true;
		}

		this->ResetShift();

		return;
	}

	if (!dt_active)
	{
		m_dt_enabled = false;

		if (!hs_active && !m_dt_off)
		{
			m_stop_movement = true;

			if (m_teleport) {
				//this->ShiftCmd(g_ctx.get_command(), dt_shift);
				g_ctx.globals.shift_ticks = dt_shift;
				m_teleport = false;
			}

			this->ResetShift();

			m_dt_off = true;
		}
		return;
	}

	m_dt_enabled = true;
	m_dt_off = false;

	if (!m_recharge_finish)
		return;

	if (!m_teleport)
		m_teleport = true;

	if (g_ctx.globals.weapon->is_non_aim()) {
		m_lag_shift = false;
		m_toggle_lag = false;
		m_shift = dt_shift;
		m_break_lc = true;
		m_shift_timer = 0;
		return;
	}

	if (!g_ctx.get_command()->m_weaponselect && g_Misc->IsFiring()) {
		//this->ShiftCmd(g_ctx.get_command(), dt_shift, true);
		g_ctx.globals.shift_ticks = dt_shift;
		last_dt_tick = m_globals()->m_tickcount;

		m_dt_bullet++;
		m_dt_enabled = false;
		m_dt_active = false;
		m_lag_shift = false;
		m_toggle_lag = false;
		recharge = true;
		g_ctx.send_packet = true;
		return;
	}
	bool peeking = g_Ragebot->is_peeking_enemy(math::clamp(g_EnginePrediction->GetUnpredictedData()->m_vecVelocity.Length2D() / g_ctx.local()->GetMaxPlayerSpeed() * 3.0f, 0.0f, 4.0f), true);
	if (peeking && (g_cfg.ragebot.defensive_doubletap) && g_ctx.globals.weapon->m_iItemDefinitionIndex() != WEAPON_REVOLVER) {
		if (!m_toggle_lag) {
			if (!m_lag_shift) {
				m_shift_timer = dt_shift;
				m_lag_shift = true;
			}

			m_toggle_lag = true;
		}
	}
	else {
		m_lag_shift = false;
		m_toggle_lag = false;
	}

	if (m_lag_shift) {
		if (m_shift_timer == dt_shift && g_ctx.send_packet) {
			m_shift = 0;

			if (m_break_lc)
				g_Tickbase->store(g_ctx.local()->m_nTickBase(), g_ctx.get_command()->m_command_number, -dt_shift + 1, false);

			m_break_lc = false;
			--m_shift_timer;
		}
		else {
			if (m_shift_timer < dt_shift && m_shift_timer > 0) {
				m_shift = dt_shift;

				if (!m_break_lc && g_Tickbase->simulation_amt)
					g_Tickbase->store(g_ctx.local()->m_nTickBase(), g_ctx.get_command()->m_command_number, g_Tickbase->simulation_amt, false);

				m_break_lc = true;
				--m_shift_timer;
			}
		}

		if (m_shift_timer <= 0)
			m_lag_shift = false;
	}
	else {
		m_shift = dt_shift;
		m_break_lc = true;
		m_shift_timer = 0;
	}
}

void TickbaseManipulation::HideShots()
{
	if (g_ctx.globals.fakeducking) {
		m_hs_enabled = false;
		m_hs_works = false;

		if (!m_hs_off) {
			m_stop_movement = true;
			//this->ShiftCmd(g_ctx.get_command(), dt_shift);
			g_ctx.globals.shift_ticks = dt_shift;
			m_hs_off = true;
		}

		this->ResetShift();
		return;
	}

	if (!hs_active || dt_active)
	{
		m_hs_enabled = false;
		m_hs_works = false;

		if (!dt_active && !m_hs_off)
		{
			m_stop_movement = true;
			//this->ShiftCmd(g_ctx.get_command(), dt_shift);
			g_ctx.globals.shift_ticks = dt_shift;
			this->ResetShift();

			m_hs_off = true;
		}
		return;
	}

	m_hs_enabled = true;
	m_hs_off = false;
	m_hs_works = false;

	if (!m_recharge_finish)
		return;

	if (g_ctx.globals.weapon->is_non_aim() || g_ctx.globals.weapon->m_iItemDefinitionIndex() == WEAPON_REVOLVER)
		return;

	int shift_cnt = hs_shift;
	if ((g_ctx.globals.aimbot_shooting|| g_Misc->IsFiring())) {
		if (g_ctx.send_packet) {
			m_hs_works = true;
			m_shift = shift_cnt;
			m_break_lc = true;

			g_Tickbase->store(g_ctx.local()->m_nTickBase(), g_ctx.get_command()->m_command_number, shift_cnt + 1, true);
		}
		else
			g_ctx.get_command()->m_buttons &= ~IN_ATTACK;
	}
	else
		g_ctx.get_command()->m_buttons &= ~IN_ATTACK;
}

void TickbaseManipulation::OnPredictStart()
{
	this->DoubleTap();
	this->HideShots();
}