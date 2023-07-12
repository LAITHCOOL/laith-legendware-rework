#pragma once
#include "..\..\includes.hpp"

namespace m_gAutowall
{
	struct pen_data_t
	{
		player_t* m_hit_player{};
		int					m_dmg{}, m_hitbox{}, m_hitgroup{}, m_remaining_pen{};
		bool did_penetrate{};
	};

	class c_auto_wall
	{
	private:
		bool IsBreakableEntity(IClientEntity* e);
		bool trace_to_exit(
			const Vector& src, const Vector& dir,
			const trace_t& enter_trace, trace_t& exit_trace
		) const;

		void clip_trace_to_player(
			const Vector& src, const Vector& dst, trace_t& trace,
			player_t* const player, const valve::should_hit_fn_t& should_hit_fn
		) const;

		bool handle_bullet_penetration(
			player_t* const shooter, const weapon_info_t* const wpn_data,
			const trace_t& enter_trace, Vector& src, const Vector& dir, int& pen_count,
			float& cur_dmg, const float pen_modifier
		) const;
	public:
		void scale_dmg(
			player_t* const player, float& dmg,
			const float armor_ratio, const float headshot_mult, const int hitgroup
		) const;

		pen_data_t fire_bullet(
			player_t* const shooter, player_t* const target,
			const weapon_info_t* const wpn_data, const bool is_taser, Vector src, const Vector& dst
		) const;

		pen_data_t fire_emulated(
			player_t* const shooter, player_t* const target, Vector src, const Vector& dst
		) const;
	};

	enum e_mask : std::uint32_t {
		solid = 0x200400bu,
		shot_hull = 0x600400bu,
		shot_player = 0x4600400bu,
		contents_hitbox = 0x40000000u,
		npcworldstatic = 0x2000bu,
		surf_hitbox = 0x8000u,
		surf_nodraw = 0x0080u,
		contents_grate = 0x8u,
		contents_solid = 1u,
		contents_current_90 = 0x80000u,
		contents_monster = 0x2000000u,
		contents_opaque = 0x80u,
		contents_ignore_nodraw_opaque = 0x2000u,
		all = 0xffffffffu
	};
	DEFINE_ENUM_FLAG_OPERATORS(e_mask)

	inline const auto g_auto_wall = std::make_unique< c_auto_wall >();
}