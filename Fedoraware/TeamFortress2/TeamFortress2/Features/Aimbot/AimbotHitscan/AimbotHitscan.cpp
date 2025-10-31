#include "AimbotHitscan.h"
#include "../../Vars.h"
#include "../../Backtrack/Backtrack.h"
#include "../../AntiHack/AntiAim.h"

#include "../../Visuals/Visuals.h"

bool IsHitboxValid(int nHitbox, int index)
{
	switch (nHitbox)
	{
	case -1: return true;
	case HITBOX_HEAD: return index & (1 << 0);
	case HITBOX_PELVIS: 
	case HITBOX_SPINE_0:
	case HITBOX_SPINE_1:
	case HITBOX_SPINE_2:
	case HITBOX_SPINE_3: return index & (1 << 1);
	}
	return false;
};

int CAimbotHitscan::GetHitbox(CBaseEntity* pLocal, CBaseCombatWeapon* pWeapon)
{
	switch (Vars::Aimbot::Hitscan::AimHitbox.Value)
	{
	case 0: { return HITBOX_HEAD; }
	case 1: { return HITBOX_SPINE_0; }
	case 2:
	{
		const int nClassNum = pLocal->GetClassNum();

		if (nClassNum == CLASS_SNIPER)
		{
			if (G::CurItemDefIndex != Sniper_m_TheSydneySleeper)
			{
				return (pLocal->IsScoped() ? HITBOX_HEAD : HITBOX_SPINE_0);
			}

			return HITBOX_SPINE_0;
		}
		if (nClassNum == CLASS_SPY)
		{
			const bool bIsAmbassador = (G::CurItemDefIndex == Spy_m_TheAmbassador || G::
				CurItemDefIndex == Spy_m_FestiveAmbassador);
			return (bIsAmbassador ? HITBOX_HEAD : HITBOX_SPINE_0);
		}

		if (nClassNum == CLASS_HEAVY)
		{
			return HITBOX_SPINE_1;
		}

		return HITBOX_SPINE_0;
	}
	}

	return HITBOX_HEAD;
}

ESortMethod CAimbotHitscan::GetSortMethod()
{
	switch (Vars::Aimbot::Hitscan::SortMethod.Value)
	{
	case 0: return ESortMethod::FOV;
	case 1: return ESortMethod::DISTANCE;
	default: return ESortMethod::UNKNOWN;
	}
}

bool CAimbotHitscan::GetTargets(CBaseEntity* pLocal, CBaseCombatWeapon* pWeapon)
{
	const ESortMethod sortMethod = GetSortMethod();

	F::AimbotGlobal.m_vecTargets.clear();

	const Vec3 vLocalPos = pLocal->GetShootPos();
	const Vec3 vLocalAngles = I::EngineClient->GetViewAngles();

	float fInterp = g_ConVars.cl_interp->GetFloat();

	if (Vars::Aimbot::Global::AimPlayers.Value)
	{
		int nHitbox = GetHitbox(pLocal, pWeapon);
		const bool bIsMedigun = pWeapon->GetWeaponID() == TF_WEAPON_MEDIGUN;
		const bool friendlyfire = pWeapon->GetActiveWeapon();

		static ConVar* mp_friendlyfire = g_ConVars.FindVar("mp_friendlyfire");


		auto ShouldRapeTeam = [](CBaseEntity* TeamPlayer) -> bool
		{
			if (!Vars::Aimbot::Hitscan::ExtinguishTeam.Value)
				return false;

			if (TeamPlayer->IsAlive())
				return true;

			return false;
		};


		for (const auto& pTarget : g_EntityCache.GetGroup(bIsMedigun ? EGroupType::PLAYERS_TEAMMATES : mp_friendlyfire->GetBool() ? EGroupType::PLAYERS_ALL : EGroupType::PLAYERS_ENEMIES))
		{


			if (!pTarget->IsAlive() || pTarget->IsAGhost())
			{
				continue;
			}

			if (pTarget == pLocal)
			{
				continue;
			}

			if (bIsMedigun && (pLocal->GetWorldSpaceCenter().DistTo(pTarget->GetWorldSpaceCenter()) > 472.f))
			{
				continue;
			}

			if (pTarget->m_bTruceActive())
			{
				continue;
			}

			PlayerInfo_t pi{};

			if (mp_friendlyfire->GetBool() && pi.friendsID)
			{
				continue;
			}

			if (F::AimbotGlobal.ShouldIgnore(pTarget, bIsMedigun)) { continue; }


			PriorityHitbox = nHitbox;

			Vec3 vPos = pTarget->GetHitboxPos(nHitbox);
			Vec3 vAngleTo = Math::CalcAngle(vLocalPos, vPos);
			const float flFOVTo = Math::CalcFov(vLocalAngles, vAngleTo);


			if ((sortMethod == ESortMethod::FOV || Vars::Aimbot::Hitscan::RespectFOV.Value) && flFOVTo > Vars::Aimbot::Global::AimFOV.Value)
			{
				continue;
			}

			const float flDistTo = sortMethod == ESortMethod::DISTANCE ? vLocalPos.DistTo(vPos) : 0.0f;

			const uint32_t priorityID = g_EntityCache.GetPR()->GetValid(pTarget->GetIndex()) ? g_EntityCache.GetPR()->GetAccountID(pTarget->GetIndex()) : 0;
			const auto& priority = G::PlayerPriority[priorityID];

			F::AimbotGlobal.m_vecTargets.push_back({
				pTarget, ETargetType::PLAYER, vPos, vAngleTo, flFOVTo, flDistTo, nHitbox, false, priority
				});
		}
	}

	// Buildings
	if (Vars::Aimbot::Global::AimBuildings.Value)
	{
		for (const auto& pBuilding : g_EntityCache.GetGroup(EGroupType::BUILDINGS_ENEMIES))
		{
			if (!pBuilding->IsAlive())
			{
				continue;
			}

			Vec3 vPos = pBuilding->GetWorldSpaceCenter();
			Vec3 vAngleTo = Math::CalcAngle(vLocalPos, vPos);
			const float flFOVTo = Math::CalcFov(vLocalAngles, vAngleTo);

			if ((sortMethod == ESortMethod::FOV || Vars::Aimbot::Hitscan::RespectFOV.Value) && flFOVTo > Vars::Aimbot::Global::AimFOV.Value)
			{
				continue;
			}


			if (pBuilding->m_bTruceActive())
			{
				continue;
			}

			const float flDistTo = sortMethod == ESortMethod::DISTANCE ? vLocalPos.DistTo(vPos) : 0.0f;

			F::AimbotGlobal.m_vecTargets.push_back({ pBuilding, ETargetType::BUILDING, vPos, vAngleTo, flFOVTo, flDistTo });
		}
	}

	// Stickies
	if (Vars::Aimbot::Global::AimStickies.Value)
	{
		for (const auto& pProjectile : g_EntityCache.GetGroup(EGroupType::WORLD_PROJECTILES))
		{
			if (!(pProjectile->GetPipebombType() == TYPE_STICKY))
			{
				continue;
			}

			const auto owner = I::ClientEntityList->GetClientEntityFromHandle(reinterpret_cast<int>(pProjectile->GetThrower()));

			if (!owner)
			{
				continue;
			}

			if ((!pProjectile->GetTouched()) || (owner->GetTeamNum() == pLocal->GetTeamNum()))
			{
				continue;
			}

			if (pProjectile->m_bTruceActive())
			{
				continue;
			}



			Vec3 vPos = pProjectile->GetWorldSpaceCenter();
			Vec3 vAngleTo = Math::CalcAngle(vLocalPos, vPos);
			const float flFOVTo = Math::CalcFov(vLocalAngles, vAngleTo);

			if ((sortMethod == ESortMethod::FOV || Vars::Aimbot::Hitscan::RespectFOV.Value) && flFOVTo > Vars::Aimbot::Global::AimFOV.Value)
			{
				continue;
			}

			const float flDistTo = sortMethod == ESortMethod::DISTANCE ? vLocalPos.DistTo(vPos) : 0.0f;
			F::AimbotGlobal.m_vecTargets.push_back({ pProjectile, ETargetType::STICKY, vPos, vAngleTo, flFOVTo, flDistTo });


		}
	}

	return !F::AimbotGlobal.m_vecTargets.empty();
}

bool CAimbotHitscan::ScanHitboxes(CBaseEntity* pLocal, Target_t& target)
{

	const Vec3 vLocalPos = pLocal->GetShootPos();

	if (target.m_TargetType == ETargetType::PLAYER)
	{
		if (!Vars::Aimbot::Hitscan::ScanHitboxes.Value) // even if they have multibox stuff on, if they are scanning nothing we return false, frick you.
		{
			return false;
		}

		matrix3x4 boneMatrix[128];
		if (const model_t* pModel = target.m_pEntity->GetModel())
		{
			if (const studiohdr_t* pHDR = I::ModelInfoClient->GetStudioModel(pModel))
			{
				if (target.m_pEntity->SetupBones(boneMatrix, 128, 0x100, 0))
				{
					if (const mstudiohitboxset_t* pSet = pHDR->GetHitboxSet(target.m_pEntity->GetHitboxSet()))
					{
						for (int nHitbox = -1; nHitbox < target.m_pEntity->GetNumOfHitboxes(); nHitbox++)
						{
							if (nHitbox == -1) { nHitbox = PriorityHitbox; }
							else if (nHitbox == PriorityHitbox) { continue; }

							if (!IsHitboxValid(nHitbox, Vars::Aimbot::Hitscan::ScanHitboxes.Value)) { continue; }

							Vec3 vHitbox = target.m_pEntity->GetHitboxPos(nHitbox);

							if (nHitbox == 0 ? Utils::VisPosHitboxId(pLocal, target.m_pEntity, vLocalPos, vHitbox, nHitbox) : Utils::VisPos(pLocal, target.m_pEntity, vLocalPos, vHitbox))
								// properly check if we hit the hitbox we were scanning and not just a hitbox. (only if the hitbox we are scanning is head)
							{
								target.m_vPos = vHitbox;
								target.m_vAngleTo = Math::CalcAngle(vLocalPos, vHitbox);
								return true;
							}

							if (IsHitboxValid(nHitbox, Vars::Aimbot::Hitscan::MultiHitboxes.Value))
							{
								if (const mstudiobbox_t* pBox = pSet->hitbox(nHitbox))
								{
									const Vec3 vMins = pBox->bbmin;
									const Vec3 vMaxs = pBox->bbmax;

									const float fScale = 0.7f;
									const std::vector<Vec3> vecPoints = {
									    Vec3(((vMins.x + vMaxs.x) * 1.0f), (vMins.y * fScale), ((vMins.z + vMaxs.z) * 0.5f)),
										Vec3((vMins.x * fScale), ((vMins.y + vMaxs.y) * 1.0f), ((vMins.z + vMaxs.z) * 0.5f)),
										Vec3((vMaxs.x * fScale), ((vMins.y + vMaxs.y) * 1.0f), ((vMins.z + vMaxs.z) * 0.5f))
									};

									for (const auto& point : vecPoints)
									{
										Vec3 vTransformed = {};
										Math::VectorTransform(point, boneMatrix[pBox->bone], vTransformed);

										if (nHitbox == 0
											? Utils::VisPosHitboxId(pLocal, target.m_pEntity, vLocalPos, vTransformed, nHitbox)
											: Utils::VisPos(pLocal, target.m_pEntity, vLocalPos, vHitbox))
											// there is no need to scan multiple times just because we hit the arm or whatever. Only count as failure if this hitbox was head.
										{
											target.m_vPos = vTransformed;
											target.m_vAngleTo = Math::CalcAngle(vLocalPos, vTransformed);
											target.m_bHasMultiPointed = true;
											return true;
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}

	else if (target.m_TargetType == ETargetType::BUILDING)
	{
		for (int nHitbox = 0; nHitbox < target.m_pEntity->GetNumOfHitboxes(); nHitbox++)
		{
			Vec3 vHitbox = target.m_pEntity->GetHitboxPos(nHitbox);

			if (Utils::VisPos(pLocal, target.m_pEntity, vLocalPos, vHitbox))
			{
				target.m_vPos = vHitbox;
				target.m_vAngleTo = Math::CalcAngle(vLocalPos, vHitbox);
				return true;
			}
		}
	}

	return false;
}

bool CAimbotHitscan::ScanBuildings(CBaseEntity* pLocal, Target_t& target)
{
	if (!Vars::Aimbot::Hitscan::ScanBuildings.Value)
	{
		return false;
	}

	const Vec3 vLocalPos = pLocal->GetShootPos();

	const float pointScale = 0.8;

	const Vec3 vMins = target.m_pEntity->GetCollideableMins() * pointScale;
	const Vec3 vMaxs = target.m_pEntity->GetCollideableMaxs() * pointScale;

	const std::vector<Vec3> vecPoints = {
		Vec3(vMins.x * 1.0f, ((vMins.y + vMaxs.y) * 0.5f), ((vMins.z + vMaxs.z) * 0.5f)),
		Vec3(vMaxs.x * 1.0f, ((vMins.y + vMaxs.y) * 0.5f), ((vMins.z + vMaxs.z) * 0.5f)),
		Vec3(((vMins.x + vMaxs.x) * 0.5f), vMins.y * 1.0f, ((vMins.z + vMaxs.z) * 0.5f)),
		Vec3(((vMins.x + vMaxs.x) * 0.5f), vMaxs.y * 1.0f, ((vMins.z + vMaxs.z) * 0.5f)),
		Vec3(((vMins.x + vMaxs.x) * 0.5f), ((vMins.y + vMaxs.y) * 0.5f), vMins.z * 1.0f),
		Vec3(((vMins.x + vMaxs.x) * 0.5f), ((vMins.y + vMaxs.y) * 0.5f), vMaxs.z * 1.0f)
	};

	const matrix3x4& transform = target.m_pEntity->GetRgflCoordinateFrame();

	for (const auto& point : vecPoints)
	{
		Vec3 vTransformed = {};
		Math::VectorTransform(point, transform, vTransformed);

		if (Utils::VisPos(pLocal, target.m_pEntity, vLocalPos, vTransformed))
		{
			target.m_vPos = vTransformed;
			target.m_vAngleTo = Math::CalcAngle(vLocalPos, vTransformed);
			return true;
		}
	}

	return false;
}

bool CAimbotHitscan::VerifyTarget(CUserCmd* pCmd, CBaseEntity* pLocal, CBaseCombatWeapon* pWeapon, Target_t& target)
{
	switch (target.m_TargetType)
	{
	case ETargetType::PLAYER:
	{
		Vec3 hitboxpos;
		if (Vars::Backtrack::Enabled.Value && Vars::Backtrack::LastTick.Value)
		{	
			auto records = F::Backtrack.GetPlayerRecords(target.m_pEntity->GetIndex());
			if (const auto& pLastTick = F::Backtrack.GetRecord(target.m_pEntity->GetIndex(), BacktrackMode::Last))
			{
				if (const auto& pHDR = pLastTick->HDR)
				{
					if (const auto& pSet = pHDR->GetHitboxSet(pLastTick->HitboxSet))
					{
						if (const auto& pBox = pSet->hitbox(target.m_nAimedHitbox))
						{
							const Vec3 vPos = (pBox->bbmin + pBox->bbmax) * 0.5f;
							Vec3 vOut;
							const matrix3x4& bone = pLastTick->BoneMatrix.BoneMatrix[pBox->bone];
							Math::VectorTransform(vPos, bone, vOut);
							hitboxpos = vOut;
							target.SimTime = pLastTick->SimulationTime;
						}
					}
				}
			}

			if (Utils::VisPos(pLocal, target.m_pEntity, pLocal->GetShootPos(), hitboxpos))
			{
				target.m_vAngleTo = Math::CalcAngle(pLocal->GetShootPos(), hitboxpos);
				target.ShouldBacktrack = true;
				return true;
			}

		

			target.ShouldBacktrack = false;
			if (Vars::Backtrack::Latency.Value > 200)
			{
				return false;
			}
		}
		else
		{
			target.ShouldBacktrack = false;
		}

		if (!ScanHitboxes(pLocal, target))
		{
			return false;
		}

		break;
	}

	case ETargetType::BUILDING:
	{
		if (!Utils::VisPos(pLocal, target.m_pEntity, pLocal->GetShootPos(), target.m_vPos))
		{
			//Sentryguns have hitboxes, it's better to use ScanHitboxes for them
			if (target.m_pEntity->GetClassID() == ETFClassID::CObjectSentrygun ? !ScanHitboxes(pLocal, target) : !ScanBuildings(pLocal, target))
			{
				return false;
			}
		}

		break;
	}

	default:
	{

		if (!Utils::VisPos(pLocal, target.m_pEntity, pLocal->GetShootPos() + pLocal->GetVelocity() * TICKS_TO_TIME(2), target.m_vPos))
		{
			return false;
		}

		break;
	}
	}

	return true;
}

bool CAimbotHitscan::GetTarget(CUserCmd* pCmd, CBaseEntity* pLocal, CBaseCombatWeapon* pWeapon, Target_t& outTarget)
{
	if (!GetTargets(pLocal, pWeapon))
	{
		return false;
	}

	F::AimbotGlobal.SortTargets(GetSortMethod());

	for (auto& target : F::AimbotGlobal.m_vecTargets)
	{
		if (!VerifyTarget(pCmd, pLocal, pWeapon, target))
		{
			continue;
		}

		outTarget = target;
		return true;
	}

	return false;
}

void CAimbotHitscan::Aim(CUserCmd* pCmd, Vec3& vAngle)
{


	const int nAimMethod = (Vars::Aimbot::Hitscan::SpectatedSmooth.Value && G::LocalSpectated)
		? 1
		: Vars::Aimbot::Hitscan::AimMethod.Value;



	switch (nAimMethod)
	{
	case 0: //Plain
	{

		pCmd->viewangles = vAngle;
		I::EngineClient->SetViewAngles(pCmd->viewangles);
		break;



	}
	case 1: //Smooth
	{
		if (Vars::Aimbot::Hitscan::SmoothingAmount.Value == 0)
		{
			// plain aim at 0 smoothing factor
			pCmd->viewangles = vAngle;
			I::EngineClient->SetViewAngles(pCmd->viewangles);
		}
		//Calculate delta of current viewangles and wanted angles
		Vec3 vecDelta = vAngle - I::EngineClient->GetViewAngles();

		//Clamp, keep the angle in possible bounds
		Math::ClampAngles(vecDelta);

		//Basic smooth by dividing the delta by wanted smooth amount
		pCmd->viewangles += vecDelta / Vars::Aimbot::Hitscan::SmoothingAmount.Value;

		//Set the viewangles from engine
		I::EngineClient->SetViewAngles(pCmd->viewangles);
		break;
	}

	case 2: //Silent
	{
		if (G::IsAttacking)
		{
			pCmd->viewangles = vAngle;
		}
		break;
	}
	default: break;
	}
}

bool CAimbotHitscan::ShouldFire(CBaseEntity* pLocal, CBaseCombatWeapon* pWeapon, CUserCmd* pCmd, const Target_t& target)
{
	if (!Vars::Aimbot::Global::AutoShoot.Value)
	{//
		return false;
	}

	switch (G::CurItemDefIndex)
	{
	case Sniper_m_TheMachina:
	case Sniper_m_ShootingStar:
	{
		if (!pLocal->IsScoped())
		{
			return false;
		}

		break;
	}
	default: break;
	}

	switch (pLocal->GetClassNum())
	{
	case CLASS_SNIPER:
	{
		const bool bIsScoped = pLocal->IsScoped();

		if (Vars::Aimbot::Hitscan::WaitForHeadshot.Value)
		{
			if (G::CurItemDefIndex != Sniper_m_TheClassic
				&& G::CurItemDefIndex != Sniper_m_TheSydneySleeper
				&& !G::WeaponCanHeadShot && bIsScoped)
			{
				return false;
			}
		}

		if (pWeapon->GetWeaponID() == TF_WEAPON_SNIPERRIFLE_CLASSIC)
		{
			if (pCmd->buttons & IN_ATTACK)
			{
				return false;
			}
			else
			{
				return true;
			}
		}

		if (Vars::Aimbot::Hitscan::WaitForCharge.Value && bIsScoped)
		{
			const int nHealth = target.m_pEntity->GetHealth();
			const bool bIsCritBoosted = pLocal->IsCritBoostedNoMini();

			if (target.m_nAimedHitbox == HITBOX_HEAD && G::CurItemDefIndex !=
				Sniper_m_TheSydneySleeper)
			{
				if (nHealth > 150)
				{
					const float flDamage = Math::RemapValClamped(pWeapon->GetChargeDamage(), 0.0f, 150.0f, 0.0f, 450.0f);
					const int nDamage = static_cast<int>(flDamage);

					if (nDamage < nHealth && nDamage != 450)
					{
						return false;
					}
				}

				else
				{
					if (!bIsCritBoosted && !G::WeaponCanHeadShot)
					{
						return false;
					}
				}
			}

			else
			{
				if (nHealth > (bIsCritBoosted ? 150 : 50))
				{
					float flMult = target.m_pEntity->IsInJarate() ? 1.35f : 1.0f;

					if (bIsCritBoosted)
					{
						flMult = 3.0f;
					}

					const float flMax = 150.0f * flMult;
					const int nDamage = static_cast<int>(pWeapon->GetChargeDamage() * flMult);

					if (nDamage < target.m_pEntity->GetHealth() && nDamage != static_cast<int>(flMax))
					{
						return false;
					}
				}
			}
		}
		break;
	}
	case CLASS_SPY:
	{
		if (Vars::Aimbot::Hitscan::WaitForHeadshot.Value && !pWeapon->AmbassadorCanHeadshot())
		{
			if (target.m_nAimedHitbox == HITBOX_HEAD)
			{
				return false;
			}
		}

		break;
	}
	default: break;
	}

	const int nAimMethod = (Vars::Aimbot::Hitscan::SpectatedSmooth.Value && G::LocalSpectated)
		? 1
		: Vars::Aimbot::Hitscan::AimMethod.Value;

	if (nAimMethod == 1)
	{
		Vec3 vForward = {};
		Math::AngleVectors(pCmd->viewangles, &vForward);
		const Vec3 vTraceStart = pLocal->GetShootPos();
		const Vec3 vTraceEnd = (vTraceStart + (vForward * 8192.0f));

		CGameTrace trace = {};
		CTraceFilterHitscan filter = {};
		filter.pSkip = pLocal;

		

		Utils::Trace(vTraceStart, vTraceEnd, MASK_SHOT, &filter, &trace);

		if (trace.entity != target.m_pEntity)
		{
			return false;
		}

		model_t* model = target.m_pEntity->GetModel();
		studiohdr_t* hdr = I::ModelInfoClient->GetStudioModel(model);
		if (const auto& pHDR = hdr)
		{
			if (const auto& pSet = pHDR->GetHitboxSet(HITBOX_HEAD))
			{
				if (const auto& pBox = pSet->hitbox(target.m_nAimedHitbox))
				{
					if (trace.hitbox != HITBOX_HEAD)
					{
						return false;
					}

					if (!target.m_bHasMultiPointed)
					{
						Vec3 vMins = {}, vMaxs = {}, vCenter = {};
						matrix3x4 matrix = {};

						if (!target.m_pEntity->GetHitboxMinsAndMaxsAndMatrix(HITBOX_HEAD, vMins, vMaxs, matrix, &vCenter))
						{
							return false;
						}

						vMins *= pBox->bbmin;
						vMaxs *= pBox->bbmax;

						if (!Math::RayToOBB(vTraceStart, vForward, vCenter, vMins, vMaxs, matrix))
						{
							return false;
						}
					}
				}
			}
		}
	}
	return true;
}

bool CAimbotHitscan::IsAttacking(const CUserCmd* pCmd, CBaseCombatWeapon* pWeapon)
{
	return ((pCmd->buttons & IN_ATTACK) && G::WeaponCanAttack);
}


void DrawBackTrackHitBox(CBaseEntity* pEntity, Color_t colourface, Color_t colouredge, float time)
{

	if (const auto& pLastTick = F::Backtrack.GetRecord(pEntity->GetIndex(), BacktrackMode::Last))
	{
		const model_t* model = pLastTick->Model;
		const studiohdr_t* hdr = I::ModelInfoClient->GetStudioModel(model);
		const mstudiohitboxset_t* set = hdr->GetHitboxSet(pEntity->GetHitboxSet());

		for (int i{}; i < set->numhitboxes; ++i)
		{

			const mstudiobbox_t* bbox = set->hitbox(i);
			if (!bbox)
			{
				continue;
			}

			/*if (bbox->m_radius <= 0.f) {*/
			matrix3x4 rotMatrix;
			Math::AngleMatrix(bbox->angle, rotMatrix);

			matrix3x4 matrix;
			matrix3x4 boneees[128];
			pEntity->SetupBones(boneees, 128, BONE_USED_BY_ANYTHING, 0);
			Math::ConcatTransforms(pLastTick->BoneMatrix.BoneMatrix[bbox->bone], rotMatrix, matrix);

			const Vec3 vPos = (bbox->bbmin + bbox->bbmax) * 0.5f;
			Vec3 vOut;
			const matrix3x4& bone = pLastTick->BoneMatrix.BoneMatrix[bbox->bone];
			Math::VectorTransform(vPos, bone, vOut);

			Vec3 bboxAngle;
			Math::MatrixAngles(matrix, bboxAngle);

			Math::GetMatrixOrigin(matrix, vOut);

			I::DebugOverlay->AddBoxOverlay2(vOut, bbox->bbmin, bbox->bbmax, bboxAngle, colourface, colouredge, time);

		}
	}
}

void CAimbotHitscan::Run(CBaseEntity* pLocal, CBaseCombatWeapon* pWeapon, CUserCmd* pCmd)
{
	static int nLastTracerTick = pCmd->tick_count;
	static int nextSafeTick = pCmd->tick_count;

	if (!Vars::Aimbot::Global::Active.Value)
	{
		return;
	}

	Target_t target = {};

	const int nWeaponID = pWeapon->GetWeaponID();
	const bool bShouldAim = (Vars::Aimbot::Global::AimKey.Value == VK_LBUTTON ? (pCmd->buttons & IN_ATTACK) : F::AimbotGlobal.IsKeyDown());

	if (bShouldAim)
	{
		if (nWeaponID == TF_WEAPON_MINIGUN)
		{
			pCmd->buttons |= IN_ATTACK2;
		}
	}

	if (GetTarget(pCmd, pLocal, pWeapon, target) && bShouldAim)
	{
		if (nWeaponID != TF_WEAPON_COMPOUND_BOW
			&& pLocal->GetClassNum() == CLASS_SNIPER
			&& pWeapon->GetSlot() == SLOT_PRIMARY)
		{
			const bool bScoped = pLocal->IsScoped();

			if (Vars::Aimbot::Hitscan::AutoScope.Value && !bScoped)
			{
				pCmd->buttons |= IN_ATTACK2;
				return;
			}

			if (Vars::Aimbot::Hitscan::ScopedOnly.Value && !bScoped)
			{
				return;
			}
		}

		G::CurrentTargetIdx = target.m_pEntity->GetIndex();
		G::HitscanRunning = true;
		G::HitscanSilentActive = Vars::Aimbot::Hitscan::AimMethod.Value == 2;

		if (G::HitscanSilentActive)
		{
			G::AimPos = target.m_vPos;
		}

		if (ShouldFire(pLocal, pWeapon, pCmd, target))
		{
			if (nWeaponID == TF_WEAPON_MINIGUN)
			{
				pCmd->buttons |= IN_ATTACK2;
			}

			if (G::CurItemDefIndex == Engi_s_TheWrangler || G::CurItemDefIndex ==
				Engi_s_FestiveWrangler)
			{
				pCmd->buttons |= IN_ATTACK2;
			}

			pCmd->buttons |= IN_ATTACK;
		}

		const bool bIsAttacking = IsAttacking(pCmd, pWeapon);
	

		Vec3 vForward = {};
		Math::AngleVectors(pCmd->viewangles, &vForward);
		const Vec3 vTraceStart = pLocal->GetShootPos();
		const Vec3 vTraceEnd = (vTraceStart + (vForward * 8192.0f));

		CGameTrace trace = {};
		CTraceFilterHitscan filter = {};
		filter.pSkip = pLocal;



		Utils::Trace(vTraceStart, vTraceEnd, MASK_SHOT, &filter, &trace);
		const int iAttachment = pWeapon->LookupAttachment("muzzle");
		pWeapon->GetAttachment(iAttachment, trace.vStartPos);

	

		if (bIsAttacking)
		{
			G::IsAttacking = true;
			I::DebugOverlay->ClearAllOverlays();



			if (target.m_TargetType == ETargetType::PLAYER)
			{
				if (target.ShouldBacktrack)
				{
					DrawBackTrackHitBox(target.m_pEntity, { 0,0,0,0 }, { 255, 255, 255, 255 }, 4);
					auto records = F::Backtrack.GetPlayerRecords(target.m_pEntity->GetIndex());
					if (const auto& pLastTick = F::Backtrack.GetRecord(target.m_pEntity->GetIndex(), BacktrackMode::Last))
					{
						const model_t* model = pLastTick->Model;
						const studiohdr_t* hdr = I::ModelInfoClient->GetStudioModel(model);
						const mstudiohitboxset_t* set = hdr->GetHitboxSet(target.m_pEntity->GetHitboxSet());
						const mstudiobbox_t* bbox = set->hitbox(target.m_nAimedHitbox);
						/*if (bbox->m_radius <= 0.f) {*/
						matrix3x4 rotMatrix;
						Math::AngleMatrix(bbox->angle, rotMatrix);

						matrix3x4 matrix;
						matrix3x4 boneees[128];
						target.m_pEntity->SetupBones(boneees, 128, BONE_USED_BY_ANYTHING, 0);
						Math::ConcatTransforms(pLastTick->BoneMatrix.BoneMatrix[bbox->bone], rotMatrix, matrix);

						const Vec3 vPos = (bbox->bbmin + bbox->bbmax) * 0.5f;
						Vec3 vOut;
						const matrix3x4& bone = pLastTick->BoneMatrix.BoneMatrix[bbox->bone];
						Math::VectorTransform(vPos, bone, vOut);

						Vec3 bboxAngle;
						Math::MatrixAngles(matrix, bboxAngle);

						Math::GetMatrixOrigin(matrix, vOut);

						I::DebugOverlay->AddBoxOverlay2(vOut, bbox->bbmin, bbox->bbmax, bboxAngle, {0,0,0,0}, {255,0,0,255}, 4);

						I::DebugOverlay->AddLineOverlay(trace.vStartPos, pLastTick->HeadPosition, 255, 255, 255, true, 4);
					}
				}
				else
				{
					const model_t* model = target.m_pEntity->GetModel();
					const studiohdr_t* hdr = I::ModelInfoClient->GetStudioModel(model);
					const mstudiohitboxset_t* set = hdr->GetHitboxSet(target.m_pEntity->GetHitboxSet());
					const mstudiobbox_t* bbox = set->hitbox(target.m_nAimedHitbox);

					matrix3x4 rotMatrix;
					Math::AngleMatrix(bbox->angle, rotMatrix);

					matrix3x4 matrix;
					matrix3x4 boneees[128];
					target.m_pEntity->SetupBones(boneees, 128, BONE_USED_BY_ANYTHING, 0);
					Math::ConcatTransforms(boneees[bbox->bone], rotMatrix, matrix);

					Vec3 bboxAngle;
					Math::MatrixAngles(matrix, bboxAngle);

					Vec3 matrixOrigin;
					Math::GetMatrixOrigin(matrix, matrixOrigin);

					I::DebugOverlay->AddBoxOverlay2(matrixOrigin, bbox->bbmin, bbox->bbmax, bboxAngle, { 0,0,0,0 }, { 255,0,0,255 }, 4);
					F::Visuals.DrawHitboxMatrix(target.m_pEntity, { 0,0,0,0 }, { 255, 255, 255, 255 }, 4);
					I::DebugOverlay->AddLineOverlay(trace.vStartPos, target.m_pEntity->GetHitboxPos(target.m_nAimedHitbox), 255, 255, 255, true, 4);
				}
			}


			if (target.m_TargetType == ETargetType::BUILDING)
			{
				I::DebugOverlay->AddBoxOverlay2(target.m_pEntity->GetAbsOrigin(), target.m_pEntity->GetCollideableMins(), target.m_pEntity->GetCollideableMaxs(), Vec3(), {0,0,0,0}, {255, 255, 255, 255}, 4);
				I::DebugOverlay->AddLineOverlay(trace.vStartPos, target.m_vPos, 255, 255, 255, true, 4);
			}

			if (target.m_TargetType == ETargetType::STICKY)
			{
				I::DebugOverlay->AddBoxOverlay2(target.m_pEntity->GetAbsOrigin(), target.m_pEntity->GetCollideableMins(), target.m_pEntity->GetCollideableMaxs(), Vec3(), { 0,0,0,0 }, { 255, 255, 255, 255 }, 4);
				I::DebugOverlay->AddLineOverlay(trace.vStartPos, target.m_vPos, 255, 255, 255, true, 4);
			}
		}

		

		Aim(pCmd, target.m_vAngleTo);

		const float tickCount = G::LerpTime;
		const float simTime = target.ShouldBacktrack ? target.SimTime : target.m_pEntity->GetSimulationTime();
		if ((target.m_TargetType == ETargetType::PLAYER && bIsAttacking) || target.ShouldBacktrack)
		{
			pCmd->tick_count = TIME_TO_TICKS(tickCount + simTime);
		}

	}
}










