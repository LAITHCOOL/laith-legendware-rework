#pragma once
#include "..\..\includes.hpp"

class penetration : public singleton <penetration> {
public:
	struct PenetrationInput_t {
		player_t* m_from;
		player_t* m_target;
		Vector  m_pos;
		Vector  m_custom_shoot_pos;
		float	m_damage;
		float   m_damage_pen;
		bool	m_can_pen;
	};

	struct PenetrationOutput_t {
		player_t* m_target;
		float   m_damage;
		int     m_hitgroup;
		bool    m_pen;
		bool    m_visible;

		__forceinline PenetrationOutput_t() : m_target{ nullptr }, m_damage{ 0.f }, m_hitgroup{ -1 }, m_pen{ false }, m_visible{ true } {}
	};

	float scale(player_t* player_t, float damage, float armor_ratio, int hitgroup);
	bool  TraceToExit(const Vector& start, const Vector& dir, Vector& out, CGameTrace* enter_trace, CGameTrace* exit_trace);
	void  ClipTraceToPlayer(const Vector& start, const Vector& end, uint32_t mask, CGameTrace* tr, player_t* player, float min);
	bool  run(PenetrationInput_t* in, PenetrationOutput_t* out, bool use_custom_shoot_pos = false);
};