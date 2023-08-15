#pragma once

#include "../../includes.hpp"
#include "..\ragebot\antiaim.h"

class fakelag
{
public:
    void Fakelag(CUserCmd* m_pcmd);
    void Createmove();
    bool FakelagCondition(CUserCmd* m_pcmd);
    void FakeLatency(INetChannel* LatencyChannel, float Latency);
    player_t* e;
    bool condition = true;
    bool started_peeking = false;

    int max_choke = 0;
    player_t* player;
};                   

inline fakelag* g_Fakelag = new fakelag();