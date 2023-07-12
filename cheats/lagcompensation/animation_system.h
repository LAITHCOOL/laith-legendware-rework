#pragma once

#include "..\..\includes.hpp"
#include "..\..\sdk\structs.hpp"

enum
{
	MAIN,
	NONE,
	FIRST,
	SECOND,
	LOW_FIRST,
	LOW_SECOND,
	LOW_FIRST_20,
	LOW_SECOND_20
};
enum EFixedVelocity
{
	Unresolved = 0,
	MovementLayer,
	AliveLoopLayer,
	LeanLayer
};
enum resolver_type
{
	ORIGINAL,
	LBY,
	LAYERS,
	TRACE,
	DIRECTIONAL,
	ENGINE,
	FREESTAND,
	HURT,
	NON_RESOLVED
};
enum resolver_side
{
	RESOLVER_ORIGINAL,
	RESOLVER_ZERO,
	RESOLVER_FIRST,
	RESOLVER_SECOND,
	RESOLVER_LOW_FIRST,
	RESOLVER_LOW_SECOND,
	RESOLVER_LOW_FIRST_20,
	RESOLVER_LOW_SECOND_20,
	RESOLVER_ON_SHOT
};

enum modes
{
	AIR,
	SLOW_WALKING,
	MOVING,
	STANDING,
	FREESTANDING,
	NO_MODE
};

enum get_side_move
{
	NO_SIDE,
	LEFT,
	RIGHT,
};

enum EMatrixFlags
{
	Interpolated = (1 << 1),
	BoneUsedByHitbox = (1 << 2),
	VisualAdjustment = (1 << 3),
	None = (1 << 4),
};
enum EBoneMatrix
{
	Aimbot,
	Left,
	Center,
	Right,
	Visual
};

enum MatrixBoneSide
{
	LeftMatrix,
	MiddleMatrix,
	RightMatrix,
	LowLeftMatrix,
	LowRightMatrix,
};

struct GameGlobals_t
{
	float m_flCurTime;
	float m_flRealTime;
	float m_flFrameTime;
	float m_flAbsFrameTime;
	float m_flInterpolationAmount;
	int m_nTickCount, m_nFrameCount;

	void AdjustData()
	{
		m_globals()->m_curtime = this->m_flCurTime;
		m_globals()->m_realtime = this->m_flRealTime;
		m_globals()->m_frametime = this->m_flFrameTime;
		m_globals()->m_absoluteframetime = this->m_flAbsFrameTime;
		m_globals()->m_interpolation_amount = this->m_flInterpolationAmount;
		m_globals()->m_tickcount = this->m_nTickCount;
		m_globals()->m_framecount = this->m_nFrameCount;
	}
	void CaptureData()
	{
		this->m_flCurTime = m_globals()->m_curtime;
		this->m_flRealTime = m_globals()->m_realtime;
		this->m_flFrameTime = m_globals()->m_frametime;
		this->m_flAbsFrameTime = m_globals()->m_absoluteframetime;
		this->m_flInterpolationAmount = m_globals()->m_interpolation_amount;
		this->m_nTickCount = m_globals()->m_tickcount;
		this->m_nFrameCount = m_globals()->m_framecount;
	}
};

struct matrixes
{
	matrix3x4_t main[MAXSTUDIOBONES];
	matrix3x4_t zero[MAXSTUDIOBONES];
	matrix3x4_t first[MAXSTUDIOBONES];
	matrix3x4_t second[MAXSTUDIOBONES];
	matrix3x4_t low_first[MAXSTUDIOBONES];
	matrix3x4_t low_second[MAXSTUDIOBONES];
	matrix3x4_t low_first_20[MAXSTUDIOBONES];
	matrix3x4_t low_second_20[MAXSTUDIOBONES];
};

class adjust_data;

class CResolver
{
	player_t* player = nullptr;
	adjust_data* player_record = nullptr;

	bool side = false;
	bool fake = false;
	bool was_first_bruteforce = false;
	bool was_second_bruteforce = false;

	struct
	{
		int missed_shots_corrected[65] = {0};
	}restype[8];


	float lock_side = 0.0f;
	float original_goal_feet_yaw = 0.0f;
	float original_pitch = 0.0f;

public:
	void initialize(player_t* e, adjust_data* record, const float& goal_feet_yaw, const float& pitch);
	void initialize_yaw(player_t* e, adjust_data* record);
	void reset();

	bool IsAdjustingBalance();

	bool is_breaking_lby(AnimationLayer cur_layer, AnimationLayer prev_layer);

	void layer_test();
	void new_layer_resolver();
	void get_side_standing();
	void detect_side();
	void get_side_trace();
	int GetChokedPackets();
	bool DesyncDetect();
	bool update_walk_data();
	void setmode();
	bool MatchShot();
	bool is_slow_walking();


	void final_detection();

	void missed_shots_correction(adjust_data* record, int missed_shots);

	void reset_resolver();

	float b_yaw(player_t* player, float angle, int n);

	void resolve_desync();

	bool hurt_resolver(shot_info* shot);

	//void hurt_resolver();


	AnimationLayer resolver_layers[7][13];
	AnimationLayer previous_layers[13];
	float resolver_goal_feet_yaw[7];



	resolver_side last_side = RESOLVER_ORIGINAL;
	resolver_type type = ORIGINAL;
};

class adjust_data
{
public:
	player_t* player;
	int i;

	AnimationLayer previous_layers[13];
	AnimationLayer layers[13];
	//matrixes matrixes_data;
	std::array < std::array < matrix3x4_t, MAXSTUDIOBONES >, 7 > m_Matricies;
	EFixedVelocity m_nVelocityMode = EFixedVelocity::Unresolved;
	resolver_type type;
	resolver_side side;
	float desync_amount;
	bool break_lagcomp;
	get_side_move curSide;
	modes curMode;

	bool invalid;
	bool immune;
	bool dormant;
	bool bot;
	bool shot;
	int shot_tick;
	int flags;
	int bone_count;
	float last_shot_time;
	bool check_hurt;

	float left;
	float right;
	float Eye;
	float Middle;

	float simulation_time;
	float duck_amount;
	float lby;
	bool flipped_s = false;
	bool low_delta_s = false;

	Vector angles;
	Vector abs_angles;
	Vector velocity;
	Vector origin;
	float m_flAnimationVelocity;
	Vector mins;
	Vector maxs;

	matrix3x4_t leftmatrixes[128] = {};
	matrix3x4_t rightmatrixes[128] = {};

	std::array<float, 24> left_poses = {};
	std::array<float, 24> right_poses = {};

	adjust_data()
	{
		reset();
	}

	void reset()
	{
		player = nullptr;
		i = -1;

		type = ORIGINAL;
		side = RESOLVER_ORIGINAL;
		curSide = NO_SIDE;
		curMode = NO_MODE;
		bot = false;
		invalid = false;
		immune = false;
		dormant = false;
		m_flAnimationVelocity = 0;
		shot = false;
		shot_tick = 0;
		flags = 0;
		bone_count = 0;

		simulation_time = 0.0f;
		duck_amount = 0.0f;
		lby = 0.0f;

		angles.Zero();
		abs_angles.Zero();
		velocity.Zero();
		origin.Zero();
		mins.Zero();
		maxs.Zero();
	}

	adjust_data(player_t* e, bool store = true)
	{
		type = ORIGINAL;
		side = RESOLVER_ORIGINAL;
		curSide = NO_SIDE;
		curMode = NO_MODE;

		invalid = false;
		store_data(e, store);
	}

	void store_data(player_t* e, bool store = true)
	{
		if (!e->is_alive())
			return;

		player = e;
		i = player->EntIndex();

		if (store)
		{
			memcpy(layers, e->get_animlayers(), e->animlayer_count() * sizeof(AnimationLayer));
			memcpy(m_Matricies[2].data(), player->m_CachedBoneData().Base(), player->m_CachedBoneData().Count() * sizeof(matrix3x4_t));
		}

		immune = player->m_bGunGameImmunity() || player->m_fFlags() & FL_FROZEN;
		dormant = player->IsDormant();

		player_info_t player_info;
		m_engine()->GetPlayerInfo(i, &player_info);
		bot = player_info.fakeplayer;

		flags = player->m_fFlags();
		bone_count = player->m_CachedBoneData().Count();

		simulation_time = player->m_flSimulationTime();
		duck_amount = player->m_flDuckAmount();
		lby = player->m_flLowerBodyYawTarget();

		angles = player->m_angEyeAngles();
		abs_angles = player->GetAbsAngles();
		velocity = player->m_vecVelocity();
		origin = player->m_vecOrigin();
		mins = player->GetCollideable()->OBBMins();
		maxs = player->GetCollideable()->OBBMaxs();
	}

	void adjust_player()
	{
		if (!valid(false))
			return;

		memcpy(player->get_animlayers(), layers, player->animlayer_count() * sizeof(AnimationLayer));
		memcpy(player->m_CachedBoneData().Base(), m_Matricies[2].data(), player->m_CachedBoneData().Count() * sizeof(matrix3x4_t));

		player->m_fFlags() = flags;
		player->m_CachedBoneData().m_Size = bone_count;

		player->m_flSimulationTime() = simulation_time;
		player->m_flDuckAmount() = duck_amount;
		player->m_flLowerBodyYawTarget() = lby;

		player->m_angEyeAngles() = angles;
		player->set_abs_angles(abs_angles);
		player->m_vecVelocity() = velocity;
		player->m_vecOrigin() = origin;
		player->set_abs_origin(origin);
		player->GetCollideable()->OBBMins() = mins;
		player->GetCollideable()->OBBMaxs() = maxs;
	}

	bool valid(bool extra_checks = true)
	{
		if (!this)
			return false;

		if (i > 0)
			player = (player_t*)m_entitylist()->GetClientEntity(i);

		if (!player)
			return false;

		if (player->m_lifeState() != LIFE_ALIVE)
			return false;

		if (immune)
			return false;

		if (dormant)
			return false;

		if (!extra_checks)
			return true;

		if (invalid)
			return false;

		auto net_channel_info = m_engine()->GetNetChannelInfo();

		if (!net_channel_info)
			return false;

		static auto sv_maxunlag = m_cvar()->FindVar(crypt_str("sv_maxunlag"));

		auto outgoing = net_channel_info->GetLatency(FLOW_OUTGOING);
		auto incoming = net_channel_info->GetLatency(FLOW_INCOMING);

		auto correct = math::clamp(outgoing + incoming + util::get_interpolation(), 0.0f, sv_maxunlag->GetFloat());

		auto curtime = g_ctx.local()->is_alive() ? TICKS_TO_TIME(g_ctx.globals.fixed_tickbase) : m_globals()->m_curtime;
		auto delta_time = correct - (curtime - simulation_time);

		if (fabs(delta_time) > 0.2f)
			return false;

		auto extra_choke = 0;

		if (g_ctx.globals.fakeducking)
			extra_choke = 14 - m_clientstate()->iChokedCommands;

		auto server_tickcount = extra_choke + m_globals()->m_tickcount + TIME_TO_TICKS(outgoing + incoming);
		auto dead_time = (int)(TICKS_TO_TIME(server_tickcount) - sv_maxunlag->GetFloat());

		if (simulation_time < (float)dead_time)
			return false;

		return true;
	}
	void update(player_t* player);
};

class optimized_adjust_data
{
public:
	int i;
	player_t* player;

	float simulation_time;
	float duck_amount;

	Vector angles;
	Vector origin;
	bool shot;
	int shot_tick;
	optimized_adjust_data()
	{
		reset();
	}

	void reset()
	{
		i = 0;
		player = nullptr;

		simulation_time = 0.0f;
		duck_amount = 0.0f;
		shot = false;
		shot_tick = 0;
		angles.Zero();
		origin.Zero();
	}
};

class lagdata
{
	c_baseplayeranimationstate* animstate;
public:
	float side;
	float realtime = animstate->m_flLastClientSideAnimationUpdateTime;
	float resolving_way;
};
enum resolver_history
{
	HISTORY_UNKNOWN = -1,
	HISTORY_ORIGINAL,
	HISTORY_ZERO,
	HISTORY_DEFAULT,
	HISTORY_LOW
};
extern std::deque <adjust_data> player_records[65];

class lagcompensation : public singleton <lagcompensation>
{
public:
	//void init();
	float GetAngle(player_t* player);
	void apply_interpolation_flags(player_t* e);

	void fsn(ClientFrameStage_t stage);
	void Extrapolate(player_t* pEntity, Vector& vecOrigin, Vector& vecVelocity, int& fFlags, bool bOnGround);
	bool valid(int i, player_t* e);


	void RebuiltLayer6(player_t* player, AnimationLayer* layer_data);

	struct AnimationData_t {
		float m_flPrimaryCycle;
		float m_flMovePlaybackRate;
		float m_flFeetWeight;
		float m_flVelocityLengthXY;
		int m_iMoveState;
	};

	AnimationData_t m_animation_data[65];
	void UpdatePlayerAnimations(player_t* pPlayer, adjust_data* m_LagRecord, c_baseplayeranimationstate* m_AnimationState);
	float BuildFootYaw(player_t* pPlayer, adjust_data* m_LagRecord);
	void SetupBones(player_t* pPlayer, int nBoneMask, matrix3x4_t* aMatrix);
	void GenerateSafePoints(player_t* pPlayer, adjust_data* m_LagRecord);
	void setup_matrix(player_t* e, AnimationLayer* layers, const int& matrix, adjust_data* record);
	void SetupPlayerMatrix(player_t* pPlayer, adjust_data* m_Record, matrix3x4_t* Matrix, int nFlags);
	void update_player_animations(player_t* e);
	void FixPvs(player_t* e);
	void SetupCollision(player_t* pPlayer, adjust_data* m_LagRecord);
	float land_time = 0.0f;
	bool land_in_cycle = false;
	bool is_landed = false;
	bool on_ground = false;
	Vector DeterminePlayerVelocity(player_t* pPlayer, adjust_data* m_LagRecord, adjust_data* m_PrevRecord, c_baseplayeranimationstate* m_AnimationState);
	void HandleDormancyLeaving(player_t* pPlayer, adjust_data* m_Record, c_baseplayeranimationstate* m_AnimationState);
	void extrapolation1(player_t* player, Vector& origin, Vector& velocity, int& flags, bool on_ground, int ticks);
	CResolver player_resolver[65];
	bool is_dormant[65];
	float previous_goal_feet_yaw[65];
private:
};

