#include "Misc.h"
#include "../Vars.h"

#include "../../Utils/Timer/Timer.hpp"
#include "../Aimbot/AimbotGlobal/AimbotGlobal.h"
#include "../Commands/Commands.h"
#include "../AntiHack/AntiAim.h"


#define CheckIfNonValidNumber(x) (fpclassify(x) == FP_INFINITE || fpclassify(x) == FP_NAN || fpclassify(x) == FP_SUBNORMAL)


extern int attackStringW;
extern int attackStringH;

void CMisc::Run(CUserCmd* pCmd)
{
	Vector OriginalView;
	if (const auto& pLocal = g_EntityCache.GetLocal())
	{
		const auto& pWeapon = pLocal->GetActiveWeapon();
		AccurateMovement(pCmd, pLocal);
		AutoJump(pCmd, pLocal);
		AutoStrafe(pCmd, pLocal, OriginalView);
		AntiBackstab(pLocal, pCmd);
		AutoSwitch(pCmd, pLocal);
	//	NoSpread(pCmd, pWeapon);
	}

	CheatsBypass();
	Teleport(pCmd);


}



void CMisc::RunLate(CUserCmd* pCmd)
{
	if (const auto& pLocal = g_EntityCache.GetLocal())
	{
		FastStop(pCmd, pLocal);
	}
}

void CMisc::AntiBackstab(CBaseEntity* pLocal, CUserCmd* pCmd)
{
	G::AvoidingBackstab = false;
	Vec3 vTargetPos;

	if (!pLocal->IsAlive() || pLocal->IsStunned() || pLocal->IsInBumperKart() || pLocal->IsAGhost())
	{
		return;
	}

	if (G::IsAttacking) { return; }

	const Vec3 vLocalPos = pLocal->GetWorldSpaceCenter();
	CBaseEntity* target = nullptr;


	for (const auto& pEnemy : g_EntityCache.GetGroup(EGroupType::PLAYERS_ENEMIES))
	{
		if (!pEnemy || !pEnemy->IsAlive() || pEnemy->GetClassNum() != CLASS_SPY || pEnemy->IsCloaked() || pEnemy->IsAGhost())
		{
			continue;
		}

		Vec3 vEnemyPos = pEnemy->GetWorldSpaceCenter();
		if (!Utils::VisPos(pLocal, pEnemy, vLocalPos, vEnemyPos)) { continue; }
		if (!target && vLocalPos.DistTo(vEnemyPos) < 150.f)
		{
			target = pEnemy;
			vTargetPos = target->GetWorldSpaceCenter();
		}
		else if (vLocalPos.DistTo(vEnemyPos) < vLocalPos.DistTo(vTargetPos) && vLocalPos.DistTo(vEnemyPos) < 150.f)
		{
			target = pEnemy;
			vTargetPos = target->GetWorldSpaceCenter();
		}
	}

	
	if (target)
	{
		vTargetPos = target->GetWorldSpaceCenter();
		const Vec3 vAngleToSpy = Math::CalcAngle(vLocalPos, vTargetPos);
		G::AvoidingBackstab = true;
		Utils::FixMovement(pCmd, vAngleToSpy);
		pCmd->viewangles = vAngleToSpy;
	}
}

void CMisc::CheatsBypass()
{


	static bool cheatset = false;
	ConVar* sv_cheats = g_ConVars.FindVar("sv_cheats");
	if (Vars::Misc::CheatsBypass.Value && sv_cheats)
	{
		sv_cheats->SetValue(1);
		cheatset = true;
	}
	else
	{
		if (cheatset)
		{
			sv_cheats->SetValue(0);
			cheatset = false;
		}
	}


}

void CMisc::Teleport(const CUserCmd* pCmd)
{
	// Stupid
	static KeyHelper tpKey{ &Vars::Misc::CL_Move::TeleportKey.Value };
	if (tpKey.Down())
	{
		if (Vars::Misc::CL_Move::TeleportMode.Value == 0 && G::TickShiftQueue == 0 && G::ShiftedTicks > 0)
		{
			
			G::TickShiftQueue = Vars::Misc::CL_Move::DTTicks.Value;
		} 
		
	}
	
}

void CMisc::AccurateMovement(CUserCmd* pCmd, CBaseEntity* pLocal)
{
	if (!Vars::Misc::AccurateMovement.Value)
	{
		return;
	}

	if (!pLocal->IsAlive()
		|| pLocal->IsSwimming()
		|| pLocal->IsInBumperKart()
		|| pLocal->IsAGhost()
		|| !pLocal->OnSolid())
	{
		return;
	}

	if (pLocal->GetMoveType() == MOVETYPE_NOCLIP
		|| pLocal->GetMoveType() == MOVETYPE_LADDER
		|| pLocal->GetMoveType() == MOVETYPE_OBSERVER)
	{
		return;
	}

	if (pCmd->buttons & (IN_JUMP | IN_MOVELEFT | IN_MOVERIGHT | IN_FORWARD | IN_BACK))
	{
		return;
	}

	const float Speed = pLocal->GetVecVelocity().Length2D();
	const float SpeedLimit = Vars::Debug::DebugBool.Value ? 2.f : 10.f;	//	does some fucky stuff

	if (Speed > SpeedLimit)
	{
		

		switch (Vars::Misc::AccurateMovement.Value) {
		case 1: {
			Vec3 direction = pLocal->GetVecVelocity().toAngle();
			direction.y = pCmd->viewangles.y - direction.y;
			const Vec3 negatedDirection = direction.fromAngle() * -Speed;
			pCmd->forwardmove = negatedDirection.x;
			pCmd->sidemove = negatedDirection.y;
		
			break;
		}
		case 2: {
			G::ShouldStop = true;
			break;
		}
		}
	}
	else
	{
	
			pCmd->forwardmove = 0.0f;
			pCmd->sidemove = 0.0f;
		
	}
}

void CMisc::AutoJump(CUserCmd* pCmd, CBaseEntity* pLocal)
{
	if (!Vars::Misc::AutoJump.Value
		|| !pLocal->IsAlive()
		|| pLocal->IsSwimming()
		|| pLocal->IsInBumperKart()
		|| pLocal->IsAGhost())
	{
		return;
	}

	if (pLocal->GetMoveType() == MOVETYPE_NOCLIP
		|| pLocal->GetMoveType() == MOVETYPE_LADDER
		|| pLocal->GetMoveType() == MOVETYPE_OBSERVER)
	{
		return;
	}



	static bool s_bState = false;
	static Timer cmdTimer{};
	if (pCmd->buttons & IN_JUMP)
	{
		if (!s_bState && !pLocal->OnSolid())
		{
			pCmd->buttons &= ~IN_JUMP;
		}
		else if (s_bState)
		{
			s_bState = false;
		}
	}
	else if (!s_bState)
	{
		s_bState = true;
	}


}

inline std::vector<Vector> GenerateSpherePoints(const Vector& center, float radius, int numSamples)
{
	std::vector<Vector> points;
	points.reserve(numSamples);

	const float goldenAngle = 2.39996323f;  // precompute the golden angle

	for (int i = 0; i < numSamples; ++i)
	{
		const float inclination = acosf(1.0f - 2.0f * i / (numSamples - 1));  // compute the inclination angle
		const float azimuth = goldenAngle * i;  // compute the azimuth angle using the golden angle

		const float sin_inc = sinf(inclination);
		const float cos_inc = cosf(inclination);
		const float sin_azi = sinf(azimuth);
		const float cos_azi = cosf(azimuth);

		// compute the sample point on the unit sphere
		const Vector sample = Vector(cos_azi * sin_inc, sin_azi * sin_inc, cos_inc) * radius;

		// translate the point to the desired center
		points.push_back(center + sample);
	}

	return points;
}

float get(CUserCmd* ucmd) {
	if (const auto& pLocal = g_EntityCache.GetLocal()) {

		if (!pLocal)
			return false;
		static int g_tick = 0;
		static CUserCmd* g_pLastCmd = nullptr;
		if (!g_pLastCmd || g_pLastCmd->hasbeenpredicted) {
			g_tick = pLocal->m_nTickBase();
		}
		else {
			// Required because prediction only runs on frames, not ticks
			// So if your framerate goes below tickrate, m_nTickBase won't update every tick
			++g_tick;
		}
		g_pLastCmd = ucmd;
		float curtime = g_tick * I::GlobalVars->interval_per_tick;
		return curtime;
	}
}

bool next(CUserCmd* cmd)
{
	if (const auto& pLocal = g_EntityCache.GetLocal()) {

		if (!pLocal)
			return false;


		float delay = 1.f;
		if (const auto nc = I::EngineClient->GetNetChannelInfo())
		{
			delay = 0.04;


			static float next_lby_update_time = 0.f;

			float curtime = get(cmd);

			if (pLocal->GetVelocity().Length() > 450)
				next_lby_update_time = curtime + delay;


			if (curtime >= next_lby_update_time)
			{
				next_lby_update_time = curtime + delay;
				return true;
			}

			return false;
		}
	}
	return false;
}

void CMisc::AutoSwitch(CUserCmd* pCmd, CBaseEntity* pLocal)
{

}

void CMisc::AutoStrafe(CUserCmd* pCmd, CBaseEntity* pLocal, Vector& OriginalView)
{
	if (!Vars::Misc::AutoStrafe.Value)
	{
		return;
	}



	if (!pLocal->IsAlive()
		|| pLocal->IsSwimming()
		|| pLocal->IsInBumperKart()
		|| pLocal->IsAGhost()
		|| pLocal->OnSolid())
	{
		return;
	}

	if (pLocal->GetMoveType() == MOVETYPE_NOCLIP
		|| pLocal->GetMoveType() == MOVETYPE_LADDER
		|| pLocal->GetMoveType() == MOVETYPE_OBSERVER)
	{
		return;
	}

	static auto cl_sidespeed = g_ConVars.FindVar("cl_sidespeed");
	if (!cl_sidespeed || !cl_sidespeed->GetFloat())
	{
		return;
	}
	

	static bool wasJumping = false;
	const bool isJumping = pCmd->buttons & IN_JUMP;

	switch (Vars::Misc::AutoStrafe.Value)
	{
	default:
		break;
	case 1:
	{
	
		break;
	}
	case 2:
	{
		static Timer cmdTimer{};
		if (Vars::Misc::DuckJump.Value)
		{
			if (pCmd->buttons & IN_DUCK)
			{

			}
			else
			{
				if (!pLocal->OnSolid() && pLocal->IsDucking() && pLocal->m_flDucktime() < 1001)
					pCmd->buttons &= ~IN_DUCK;
				else
					pCmd->buttons |= IN_DUCK;
			}
		}


		if (!isJumping || wasJumping)
		{
			static auto old_yaw = 0.0f;

			auto get_velocity_degree = [](float velocity)
			{
				auto tmp = RAD2DEG(atan(35.0f / velocity));

				if (CheckIfNonValidNumber(tmp) || tmp > 90.0f)
					return 90.0f;

				else if (tmp < 0.0f)
					return 0.0f;
				else
					return tmp;
			};

			if (pLocal->GetMoveType() != MOVETYPE_WALK)
				return;

			auto velocity = pLocal->m_vecVelocity();
			velocity.z = 0.0f;

			auto forwardmove = pCmd->forwardmove;
			auto sidemove = pCmd->sidemove;

			if (velocity.Length2D() < 5.0f && !forwardmove && !sidemove)
				return;

			static auto flip = false;
			flip = !flip;

			auto turn_direction_modifier = flip ? 1.0f : -1.0f;
			auto viewangles = pCmd->viewangles;

			if (forwardmove || sidemove)
			{
				pCmd->forwardmove = 0.0f;
				pCmd->sidemove = 0.0f;

				auto turn_angle = atan2(-sidemove, forwardmove);
				viewangles.y += turn_angle * M_RADPI;
			}
			else if (forwardmove) //-V550
				pCmd->forwardmove = 0.0f;

			auto strafe_angle = RAD2DEG(atan(35.0f / velocity.Length2D()));

			if (strafe_angle > 90.0f)
				strafe_angle = 90.0f;
			else if (strafe_angle < 0.0f)
				strafe_angle = 0.0f;

			auto temp = Vector(0.0f, viewangles.y - old_yaw, 0.0f);
			temp.y = Math::NormalizeYaw(temp.y);

			auto yaw_delta = temp.y;
			old_yaw = viewangles.y;

			auto abs_yaw_delta = fabs(yaw_delta);

			
			

			if (abs_yaw_delta <= strafe_angle || abs_yaw_delta >= 30.f) {
				Vector velocity_angles;
				Math::VectorAngles(velocity, velocity_angles);

				temp = Vector(0.0f, viewangles.y - velocity_angles.y, 0.0f);
				temp.y = Math::NormalizeYaw(temp.y);

				auto velocityangle_yawdelta = temp.y;
				auto velocity_degree = get_velocity_degree(velocity.Length2D())/*menu->main.misc.retrack_speed.value()*/;

				if (velocityangle_yawdelta <= velocity_degree || velocity.Length2D() <= 15.0f)
				{
					if (-velocity_degree <= velocityangle_yawdelta || velocity.Length2D() <= 15.0f)
					{
						viewangles.y += strafe_angle * turn_direction_modifier;
						pCmd->sidemove = cl_sidespeed->GetFloat() * turn_direction_modifier;
					}
					else
					{
						viewangles.y = velocity_angles.y - velocity_degree;
						pCmd->sidemove = cl_sidespeed->GetFloat();
					}
				}
				else
				{
					viewangles.y = velocity_angles.y + velocity_degree;
					pCmd->sidemove = -cl_sidespeed->GetFloat();
				}
			}
		


			auto move = Vector(pCmd->forwardmove, pCmd->sidemove, 0.0f);
			auto speed = move.Length();

			Vector angles_move;
			Math::VectorAngles(move, angles_move);


			auto normalized_y = fmod(pCmd->viewangles.y + 180.0f, 360.0f) - 180.0f;

			auto yaw = DEG2RAD(normalized_y - viewangles.y + angles_move.y);


			pCmd->forwardmove = cos(yaw) * speed;

			pCmd->sidemove = sin(yaw) * speed;
		}
		wasJumping = isJumping;
		break;
	}
	}

};

void CMisc::FastStop(CUserCmd* pCmd, CBaseEntity* pLocal)
{
	if (pLocal && pLocal->IsAlive() && !pLocal->IsTaunting() && !pLocal->IsStunned() && pLocal->GetVelocity().Length2D() > 0.0f) {
		static Vec3 vStartOrigin = {};
		static Vec3 vStartVel = {};
		static int nShiftTick = 0;
		if (pLocal && pLocal->IsAlive())
		{
			if (G::DT)
			{
				if (vStartOrigin.IsZero())
				{
					vStartOrigin = pLocal->GetVecOrigin();
				}

				if (vStartVel.IsZero())
				{
					vStartVel = pLocal->GetVecVelocity();
				}

				const float Speed = pLocal->GetVecVelocity().Length2D();
				const float SpeedLimit = Vars::Debug::DebugBool.Value ? 2.f : 10.f;	//	does some fucky stuff

				if (Speed > SpeedLimit)
				{

					Vec3 direction = pLocal->GetVecVelocity().toAngle();
					direction.y = pCmd->viewangles.y - direction.y;
					const Vec3 negatedDirection = direction.fromAngle() * -Speed;
					pCmd->forwardmove = negatedDirection.x;
					pCmd->sidemove = negatedDirection.y;
				}
				else
				{

					pCmd->forwardmove = 0.0f;
					pCmd->sidemove = 0.0f;

				}

				Vec3 vPredicted = vStartOrigin + (vStartVel * TICKS_TO_TIME(6 - nShiftTick));
				Vec3 vPredictedMax = vStartOrigin + (vStartVel * TICKS_TO_TIME(6));

				float flScale = Math::RemapValClamped(vPredicted.DistTo(vStartOrigin), 0.0f, vPredictedMax.DistTo(vStartOrigin) * 1.5f, 1.0f, 0.0f);
				float flScaleScale = Math::RemapValClamped(vStartVel.Length2D(), 0.f, 520.f, 0.f, 1.f);
				Utils::WalkTo(pCmd, pLocal, vPredictedMax, vStartOrigin, flScale * flScaleScale);

				nShiftTick++;
			}
			else
			{
				vStartOrigin = Vec3(0, 0, 0);
				vStartVel = Vec3(0, 0, 0);
				nShiftTick = 0;
			}
		}
	}
}

bool CanAttack(CBaseEntity* pLocal, const Vec3& pPos)
{
	if (const auto pWeapon = pLocal->GetActiveWeapon())
	{
		if (!G::WeaponCanHeadShot && pLocal->IsScoped()) { return false; }
		if (!pWeapon->CanShoot(pLocal)) { return false; }
		
		for (const auto& target : g_EntityCache.GetGroup(EGroupType::PLAYERS_ENEMIES))
		{
			if (!target->IsAlive()) { continue; }
			if (F::AimbotGlobal.ShouldIgnore(target)) { continue; }

			if (Utils::VisPos(pLocal, target, pPos, target->GetHitboxPos(HITBOX_HEAD)))
			{
				return true;
			}
		}
	}

	return false;
}
