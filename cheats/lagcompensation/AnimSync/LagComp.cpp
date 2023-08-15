#include "LagComp.hpp"

void C_LagComp::RunLagComp(ClientFrameStage_t Stage)
{
	if (Stage != ClientFrameStage_t::FRAME_NET_UPDATE_END)
		return;

	/* Iterate all players */
	for (int nPlayerID = 1; nPlayerID <= m_globals()->m_maxclients; nPlayerID++)
	{
		/* Get player data of this player */
		auto m_PlayerData = &m_LagCompensationPlayerData[nPlayerID - 1];

		/* Get player and check its data */
		player_t* pPlayer = static_cast<player_t*>(m_entitylist()->GetClientEntity(nPlayerID));
		if (!pPlayer || !pPlayer->is_player() || !pPlayer->is_alive() || pPlayer->m_iTeamNum() == g_ctx.local()->m_iTeamNum() || pPlayer == g_ctx.local())
		{
			m_PlayerData->m_bLeftDormancy = true;
			continue;
		}

		/* Get record of this player */
		auto m_LagRecords = &m_LagCompensationRecords[nPlayerID - 1];

		/* Check data changed */
		if (pPlayer->m_flOldSimulationTime() == pPlayer->m_flSimulationTime())
			continue;

		/* Set left dormancy */
		if (pPlayer->IsDormant())
		{
			m_PlayerData->m_bLeftDormancy = true;
			continue;
		}

		/* define previous record of this player */
		LagRecord_t m_PreviousRecord;
		m_PreviousRecord.m_bRestoreData = false;
		if (!m_LagRecords->empty())
		{
			m_PlayerData->m_LagRecord = m_LagRecords->back();
			m_PreviousRecord = m_PlayerData->m_LagRecord;
			m_PreviousRecord.m_bRestoreData = true;
		}

		/* Store data */
		LagRecord_t m_Record;
		StoreRecordData(pPlayer, &m_Record, &m_PreviousRecord);

		/* check for fake update */
		if (m_PreviousRecord.m_bRestoreData)
		{
			if (m_PreviousRecord.m_Layers[11].m_flCycle == m_Record.m_Layers[11].m_flCycle)
			{
				pPlayer->m_flSimulationTime() = m_PreviousRecord.m_flSimulationTime;
				continue;
			}
		}

		/* Check tickbase exploits */
		if (m_PreviousRecord.m_flSimulationTime > m_Record.m_flSimulationTime)
			HandleTickbaseShift(pPlayer, &m_PreviousRecord);

		/* Player who left dormancy right now shouldn't have any records */
		if (m_PlayerData->m_bLeftDormancy)
			CleanPlayer(pPlayer);

		/* Invalidate records for defensive and break lc */
		if (m_Record.m_flSimulationTime <= m_PlayerData->m_flExploitTime)
		{
			/* Don't care at all about this cases if we have anti-exploit */
			m_Record.m_bIsInvalid = true;
			m_Record.m_bHasBrokenLC = true;
		}

		/* handle break lagcompensation by high speed and fakelags */
		if (m_PreviousRecord.m_bRestoreData)
		{
			if ((m_Record.m_vecOrigin - m_PreviousRecord.m_vecOrigin).Length2DSqr() > 4096.0f)
			{
				m_Record.m_bHasBrokenLC = true;
				CleanPlayer(pPlayer);
			}
		}

		/* Determine simulation ticks */
		/* Another code for tickbase shift */
		if (m_Record.m_flSimulationTime < m_PreviousRecord.m_flSimulationTime)
		{
			m_Record.m_bIsInvalid = true;
			m_Record.m_bHasBrokenLC = true;

			if (m_PreviousRecord.m_bRestoreData)
				m_Record.m_nSimulationTicks = TIME_TO_TICKS(m_Record.m_flOldSimulationTime - m_PreviousRecord.m_flOldSimulationTime);
			else
				m_Record.m_nSimulationTicks = m_PreviousRecord.m_nSimulationTicks;
		}
		else
		{
			if (m_PreviousRecord.m_bRestoreData)
				m_Record.m_nSimulationTicks = TIME_TO_TICKS(m_Record.m_flSimulationTime - m_PreviousRecord.m_flSimulationTime);
			else
				m_Record.m_nSimulationTicks = TIME_TO_TICKS(m_Record.m_flSimulationTime - m_Record.m_flOldSimulationTime);
		}


		/* Set this record as first after dormant */
		if (m_PlayerData->m_bLeftDormancy)
			m_Record.m_bFirstAfterDormant = true;

		/* Push record and erase extra records */
		m_LagRecords->emplace_back(m_Record);
		while (m_LagRecords->size() > 32)
			m_LagRecords->pop_front();

		/* Simulate player animations */
		//g_PlayerAnimations->SimulatePlayerAnimations(pPlayer, &m_LagRecords->back(), m_PlayerData);

		C_PlayerAnimations::get().SimulatePlayerAnimations(pPlayer, &m_LagRecords->back(), m_PlayerData);

		/* Dormancy handled */
		m_PlayerData->m_bLeftDormancy = false;
	}
}
void C_LagComp::StoreRecordData(player_t* pPlayer, LagRecord_t* m_LagRecord, LagRecord_t* m_PreviousRecord)
{
	std::memcpy(m_LagRecord->m_Layers.data(), pPlayer->get_animlayers(), sizeof(AnimationLayer) * 13);

	m_LagRecord->m_flSimulationTime = pPlayer->m_flSimulationTime();
	m_LagRecord->m_flOldSimulationTime = pPlayer->m_flOldSimulationTime();
	m_LagRecord->m_nFlags = pPlayer->m_fFlags();
	m_LagRecord->m_angEyeAngles = pPlayer->m_angEyeAngles();
	m_LagRecord->m_flDuckAmount = pPlayer->m_flDuckAmount();
	m_LagRecord->m_flThirdPersonRecoil = pPlayer->m_flThirdpersonRecoil();
	//m_LagRecord->m_flMaxSpeed = g_EnginePrediction->GetMaxSpeed(pPlayer);
	m_LagRecord->m_flLowerBodyYaw = pPlayer->m_flLowerBodyYawTarget();
	m_LagRecord->m_vecOrigin = pPlayer->m_vecOrigin();
	m_LagRecord->m_vecVelocity = pPlayer->m_vecVelocity();
	m_LagRecord->m_bIsWalking = pPlayer->m_bIsWalking();
	m_LagRecord->m_vecAbsOrigin = pPlayer->get_abs_origin();
	m_LagRecord->player = pPlayer;

	weapon_t* pWeapon = pPlayer->m_hActiveWeapon();
	if (pWeapon)
	{
		if (pWeapon->m_fLastShotTime() <= pPlayer->m_flSimulationTime() && pWeapon->m_fLastShotTime() > pPlayer->m_flOldSimulationTime())
		{
			m_LagRecord->m_bDidShot = true;
			m_LagRecord->m_nShotTick = TIME_TO_TICKS(pWeapon->m_fLastShotTime());
		}
	}

	///* Get player info */
	//player_info_t m_PlayerInfo;

	//if (!m_engine()->GetPlayerInfo(pPlayer->EntIndex(), &m_PlayerInfo))
	//	return;

	//if (m_PlayerInfo.fakeplayer)
	//{
	//	m_LagRecord->m_nSimulationTicks = 1;
	//	m_LagRecord->m_bIsFakePlayer = true;
	//}
}

void C_LagComp::HandleTickbaseShift(player_t* pPlayer, LagRecord_t* m_PreviousRecord)
{
	float flOldSimulationTime = pPlayer->m_flOldSimulationTime();
	if (m_PreviousRecord)
		flOldSimulationTime = m_PreviousRecord->m_flSimulationTime;

	m_LagCompensationPlayerData[pPlayer->EntIndex() - 1].m_flExploitTime = pPlayer->m_flSimulationTime();
	CleanPlayer(pPlayer);
}
void C_LagComp::CleanPlayer(player_t* pPlayer)
{
	if (!pPlayer)
		return;

	auto m_LagRecords = &m_LagCompensationRecords[pPlayer->EntIndex() - 1];
	if (!m_LagRecords || m_LagRecords->empty())
		return;

	m_LagRecords->clear();
}

void C_LagComp::StartLagCompensation()
{
	for (int nPlayerID = 1; nPlayerID <= m_globals()->m_maxclients; nPlayerID++)
	{
		player_t* pPlayer = static_cast<player_t*>(m_entitylist()->GetClientEntity(nPlayerID));
		if (!pPlayer || !pPlayer->is_player() || !pPlayer->is_alive() || pPlayer->m_iTeamNum() == g_ctx.local()->m_iTeamNum() || pPlayer->IsDormant())
			continue;

		LagRecord_t* Data = &m_BackupData[nPlayerID - 1];
		if (!Data)
			continue;

		Data->m_angAbsAngles = pPlayer->get_abs_angles();
		Data->m_vecAbsOrigin = pPlayer->get_abs_origin();
		Data->m_angEyeAngles = pPlayer->m_angEyeAngles();
		Data->m_vecOrigin = pPlayer->m_vecOrigin();
		Data->m_flSimulationTime = pPlayer->m_flSimulationTime();
		Data->m_nFlags = pPlayer->m_fFlags();
		Data->m_vecMins = pPlayer->GetCollideable()->OBBMins();
		Data->m_vecMaxs = pPlayer->GetCollideable()->OBBMaxs();
		Data->m_flCollisionChangeTime = pPlayer->m_flCollisionChangeTime();
		Data->m_flCollisionZ = pPlayer->m_flCollisionChangeOrigin();
		Data->m_bRestoreData = true;
		Data->player = pPlayer;

		std::memcpy(Data->m_Layers.data(), pPlayer->get_animlayers(), sizeof(AnimationLayer) * 13);
		std::memcpy(Data->m_PoseParameters.data(), pPlayer->m_flPoseParameter().data(), sizeof(float) * MAXSTUDIOPOSEPARAM);
		std::memcpy(Data->m_Matricies[0].data(), pPlayer->m_CachedBoneData().Base(), sizeof(matrix3x4_t) * pPlayer->m_CachedBoneData().Count());
	}
}

void C_LagComp::FinishLagCompensation()
{
	for (int nPlayerID = 1; nPlayerID <= m_globals()->m_maxclients; nPlayerID++)
	{
		player_t* pPlayer = static_cast<player_t*>(m_entitylist()->GetClientEntity(nPlayerID));
		if (!pPlayer || !pPlayer->is_player() || !pPlayer->is_alive() || pPlayer->m_iTeamNum() == g_ctx.local()->m_iTeamNum() || pPlayer->IsDormant())
			continue;

		LagRecord_t* Data = &m_BackupData[nPlayerID - 1];
		if (!Data || !Data->m_bRestoreData)
			continue;

		std::memcpy(pPlayer->get_animlayers(), Data->m_Layers.data(), sizeof(AnimationLayer) * 13);
		std::memcpy(pPlayer->m_flPoseParameter().data(), Data->m_PoseParameters.data(), sizeof(float) * MAXSTUDIOPOSEPARAM);
		std::memcpy(pPlayer->m_CachedBoneData().Base(), Data->m_Matricies[0].data(), sizeof(matrix3x4_t) * pPlayer->m_CachedBoneData().Count());

		pPlayer->GetCollideable()->OBBMins() = Data->m_vecMins;
		pPlayer->GetCollideable()->OBBMaxs() = Data->m_vecMaxs;
		pPlayer->m_angEyeAngles() = Data->m_angEyeAngles;
		pPlayer->set_abs_angles(Data->m_angAbsAngles);
		pPlayer->set_abs_origin(Data->m_vecAbsOrigin);
		pPlayer->m_vecOrigin() = Data->m_vecOrigin;
		pPlayer->m_flSimulationTime() = Data->m_flSimulationTime;
		pPlayer->m_fFlags() = Data->m_nFlags;
		pPlayer->m_flCollisionChangeTime() = Data->m_flCollisionChangeTime;
		pPlayer->m_flCollisionChangeOrigin() = Data->m_flCollisionZ;
		pPlayer = Data->player;

		Data->m_bRestoreData = false;
	}
}

void C_LagComp::ForcePlayerRecord(player_t* Player, LagRecord_t* m_LagRecord)
{
	Player->m_fFlags() = m_LagRecord->m_nFlags;
	Player->m_flSimulationTime() = m_LagRecord->m_flSimulationTime;
	Player->m_vecOrigin() = m_LagRecord->m_vecOrigin;
	Player->m_angEyeAngles() = m_LagRecord->m_angEyeAngles;
	Player->set_abs_origin(m_LagRecord->m_vecOrigin);
	Player->set_abs_angles(m_LagRecord->m_angAbsAngles);
	Player = m_LagRecord->player;
	std::memcpy(Player->get_animlayers(), m_LagRecord->m_Layers.data(), sizeof(AnimationLayer) * 13);
	std::memcpy(Player->m_flPoseParameter().data(), m_LagRecord->m_PoseParameters.data(), sizeof(float) * MAXSTUDIOPOSEPARAM);

	/* set time */
	float flCurTime = m_globals()->m_curtime;
	m_globals()->m_curtime = TIME_TO_TICKS(g_ctx.globals.fixed_tickbase);
	Player->SetCollisionBounds(m_LagRecord->m_vecMins, m_LagRecord->m_vecMaxs);
	m_globals()->m_curtime = flCurTime;

	std::memcpy(Player->m_CachedBoneData().Base(), m_LagRecord->m_Matricies[EBoneMatrix::Aimbot].data(), sizeof(matrix3x4_t) * Player->m_CachedBoneData().Count());
	return Player->invalidate_bone_cache();
}

float C_LagComp::GetLerpTime()
{
	float updaterate = m_cvar()->FindVar("cl_updaterate")->GetFloat();
	auto minupdate = m_cvar()->FindVar("sv_minupdaterate");
	auto maxupdate = m_cvar()->FindVar("sv_maxupdaterate");

	if (minupdate && maxupdate)
		updaterate = maxupdate->GetFloat();

	float ratio = m_cvar()->FindVar("cl_interp_ratio")->GetFloat();

	if (ratio == 0)
		ratio = 1.0f;

	float lerp = m_cvar()->FindVar("cl_interp")->GetFloat();
	auto cmin = m_cvar()->FindVar("sv_client_min_interp_ratio");
	auto cmax = m_cvar()->FindVar("sv_client_max_interp_ratio");

	if (cmin && cmax && cmin->GetFloat() != 1)
		ratio = math::clamp(ratio, cmin->GetFloat(), cmax->GetFloat());

	return max(lerp, ratio / updaterate);
}


int C_LagComp::GetRecordPriority(LagRecord_t* m_Record)
{
	int nTotalPoints = 0;
	if (m_Record->m_bDidShot)
		nTotalPoints += 2;

	if (m_Record->m_bIsResolved)
		nTotalPoints += 2;

	if (m_Record->m_flAnimationVelocity > 0.0f)
		nTotalPoints++;

	if (!(m_Record->m_nFlags & FL_ONGROUND))
		nTotalPoints++;

	if (m_Record->m_flDesyncDelta <= 30.0f)
		nTotalPoints++;

	if (m_Record->m_nSimulationTicks <= 1)
		nTotalPoints++;

	return nTotalPoints;
}

void C_LagComp::GetLatency()
{
	/* set networking data */
	int m_nTickRate = (int)(1.0f / m_globals()->m_intervalpertick);
	int m_nMaximumChoke = 14;
	int m_bSkipDatagram = true;


	auto net_channel_info = m_engine()->GetNetChannelInfo();
	/* calc latency */
	INetChannel* m_NetChannel = m_clientstate()->pNetChannel;

	if (net_channel_info != nullptr && m_NetChannel != nullptr)
	{
		auto outgoing = net_channel_info->GetLatency(FLOW_OUTGOING);
		auto incoming = net_channel_info->GetLatency(FLOW_INCOMING);
		/* set latency */
		latency = outgoing + incoming;
		/* set sequence */
		int m_nSequence = m_NetChannel->m_nOutSequenceNr;
	}

	/* set servertick */
	int m_nServerTick = m_globals()->m_tickcount + TIME_TO_TICKS(latency);
	m_nCompensatedServerTick = m_nServerTick;
}

bool C_LagComp::GetBacktrackMatrix(player_t* pPlayer, matrix3x4_t* aMatrix)
{
	if (g_cfg.ragebot.anti_exploit)
		return false;

	auto aLagRecords = m_LagCompensationRecords[pPlayer->EntIndex() - 1];
	if (aLagRecords.size() < 2)
		return false;

	float lerp = m_cvar()->FindVar("cl_interp")->GetFloat();
	float ratio = m_cvar()->FindVar("cl_interp_ratio")->GetFloat();
	float updaterate = m_cvar()->FindVar("cl_updaterate")->GetFloat();

	float flCorrectTime = latency + fmax(lerp, ratio / updaterate);
	for (auto m_Record = aLagRecords.rend() - 1; m_Record > aLagRecords.rbegin(); m_Record--)
	{
		if (!IsRecordValid(pPlayer, m_Record.operator->()))
			continue;

		if (m_Record->m_vecAbsOrigin.DistTo(pPlayer->get_abs_origin()) < 1.f)
			return false;

		float flTimeDelta = (m_Record - 1)->m_flSimulationTime - m_Record->m_flSimulationTime;
		float flDeadTime = (m_Record->m_flSimulationTime + flCorrectTime + flTimeDelta) - TICKS_TO_TIME(g_ctx.globals.fixed_tickbase);

		std::array < matrix3x4_t, MAXSTUDIOBONES > m_BoneArray;
		std::memcpy(m_BoneArray.data(), m_Record->m_Matricies[EBoneMatrix::Visual].data(), sizeof(matrix3x4_t) * MAXSTUDIOBONES);

		Vector vecInterpolatedOrigin = math::interpolate((m_Record - 1)->m_vecAbsOrigin, m_Record->m_vecAbsOrigin, std::clamp(flDeadTime * (1.0f / flTimeDelta), 0.f, 1.f));
		for (int nBone; nBone < MAXSTUDIOBONES; nBone++)
			m_BoneArray[nBone].SetOrigin(m_Record->m_Matricies[EBoneMatrix::Visual][nBone].GetOrigin() - m_Record->m_vecAbsOrigin + vecInterpolatedOrigin);

		std::memcpy(aMatrix, m_BoneArray.data(), sizeof(matrix3x4_t) * MAXSTUDIOBONES);
		return true;
	}

	return false;
}
bool C_LagComp::IsRecordValid(player_t* pPlayer, LagRecord_t* m_LagRecord)
{
	if (!m_LagRecord || ((m_LagRecord->m_bIsInvalid || m_LagRecord->m_bHasBrokenLC) && !g_cfg.ragebot.anti_exploit))
		return false;

	return this->IsTimeValid(m_LagRecord->m_flSimulationTime, g_ctx.globals.fixed_tickbase);
}
bool C_LagComp::IsTimeValid(float flSimulationTime, int nTickBase)
{
	const float flLerpTime = GetLerpTime();

	float flDeltaTime = fmin(latency + flLerpTime, 0.2f) - TICKS_TO_TIME(nTickBase - TIME_TO_TICKS(flSimulationTime));
	if (fabs(flDeltaTime) > 0.2f)
		return false;

	int nDeadTime = (int)((float)(TICKS_TO_TIME(m_nCompensatedServerTick)) - 0.2f);
	if (TIME_TO_TICKS(flSimulationTime + flLerpTime) < nDeadTime)
		return false;

	return true;
}



LagRecord_t* C_LagComp::SetupData(player_t* pPlayer, LagRecord_t* m_Record, PlayerData_t* m_PlayerData)
{
	/* No previous record on spawn or left dormancy */
	if (m_Record->m_bFirstAfterDormant)
		return nullptr;

	/* Get player's previous simulated record */
	LagRecord_t m_PrevRecord = m_PlayerData->m_LagRecord;
	if (m_Record->m_bIsFakePlayer)
		return &m_PrevRecord;

	/* Determine simulation ticks with anim cycle */
	m_Record->m_nSimulationTicks = C_PlayerAnimations::get().DetermineAnimationCycle(pPlayer, m_Record, &m_PrevRecord);
	if (TIME_TO_TICKS(m_Record->m_flSimulationTime - m_PrevRecord.m_flSimulationTime) > 17)
		return nullptr;

	/* return previous record */
	return &m_PrevRecord;
}
LagRecord_t C_LagComp::FindFirstRecord(player_t* pPlayer, std::deque < LagRecord_t > m_LagRecords)
{
	/* define record and make it invalid */
	LagRecord_t LagRecord;
	LagRecord.m_bIsInvalid = true;

	/* iterate all records */
	for (int nRecord = m_LagRecords.size() - 1; nRecord > -1; nRecord--)
	{
		auto Record = m_LagRecords[nRecord];
		if (IsRecordValid(pPlayer, &Record))
			return Record;
	}

	/* return record */
	return LagRecord;
}

LagRecord_t C_LagComp::FindBestRecord(player_t* pPlayer, std::deque < LagRecord_t > m_LagRecords, int& nPriority, const float& flSimTime)
{
	/* set the latest record as default */
	LagRecord_t Record;
	Record.m_bIsInvalid = true;
	if (g_cfg.ragebot.anti_exploit)
	{
		/* Time breaker exploit fix is coming soon! */
		return Record;
	}

	/* state */
	bool bState = false;

	/* iterate all records */
	for (int nRecord = 0; nRecord < m_LagRecords.size(); nRecord++)
	{
		/* skip exist record */
		if (m_LagRecords[nRecord].m_flSimulationTime == flSimTime)
			continue;

		/* skip invalid records automatically */
		if (!IsRecordValid(pPlayer, &m_LagRecords[nRecord]))
			continue;

		/* check the oldest record */
		if (!bState)
		{
			/* set record */
			Record = m_LagRecords[nRecord];

			/* check record priority */
			if (!Record.m_bIsInvalid)
			{
				/* get priority */
				nPriority = GetRecordPriority(&Record);

				/* if record has 75% of max points then use it immediately */
				if (nPriority >= 6)
					return Record;
			}

			/* set state */
			bState = true;

			/* this record has been checked */
			continue;
		}

		/* check priority */
		int nTempPriority =GetRecordPriority(&m_LagRecords[nRecord]);
		if (nTempPriority <= nPriority)
			continue;

		/* set as the best */
		nPriority = nTempPriority;
		Record = m_LagRecords[nRecord];
	}

	/* return result */
	return Record;
}