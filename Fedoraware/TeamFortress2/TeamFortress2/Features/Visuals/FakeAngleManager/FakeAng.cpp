#include "FakeAng.h"
#include "../../lagcomp/lagcomp.h"
#include "../../../Hooks/HookManager.h"
#include "../../Visuals/chams/Chams.h"
#include "./../../Glow/Glow.h"
#include "../../../Hooks/Hooks.h"
#include "../../Visuals/chams/dme.h"

bool is_fresh()
{
	static int old_tick_count;

	if (old_tick_count != I::GlobalVars->tickcount)
	{
		old_tick_count = I::GlobalVars->tickcount;
		return true;
	}

	return false;
}

void CFakeAng::AnimFix() {
	for (auto ent : g_EntityCache.GetGroup(EGroupType::PLAYERS_ALL))
	{
		if (auto diff = std::clamp(TIME_TO_TICKS(fabs(ent->GetSimulationTime() - ent->GetOldSimulationTime())), 0, 24))
		{
			float oldFrame = I::GlobalVars->frametime;
			I::GlobalVars->frametime = I::Prediction->m_bEnginePaused ? 0.0f : TICK_INTERVAL;
			for (int i = 0; i < diff; i++)
			{
				G::UpdateAnim = true;
				if (Vars::AntiHack::AntiAim::Active.Value) { I::Prediction->SetLocalViewAngles(G::RealViewAngles); }
				ent->UpdateAnim();
				if (Vars::AntiHack::AntiAim::Active.Value) { I::Prediction->SetLocalViewAngles(G::RealViewAngles); }
				G::UpdateAnim = false;
			}
			I::GlobalVars->frametime = oldFrame;
		}
	}
}

#include "FakeAng.h"

void DrawFakeAngles(void* ecx, void* edx, const CBaseEntity* pEntity, const DrawModelState_t& pState, const ModelRenderInfo_t& pInfo);

MAKE_HOOK(ModelRender_DrawModelExecute, Utils::GetVFuncPtr(I::ModelRender, 19), void, __fastcall, void* ecx, void* edx, const DrawModelState_t& pState, const ModelRenderInfo_t& pInfo, matrix3x4* pBoneToWorld)
{
	CBaseEntity* pEntity = I::ClientEntityList->GetClientEntity(pInfo.m_nEntIndex);


	DrawFakeAngles(ecx, edx, pEntity, pState, pInfo);

	if (!F::Glow.m_bRendering) {
		if (F::DMEChams.Render(pState, pInfo, pBoneToWorld)) { return; }
	}


	Hook.Original<FN>()(ecx, edx, pState, pInfo, pBoneToWorld);
}

void DrawFakeAngles(void* ecx, void* edx, const CBaseEntity* pEntity, const DrawModelState_t& pState, const ModelRenderInfo_t& pInfo)
{
	auto OriginalFn = Hooks::ModelRender_DrawModelExecute::Hook.Original<Hooks::ModelRender_DrawModelExecute::FN>();

	if (Vars::AntiHack::AntiAim::Active.Value)
	{
		if (pEntity && pEntity == g_EntityCache.GetLocal())
		{
			if (!F::Glow.m_bRendering && !F::Chams.m_bRendering)
			{
				IMaterial* chosenMat = F::DMEChams.v_MatList.at(Vars::Misc::CL_Move::FLGChams::Material.Value) ? F::DMEChams.v_MatList.at(Vars::Misc::CL_Move::FLGChams::Material.Value) : nullptr;

				I::ModelRender->ForcedMaterialOverride(chosenMat);
				if (chosenMat)
				{
					I::RenderView->SetColorModulation(
						Color::TOFLOAT(Vars::Misc::CL_Move::FLGChams::FakelagColor.r),
						Color::TOFLOAT(Vars::Misc::CL_Move::FLGChams::FakelagColor.g),
						Color::TOFLOAT(Vars::Misc::CL_Move::FLGChams::FakelagColor.b));
				}

				I::RenderView->SetBlend(Color::TOFLOAT(Vars::Misc::CL_Move::FLGChams::FakelagColor.a)); // this is so much better than having a seperate alpha slider lmao
				OriginalFn(ecx, edx, pState, pInfo, reinterpret_cast<matrix3x4*>(&F::FakeAng.BoneMatrix));

				I::ModelRender->ForcedMaterialOverride(nullptr);
				I::RenderView->SetColorModulation(1.0f, 1.0f, 1.0f);

				I::RenderView->SetBlend(1.0f);
			}
		}
	}
}


void CFakeAng::Run(CUserCmd* pCmd)
{
	if (const auto& pLocal = g_EntityCache.GetLocal())
	{
		if (!pCmd)
		{
			return;
		}
		if (pLocal->IsAlive())
		{

			if (const auto& pAnimState = pLocal->GetAnimState())
			{
				

				float flOldFrameTime = I::GlobalVars->frametime;
				int nOldSequence = pLocal->m_nSequence();
				float flOldCycle = pLocal->m_flCycle();
				auto pOldPoseParams = pLocal->GetPoseParam();
				char pOldAnimState[sizeof(CMultiPlayerAnimState)] = {};

				memcpy(pOldAnimState, pAnimState, sizeof(CMultiPlayerAnimState));

				auto Restore = [&]()
					{
						I::GlobalVars->frametime = flOldFrameTime;
						pLocal->m_nSequence() = nOldSequence;
						pLocal->m_flCycle() = flOldCycle;
						pLocal->SetPoseParam(pOldPoseParams);
						memcpy(pAnimState, pOldAnimState, sizeof(CMultiPlayerAnimState));
					};

				I::GlobalVars->frametime = 0.0f;

				
					pAnimState->m_flCurrentFeetYaw;
				

					pAnimState->Update(G::FakeViewAngles.y, G::FakeViewAngles.x);

					pAnimState->m_flGoalFeetYaw;
				

				matrix3x4 bones[128];
				if (pLocal->SetupBones(bones, 128, BONE_USED_BY_ANYTHING, I::GlobalVars->curtime))
				{
					BoneMatrix = *reinterpret_cast<FakeMtrixes*>(bones);
				}

		

				Restore();
			}
		}

	}
}


