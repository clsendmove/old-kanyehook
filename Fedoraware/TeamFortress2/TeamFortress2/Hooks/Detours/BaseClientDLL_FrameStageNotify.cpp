#include "../Hooks.h"
#include "../../Features/Aimbot/ProjectileAim/movementsim.h"
#include "../../Features/Visuals/Visuals.h"

#include "../../Features/Menu/Playerlist/Playerlist.h"
#include "../../Features/Backtrack/Backtrack.h"
#include "../../Features/lagcomp/lagcomp.h"
#include "../../Features/Visuals/FakeAngleManager/FakeAng.h"
#include "../../Features/AntiHack/Resolver.h"
#include "../../Features/Aimbot/ProjectileAim/ProjSim.h"
#include "../../Features/Aimbot/ProjectileAim/projectile.h"

float RealPitch(float WishPitch, bool FakeDown) {
	return FakeDown ? 2160 + WishPitch : -2160 + WishPitch;
}

MAKE_HOOK(animations, g_Pattern.Find(L"client.dll", L"55 8B EC 81 EC ? ? ? ? 53 57 8B F9 8B 9F ? ? ? ? 89 5D E0 85 DB 0F 84"), void, __fastcall, void* ecx, void* edx, float eyeyaw, float eyepitch)
{
	if (!g_EntityCache.GetLocal()) {
		if (G::UpdateAnim) {
			G::UpdateAnim = false;
			return Hook.Original<FN>()(ecx, edx, eyeyaw, eyepitch);
		}
	}

	if (ecx == g_EntityCache.GetLocal()) {
		if (G::UpdateAnim) {
			G::UpdateAnim = false;
			return Hook.Original<FN>()(ecx, edx, G::FakeViewAngles.y, G::RealViewAngles.x);
		}
		return; // we update our animstate ourselves, stop game from doing it
	}

	return  Hook.Original<FN>()(ecx, edx, eyeyaw, eyepitch);
}

MAKE_HOOK(BaseClientDLL_FrameStageNotify, Utils::GetVFuncPtr(I::BaseClientDLL, 35), void, __fastcall,
		  void* ecx, void* edx, EClientFrameStage curStage)
{
	switch (curStage)
	{
		case EClientFrameStage::FRAME_RENDER_START:
		{
			G::PunchAngles = Vec3();

			if (const auto& pLocal = g_EntityCache.GetLocal())
			{
				if (Vars::Visuals::RemovePunch.Value)
				{
					G::PunchAngles = pLocal->GetPunchAngles();
					//Store punch angles to be compesnsated for in aim
					pLocal->ClearPunchAngle(); //Clear punch angles for visual no-recoil
				}
			}

			F::Resolver.Run();
			break;
		}
	}

	Hook.Original<FN>()(ecx, edx, curStage);

	switch (curStage)
	{
		case EClientFrameStage::FRAME_NET_UPDATE_START:
		{
			g_EntityCache.Clear();

			if (const auto& pLocal = g_EntityCache.GetLocal())
			{
		
			}
			break;
		}

		case EClientFrameStage::FRAME_NET_UPDATE_END:
		{
			CUserCmd* pCmd = {};

			F::MoveSim.FillVelocities();
			g_EntityCache.Fill();
			F::Backtrack.Run();
			F::FakeAng.AnimFix();
			G::LocalSpectated = false;

			if (const auto& pLocal = g_EntityCache.GetLocal())
			{
				for (const auto& teammate : g_EntityCache.GetGroup(EGroupType::PLAYERS_TEAMMATES))
				{
					if (teammate->IsAlive() || g_EntityCache.IsFriend(teammate->GetIndex()))
					{
						continue;
					}

					const CBaseEntity* pObservedPlayer = I::ClientEntityList->GetClientEntityFromHandle(teammate->GetObserverTarget());

					if (pObservedPlayer == pLocal)
					{
						G::LocalSpectated = true;
						break;
					}
				}

				if (!G::UpdateAnim)
				{
					pLocal->GetAnimState()->ShouldUpdateAnimState() == true;
					pLocal->GetAnimState()->m_DebugAnimData.m_flAimYaw = G::RealViewAngles.y;
					pLocal->GetAnimState()->m_DebugAnimData.m_flAimPitch = G::RealViewAngles.x;
					pLocal->GetAnimState()->m_MovementData.m_flBodyYawRate = I::GlobalVars->curtime;
					pLocal->GetAnimState()->m_MovementData.m_flRunSpeed = 320;
					pLocal->GetAnimState()->m_MovementData.m_flWalkSpeed = 75;
					pLocal->GetAnimState()->m_MovementData.m_flSprintSpeed = -1;
					pLocal->GetAnimState()->m_flLastAnimationStateClearTime = I::GlobalVars->curtime;
					pLocal->GetAnimState()->m_flLastGroundSpeedUpdateTime = I::GlobalVars->curtime;
					pLocal->GetAnimState()->m_PoseParameterData.m_flLastAimTurnTime = I::GlobalVars->curtime;
					pLocal->GetAnimState()->ShouldUpdateAnimState() == false;

					switch (G::CurrentUserCmd->buttons)
					{
					case IN_FORWARD:
					{
						pLocal->GetAnimState()->m_DebugAnimData.m_vecMoveYaw.x = 5.0;
						break;
					}
					case IN_BACK:
					{
						pLocal->GetAnimState()->m_DebugAnimData.m_vecMoveYaw.x = -5.0;
						break;
					}
					case IN_MOVELEFT:
					{
						pLocal->GetAnimState()->m_DebugAnimData.m_vecMoveYaw.y = -5.0;
						break;
					}

					case IN_MOVERIGHT:
					{
						pLocal->GetAnimState()->m_DebugAnimData.m_vecMoveYaw.y = 5.0;
						break;
					}

					}

					pLocal->GetAnimState()->m_flGoalFeetYaw = G::RealViewAngles.y;
				}
			}


			for (const auto& pLocal : g_EntityCache.GetGroup(EGroupType::PLAYERS_ENEMIES))
			{
				float flOldFrameTime = I::GlobalVars->frametime;
				auto pOldPoseParams = pLocal->GetPoseParam();
				

				auto Restore = [&]()
					{
						I::GlobalVars->frametime = flOldFrameTime;
						pLocal->SetPoseParam(pOldPoseParams);
					};

				I::GlobalVars->frametime = 0.0f;


				pLocal->GetAnimState()->m_flGoalFeetYaw = pLocal->GetEyeAngles().y;

				pLocal->GetAnimState()->Update(pLocal->GetEyeAngles().y,  pLocal->GetEyeAngles().x);

				Restore();
			}

			for (int i = 0; i < I::EngineClient->GetMaxClients(); i++)
			{
				if (const auto& player = I::ClientEntityList->GetClientEntity(i))
				{
					const VelFixRecord record = { player->GetAbsOrigin(), player->m_fFlags(), player->GetSimulationTime()};
					G::VelFixRecords[player] = record;
				}
			}

			F::PlayerList.UpdatePlayers();
			break;
		}

	}
}