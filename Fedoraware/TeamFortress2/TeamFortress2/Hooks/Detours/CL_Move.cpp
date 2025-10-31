#include "../Hooks.h"
#include "../../Features/Aimbot/AimbotGlobal/AimbotGlobal.h"
#include "../../Features/Misc/Misc.h"





MAKE_HOOK(CL_Move, g_Pattern.Find(L"engine.dll", L"55 8B EC 83 EC 38 83 3D ? ? ? ? ? 0F 8C ? ? ? ? E8 ? ? ? ? 84 C0 0F 84 ? ? ? ? 8B 0D ? ? ? ? 56 33 F6 57 33 FF 89 75 EC"), void, __cdecl,
	float accumulated_extra_samples, bool bFinalTick)
{
	static auto oClMove = Hook.Original<FN>();
	const auto pLocal = g_EntityCache.GetLocal();

	static KeyHelper tpKey{ &Vars::Misc::CL_Move::TeleportKey.Value };
	static KeyHelper rechargeKey{ &Vars::Misc::CL_Move::RechargeKey.Value };

	if (!Vars::Misc::CL_Move::Enabled.Value)
	{
		G::ShiftedTicks = 0;
		return oClMove(accumulated_extra_samples, bFinalTick);
	}

	

	static bool streaming = false;
	if ((tpKey.Down() || streaming) && G::ShiftedTicks > 0 && !G::Recharging && !G::RechargeQueued)
	{
		oClMove(accumulated_extra_samples, bFinalTick);

		int wishSpeed = Vars::Misc::CL_Move::DTTicks.Value;
		int speed = 0;
		while (speed < wishSpeed && G::ShiftedTicks)
		{
			oClMove(accumulated_extra_samples, 0);
			G::ShiftedTicks--;
			speed++;
		}
		streaming = G::ShiftedTicks;

		if (Vars::Misc::CL_Move::DTTicks.Value > G::ShiftedTicks)
		{
			if (G::ShiftedTicks)
			{
				oClMove(0, G::ShiftedTicks == 1);
				G::ShiftedTicks--;
			}
		}
		return;
	}

	if (G::RechargeQueued)
	{
		G::RechargeQueued = false; // see relevant code @clientmodehook
		G::Recharging = true;
		G::TickShiftQueue = 0;
	}
	else if (G::Recharging && (G::ShiftedTicks < 24))
	{
		G::ForceSendPacket = true; // force uninterrupted connection with server
		G::ShiftedTicks++; // add ticks to tick counter
		return; // this recharges
	}
	else if (rechargeKey.Down() && !G::RechargeQueued && (G::ShiftedTicks < 24))
	{
		// queue recharge
		G::ForceSendPacket = true;
		G::RechargeQueued = true;
	}
	else
	{
		G::Recharging = false;
	}

	oClMove(accumulated_extra_samples, bFinalTick);

	if (!pLocal)
	{
		G::ShiftedTicks = 0; // we do not have charge if we do not exist
		return;
	}

	if (G::WaitForShift)
	{
		G::WaitForShift--;
		return;
	}

	if (G::IsAttacking)
	{
		G::ShouldShift = true;
	}

	if (Vars::Misc::CL_Move::DTs.Value > G::ShiftedTicks)
	{
		G::ShouldShift = false; //we don't want to waste ticks, exmp: (2/24)
	}

	if (G::ShouldShift && !G::WaitForShift)
	{
		int wishSpeed = Vars::Misc::CL_Move::DTs.Value;
		int speed = 0;
		while (speed < wishSpeed && G::ShiftedTicks)
		{
			G::DT = true;
			oClMove(0, true);
			G::DT = false;
			G::ShiftedTicks--;
			speed++;
		}
		G::ShouldShift = false;
	}
}