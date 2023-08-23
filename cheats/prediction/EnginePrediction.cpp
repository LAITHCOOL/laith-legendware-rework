#include "EnginePrediction.h"
#include "../lagcompensation/LocalAnimFix.hpp"

#define VEC_VIEW			Vector( 0, 0, 64 )
#define VEC_HULL_MIN		Vector( -16, -16, 0 )
#define VEC_HULL_MAX		Vector( 16, 16, 72 )
#define VEC_DUCK_HULL_MIN	Vector( -16, -16, 0 )
#define VEC_DUCK_HULL_MAX	Vector( 16, 16, 36 )
#define VEC_DUCK_VIEW		Vector( 0, 0, 46 )

void C_EnginePrediction::UpdatePrediction()
{
	Vector vecAbsOrigin = g_ctx.local()->get_abs_origin();
	if (m_clientstate()->iDeltaTick > 0)
		m_prediction()->Update
		(
			m_clientstate()->iDeltaTick,
			m_clientstate()->iDeltaTick > 0,
			m_clientstate()->nLastCommandAck,
			m_clientstate()->nLastOutgoingCommand + m_clientstate()->iChokedCommands
		);
	g_ctx.local()->set_abs_origin(vecAbsOrigin);

	return g_EnginePrediction->SaveNetvars();
}
void C_EnginePrediction::OnRunCommand(int nCommand)
{
	NetvarData_t* m_Data = &m_NetVars[nCommand % MULTIPLAYER_BACKUP];

	m_Data->m_nTickBase = g_ctx.globals.fixed_tickbase;
	m_Data->m_angAimPunchAngle = g_ctx.local()->m_aimPunchAngle();
	m_Data->m_angViewPunchAngle = g_ctx.local()->m_viewPunchAngle();
	m_Data->m_vecAimPunchAngleVel = g_ctx.local()->m_aimPunchAngleVel();
	m_Data->m_vecViewOffset = g_ctx.local()->m_vecViewOffset();
	m_Data->m_vecNetworkOrigin = g_ctx.local()->m_vecNetworkOrigin();
	m_Data->m_vecAbsVelocity = g_ctx.local()->m_vecAbsVelocity();
	m_Data->m_vecVelocity = g_ctx.local()->m_vecVelocity();
	m_Data->m_vecBaseVelocity = g_ctx.local()->m_vecBaseVelocity();
	m_Data->m_flFallVelocity = g_ctx.local()->m_flFallVelocity();
	m_Data->m_flDuckAmount = g_ctx.local()->m_flDuckAmount();
	m_Data->m_flDuckSpeed = g_ctx.local()->m_flDuckSpeed();
	m_Data->m_flVelocityModifier = g_ctx.local()->m_flVelocityModifier();


	m_Data->m_bIsPredictedData = true;
}
void C_EnginePrediction::OnVelocityModifierProxy(float flValue)
{
	m_flVelocityModifier = flValue;
	m_nVelocityAcknowledged = m_clientstate()->iCommandAck;
}
void C_EnginePrediction::OnFrameStageNotify(ClientFrameStage_t Stage)
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

int C_EnginePrediction::CalculateCorrectionTicks()  {
	static auto sv_clockcorrection_msecs = m_cvar()->FindVar(("sv_clockcorrection_msecs"));
	return m_globals()->m_maxclients <= 1
		? -1 : TIME_TO_TICKS(std::clamp(sv_clockcorrection_msecs->GetFloat() / 1000.f, 0.f, 1.f));
}

int C_EnginePrediction::AdjustPlayerTimeBase(int nSimulationTicks)
{
	// get tickbase
	int nTickBase = g_ctx.local()->m_nTickBase();

	// define const
	const int nCorrectionTicks = CalculateCorrectionTicks();
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
void C_EnginePrediction::OnPostNetworkDataReceived()
{
	// don't need without localplayer
	if (!g_ctx.local() || !g_ctx.local()->is_alive())
		return;

	// specific functions to adjust 
	auto AdjustFieldFloat = [&](float& flNetworked, float flPredicted, float flTolerance, const std::string& szFieldName) -> bool
	{
		if (std::fabsf(flNetworked - flPredicted) > flTolerance)
		{
#ifdef OVERSEE_DEV
			SDK::Interfaces::CVar->ConsolePrintf(_S("[ oversee ] Prediction error detected. %s out of tolerance ( Tolerance: %s )( Diff ( Net - Pred ): %s )\n"), szFieldName.c_str(), std::to_string(flTolerance).c_str(), std::to_string(flNetworked - flPredicted).c_str());
#endif
			return false;
		}

		flNetworked = flPredicted;
		return true;
	};
	auto AdjustFieldVector = [&](Vector& vecNetworked, Vector vecPredicted, float flTolerance, const std::string& szFieldName) -> bool
	{
		for (int nAxis = 0; nAxis < 3; nAxis++)
		{
			if (fabsf(vecNetworked[nAxis] - vecPredicted[nAxis]) <= flTolerance)
				continue;

#ifdef OVERSEE_DEV
			SDK::Interfaces::CVar->ConsolePrintf(_S("[ oversee ] Prediction error detected. %s out of tolerance ( Tolerance: %s )( Diff ( Net - Pred ): %s )\n"), szFieldName.c_str(), std::to_string(flTolerance).c_str(), std::to_string(vecNetworked[nAxis] - vecPredicted[nAxis]).c_str());
#endif
			return false;
		}

		vecNetworked = vecPredicted;
		return true;
	};
	auto AdjustFieldAngle = [&](Vector& vecNetworked, Vector vecPredicted, float flTolerance, const std::string& szFieldName) -> bool
	{
		for (int nAxis = 0; nAxis < 3; nAxis++)
		{
			if (fabsf(vecNetworked[nAxis] - vecPredicted[nAxis]) <= flTolerance)
				continue;

#ifdef OVERSEE_DEV
			SDK::Interfaces::CVar->ConsolePrintf(_S("[ oversee ] Prediction error detected. %s out of tolerance ( Tolerance: %s )( Diff ( Net - Pred ): %s )\n"), szFieldName.c_str(), std::to_string(flTolerance).c_str(), std::to_string(vecNetworked[nAxis] - vecPredicted[nAxis]).c_str());
#endif
			return false;
		}

		vecNetworked = vecPredicted;
		return true;
	};

	/* get predicted data */
	NetvarData_t* aNetVars = &m_NetVars[m_clientstate()->iCommandAck % MULTIPLAYER_BACKUP];
	if (!aNetVars || !aNetVars->m_bIsPredictedData)
		return;

	/* check predicted data for errors */
	bool m_bHadPredictionErrors = false;
	if (!AdjustFieldFloat(g_ctx.local()->m_flVelocityModifier(), aNetVars->m_flVelocityModifier, 0.4f * m_globals()->m_intervalpertick, "m_flVelocityModifier"))
	{
		m_bHadPredictionErrors = true;
	}
	if (!AdjustFieldFloat(g_ctx.local()->m_flFallVelocity(), aNetVars->m_flFallVelocity, 0.05f, "m_flFallVelocity"))
	{
		m_bHadPredictionErrors = true;
	}
	if (!AdjustFieldFloat(g_ctx.local()->m_vecViewOffset().z, aNetVars->m_vecViewOffset.z, 0.25f, "m_vecViewOffset[2]"))
	{
		m_bHadPredictionErrors = true;
	}
	if (!AdjustFieldFloat(g_ctx.local()->m_flDuckAmount(), aNetVars->m_flDuckAmount, 0.05f, "m_flDuckAmount"))
	{
		m_bHadPredictionErrors = true;
	}
	if (!AdjustFieldFloat(g_ctx.local()->m_flDuckSpeed(), aNetVars->m_flDuckSpeed, 0.05f, "m_flDuckSpeed"))
	{
		m_bHadPredictionErrors = true;
	}
	if (!AdjustFieldVector(g_ctx.local()->m_vecNetworkOrigin(), aNetVars->m_vecNetworkOrigin, 0.005f, "m_vecNetworkOrigin"))
	{
		m_bHadPredictionErrors = true;
	}
	if (!AdjustFieldVector(g_ctx.local()->m_vecVelocity(), aNetVars->m_vecVelocity, 0.05f, "m_vecVelocity"))
	{
		m_bHadPredictionErrors = true;
	}
	if (!AdjustFieldVector(g_ctx.local()->m_vecBaseVelocity(), aNetVars->m_vecBaseVelocity, 0.05f, "m_vecBaseVelocity"))
	{
		m_bHadPredictionErrors = true;
	}
	if (!AdjustFieldVector(g_ctx.local()->m_aimPunchAngleVel(), aNetVars->m_vecAimPunchAngleVel, 0.05f, "m_aimPunchAngleVel"))
	{
		m_bHadPredictionErrors = true;
	}
	if (!AdjustFieldAngle(g_ctx.local()->m_viewPunchAngle(), aNetVars->m_angViewPunchAngle, 0.05f, "m_viewPunchAngle"))
	{
		m_bHadPredictionErrors = true;
	}
	if (!AdjustFieldAngle(g_ctx.local()->m_aimPunchAngle(), aNetVars->m_angAimPunchAngle, 0.05f, "m_aimPunchAngle"))
	{
		m_bHadPredictionErrors = true;
	}

	/* repredict */
	if (m_bHadPredictionErrors)
		g_EnginePrediction->ForceEngineRepredict();

	// data is used
	aNetVars->m_bIsPredictedData = false;
}
void C_EnginePrediction::OnPlayerMove()
{
	if (!g_ctx.local() || !g_ctx.local()->is_alive() || m_gamemovement()->GetMovingPlayer() != g_ctx.local())
		return;

	if (!(g_ctx.local()->m_fFlags() & FL_DUCKING) && !g_ctx.local()->m_bDucking() && !g_ctx.local()->m_bDucked())
		g_ctx.local()->m_vecViewOffset() = VEC_VIEW;
	else if (g_ctx.local()->m_bDuckUntilOnGround())
	{
		Vector hullSizeNormal = VEC_HULL_MAX - VEC_HULL_MIN;
		Vector hullSizeCrouch = VEC_DUCK_HULL_MAX - VEC_DUCK_HULL_MIN;
		Vector lowerClearance = hullSizeNormal - hullSizeCrouch;
		Vector duckEyeHeight = m_gamemovement()->GetPlayerViewOffset(false) - lowerClearance;

		g_ctx.local()->m_vecViewOffset() = duckEyeHeight;
	}
	else if (g_ctx.local()->m_bDucked() && !g_ctx.local()->m_bDucking())
		g_ctx.local()->m_vecViewOffset() = VEC_DUCK_VIEW;
}
//bool C_EnginePrediction::CanAttack(bool bRevolverCheck, int nShiftAmount)
//{
//	weapon_t* pCombatWeapon = g_ctx.local()->m_hActiveWeapon().Get();
//	if (!pCombatWeapon || (pCombatWeapon->m_iItemDefinitionIndex() != WEAPON_TASER && !pCombatWeapon->IsGun()))
//		return true;
//
//	float flPlayerTime = TICKS_TO_TIME(g_ctx.local()->m_nTickBase() - nShiftAmount);
//	if (pCombatWeapon->m_iClip1() <= 0 || g_ctx.local()->m_flNextAttack() >= flPlayerTime)
//		return false;
//
//	bool bIsDefusing = g_ctx.local()->m_bIsDefusing();
//	if (bIsDefusing)
//		return false;
//
//	bool bWaitForNoAttack = g_ctx.local()->m_bWaitForNoAttack();
//	if (bWaitForNoAttack)
//		return false;
//
//	int nPlayerState = g_ctx.local()->m_nPlayerState();
//	if (nPlayerState)
//		return false;
//
//	if (g_ctx.local()->IsFrozen())
//		return false;
//
//	bool bIsRevolver = pCombatWeapon->m_nItemID() == WEAPON_REVOLVER;
//	if (bIsRevolver)
//	{
//		if (bRevolverCheck)
//			if (pCombatWeapon->m_flPostponeFireReadyTime() >= flPlayerTime || pCombatWeapon->m_Activity() != 208)
//				return false;
//	}
//
//	return pCombatWeapon->m_flNextPrimaryAttack() <= flPlayerTime;
//}
void C_EnginePrediction::StartPrediction()
{
	// save globals data
	m_flCurTime = m_globals()->m_curtime;
	m_flFrameTime = m_globals()->m_frametime;

	// save prediction data
	m_bInPrediction = m_prediction()->InPrediction;
	m_bIsFirstTimePredicted = m_prediction()->IsFirstTimePredicted;

	// save local data
	g_LocalAnimations->BeforePrediction();

	// run prediction
	g_EnginePrediction->RunPrediction();

	// run edge jump
	//g_Movement->EdgeJump();

	// update the weapon
	weapon_t* pCombatWeapon = g_ctx.local()->m_hActiveWeapon();
	if (pCombatWeapon)
	{
		pCombatWeapon->update_accuracy_penality();

		// get inaccuracy and spread
		m_flSpread = pCombatWeapon->get_spread();
		m_flInaccuracy = pCombatWeapon->get_inaccuracy();
	}
}
void C_EnginePrediction::RunPrediction()
{
	Vector vecAbsOrigin = g_ctx.local()->get_abs_origin();

	// push prediction data
	m_prediction()->InPrediction = true;
	m_prediction()->IsFirstTimePredicted = false;

	// save old buttons
	int nButtons = g_ctx.get_command()->m_buttons;

	// update button state
	g_EnginePrediction->UpdateButtonState();

	// push globals data
	m_globals()->m_curtime = TICKS_TO_TIME(g_ctx.globals.fixed_tickbase);
	m_globals()->m_frametime = m_globals()->m_intervalpertick;

	// setup velocity
	g_ctx.local()->m_vecAbsVelocity() = g_ctx.local()->m_vecVelocity();

	// start command
	g_EnginePrediction->StartCommand();

	// set host
	m_movehelper()->set_host(g_ctx.local());

	m_prediction()->SetupMove(g_ctx.local(), g_ctx.get_command(), m_movehelper(), &m_MoveData);
	m_gamemovement()->ProcessMovement(g_ctx.local(), &m_MoveData);

	//local_animations::get().run(g_ctx.get_command());

	m_prediction()->FinishMove(g_ctx.local(), g_ctx.get_command(), &m_MoveData);

	// finish command
	g_EnginePrediction->FinishCommand();

	// save last predicted cmd
	m_LastPredictedCmd = g_ctx.get_command();

	// restore old buttons
	g_ctx.get_command()->m_buttons = nButtons;

	/*set predicted tickbase*/
	g_ctx.local()->m_nTickBase() = g_ctx.globals.fixed_tickbase;

	// reset host
	m_movehelper()->set_host(nullptr);

	g_ctx.local()->set_abs_origin(vecAbsOrigin);

	// finish gamemovemesnt
	return m_gamemovement()->Reset();
}
void C_EnginePrediction::FinishPrediction()
{
	// reset globals data
	m_globals()->m_curtime = m_flCurTime;
	m_globals()->m_frametime = m_flFrameTime;

	// reset prediction data
	m_prediction()->InPrediction = m_bInPrediction;
	m_prediction()->IsFirstTimePredicted = m_bIsFirstTimePredicted;
}
void C_EnginePrediction::StartCommand()
{
	// setup player data
	g_ctx.local()->GetUserCmd() = g_ctx.get_command();
	if (g_ctx.local())
		g_ctx.local()->SetAsPredictionPlayer();
	static auto seed = (*(*(int**)((void*)((DWORD)(util::FindSignature(("client.dll"), ("8B 0D ? ? ? ? BA ? ? ? ? E8 ? ? ? ? 83 C4 04"))) + 0x2))));
	// set prediction seed
	seed = g_ctx.get_command()->m_random_seed;

	// start track prediction errors
	return m_gamemovement()->StartTrackPredictionErrors(g_ctx.local());
}
void C_EnginePrediction::FinishCommand()
{
	// reset player data
	g_ctx.local()->GetUserCmd() = NULL;
	if (g_ctx.local())
		g_ctx.local()->UnsetAsPredictionPlayer();

	// set prediction seed
	static auto seed = (*(*(int**)((void*)((DWORD)(util::FindSignature(("client.dll"), ("8B 0D ? ? ? ? BA ? ? ? ? E8 ? ? ? ? 83 C4 04"))) + 0x2))));
	seed = -1;

	// start track prediction errors
	return m_gamemovement()->FinishTrackPredictionErrors(g_ctx.local());
}
void C_EnginePrediction::UpdateButtonState()
{
	// force buttons
	g_ctx.get_command()->m_buttons |= *(int*)((DWORD)(g_ctx.local()) + 0x3344);
	g_ctx.get_command()->m_buttons &= ~*(int*)((DWORD)(g_ctx.local()) + 0x3340);

	// set impulse
	*(int*)((DWORD)(g_ctx.local()) + 0x320C) = g_ctx.get_command()->m_buttons;

	// save old buttons
	int nOldButtons = g_ctx.get_command()->m_buttons;
	int nButtonDifference = nOldButtons ^ *(int*)((DWORD)(g_ctx.local()) + 0x3208);


	// update button state
	*(int*)((DWORD)(g_ctx.local()) + 0x31FC) = *(int*)((DWORD)(g_ctx.local()) + 0x3208);
	*(int*)((DWORD)(g_ctx.local()) + 0x3208) = nOldButtons;
	*(int*)((DWORD)(g_ctx.local()) + 0x3200) = nOldButtons & nButtonDifference;
	*(int*)((DWORD)(g_ctx.local()) + 0x3204) = nButtonDifference & ~nOldButtons;
}
float C_EnginePrediction::GetSpread()
{
	return m_flSpread;
}
float C_EnginePrediction::GetInaccuracy()
{
	return m_flInaccuracy;
}

void C_EnginePrediction::SaveNetvars()
{
	m_PredictionData.m_nFlags = g_ctx.local()->m_fFlags();
	m_PredictionData.m_angAimPunchAngle = g_ctx.local()->m_aimPunchAngle();
	m_PredictionData.m_angViewPunchAngle = g_ctx.local()->m_viewPunchAngle();
	m_PredictionData.m_vecAimPunchAngleVel = g_ctx.local()->m_aimPunchAngleVel();
	m_PredictionData.m_flDuckAmount = g_ctx.local()->m_flDuckAmount();
	m_PredictionData.m_flDuckSpeed = g_ctx.local()->m_flDuckSpeed();
	m_PredictionData.m_flVelocityModifier = g_ctx.local()->m_flVelocityModifier();
	m_PredictionData.m_flStamina = g_ctx.local()->m_flStamina();
	m_PredictionData.m_vecVelocity = g_ctx.local()->m_vecVelocity();
	m_PredictionData.m_vecAbsVelocity = g_ctx.local()->m_vecAbsVelocity();
	m_PredictionData.m_vecOrigin = g_ctx.local()->m_vecOrigin();
	m_PredictionData.m_vecViewOffset = g_ctx.local()->m_vecViewOffset();

	m_PredictionData.OBBMins = g_ctx.local()->GetCollideable()->OBBMins();
	m_PredictionData.OBBMaxs = g_ctx.local()->GetCollideable()->OBBMaxs();

	weapon_t* pCombatWeapon = g_ctx.local()->m_hActiveWeapon();
	if (pCombatWeapon)
	{
		m_PredictionData.m_flAccuracyPenalty = pCombatWeapon->m_fAccuracyPenalty();
		m_PredictionData.m_flRecoilIndex = pCombatWeapon->m_flRecoilSeed();
	}

	m_PredictionData.m_GroundEntity = g_ctx.local()->m_hGroundEntity();
}
void C_EnginePrediction::RestoreNetvars()
{
	g_ctx.local()->m_fFlags() = m_PredictionData.m_nFlags;
	g_ctx.local()->m_aimPunchAngle() = m_PredictionData.m_angAimPunchAngle;
	g_ctx.local()->m_viewPunchAngle() = m_PredictionData.m_angViewPunchAngle;
	g_ctx.local()->m_aimPunchAngleVel() = m_PredictionData.m_vecAimPunchAngleVel;
	g_ctx.local()->m_flDuckAmount() = m_PredictionData.m_flDuckAmount;
	g_ctx.local()->m_flDuckSpeed() = m_PredictionData.m_flDuckSpeed;
	g_ctx.local()->m_vecVelocity() = m_PredictionData.m_vecVelocity;
	g_ctx.local()->m_vecAbsVelocity() = m_PredictionData.m_vecAbsVelocity;
	g_ctx.local()->m_vecOrigin() = m_PredictionData.m_vecOrigin;
	g_ctx.local()->m_vecViewOffset() = m_PredictionData.m_vecViewOffset;
	g_ctx.local()->m_flVelocityModifier() = m_PredictionData.m_flVelocityModifier;
	g_ctx.local()->m_flStamina() = m_PredictionData.m_flStamina;

	g_ctx.local()->UpdateCollisionBounds();
	g_ctx.local()->SetCollisionBounds(m_PredictionData.OBBMins, m_PredictionData.OBBMaxs);

	weapon_t* pCombatWeapon = g_ctx.local()->m_hActiveWeapon();
	if (pCombatWeapon)
	{
		pCombatWeapon->m_fAccuracyPenalty() = m_PredictionData.m_flAccuracyPenalty;
		pCombatWeapon->m_flRecoilSeed() = m_PredictionData.m_flRecoilIndex;
	}

	g_ctx.local()->m_hGroundEntity() = m_PredictionData.m_GroundEntity;
}
void C_EnginePrediction::RePredict()
{
	this->FinishPrediction();
	this->RestoreNetvars();
	this->RunPrediction();
}
void C_EnginePrediction::ForceEngineRepredict()
{
	m_prediction()->m_previous_startframe = -1;
	m_prediction()->m_nCommandsPredicted = 0;
}
void C_EnginePrediction::ModifyDatamap()
{
	if (!g_ctx.local() || !g_ctx.local()->is_alive())
		return;

	typedescription_t* pVphysicsCollisionState = g_ctx.local()->GetDatamapEntry(g_ctx.local()->GetPredDescMap(), ("m_vphysicsCollisionState"));
	if (!pVphysicsCollisionState || pVphysicsCollisionState->fieldOffset == 0x103EC)
		return;

	pVphysicsCollisionState->fieldType = FIELD_FLOAT;
	pVphysicsCollisionState->fieldTolerance = (1.0f / 2.5f) * m_globals()->m_intervalpertick;
	pVphysicsCollisionState->fieldOffset = 0x103EC;
	pVphysicsCollisionState->fieldSizeInBytes = sizeof(float);
	pVphysicsCollisionState->flatOffset[TD_OFFSET_NORMAL] = 0x103EC;
	g_ctx.local()->GetPredDescMap()->m_pOptimizedDataMap = nullptr;
}