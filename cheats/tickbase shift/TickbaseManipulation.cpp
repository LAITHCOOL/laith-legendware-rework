#include "TickbaseManipulation.h"
#include "../misc/misc.h"
#include "../prediction/EnginePrediction.h"
#include "../ragebot/aim.h"
#include "../../hooks/hooks.hpp"
//bool hs_bind = g_cfg.antiaim.hide_shots_key.key <= KEY_NONE || g_cfg.antiaim.hide_shots_key.key >= KEY_MAX;
bool hs_bind = g_cfg.antiaim.hide_shots && !g_cfg.ragebot.double_tap;
bool dt_bind = g_cfg.ragebot.double_tap;
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

void TickbaseManipulation::force_shift(CUserCmd* cmd, int amount, bool buffer) {
	if (charge_ticks < amount)
		return;

	if (!stop_movement)
		stop_movement = true;

	if (!teleportshift)
		teleportshift = true;

	shift_tick = g_ctx.get_command()->m_command_number;

	cl_move.shift = true;
	cl_move.amount = amount;
}

bool TickbaseManipulation::recharging(CUserCmd* cmd) 
{

	if (!cmd)
		cmd = g_ctx.get_command();

	if (cmd->m_weaponselect)
		return false;

	static int last_choke = 0;
	if (recharge && !last_choke) {
		if (++charge_ticks >= amounts.recharge) {
			recharge = false;
			recharge_finish = true;
			stop_movement = true;
		}

		shift = 0;

		lag_shift = false;
		toggle_lag = false;
		teleportshift = false;
		return true;
	}
	else
		last_choke = m_clientstate()->iChokedCommands;

	return false;
}

void TickbaseManipulation::on_pre_predict() {
	this->update_amounts();

	static bool toggle_hs = false;
	static bool toggle_dt = false;

	if (g_cfg.ragebot.enable && hs_bind) {
		if (!toggle_hs) {
			hs_active = true;
			toggle_hs = true;
		}
	}
	else
		toggle_hs = false;

	if (g_cfg.ragebot.enable && dt_bind) {
		if (!toggle_dt) {
			dt_active = true;
			toggle_dt = true;
		}
	}
	else
		toggle_dt = false;

	bool active = dt_toggled && dt_active || hs_toggled && hs_active;

	if (charge_ticks < amounts.recharge && active) {
		recharge = true;
		recharge_finish = false;
	}
}

void TickbaseManipulation::double_tap() {
	static bool toggle_charge = false;

	if (toggle_charge) {
		toggle_charge = false;
		charge_dt = true;
		this->reset_shift();
		return;
	}

	static int last_dt_tick = 0;

	if (charge_dt) {
		float shot_diff = std::abs(g_ctx.globals.weapon->m_fLastShotTime() - m_globals()->m_curtime);
		bool shot_finish = shot_diff >= 0.3f && !cl_move.shift;
		if (shot_finish && !g_Ragebot->m_working && !m_clientstate()->iChokedCommands) {
			if (!dt_bind)
				teleport = false;
			else
				teleport = true;

			dt_bullet = 0;
			charge_dt = false;
			dt_active = true;
		}
		else if (g_ctx.get_command()->m_buttons & IN_ATTACK) {
			dt_bullet++;

			last_dt_tick = m_globals()->m_tickcount;
		}

		shift_tick = 0;
		return;
	}

	if (g_ctx.globals.fakeducking) {
		dt_toggled = false;

		if (!dt_off) {
			stop_movement = true;

			if (teleport) {
				this->force_shift(g_ctx.get_command(), amounts.dt_shift - 1);
				teleport = false;
			}

			dt_off = true;
		}

		this->reset_shift();

		return;
	}

	if (!dt_bind) {
		dt_toggled = false;

		if (!hs_bind && !dt_off) {
			stop_movement = true;

			if (teleport) {
				this->force_shift(g_ctx.get_command(), amounts.dt_shift - 1);
				teleport = false;
			}

			this->reset_shift();

			dt_off = true;
		}
		return;
	}

	dt_toggled = true;
	dt_off = false;

	if (!recharge_finish)
		return;

	if (!teleport)
		teleport = true;

	if (g_ctx.globals.weapon->is_non_aim() && !g_ctx.globals.weapon->is_knife()) {
		lag_shift = false;
		toggle_lag = false;
		break_lc = true;
		shift = amounts.dt_shift;
		shift_timer = 0;
		return;
	}

	if (!g_ctx.get_command()->m_weaponselect && g_Misc->IsFiring()) {
		this->force_shift(g_ctx.get_command(), amounts.dt_shift - 1, true);

		last_dt_tick = m_globals()->m_tickcount;

		dt_bullet++;

		dt_toggled = false;
		dt_active = false;
		lag_shift = false;
		toggle_lag = false;
		toggle_charge = true;
		return;
	}

	if (g_Ragebot->ShouldScanPlayer(8, g_Ragebot->m_record) && g_ctx.globals.weapon->m_iItemDefinitionIndex() != WEAPON_REVOLVER && !g_ctx.globals.weapon->is_non_aim()) {
		if (!toggle_lag) {
			if (!lag_shift) {
				shift_timer = 0;
				lag_shift = true;
			}

			toggle_lag = true;
		}
	}
	else {
		lag_shift = false;
		toggle_lag = false;
	}

	if (lag_shift) {
		shift = shift_timer > 0 ? amounts.dt_shift : 0;

		if (++shift_timer >= amounts.dt_shift) {
			shift_timer = 0;
			lag_shift = false;
		}
	}
	else {
		shift = amounts.dt_shift;
		shift_timer = 0;
		toggle_lag = false;
	}

	/*static vector3d origin{};
	if (!shift)
		interfaces::debug_overlay->add_text_overlay(g_ctx.local->origin(), 3.f,
			"*");*/
}

void TickbaseManipulation::hide_shots() {
	if (g_ctx.globals.fakeducking) {
		hs_toggled = false;
		hs_works = false;

		if (!hs_off) {
			stop_movement = true;
			this->force_shift(g_ctx.get_command(), amounts.dt_shift - 1);
			hs_off = true;
		}

		this->reset_shift();
		return;
	}

	if (!hs_bind || dt_bind) {
		hs_toggled = false;
		hs_works = false;

		if (!dt_bind && !hs_off) {
			stop_movement = true;
			this->force_shift(g_ctx.get_command(), amounts.dt_shift - 1);
			this->reset_shift();

			hs_off = true;
		}
		return;
	}

	hs_toggled = true;
	hs_off = false;
	hs_works = false;

	if (!recharge_finish)
		return;

	if (g_ctx.globals.weapon->is_non_aim() || g_ctx.globals.weapon->m_iItemDefinitionIndex() == WEAPON_REVOLVER)
		return;

	int shift_cnt = amounts.hs_shift;
	if ((g_Ragebot->m_firing || g_Misc->IsFiring())) {
		if (g_ctx.send_packet) {
			hs_works = true;
			shift = shift_cnt;

			//g_tickbase->store(g_ctx.local->tickbase(), g_ctx.get_command()->m_command_number, shift_cnt + 1, true);
		}
		else
			g_ctx.get_command()->m_buttons &= ~IN_ATTACK;
	}
	else
		g_ctx.get_command()->m_buttons &= ~IN_ATTACK;
}

void TickbaseManipulation::on_predict_start() {
	if (cl_move.shift)
		return;

	this->double_tap();
	this->hide_shots();

	if (!key_binds::get().get_key_bind_state(18))
		g_Ragebot->m_firing = false;
}
