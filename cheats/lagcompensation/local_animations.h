#include "..\..\includes.hpp"

// class onetap;

struct Local_data
{
	bool visualize_lag = false;

	C_CSGOPlayerAnimationState* fake_animstate = nullptr;

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
			tickbase = INT_MAX;
			send_packet = true;
			anim_cmd = nullptr;
		}

		int tickbase;
		bool send_packet;
		CUserCmd* anim_cmd;
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
	std::array< AnimationLayer, 13 > local_animations::get_animlayers();

	void get_cached_matrix(player_t* player, matrix3x4_t* matrix);
	void reset_data();
	void store_animations_data(CUserCmd* cmd, bool& send_packet);
	void run(CUserCmd* m_pcmd);
	void do_animation_event(Local_data::Anim_data* data);
	void update_local_animations(Local_data::Anim_data* data);
	void update_fake_animations(Local_data::Anim_data* data);
	void setup_shoot_position(CUserCmd* m_pcmd);
	void on_update_clientside_animation(player_t* player);
};