#include "lagcomp.h"
#include "../../Hooks/Hooks.h"
#include "../seedprediction/seed.h"

    std::unordered_map<void*, std::pair<int, float>> pAnimatingInfo;

	MAKE_HOOK(GetClientInterpAmount, g_Pattern.Find(L"client.dll", L"55 8B EC A1 ? ? ? ? 83 EC 08 56 A8 01 75 22 8B 0D ? ? ? ? 83 C8 01 A3 ? ? ? ? 68 ? ? ? ? 8B 01 FF 50 34 8B F0 89 35 ? ? ? ? EB 06 8B 35 ? ? ? ? 85 F6 74 68 8B 06 8B CE 8B 40 3C FF D0 8B 0D"), float, __cdecl)
	{
		
			return 0;
		
	}

	MAKE_HOOK(CInterpolatedVarArrayBase__Extrapolate, g_Pattern.Find(L"client.dll", L"55 8B EC 8B 45 0C 8B D1 F3 0F 10 05 ? ? ? ? 56"), void, __fastcall,
		void* ecx, void* edx, void* pOut, void* pOld, void* pNew, float flDestinationTime, float flMaxExtrapolationAmount)
	{
		if (!G::UpdateAnim)
		{ 
			return;
		}

		return Hook.Original<FN>()(ecx, edx, pOut, pOld, pNew, 2, 64);
	}

	MAKE_HOOK(CSequenceTransitioner_CheckForSequenceChange, g_Pattern.Find(L"client.dll", L"55 8B EC 53 8B 5D 08 57 8B F9 85 DB 0F 84 ? ? ? ? 83 7F 0C 00 75 05 E8 ? ? ? ? 6B 4F 0C 2C 0F 57 C0 56 8B 37 83 C6 D4 03 F1 F3 0F 10 4E ? 0F 2E C8 9F F6 C4 44 7B 62 8B 45 0C"), void, __fastcall,
		void* ecx, void* edx, CStudioHdr* hdr, int nCurSequence, bool bForceNewSequence, bool bInterpolate)
	{
		if (!G::UpdateAnim)
		{
			return;
		}

		return Hook.Original<FN>()(ecx, edx, hdr, nCurSequence, true, false);
	}

	MAKE_HOOK(C_BaseAnimating_FrameAdvance, g_Pattern.Find(L"client.dll", L"55 8B EC 83 EC 14 56 8B F1 57 80 BE ? ? ? ? ? 0F 85 ? ? ? ? 83 BE ? ? ? ? ? 75 05 E8 ? ? ? ? 8B BE ? ? ? ? 85 FF 0F 84 ? ? ? ? F3 0F 10 45 ? 0F 57 D2 A1 ? ? ? ? 0F 2E C2 F3 0F 10 48 ? F3 0F 11 4D"), float, __fastcall,
		void* ecx, void* edx, float flInterval)
	{
		flInterval = pAnimatingInfo[ecx].second; pAnimatingInfo[ecx].second = 0.0f; pAnimatingInfo[ecx].first = I::GlobalVars->tickcount;

		return Hook.Original<FN>()(ecx, edx, flInterval);
	}

	MAKE_HOOK(C_BaseAnimating_Interpolate, g_Pattern.Find(L"client.dll", L"55 8B EC 83 EC 2C 56 8B F1 83 BE ? ? ? ? ? 0F 85 ? ? ? ? F3 0F 10 86 ? ? ? ? 57 33 FF F3 0F 11 45 ? 80 BE ? ? ? ? ? 75 13 FF B6 ? ? ? ? E8 ? ? ? ? 8B 8E ? ? ? ? 88 01 8D 45 FC 8B CE 50"), bool, __fastcall,
		void* ecx, void* edx, float currentTime)
	{
	
		return true;
	}

	MAKE_HOOK(C_BaseEntity_Interpolate, g_Pattern.Find(L"client.dll", L"55 8B EC 83 EC 28 53 57 8D 45 FC 8B F9 50 8D 45 F0 50 8D 45 E4 50 8D 45 D8 50 8D 45 08 50 E8 ? ? ? ? 83 7D FC 00 8B D8 74 46 0F B7 8F ? ? ? ? B8 ? ? ? ? 66 3B C8"), bool, __fastcall,
		void* ecx, void* edx, float currentTime)
	{
		return true;
	}

	MAKE_HOOK(C_BaseEntity_BaseInterpolatePart1, g_Pattern.Find(L"client.dll", L"55 8B EC 53 8B 5D 18 56 8B F1 C7 03"), int, __fastcall,
		void* ecx, void* edx, float& currentTime, Vec3& oldOrigin, Vec3& oldAngles, Vec3& oldVel, int& bNoMoreChanges)
	{
		return 1;
	}

	MAKE_HOOK(CInterpolatedVarArrayBase_Interpolate, g_Pattern.Find(L"client.dll", L"55 8B EC 83 EC 1C D9 45 0C 8D 45 FC 56 50 83 EC 08 C7 45 ? ? ? ? ? 8D 45 E4 8B F1 D9 5C 24 04 D9 45 08 D9 1C 24 50 E8 ? ? ? ? 84 C0 0F 84 ? ? ? ? F6 46 2C 01 53 57 74 31 83 7D FC 00 B9 ? ? ? ? D9 45 08 B8 ? ? ? ? 0F 44 C1 8B CE 50 8B 06 83 EC 08 DD 1C 24 FF 50 28 50 68 ? ? ? ? FF 15 ? ? ? ? 83 C4 14 80 7D E4 00 74 60 0F B7 5E 0E 8B 55 F0 0F B7 4E 0C 03"), int, __fastcall,
		void* ecx, void* edx, float currentTime, float interpolation_amount)
	{
		return 1;
	}

	MAKE_HOOK(CL_LatchInterpolationAmount, g_Pattern.Find(L"engine.dll", L"55 8B EC 83 EC 0C 83 3D ? ? ? ? ? 0F 8C ? ? ? ? 8B 0D ? ? ? ? 8B 01 8B 40 54 FF D0 D9 5D F8 B9 ? ? ? ? E8 ? ? ? ? D9 5D FC F3 0F 10 45"), void, __fastcall,
		void* ecx, void* edx)
	{
		return Hook.Original<FN>()(ecx, edx);
	}

	MAKE_HOOK(C_BaseEntity_InterpolateServerEntities, g_Pattern.Find(L"client.dll", L"55 8B EC 83 EC 30 8B 0D ? ? ? ? 53 33 DB 89 5D DC 89 5D E0 8B 41 08 89 5D E4 89 5D E8 85 C0 74 41 68 ? ? ? ? 68 ? ? ? ? 68 ? ? ? ? 68 ? ? ? ? 68 ? ? ? ? 68 ? ? ? ? 53 53"), void, __fastcall,
		void* ecx, void* edx)
	{
		return Hook.Original<FN>()(ecx, edx);
	}

	MAKE_HOOK(CInterpolatedVarArrayBase__Interpolate, g_Pattern.Find(L"client.dll", L"55 8B EC 8B 45 14 33 D2 56 8B F1 8B 4D 10 57 3B C8 75 28 38 56 1D 0F 86 ? ? ? ? 8B 78 08 8B 4D 08 2B F9 D9 04 0F 42 D9 19 0F B6 46 1D 83 C1 04 3B D0 7C EF 5F 5E"), void, __fastcall,
		void* ecx, void* edx, void* out, float frac, void* start, void* end)
	{
		return;
	}



#include "../../Features/Auto/AutoUber/AutoUber.h"

MAKE_HOOK(FX_FireBullets, g_Pattern.Find(L"client.dll", L"55 8B EC 81 EC ? ? ? ? 53 8B 5D ? 56 53"), void, __cdecl,
	void* pWpn, int iPlayer, const Vec3& vecOrigin, const Vec3& vecAngles, int iWeapon, int iMode, int iSeed, float flSpread, float flDamage, bool bCritical)
{
	return Hook.Original<FN>()(pWpn, iPlayer, vecOrigin, vecAngles, iWeapon, iMode, iSeed, flSpread, flDamage, bCritical);
}

void CLagComp::run(int iIndex, const Vec3 vAngles, const Vec3 vecOrigin)
{

}








