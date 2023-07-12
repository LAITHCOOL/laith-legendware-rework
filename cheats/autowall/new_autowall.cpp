#include "new_autowall.h"
static auto trace_filter_simple = util::FindSignature("client.dll", ("55 8B EC 83 E4 F0 83 EC 7C 56 52")) + 0x3D;

void C_AutoWall::CacheWeaponData()
{
	weapon_t* m_Weapon = g_ctx.local()->m_hActiveWeapon().Get();
	if (!m_Weapon)
		return;

	weapon_info_t* m_WeaponData = m_Weapon->get_csweapon_info();
	if (!m_WeaponData)
		return;

	m_Data.m_flMaxRange = m_WeaponData->flRange;
	m_Data.m_flWeaponDamage = m_WeaponData->iDamage;
	m_Data.m_flPenetrationPower = m_WeaponData->flPenetration;
	m_Data.m_flArmorRatio = m_WeaponData->flArmorRatio;
	m_Data.m_flPenetrationDistance = 3000.0f;
	m_Data.m_flRangeModifier = m_WeaponData->flRangeModifier;
}
bool C_AutoWall::HandleBulletPenetration(PenetrationData_t* m_PenetrationData)
{
	bool bHitGrate = m_PenetrationData->m_EnterTrace.contents & CONTENTS_GRATE;
	bool bIsNoDraw = m_PenetrationData->m_EnterTrace.surface.flags & SURF_NODRAW;

	int iEnterMaterial = m_physsurface()->GetSurfaceData(m_PenetrationData->m_EnterTrace.surface.surfaceProps)->game.material;
	if (m_PenetrationData->m_PenetrationCount == 0 && !bHitGrate && !bIsNoDraw
		&& iEnterMaterial != CHAR_TEX_GLASS && iEnterMaterial != CHAR_TEX_GRATE)
		return false;

	if (m_PenetrationData->m_PenetrationCount <= 0 || m_Data.m_flPenetrationPower <= 0.0f)
		return false;

	if (!TraceToExit(m_PenetrationData, m_PenetrationData->m_EnterTrace.endpos, &m_PenetrationData->m_EnterTrace, &m_PenetrationData->m_ExitTrace))
		if (!(m_trace()->GetPointContents_WorldOnly(m_PenetrationData->m_EnterTrace.endpos, MASK_SHOT_HULL) & MASK_SHOT_HULL))
			return false;

	float flDamLostPercent = 0.16f;
	float flDamageModifier = m_PenetrationData->m_flDamageModifier;
	float flPenetrationModifier = m_PenetrationData->m_flPenetrationModifier;

	//
	static auto ff_damage_reduction_bullets = m_cvar()->FindVar(crypt_str("ff_damage_reduction_bullets"));
	//
	const float flDamageReductionBullet = ff_damage_reduction_bullets->GetFloat();
	if (bHitGrate || bIsNoDraw || iEnterMaterial == CHAR_TEX_GLASS || iEnterMaterial == CHAR_TEX_GRATE)
	{
		if (iEnterMaterial == CHAR_TEX_GLASS || iEnterMaterial == CHAR_TEX_GRATE)
		{
			flPenetrationModifier = 3.0f;
			flDamLostPercent = 0.05;
		}
		else
			flPenetrationModifier = 1.0f;

		flDamageModifier = 0.99f;
	}
	else if (iEnterMaterial == CHAR_TEX_FLESH && flDamageReductionBullet == 0
		&& m_PenetrationData->m_EnterTrace.hit_entity && ((player_t*)(m_PenetrationData->m_EnterTrace.hit_entity))->is_player() && ((player_t*)(m_PenetrationData->m_EnterTrace.hit_entity))->m_iTeamNum() == g_ctx.local()->m_iTeamNum())
	{
		if (flDamageReductionBullet == 0)
			return true;

		flPenetrationModifier = flDamageReductionBullet;
		flDamageModifier = flDamageReductionBullet;
	}
	else
	{
		float flExitPenetrationModifier = m_physsurface()->GetSurfaceData(m_PenetrationData->m_ExitTrace.surface.surfaceProps)->game.flPenetrationModifier;
		float flExitDamageModifier = m_physsurface()->GetSurfaceData(m_PenetrationData->m_ExitTrace.surface.surfaceProps)->game.flDamageModifier;
		flPenetrationModifier = (flPenetrationModifier + flExitPenetrationModifier) * 0.5f;
		flDamageModifier = (flDamageModifier + flExitDamageModifier) * 0.5f;
	}

	int iExitMaterial = m_physsurface()->GetSurfaceData(m_PenetrationData->m_ExitTrace.surface.surfaceProps)->game.material;
	if (iEnterMaterial == iExitMaterial)
	{
		if (iExitMaterial == CHAR_TEX_WOOD || iExitMaterial == CHAR_TEX_CARDBOARD)
			flPenetrationModifier = 3.0f;
		else if (iExitMaterial == CHAR_TEX_PLASTIC)
			flPenetrationModifier = 2.0f;
	}

	float flTraceDistance = (m_PenetrationData->m_ExitTrace.endpos - m_PenetrationData->m_EnterTrace.endpos).Length();
	float flPenMod = fmax(0.f, (1.f / flPenetrationModifier));
	float flPercentDamageChunk = m_PenetrationData->m_flCurrentDamage * flDamLostPercent;
	float flPenWepMod = flPercentDamageChunk + fmax(0.0f, (3.0f / m_Data.m_flPenetrationPower) * 1.25f) * (flPenMod * 3.0f);
	float flLostDamageObject = ((flPenMod * (flTraceDistance * flTraceDistance)) / 24.0f);
	float flTotalLostDamage = flPenWepMod + flLostDamageObject;

	m_PenetrationData->m_flCurrentDamage -= fmax(flTotalLostDamage, 0.0f);
	if (m_PenetrationData->m_flCurrentDamage <= 0.0f)
		return false;

	m_PenetrationData->m_vecShootPosition = m_PenetrationData->m_ExitTrace.endpos;
	m_PenetrationData->m_flCurrentDistance += flTraceDistance;
	m_PenetrationData->m_PenetrationCount--;

	return true;
}
bool C_AutoWall::TraceToExit(PenetrationData_t* m_PenetrationData, const Vector& vecStart, CGameTrace* pEnterTrace, CGameTrace* pExitTrace)
{
	// build filter
	std::array < uintptr_t, 4 > aTraceFilterSimple
		=
	{
		*(uintptr_t*)(trace_filter_simple),
		(uintptr_t)(g_ctx.local()),
		NULL,
		NULL,
	};

	// run function
	int	nFirstContents = 0;
	for (int nIteration = 1; nIteration <= 22; nIteration++)
	{
		// increase distance
		float flDistance = (float)(nIteration) * 4.0f;

		// calc new end
		Vector vecEnd = vecStart + (m_PenetrationData->m_vecDirection * flDistance);

		// cache contents
		int nContents = m_trace()->GetPointContents_WorldOnly(vecEnd, MASK_SHOT_HULL | CONTENTS_HITBOX) & (MASK_SHOT_HULL | CONTENTS_HITBOX);
		if (!nFirstContents)
			nFirstContents = nContents;

		Vector vecTraceEnd = vecEnd - (m_PenetrationData->m_vecDirection * 4.0f);
		if (!(nContents & MASK_SHOT_HULL) || ((nContents & CONTENTS_HITBOX) && nFirstContents != nContents))
		{
			// this gets a bit more complicated and expensive when we have to deal with displacements
			m_trace()->TraceLine(vecEnd, vecTraceEnd, MASK_SHOT_HULL | CONTENTS_HITBOX, NULL, COLLISION_GROUP_NONE, &m_PenetrationData->m_ExitTrace);

			// sv_clip_trace_to_players
			g_AutoWall->ClipTraceToPlayers(vecTraceEnd, vecEnd, &m_PenetrationData->m_ExitTrace, (CTraceFilter*)(aTraceFilterSimple.data()), MASK_SHOT_HULL | CONTENTS_HITBOX);

			// we exited the wall into a player's hitbox
			if (m_PenetrationData->m_ExitTrace.startsolid && (m_PenetrationData->m_ExitTrace.surface.flags & SURF_HITBOX))
			{
				m_trace()->TraceLine(vecEnd, vecStart, MASK_SHOT_HULL, m_PenetrationData->m_ExitTrace.hit_entity, COLLISION_GROUP_NONE, &m_PenetrationData->m_ExitTrace);
				if (m_PenetrationData->m_ExitTrace.DidHit() && !m_PenetrationData->m_ExitTrace.startsolid)
					return true;
			}
			else if (m_PenetrationData->m_ExitTrace.DidHit() && !m_PenetrationData->m_ExitTrace.startsolid)
			{
				bool bStartIsNodraw = m_PenetrationData->m_EnterTrace.surface.flags & SURF_NODRAW;
				bool bExitIsNodraw = m_PenetrationData->m_ExitTrace.surface.flags & SURF_NODRAW;

				if (bExitIsNodraw && ((player_t*)(m_PenetrationData->m_ExitTrace.hit_entity))->IsBreakableEntity() && ((player_t*)(m_PenetrationData->m_EnterTrace.hit_entity))->IsBreakableEntity())
					return true;
				else if (!bExitIsNodraw || (bStartIsNodraw && bExitIsNodraw))
				{
					if (m_PenetrationData->m_vecDirection.Dot(m_PenetrationData->m_ExitTrace.plane.normal) <= 1.0f)
						return true;
				}
			}
			else if (m_PenetrationData->m_EnterTrace.DidHitNonWorldEntity() && ((player_t*)(m_PenetrationData->m_EnterTrace.hit_entity))->IsBreakableEntity())
			{
				m_PenetrationData->m_ExitTrace = m_PenetrationData->m_EnterTrace;
				m_PenetrationData->m_ExitTrace.endpos = vecStart + m_PenetrationData->m_vecDirection;

				return true;
			}
		}
	}

	return false;
}
bool C_AutoWall::IsPenetrablePoint(const Vector& vecShootPosition, const Vector& vecTargetPosition)
{
	Vector angLocalAngles;
	m_engine()->GetViewAngles(angLocalAngles);

	Vector vecDirection;
	math::angle_vectors(angLocalAngles, vecDirection);

	CGameTrace Trace;
	m_trace()->TraceLine(vecShootPosition, vecTargetPosition, MASK_SHOT_HULL | CONTENTS_HITBOX, g_ctx.local(), NULL, &Trace);

	if ((int)(Trace.fraction))
		return false;

	PenetrationData_t m_PenetrationData;
	m_PenetrationData.m_EnterTrace = Trace;
	m_PenetrationData.m_vecDirection = vecDirection;
	m_PenetrationData.m_flCurrentDistance = 0.0f;
	m_PenetrationData.m_flCurrentDamage = (float)(g_ctx.local()->m_hActiveWeapon()->get_csweapon_info()->iDamage);
	m_PenetrationData.m_PenetrationCount = 1;
	m_PenetrationData.m_vecShootPosition = vecShootPosition;
	m_PenetrationData.m_vecTargetPosition = vecTargetPosition;

	if (!g_AutoWall->HandleBulletPenetration(&m_PenetrationData))
		return false;

	return true;
}
bool C_AutoWall::SimulateFireBullet(PenetrationData_t* m_PenetrationData)
{



	std::array < uintptr_t, 5 > aSkipTwoEntities
		=
	{


		*(uintptr_t*)(trace_filter_simple),
		(uintptr_t)(g_ctx.local()),
		NULL,
		NULL,
		NULL
	};

	while (m_PenetrationData->m_flCurrentDamage > 0.0f)
	{
		// reset trace
		m_PenetrationData->m_EnterTrace = CGameTrace();
		m_PenetrationData->m_ExitTrace = CGameTrace();

		// calc end point
		Vector vecEnd = m_PenetrationData->m_vecShootPosition + m_PenetrationData->m_vecDirection * (m_Data.m_flMaxRange - m_PenetrationData->m_flCurrentDistance);

		// create ray
		Ray_t Ray;
		Ray.Init(m_PenetrationData->m_vecShootPosition, vecEnd);

		// run trace
		m_trace()->TraceRay(Ray, MASK_SHOT_HULL | CONTENTS_HITBOX, (CTraceFilter*)(aSkipTwoEntities.data()), &m_PenetrationData->m_EnterTrace);
		{
			g_AutoWall->ClipTraceToPlayers
			(
				m_PenetrationData->m_vecShootPosition,
				vecEnd + m_PenetrationData->m_vecDirection * 40.0f,
				&m_PenetrationData->m_EnterTrace,
				(CTraceFilter*)(aSkipTwoEntities.data()),
				MASK_SHOT_HULL | CONTENTS_HITBOX
			);
		}

		// save latest hit player
		aSkipTwoEntities[4] = (uintptr_t)(m_PenetrationData->m_EnterTrace.hit_entity);

		// we didn't hit anything, stop tracing shoot
		if (m_PenetrationData->m_EnterTrace.fraction == 1.0f)
			break;

		/************* MATERIAL DETECTION ***********/
		surfacedata_t* pSurfaceData = m_physsurface()->GetSurfaceData(m_PenetrationData->m_EnterTrace.surface.surfaceProps);

		// set modifiers
		m_PenetrationData->m_flPenetrationModifier = pSurfaceData->game.flPenetrationModifier;
		m_PenetrationData->m_flDamageModifier = pSurfaceData->game.flDamageModifier;

		// calculate the damage based on the distance the bullet travelled.
		m_PenetrationData->m_flCurrentDistance += m_PenetrationData->m_EnterTrace.fraction * (m_Data.m_flMaxRange - m_PenetrationData->m_flCurrentDistance);
		m_PenetrationData->m_flCurrentDamage *= std::powf(m_Data.m_flRangeModifier, (m_PenetrationData->m_flCurrentDistance / 500.0f));

		// check if we reach penetration distance, no more penetrations after that
		// or if our modifyer is super low, just stop the bullet
		if ((m_PenetrationData->m_flCurrentDistance > m_Data.m_flPenetrationDistance && m_Data.m_flPenetrationPower > 0) ||
			m_PenetrationData->m_flPenetrationModifier < 0.1f)
			m_PenetrationData->m_PenetrationCount = 0;

		// handle hit entity
		if (m_PenetrationData->m_EnterTrace.hit_entity)
		{
			player_t* pPlayer = (player_t*)(m_PenetrationData->m_EnterTrace.hit_entity);
			if (pPlayer->is_player() && pPlayer->is_alive() && pPlayer->m_iTeamNum() != g_ctx.local()->m_iTeamNum() && !pPlayer->IsDormant())
			{
				g_AutoWall->ScaleDamage(m_PenetrationData->m_EnterTrace, m_PenetrationData->m_flCurrentDamage);
				return (int)(std::floorf(m_PenetrationData->m_flCurrentDamage)) >= (int)(m_PenetrationData->m_flMinDamage);
			}
		}

		bool bIsBulletStopped = !this->HandleBulletPenetration(m_PenetrationData);
		if (bIsBulletStopped)
			break;


		m_PenetrationData->m_flVisible = true;


	}

	return false;
}
void C_AutoWall::ScaleDamage(CGameTrace Trace, float& flDamage)
{
	player_t* pPlayer = (player_t*)(Trace.hit_entity);
	if (!pPlayer || !pPlayer->is_player() || !pPlayer->is_alive())
		return;

	static auto mp_damage_scale_ct_head = m_cvar()->FindVar(crypt_str("mp_damage_scale_ct_head")); //-V807
	static auto mp_damage_scale_t_head = m_cvar()->FindVar(crypt_str("mp_damage_scale_t_head"));

	static auto mp_damage_scale_ct_body = m_cvar()->FindVar(crypt_str("mp_damage_scale_ct_body"));
	static auto mp_damage_scale_t_body = m_cvar()->FindVar(crypt_str("mp_damage_scale_t_body"));

	float flHeadDamageScale = pPlayer->m_iTeamNum() == 3 ? mp_damage_scale_ct_head->GetFloat() : pPlayer->m_iTeamNum() == 2 ? mp_damage_scale_t_head->GetFloat() : 1.0f;
	const float flBodyDamageScale = pPlayer->m_iTeamNum() == 3 ? mp_damage_scale_ct_body->GetFloat() : pPlayer->m_iTeamNum() == 2 ? mp_damage_scale_t_body->GetFloat() : 1.0f;
	if (pPlayer->m_bHasHeavyArmor())
		flHeadDamageScale *= 0.5f;

	int nHitGroup = Trace.hitgroup;
	switch (nHitGroup)
	{
	case HITGROUP_HEAD:
		flDamage = (flDamage * 4.0f) * flHeadDamageScale;
		break;

	case HITGROUP_STOMACH:
		flDamage = (flDamage * 1.25f) * flBodyDamageScale;
		break;

	case HITGROUP_NECK:
	case HITGROUP_CHEST:
	case HITGROUP_LEFTARM:
	case HITGROUP_RIGHTARM:
		flDamage *= flBodyDamageScale;
		break;

	case HITGROUP_LEFTLEG:
	case HITGROUP_RIGHTLEG:
		flDamage = (flDamage * 0.75f) * flBodyDamageScale;
		break;

	default:
		break;
	}

	// must be armored to calculate ratio
	if (!g_AutoWall->IsArmored(pPlayer, nHitGroup))
	{
		flDamage = std::floorf(flDamage);
		return;
	}

	float fDamageToHealth = flDamage;
	float fDamageToArmor = 0;
	float flArmorBonus = 0.5f;
	float fHeavyArmorBonus = 1.0f;
	float flArmorRatio = m_Data.m_flArmorRatio;

	if (pPlayer->m_bHasHeavyArmor())
	{
		flArmorRatio *= 0.5f;
		flArmorBonus = 0.33f;
		fHeavyArmorBonus = 0.33f;
	}

	fDamageToHealth *= flArmorRatio;
	fDamageToArmor = (flDamage - fDamageToHealth) * (flArmorBonus * fHeavyArmorBonus);
	if (fDamageToArmor > pPlayer->m_ArmorValue())
		fDamageToHealth = flDamage - (float)(pPlayer->m_ArmorValue()) / flArmorBonus;

	flDamage = std::floorf(fDamageToHealth);
}
bool C_AutoWall::IsArmored(player_t* pPlayer, const int& nHitGroup)
{
	bool bIsArmored = false;
	switch (nHitGroup)
	{
	case HITGROUP_HEAD:
		bIsArmored = pPlayer->m_bHasHelmet();
		break;

	default:
		bIsArmored = pPlayer->m_bHasHeavyArmor();
		break;
	}

	if (pPlayer->m_ArmorValue() <= 0)
		return false;

	return bIsArmored;
}
void C_AutoWall::ClipTraceToPlayers(const Vector& vecStart, const Vector& vecEnd, CGameTrace* Trace, CTraceFilter* pTraceFilter, uint32_t nMask)
{
	CGameTrace NewTrace;
	Ray_t Ray;
	Ray.Init(vecStart, vecEnd);

	float flSmallestFraction = Trace->fraction;
	for (int nPlayerID = 1; nPlayerID <= m_globals()->m_maxclients; nPlayerID++)
	{

		player_t* pPlayer = static_cast<player_t*>(m_entitylist()->GetClientEntity(nPlayerID));
		if (!pPlayer || !pPlayer->is_player() || !pPlayer->is_alive() || pPlayer->IsDormant() || pPlayer->m_iTeamNum() == g_ctx.local()->m_iTeamNum())
			continue;

		if (pTraceFilter && !pTraceFilter->ShouldHitEntity(pPlayer, nMask))
			continue;

		const ICollideable* pCollideable = pPlayer->GetCollideable();

		if (pCollideable == nullptr)
			continue;

		// get bounding box
		const Vector vecMin = pCollideable->OBBMins();
		const Vector vecMax = pCollideable->OBBMaxs();

		// calculate world space center
		const Vector vecCenter = (vecMax + vecMin) * 0.5f;
		const Vector vecPosition = vecCenter + pPlayer->m_vecOrigin();

		const Vector vecTo = vecPosition - vecStart;
		Vector vecDirection = vecEnd - vecStart;
		const float flLength = vecDirection.NormalizeInPlace1();

		const float flRangeAlong = vecDirection.Dot(vecTo);
		float flRange = 0.0f;

		// calculate distance to ray
		if (flRangeAlong < 0.0f)
			// off start point
			flRange = -vecTo.Length();
		else if (flRangeAlong > flLength)
			// off end point
			flRange = -(vecPosition - vecEnd).Length();
		else
			// within ray bounds
			flRange = (vecPosition - (vecDirection * flRangeAlong + vecStart)).Length();

		if (flRange < 0.0f || flRange > 60.0f)
			continue;

		m_trace()->ClipRayToEntity(Ray, nMask | CONTENTS_HITBOX, pPlayer, &NewTrace);
		if (NewTrace.fraction < flSmallestFraction)
		{
			*Trace = NewTrace;
			flSmallestFraction = NewTrace.fraction;
		}
	}
}
void C_AutoWall::ScanPoint(PenetrationData_t* m_PenetrationData)
{
	m_PenetrationData->m_vecDirection = g_AutoWall->GetPointDirection(m_PenetrationData->m_vecShootPosition, m_PenetrationData->m_vecTargetPosition);
	m_PenetrationData->m_flCurrentDistance = 0.0f;
	m_PenetrationData->m_PenetrationCount = 4;
	m_PenetrationData->m_flDamageModifier = 0.5f;
	m_PenetrationData->m_flPenetrationModifier = 1.0f;
	m_PenetrationData->m_flCurrentDamage = g_AutoWall->m_Data.m_flWeaponDamage;
	m_PenetrationData->m_bSuccess = g_AutoWall->SimulateFireBullet(m_PenetrationData);
	if (!m_PenetrationData->m_bSuccess)
		m_PenetrationData->m_flCurrentDamage = -1.0f;
}
Vector C_AutoWall::GetPointDirection(const Vector& vecShootPosition, const Vector& vecTargetPosition)
{
	Vector vecDirection;
	Vector angDirection;

	math::vector_angles(vecTargetPosition - vecShootPosition, angDirection);
	math::angle_vectors(angDirection, vecDirection);

	vecDirection.NormalizeInPlace();
	return vecDirection;
}
bool CGameTrace::DidHitWorld() const
{
	return hit_entity == m_entitylist()->GetClientEntity(NULL);
}