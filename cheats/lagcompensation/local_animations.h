#include "..\..\includes.hpp"

// class onetap;

struct Local_data
{
	bool visualize_lag = false;

	C_CSGOPlayerAnimationState* animstate = nullptr;
	C_CSGOPlayerAnimationState* real_animstate = nullptr;
	C_CSGOPlayerAnimationState* dull_animstate = nullptr;

	Vector stored_real_angles = ZERO;
	Vector real_angles = ZERO;
	Vector fake_angles = ZERO;

	struct Anim_data
	{
		Anim_data() {
			reset();
		};
		~Anim_data() {
			reset();
		};

		void reset()
		{
			m_nTickBase = INT_MIN;
			m_command_number = INT_MIN;
			send_packet = true;
			m_viewangles = ZERO;
		}

		int m_nTickBase;
		int m_command_number;
		bool send_packet;
		Vector m_viewangles;
	};
};

class local_animations : public singleton <local_animations>
{
	int m_movetype = 0;
	int m_flags = 0;
	float m_real_spawntime = 0.0f;
	float m_fake_spawntime = 0.0f;

	unsigned long m_fake_handle;
	bool m_should_update_fake;
	bool init_fake_anim;

	bool m_should_update_real;
public:
	AnimationLayer m_real_layers[13];
	float m_real_poses[24];
	float m_real_angles;
public:
	Local_data local_data;
	Local_data::Anim_data anim_data[MULTIPLAYER_BACKUP];
	void OnUPD_ClientSideAnims(player_t* local);
	bool cached_matrix(matrix3x4_t* aMatrix);
	std::array< AnimationLayer, 13 > local_animations::get_animlayers();

	void store_animations_data(CUserCmd* cmd, bool& send_packet);
	void run(CUserCmd* m_pcmd);
	void run_animations(CUserCmd* cmd);
	void do_animation_event();
	void update_local_animations(Local_data::Anim_data* data);
	void update_fake_animations(Local_data::Anim_data* data);
	void update_local_animations_2(Local_data::Anim_data* data);
	void setup_shoot_position(CUserCmd* m_pcmd);
	void build_local_bones(player_t* local);
};