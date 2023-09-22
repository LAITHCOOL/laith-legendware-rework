#pragma once

#include "aim.h"
#include "..\..\includes.hpp"
#include "..\..\sdk\structs.hpp"

struct shot_info
{
	bool should_log = false;
	bool safe = false;
	HitscanPoint_t point;
};

struct shot_record
{
	bool start = false;
	bool end = false;
	bool impacts = false;
	bool latency = false;
	bool hurt_player = false;
	bool impact_hit_player = false;
	bool occlusion = false;

	int fire_tick = INT_MAX;
	int event_fire_tick = INT_MAX;
	int distance = INT_MAX;

	player_t* target = nullptr;

	Vector eye_pos = ZERO;

	LagRecord_t* record = nullptr;

	shot_info shot_info;
};

class shots : public singleton <shots>
{
private:
	std::vector <shot_record> m_shots;
public:
	void register_shot(player_t* target, Vector eye_pos, LagRecord_t* record, int fire_tick, HitscanPoint_t point, bool safe);
	void on_fsn();
	void on_impact(Vector impactpos);
	void on_weapon_fire();
	void on_player_hurt(IGameEvent* event, int user_id);
	void clear_stored_data();
};