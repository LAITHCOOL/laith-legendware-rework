#include "airstrafe.h"
#include "..\misc\prediction_system.h"
#include "../prediction/EnginePrediction.h"
static auto cl_sidespeed = m_cvar()->FindVar(crypt_str("cl_sidespeed"));
#define CheckIfNonValidNumber(x) (fpclassify(x) == FP_INFINITE || fpclassify(x) == FP_NAN || fpclassify(x) == FP_SUBNORMAL)

void airstrafe::create_move(CUserCmd* m_pcmd, float& wish_yaw) //-V2008
{
	if (g_ctx.local()->get_move_type() == MOVETYPE_LADDER) //-V807
		return;

	if (g_ctx.local()->m_fFlags() & FL_ONGROUND && g_EnginePrediction->GetUnpredictedData()->m_nFlags & FL_ONGROUND)
		return;

	static auto side_speed = m_cvar()->FindVar(crypt_str("cl_sidespeed"))->GetFloat();

	if (g_cfg.misc.airstrafe == 1)
	{
		Vector engine_angles;
		m_engine()->GetViewAngles(engine_angles);

		auto velocity = g_ctx.local()->m_vecVelocity();

		m_pcmd->m_forwardmove = min(5850.0f / velocity.Length2D(), side_speed);
		m_pcmd->m_sidemove = m_pcmd->m_command_number % 2 ? side_speed : -side_speed;

		auto yaw_velocity = math::calculate_angle(Vector(0.0f, 0.0f, 0.0f), velocity).y;
		auto ideal_rotation = math::clamp(RAD2DEG(atan2(15.0f, velocity.Length2D())), 0.0f, 45.0f);

		auto yaw_rotation = fabs(yaw_velocity - engine_angles.y) + (m_pcmd->m_command_number % 2 ? ideal_rotation : -ideal_rotation);
		auto ideal_yaw_rotation = yaw_rotation < 5.0f ? yaw_velocity : engine_angles.y;

		util::RotateMovement(m_pcmd, ideal_yaw_rotation);
	}
	else if (g_cfg.misc.airstrafe == 2 || g_cfg.misc.airstrafe == 3)
	{
		static auto old_yaw = 0.0f;

		auto get_velocity_degree = [](float velocity)
		{
			auto tmp = RAD2DEG(atan2(0.0f, velocity));
			return math::clamp(tmp, 0, 90);
		};

		if (g_ctx.local()->get_move_type() != MOVETYPE_WALK)
			return;

		auto velocity = g_ctx.local()->m_vecVelocity();
		velocity.z = 0.0f;

		auto forwardmove = m_pcmd->m_forwardmove;
		auto sidemove = m_pcmd->m_sidemove;

		static auto flip = false;
		flip = !flip;

		auto turn_direction_modifier = flip ? 1.0f : -1.0f;

		if ((forwardmove || sidemove) && g_cfg.misc.airstrafe == 2)
		{
			m_pcmd->m_forwardmove = 0.0f;
			m_pcmd->m_sidemove = 0.0f;

			auto turn_angle = atan2(-sidemove, forwardmove);
			wish_yaw += turn_angle * M_RADPI;
		}
		else if (forwardmove) //-V550
			m_pcmd->m_forwardmove = 0.0f;

		auto strafe_angle = RAD2DEG(atan(15.0f / velocity.Length2D()));
		math::clamp(strafe_angle, 0.f, 90.f);

		auto temp = Vector(0.0f, wish_yaw - old_yaw, 0.0f);
		temp.y = math::normalize_yaw(temp.y);

		auto yaw_delta = temp.y;
		old_yaw = wish_yaw;

		auto abs_yaw_delta = fabs(yaw_delta);

		if (abs_yaw_delta <= strafe_angle || abs_yaw_delta >= 15.0f)
		{
			Vector velocity_angles;
			math::vector_angles(velocity, velocity_angles);

			temp = Vector(0.0f, wish_yaw - velocity_angles.y, 0.0f);
			temp.y = math::normalize_yaw(temp.y);

			auto velocityangle_yawdelta = temp.y;
			auto velocity_degree = get_velocity_degree(velocity.Length2D());

			if (velocityangle_yawdelta <= velocity_degree || velocity.Length2D() <= 15.0f)
			{
				if (-velocity_degree <= velocityangle_yawdelta || velocity.Length2D() <= 15.0f)
				{
					wish_yaw += strafe_angle * turn_direction_modifier;
					m_pcmd->m_sidemove = side_speed * turn_direction_modifier;
				}
				else
				{
					wish_yaw = velocity_angles.y - velocity_degree;
					m_pcmd->m_sidemove = side_speed;
				}
			}
			else
			{
				wish_yaw = velocity_angles.y + velocity_degree;
				m_pcmd->m_sidemove = -side_speed;
			}
		}
		else if (yaw_delta > 0.0f)
			m_pcmd->m_sidemove = -side_speed;
		else if (yaw_delta < 0.0f)
			m_pcmd->m_sidemove = side_speed;
	}
}