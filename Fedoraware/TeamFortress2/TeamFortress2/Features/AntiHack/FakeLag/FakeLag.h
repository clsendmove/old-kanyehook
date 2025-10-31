#pragma once
#include "../../../SDK/SDK.h"

class CFakeLag {

	// Update this enum if you're adding/removing modes!
	enum FakelagModes {
		FL_Plain, FL_Velocity
	};



public:
	void isAttacker(CGameEvent* pEvent, const FNV1A_t uNameHash);
	bool IsVisible(CBaseEntity* pLocal);
	bool IsAllowed(CBaseEntity* pLocal);
	void OnTick(CUserCmd* pCmd, bool* pSendPacket);
	int ChokeCounter = 0; // How many ticks have been choked
	int ChosenAmount = 0; // How many ticks should be choked
};

ADD_FEATURE(CFakeLag, FakeLag)
