#include "tickbase_shift.h"
#include "../misc/misc.h"
#include "../misc/fakelag.h"
#include "..\ragebot\aim.h"
#include "../misc/prediction_system.h"



void tickbase::double_tap_deffensive(CUserCmd* cmd)
{
	
	g_ctx.globals.next_tickbase_shift = 0;
	g_ctx.globals.tickbase_shift = g_ctx.globals.next_tickbase_shift;
	// predpos
	Vector predicted_eye_pos = g_ctx.globals.eye_pos + (engineprediction::get().backup_data.velocity * m_globals()->m_intervalpertick);

	for (auto i = 1; i <= m_globals()->m_maxclients; i++)
	{
		auto e = static_cast<player_t*>(m_entitylist()->GetClientEntity(i));
		if (!e->valid(true))
			continue;

		auto records = &player_records[i];
		if (records->empty())
			continue;

		auto record = &records->front();
		if (!record->valid())
			continue;

		// apply player animated data
		record->adjust_player();

		// look all ticks for get first hitable
		for (int next_chock = 1; next_chock <= 14; ++next_chock)
		{
			predicted_eye_pos *= next_chock;


			FireBulletData_t fire_data = { };
			/*we scan for pelvis because relativly it has a very low desync amount and its the first body part we detect when peeking*/
			fire_data.damage = CAutoWall::GetDamage(predicted_eye_pos, g_ctx.local(), e->hitbox_position_matrix(HITBOX_PELVIS, record->m_Matricies[MiddleMatrix].data()), &fire_data);
			/*you can do the same for default lw autowall*/

			if (fire_data.damage < 1)
				continue;

			g_ctx.globals.m_Peek.m_bIsPeeking = true;
		}
	}

	if (g_ctx.globals.m_Peek.m_bIsPeeking && !g_ctx.globals.isshifting) /*we dont want to shift while we're recharching because it will cause our dt and tickbase to break*/
	{

		int shift_amount = math::clamp(math::random_int(1, 4), 0, 15); /*we dont want to shift more than 14 and it goes as folows*/
		/* (16 ticks in total we can send without byte patching) 1 tick is for simulating our other 15 commands , 1 tick is for desync, 13 for dt, and 1 is enough to break lc*/

		//shift_cmd(cmd, shift_amount);
		//util::copy_command(cmd, shift_amount);
		g_ctx.globals.next_tickbase_shift = shift_amount;
		g_ctx.globals.tickbase_shift = g_ctx.globals.next_tickbase_shift;
		//g_ctx.globals.defensive_shift_ticks = shift_amount;
		g_ctx.globals.m_Peek.m_bIsPeeking = false;
		g_ctx.globals.m_shifted_command = cmd->m_command_number;

		/* predict tickbase for next 2 cmds */
		const int nFirstTick = engineprediction::get().AdjustPlayerTimeBase(-g_ctx.globals.defensive_shift_ticks);
		const int nSecondTick = engineprediction::get().AdjustPlayerTimeBase(1);

		/* set tickbase for next 2 cmds */
		engineprediction::get().SetTickbase(cmd->m_command_number, nFirstTick);
		engineprediction::get().SetTickbase(cmd->m_command_number + 1, nSecondTick);

		/*now its up to you to set a timer from 50ms up to 400ms i'd recommend, refer to using TICKS_TO_TIME function for that*/
		/*but personally shifting constantly is fine*/
	}
	else
	{
		g_ctx.globals.next_tickbase_shift = 0;
		g_ctx.globals.tickbase_shift = 0;
		//g_ctx.globals.defensive_shift_ticks = shift_amount;
		g_ctx.globals.m_Peek.m_bIsPeeking = false;
	}
	
}

void tickbase::DoubleTap(CUserCmd* m_pcmd)
{
	double_tap_enabled = true;

	if (g_cfg.ragebot.defensive_doubletap)
		double_tap_deffensive(m_pcmd);
	//vars
	auto shiftAmount = g_cfg.ragebot.shift_amount;
	float shiftTime = shiftAmount * m_globals()->m_intervalpertick;
	float recharge_time = TIME_TO_TICKS(g_cfg.ragebot.recharge_time);
	auto weapon = g_ctx.local()->m_hActiveWeapon();


	//Check if we can doubletap
	if (!CanDoubleTap(false))
		return;



	//Fix for doubletap hitchance
	if (g_ctx.globals.dt_shots == 1) {
		g_ctx.globals.dt_shots = 0;
	}
	g_ctx.globals.tickbase_shift = shiftAmount;

	//Recharge
	if (!aim::get().should_stop && !(m_pcmd->m_buttons & IN_ATTACK || m_pcmd->m_buttons & IN_ATTACK2 && g_ctx.globals.weapon->is_knife())
		&& !util::is_button_down(MOUSE_LEFT) && g_ctx.globals.tocharge < shiftAmount && (m_pcmd->m_command_number - lastdoubletaptime) > recharge_time)
	{
		lastdoubletaptime = 0;
		g_ctx.globals.startcharge = true;
		g_ctx.globals.tochargeamount = shiftAmount;
		g_ctx.globals.dt_shots = 0;
	}
	else
		g_ctx.globals.startcharge = false;

	//Do the magic.
	bool restricted_weapon = (g_ctx.globals.weapon->m_iItemDefinitionIndex() == WEAPON_TASER || g_ctx.globals.weapon->m_iItemDefinitionIndex() == WEAPON_REVOLVER
		|| g_ctx.globals.weapon->is_knife() || g_ctx.globals.weapon->is_grenade());
	if ((m_pcmd->m_buttons & IN_ATTACK) && !restricted_weapon && g_ctx.globals.tocharge == shiftAmount && weapon->m_flNextPrimaryAttack() <= m_pcmd->m_command_number - shiftTime)
	{

		misc::get().fix_autopeek(m_pcmd);

		if (g_ctx.globals.aimbot_working)
		{
			g_ctx.globals.double_tap_aim = true;
			g_ctx.globals.double_tap_aim_check = true;
		}
		g_ctx.globals.shift_ticks = shiftAmount;

		//g_ctx.globals.m_shifted_command = m_pcmd->m_command_number;
		g_ctx.globals.shifting_command_number = m_pcmd->m_command_number; // used for tickbase fix 

		lastdoubletaptime = m_pcmd->m_command_number;
	}



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

		return;
	}

	if (!g_cfg.antiaim.hide_shots)
	{
		hide_shots_enabled = false;
		hide_shots_key = false;
		g_ctx.globals.ticks_allowed = 0;
		g_ctx.globals.tickbase_shift = 0;

		return;
	}

	if (g_cfg.antiaim.hide_shots_key.key <= KEY_NONE || g_cfg.antiaim.hide_shots_key.key >= KEY_MAX)
	{
		hide_shots_enabled = false;
		hide_shots_key = false;
		g_ctx.globals.ticks_allowed = 0;
		g_ctx.globals.tickbase_shift = 0;

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
		return;
	}

	double_tap_key = false;

	if (g_ctx.local()->m_bGunGameImmunity() || g_ctx.local()->m_fFlags() & FL_FROZEN)
	{
		hide_shots_enabled = false;
		g_ctx.globals.ticks_allowed = 0;
		g_ctx.globals.tickbase_shift = 0;
		return;
	}

	if (g_ctx.globals.fakeducking)
	{
		hide_shots_enabled = false;
		g_ctx.globals.ticks_allowed = 0;
		g_ctx.globals.tickbase_shift = 0;
		return;
	}

	if (antiaim::get().freeze_check)
		return;

	g_ctx.globals.next_tickbase_shift = m_gamerules()->m_bIsValveDS() ? 6 : 9;

	auto revolver_shoot = g_ctx.globals.weapon->m_iItemDefinitionIndex() == WEAPON_REVOLVER && !g_ctx.globals.revolver_working && (m_pcmd->m_buttons & IN_ATTACK || m_pcmd->m_buttons & IN_ATTACK2);
	auto weapon_shoot = m_pcmd->m_buttons & IN_ATTACK && g_ctx.globals.weapon->m_iItemDefinitionIndex() != WEAPON_REVOLVER || m_pcmd->m_buttons & IN_ATTACK2 && g_ctx.globals.weapon->is_knife() || revolver_shoot;

	if (g_ctx.send_packet && !g_ctx.globals.weapon->is_grenade() && weapon_shoot)
	{
		g_ctx.globals.tickbase_shift = g_ctx.globals.next_tickbase_shift;
		g_ctx.globals.m_shifted_command = m_pcmd->m_command_number;
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

void tickbase::fix_angle_movement(CUserCmd* cmd, Vector& ang)
{
	Vector move = Vector(cmd->m_forwardmove, cmd->m_sidemove, 0.f);
	Vector dir = {};

	float delta, len;
	Vector move_angle = {};

	len = move.normalized_float();

	if (!len)
		return;

	math::VectorAngles(move, move_angle);

	delta = (cmd->m_viewangles.y - ang.y);

	move_angle.y += delta;

	math::AngleVectors(move_angle, &dir);

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

	//fix_angle_movement(new_cmd, g_ctx.globals.wish_angle);
	util::movement_fix(new_cmd->m_viewangles, new_cmd);


	for (int i = 0; i < amount; ++i)
	{
		int cmd_num = new_cmd->m_command_number + i;

		auto cmd_ = m_input()->GetUserCmd(cmd_num);
		auto verifided_cmd = m_input()->GetVerifiedUserCmd(cmd_num);

		std::memcpy(cmd_, new_cmd, sizeof(CUserCmd));



		cmd_->m_command_number = cmd_num;
		cmd_->m_predicted = cmd_->m_tickcount != INT_MAX;

		std::memcpy(verifided_cmd, cmd_, sizeof(CUserCmd));
		verifided_cmd->m_crc = cmd_->GetChecksum();

		++m_clientstate()->iChokedCommands;
		++net_chan->m_nChokedPackets;
		++net_chan->m_nOutSequenceNr;
	}

	/*if (cmd->m_tickcount != INT_MAX && m_clientstate()->iDeltaTick > 0)
		m_prediction()->Update(m_clientstate()->iDeltaTick, true, m_clientstate()->nLastCommandAck, m_clientstate()->nLastOutgoingCommand + m_clientstate()->iChokedCommands);*/

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