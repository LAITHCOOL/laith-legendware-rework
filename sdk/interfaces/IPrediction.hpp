#pragma once

#include "../math/QAngle.hpp"
#include "../misc/CUserCmd.hpp"
#include "IMoveHelper.hpp"

class CMoveData {
public:
	bool    m_bFirstRunOfFunctions : 1;
	bool    m_bGameCodeMovedPlayer : 1;
	int     m_nPlayerHandle;        // edict index on server, client entity handle on client=
	int     m_nImpulseCommand;      // Impulse command issued.
	Vector  m_vecViewAngles;        // Command view angles (local space)
	Vector  m_vecAbsViewAngles;     // Command view angles (world space)
	int     m_nButtons;             // Attack buttons.
	int     m_nOldButtons;          // From host_client->oldbuttons;
	float   m_flForwardMove;
	float   m_flSideMove;
	float   m_flUpMove;
	float   m_flMaxSpeed;
	float   m_flClientMaxSpeed;
	Vector  m_vecVelocity;          // edict::velocity        // Current movement direction.
	Vector  m_vecAngles;            // edict::angles
	Vector  m_vecOldAngles;
	float   m_outStepHeight;        // how much you climbed this move
	Vector  m_outWishVel;           // This is where you tried 
	Vector  m_outJumpVel;           // This is your jump velocity
	Vector  m_vecConstraintCenter;
	float   m_flConstraintRadius;
	float   m_flConstraintWidth;
	float   m_flConstraintSpeedFactor;
	float   m_flUnknown[5];
	Vector  m_vecAbsOrigin;
};

class player_t;

class IGameMovement {
public:
	virtual					  ~IGameMovement(void) { }

	virtual void	          ProcessMovement(player_t* pPlayer, CMoveData* pMove) = 0;
	virtual void	          Reset(void) = 0;
	virtual void	          StartTrackPredictionErrors(player_t* pPlayer) = 0;
	virtual void	          FinishTrackPredictionErrors(player_t* pPlayer) = 0;
	virtual void	          DiffPrint(char const* fmt, ...) = 0;
	virtual Vector const& GetPlayerMins(bool ducked) const = 0;
	virtual Vector const& GetPlayerMaxs(bool ducked) const = 0;
	virtual Vector const& GetPlayerViewOffset(bool ducked) const = 0;
	virtual bool		      IsMovingPlayerStuck(void) const = 0;
	virtual player_t* GetMovingPlayer(void) const = 0;
	virtual void		      UnblockPusher(player_t* pPlayer, player_t* pPusher) = 0;
	virtual void			  SetupMovementBounds(CMoveData* pMove) = 0;

	inline player_t*& m_Player()
	{
		return *(player_t**)((DWORD)(this) + 0x4);
	}
	inline CMoveData*& m_MoveData()
	{
		return *(CMoveData**)((DWORD)(this) + 0x8);
	}
	inline Vector& m_vecForward()
	{
		return *(Vector*)((DWORD)(this) + 0x18);
	}
	inline Vector& m_vecRight()
	{
		return *(Vector*)((DWORD)(this) + 0x24);
	}
	inline Vector& m_vecUp()
	{
		return *(Vector*)((DWORD)(this) + 0x30);
	}
	inline int& m_nTraceCount()
	{
		return *(int*)((DWORD)(this) + 0xE50);
	}
	inline int& m_iSpeedCropped()
	{
		return *(int*)((DWORD)(this) + 0xC3C);
	}
	inline bool& m_bProcessingMovement()
	{
		return *(bool*)((DWORD)(this) + 0xC40);
	}
};

class CGameMovement
	: public IGameMovement {
public:
	virtual ~CGameMovement(void) { }
};

class IPrediction
{
public:
	void Update(int start_frame, bool valid_frame, int inc_ack, int out_cmd)
	{
		typedef void(__thiscall* oUpdate)(void*, int, bool, int, int);
		return call_virtual<oUpdate>(this, 3)(this, start_frame, valid_frame, inc_ack, out_cmd);
	}

	void CheckMovingGround(entity_t* player, double frametime)
	{
		typedef void(__thiscall* oCheckMovingGround)(void*, entity_t*, double);
		return call_virtual<oCheckMovingGround>(this, 18)(this, player, frametime);
	}

	void RunCommand(player_t* player, CUserCmd* ucmd, IMoveHelper* moveHelper)
	{
		typedef void(__thiscall* oRunCommand)(void*, player_t*, CUserCmd*, IMoveHelper*);
		return call_virtual<oRunCommand>(this, 19)(this, player, ucmd, moveHelper);
	}

	void SetupMove(player_t* player, CUserCmd* ucmd, IMoveHelper* moveHelper, void* pMoveData)
	{
		typedef void(__thiscall* oSetupMove)(void*, player_t*, CUserCmd*, IMoveHelper*, void*);
		return call_virtual<oSetupMove>(this, 20)(this, player, ucmd, moveHelper, pMoveData);
	}

	void FinishMove(player_t* player, CUserCmd* ucmd, void* pMoveData)
	{
		typedef void(__thiscall* oFinishMove)(void*, player_t*, CUserCmd*, void*);
		return call_virtual<oFinishMove>(this, 21)(this, player, ucmd, pMoveData);
	}

	void SetLocalViewAngles(Vector& Angles)
	{
		typedef void(__thiscall* Fn)(void*, Vector&);
		return call_virtual<Fn>(this, 13)(this, Angles);
	}

	char pad00[8]; 					// 0x0000
	bool InPrediction;				// 0x0008
	int				m_previous_startframe;			// 0x000C
	char pad01[1];					// 0x0009
	bool EnginePaused;				// 0x000A
	int				m_nCommandsPredicted;				// 0x001C

	char pad02[13];					// 0x000B
	bool IsFirstTimePredicted;		// 0x0018
	void force_repredict()
	{
		//m_bPreviousAckHadErrors = 1;
		m_nCommandsPredicted = 0;
		m_previous_startframe = -1;
	}
};