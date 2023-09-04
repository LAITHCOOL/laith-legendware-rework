#pragma once
#include <array>
#include <stack>
#include "../../includes.hpp"

enum tick_amt_t
{
	recharge = 16,
	dt_shift = 14,
	hs_shift = 9,
};

class CTickBase
{
private:
	struct tickbase_data_t
	{
		int tickbase;
		int command_number;
		int shift_amount;
		int cmd_diff;
		bool restore_tickbase;
	} data;
public:
	int simulation_amt{ };
	/// <summary>
	/// get data for tickbase restoring after exploit usage
	/// </summary>
	/// <param name="tickbase"> tickbase before shift </param>
	/// <param name="cmd"> last command number when shift began </param>
	/// <param name="shift"> how many commands were shifted </param>
	/// <param name="restore"> should we restore tickbase back? </param>
	/// <param name="cmd_diff"> how many commands created after disabling shift </param>
	void store(int tickbase, int cmd, int shift, bool restore, int cmd_diff = 1);
	void fix(int new_command_number, int& tickbase);
};

inline CTickBase* g_Tickbase = new CTickBase();

class TickbaseManipulation
{
private:
private:
	void force_shift(CUserCmd* cmd, int amount, bool buffer = false);

	bool dt_toggled{};
	bool dt_active{};
	bool dt_off{};
	bool teleport{};

	bool hs_toggled{};
	bool hs_active{};
	bool hs_off{};
public:
	bool charge_dt{};
	bool recharge{};
	bool recharge_finish{};
	bool stop_movement{};
	bool hs_works{};
	bool lag_shift{};
	bool toggle_lag{};
	bool break_lc{};
	bool teleportshift{};

	// they now should be updated always on
	// based on sv_maxusrcmdprocessticks cvar
	struct {
		int recharge{};
		int dt_shift{};
		int hs_shift{};
	} amounts;

	__forceinline void update_amounts() {
		amounts.recharge = 16;
		amounts.dt_shift = 16 - 2;
		amounts.hs_shift = std::max<int>(0, 16 - 5);
	}

	struct cl_move_t {
		bool shift = false;
		bool shifting = false;
		bool valid = false;
		int amount = 0;
	}cl_move;

	int shift{};
	int shift_timer{};
	int charge_ticks{};
	int shift_tick{};
	int dt_bullet{};

	void reset_shift() {
		recharge = false;
		toggle_lag = false;
		lag_shift = false;

		shift = 0;
		shift_timer = 0;
		charge_ticks = 0;
		shift_tick = 0;
	}

	bool Recharged() {
		return charge_ticks >= amounts.recharge && recharge_finish;
	}

	bool Enabled() {
		return (dt_active || hs_active) && this->Recharged();
	}

	bool DTActive() {
		return dt_active && this->Recharged();
	}

	bool HSActive() {
		return !dt_active && hs_active && this->Recharged();
	}

	int GetShootTick() {
		if (g_ctx.globals.weapon && !g_ctx.globals.weapon->is_non_aim() && this->HSActive())
			return hs_shift;

		return 0;
	}

	int tickbase_offset(bool dt = false) {
		if (this->HSActive() && !dt)
			return amounts.hs_shift;

		if (dt && this->DTActive())
			return amounts.dt_shift;

		return 0;
	}

	bool recharging(CUserCmd* cmd);

	void double_tap();
	void hide_shots();
	bool should_shift_cmd(int* new_commands, int* backup_commands, void* ecx, int slot, void* buf, int from, int to);

	void on_pre_predict();
	void on_predict_start();

	void on_cl_move(float sample, bool final_tick);
};
inline TickbaseManipulation* g_Exploits = new TickbaseManipulation();