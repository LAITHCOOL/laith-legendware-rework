#include "Exploits.hpp"
#include "../prediction/EnginePrediction.h"
#include "../prediction/Networking.h"
#define RechargeTime 0.4f
#define MaximumTicksAllowedForProcessing 16 // required 17 for TOO shit servers
#define DoubleTapShift 14

#define MIN_SIMULATION_TICKS 1
#define MAX_SIMULATION_TICKS 17

/*TODO: make sure to remove tickbase_shift from server_tick function*/

void C_Exploits::SetupExploits()
{
	/* Check connection */
	if (!m_clientstate() || m_clientstate()->iSignonState < SIGNONSTATE_FULL)
		return;

	/* Check localplayer */
	if (!g_ctx.local() || !g_ctx.local()->is_alive() || m_Data.m_bRunAlternativeCreateMove)
		return;

	/* determine simulation ticks */
	m_Data.m_nSimulationTicks = max(min((m_clientstate()->iChokedCommands + 1), MAX_SIMULATION_TICKS), MIN_SIMULATION_TICKS);

	/* Reset Shift Data */
	m_Data.m_bInReset = false;
	m_Data.m_bDefensiveStarted = false;
	m_Data.m_bUseCLMove = false;
	m_Data.m_bDueToFakeDuck = false;
	m_Data.m_nPerspectiveShift = 0;

	/* Get command */
	const int m_CmdNum = m_clientstate()->nLastOutgoingCommand + m_Data.m_nSimulationTicks;

	/* FD fix */
	if (g_ctx.globals.fakeducking)
	{
		/* set max choke */
		//g_Networking->SetMaxChoke(14);

		/* reset shift */
		m_Data.m_nPerspectiveShift = 0;

		/* set fire states */
		g_Globals->m_Packet.m_bCanFire = g_EnginePrediction->CanAttack(false);
		g_Globals->m_Packet.m_bCanFireRev = g_EnginePrediction->CanAttack(true);

		/* do not setup exploits */
		return;
	}

	/* select exploit */
	int nOldExploit = m_Data.m_nActiveExploit;
	if (g_cfg.ragebot.double_tap && g_cfg.ragebot.double_tap_key.key)
		m_Data.m_nActiveExploit = Exploit::DoubleTap;
	else if (g_cfg.antiaim.hide_shots && g_cfg.antiaim.hide_shots_key.key)
		m_Data.m_nActiveExploit = Exploit::HideShots;
	else
		m_Data.m_nActiveExploit = Exploit::NoExploit;

	/* Setup shift amount and recharge if we have to */
	bool bRecharge = false;
	if (m_Data.m_nActiveExploit)
	{
		if (nOldExploit != m_Data.m_nActiveExploit)
			bRecharge = true;
		else
		{
			const int nRechargeTime = TIME_TO_TICKS(RechargeTime);
			int nServerTick = g_Networking->server_tick();
			int nTimeDelta = abs(nServerTick - m_Data.m_nShiftTick);

			// set max choke
			g_Networking->SetMaxChoke(1);

			/* handle recharge and post-shift timings */
			if (m_Data.m_nShiftTick)
			{
				if (nTimeDelta == nRechargeTime)
					bRecharge = true;
				else if (nTimeDelta < nRechargeTime)
					g_Networking->SetMaxChoke(14);
			}

			// calculate next shift amount
			switch (m_Data.m_nActiveExploit)
			{
			case Exploit::DoubleTap:
			{
				m_Data.m_nPerspectiveShift = DoubleTapShift;
			}
			break;

			case Exploit::HideShots:
			{
				m_Data.m_nPerspectiveShift = 9;
			}
			break;

			default: break;
			}
		}
	}
	else
	{
		if (nOldExploit != m_Data.m_nActiveExploit)
		{
			/* calc perspective shift this tick */
			m_Data.m_nPerspectiveShift = m_Data.m_nTicksAllowed - m_Data.m_nSimulationTicks - 1;

			/* setup shift settings */
			m_Data.m_bUseCLMove = true;
			m_Data.m_bInReset = true;
			m_Data.m_nShift = m_Data.m_nPerspectiveShift;

			/* predict tickbase this cmd */
			if (nOldExploit != Exploit::DoubleTap)
				g_EnginePrediction->SetTickbase(m_CmdNum, g_EnginePrediction->AdjustPlayerTimeBase(-m_Data.m_nPerspectiveShift));
		}
	}

	/* force recharge */
	if (bRecharge)
	{
		/* run full recharge */
		m_Data.m_bReCharging = true;

		/* ticks will be in recharge */
		int nTicksInRecharge = MaximumTicksAllowedForProcessing - m_Data.m_nTicksAllowed;

		/* predict tickbase for next cmd */
		m_Data.m_nFirstShift = nTicksInRecharge;
		m_Data.m_bFirstTick = true;

		/* let next tick be networked */
		g_ctx.globals.force_send_packet = true;
		g_Globals->m_Packet.m_bIsValidPacket = false;
	}

	/* if we are resetting or begin charge then network this cmd */
	if (m_Data.m_bReCharging || m_Data.m_bInReset)
		g_Globals->m_Packet.m_bIsValidPacket = false;

	/* if we doesn't have to shift this tick, then fix aimbot */
	if (m_Data.m_bReCharging)
		m_Data.m_nPerspectiveShift = 0;

	/* set ragebot doubletap state */
	if (m_Data.m_nActiveExploit == Exploit::DoubleTap)
	{
		/* packet must be valid */
		if (g_Globals->m_Packet.m_bIsValidPacket && !m_Data.m_nShiftTick)
		{
			/* we must be able to shift */
			if (m_Data.m_nTicksAllowed >= m_Data.m_nPerspectiveShift)
				g_Globals->m_Packet.m_bDoubleTap = true;
		}
	}

	/* get weapon */
	weapon_t* pWeapon = g_ctx.local()->m_hActiveWeapon();
	if (!pWeapon)
	{
		/* cannot fire without weapon tbh*/
		g_Globals->m_Packet.m_bCanFire = false;
		g_Globals->m_Packet.m_bCanFireRev = false;

		/* we cannot also shift */
		m_Data.m_nPerspectiveShift = 0;

		/* skip shift calculation */
		return;
	}

	/* Disable exploits for some weapons and if we recharging */
	if (m_Data.m_nShiftTick || (pWeapon->m_iItemDefinitionIndex() == WEAPON_REVOLVER && m_Data.m_nActiveExploit == Exploit::HideShots))
		m_Data.m_nPerspectiveShift = 0;

	/* fix for hide shots */
	int nShiftAmount = (m_Data.m_nActiveExploit == Exploit::HideShots && pWeapon->m_iItemDefinitionIndex() != WEAPON_REVOLVER && g_Globals->m_Packet.m_bIsValidPacket) ? 9 : 0;

	/* set fire state and make auto fire */
	g_Globals->m_Packet.m_bCanFire = g_EnginePrediction->CanAttack(false, nShiftAmount);
	if (!g_Globals->m_Packet.m_bCanFire && (g_SettingsManager->B[_S("ragebot.enable")] || m_Data.m_nActiveExploit))
		g_ctx.get_command()->m_buttons &= ~IN_ATTACK;

	/* set global can-fire state with revolver checked */
	g_Globals->m_Packet.m_bCanFireRev = g_EnginePrediction->CanAttack(true);
}
int C_Exploits::GetPredictedTickBase()
{
	/* correct tickbase for lagcomp */
	int nShiftAmount = 0;
	if (m_Data.m_nActiveExploit == Exploit::HideShots)
		nShiftAmount = m_Data.m_nPerspectiveShift;

	return g_ctx.local()->m_nTickBase() - nShiftAmount;
}
void C_Exploits::OnTickBaseProxy(int nTickBase)
{
	if (nTickBase == m_Data.m_nTickBase)
		return;

	m_Data.m_nNetworked = nTickBase - m_Data.m_nTickBase;
	m_Data.m_nTickBase = nTickBase;
}
void C_Exploits::OnFakeDuck()
{
	/* we'll need to be sure that user is fakeducking */
	if (!g_ctx.globals.fakeducking || m_Data.m_nShiftTick)
		return;

	/* reset fakeduck */
	g_ctx.send_packet = true;

	/* reset and then fakeduck */
	/* calc perspective shift this tick */
	m_Data.m_nPerspectiveShift = m_Data.m_nTicksAllowed - m_Data.m_nSimulationTicks - 1;

	/* setup shift settings */
	m_Data.m_bUseCLMove = true;
	m_Data.m_bInReset = true;
	m_Data.m_nShift = m_Data.m_nPerspectiveShift;
	m_Data.m_bDueToFakeDuck = true;

	/* predict cmd */
	if (m_Data.m_nActiveExploit == Exploit::HideShots)
		g_EnginePrediction->SetTickbase(g_ctx.get_command()->m_command_number, g_EnginePrediction->AdjustPlayerTimeBase(-m_Data.m_nPerspectiveShift));
}
void C_Exploits::BeforeRageBot()
{
	/* Check weapon */
	weapon_t* pWeapon = g_ctx.local()->m_hActiveWeapon();
	if (!pWeapon)
		return;

	/* Check exploit data */
	if (m_Data.m_nActiveExploit == Exploit::NoExploit || m_Data.m_bReCharging || m_Data.m_bInReset)
		return;

	/* Check packet valid and fakeduck */
	if (!g_Globals->m_Packet.m_bIsValidPacket || g_Globals->m_Packet.m_bFakeDucking || !*g_Globals->m_Packet.m_bSendPacket)
		return;

	/* Get server tick */
	const int m_nServerTick = networking::get().server_tick();

	/* Skip defensive right after recharge */
	if (m_Data.m_bFirstTick)
		return;

	/* Fire able state */
	bool bIsAbleToFire = false;
	if (m_Data.m_nActiveExploit == Exploit::HideShots)
		bIsAbleToFire = g_EnginePrediction->CanAttack(false, m_Data.m_nPerspectiveShift);
	else if (m_Data.m_nActiveExploit == Exploit::DoubleTap)
		bIsAbleToFire = g_EnginePrediction->CanAttack(true);

	/* Check shift */
	if (!m_Data.m_nPerspectiveShift)
	{
		/* Extend recharge time on peek or shoot */
		if (bIsAbleToFire)
		{
			/* Check attack */
			if (g_ctx.get_command()->m_buttons & IN_ATTACK)
				m_Data.m_nShiftTick = m_nServerTick;
		}
		else if (g_ctx.globals.m_Peek.m_bIsPeeking) /* Check peek */
			m_Data.m_nShiftTick = m_nServerTick;

		/* skip defensive and time breaker */
		return;
	}

	/* Run defensive or time breaker ( alpha ) on DT */
	if (m_Data.m_nActiveExploit == Exploit::DoubleTap)
	{
		/* defensive status */
		bool bRunDefensive = false;

		/* do not run defensive in attack */
		if (!(g_ctx.get_command()->m_buttons & IN_ATTACK))
		{
			/* run defensive on peek */
			if (g_ctx.globals.m_Peek.m_bIsPeeking)
				bRunDefensive = true;

			/* do not run defensive with invalid weapons */
			if (pWeapon->is_knife() || pWeapon->is_grenade() || !pWeapon->IsGun() || m_Data.m_nShiftTick || pWeapon->m_iItemDefinitionIndex() == WEAPON_REVOLVER)
				bRunDefensive = false;

			/* Run defensive */
			if (abs(m_nServerTick - m_Data.m_nDefensiveTick) > m_Data.m_nPerspectiveShift)
			{
				/* Should we run defensive? */
				if (bRunDefensive)
				{
					/* Start defensive */
					m_Data.m_bDefensiveStarted = true;
					m_Data.m_nDefensiveTick = m_nServerTick;

					/* Force this packet true */
					g_ctx.send_packet = true;

					/* Invalidate the packet */
					g_Globals->m_Packet.m_bIsValidPacket = false;
				}
			}
		}
	}
}
void C_Exploits::PostRageBot()
{
	/* Check weapon */
	weapon_t* pWeapon = g_ctx.local()->m_hActiveWeapon();
	if (!pWeapon)
		return;

	/* Check exploit data */
	if (m_Data.m_nActiveExploit == Exploit::NoExploit || m_Data.m_bReCharging || m_Data.m_bInReset || !*g_Globals->m_Packet.m_bSendPacket)
		return;

	/* Fix interpolation on recharge */
	if (m_Data.m_bFirstTick)
	{
		/* Shift tickbase with fake commands */
		m_Data.m_nShift = m_Data.m_nFirstShift;
		m_Data.m_bUseCLMove = false;

		/* Reset first tick after recharge state */
		m_Data.m_bFirstTick = false;

		/* Don't run exploits this tick */
		return;
	}

	/* Check packet valid and fakeduck and defensive */
	if (!g_Globals->m_Packet.m_bIsValidPacket || g_Globals->m_Packet.m_bFakeDucking || g_Exploits->IsInDefensive())
		return;

	/* Get server tick */
	const int m_nServerTick = networking::get().server_tick();

	/* Fire able state */
	bool bIsAbleToFire = false;
	if (m_Data.m_nActiveExploit == Exploit::HideShots)
		bIsAbleToFire = g_EnginePrediction->CanAttack(false, m_Data.m_nPerspectiveShift);
	else if (m_Data.m_nActiveExploit == Exploit::DoubleTap)
		bIsAbleToFire = g_EnginePrediction->CanAttack(true);

	/* Extend recharge time on peek or attack */
	if (m_Data.m_nShiftTick)
	{
		if ((bIsAbleToFire && g_ctx.get_command()->m_nButtons & IN_ATTACK && pWeapon->IsGun()) || g_ctx.globals.m_Peek.m_bIsPeeking)
			m_Data.m_nShiftTick = m_nServerTick;

		return;
	}

	/* Double Tap */
	if (m_Data.m_nActiveExploit == Exploit::DoubleTap)
	{
		/* Double Tap state */
		bool m_bDidNotDoubleTap = true;

		/* Check fire ability */
		if (bIsAbleToFire)
		{
			/* Check weapon */
			if (!pWeapon->is_knife() && !pWeapon->is_grenade() && pWeapon->IsGun())
			{
				/* Check fire button */
				if (g_ctx.get_command()->m_nButtons & IN_ATTACK)
				{
					/* Set exploit data */
					m_Data.m_nShift = m_Data.m_nPerspectiveShift;
					m_Data.m_bDueToFakeDuck = false;
					m_Data.m_nShiftTick = 0;
					m_Data.m_bUseCLMove = true;

					/* Set doubletapping */
					m_bDidNotDoubleTap = false;
				}
			}
		}

		/* Run break lagcompensation if we are not double tapping */
		if (m_bDidNotDoubleTap)
		{
			/* Set exploit data */
			m_Data.m_nShift = m_Data.m_nPerspectiveShift;
			m_Data.m_bUseCLMove = false;
			m_Data.m_bDueToFakeDuck = false;
			m_Data.m_nShiftTick = 0;
		}
	}

	/* Hide Shots */
	else if (m_Data.m_nActiveExploit == Exploit::HideShots)
	{
		/* Check fire ability */
		if (bIsAbleToFire)
		{
			/* Check weapon */
			if (!pWeapon->is_knife() && !pWeapon->is_grenade() && pWeapon->IsGun())
			{
				/* Check fire button */
				if (g_ctx.get_command()->m_nButtons & IN_ATTACK)
				{
					/* Set exploit data */
					m_Data.m_nShift = m_Data.m_nPerspectiveShift;
					m_Data.m_bUseCLMove = false;
					m_Data.m_bDueToFakeDuck = false;
					m_Data.m_nShiftTick = 0;

					/* predict tickbase for next 2 cmds */
					const int nFirstTick = g_EnginePrediction->AdjustPlayerTimeBase(-m_Data.m_nShift);
					const int nSecondTick = g_EnginePrediction->AdjustPlayerTimeBase(1);

					/* set tickbase for next 2 cmds */
					g_EnginePrediction->SetTickbase(g_ctx.get_command()->m_nCommand, nFirstTick);
					g_EnginePrediction->SetTickbase(g_ctx.get_command()->m_nCommand + 1, nSecondTick);

					/* skip matrix setup and make next packet networked */
					g_Globals->m_Packet.m_bSkipMatrix = true;
					g_Globals->m_Packet.m_bForcePacket = true;
				}
			}
		}
	}
}
void C_Exploits::WriteUsercmd(bf_write* lpBuffer, CUserCmd* pToCmd, CUserCmd* pFromCmd)
{
	auto WriteUsercmd = SDK::EngineData::m_AddressList[CheatAddressList::WriteUsercmd];
	__asm
	{
		mov     ecx, lpBuffer
		mov     edx, pToCmd
		push    pFromCmd
		call    WriteUsercmd
		add     esp, 4
	}
}

void C_Exploits::OnMovePacket(float flFrameTime)
{
	/* run this shift method only if we have to */
	if (!m_Data.m_bUseCLMove || !m_Data.m_nShift)
		return;

	/* get netchannel */
	INetChannel* m_NetChannel = m_clientstate()->pNetChannel;
	if (!m_NetChannel)
		return;

	/* shift commands */
	m_Data.m_bRunAlternativeCreateMove = true;
	do
	{
		/* build Sequence */
		int nSequence = m_clientstate()->nLastOutgoingCommand + m_clientstate()->iChokedCommands + 1;

		/* collect data and reduce the ping */
		networking::get().start_network();

		/* sendpacket */
		bool bSendPacket = true;
		g_ctx.send_packet = &bSendPacket;

		/* call CreateMove */
		typedef void(__thiscall* CreateMove_t)(void*, int, float, bool);
		((CreateMove_t)(SDK::EngineData::m_AddressList[CheatAddressList::CreateMove]))(SDK::Interfaces::CHLClient, nSequence, SDK::Interfaces::GlobalVars->m_flIntervalPerTick - flFrameTime, true);

		/* simulate sendpacket */
		if (bSendPacket)
		{
			/* send CL_Move */
			((void(__cdecl*)())(SDK::EngineData::m_AddressList[CheatAddressList::CL_SendMove]))();

			/* it's final tick, sendpacket and reset choked */
			m_clientstate()->nLastOutgoingCommand = m_NetChannel->SendDatagram(nullptr);
			m_clientstate()->iChokedCommands = 0;

			/* push cmd */
			g_Networking->PushCommand(m_clientstate()->m_nLastOutgoingCommand);
		}
		else
		{
			/* SetChoked( ) */
			++m_clientstate()->iChokedCommands;
			++m_NetChannel->m_nOutSequenceNr;
			++m_NetChannel->m_nChokedPackets;

			/* fix network */
			g_Networking->FinishNetwork();
		}

		// decrease shift amount and ticks allowed amount
		--m_Data.m_nShift;
	} while (m_Data.m_nShift > 0);

	/* reset shift parameters */
	m_Data.m_bRunAlternativeCreateMove = false;
	m_Data.m_nShiftTick = g_Networking->server_tick();
	m_Data.m_bUseCLMove = false;

	/* Reset */
	m_Data.m_nTicksAllowed = 0;

	/* if we have to reset then reset everything */
	if (m_Data.m_bInReset)
	{
		m_Data.m_nShiftTick = 0;
		m_Data.m_nShift = 0;
		m_Data.m_bReCharging = false;
		m_Data.m_bInReset = false;
	}

	/* reset fd */
	m_Data.m_bDueToFakeDuck = false;
}
bool C_Exploits::IsRecharging()
{
	/* make sure we are connected */
	if (!m_engine()->IsConnected() || !m_engine()->IsInGame())
	{
		g_Exploits->ResetData();
		return false;
	}

	/* make sure we have clientstate and we are FULLY connected */
	if (!m_clientstate() || m_clientstate()->iSignonState < SIGNONSTATE_FULL)
	{
		g_Exploits->ResetData();
		return false;
	}

	/* make sure that local is valid */
	if (!g_ctx.local() || !g_ctx.local()->is_alive())
	{
		g_Exploits->ResetData();
		return false;
	}

	/* skip if fakeduck */
	if (g_ctx.globals.fakeducking)
	{
		m_Data.m_nShiftTick = g_Networking->server_tick();
		return false;
	}

	/* restore interp */
	//g_LocalInterpolation->ContinueInterpolation();

	/* recharge if needed */
	bool bSkipInterpolation = false;
	if (m_Data.m_nActiveExploit > Exploit::NoExploit)
	{
		const int nRechargeTime = TIME_TO_TICKS(RechargeTime);
		if (m_Data.m_bReCharging)
		{
			int nServerTick = g_Networking->server_tick();
			int nTimeDelta = abs(nServerTick - m_Data.m_nShiftTick);

			/* make sure that the time from last shift is out of bounds and we have to run recharge */
			if (nTimeDelta > nRechargeTime)
			{
				/* recharge until we reach MaximumTicksAllowedForProcessing */
				if (++m_Data.m_nTicksAllowed < MaximumTicksAllowedForProcessing)
				{
					//g_LocalInterpolation->SkipInterpolation();
					return true;
				}

				/* reset shift tick */
				m_Data.m_nShiftTick = 0;

			}
		}
	}

	/* stop recharging */
	m_Data.m_bReCharging = false;

	/* run command */
	return false;
}
bool C_Exploits::RunExploits()
{
	/* run this code only if we are shifting tickbase trough CL_Move */
	if (!m_Data.m_bRunAlternativeCreateMove)
		return false;

	/* choke everything except final tick */
	*g_Globals->m_Packet.m_bSendPacket = m_Data.m_nShift == 1;

	/* cannot fire between shift */
	g_Globals->m_Packet.m_bCanFire = g_EnginePrediction->CanAttack(true);
	if (!g_Globals->m_Packet.m_bCanFire)
		g_ctx.get_command()->m_nButtons &= ~IN_ATTACK;

	/* run movement features */
	{
		// default movement features
		g_Movement->BunnyHop();
		g_Movement->FastAutoStop();
		g_Movement->MouseCorrections();
		g_Movement->AutoStrafe();

		// rage-related movement features
		g_AntiAim->SlowWalk();
		g_AntiAim->MicroMovement();
		g_AntiAim->JitterMove();
		g_AntiAim->LegMovement();

		// infinity duck
		if (g_SettingsManager->B[_S("misc.infinity_duck")])
			g_ctx.get_command()->m_nButtons |= IN_BULLRUSH;
	}

	/* predict this command and run anti-aims && autostop */
	g_EnginePrediction->StartPrediction();
	{
		/* boost movement function */
		auto BoostMovement = [&]()
		{
			/* get cmd move data */
			float& flForwardMove = g_ctx.get_command()->m_flForwardMove;
			float& flSideMove = g_ctx.get_command()->m_flSideMove;
			int& nButtons = g_ctx.get_command()->m_nButtons;

			/* Force .y movement */
			if (flForwardMove > 5.0f)
			{
				/* force buttons */
				nButtons |= IN_FORWARD;
				nButtons &= ~IN_BACK;

				/* force movement */
				flForwardMove = 450.0f;
			}
			else if (flForwardMove < -5.0f)
			{
				/* force buttons */
				nButtons |= IN_BACK;
				nButtons &= ~IN_FORWARD;

				/* force movement */
				flForwardMove = -450.0f;
			}

			/* Force .x movement */
			if (flSideMove > 5.0f)
			{
				/* force buttons */
				nButtons |= IN_MOVERIGHT;
				nButtons &= ~IN_MOVELEFT;

				/* force movement */
				flSideMove = 450.0f;
			}
			else if (flSideMove < -5.0f)
			{
				/* force buttons */
				nButtons |= IN_MOVELEFT;
				nButtons &= ~IN_MOVERIGHT;

				/* force movement */
				flSideMove = -450.0f;
			}

			/* do not slowdown */
			nButtons &= ~IN_SPEED;
		};

		/* run autostop */
		{
			g_AutoStop->Initialize(true);
			g_AutoStop->RunEarlyAutoStop(true);
		}

		/* get buttons */
		const int nButtons = g_ctx.get_command()->m_buttons;

		/* if we got here due to fakeduck, then force duck in last 7 ticks */
		if (m_Data.m_bInReset || !(g_ctx.local()->m_fFlags() & FL_ONGROUND))
			BoostMovement();
		else if (m_Data.m_bDueToFakeDuck)
		{
			/* Force to duck */
			if (m_Data.m_nShift <= 7)
				g_ctx.get_command()->m_buttons |= IN_DUCK;
		}
		else if (g_SettingsManager->B[_S("ragebot.enable")]) /* Ragebot is enabled */
		{
			/* Get ticks to stop */
			const int nTicksToStop = g_AutoStop->GetTicksToStop();

			/* If extended teleport is active, then boost movement until we reach ticks to stop */
			if (g_ctx.local()->m_vecVelocity().Length2D() < 10.0f)
				g_ctx.get_command()->m_sidemove = g_ctx.get_command()->m_forwardmove = 0.0f;
			else
			{
				if (g_RageBot->GetSettings()->m_bExtendedTeleport)
				{
					/* Do not change movement if we stand */
					if (nTicksToStop > 0)
					{
						/* Boost movement until we reach shift remain value lower or equals ticks to stop */
						if (m_Data.m_nShift <= nTicksToStop + 1)
							g_AutoStop->RunAutoStop();
						else
							BoostMovement();
					}
				}
				else
					g_AutoStop->RunAutoStop();
			}
		}

		/* run anti-aim */
		g_AntiAim->OnCreateMove();
	};
	g_EnginePrediction->FinishPrediction();

	/* finish packet alternative code */
	{
		/* fix movement */
		{
			Math::Normalize3(g_ctx.get_command()->m_angViewAngles);
			Math::ClampAngles(g_ctx.get_command()->m_angViewAngles);
			Math::FixMovement(g_ctx.get_command());
		}

		/* verify cmd ( it is const after this code part and couldn't be changed without new verification )*/
		{
			C_VerifiedUserCmd* m_pVerifiedCmd = SDK::Interfaces::Input->GetVerifiedCmd(g_Globals->m_Packet.m_nSequence);
			if (m_pVerifiedCmd)
			{
				m_pVerifiedCmd->m_Cmd = *g_ctx.get_command();
				m_pVerifiedCmd->m_CRC = m_pVerifiedCmd->m_Cmd.GetChecksum();
			}
		}

		/* run local animations */
		g_LocalAnimations->OnCreateMove();
	}

	/* do not let process default createmove */
	return true;
}

void C_Exploits::ResetData()
{
	m_Data.m_nPerspectiveShift = 0;
	m_Data.m_nShift = 0;
	m_Data.m_nShiftTick = 0;
	m_Data.m_nTicksAllowed = 1;
	m_Data.m_nActiveExploit = 0;
	m_Data.m_nFirstShift = 0;
	m_Data.m_nDefensiveTick = 0;
	m_Data.m_bDueToFakeDuck = false;
	m_Data.m_bUseCLMove = false;
	m_Data.m_bReCharging = false;
	m_Data.m_bInReset = false;
	m_Data.m_bFirstTick = false;
	m_Data.m_nTickBase = 0;
	m_Data.m_bRunAlternativeCreateMove = false;
}