#pragma once
#include "..\..\includes.hpp"
#include "..\lagcompensation\animation_system.h"
#include "../lagcompensation/AnimSync/LagComp.hpp"
enum
{
	PREFER_BAIM_ALWAYS,
	PREFER_BAIM_LETHAL,
	PREFER_BAIM_AIR
};
enum
{
	ALWAYS_BAIM_ALWAYS,
	ALWAYS_BAIM_HEATH_BELOW,
	ALWAYS_BAIM_AIR
};
struct HitboxData_t
{
	HitboxData_t() = default;

	mstudiobbox_t* m_pHitbox = nullptr;
	Vector m_vecMins = Vector(0, 0, 0), m_vecMaxs = Vector(0, 0, 0), m_vecCenter = Vector(0, 0, 0);
	Vector m_vecBBMin, m_vecBBMax;
	int m_nHitbox = 0;
	float m_flRadius = 0.0f;
	bool m_bIsCapsule;
};
struct CollisionData_t
{
	Vector m_vecStart, m_vecForward;
	HitboxData_t m_Hitbox;
	matrix3x4_t* aMatrix = nullptr;
	bool m_bResult = false;
};


namespace stdpp {
	// unique_vector
	// goal; only unique elements in a vector while preserving original order
	// aka, no sort (looking at you std::set and std::unique)
	template< typename t >
	class unique_vector {
		using vec_type_t = std::vector< t >;
		using const_iterator_t = typename vec_type_t::const_iterator;

	private:
		vec_type_t m_vector;

	public:
		// ctor.
		__forceinline unique_vector() : m_vector{} {}

		// hood boss ctor.
		__forceinline unique_vector(std::initializer_list< t > l) : m_vector{} {
			// for all elements in the list, add them to our container.
			for (const auto& e : l)
				push_back(e);
		}

		// returns true if container contains element.
		__forceinline bool contains(const t& elem) const {
			if (std::find(begin(), end(), elem) != end())
				return true;

			return false;
		}

		// adds element to tail of container, if not present yet.
		__forceinline void push_back(const t& elem) {
			// check if element does not exist
			if (!contains(elem))
				m_vector.push_back(elem);
		}

		// wrappers.
		__forceinline const_iterator_t begin() const {
			return m_vector.begin();
		}

		__forceinline const_iterator_t end() const {
			return m_vector.end();
		}

		__forceinline void clear() {
			m_vector.clear();
		}

		__forceinline bool empty() {
			return m_vector.empty();
		}
	};
}

enum HitscanMode : int {
	NORMAL = 0,
	LETHAL = 1,
	PREFER = 4,
	PREFER_SAFEPOINT = 5,
};

struct HitscanBox_t {
	int         m_index;
	HitscanMode m_mode;
	bool m_safepoint;

	__forceinline bool operator==(const HitscanBox_t& c) const {
		return m_index == c.m_index && m_mode == c.m_mode && m_safepoint == c.m_safepoint;
	}
};

class HitscanPoint_t
{
public:
	Vector point;
	int hitbox;
	bool center;
public:
	__forceinline HitscanPoint_t() :point{}, hitbox{}, center{} {}
	__forceinline HitscanPoint_t(Vector point, int hitbox, bool center) : point{ point }, hitbox{ hitbox }, center{ center } {}

	__forceinline bool operator==(const HitscanPoint_t& c) const {
		return point == c.point && hitbox == c.hitbox && center == c.center;
	}
};

struct AnimationBackup_t {
	Vector           m_origin, m_abs_origin;
	Vector           m_velocity, m_abs_velocity;
	int              m_flags, m_eflags;
	float            m_duck, m_body;
	AnimationLayer m_layers[13];
};

struct HitscanData_t {
	float  m_damage;
	HitscanPoint_t m_point;
	int m_hitbox;
	bool m_safepoint;
	int m_mode;

	__forceinline HitscanData_t() : m_damage{ 0.f }, m_point{ }, m_hitbox{ }, m_safepoint{}, m_mode{} {}
};

class AimPlayer {
public:
	using hitboxcan_t = stdpp::unique_vector< HitscanBox_t >;

public:
	// essential data.
	player_t* m_player;
	float	  m_spawn;

	// aimbot data.
	hitboxcan_t m_hitboxes;

	// resolve data.
	int       m_shots;
	int       m_missed_shots;
	int       m_last_side_hit;
	int		  m_type;
	int		  m_side;
	int		  m_type_before_bruteforce;
	int		  m_side_before_bruteforce;
	float	  m_resolve_strenght;
	float	  m_resolve_strenght_before_bruteforce;
	float	  m_last_resolve_strenght_hit;

	float m_abs_angles;

	matrix3x4_t* m_matrix;

public:

	void OnNetUpdate(player_t* player);
	void OnRoundStart(player_t* player);
	void SetupHitboxes(LagRecord_t* record, bool history);
	
	bool GetBestAimPosition(HitscanPoint_t& point, float& damage, int& hitbox, bool& safe, LagRecord_t* record, float& min_damage, player_t* player);

public:
	void reset() {
		m_player = nullptr;
		m_shots = 0;
		//m_missed_shots = 0;
		m_last_side_hit = 0;
		m_type = 0;
		m_side = 0;
		m_type_before_bruteforce = 0;
		m_side_before_bruteforce = 0;
		m_resolve_strenght = 0.f;
		m_resolve_strenght_before_bruteforce = 0.f;
		m_last_resolve_strenght_hit = 0.f;

		m_hitboxes.clear();
	}
};
struct BestTarget_t
{
	player_t* player;
	AimPlayer* target;
	HitscanPoint_t point;
	float damage;
	float min_damage;
	LagRecord_t* record;
	int hitbox;
	bool safe;
	float damage_compare;

	BestTarget_t() {
		player = nullptr;
		target = nullptr;
		damage = -1.f;
		point = HitscanPoint_t{};
		record = nullptr;
		hitbox = -1;
		safe = false;
		damage_compare = -1.f;
	}
};

class aimbot {
private:
	struct target_t {
		player_t* m_player;
		AimPlayer* m_data;
	};

	struct knife_target_t {
		target_t  m_target;
	};

	struct table_t {
		uint8_t swing[2][2][2]; // [ first ][ armor ][ back ]
		uint8_t stab[2][2];		  // [ armor ][ back ]
	};

	const table_t m_knife_dmg{ { { { 25, 90 }, { 21, 76 } }, { { 40, 90 }, { 34, 76 } } }, { { 65, 180 }, { 55, 153 } } };

	std::array< Vector, 12 > m_knife_ang{
		Vector{ 0.f, 0.f, 0.f }, Vector{ 0.f, -90.f, 0.f }, Vector{ 0.f, 90.f, 0.f }, Vector{ 0.f, 180.f, 0.f },
		Vector{ -80.f, 0.f, 0.f }, Vector{ -80.f, -90.f, 0.f }, Vector{ -80.f, 90.f, 0.f }, Vector{ -80.f, 180.f, 0.f },
		Vector{ 80.f, 0.f, 0.f }, Vector{ 80.f, -90.f, 0.f }, Vector{ 80.f, 90.f, 0.f }, Vector{ 80.f, 180.f, 0.f }
	};

public:
	std::array< AimPlayer, 64 > m_players;
	std::vector< AimPlayer* >   m_targets;

	// target selection stuff.
	float m_best_dist;
	float m_best_fov;
	float m_best_damage;
	int   m_best_hp;
	float m_best_lag;
	float m_best_height;

	// found target stuff.
	player_t* m_target;
	Vector     m_angle;
	HitscanPoint_t     m_point;
	float      m_damage;
	LagRecord_t* m_record;
	bool	   m_safe;
	std::vector <HitscanPoint_t> m_debug_points;
	bool m_working;
	bool m_firing;
	int m_hitbox;
	bool m_stop;
	bool m_override_damage;
	bool m_force_body;
	bool m_force_safepoint;

	matrix3x4_t* m_current_matrix;
public:
	__forceinline void reset() {
		// reset aimbot data.
		init();

		// reset all players data.
		for (auto& p : m_players)
			p.reset();
	}

	__forceinline bool IsValidTarget(player_t* player) {
		if (!player)
			return false;

		if (!player->is_player())
			return false;

		if (!player->is_alive())
			return false;

		if (player == g_ctx.local())
			return false;

		if (player->m_iTeamNum() == g_ctx.local()->m_iTeamNum())
			return false;

		return true;
	}

	bool CanHit(Vector start, Vector end, LagRecord_t* record, int box, bool in_shot = false, matrix3x4_t* bones = nullptr);
public:
	// aimbot.
	void init();
	bool SetupHitboxPoints(LagRecord_t* record, matrix3x4_t* bones, int index, std::vector< HitscanPoint_t >& points);
	
	bool ShouldScanPlayer(float ticks_to_stop, LagRecord_t* record, player_t* player);
	void think(CUserCmd* m_pcmd);
	void find(CUserCmd* m_pcmd);
	bool HasMaximumAccuracy();
	float CheckHitchance(player_t* player, const Vector& angle, LagRecord_t* record, int hitbox);
	bool SelectTarget(LagRecord_t* record, const HitscanPoint_t& point, float damage);
	void apply(CUserCmd* m_pcmd);
	void AutoStop(CUserCmd* m_pcmd);
	void AutoRevolver(CUserCmd* m_pcmd);
	void AdjustRevolverData(int commandnumber, int buttons);
	//bool IsSafePoint(LagRecord_t* LagRecord, Vector vecStartPosition, Vector vecEndPosition, int iHitbox);
	bool bTraceMeantForHitbox(const Vector& vecEyePosition, const Vector& vecEnd, int iHitbox, LagRecord_t* pRecord);


	HitboxData_t GetHitboxData(player_t* Player, matrix3x4_t* aMatrix, int nHitbox);

	bool CollidePoint(const Vector& vecStart, const Vector& vecEnd, mstudiobbox_t* pHitbox, matrix3x4_t* aMatrix);
	void ThreadedCollisionFunc(CollisionData_t* m_Collision);
	bool IsSafePoint(player_t* Player, LagRecord_t* m_Record, const Vector& vecStart, const Vector& vecEnd, HitboxData_t HitboxData);

	// knifebot.
	void knife(CUserCmd* m_pcmd);
	bool CanKnife(LagRecord_t* record, Vector angle, bool& stab);

	bool KnifeTrace(Vector dir, bool stab, CGameTrace* trace);
	bool KnifeIsBehind(LagRecord_t* record);
};


class hit_chance {
public:
	struct hit_chance_data_t {
		float random[2];
		float inaccuracy[2];
		float spread[2];
	};

	struct hitbox_data_t {
		hitbox_data_t(const Vector& min, const Vector& max, float radius, mstudiobbox_t* hitbox, int bone, const Vector& rotation) {
			m_min = min;
			m_max = max;
			m_radius = radius;
			m_hitbox = hitbox;
			m_bone = bone;
			m_rotation = rotation;
		}

		Vector m_min{ };
		Vector m_max{ };
		float m_radius{ };
		mstudiobbox_t* m_hitbox{ };
		int m_bone{ };
		Vector m_rotation{ };
	};

	void build_seed_table();
	Vector get_spread_direction(weapon_t* weapon, Vector angles, int seed);
	bool can_intersect_hitbox(const Vector start, const Vector end, Vector spread_dir, LagRecord_t* log, int hitbox);
	std::vector<hitbox_data_t> get_hitbox_data(LagRecord_t* log, int hitbox);
	bool intersects_bb_hitbox(Vector start, Vector delta, Vector min, Vector max);
	bool __vectorcall intersects_hitbox(Vector eye_pos, Vector end_pos, Vector min, Vector max, float radius);
	bool can_hit(LagRecord_t* log, weapon_t* weapon, Vector angles, int hitbox);
private:
	std::array<hit_chance_data_t, 256> hit_chance_records = {};
};

inline hit_chance* g_hit_chance = new hit_chance();

inline aimbot* g_Ragebot = new aimbot();