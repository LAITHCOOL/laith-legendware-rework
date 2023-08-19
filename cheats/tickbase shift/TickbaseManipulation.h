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
	void ShiftCmd(CUserCmd* cmd, int amount, bool buffer = false);

	bool m_dt_enabled{ false };
	bool m_dt_active{ false };
	bool m_dt_off{ false };
	bool m_charge_dt{ false };
	bool m_teleport{ false };

	bool m_hs_enabled{ false };
	bool m_hs_active{ false };
	bool m_hs_off{ false };
public:
	bool hs_active = g_cfg.antiaim.hide_shots && g_cfg.antiaim.hide_shots_key.key > KEY_NONE && g_cfg.antiaim.hide_shots_key.key < KEY_MAX;
	bool dt_active = key_binds::get().get_key_bind_state(2) || g_cfg.ragebot.double_tap && g_cfg.ragebot.double_tap_key.key > KEY_NONE && g_cfg.ragebot.double_tap_key.key < KEY_MAX;


	bool m_recharge{ false };
	bool m_recharge_finish{ false };
	bool m_stop_movement{ false };
	bool m_hs_works{ false };
	bool m_lag_shift{ false };
	bool m_toggle_lag{ false };
	bool m_break_lc{ false };
	bool m_teleportshift{ false };

	int m_shift{ 0 };
	int m_shift_timer{ 0 };
	int m_charge_ticks{ 0 };
	int m_shift_tick{ 0 };
	int m_dt_bullet{ 0 };

	__forceinline void ResetShift() {
		m_recharge = false;
		m_toggle_lag = false;
		m_lag_shift = false;

		m_shift = 0;
		m_shift_timer = 0;
		m_charge_ticks = 0;
		m_shift_tick = 0;
	}

	__forceinline bool Recharged() {
		return m_charge_ticks >= recharge && m_recharge_finish;
	}

	__forceinline bool Enabled() {
		return (dt_active || hs_active) && this->Recharged();
	}

	__forceinline bool DTActive() {
		return dt_active && this->Recharged();
	}

	__forceinline bool HSActive() {
		return !dt_active && hs_active && this->Recharged();
	}

	__forceinline int GetShootTick() {
		if (g_ctx.globals.weapon && !g_ctx.globals.weapon->is_non_aim() && this->HSActive())
			return hs_shift;

		return 0;
	}

	bool IsRecharging(CUserCmd* cmd);
	void BypassFakelagLimit();

	void OnPrePredict();
	void DoubleTap();
	void HideShots();
	//bool ShiftCmdBuffer(int* new_commands, int* backup_commands, void* ecx, int slot, void* buf, int from, int to, bool real_cmd);
	void OnPredictStart();
};
inline TickbaseManipulation* g_Exploits = new TickbaseManipulation();