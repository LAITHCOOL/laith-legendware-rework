#pragma once

#include "../../includes.hpp"
#include "..\ragebot\antiaim.h"
enum
{
    FAKELAG_IN_AIR,
    FAKELAG_ON_LAND,
    FAKELAG_ON_PEEK,
    FAKELAG_ON_SHOT,
    FAKELAG_ON_RELOAD,
    FAKELAG_ON_VELOCITY_CHANGE
};
class fakelag
{
private:
    bool force_ticks_allowed = false;
public:

    void Fakelag(CUserCmd* m_pcmd);
    void ForceTicksAllowedForProcessing();
    void Createmove();
    void SetMoveChokeClampLimit();
    bool FakelagCondition(CUserCmd* m_pcmd);
    void FakeLatency(INetChannel* LatencyChannel, float Latency);
    player_t* e;
    bool condition = true;
    bool started_peeking = false;

    int max_choke = 0;
    player_t* player;
};                   

inline fakelag* g_Fakelag = new fakelag();