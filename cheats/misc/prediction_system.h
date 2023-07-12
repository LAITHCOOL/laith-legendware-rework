#pragma once

#include "..\..\includes.hpp"
#include "..\..\sdk\structs.hpp"
struct TickbaseRecord_t
{
	TickbaseRecord_t()
	{
		m_nTickBase = 0;
		m_bIsValidRecord = false;
	}
	TickbaseRecord_t(int nTick)
	{
		m_nTickBase = nTick;
		m_bIsValidRecord = true;
	}

	int m_nTickBase = 0;
	bool m_bIsValidRecord = false;
};
class networking : public singleton <networking>
{
public:
	float latency;
	int final_predicted_tick;
	int out_sequence;
	float flow_outgoing;
	float flow_incoming;
	float tickcount;
public:
	std::string current_time();
	int ping();
	int framerate();
	float tickrate();
	void update_clmove();
	void process_interpolation(ClientFrameStage_t stage, bool postframe);
};

class engineprediction : public singleton <engineprediction>
{
	struct Netvars_data
	{
		int tickbase;

		Vector m_aimPunchAngle;
		Vector m_aimPunchAngleVel;
		Vector m_viewPunchAngle;
		Vector m_vecViewOffset;
		Vector m_vecVelocity;
		Vector m_vecOrigin;

		float m_flDuckAmount;
		float m_flThirdpersonRecoil;
		float m_flFallVelocity;
		float m_flDuckSpeed;

		float m_flRecoilIndex;
		float m_fAccuracyPenalty;
	};

	struct Backup_data
	{
		float velmod = FLT_MIN;
		int flags = 0;
		Vector velocity = ZERO;
	};

	struct Prediction_data
	{
		float old_curtime;
		float old_frametime;

		int old_prediction_random_seed;

		CUserCmd* old_current_command;

		bool old_in_prediction;
		bool old_first_prediction;

		int* prediction_random_seed;
		int* prediction_player;
		CMoveData move_data;
	};

	struct Viewmodel_data
	{
		weapon_t* weapon = nullptr;

		int viewmodel_index = 0;
		int sequence = 0;
		int animation_parity = 0;

		float cycle = 0.0f;
		float animation_time = 0.0f;
	};

	uintptr_t prediction_player;
	uintptr_t prediction_random_seed;

public:
	Netvars_data netvars_data[MULTIPLAYER_BACKUP];

	Backup_data backup_data;
	Prediction_data prediction_data;
	Viewmodel_data viewmodel_data;

	int tickbase;


	struct
	{
		int m_nSimulationTicks = 0;

		std::array < TickbaseRecord_t, MULTIPLAYER_BACKUP > m_aTickBase;
		std::array < int, MULTIPLAYER_BACKUP > m_aGameTickBase;
	} m_TickBase;
	virtual void SetTickbase(int nCommand, int nTickBase)
	{
		m_TickBase.m_aTickBase[nCommand % MULTIPLAYER_BACKUP] = TickbaseRecord_t(nTickBase);
	};
	virtual TickbaseRecord_t* GetTickbaseRecord(int nCommand)
	{
		return &m_TickBase.m_aTickBase[nCommand % MULTIPLAYER_BACKUP];
	}
	void store_netvars(int command_number);
	void restore_netvars(int command_number);
	void StartCommand(player_t* player, CUserCmd* cmd);
	void update();
	void start(CUserCmd* cmd);
	void finish();
	void OnFrameStageNotify(ClientFrameStage_t Stage);
	int AdjustPlayerTimeBase(int nSimulationTicks);
};