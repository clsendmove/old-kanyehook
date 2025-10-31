#pragma once
#include "../../SDK/SDK.h"


struct angle_data {
	float angle;
	float thickness;
	angle_data(const float angle, const float thickness) : angle(angle), thickness(thickness) {}
};

class CAntiAim
{
private:
	
	void  GetEdge(CUserCmd* pCmd);
	float GetFake(CUserCmd* pCmd,int nIndex, bool* pSendPacket);
	float GetReal(CUserCmd* pCmd, int nIndex, bool* pSendPacket);


    float GetAnglePairPitch(int nIndex,CUserCmd* pCmd);	
	float CalculateCustomRealPitch(float WishPitch, bool FakeDown);
	bool bPacketFlip = true;
	bool bInvert = false;

public:
	void FixMovement(CUserCmd* pCmd, const Vec3& vOldAngles);
	void Run(CUserCmd* pCmd, bool* pSendPacket);

	

};

ADD_FEATURE(CAntiAim, AntiAim)