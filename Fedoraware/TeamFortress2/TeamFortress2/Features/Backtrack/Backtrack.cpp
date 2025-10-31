#include "Backtrack.h"
#include "../Aimbot/AimbotGlobal/AimbotGlobal.h"

bool CBacktrack::IsTickInRange(int tickCount)
{
	auto sv_maxunlag = g_ConVars.FindVar("sv_maxunlag");
	auto nci = I::EngineClient->GetNetChannelInfo();

	if (nci == nullptr || sv_maxunlag == nullptr)
		return false;

	float correct = 0.0f;

	if (nci)
	{
		// add network latency
		correct += nci->GetLatency(FLOW_OUTGOING);
	}

	// calc number of view interpolation ticks - 1
	int lerpTicks = TIME_TO_TICKS(G::LerpTime);

	// add view interpolation latency see C_BaseEntity::GetInterpolationAmount()
	correct += TICKS_TO_TIME(lerpTicks);

	// check bouns [0,sv_maxunlag]
	correct = std::clamp(correct, 0.0f, sv_maxunlag->GetFloat()); 

	// correct tick send by player 
	int targettick = G::CurrentUserCmd->tick_count - lerpTicks;

	// calc difference between tick send by player and our latency based tick
	float deltaTime = correct - TICKS_TO_TIME(tickCount - targettick);

	return std::fabsf(deltaTime) < 0.2f;
}

void CBacktrack::UpdateRecords()
{
	const auto& pLocal = g_EntityCache.GetLocal();
	if (!pLocal || !pLocal->IsAlive())
	{
		for (auto& record : Records)
		{
			record.clear();
		}
		return;
	}

	for (int i = 1; i <= I::EngineClient->GetMaxClients(); i++)
	{
		const auto& pEntity = I::ClientEntityList->GetClientEntity(i);
		if (!pEntity || pEntity->GetDormant() || !pEntity->IsAlive() || !pEntity->IsPlayer() || pEntity->GetTeamNum() == pLocal->GetTeamNum()) { Records[i].clear(); continue; }

	
		matrix3x4 bones[128];
		pEntity->SetupBones(bones, 128, 0x7FF00, pEntity->GetSimulationTime());
		model_t* model = pEntity->GetModel();
		studiohdr_t* hdr = I::ModelInfoClient->GetStudioModel(model);
		const Vec3 vOrigin = pEntity->m_vecOrigin();

		Target_t backtrack;
		if (auto diff = std::clamp(TIME_TO_TICKS(fabs(pEntity->GetSimulationTime() - pEntity->GetOldSimulationTime())), 0, 24))
		{
			if (!Records[i].empty()) {
				const Vec3 vPrevOrigin = Records[i].front().AbsOrigin;
				const Vec3 vDelta = vOrigin - vPrevOrigin;
				if (vDelta.Length2DSqr() > 4096.f)
				{
					backtrack.ShouldBacktrack = false;
					Records[i].clear();
					continue;
				}
			}
		}



		if (model && hdr)
		{
			Records[i].push_front({
			pEntity->GetSimulationTime(),
			I::GlobalVars->tickcount,
			pEntity->GetHitboxPos(backtrack.m_nAimedHitbox),
			pEntity->GetAbsOrigin(),
			*reinterpret_cast<BoneMatrixes*>(&bones),
			model,
			hdr,
			pEntity->GetHitboxSet(),
			pEntity->m_vecMins(),
			pEntity->m_vecMaxs(),
			pEntity->GetWorldSpaceCenter(),
			pEntity->GetEyeAngles()
			});
		}


		while (static_cast<int>(Records[i].size()) > std::clamp(TIME_TO_TICKS(GetLatency()), 0, TIME_TO_TICKS(1.f))) //why 1?, sv_maxunlag
		{
			Records[i].pop_back();
		}
	}
	
}

void CBacktrack::Run()
{
	if (!Vars::Backtrack::Enabled.Value)
	{
		LatencyRampup = 0.f;
		return;
	}

	LatencyRampup = std::min(1.f, LatencyRampup += I::GlobalVars->interval_per_tick);

	if (g_EntityCache.GetLocal())
	{
		UpdateDatagram();
		UpdateRecords();
	}
	else
	{
		Sequences.clear();
	}
}

// Store the last 2048 sequences
void CBacktrack::UpdateDatagram()
{
	const INetChannel* netChannel = I::EngineClient->GetNetChannelInfo();
	if (netChannel)
	{
		if (netChannel->m_nInSequenceNr > LastInSequence)
		{
			LastInSequence = netChannel->m_nInSequenceNr;
			Sequences.push_front(CIncomingSequence(netChannel->m_nInReliableState, netChannel->m_nInSequenceNr, I::GlobalVars->realtime));
		}

		if (Sequences.size() > 2048)
		{
			Sequences.pop_back();
		}
	}
}

// Returns the current (custom) backtrack latency
float CBacktrack::GetLatency()
{
	float realLatency = 0.f;

	if (const auto netChannel = I::EngineClient->GetNetChannelInfo())
	{
		realLatency = std::clamp(netChannel->GetLatency(FLOW_OUTGOING), 0.f, 1.0f);
	}

	float flLatency = Vars::Backtrack::Latency.Value;

	if (flLatency <= 200 || !AllowLatency)
	{
		flLatency = 200;
	}

	return LatencyRampup * std::clamp(flLatency * 0.001f, 0.f, 1.0f - realLatency);
}

// Adjusts the fake latency ping
void CBacktrack::AdjustPing(INetChannel* netChannel)
{
	for (const auto& sequence : Sequences)
	{
		if (I::GlobalVars->realtime - sequence.CurTime >= GetLatency())
		{
			netChannel->m_nInReliableState = sequence.InReliableState;
			netChannel->m_nInSequenceNr = sequence.SequenceNr;
			break;
		}
	}
}

void CBacktrack::ResetLatency()
{
	LastInSequence = 0;
	LatencyRampup = 0.f;
}

std::deque<TickRecord>* CBacktrack::GetPlayerRecords(int entIdx)
{
	if (Records[entIdx].empty())
	{
		return nullptr;
	}

	return &Records[entIdx];
}

// Returns the last valid backtrack tick (further away from the player)
std::optional<TickRecord> CBacktrack::GetLastRecord(int entIdx)
{
	const auto& entRecord = Records[entIdx];
	if (!entRecord.empty()) {
		return { entRecord.back() };
	}

	return std::nullopt;
}

/* Returns the first valid backtrack tick(close to the player) */
std::optional<TickRecord> CBacktrack::GetFirstRecord(int entIdx)
{
	const auto& entRecord = Records[entIdx];
	for (const auto& record : entRecord)
	{
		return record;
	}

	return std::nullopt;
}

/* Returns the best tick for the chosen mode */
std::optional<TickRecord> CBacktrack::GetRecord(int entIdx, BacktrackMode mode)
{
	switch (mode)
	{
	case BacktrackMode::First: { return GetFirstRecord(entIdx); }
	default: { return GetLastRecord(entIdx); }
	}
}