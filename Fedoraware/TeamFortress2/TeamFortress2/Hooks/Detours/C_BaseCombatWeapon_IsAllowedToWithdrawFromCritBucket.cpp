#include "../Hooks.h"
#include "../../Features/CritHack/CritHack.h"
#include "../../Features/Commands/Commands.h"

MAKE_HOOK(C_BaseCombatWeapon_IsAllowedToWithdrawFromCritBucket, g_Pattern.Find(L"client.dll", L"55 8B EC 56 8B F1 0F B7 86 ? ? ? ? FF 86 ? ? ? ? 50 E8 ? ? ? ? 83 C4 04 80 B8 ? ? ? ? ? 74 0A F3 0F 10 15"), bool, __fastcall,
	void* ecx, void* edx, float flDamage)
{
	
	return Hook.Original<FN>()(ecx, edx, flDamage);

}


MAKE_HOOK(CanFireRandomCriticalShot, g_Pattern.Find(L"client.dll", L"55 8B EC F3 0F 10 4D ? F3 0F 58 0D ? ? ? ? F3 0F 10 81 ? ? ? ? 0F 2F C1 76 ? 32 C0 5D C2 ? ? B0 ? 5D C2"), bool, __fastcall, void* ecx, void* edx, float crit_chance)
{
	const auto& weapon = g_EntityCache.GetWeapon();

	if (!weapon)
	{

		return Hook.Original<FN>()(ecx, edx, crit_chance);
	}

	

	return Hook.Original<FN>()(ecx, edx, crit_chance);
}
