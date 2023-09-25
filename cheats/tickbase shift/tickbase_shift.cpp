#include "tickbase_shift.h"
#include "../misc/misc.h"
#include "../misc/fakelag.h"
#include "..\ragebot\aim.h"
#include "../misc/prediction_system.h"
#include "../prediction/EnginePrediction.h"

void tickbase::shift_silent(CUserCmd* current_cmd, CUserCmd* first_cmd, int amount)
{
	if (!g_ctx.send_packet)
		return;

	// alloc empty cmds
	std::vector<CUserCmd> fake_cmds{};
	fake_cmds.resize(amount);

	// create fake commands & simulate their movement
	for (int i = 0; i < amount; ++i)
	{
		auto cmd = &fake_cmds[i];
		if (cmd != first_cmd)
			std::memcpy(cmd, first_cmd, sizeof(CUserCmd));

		// disable in-game simulation for this cmd
		cmd->m_predicted = true;

		// don't add cmd to prediction & simulation record
		cmd->m_tickcount = INT_MAX;
	}

	// shift cmds
	auto net_channel_info = m_engine()->GetNetChannelInfo();

	if (!net_channel_info)
		return;

	auto command_number = current_cmd->m_command_number + 1;
	auto add_command_number = current_cmd->m_command_number + 1;
	for (int i = 0; i < fake_cmds.size(); ++i)
	{
		auto fake_cmd = &fake_cmds[i];
		auto new_cmd = m_input()->GetUserCmd(command_number);

		if (new_cmd != fake_cmd)
			memcpy(new_cmd, fake_cmd, sizeof(CUserCmd));

		// don't add cmd to prediction & simulation record
		new_cmd->m_tickcount = INT_MAX;

		new_cmd->m_command_number = command_number;
		new_cmd->m_predicted = true;

		auto verified_cmd = m_input()->GetVerifiedUserCmd(command_number);
		auto verfied_cmd_ptr = &verified_cmd->m_cmd;

		if (verfied_cmd_ptr != new_cmd)
			memcpy(verified_cmd, new_cmd, sizeof(CUserCmd));

		verified_cmd->m_crc = new_cmd->GetChecksum();

		++m_clientstate()->iChokedCommands;

		command_number = add_command_number + 1;
		++add_command_number;
	}

	fake_cmds.clear();
}

void tickbase::double_tap_deffensive(CUserCmd* cmd)
{

	if (!g_cfg.ragebot.defensive_doubletap)
		return;

	if (g_Ragebot->ShouldScanPlayer(8, g_Ragebot->m_record, g_Ragebot->m_target))
	{
		if (g_ctx.globals.m_Peek.m_UpdateLc)
		{
			g_ctx.globals.m_Peek.m_UpdateLc = false;
			g_ctx.globals.should_send_packet = true;
			g_ctx.globals.tickbase_shift = 0;
		}
		else
		{
			if (g_ctx.send_packet)
			{
				g_ctx.globals.tickbase_shift = g_ctx.globals.tocharge;
			}
		}

	}
	else
	{
		g_ctx.globals.m_Peek.m_ResetTicks = 0;
		g_ctx.globals.m_Peek.m_UpdateLc = true;

		if (g_ctx.send_packet)
		{
			g_ctx.globals.tickbase_shift = g_ctx.globals.tocharge;
		}
		else if (g_ctx.local()->m_flOldSimulationTime() >= g_ctx.local()->m_flSimulationTime())
		{
			g_ctx.globals.tickbase_shift = g_ctx.local()->m_flOldSimulationTime() - g_ctx.local()->m_flSimulationTime() + 1;
		}
	}
}

void tickbase::DoubleTap(CUserCmd* m_pcmd)
{
	double_tap_enabled = true;

	//vars
	auto shiftAmount = g_cfg.ragebot.shift_amount;
	float shiftTime = shiftAmount * m_globals()->m_intervalpertick;
	float recharge_time = TIME_TO_TICKS(g_cfg.ragebot.recharge_time);
	auto weapon = g_ctx.local()->m_hActiveWeapon();

	g_ctx.globals.block_charge = false;
	//Check if we can doubletap
	if (!CanDoubleTap(false))
		return;



	//Fix for doubletap hitchance
	if (g_ctx.globals.dt_shots == 1) {
		g_ctx.globals.dt_shots = 0;
	}

	//g_ctx.globals.tickbase_shift = shiftAmount;

	//Recharge
	if (!g_Ragebot->m_stop && !(m_pcmd->m_buttons & IN_ATTACK || m_pcmd->m_buttons & IN_ATTACK2 && g_ctx.globals.weapon->is_knife())
		&& (!util::is_button_down(MOUSE_LEFT) || !g_Ragebot->m_working) && g_ctx.globals.tocharge < shiftAmount && (m_pcmd->m_command_number - lastdoubletaptime) > recharge_time)
	{
		lastdoubletaptime = 0;
		g_ctx.globals.startcharge = true;
		g_ctx.globals.tochargeamount = shiftAmount;
		g_ctx.globals.dt_shots = 0;
		g_ctx.globals.block_charge = true;
		
		
	}
	else
		g_ctx.globals.startcharge = false;

	//Do the magic.
	bool restricted_weapon = (g_ctx.globals.weapon->m_iItemDefinitionIndex() == WEAPON_TASER || g_ctx.globals.weapon->m_iItemDefinitionIndex() == WEAPON_REVOLVER
		|| g_ctx.globals.weapon->is_knife() || g_ctx.globals.weapon->is_grenade());
	if ((m_pcmd->m_buttons & IN_ATTACK) && !restricted_weapon && g_ctx.globals.tocharge == shiftAmount && weapon->m_flNextPrimaryAttack() <= m_pcmd->m_command_number - shiftTime)
	{
		g_Misc->fix_autopeek(m_pcmd);

		if (g_ctx.globals.aimbot_working)
		{
			g_ctx.globals.double_tap_aim = true;
			g_ctx.globals.double_tap_aim_check = true;
		}
		g_ctx.globals.shift_ticks = shiftAmount;
		

		/* determine simulation ticks */
		auto m_nSimulationTicks = max(min((m_clientstate()->iChokedCommands + 1), 17), 1);
		/* Get command */
		const int m_CmdNum = m_clientstate()->nLastOutgoingCommand + m_nSimulationTicks;
		/* calc perspective shift this tick */
		auto m_nPerspectiveShift = g_ctx.globals.ticks_allowed - m_nSimulationTicks - 1;
		//g_EnginePrediction->SetTickbase(m_CmdNum, g_EnginePrediction->AdjustPlayerTimeBase(-m_nPerspectiveShift));

		g_ctx.globals.block_charge = true;
		g_ctx.globals.shifting_command_number = m_pcmd->m_command_number; // used for tickbase fix 

		lastdoubletaptime = m_pcmd->m_command_number;

		g_ctx.globals.tickbase_shift = shiftAmount;
		//g_ctx.globals.fixed_tickbase = g_ctx.globals.backup_tickbase - g_ctx.globals.tickbase_shift;
	}
	else
		g_ctx.globals.block_charge = false;

	//g_ctx.globals.tickbase_shift = g_ctx.globals.tochargeamount;
	//g_ctx.globals.fixed_tickbase = g_ctx.globals.backup_tickbase - g_ctx.globals.tickbase_shift;

}

bool tickbase::CanDoubleTap(bool check_charge) {
	//check if DT key is enabled.
	if (!g_cfg.ragebot.double_tap || g_cfg.ragebot.double_tap_key.key <= KEY_NONE || g_cfg.ragebot.double_tap_key.key >= KEY_MAX) {
		double_tap_enabled = false;
		double_tap_key = false;
		lastdoubletaptime = 0;
		g_ctx.globals.tocharge = 0;
		g_ctx.globals.tochargeamount = 0;
		g_ctx.globals.shift_ticks = 0;
		return false;
	}

	//if DT is on, disable hide shots.
	if (double_tap_key && g_cfg.ragebot.double_tap_key.key != g_cfg.antiaim.hide_shots_key.key)
		hide_shots_key = false;

	//disable DT if frozen, fakeducking, revolver etc.
	if (!double_tap_key || g_ctx.local()->m_bGunGameImmunity() || g_ctx.local()->m_fFlags() & FL_FROZEN || m_gamerules()->m_bIsValveDS() || g_ctx.globals.fakeducking) {
		double_tap_enabled = false;
		lastdoubletaptime = 0;
		g_ctx.globals.tocharge = 0;
		g_ctx.globals.tochargeamount = 0;
		g_ctx.globals.shift_ticks = 0;
		return false;
	}

	if (check_charge) {
		if (g_ctx.globals.tochargeamount > 0)
			return false;

		if (g_ctx.globals.startcharge)
			return false;
	}

	return true;
}


void tickbase::HideShots(CUserCmd* m_pcmd)
{
	if (double_tap_key)
		return;

	hide_shots_enabled = true;

	if (!g_cfg.ragebot.enable)
	{
		hide_shots_enabled = false;
		hide_shots_key = false;
		g_ctx.globals.ticks_allowed = 0;
		g_ctx.globals.tickbase_shift = 0;
		g_ctx.globals.next_tickbase_shift = 0;

		return;
	}

	if (!g_cfg.antiaim.hide_shots)
	{
		hide_shots_enabled = false;
		hide_shots_key = false;
		g_ctx.globals.ticks_allowed = 0;
		g_ctx.globals.tickbase_shift = 0;
		g_ctx.globals.next_tickbase_shift = 0;

		return;
	}

	if (g_cfg.antiaim.hide_shots_key.key <= KEY_NONE || g_cfg.antiaim.hide_shots_key.key >= KEY_MAX)
	{
		hide_shots_enabled = false;
		hide_shots_key = false;
		g_ctx.globals.ticks_allowed = 0;
		g_ctx.globals.tickbase_shift = 0;
		g_ctx.globals.next_tickbase_shift = 0;

		return;
	}

	if (double_tap_key)
	{

		hide_shots_enabled = false;
		hide_shots_key = false;
		return;
	}

	if (!hide_shots_key)
	{
		hide_shots_enabled = false;
		g_ctx.globals.ticks_allowed = 0;
		g_ctx.globals.tickbase_shift = 0;
		g_ctx.globals.next_tickbase_shift = 0;
		return;
	}

	double_tap_key = false;

	if (g_ctx.local()->m_bGunGameImmunity() || g_ctx.local()->m_fFlags() & FL_FROZEN)
	{
		hide_shots_enabled = false;
		g_ctx.globals.ticks_allowed = 0;
		g_ctx.globals.tickbase_shift = 0;
		g_ctx.globals.next_tickbase_shift = 0;
		return;
	}

	if (g_ctx.globals.fakeducking)
	{
		hide_shots_enabled = false;
		g_ctx.globals.ticks_allowed = 0;
		g_ctx.globals.tickbase_shift = 0;
		g_ctx.globals.next_tickbase_shift = 0;
		return;
	}

	if (g_AntiAim->freeze_check)
		return;

	g_ctx.globals.next_tickbase_shift = m_gamerules()->m_bIsValveDS() ? 6 : 9;

	auto revolver_shoot = g_ctx.globals.weapon->m_iItemDefinitionIndex() == WEAPON_REVOLVER && !g_ctx.globals.revolver_working && (m_pcmd->m_buttons & IN_ATTACK || m_pcmd->m_buttons & IN_ATTACK2);
	auto weapon_shoot = m_pcmd->m_buttons & IN_ATTACK && g_ctx.globals.weapon->m_iItemDefinitionIndex() != WEAPON_REVOLVER || m_pcmd->m_buttons & IN_ATTACK2 && g_ctx.globals.weapon->is_knife() || revolver_shoot;

	if (g_ctx.send_packet && !g_ctx.globals.weapon->is_grenade() && weapon_shoot)
	{
		g_ctx.globals.tickbase_shift = g_ctx.globals.next_tickbase_shift;
		g_ctx.globals.m_shifted_command = m_pcmd->m_command_number;
		g_ctx.globals.shifting_command_number = m_pcmd->m_command_number; // used for tickbase fix 


		/* predict tickbase for next 2 cmds */
		const int nFirstTick = g_EnginePrediction->AdjustPlayerTimeBase(-g_ctx.globals.next_tickbase_shift);
		const int nSecondTick = g_EnginePrediction->AdjustPlayerTimeBase(1);
		/* set tickbase for next 2 cmds */
		//g_EnginePrediction->SetTickbase(m_pcmd->m_command_number, nFirstTick);
		//g_EnginePrediction->SetTickbase(m_pcmd->m_command_number + 1, nSecondTick);
	}
}

void tickbase::fix_tickbase(CUserCmd* cmd, int new_command_number, int& tickbase)
{
	//auto d = cmd;
	if (cmd->m_command_number <= 0)
		return;

	if (cmd->m_command_number == new_command_number)
		tickbase = g_ctx.local()->m_nTickBase() - g_ctx.globals.shift_ticks + m_globals()->m_simticksthisframe;

	if (cmd->m_command_number + 1 == new_command_number)
		tickbase += g_ctx.globals.shift_ticks - m_globals()->m_simticksthisframe;

}


void fix_angle_movement(CUserCmd* cmd, Vector& ang)
{
	if (!cmd)
		return;

	if (!ang.IsValid())
		ang = cmd->m_viewangles;

	Vector move = Vector(cmd->m_forwardmove, cmd->m_sidemove, 0.f);
	Vector dir = {};

	float delta, len;
	Vector move_angle = {};

	len = move.normalized_float();

	if (!len)
		return;

	math::vector_to_angles(move, move_angle);

	delta = (cmd->m_viewangles.y - ang.y);

	move_angle.y += delta;

	math::angle_to_vectors(move_angle, &dir);

	dir *= len;

	if (g_ctx.local()->get_move_type() == MOVETYPE_LADDER)
	{
		if (cmd->m_viewangles.x >= 45 && ang.x < 45 && std::abs(delta) <= 65)
			dir.x = -dir.x;

		cmd->m_forwardmove = dir.x;
		cmd->m_sidemove = dir.y;

		if (cmd->m_forwardmove > 200)
			cmd->m_buttons |= IN_FORWARD;

		else if (cmd->m_forwardmove < -200)
			cmd->m_buttons |= IN_BACK;

		if (cmd->m_sidemove > 200)
			cmd->m_buttons |= IN_MOVERIGHT;

		else if (cmd->m_sidemove < -200)
			cmd->m_buttons |= IN_MOVELEFT;
	}
	else
	{
		if (cmd->m_viewangles.x < -90 || cmd->m_viewangles.x > 90)
			dir.x = -dir.x;

		cmd->m_forwardmove = dir.x;
		cmd->m_sidemove = dir.y;
	}

	cmd->m_forwardmove = std::clamp(cmd->m_forwardmove, -450.f, 450.f);
	cmd->m_sidemove = std::clamp(cmd->m_sidemove, -450.f, 450.f);
	cmd->m_upmove = std::clamp(cmd->m_upmove, -320.f, 320.f);
}


void tickbase::shift_cmd(CUserCmd* cmd, int amount)
{
	int cmd_number = cmd->m_command_number;
	int cnt = cmd_number - 150 * ((cmd_number + 1) / 150) + 1;
	auto new_cmd = &m_input()->m_pCommands[cnt];

	auto net_chan = m_clientstate()->pNetChannel;
	if (!new_cmd || !net_chan)
		return;

	std::memcpy(new_cmd, cmd, sizeof(CUserCmd));

	new_cmd->m_command_number = cmd->m_command_number + 1;
	new_cmd->m_buttons &= ~0x801u;

	fix_angle_movement(new_cmd, cmd->m_viewangles);

	for (int i = 0; i < amount; ++i)
	{
		int cmd_num = new_cmd->m_command_number + i;

		auto cmd_ = m_input()->GetUserCmd(cmd_num);
		auto verifided_cmd = m_input()->GetVerifiedUserCmd(cmd_num);

		std::memcpy(cmd_, new_cmd, sizeof(CUserCmd));

		cmd_->m_command_number = cmd_num;
		cmd_->m_predicted = cmd_->m_tickcount == INT_MAX;

		std::memcpy(verifided_cmd, cmd_, sizeof(CUserCmd));
		verifided_cmd->m_crc = cmd_->GetChecksum();

		++m_clientstate()->iChokedCommands;
		++net_chan->m_nChokedPackets;
		++net_chan->m_nOutSequenceNr;
	}

	/*	if (shift_timer == 0)
			tick_base.store(g::local()->tick_base(), cmd->command_number, amount, false);*/

	*(int*)((uintptr_t)m_prediction() + 0xC) = -1;
	*(int*)((uintptr_t)m_prediction() + 0x1C) = 0;
}

int tickbase::adjust_player_time_base(int delta) {
	if (delta == -1)
		return false;

	static auto clockcorrection = m_cvar()->FindVar(crypt_str("sv_clockcorrection_msecs"));
	auto flCorrectionSeconds = math::clamp(clockcorrection->GetFloat() / 1000.0f, 0.0f, 1.0f);
	auto correction_ticks = TIME_TO_TICKS(flCorrectionSeconds);

	auto ideal_final_tick = delta + correction_ticks;

	auto estimated_final_tick = g_ctx.local()->m_nTickBase() + 14;

	auto too_fast_limit = ideal_final_tick + correction_ticks;
	auto too_slow_limit = ideal_final_tick - correction_ticks;

	if (estimated_final_tick > too_fast_limit
		|| estimated_final_tick > too_slow_limit)
		return ideal_final_tick - 14 + m_globals()->m_simticksthisframe;
}
