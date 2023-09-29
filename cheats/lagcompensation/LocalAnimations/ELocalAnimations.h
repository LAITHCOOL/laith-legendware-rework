#pragma once
#include "../../includes.hpp"

class EC_LocalAnimations
{
public:
	virtual void Instance(CUserCmd* cmd);
	virtual void SetupShootPosition(CUserCmd* cmd);
	virtual void DoAnimationEvent(int type);
	virtual bool GetCachedMatrix(matrix3x4_t* aMatrix);
	virtual void OnUpdateClientSideAnimation();

	virtual void ResetData();

	virtual std::array < matrix3x4_t, MAXSTUDIOBONES > GetDesyncMatrix();
	virtual std::array < matrix3x4_t, MAXSTUDIOBONES > GetLagMatrix();
	virtual std::array < AnimationLayer, 13 > GetAnimationLayers();
	virtual std::array < AnimationLayer, 13 > GetFakeAnimationLayers();
	Vector m_angViewAngles;


private:
	struct
	{
		std::array < AnimationLayer, 13 > m_AnimationLayers;
		std::array < AnimationLayer, 13 > m_FakeAnimationLayers;

		std::array < float_t, 24 > m_PoseParameters;
		std::array < float_t, 24 > m_FakePoseParameters;
		C_CSGOPlayerAnimationState m_FakeAnimationState;

		float_t m_flSpawnTime = 0.0f;
		float_t m_flLowerBodyYaw = 0.0f;
		float_t m_flNextLowerBodyYawUpdateTime = 0.0f;

		std::array < int32_t, 2 > m_iMoveType;
		std::array < int32_t, 2 > m_iFlags;

		std::array < Vector, MAXSTUDIOBONES > m_vecBoneOrigins;
		std::array < Vector, MAXSTUDIOBONES > m_vecFakeBoneOrigins;

		Vector m_vecNetworkedOrigin = Vector(0, 0, 0);
		Vector m_vecShootPosition = Vector(0, 0, 0);

		bool m_bDidShotAtChokeCycle = false;
		Vector m_angShotChokedAngle = Vector(0, 0, 0);

		float_t m_flShotTime = 0.0f;
		Vector m_angForcedAngles = Vector(0, 0, 0);

		std::array < matrix3x4_t, MAXSTUDIOBONES > m_aMainBones;
		std::array < matrix3x4_t, MAXSTUDIOBONES > m_aDesyncBones;
		std::array < matrix3x4_t, MAXSTUDIOBONES > m_aLagBones;
	} m_LocalData;
};

inline EC_LocalAnimations* g_NewLocalAnims = new EC_LocalAnimations();