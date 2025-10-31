#pragma once
#include "../../../SDK/SDK.h"

#pragma warning ( disable : 4091 )

typedef struct FakeMtrixes {
	float BoneMatrix[128][3][4];
};

typedef struct RealMtrixes {
	float BoneMatrixer[128][3][4];
};

class CFakeAng {
public:
	bool reset_animstate = false;
	int lastcmd_num = 0;
	int ticks_elapsed = 0;


	FakeMtrixes BoneMatrix;
	RealMtrixes BoneMatrixer;
	void Run(CUserCmd* pCmd);
	void AnimFix();
	bool DrawChams = false;
};

ADD_FEATURE(CFakeAng, FakeAng)