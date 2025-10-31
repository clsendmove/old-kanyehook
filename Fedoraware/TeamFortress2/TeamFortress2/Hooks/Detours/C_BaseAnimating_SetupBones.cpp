#include "../Hooks.h"
#include "../HookManager.h"
#include "CInventoryManager_ShowItemsPickedUp.cpp"



MAKE_HOOK(C_BaseAnimating_SetupBones, g_Pattern.Find(L"client.dll", L"55 8B EC 81 EC ? ? ? ? 53 56 8B 35 ? ? ? ? 8B D9 33 C9 33 D2 89 4D EC 89 55 F0 8B 46 08 85 C0 74 3B"), bool, __fastcall, void* ecx, void* edx, matrix3x4* pBoneToWorldOut, int nMaxBones, int boneMask, float currentTime)
{
	const auto& pLocal = g_EntityCache.GetLocal();

	if (ecx == pLocal) {
		pLocal->m_effects() |= EF_NOINTERP;
		Hook.Original<FN>()(ecx, edx, pBoneToWorldOut, nMaxBones, boneMask, currentTime);
		pLocal->m_effects() &= ~EF_NOINTERP;
		return true;
	}

	return Hook.Original<FN>()(ecx, edx, pBoneToWorldOut, nMaxBones, boneMask, currentTime); 
}

MAKE_HOOK(build_transformations, g_Pattern.Find(L"client.dll", L"55 8B EC 81 EC ? ? ? ? 53 8B D9 8B 0D ? ? ? ? 56 33 F6 89 75 ? 89 75 ? 8B 41 ? 89 75 ? 89 75 ? 57 85 C0"), void, __fastcall, void* ecx, void* edx, void* hdr, Vector* pos, void* q, matrix3x4* camera_transform, int bone_mask, void* bone_computed)
{
	static auto addr1 = g_Pattern.Find(L"client.dll", L"8B 46 ? 83 C4 ? F6 04 B8 ? 0F 84");
	static auto addr2 = g_Pattern.Find(L"client.dll", L"8B 46 ? 8B 14 B8 83 FA ? 75 ? 8B 83 ? ? ? ? 03 45 ? 50 8D 85");
	if (reinterpret_cast<uintptr_t>(_ReturnAddress()) == addr1)
		*reinterpret_cast<uintptr_t*>(_AddressOfReturnAddress()) = addr2;


	const auto& pLocal = g_EntityCache.GetLocal();
	if (ecx == pLocal) {
		return Hook.Original<FN>()(ecx, edx, hdr, pos, q, camera_transform, bone_mask, bone_computed);
	}

	Hook.Original<FN>()(ecx, edx, hdr, pos, q, camera_transform, bone_mask, bone_computed);

	return Hook.Original<FN>()(ecx, edx, hdr, pos, q, camera_transform, bone_mask, bone_computed);
}

MAKE_HOOK(C_BaseAnimating_EnableAbsRecomputations, g_Pattern.Find(L"client.dll", L"55 8B EC FF 15 ? ? ? ? 0F B6 15 ? ? ? ? 84 C0 0F B6 4D 08 0F 45 D1 88 15 ? ? ? ? 5D C3"), void, __cdecl,
	bool bEnable)
{

	return Hook.Original<FN>()(true);
}

MAKE_HOOK(C_BaseAnimating_SetAbsQueriesValid, g_Pattern.Find(L"client.dll", L"55 8B EC FF 15 ? ? ? ? 84 C0 74 0B 80 7D 08 00 0F 95 05 ? ? ? ? 5D C3"), void, __cdecl,
	bool bValid)
{
	return Hook.Original<FN>()(true);
}