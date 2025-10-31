#include "AutoUber.h"

#include "../../Vars.h"
#include "../AutoGlobal/AutoGlobal.h"




class CEntitySphereQue {
public:
	CEntitySphereQue(const Vec3& center, const float radius, const int flagMask = 0,
		const int partitionMask = PARTITION_CLIENT_NON_STATIC_EDICTS)
	{
		static DWORD dwAddress = g_Pattern.Find(_(L"client.dll"), _(L"55 8B EC 83 EC 14 D9 45 0C"));
		reinterpret_cast<void(__thiscall*)(void*, const Vec3&, float, int, int)>(dwAddress)(
			this, center, radius, flagMask, partitionMask);
	}

	CBaseEntity* GetCurrentEntity()
	{
		return (m_nListIndex < m_nListCount) ? m_pList[m_nListIndex] : nullptr;
	}

	void NextEntity()
	{
		m_nListIndex++;
	}

private:
	int m_nListIndex, m_nListCount;
	CBaseEntity* m_pList[MAX_SPHERE_QUERY];
};

bool CheckDetonation(CBaseEntity* pLocal, const std::vector<CBaseEntity*>& entityGroup, float radius)
{
	for (const auto& pExplosive : entityGroup)
	{
		CBaseEntity* pTarget;

		// Iterate through entities in sphere radius
		for (CEntitySphereQue sphere(pExplosive->GetWorldSpaceCenter(), radius);
			(pTarget = sphere.GetCurrentEntity()) != nullptr;
			sphere.NextEntity())
		{
			if (!pTarget || pTarget == pLocal || !pTarget->IsAlive() || pTarget->GetTeamNum() == pLocal->
				GetTeamNum())
			{
				continue;
			}

			const bool bIsPlayer = pTarget->IsPlayer();
		
			if (bIsPlayer)
			{
				if (bIsPlayer && F::AutoGlobal.ShouldIgnore(pTarget)) { continue; }

				CGameTrace trace = {};
				CTraceFilterWorldAndPropsOnly traceFilter = {};
				Utils::Trace(pExplosive->GetWorldSpaceCenter(), pTarget->GetWorldSpaceCenter(), MASK_SOLID, &traceFilter, &trace);

				if (trace.flFraction >= 0.99f || trace.entity == pTarget)
				{
					return true;
				}
			}
		}
	}

	return false;
}

const static int defaultResistance = 0;

int vaccChangeState = 0;
int vaccChangeTicks = 0;
int vaccIdealResist = 0;
int vaccChangeTimer = 0;

enum MinigunState_t
{
	AC_STATE_IDLE = 0,
	AC_STATE_STARTFIRING,
	AC_STATE_FIRING,
	AC_STATE_SPINNING,
	AC_STATE_DRYFIRE
};

int BulletDangerValue(CBaseEntity* pPatient)
{
	if (const auto& pNet = I::EngineClient->GetNetChannelInfo())
	{
		const auto& pWeapon = g_EntityCache.GetWeapon();
		bool sentry = false;

		for (const auto& pBuilding : g_EntityCache.GetGroup(EGroupType::BUILDINGS_ENEMIES))
		{
			const auto& building = reinterpret_cast<CBaseObject*>(pBuilding);
			const auto nType = static_cast<EBuildingType>(building->GetType());
			if (nType == EBuildingType::DISPENSER || nType == EBuildingType::TELEPORTER)
			{
				continue;
			}

			if (building->GetSapped() || building->GetDisabled() || building->GetConstructed())
			{
				continue;
			}

			if (nType == EBuildingType::SENTRY)
			{
				if (pPatient->GetAbsOrigin().DistTo(pBuilding->GetAbsOrigin()) <= 250.f && Utils::VisPos(pPatient, pBuilding, pPatient->GetWorldSpaceCenter(), pBuilding->GetEyePosition()))
				{
					return true;
				}
			}
		}

		const Vec3 vEyePos = pPatient->GetEyePosition();
		float flLatency = ( pNet->GetLatency(FLOW_OUTGOING));
		bool projectile = false;
		for (const auto& pProjectile : g_EntityCache.GetGroup(EGroupType::WORLD_PROJECTILES))
		{
			if (pProjectile->GetTeamNum() == pPatient->GetTeamNum()
				|| pProjectile->GetVelocity().IsZero())
				continue;

			if (pProjectile->GetClassID() != ETFClassID::CTFProjectile_Arrow &&
				pProjectile->GetClassID() != ETFClassID::CTFProjectile_HealingBolt)
				continue;

			Vec3 vPredicted = (pProjectile->GetAbsOrigin() + pProjectile->GetVelocity().Scale(flLatency));

			if (vEyePos.DistTo(vPredicted) <= 225.0f && Utils::VisPos(pPatient, pProjectile, vEyePos, vPredicted))
			{
				return 2;
			}
		}

		bool allowed = false;

		switch (pWeapon->GetWeaponID())
		{
		case TF_WEAPON_PDA:
		case TF_WEAPON_PDA_ENGINEER_BUILD:
		case TF_WEAPON_PDA_ENGINEER_DESTROY:
		case TF_WEAPON_PDA_SPY:
		case TF_WEAPON_PDA_SPY_BUILD:
		case TF_WEAPON_BUILDER:
		case TF_WEAPON_INVIS:
		case TF_WEAPON_BUFF_ITEM:
		{
			allowed = true;
			break;
		}
		default: break;
		}

		bool hitscan = false;

		for (const auto& enemy : g_EntityCache.GetGroup(EGroupType::PLAYERS_ENEMIES))
		{
			if (const auto& pWeapon = enemy->GetActiveWeapon())
			{
				if (!allowed)
				{
					if (!enemy->IsAlive())
					{
						continue;
					}

					if (enemy->GetDormant())
					{
						continue;
					}

					if (HAS_CONDITION(enemy, TFCond_Bonked))
					{
						continue;
					}

					if (pWeapon->GetSlot() == SLOT_MELEE)
					{
						continue;
					}

					if (enemy->GetFeignDeathReady())
					{
						continue;
					}

					if (HAS_CONDITION(enemy, TFCond_Zoomed))
					{
						if (Utils::VisPos(pPatient, enemy, pPatient->GetHitboxPos(HITBOX_HEAD), enemy->GetEyePosition()))
						{
							return 2;
						}
					}

					if (enemy->GetClassNum() == CLASS_MEDIC || enemy->GetClassNum() == CLASS_DEMOMAN)
					{
						continue;
					}

					if (pWeapon->GetClassID() == ETFClassID::CTFShotgun_Pyro || pWeapon->GetClassID() == ETFClassID::CTFShotgun_Soldier)
					{
						if (vEyePos.DistTo(enemy->GetWorldSpaceCenter()) <= 185.0f && Utils::VisPos(pPatient, enemy, pPatient->GetHitboxPos(HITBOX_SPINE_0), enemy->GetEyePosition()))
						{
							return 2;
						}
					}

					if (pWeapon->m_iWeaponState() == AC_STATE_STARTFIRING && pWeapon->m_bCritShot())
					{
						if (vEyePos.DistTo(enemy->GetWorldSpaceCenter()) <= 625.0f && Utils::VisPos(pPatient, enemy, pPatient->GetHitboxPos(HITBOX_SPINE_0), enemy->GetEyePosition()))
						{
							return 2;
						}
					}

					if (pWeapon->m_iWeaponState() == (AC_STATE_STARTFIRING || AC_STATE_FIRING)) 
					{
						if (vEyePos.DistTo(enemy->GetWorldSpaceCenter()) <= 205.0f && Utils::VisPos(pPatient, enemy, pPatient->GetHitboxPos(HITBOX_SPINE_0), enemy->GetEyePosition()))
						{
							return 2;
						}
					}

					if (pWeapon->m_iWeaponState() == AC_STATE_IDLE || pWeapon->m_iWeaponState() == AC_STATE_DRYFIRE || pWeapon->m_iWeaponState() == AC_STATE_SPINNING)
					{
						continue;
					}

					if (enemy->GetClassNum() == CLASS_SCOUT || enemy->GetClassNum() == CLASS_HEAVY && pWeapon->GetSlot() == SLOT_SECONDARY || enemy->GetClassNum() == CLASS_ENGINEER || enemy->GetClassNum() == CLASS_SNIPER || enemy->GetClassNum() == CLASS_SPY)
					{
						if (vEyePos.DistTo(enemy->GetWorldSpaceCenter()) <= 185.0f && Utils::VisPos(pPatient, enemy, pPatient->GetHitboxPos(HITBOX_SPINE_0), enemy->GetEyePosition()))
						{
							return 2;
						}
					}

				}
			}
		}
		return 1;
	}

	return 0;
}

int FireDangerValue(CBaseEntity* pPatient)
{
	if (const auto& pNet = I::EngineClient->GetNetChannelInfo())
	{
		const Vec3 vEyePos = pPatient->GetEyePosition();
		float flLatency = (pNet->GetLatency(FLOW_OUTGOING));
		bool hasHitscan = false;
		for (const auto& pProjectile : g_EntityCache.GetGroup(EGroupType::WORLD_PROJECTILES))
		{
			if (pProjectile->GetTeamNum() == pPatient->GetTeamNum()
				|| pProjectile->GetVelocity().IsZero())
				continue;

			if (pProjectile->GetClassID() != ETFClassID::CTFProjectile_BallOfFire &&
				pProjectile->GetClassID() != ETFClassID::CTFProjectile_Flare &&
				pProjectile->GetClassID() != ETFClassID::CTFProjectile_JarGas &&
				pProjectile->GetClassID() != ETFClassID::CTFProjectile_SpellFireball)
				continue;


			Vec3 vPredicted = (pProjectile->GetAbsOrigin() + pProjectile->GetVelocity().Scale(flLatency));

			if (vEyePos.DistTo(vPredicted) <= 225.0f && Utils::VisPos(pPatient, pProjectile, vEyePos, vPredicted))
			{
				hasHitscan = true;
			}
		}

		float m_flHealth = 0.0f, m_flMaxHealth = 0.0f;
		m_flHealth = static_cast<float>(pPatient->GetHealth());
		m_flMaxHealth = static_cast<float>(pPatient->GetMaxHealth());

		if (hasHitscan || HAS_CONDITION(pPatient, TFCond_OnFire))
		{
			if (((m_flHealth / m_flMaxHealth) * 100.0f) <= 85)
			{
				return 2;
			}
		}
		return 1;
	}
	return 0;
}

int BlastDangerValue(CBaseEntity* pPatient)
{
	if (const auto& pNet = I::EngineClient->GetNetChannelInfo())
	{
		const Vec3 vEyePos = pPatient->GetEyePosition();
		float flLatency = ( pNet->GetLatency(FLOW_OUTGOING));
		bool bIsRocket = false;
		const auto& pWeapon = g_EntityCache.GetWeapon();

		for (const auto& player : g_EntityCache.GetGroup(EGroupType::WORLD_PROJECTILES))
		{
			if (player->GetTeamNum() == pPatient->GetTeamNum()
				|| player->GetVelocity().IsZero())
				continue;

			if (player->GetClassID() != ETFClassID::CTFGrenadePipebombProjectile
				&& player->GetClassID() != ETFClassID::CTFPipebombLauncher
				&& player->GetClassID() != ETFClassID::CTFWeaponBaseGrenadeProj
				&& player->GetClassID() != ETFClassID::CTFProjectile_SentryRocket
				&& player->GetClassID() != ETFClassID::CTFProjectile_Rocket
				&& player->GetClassID() != ETFClassID::CTFProjectile_SpellMeteorShower)
				continue;

			Vec3 vPredicted = (player->GetAbsOrigin() + player->GetVelocity().Scale(flLatency));

			if (vEyePos.DistTo(vPredicted) <= 225.0f && Utils::VisPos(pPatient, player, vEyePos, vPredicted))
			{
				return 2;
			}
		}
		return 1;
	}
	return 0;
}

int CurrentResistance()
{
	if (const auto& pWeapon = g_EntityCache.GetWeapon())
	{
		return pWeapon->GetChargeResistType();
	}
	return 0;
}

int ChargeCount()
{
	if (const auto& pWeapon = g_EntityCache.GetWeapon())
	{
		if (G::CurItemDefIndex == Medic_s_TheVaccinator) { return pWeapon->GetUberCharge() / 0.25f; }
		return pWeapon->GetUberCharge() / 1.f;
	}
	return 1;
}

int OptimalResistance(CBaseEntity* pPatient, bool* pShouldPop)
{
	int bulletDanger = BulletDangerValue(pPatient);
	int fireDanger = FireDangerValue(pPatient);
	int blastDanger = BlastDangerValue(pPatient);
	if (pShouldPop)
	{
		int charges = ChargeCount();
		if (bulletDanger > 1) { *pShouldPop = true; }
		if (fireDanger > 1) { *pShouldPop = true; }
		if (blastDanger > 1) { *pShouldPop = true; }
	}

	if (!(bulletDanger || fireDanger || blastDanger))
	{
		return -1;
	}

	vaccChangeTimer = 0;

	// vaccinator_change_timer = (int) change_timer;
	if (bulletDanger >= fireDanger && bulletDanger >= blastDanger) { return 0; }
	if (blastDanger >= fireDanger && blastDanger >= bulletDanger) { return 1; }
	if (fireDanger >= bulletDanger && fireDanger >= blastDanger) { return 2; }
	return -1;
}

void SetResistance(int pResistance)
{
	Math::Clamp(pResistance, 0, 2);
	vaccChangeTimer = 0;
	vaccIdealResist = pResistance;

	int curResistance = CurrentResistance();
	if (pResistance == curResistance) { return; }
	if (pResistance > curResistance)
	{
		vaccChangeState = pResistance - curResistance;
	}
	else
	{
		vaccChangeState = 3 - curResistance + pResistance;
	}
}

void DoResistSwitching(CUserCmd* pCmd)
{
	if (vaccChangeTimer > 0)
	{
		if (vaccChangeTimer == 1 && false)
		{
			SetResistance(defaultResistance - 1);
		}
		vaccChangeTimer--;
	}
	else
	{
		vaccChangeTimer = 0;
	}

	if (!vaccChangeState) { return; }
	if (CurrentResistance() == vaccIdealResist)
	{
		vaccChangeTicks = 0;
		vaccChangeState = 0;
		return;
	}
	if (pCmd->buttons & IN_RELOAD)
	{
		vaccChangeTicks = 1;
		return;
	}
	if (vaccChangeTicks <= 0)
	{
		pCmd->buttons |= IN_RELOAD;
		vaccChangeState--;
		vaccChangeTicks = 1;
	}
	else
	{
		vaccChangeTicks--;
	}
}

void CAutoUber::Run(CBaseEntity* pLocal, CBaseCombatWeapon* pWeapon, CUserCmd* pCmd)
{
	if (!Vars::Triggerbot::Uber::Active.Value || //Not enabled, return
		pWeapon->GetWeaponID() != TF_WEAPON_MEDIGUN || //Not medigun, return
		G::CurItemDefIndex == Medic_s_TheKritzkrieg || //Kritzkrieg,  return
		ChargeCount() < 1) //Not charged
		return;

	//Check local status, if enabled. Don't pop if local already is not vulnerable
	if (Vars::Triggerbot::Uber::PopLocal.Value)
	{
		m_flHealth = static_cast<float>(pLocal->GetHealth());
		m_flMaxHealth = static_cast<float>(pLocal->GetMaxHealth());

		if (Vars::Triggerbot::Uber::AutoVacc.Value && G::CurItemDefIndex == Medic_s_TheVaccinator)
		{
			// Auto vaccinator
			bool shouldPop = false;



			DoResistSwitching(pCmd);

			int optResistance = OptimalResistance(pLocal, &shouldPop);
			if (optResistance >= 0 && optResistance != CurrentResistance())
			{
				SetResistance(optResistance);
			}

			if (shouldPop && CurrentResistance() == optResistance)
			{
				pCmd->buttons |= IN_ATTACK2;
			}
		}
		else
		{
			// Default medigun
			if (((m_flHealth / m_flMaxHealth) * 100.0f) <= Vars::Triggerbot::Uber::HealthLeft.Value)
			{
				pCmd->buttons |= IN_ATTACK2; //We under the wanted health percentage, pop
				return; //Popped, no point checking our target's status
			}
		}
	}

	//Will be null as long as we aren't healing anyone
	if (const auto& pTarget = pWeapon->GetHealingTarget())
	{
		//Ignore if target is somehow dead, or already not vulnerable
		if (!pTarget->IsAlive() || !pTarget->IsVulnerable())
			return;

		//Dont waste if not a friend, fuck off scrub
		if (Vars::Triggerbot::Uber::OnlyFriends.Value && !g_EntityCache.IsFriend(pTarget->GetIndex()))
			return;

		//Check target's status
		m_flHealth = static_cast<float>(pTarget->GetHealth());
		m_flMaxHealth = static_cast<float>(pTarget->GetMaxHealth());

		int iTargetIndex = pTarget->GetIndex();

		if (Vars::Triggerbot::Uber::VoiceCommand.Value)
		{
			for (auto& iEntity : G::MedicCallers)
			{
				if (iEntity == iTargetIndex)
				{
					pCmd->buttons |= IN_ATTACK2;
					break;
				}
			}
		}
		G::MedicCallers.clear();


		if (Vars::Triggerbot::Uber::AutoVacc.Value && G::CurItemDefIndex == Medic_s_TheVaccinator)
		{
			// Auto vaccinator
			bool shouldPop = false;
			DoResistSwitching(pCmd);

			int optResistance = OptimalResistance(pTarget, &shouldPop);
			if (optResistance >= 0 && optResistance != CurrentResistance())
			{
				SetResistance(optResistance);
			}

			if (shouldPop && CurrentResistance() == optResistance)
			{
				pCmd->buttons |= IN_ATTACK2;
			}
		}
		else
		{
			// Default mediguns
			if (((m_flHealth / m_flMaxHealth) * 100.0f) <= Vars::Triggerbot::Uber::HealthLeft.Value)
			{
				pCmd->buttons |= IN_ATTACK2; //Target under wanted health percentage, pop
			}
		}
	}
}