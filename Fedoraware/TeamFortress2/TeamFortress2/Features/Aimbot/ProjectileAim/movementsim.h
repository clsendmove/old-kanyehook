#pragma once

#include "../../../SDK/SDK.h"

class CMovementSimulation
{
public:
	CBaseEntity* m_pPlayer = nullptr;
	CMoveData m_MoveData = {};

private:
	//void SetCurrentCommand(CBaseEntity* pPlayer, CUserCmd* pCmd);
	void SetupMoveData(CBaseEntity* pPlayer, CMoveData* pMoveData);
private:
	bool m_bOldInPrediction = false;
	bool m_bOldFirstTimePredicted = false;
	bool bFirstRunTick = false;
	bool strafepred();
	float m_flOldFrametime = 0.0f;
	std::vector< Vector > m_positions = { };

public:
	std::map<CBaseEntity*, std::deque<Vec3>> m_directions = { };
	void compute_turn_speed(CBaseEntity* pPlayer, CMoveData* pMoveData);
	std::map<CBaseEntity*, bool> m_ground_state = { };
	bool Initialize(CBaseEntity* pPlayer);
	float m_old_frame_time = 0.0f, m_turn_speed = 0.0f, m_old_forward_move = 0.0f, m_old_side_move = 0.0f, m_time_stamp = 0.0f;
	void Restore();
	void FillVelocities();
	void RunTick(CMoveData& moveDataOut, Vec3& m_vecAbsOrigin);
	const Vec3& GetOrigin() { return m_MoveData.m_vecAbsOrigin; }
};

ADD_FEATURE(CMovementSimulation, MoveSim)