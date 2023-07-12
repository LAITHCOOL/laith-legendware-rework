#include "..\..\includes.hpp"


class tickbase : public singleton <tickbase>
{
public:
	struct tickbase_data_t
	{
		int tickbase;
		int command_number;
		int shift_amount;
		int cmd_diff;
		bool restore_tickbase;
	} data;
	void DoubleTap(CUserCmd* m_pcmd);
	bool CanDoubleTap(bool check_charge);
	void HideShots(CUserCmd* m_pcmd);
	void fix_tickbase(CUserCmd* cmd, int new_command_number, int& tickbase);
	void fix_angle_movement(CUserCmd* cmd, Vector& ang);
	void shift_cmd(CUserCmd* cmd, int amount);
	int adjust_player_time_base(int delta);
	void double_tap_deffensive(CUserCmd* m_pcmd);
	void lagexploit(CUserCmd* m_pcmd);

	bool recharging_double_tap = false;


	bool double_tap_enabled = false;
	bool double_tap_key = false;
	bool double_tap_checkc = false;

	bool bDidPeek = false;
	int lastdoubletaptime = 0;
	bool hide_shots_enabled = false;
	bool hide_shots_key = false;

};