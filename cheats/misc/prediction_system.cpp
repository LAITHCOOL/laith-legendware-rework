// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: http://www.viva64.com

#include "prediction_system.h"
#include "..\lagcompensation\local_animations.h"

inline bool is_vector_differences(Vector original, Vector current)
{
	auto delta = original - current;
	for (int i = 0; i < 3; i++)
	{
		if (fabsf(delta[i]) > 0.03125f)
			return false;
	}

	return true;
}

inline bool is_float_differences(float original, float current)
{
	if (fabsf(original - current) > 0.03125f)
		return false;

	return true;
}

void engineprediction::update()
{
	if (m_clientstate()->iDeltaTick > 0)
		m_prediction()->Update(m_clientstate()->iDeltaTick, true, m_clientstate()->nLastCommandAck, m_clientstate()->nLastOutgoingCommand + m_clientstate()->iChokedCommands);
}

void engineprediction::store_netvars(int command_number)
{
	auto data = &netvars_data[command_number % MULTIPLAYER_BACKUP];

	data->tickbase = g_ctx.local()->m_nTickBase();

	data->m_aimPunchAngle = g_ctx.local()->m_aimPunchAngle();
	data->m_aimPunchAngleVel = g_ctx.local()->m_aimPunchAngleVel();
	data->m_viewPunchAngle = g_ctx.local()->m_viewPunchAngle();
	data->m_vecViewOffset = g_ctx.local()->m_vecViewOffset();
	data->m_vecVelocity = g_ctx.local()->m_vecVelocity();
	data->m_vecOrigin = g_ctx.local()->m_vecOrigin();

	data->m_flDuckAmount = g_ctx.local()->m_flDuckAmount();
	data->m_flThirdpersonRecoil = g_ctx.local()->m_flThirdpersonRecoil();
	data->m_flFallVelocity = g_ctx.local()->m_flFallVelocity();
	data->m_flDuckSpeed = g_ctx.local()->m_flDuckSpeed();

	auto weapon = g_ctx.local()->m_hActiveWeapon().Get();
	if (!weapon)
	{
		data->m_flRecoilIndex = 0.0f;
		data->m_fAccuracyPenalty = 0.0f;

		return;
	}

	data->m_flRecoilIndex = weapon->m_flRecoilSeed();
	data->m_fAccuracyPenalty = weapon->m_fAccuracyPenalty();
}

void engineprediction::restore_netvars(int command_number)
{
	auto data = &netvars_data[command_number % MULTIPLAYER_BACKUP];

	if (data->tickbase != g_ctx.local()->m_nTickBase())
		return;

	if (is_vector_differences(data->m_aimPunchAngle, g_ctx.local()->m_aimPunchAngle()))
		g_ctx.local()->m_aimPunchAngle() = data->m_aimPunchAngle;

	if (is_vector_differences(data->m_aimPunchAngleVel, g_ctx.local()->m_aimPunchAngleVel()))
		g_ctx.local()->m_aimPunchAngleVel() = data->m_aimPunchAngleVel;

	if (is_vector_differences(data->m_viewPunchAngle, g_ctx.local()->m_viewPunchAngle()))
		g_ctx.local()->m_viewPunchAngle().x = data->m_viewPunchAngle.x;

	if (is_vector_differences(data->m_vecViewOffset, g_ctx.local()->m_vecViewOffset()))
		g_ctx.local()->m_vecViewOffset() = data->m_vecViewOffset;

	if (is_vector_differences(data->m_vecOrigin, g_ctx.local()->m_vecOrigin()))
		g_ctx.local()->m_vecOrigin() = data->m_vecOrigin;

	if (is_vector_differences(data->m_vecVelocity, g_ctx.local()->m_vecVelocity()))
		g_ctx.local()->m_vecVelocity() = data->m_vecVelocity;

	if (is_float_differences(data->m_flDuckAmount, g_ctx.local()->m_flDuckAmount()))
		g_ctx.local()->m_flDuckAmount() = data->m_flDuckAmount;

	if (is_float_differences(data->m_flThirdpersonRecoil, g_ctx.local()->m_flThirdpersonRecoil()))
		g_ctx.local()->m_flThirdpersonRecoil() = data->m_flThirdpersonRecoil;

	if (is_float_differences(data->m_flFallVelocity, g_ctx.local()->m_flFallVelocity()))
		g_ctx.local()->m_flFallVelocity() = data->m_flFallVelocity;

	if (is_float_differences(data->m_flDuckSpeed, g_ctx.local()->m_flDuckSpeed()))
		g_ctx.local()->m_flDuckSpeed() = data->m_flDuckSpeed;

	if (g_ctx.local()->m_hActiveWeapon().Get())
	{
		if (is_float_differences(data->m_fAccuracyPenalty, g_ctx.local()->m_hActiveWeapon().Get()->m_fAccuracyPenalty()))
			g_ctx.local()->m_hActiveWeapon().Get()->m_fAccuracyPenalty() = data->m_fAccuracyPenalty;

		if (is_float_differences(data->m_flRecoilIndex, g_ctx.local()->m_hActiveWeapon().Get()->m_flRecoilSeed()))
			g_ctx.local()->m_hActiveWeapon().Get()->m_flRecoilSeed() = data->m_flRecoilIndex;
	}

	if (g_ctx.local()->m_vecViewOffset().z > 64.0f)
		g_ctx.local()->m_vecViewOffset().z = 64.0f;
	else if (g_ctx.local()->m_vecViewOffset().z <= 46.05f)
		g_ctx.local()->m_vecViewOffset().z = 46.0f;

	if (g_ctx.local()->m_fFlags() & FL_ONGROUND)
		g_ctx.local()->m_flFallVelocity() = 0.0f;
}


void engineprediction::StartCommand(player_t* player, CUserCmd* cmd)
{
	//memset(&m_stored_variables.m_MoveData, 0, sizeof(CMoveData));


	//uintptr_t post_think_v_physicss = util::FindSignature(crypt_str("client.dll"), crypt_str("55 8B EC 83 E4 F8 81 EC ? ? ? ? 53 8B D9 56 57 83 BB ? ? ? ? ? 0F 84"));
	//uintptr_t simulate_player_simulated_entitiess = util::FindSignature(crypt_str("client.dll"), crypt_str("56 8B F1 57 8B BE ? ? ? ? 83 EF 01 78 74"));

	if (!prediction_player || !prediction_random_seed)
	{
		prediction_random_seed = *reinterpret_cast<std::uintptr_t*>(std::uintptr_t(util::FindSignature(crypt_str("client.dll"), crypt_str("8B 47 40 A3"))) + 4);
		prediction_player = *reinterpret_cast<std::uintptr_t*>(std::uintptr_t(util::FindSignature(crypt_str("client.dll"), crypt_str("0F 5B C0 89 35"))) + 5);
	}

	*reinterpret_cast<int*>(prediction_random_seed) = cmd ? cmd->m_random_seed : -1;
	*reinterpret_cast<int*>(prediction_player) = std::uintptr_t(player);
	g_ctx.local()->m_pCurrentCommand() = cmd;
	g_ctx.local()->m_lastcmd() = *cmd;

}
void engineprediction::start(CUserCmd* cmd)
{



	StartCommand(g_ctx.local(), cmd);

	// backup data before messing with prediction.
	backup_data.flags = g_ctx.local()->m_fFlags();
	backup_data.velocity = g_ctx.local()->m_vecVelocity();

	// backup prediction data.
	prediction_data.old_curtime = m_globals()->m_curtime;
	prediction_data.old_frametime = m_globals()->m_frametime;

	const auto old_tickbase = g_ctx.local()->m_nTickBase();
	prediction_data.old_in_prediction = m_prediction()->InPrediction;
	prediction_data.old_first_prediction = m_prediction()->IsFirstTimePredicted;

	// proper.
	m_globals()->m_curtime = TICKS_TO_TIME(g_ctx.globals.fixed_tickbase);
	m_globals()->m_frametime = m_prediction()->EnginePaused ? 0 : m_globals()->m_intervalpertick;

	m_globals()->m_tickcount = g_ctx.local()->m_nTickBase();

	m_prediction()->IsFirstTimePredicted = false;
	m_prediction()->InPrediction = true;

	if (cmd->m_impulse)
		*reinterpret_cast<uint32_t*>((uintptr_t)g_ctx.local() + 0x320C) = cmd->m_impulse;

	cmd->m_buttons |= *reinterpret_cast<int*>((uintptr_t)g_ctx.local() + 0x3334);
	cmd->m_buttons &= ~(*reinterpret_cast<int*>((uintptr_t)g_ctx.local() + 0x3330));

	// update button state
	const int buttons = cmd->m_buttons;
	int* player_buttons = g_ctx.local()->m_nButtons();
	const int buttons_changed = buttons ^ *player_buttons;

	g_ctx.local()->m_afButtonLast() = *player_buttons;
	*g_ctx.local()->m_nButtons() = buttons;
	g_ctx.local()->m_afButtonPressed() = buttons & buttons_changed;
	g_ctx.local()->m_afButtonReleased() = buttons_changed & ~buttons;

	m_prediction()->CheckMovingGround(g_ctx.local(), m_globals()->m_frametime);
	//m_prediction()->SetLocalViewAngles(cmd->m_viewangles);

	local_animations::get().run(cmd);

	if (g_ctx.local()->physics_run_think(0))
		g_ctx.local()->pre_think();

	if (*g_ctx.local()->m_nNextThinkTick1() > 0 && *g_ctx.local()->m_nNextThinkTick1() <= g_ctx.local()->m_nTickBase())
	{
		*g_ctx.local()->m_nNextThinkTick1() = -1;
		g_ctx.local()->unknown_function();
		g_ctx.local()->think();
	}

	m_movehelper()->set_host(g_ctx.local());

	m_gamemovement()->StartTrackPredictionErrors(g_ctx.local());

	memset(&prediction_data.move_data, 0, sizeof(CMoveData));
	m_prediction()->SetupMove(g_ctx.local(), cmd, m_movehelper(), &prediction_data.move_data);

	m_gamemovement()->ProcessMovement(g_ctx.local(), &prediction_data.move_data);

	m_prediction()->FinishMove(g_ctx.local(), cmd, &prediction_data.move_data);

	m_movehelper()->process_impacts();

	g_ctx.local()->RunPostThink();

	g_ctx.local()->m_nTickBase() = g_ctx.globals.backup_tickbase;
	//g_ctx.local()->m_nTickBase() = old_tickbase;
	m_gamemovement()->FinishTrackPredictionErrors(g_ctx.local());
	m_movehelper()->set_host(nullptr);

	g_ctx.globals.inaccuracy = g_ctx.globals.spread = g_ctx.globals.recoil_seed = FLT_MAX;

	if (g_ctx.globals.weapon)
	{
		g_ctx.globals.weapon->update_accuracy_penality();

		g_ctx.globals.inaccuracy = g_ctx.globals.weapon->get_inaccuracy();
		g_ctx.globals.spread = g_ctx.globals.weapon->get_spread();
		g_ctx.globals.recoil_seed = g_ctx.globals.weapon->m_flRecoilSeed();
	}

	auto viewmodel = g_ctx.local()->m_hViewModel().Get();
	if (viewmodel)
	{
		viewmodel_data.weapon = viewmodel->m_hWeapon().Get();

		viewmodel_data.viewmodel_index = viewmodel->m_nViewModelIndex();
		viewmodel_data.sequence = viewmodel->m_nSequence();
		viewmodel_data.animation_parity = viewmodel->m_nAnimationParity();

		viewmodel_data.cycle = viewmodel->m_flCycle();
		viewmodel_data.animation_time = viewmodel->m_flAnimTime();
	}

	local_animations::get().setup_shoot_position(cmd);
}

void engineprediction::finish()
{
	m_prediction()->IsFirstTimePredicted = prediction_data.old_first_prediction;
	m_prediction()->InPrediction = prediction_data.old_in_prediction;

	m_globals()->m_curtime = prediction_data.old_curtime;
	m_globals()->m_frametime = prediction_data.old_frametime;

	//	CPrediction::FinishCommand
	{
		g_ctx.local()->m_pCurrentCommand() = prediction_data.old_current_command;
		if (prediction_player && prediction_random_seed)
		{
			*reinterpret_cast<int*>(prediction_random_seed) = 0xffffffff;
			*reinterpret_cast<int*>(prediction_player) = 0;
		}
	}

	m_gamemovement()->Reset();
}

void networking::process_interpolation(ClientFrameStage_t stage, bool postframe)
{
	if (stage != FRAME_RENDER_START)
		return;

	if (!g_ctx.local()->is_alive())
		return;

	if (!postframe)
	{
		final_predicted_tick = g_ctx.local()->m_nFinalPredictedTick();
		g_ctx.local()->m_nFinalPredictedTick() = m_globals()->m_tickcount + TIME_TO_TICKS(latency);

		return;
	}

	g_ctx.local()->m_nFinalPredictedTick() = final_predicted_tick;
}

int networking::framerate()
{
	static auto framerate = 0.0f;
	framerate = 0.9f * framerate + 0.1f * m_globals()->m_absoluteframetime;

	if (framerate <= 0.0f)
		framerate = 1.0f;

	return (int)(1.0f / framerate);
}

std::string networking::current_time()
{
	time_t lt;
	struct tm* t_m;

	lt = time(nullptr);
	t_m = localtime(&lt);

	auto time_h = t_m->tm_hour;
	auto time_m = t_m->tm_min;
	auto time_s = t_m->tm_sec;

	std::string time;

	if (time_h < 10)
		time += "0";

	time += std::to_string(time_h) + ":";

	if (time_m < 10)
		time += "0";

	time += std::to_string(time_m) + ":";

	if (time_s < 10)
		time += "0";

	time += std::to_string(time_s);
	return std::move(time);
}

//int networking::ping()
//{
//	if (!g_ctx.available())
//		return 0;
//
//	auto latency = m_engine()->IsPlayingDemo() ? 0.0f : flow_outgoing;
//
//	if (latency)
//		latency -= 0.5f / g_ctx.convars.cl_updaterate->GetFloat();
//
//	return (int)(max(0.0f, latency) * 1000.0f);
//}

float networking::tickrate()
{
	if (!g_ctx.available())
		return 0.0f;

	return (1.0f / m_globals()->m_intervalpertick);
}

//void networking::update_clmove()
//{
//	g_ctx.globals.should_update_netchannel = true;
//
//	if (m_clientstate() && m_clientstate()->pNetChannel)
//		out_sequence = m_clientstate()->pNetChannel->m_nOutSequenceNr;
//
//	if (m_engine()->GetNetChannelInfo())
//	{
//		flow_outgoing = m_engine()->GetNetChannelInfo()->GetLatency(FLOW_OUTGOING);
//		flow_incoming = m_engine()->GetNetChannelInfo()->GetLatency(FLOW_INCOMING);
//
//		latency = flow_outgoing + flow_incoming;
//	}
//
//	tickcount = m_globals()->m_tickcount;
//}







void engineprediction::OnFrameStageNotify(ClientFrameStage_t Stage)
{
	// local must be alive and we also must receive an update
	if (Stage != ClientFrameStage_t::FRAME_NET_UPDATE_END || !g_ctx.local()->is_alive())
		return;

	// define const
	const int nSimulationTick = TIME_TO_TICKS(g_ctx.local()->m_flSimulationTime());
	const int nOldSimulationTick = TIME_TO_TICKS(g_ctx.local()->m_flOldSimulationTime());
	const int nCorrectionTicks = TIME_TO_TICKS(0.03f);
	const int nServerTick = m_clientstate()->m_iServerTick;

	// check time
	if (nSimulationTick <= nOldSimulationTick || abs(nSimulationTick - nServerTick) > nCorrectionTicks)
		return;

	// save last simulation ticks amount
	m_TickBase.m_nSimulationTicks = nSimulationTick - nServerTick;
}
int engineprediction::AdjustPlayerTimeBase(int nSimulationTicks)
{
	// get tickbase
	int nTickBase = g_ctx.local()->m_nTickBase() + 1;

	// define const
	const int nCorrectionTicks = TIME_TO_TICKS(0.03f);
	const int nChokedCmds = m_clientstate()->iChokedCommands;

	// if client gets ahead or behind of this, we'll need to correct.
	const int nTooFastLimit = nTickBase + nCorrectionTicks + nChokedCmds - m_TickBase.m_nSimulationTicks + 1;
	const int nTooSlowLimit = nTickBase - nCorrectionTicks + nChokedCmds - m_TickBase.m_nSimulationTicks + 1;

	// correct tick 
	if (nTickBase + 1 > nTooFastLimit || nTickBase + 1 < nTooSlowLimit)
		nTickBase += nCorrectionTicks + nChokedCmds - m_TickBase.m_nSimulationTicks;

	// save predicted tickbase
	return nTickBase + nSimulationTicks;
}