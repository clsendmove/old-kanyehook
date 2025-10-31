#include "FakeLag.h"
#include "../../Visuals/FakeAngleManager/FakeAng.h"
#include "../../Visuals/Visuals.h"

void CFakeLag::isAttacker(CGameEvent* pEvent, const FNV1A_t uNameHash)
{

}

bool CFakeLag::IsVisible(CBaseEntity* pLocal)
{
	const Vec3 vVisCheckPoint = pLocal->GetEyePosition();
	const Vec3 vPredictedCheckPoint = pLocal->GetEyePosition() + pLocal->m_vecVelocity() + I::GlobalVars->interval_per_tick * 4;	//	6 ticks in da future
	for (const auto& pEnemy : g_EntityCache.GetGroup(EGroupType::PLAYERS_ENEMIES))
	{
		if (!pEnemy || !pEnemy->IsAlive() || pEnemy->IsCloaked() || pEnemy->IsAGhost() || pEnemy->GetFeignDeathReady() || pEnemy->IsBonked()) { continue; }

		PlayerInfo_t pInfo{};	//	ignored players shouldn't trigger this
		if (!I::EngineClient->GetPlayerInfo(pEnemy->GetIndex(), &pInfo))
		{
			if (G::IsIgnored(pInfo.friendsID)) { continue; }
		}

		const Vec3 vEnemyPos = pEnemy->GetEyePosition();
		if (!Utils::VisPos(pLocal, pEnemy, vVisCheckPoint, vEnemyPos) && (!Utils::VisPos(pLocal, pEnemy, vPredictedCheckPoint, vEnemyPos))) { continue; }
		return true;
	}
	return false;
}

bool CFakeLag::IsAllowed(CBaseEntity* pLocal) {


	if (ChokeCounter >= ChosenAmount || G::IsAttacking) {
		return false;
	}

	return true;
}


void CFakeLag::OnTick(CUserCmd* pCmd, bool* pSendPacket) {
	if (!Vars::Misc::CL_Move::Fakelag.Value) { return; }

	if (const auto& pLocal = g_EntityCache.GetLocal())
	{
		Vec3 vel = pLocal->m_vecVelocity();
		vel.z = 0;
		auto speed = vel.Length();
		auto distance_per_tick = speed * I::GlobalVars->interval_per_tick;
		int choked_ticks = std::ceilf(63.9f / distance_per_tick);

		if (Vars::Misc::CL_Move::FakelagMode.Value != FL_Velocity) {
			ChosenAmount = std::min<int>(choked_ticks, 23 - G::ShiftedTicks); //avoid lagback.
		}

		if (!pLocal || !pLocal->IsAlive())
		{
			if (ChokeCounter > 0)
			{
				*pSendPacket = true;
				ChokeCounter = 0;
			}
			else
			{
				F::FakeAng.DrawChams = false;
			}

			G::IsChoking = false;
			return;
		}

		if (!IsAllowed(pLocal)) {
			ChokeCounter = 0;
			*pSendPacket = true;
			return;
		}

		*pSendPacket = false;
		ChokeCounter++;
	}
}