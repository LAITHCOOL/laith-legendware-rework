#pragma once
#include "..\..\includes.hpp"
#include "..\lagcompensation\animation_system.h"


class C_Hitbox
{
public:
	int hitboxID;
	bool isOBB;
	Vector mins;
	Vector maxs;
	float radius;
	int bone;
};

class target
{
public:
	player_t* e;

	adjust_data* last_record;
	adjust_data* history_record;

	target()
	{
		e = nullptr;

		last_record = nullptr;
		history_record = nullptr;
	}

	target(player_t* e, adjust_data* last_record, adjust_data* history_record) //-V818
	{
		this->e = e;

		this->last_record = last_record;
		this->history_record = history_record;
	}
};

class scan_point
{
public:
	Vector point;
	int hitbox;
	bool center;
	float safe;
	int damage;
	int priority;

	Vector vec; // Assuming 'Vector' is the type of the original point

	// Conversion constructor from Vector to scan_point
	scan_point(const Vector& v) : vec(v)
	{
		// Implement any additional logic if needed
	}

	// Conversion function from scan_point to Vector
	operator Vector() const
	{
		return vec;
	}

	scan_point()
	{
		point.Zero();
		hitbox = -1;
		center = false;
		safe = 0.0f;
		priority = 1;
		damage = -1;
	}

	scan_point(const Vector& point, const int& hitbox, const bool& center) //-V818 //-V730
	{
		this->point = point;
		this->hitbox = hitbox;
		this->center = center;
	}

	void reset()
	{
		point.Zero();
		hitbox = -1;
		center = false;
		safe = 0.0f;
	}
};

class scan_data
{
public:
	scan_point point;
	bool visible;
	int damage;
	int hitbox;
	int priority;

	scan_data()
	{
		reset();
	}

	void reset()
	{
		point.reset();
		visible = false;
		damage = -1;
		hitbox = -1;
		priority = 1;
	}

	bool valid()
	{
		return damage >= 1 && hitbox != -1;
	}
};

struct Last_target
{
	adjust_data record;
	scan_data data;
	float distance;
};

class scanned_target
{
public:
	adjust_data* record;
	scan_data data;

	float fov;
	float distance;
	int health;

	scanned_target()
	{
		reset();
	}

	scanned_target(const scanned_target& data) //-V688
	{
		this->record = data.record;
		this->data = data.data;
		this->fov = data.fov;
		this->distance = data.distance;
		this->health = data.health;
	}

	scanned_target& operator=(const scanned_target& data) //-V688
	{
		this->record = data.record;
		this->data = data.data;
		this->fov = data.fov;
		this->distance = data.distance;
		this->health = data.health;

		return *this;
	}

	scanned_target(adjust_data* record, const scan_data& data) //-V688 //-V818
	{
		this->record = record;
		this->data = data;

		Vector viewangles;
		m_engine()->GetViewAngles(viewangles);

		auto aim_angle = math::calculate_angle(g_ctx.globals.eye_pos, data.point.point); //-V688
		auto fov = math::get_fov(viewangles, aim_angle); //-V688

		this->fov = fov;
		this->distance = g_ctx.globals.eye_pos.DistTo(data.point.point);
		this->health = record->player->m_iHealth();
	}

	void reset()
	{
		record = nullptr;
		data.reset();

		fov = 0.0f;
		distance = 0.0f;
		health = 0;
	}
};
static std::vector < std::tuple < float, float, float >> precomputed_seeds = {};

class aim : public singleton <aim>
{

	

	enum CSGOHitboxID : int {
		Head = 0,
		Neck,
		Pelvis,
		Stomach,
		LowerChest,
		Chest,
		UpperChest,
		RightThigh,
		LeftThigh,
		RightCalf,
		LeftCalf,
		LeftFoot,
		RightFoot,
		RightHand,
		LeftHand,
		RightUpperArm,
		RightLowerArm,
		LeftUpperArm,
		LeftLowerArm,
		hitbox_max
	};

	void automatic_revolver(CUserCmd* cmd);
	void PlayerMove(adjust_data* record);


	int get_minimum_damage(bool visible, int health);
	bool ComparePoints(scan_point a, scan_point b, scan_point desired_point);
	void StartScan();
	int GetTicksToShoot();
	int GetTicksToStop();
	void PredictiveQuickStop(CUserCmd* cmd, int idx);
	void QuickStop(CUserCmd* cmd);

	void scan_targets();
	bool automatic_stop(CUserCmd* cmd);
	void find_best_target();
	bool SemiSafe;
	int calc_bt_ticks();
	void fire(CUserCmd* cmd);

	void build_seed_table();
	bool CalculateHitchance1(const Vector& aim_angle, int& final_hitchance);

	std::vector<scan_data> results; // Add this member variable

	scan_data& getResultRef(int i) {
		return results[i];
	}


	

	void BuildSeedTable() {
		if (!precomputed_seeds.empty())
			return;

		for (auto i = 0; i < 255; i++) {
			math::random_seed(i + 1);

			const auto pi_seed = math::random_float(0.f, twopi);

			precomputed_seeds.emplace_back(math::random_float(0.f, 1.f),
				sin(pi_seed), cos(pi_seed));
		}
	}


	std::vector <scanned_target> scanned_targets;
	scanned_target final_target;
public:

	

	adjust_data* get_record_history(std::deque <adjust_data>* records);
	adjust_data* get_record(std::deque <adjust_data>* records);
	bool IsSafePoint(adjust_data* record, Vector start_position, Vector end_position, int hitbox);
	void CalculateHitboxData(C_Hitbox* rtn, player_t* ent, int ihitbox, matrix3x4_t* matrix);
	bool CanSafepoint(adjust_data* record, Vector Eyepos, Vector aimpoint, int hitbox);
	//bool CanSafepoint(adjust_data* record, Vector aimpoint, int hitbox);
	bool SanityCheck(CUserCmd* cmd, bool weapon, int idx, bool check_weapon);
	void run(CUserCmd* cmd);
	std::vector<int> GetHitboxes(adjust_data* record);
	int GetDamage(int health);
	void CalculateDamage(std::vector <scan_point> points, int startIndex, int endIndex, scan_data& data, adjust_data* record, const Vector& shoot_position);
	
	void ComputePointsPriority(adjust_data* m_Record, scan_data &Point);
	void scan(adjust_data* record, scan_data& data, const Vector& shoot_position = g_ctx.globals.eye_pos, bool optimized = false);
	void ThreadedScan(adjust_data* record, scan_data& data, const Vector& shoot_position = g_ctx.globals.eye_pos, bool optimized = false);
	std::vector <int> get_hitboxes(adjust_data* record, bool optimized = false);
	//std::vector<scan_point> get_points(adjust_data* record, int hitbox);
	std::vector<scan_point> get_points(adjust_data* record, int hitbox);
	//bool hitbox_intersection(player_t* e, matrix3x4_t* matrix, int hitbox, const Vector& start, const Vector& end, float* safe = nullptr);
	bool hitbox_intersection(player_t* e, matrix3x4_t* matrix, int hitbox, const Vector& start, const Vector& end);
	void prepare_targets();
	std::vector <target> targets;
	std::vector <adjust_data> backup;

	int last_target_index = -1;
	Last_target last_target[65];

	Vector last_shoot_position;
	bool should_stop;
	int lastshifttime;
	template<typename T, T& Fn>
	void AllThreadsStub(void* data);
	template<typename T, T& Fn>
	void DispatchToAllThreads(adjust_data* record, scan_data* data, const Vector* shoot_position);
};

#pragma once

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
	bool can_intersect_hitbox(const Vector start, const Vector end, Vector spread_dir, adjust_data* log, int hitbox);
	std::vector<hitbox_data_t> get_hitbox_data(adjust_data* log, int hitbox);
	bool intersects_bb_hitbox(Vector start, Vector delta, Vector min, Vector max);
	bool __vectorcall intersects_hitbox(Vector eye_pos, Vector end_pos, Vector min, Vector max, float radius);
	bool can_hit(adjust_data* log, weapon_t* weapon, Vector angles, int hitbox);
private:
	std::array<hit_chance_data_t, 256> hit_chance_records = {};
};

inline hit_chance* g_hit_chance = new hit_chance();