#include "AutoPeek.h"
#include "../prediction/EnginePrediction.h"
#include "../ragebot/aim.h"
void CAutopeek::GotoStart(CUserCmd* cur_cmd)
{
    if (!cur_cmd) {
        cur_cmd = g_ctx.get_command();
    }

    float wish_yaw = g_ctx.globals.wish_angle.y;
    auto difference = g_ctx.local()->GetRenderOrigin() - g_ctx.globals.start_position;

    const auto chocked_ticks = (cur_cmd->m_command_number % 2) != 1
        ? (g_cfg.ragebot.shift_amount - m_clientstate()->iChokedCommands) : m_clientstate()->iChokedCommands;

    static auto cl_forwardspeed = m_cvar()->FindVar("cl_forwardspeed");

    if (difference.Length2D() > 5.0f)
    {
        auto angle = math::calculate_angle(g_ctx.local()->GetRenderOrigin(), g_ctx.globals.start_position);
        g_ctx.globals.wish_angle.y = angle.y;

        cur_cmd->m_forwardmove = cl_forwardspeed->GetFloat() - (1.2f * chocked_ticks);
        cur_cmd->m_sidemove = 0.0f;
    }
    else {
        Reset();
    }
}

void CAutopeek::Run(CUserCmd* cur_cmd)
{
    if (!(g_EnginePrediction->GetUnpredictedData()->m_nFlags & FL_ONGROUND))
        return;

    if (key_binds::get().get_key_bind_state(18)) {
        if (g_ctx.globals.start_position == Vector(0, 0, 0))
            g_ctx.globals.start_position = g_ctx.local()->GetRenderOrigin();

        auto revolver_shoot = g_ctx.globals.weapon->m_iItemDefinitionIndex() == WEAPON_REVOLVER && !g_ctx.globals.revolver_working && (cur_cmd->m_buttons & IN_ATTACK || cur_cmd->m_buttons & IN_ATTACK2);

        if (cur_cmd->m_buttons & IN_ATTACK && g_ctx.globals.weapon->m_iItemDefinitionIndex() != WEAPON_REVOLVER || revolver_shoot)
            has_shot = true;
        if (has_shot)
            GotoStart(cur_cmd);
    }
    else {
        Reset();
    }
}
