#include "..\hooks.hpp"
#include "../../cheats/misc/prediction_system.h"
#include "../../cheats/prediction/EnginePrediction.h"
#include "../../cheats/prediction/Networking.h"
using PacketStart_t = void(__thiscall*)(void*, int, int);

void __fastcall hooks::hooked_packetstart(void* ecx, void* edx, int incoming, int outgoing)
{
	static auto original_fn = clientstate_hook->get_func_address <PacketStart_t>(5);
	g_ctx.local((player_t*)m_entitylist()->GetClientEntity(m_engine()->GetLocalPlayer()), true);

	if (g_Networking->should_process_packetstart(outgoing))
		return original_fn(ecx, incoming, outgoing);
}

using PacketEnd_t = void(__thiscall*)(void*);

void __fastcall hooks::hooked_packetend(void* ecx, void* edx)
{
	static auto original_fn = clientstate_hook->get_func_address <PacketEnd_t>(6);
	g_ctx.local((player_t*)m_entitylist()->GetClientEntity(m_engine()->GetLocalPlayer()), true);

	g_EnginePrediction->OnPostNetworkDataReceived();
	return original_fn(ecx);

}

void __fastcall hooks::hooked_checkfilecrcswithserver(void* ecx, void* edx)
{

}

bool __fastcall hooks::hooked_loosefileallowed(void* ecx, void* edx)
{
	return true;
}