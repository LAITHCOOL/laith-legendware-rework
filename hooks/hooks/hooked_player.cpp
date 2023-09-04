// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: http://www.viva64.com

#include "..\hooks.hpp"
#include "..\..\cheats\lagcompensation\local_animations.h"
#include "..\..\cheats\misc\prediction_system.h"
#include "../../cheats/prediction/EnginePrediction.h"
#include "../../cheats/lagcompensation/LocalAnimFix.hpp"
#include "../../cheats/lagcompensation/animation_system.h"
//_declspec(noinline)bool hooks::setupbones_detour(void* ecx, matrix3x4_t* bone_world_out, int max_bones, int bone_mask, float current_time)
//{
//    auto player = reinterpret_cast<player_t*>(uintptr_t(ecx) - 0x4);
//
//    if (!player->valid(false))
//        return ((SetupBonesFn)original_setupbones)(ecx, bone_world_out, max_bones, bone_mask, current_time);
//
//    if (g_ctx.globals.setuping_bones)
//        return ((SetupBonesFn)original_setupbones)(ecx, bone_world_out, max_bones, bone_mask, current_time);
//
//    if (!g_cfg.ragebot.enable )
//        return ((SetupBonesFn)original_setupbones)(ecx, bone_world_out, max_bones, bone_mask, current_time);
//
//    else if (bone_world_out)
//    {
//        if (player->EntIndex() == g_ctx.local()->EntIndex())
//            g_LocalAnimations->CopyCachedMatrix(bone_world_out, max_bones);
//        else
//            g_Lagcompensation->CopyCachedMatrix(player, bone_world_out);
//    }
//
//    return true;
//}
_declspec(noinline)bool hooks::setupbones_detour(void* ecx, matrix3x4_t* bone_world_out, int max_bones, int bone_mask, float current_time)
{
    auto result = true;

    static auto r_jiggle_bones = m_cvar()->FindVar(crypt_str("r_jiggle_bones"));
    auto r_jiggle_bones_backup = r_jiggle_bones->GetInt();

    r_jiggle_bones->SetValue(0);

    if (!ecx)
        result = ((SetupBonesFn)original_setupbones)(ecx, bone_world_out, max_bones, bone_mask, current_time);
    else if (!g_cfg.ragebot.enable)
        result = ((SetupBonesFn)original_setupbones)(ecx, bone_world_out, max_bones, bone_mask, current_time);
    else
    {
        auto player = reinterpret_cast<player_t*>(uintptr_t(ecx) - 0x4);

        if (!player->valid(false, false))
            result = ((SetupBonesFn)original_setupbones)(ecx, bone_world_out, max_bones, bone_mask, current_time);
        else
        {
            auto animstate = player->get_animation_state();
            auto previous_weapon = animstate ? animstate->m_pLastBoneSetupWeapon : nullptr;

            if (previous_weapon)
                animstate->m_pLastBoneSetupWeapon = animstate->m_pActiveWeapon; //-V1004

            if (g_ctx.globals.setuping_bones)
                result = ((SetupBonesFn)original_setupbones)(ecx, bone_world_out, max_bones, bone_mask, current_time);
            else if (g_cfg.legitbot.enabled && player != g_ctx.local())
                result = ((SetupBonesFn)original_setupbones)(ecx, bone_world_out, max_bones, bone_mask, current_time);
            else if (!g_ctx.local()->is_alive())
                result = ((SetupBonesFn)original_setupbones)(ecx, bone_world_out, max_bones, bone_mask, current_time);
            else if (player == g_ctx.local() && bone_world_out)
                result = g_LocalAnimations->CopyCachedMatrix(bone_world_out, max_bones);
            else if (!player->m_CachedBoneData().Count()) //-V807
                result = ((SetupBonesFn)original_setupbones)(ecx, bone_world_out, max_bones, bone_mask, current_time);
            else if (bone_world_out && max_bones != -1)
                memcpy(bone_world_out, player->m_CachedBoneData().Base(), player->m_CachedBoneData().Count() * sizeof(matrix3x4_t));
           
           

            if (previous_weapon)
                animstate->m_pLastBoneSetupWeapon = previous_weapon;
        }
    }

    r_jiggle_bones->SetValue(r_jiggle_bones_backup);
    return result;
}

bool __fastcall hooks::hooked_setupbones(void* ecx, void* edx, matrix3x4_t* bone_world_out, int max_bones, int bone_mask, float current_time)
{
    return setupbones_detour(ecx, bone_world_out, max_bones, bone_mask, current_time);
}

_declspec(noinline)void hooks::standardblendingrules_detour(player_t* player, int i, CStudioHdr* hdr, Vector* pos, Quaternion* q, float curtime, int boneMask)
{
    // return to original if we are not in game or wait till we find the players.
    if (!player->valid(false))
        return ((StandardBlendingRulesFn)original_standardblendingrules)(player, hdr, pos, q, curtime, boneMask);

    // backup effects.
    auto backup_effects = player->m_fEffects();

    // enable extrapolation(disabling interpolation).
    if (!(player->m_fEffects() & 8))
        player->m_fEffects() |= 8;

    ((StandardBlendingRulesFn)original_standardblendingrules)(player, hdr, pos, q, curtime, boneMask);

    // disable extrapolation after hooks(enabling interpolation).
    if (player->m_fEffects() & 8)
        player->m_fEffects() = backup_effects;
}

void __fastcall hooks::hooked_standardblendingrules(player_t* player, int i, CStudioHdr* hdr, Vector* pos, Quaternion* q, float curtime, int boneMask)
{
    return standardblendingrules_detour(player, i, hdr, pos, q, curtime, boneMask);
}

_declspec(noinline)void hooks::doextrabonesprocessing_detour(player_t* player, CStudioHdr* hdr, Vector* pos, Quaternion* q, const matrix3x4_t& matrix, uint8_t* bone_list, void* context)
{
    // get animstate ptr.
    auto animstate = player->get_animation_state();

    // backup pointer.
    player_t* backup = nullptr;

    // zero out animstate player pointer so CCSGOPlayerAnimState::DoProceduralFootPlant will not do anything.
    if (animstate) {
        // backup player ptr.
        backup = animstate->m_pBaseEntity;

        // null player ptr, GUWOP gang.
        animstate->m_pBaseEntity = nullptr;
    }

    ((DoExtraBonesProcessingFn)original_doextrabonesprocessing)(player, hdr, pos, q, matrix, bone_list, context);

    // restore ptr.
    if (animstate && backup)
        animstate->m_pBaseEntity = backup;
}

void __fastcall hooks::hooked_doextrabonesprocessing(player_t* player, void* edx, CStudioHdr* hdr, Vector* pos, Quaternion* q, const matrix3x4_t& matrix, uint8_t* bone_list, void* context)
{
    return doextrabonesprocessing_detour(player, hdr, pos, q, matrix, bone_list, context);
}

_declspec(noinline)void hooks::updateclientsideanimation_detour(player_t* player)
{

    //if (!player->valid(false))
    //    return  ((UpdateClientSideAnimationFn)original_updateclientsideanimation)(player);

    //if (!g_cfg.ragebot.enable && !g_cfg.legitbot.enabled)
    //    return ((UpdateClientSideAnimationFn)original_updateclientsideanimation)(player);

    //if (g_ctx.globals.updating_skins)
    //    return;

    //// handling clientsided animations on localplayer.
    //if (!g_ctx.globals.updating_animation)
    //{
    //    if (player->EntIndex() == g_ctx.local()->EntIndex())
    //        return ((UpdateClientSideAnimationFn)original_updateclientsideanimation)(player);
    //    else
    //        g_Lagcompensation->OnUpdateClientSideAnimations(player);

    //    return;
    //}

    //return ((UpdateClientSideAnimationFn)original_updateclientsideanimation)(player);

    if (g_ctx.globals.updating_skins)
        return;

   
    if (player == g_ctx.local())
        return ((UpdateClientSideAnimationFn)original_updateclientsideanimation)(player);

    if (g_ctx.globals.updating_animation && player != g_ctx.local())
        return ((UpdateClientSideAnimationFn)original_updateclientsideanimation)(player);

    if (!g_cfg.ragebot.enable && !g_cfg.legitbot.enabled)
        return ((UpdateClientSideAnimationFn)original_updateclientsideanimation)(player);

    if (!player->valid(false, false))
        return ((UpdateClientSideAnimationFn)original_updateclientsideanimation)(player);

}


void __fastcall hooks::hooked_updateclientsideanimation(player_t* player, uint32_t i)
{
    return updateclientsideanimation_detour(player);
}

#include "../../cheats/tickbase shift/tickbase_shift.h"
#include "../../cheats/ragebot/aim.h"
_declspec(noinline)void hooks::physicssimulate_detour(player_t* player)
{
    // don't run the codes if we are not in game, or there is no player, or we are not alive, or the target player is not us.
    if (!player->valid(false, false) || player->EntIndex() != g_ctx.local()->EntIndex())
        return ((PhysicsSimulateFn)original_physicssimulate)(player);

    auto simulation_tick = *(int*)((uintptr_t)player + 0x2AC);

    if (player != g_ctx.local() || !g_ctx.local()->is_alive() || m_globals()->m_tickcount == simulation_tick)
    {
        ((PhysicsSimulateFn)original_physicssimulate)(player);
        return;
    }

    // don't run the code if we didn't even need to process simulation.
    C_CommandContext* cmd_ctx = reinterpret_cast <C_CommandContext*>((uintptr_t)(g_ctx.local()) + 0x350C);
    if (!cmd_ctx || !cmd_ctx->needsprocessing)
    {
        ((PhysicsSimulateFn)original_physicssimulate)(player);
        return;
    }

   /* if (cmd_ctx->command_number == g_ctx.globals.shifting_command_number)
        player->m_nTickBase() = g_EnginePrediction->AdjustPlayerTimeBase(-g_cfg.ragebot.shift_amount);
    else if (cmd_ctx->command_number == g_ctx.globals.shifting_command_number + 1)
        player->m_nTickBase() = g_EnginePrediction->AdjustPlayerTimeBase(g_cfg.ragebot.shift_amount);*/

    g_Ragebot->AdjustRevolverData(cmd_ctx->command_number, cmd_ctx->cmd.m_buttons);

    ((PhysicsSimulateFn)original_physicssimulate)(player);
   
}


void __fastcall hooks::hooked_physicssimulate(player_t* player)
{
    return physicssimulate_detour(player);
}

_declspec(noinline)void hooks::modifyeyeposition_detour(c_baseplayeranimationstate* state, Vector& position)
{
    if (state && g_ctx.globals.in_createmove)
        return ((ModifyEyePositionFn)original_modifyeyeposition)(state, position);
}

void __fastcall hooks::hooked_modifyeyeposition(c_baseplayeranimationstate* state, void* edx, Vector& position)
{
    return modifyeyeposition_detour(state, position);
}

_declspec(noinline)void hooks::calcviewmodelbob_detour(player_t* player, Vector& position)
{
    if (!g_cfg.esp.removals[REMOVALS_LANDING_BOB] || player != g_ctx.local() || !g_ctx.local()->is_alive())
        return ((CalcViewmodelBobFn)original_calcviewmodelbob)(player, position);
}

void __fastcall hooks::hooked_calcviewmodelbob(player_t* player, void* edx, Vector& position)
{
    return calcviewmodelbob_detour(player, position);
}

bool __fastcall hooks::hooked_shouldskipanimframe()
{
    return false;
}


_declspec(noinline)void hooks::setupaliveloop_detour(C_CSGOPlayerAnimationState* animstate)
{
    if (!animstate->m_pBasePlayer->valid(false))
        return;

    AnimationLayer* aliveloop = &animstate->m_pBasePlayer->get_animlayers()[ANIMATION_LAYER_ALIVELOOP];
    if (!aliveloop)
        return;

    if (animstate->m_pBasePlayer->sequence_activity(aliveloop->m_nSequence) != ACT_CSGO_ALIVE_LOOP)
    {
        animstate->set_layer_sequence(aliveloop, ACT_CSGO_ALIVE_LOOP);
        animstate->set_layer_cycle(aliveloop, math::random_float(0.0f, 1.0f));
        animstate->set_layer_rate(aliveloop, animstate->m_pBasePlayer->GetLayerSequenceCycleRate(aliveloop, aliveloop->m_nSequence) * math::random_float(0.8, 1.1f));
    }
    else
    {
        float retain_cycle = aliveloop->m_flCycle;
        if (animstate->m_pWeapon != animstate->m_pWeaponLast)
        {
            animstate->set_layer_sequence(aliveloop, ACT_CSGO_ALIVE_LOOP);
            animstate->set_layer_cycle(aliveloop, retain_cycle);
        }
        else if (animstate->is_layer_sequence_finished(aliveloop, animstate->m_flLastUpdateIncrement))
            animstate->set_layer_rate(aliveloop, animstate->m_pBasePlayer->GetLayerSequenceCycleRate(aliveloop, aliveloop->m_nSequence) * math::random_float(0.8, 1.1f));
        else
            animstate->set_layer_weight(aliveloop, math::RemapValClamped(animstate->m_flSpeedAsPortionOfRunTopSpeed, 0.55f, 0.9f, 1.0f, 0.0f));
    }

    return animstate->increment_layer_cycle(aliveloop, true);
}

void __fastcall hooks::hooked_setupaliveloop(C_CSGOPlayerAnimationState* animstate)
{
    return setupaliveloop_detour(animstate);
}
int hooks::processinterpolatedlist()
{
    static auto allow_extrapolation = *(bool**)(util::FindSignature(crypt_str("client.dll"), crypt_str("A2 ? ? ? ? 8B 45 E8")) + 0x1);

    if (allow_extrapolation)
        *allow_extrapolation = false;

    return ((ProcessInterpolatedListFn)original_processinterpolatedlist)();
}

typedef void(__thiscall* InterpolateServerEntities_t)(void*);
void hooks::hkInterpolateServerEntities(void* ecx)
{
    if (!g_ctx.local())
        return ((InterpolateServerEntities_t)original_o_InterpolateServerEntities)(ecx);


    ((InterpolateServerEntities_t)original_o_InterpolateServerEntities)(ecx);
    return g_LocalAnimations->InterpolateMatricies();
}

using ClampBonesInBBox_t = void(__thiscall*) (void*, matrix3x4_t*, int);
void __fastcall hooks::ClampBonesInBBox(player_t* player, void* edx, matrix3x4_t* matrix, int mask)
{

    auto curtime = m_globals()->m_curtime;

    m_globals()->m_curtime = player->m_flSimulationTime();

    if (player == g_ctx.local())
        m_globals()->m_curtime = TICKS_TO_TIME(m_globals()->m_tickcount);

    ((ClampBonesInBBox_t)original_oClampBonesInBBox)(player, matrix, mask);

    m_globals()->m_curtime = curtime;
}
