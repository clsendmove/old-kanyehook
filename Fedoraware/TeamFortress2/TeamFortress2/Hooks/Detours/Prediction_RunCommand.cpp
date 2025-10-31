#include "../Hooks.h"

MAKE_HOOK(Prediction_RunCommand, Utils::GetVFuncPtr(I::Prediction, 17), void, __fastcall,
	void* ecx, void* edx, CBaseEntity* pEntity, CUserCmd* pCmd, CMoveHelper* pMoveHelper)
{
	
	return Hook.Original<FN>()(ecx, edx, pEntity, pCmd, pMoveHelper);
	
}