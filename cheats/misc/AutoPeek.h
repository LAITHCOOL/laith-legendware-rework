#pragma once
#include "../../includes.hpp"
class CAutopeek {
public:
    bool has_shot;

    void Reset() {
        g_ctx.globals.start_position = Vector{ 0, 0, 0 };
        has_shot = false;
    }
    void GotoStart(CUserCmd* cur_cmd);
    void Run(CUserCmd* cur_cmd);
};

inline CAutopeek * g_AutoPeek = new CAutopeek();