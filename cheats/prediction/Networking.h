#pragma once

#include "..\..\includes.hpp"
#include "..\..\sdk\structs.hpp"
#include "..\misc\prediction_system.h"

class networking
{
private:
	

	int final_predicted_tick = 0;
	float interp = 0.0f;
public:
	std::vector<std::pair<float, float>> computed_seeds;

	float latency;
	float flow_outgoing;
	float flow_incoming;
	float average_outgoing;
	float average_incoming;

	void start_move(CUserCmd* m_pcmd, bool& bSendPacket);
	void process_dt_aimcheck(CUserCmd* m_pcmd);
	void packet_cycle(CUserCmd* m_pcmd, bool& bSendPacket);
	bool setup_packet(int sequence_number, bool* pbSendPacket);
	int ping();
	int framerate();
	float tickrate();
	int server_tick();
	void on_packetend(CClientState* client_state);
	void start_network();
	void process_interpolation(ClientFrameStage_t Stage, bool bPostFrame);
	void reset_data();
	void build_seed_table();
	void process_packets(CUserCmd* m_pcmd);
	void finish_packet(CUserCmd* m_pcmd, CVerifiedUserCmd* verified, bool& bSendPacket , bool shifting);
};

inline networking* g_Networking = new networking();