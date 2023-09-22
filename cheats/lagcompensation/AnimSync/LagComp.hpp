#pragma once
#include "..\..\includes.hpp"
#include "..\..\sdk\structs.hpp"
#include "../animation_system.h"
enum EPlayerActivity
{
	NoActivity = 0,
	Jump = 1,
	Land = 2
};

enum EBoneMatrix
{
	Aimbot,
	Left,
	Center,
	Right,
	Visual
};

enum ESafeSied
{
	CenterS = 0,
	LeftS = -1,
	RightS = 1,
	LowLeftS = -2,
	LowRightS = 2
};

struct CGameGlobals_t
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

struct CPlayersGlobals_t
{
	Vector m_vecVelocity;
	Vector m_vecAbsVelocity;
	Vector m_vecAbsOrigin;
	int m_fFlags;
	int m_iEFlags;
	float m_flDuckAmount, m_flLowerBodyYawTarget, m_flThirdpersonRecoil;


	void AdjustData(player_t* e)
	{
		e->m_vecVelocity() = this->m_vecVelocity;
		e->m_vecAbsVelocity() = this->m_vecAbsVelocity;
		e->m_fFlags() = this->m_fFlags;
		e->m_iEFlags() = this->m_iEFlags;
		e->m_flDuckAmount() = this->m_flDuckAmount;
		e->m_flLowerBodyYawTarget() = this->m_flLowerBodyYawTarget;
		e->m_flThirdpersonRecoil() = this->m_flThirdpersonRecoil;
		e->set_abs_origin(this->m_vecAbsOrigin);
	}
	void CaptureData(player_t* e)
	{
		this->m_vecVelocity = e->m_vecVelocity();
		this->m_vecAbsVelocity = e->m_vecAbsVelocity();
		this->m_fFlags = e->m_fFlags();
		this->m_iEFlags = e->m_iEFlags();
		this->m_flDuckAmount = e->m_flDuckAmount();
		this->m_flLowerBodyYawTarget = e->m_flLowerBodyYawTarget();
		this->m_flThirdpersonRecoil = e->m_flThirdpersonRecoil();
		this->m_vecAbsOrigin = e->get_abs_origin();
	}
};

struct LagRecord_t
{
	float m_flSimulationTime = 0.0f;
	float m_flOldSimulationTime = 0.0f;
	float m_flCollisionChangeTime = 0.0f;
	float m_flCollisionZ = 0.0f;
	float m_flDuckAmount = 0.0f;
	float m_flLowerBodyYaw = 0.0f;
	float m_flThirdPersonRecoil = 0.0f;
	float m_flMaxSpeed = 0.0f;
	float m_flDesyncDelta = 58.0f;
	float m_flAnimationVelocity = 0.0f;
	float m_flEyeYaw = 0.0f;
	player_t* player = nullptr;

	Vector m_angEyeAngles = Vector(0, 0, 0);
	Vector m_angAbsAngles = Vector(0, 0, 0);
	Vector m_vecOrigin = Vector(0, 0, 0);
	Vector m_vecAbsOrigin = Vector(0, 0, 0);
	Vector m_vecVelocity = Vector(0, 0, 0);
	Vector m_vecMins = Vector(0, 0, 0);
	Vector m_vecMaxs = Vector(0, 0, 0);

	int m_nFlags = 0;
	int m_nSimulationTicks = 0;

	int m_nActivityTick = 0;
	float m_flActivityPlayback = 0.0f;
	float m_flDurationInAir = 0.0f;
	EPlayerActivity m_nActivityType = EPlayerActivity::NoActivity;
	EFixedVelocity m_nVelocityMode = EFixedVelocity::Unresolved;

	bool m_bDidShot = false;
	int m_nShotTick = 0;

	bool m_bIsFakePlayer = false;
	bool m_bHasBrokenLC = false;
	bool m_bFirstAfterDormant = false;
	bool m_bIsInvalid = false;
	bool m_bIsResolved = false;
	bool m_bIsWalking = false;
	bool m_bUsedAsPreviousRecord = false;
	bool m_fDidBacktrack = false;
	bool m_bRestoreData = true;

	std::array < AnimationLayer, 13 > m_Layers = { };
	std::array < float, MAXSTUDIOPOSEPARAM > m_PoseParameters = { };
	std::array < std::array < matrix3x4_t, MAXSTUDIOBONES >, 7 > m_Matricies;
};

struct PlayerData_t
{
	float m_flExploitTime = 0.0f;
	bool m_bLeftDormancy = false;
	bool m_bJustSpawned = true;
	LagRecord_t m_LagRecord = LagRecord_t();
	std::array < matrix3x4_t, MAXSTUDIOBONES > m_aCachedMatrix;
	Vector m_vecLastOrigin = Vector(0, 0, 0);
};

class C_LagComp
{
public:
	void RunLagComp(ClientFrameStage_t stage);


	void StoreRecordData(player_t* pPlayer, LagRecord_t* m_LagRecord, LagRecord_t* m_PreviousRecord);
	void HandleTickbaseShift(player_t* pPlayer, LagRecord_t* m_PreviousRecord);
	void CleanPlayer(player_t* pPlayer);
	void StartLagCompensation();
	void FinishLagCompensation();
	void ForcePlayerRecord(player_t* Player, LagRecord_t* m_LagRecord);
	float GetLerpTime();
	int GetRecordPriority(LagRecord_t* m_Record);
	bool GetBacktrackMatrix(player_t* pPlayer, matrix3x4_t* aMatrix);
	bool IsRecordValid(player_t* pPlayer, LagRecord_t* m_LagRecord);
	bool IsTimeValid(float flSimulationTime, int nTickBase);
	void FixPvs(ClientFrameStage_t stage);
	LagRecord_t* SetupData(player_t* pPlayer, LagRecord_t* m_Record, PlayerData_t* m_PlayerData);
	LagRecord_t FindFirstRecord(player_t* pPlayer, std::deque<LagRecord_t> m_LagRecords);
	LagRecord_t FindBestRecord(player_t* pPlayer, std::deque<LagRecord_t> m_LagRecords, int& nPriority, const float& flSimTime);
	virtual std::deque < LagRecord_t >& GetPlayerRecords(player_t* pPlayer)
	{
		return m_LagCompensationRecords[pPlayer->EntIndex() - 1];
	}

	virtual PlayerData_t& GetPlayerData(player_t* pPlayer)
	{
		return m_LagCompensationPlayerData[pPlayer->EntIndex() - 1];
	}
	virtual void ResetData()
	{
		for (int nPlayer = 0; nPlayer < m_LagCompensationRecords.size(); nPlayer++)
		{
			if (m_LagCompensationRecords[nPlayer].empty())
				continue;

			m_LagCompensationRecords[nPlayer].clear();
		}
	}


	float latency = 0;
	int m_nCompensatedServerTick = 0;


	std::array < LagRecord_t, 64 > m_BackupData;
	std::array < std::deque < LagRecord_t >, 64 > m_LagCompensationRecords;
	std::array < PlayerData_t, 64 > m_LagCompensationPlayerData;


private:

	
};
inline C_LagComp* g_LagComp = new C_LagComp();

struct C_PlayerAnimations
{
public:
	template < class T >
	static T interpolate(const T& current, const T& target, const int progress, const int maximum)
	{
		return current + (((target - current) / maximum) * progress);
	}
	void TransformateMatrix(player_t* pPlayer, PlayerData_t* m_PlayerData);
	Vector DeterminePlayerVelocity(player_t* pPlayer, LagRecord_t* m_LagRecord, LagRecord_t* m_PrevRecord, AnimState_s* m_AnimationState);
	void CopyRecordData(player_t* pPlayer, LagRecord_t* m_LagRecord, LagRecord_t* m_PrevRecord, AnimState_s* m_AnimationState);
	int DetermineAnimationCycle(player_t* pPlayer, LagRecord_t* m_LagRecord, LagRecord_t* m_PrevRecord);
	void HandleDormancyLeaving(player_t* pPlayer, LagRecord_t* m_Record, AnimState_s* m_AnimationState);
	void SetupCollision(player_t* pPlayer, LagRecord_t* m_LagRecord);;
	void UpdatePlayerAnimations(player_t* pPlayer, LagRecord_t* m_LagRecord, AnimState_s* m_AnimationState);
	void SimulatePlayerActivity(player_t* pPlayer, LagRecord_t* m_LagRecord, LagRecord_t* m_PrevRecord);
	float ComputeActivityPlayback(player_t* pPlayer, LagRecord_t* m_Record);
	void SetupBones(player_t* pPlayer, int nBoneMask, matrix3x4_t* aMatrix);
	void SetupPlayerMatrix(player_t* pPlayer, LagRecord_t* m_Record, matrix3x4_t* Matrix, int nFlags);
	float BuildFootYaw(player_t* pPlayer, LagRecord_t* m_LagRecord);
	void GenerateSafePoints(player_t* pPlayer, LagRecord_t* m_LagRecord);
	void SimulatePlayerAnimations(player_t* e, LagRecord_t* record, PlayerData_t* player_data);
	bool CopyCachedMatrix(player_t* pPlayer, matrix3x4_t* aMatrix, int nBoneCount);
	void InterpolateMatricies();
	CResolver player_resolver[65];
};

inline C_PlayerAnimations* g_PlayerAnimations = new C_PlayerAnimations();