#include "../Hooks.h"

#include "../../Features/Prediction/Prediction.h"
#include "../../Features/Aimbot/Aimbot.h"
#include "../../Features/lagcomp/lagcomp.h"
#include "../../Features/Auto/Auto.h"
#include "../../Features/Aimbot/AimbotHitscan/AimbotHitscan.h"
#include "../../Features/Misc/Misc.h"
#include "../../Features/Visuals/Visuals.h"
#include "../../Features/AntiHack/AntiAim.h"
#include "../../Features/AntiHack/FakeLag/FakeLag.h"
#include "../../Features/Backtrack/Backtrack.h"
#include "../../Features/Visuals/FakeAngleManager/FakeAng.h"
#include "../../Features/CritHack/CritHack.h"
#include "../../Features/Vars.h"
#include "../../Features/AntiHack/Resolver.h"
#include "../../Features/seedprediction/seed.h"

float AngleNormalize(float angle)
{
	angle = fmodf(angle, 360.0f);
	if (angle > 180)
	{
		angle -= 360;
	}
	if (angle < -180)
	{
		angle += 360;
	}
	return angle;
}

enum
{
	TURN_NONE = 0,
	TURN_LEFT,
	TURN_RIGHT
};

int ConvergeAngles(float goal, float maxrate, float maxgap, float dt, float& current)
{
	int direction = TURN_NONE;

	float anglediff = goal - current;
	anglediff = AngleNormalize(anglediff);

	float anglediffabs = fabs(anglediff);

	float scale = 1.0f;
	if (anglediffabs <= 45)
	{
		scale = anglediffabs / 45;
		// Always do at least a bit of the turn ( 1% )
		scale = std::clamp(scale, 0.01f, 1.0f);
	}

	float maxmove = maxrate * dt * scale;

	if (anglediffabs > maxgap)
	{
		// gap is too big, jump
		maxmove = (anglediffabs - maxgap);
	}

	if (anglediffabs < maxmove)
	{
		// we are close enought, just set the final value
		current = goal;
	}
	else
	{
		// adjust value up or down
		if (anglediff > 0)
		{
			current += maxmove;
			direction = TURN_LEFT;
		}
		else
		{
			current -= maxmove;
			direction = TURN_RIGHT;
		}
	}

	current = AngleNormalize(current);

	return direction;
}



void AppendCache()
{
	CBaseEntity* pLocal = g_EntityCache.GetLocal();
	const int tickcount = I::GlobalVars->tickcount;

	for (CBaseEntity* pCaching : g_EntityCache.GetGroup(EGroupType::PLAYERS_ALL))
	{
		std::unordered_map<int, PlayerCache>& openCache = G::Cache[pCaching];
		if (pCaching == pLocal || pCaching->GetDormant())
		{
			openCache.clear();
			continue;
		}

		if (openCache.size() > round(1.f / I::GlobalVars->interval_per_tick))
		{
			openCache.erase(openCache.begin()); // delete the first value if our cache lasts longer than a second.
		}

		openCache[tickcount].m_vecOrigin = pCaching->m_vecOrigin();
		openCache[tickcount].m_vecVelocity = pCaching->m_vecVelocity();
		openCache[tickcount].eyePosition = pCaching->GetEyeAngles();
		openCache[tickcount].playersPredictedTick = TIME_TO_TICKS(pCaching->GetSimulationTime());
	}
}

MAKE_HOOK(ClientModeShared_CreateMove, Utils::GetVFuncPtr(I::ClientModeShared, 21), bool, __fastcall,
	void* ecx, void* edx, float input_sample_frametime, CUserCmd* pCmd)
{
	G::SilentTime = false;
	G::IsAttacking = false;
	G::FakeShotPitch = false;

	if (!pCmd || !pCmd->command_number)
	{
		return Hook.Original<FN>()(ecx, edx, input_sample_frametime, pCmd);
	}

	if (Hook.Original<FN>()(ecx, edx, input_sample_frametime, pCmd))
	{
		I::Prediction->SetLocalViewAngles(pCmd->viewangles);
	}

	// Get the pointer to pSendPacket
	uintptr_t _bp;
	__asm mov _bp, ebp;
	auto pSendPacket = reinterpret_cast<bool*>(***reinterpret_cast<uintptr_t***>(_bp) - 0x1);


	int nOldFlags = 0;
	int nOldGroundEnt = 0;
	Vec3 vOldAngles = pCmd->viewangles;
	float fOldSide = pCmd->sidemove;
	float fOldForward = pCmd->forwardmove;

	G::CurrentUserCmd = pCmd;

	if (const auto& pLocal = g_EntityCache.GetLocal())
	{
		nOldFlags = pLocal->GetFlags();
		nOldGroundEnt = pLocal->m_hGroundEntity();

		if (const int MaxSpeed = pLocal->GetMaxSpeed()) {
			G::Frozen = MaxSpeed == 1;
		}

		if (const auto& pWeapon = g_EntityCache.GetWeapon())
		{
			const int nItemDefIndex = pWeapon->GetItemDefIndex();

			if (G::ShiftedTicks)
			{
				if (G::CurItemDefIndex != nItemDefIndex || !pWeapon->GetClip1() || !pWeapon->GetClip2() || !G::WeaponCanAttack || (!pLocal->IsAlive()|| pLocal->IsTaunting()|| pLocal->IsBonked() || pLocal->GetFeignDeathReady() || pLocal->IsCloaked() || pLocal->IsInBumperKart() || pLocal->IsAGhost()))
				{
					G::WaitForShift = 12;
				}

				if (pLocal->GetClassNum() == CLASS_PYRO && pWeapon->GetSlot() == SLOT_PRIMARY && !G::WeaponCanSecondaryAttack || pLocal->GetClassNum() == CLASS_PYRO && pWeapon->GetSlot() == SLOT_PRIMARY && pCmd->buttons & IN_ATTACK)
				{
					G::WaitForShift = 12;
				}


				

				switch (G::CurItemDefIndex)
				{
				case Scout_m_ForceANature:
				case Scout_m_FestiveForceANature:
				{
					Vars::Misc::CL_Move::DTs.Value = 22;
					break;
				}
				case Scout_m_BabyFacesBlaster:
				{
					Vars::Misc::CL_Move::DTs.Value = 24;
					break;
				}
				case Scout_m_TheShortstop:
				{
					Vars::Misc::CL_Move::DTs.Value = 24;
					break;
				}
				case Scout_m_Scattergun:
				{
					Vars::Misc::CL_Move::DTs.Value = 24;
					break;
				}
				default:
				{
					Vars::Misc::CL_Move::DTs.Value = 22;
					break;
				}
				}

				switch (pWeapon->GetWeaponID())
				{
				case TF_WEAPON_PISTOL:
				case TF_WEAPON_PISTOL_SCOUT:
				{
					Vars::Misc::CL_Move::DTs.Value = 20;
					break;
				}
				case TF_WEAPON_MINIGUN:
				{
					Vars::Misc::CL_Move::DTs.Value = 21;
					break;
				}
				case TF_WEAPON_DIRECTHIT:
				case TF_WEAPON_ROCKETLAUNCHER:
				case TF_WEAPON_REVOLVER:
				case TF_WEAPON_SHOTGUN_PYRO:
				case TF_WEAPON_SHOTGUN_SOLDIER:
				case TF_WEAPON_SHOTGUN_HWG:
				case TF_WEAPON_SHOTGUN_PRIMARY:
				case TF_WEAPON_SMG:
				case TF_WEAPON_WRENCH:
				{
					Vars::Misc::CL_Move::DTs.Value = 22;
					break;
				}
				case TF_WEAPON_GRENADELAUNCHER:
				case TF_WEAPON_PIPEBOMBLAUNCHER:
				{
					Vars::Misc::CL_Move::DTs.Value = 24;
					break;
				}
				default: 
				{
					Vars::Misc::CL_Move::DTs.Value = 22;
					break;
				}
				}

				switch (G::CurItemDefIndex)
				{
				case Soldier_m_RocketJumper:
				case Demoman_s_StickyJumper:
				case Engi_s_TheShortCircuit:
				case Engi_s_TheWrangler:
				case Pyro_s_ThermalThruster:
				{
					G::WaitForShift = DT_WAIT_CALLS;
					break;
				}
				default: break;
				}

				switch (pWeapon->GetWeaponID())
				{
				case TF_WEAPON_FLAREGUN:
				case TF_WEAPON_KNIFE:
				case TF_WEAPON_SNIPERRIFLE:
				case TF_WEAPON_JAR:
				case TF_WEAPON_MEDIGUN:
				case TF_WEAPON_CROSSBOW:
				case TF_WEAPON_COMPOUND_BOW:
				case TF_WEAPON_PDA:
				case TF_WEAPON_LUNCHBOX:
				case TF_WEAPON_PDA_ENGINEER_BUILD:
				case TF_WEAPON_PDA_ENGINEER_DESTROY:
				case TF_WEAPON_PDA_SPY:
				case TF_WEAPON_PDA_SPY_BUILD:
				case TF_WEAPON_BUILDER:
				case TF_WEAPON_INVIS:
				case TF_WEAPON_BUFF_ITEM:
				{
					G::WaitForShift = DT_WAIT_CALLS;
					break;
				}
				default: break;
				}
			}

			G::CurItemDefIndex = nItemDefIndex;
			G::WeaponCanHeadShot = pWeapon->CanWeaponHeadShot();
			G::WeaponCanAttack = pWeapon->CanShoot(pLocal);
			G::WeaponCanSecondaryAttack = pWeapon->CanSecondaryAttack(pLocal);
			G::CurWeaponType = Utils::GetWeaponType(pWeapon);
			G::IsAttacking = Utils::IsAttacking(pCmd, pWeapon);

			if (pWeapon->GetSlot() != SLOT_MELEE)
			{
				if (pWeapon->IsInReload())
				{
					G::WeaponCanAttack = true;
				}

				if (G::CurItemDefIndex != Soldier_m_TheBeggarsBazooka)
				{
					if (pWeapon->GetClip1() == 0)
					{
						G::WeaponCanAttack = false;
					}
				}
			}
		}
	}

	
	
	{
		F::Misc.Run(pCmd);
		F::EnginePrediction.Start(pCmd);
		{
			F::Aimbot.Run(pCmd);
			F::Auto.Run(pCmd);
			F::CritHack.Run(pCmd);
		}
		F::EnginePrediction.End(pCmd);
		F::AntiAim.Run(pCmd, pSendPacket);
		F::FakeLag.OnTick(pCmd, pSendPacket);
		F::Resolver.Update(pCmd);
		F::Misc.RunLate(pCmd);

	}



	if (*pSendPacket) {
		if (Vars::AntiHack::AntiAim::Active.Value)
		{
			
		}
	}


	AppendCache(); // hopefully won't cause issues.
	G::ViewAngles = pCmd->viewangles;

	static int nChoked = G::Recharging;

	if (G::ShouldShift)
	{
		if (!*pSendPacket)
		{
			nChoked++;
		}
		else
		{
			nChoked = 0;
		}
		if (nChoked > 22)
		{
			*pSendPacket = true;
		}

	}

	static bool bWasSet = false;
	if (G::SilentTime)
	{
		*pSendPacket = false;
		bWasSet = true;
	}
	else
	{
		if (bWasSet)
		{
			*pSendPacket = true;
			pCmd->viewangles = vOldAngles;
			pCmd->sidemove = fOldSide;
			pCmd->forwardmove = fOldForward;
			bWasSet = false;
		}
	}

	G::EyeAngDelay++; // Used for the return delay in the viewmodel aimbot
	G::LastUserCmd = pCmd;

	if (G::ForceSendPacket)
	{
		*pSendPacket = true;
		G::ForceSendPacket = false;
	} // if we are trying to force update do this lol
	else if (G::ForceChokePacket)
	{
		*pSendPacket = false;
		G::ForceChokePacket = false;
	} // check after force send to prevent timing out possibly

	// Stop movement if required
	if (G::ShouldStop)
	{
		G::ShouldStop = false;
		Utils::StopMovement(pCmd, !G::ShouldShift);
		return false;
	}

	if (G::SilentTime ||
		G::AAActive ||
		G::FakeShotPitch ||
		G::HitscanSilentActive ||
		G::AvoidingBackstab ||
		G::ProjectileSilentActive ||
		G::RollExploiting)
	{
		return false;
	}

	return Hook.Original<FN>()(ecx, edx, input_sample_frametime, pCmd);
}