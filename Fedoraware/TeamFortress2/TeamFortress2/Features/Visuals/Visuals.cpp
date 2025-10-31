#include "Visuals.h"
#include "../Vars.h"
#include "../AntiHack/FakeLag/FakeLag.h"




void CVisuals::DrawHitboxMatrix(CBaseEntity* pEntity, Color_t colourface, Color_t colouredge, float time)
{
	
	const model_t* model = pEntity->GetModel();
	const studiohdr_t* hdr = I::ModelInfoClient->GetStudioModel(model);
	const mstudiohitboxset_t* set = hdr->GetHitboxSet(pEntity->GetHitboxSet());

	for (int i{}; i < set->numhitboxes; ++i)
	{
		const mstudiobbox_t* bbox = set->hitbox(i);
		if (!bbox)
		{
			continue;
		}

		/*if (bbox->m_radius <= 0.f) {*/
		matrix3x4 rotMatrix;
		Math::AngleMatrix(bbox->angle, rotMatrix);

		matrix3x4 matrix;
		matrix3x4 boneees[128];
		pEntity->SetupBones(boneees, 128, BONE_USED_BY_ANYTHING, 0);
		Math::ConcatTransforms(boneees[bbox->bone], rotMatrix, matrix);

		Vec3 bboxAngle;
		Math::MatrixAngles(matrix, bboxAngle);

		Vec3 matrixOrigin;
		Math::GetMatrixOrigin(matrix, matrixOrigin);

		I::DebugOverlay->AddBoxOverlay2(matrixOrigin, bbox->bbmin, bbox->bbmax, bboxAngle, colourface, colouredge, time);
	}
}

bool CVisuals::RemoveScope(int nPanel)
{
	if (!Vars::Visuals::RemoveScope.Value) { return false; }

	if (!m_nHudZoom && Hash::IsHudScope(I::VGuiPanel->GetName(nPanel)))
	{
		m_nHudZoom = nPanel;
	}

	return (nPanel == m_nHudZoom);
}

void CVisuals::FOV(CViewSetup* pView)
{
	CBaseEntity* pLocal = g_EntityCache.GetLocal();

	if (pLocal && pView)
	{
		if (pLocal->GetObserverMode() == OBS_MODE_FIRSTPERSON || (pLocal->IsScoped() && !Vars::Visuals::RemoveZoom.Value))
		{
			return;
		}

		pView->fov = static_cast<float>(Vars::Visuals::FieldOfView.Value);

		if (pLocal->IsAlive())
		{
			pLocal->SetFov(Vars::Visuals::FieldOfView.Value);
			pLocal->m_iDefaultFOV() = Vars::Visuals::FieldOfView.Value;
		}
	}
}

void CVisuals::ThirdPerson(CViewSetup* pView)
{
	if (const auto& pLocal = g_EntityCache.GetLocal())
	{

		// Toggle key
		if (Vars::Visuals::ThirdPersonKey.Value)
		{
			if (!I::EngineVGui->IsGameUIVisible() && !I::VGuiSurface->IsCursorVisible())
			{
				static KeyHelper tpKey{ &Vars::Visuals::ThirdPersonKey.Value };
				if (tpKey.Pressed())
				{
					Vars::Visuals::ThirdPerson.Value = !Vars::Visuals::ThirdPerson.Value;
				}
			}
		}

		const bool bIsInThirdPerson = I::Input->CAM_IsThirdPerson();

		if (!Vars::Visuals::ThirdPerson.Value
			|| ((!Vars::Visuals::RemoveScope.Value || !Vars::Visuals::RemoveZoom.Value) && pLocal->IsScoped()))
		{
			if (bIsInThirdPerson)
			{
				pLocal->ForceTauntCam(0);
			}

			return;
		}

		if (!bIsInThirdPerson)
		{
			pLocal->ForceTauntCam(1);
		}

		// Thirdperson angles
		if (bIsInThirdPerson)
		{
			I::Prediction->SetLocalViewAngles(G::RealViewAngles);
		}

	}
}

void CVisuals::DrawTickbaseInfo(CBaseEntity* pLocal)
{
	//Tickbase info
	if (Vars::Misc::CL_Move::Enabled.Value)
	{
		const auto& pLocal = g_EntityCache.GetLocal();
		const auto& pWeapon = g_EntityCache.GetWeapon();

		if (pWeapon)
		{
			if (pLocal->GetLifeState() == LIFE_ALIVE)
			{
				const int nY = (g_ScreenSize.h / 2) + 20;

				static Color_t color1, color2;

				if (G::WaitForShift)
				{
					color1 = Colors::DTBarIndicatorsCharging.startColour;
					color2 = Colors::DTBarIndicatorsCharging.endColour;
				}
				else
				{
					color1 = Colors::DTBarIndicatorsCharged.startColour;
					color2 = Colors::DTBarIndicatorsCharged.endColour;
				}

				switch (Vars::Misc::CL_Move::DTBarStyle.Value)
				{
				case 0:
					return;
				case 1:
				{
					const float ratioCurrent = std::clamp((static_cast<float>(G::ShiftedTicks) / static_cast<float>(24)), 0.0f, 1.0f);
					const DragBox_t DTBox = Vars::Visuals::DTIndicator;
					const int drawX = DTBox.x;
					const auto fontHeight = Vars::Fonts::FONT_INDICATORS::nTall.Value;
					const int drawY = g_ScreenSize.h / static_cast<double>(2) + (g_ScreenSize.h * 0.25);
					const std::wstring bucketstr = L"Ticks " + std::to_wstring(static_cast<int>(G::ShiftedTicks)) + L"/" + std::to_wstring(static_cast<int>(24));
					g_Draw.RoundedBoxStatic(DTBox.x, DTBox.y, 100, 12, 4, { 0,0,0, 200 });
					const int chargeWidth = Math::RemapValClamped(G::ShiftedTicks, 0, 24, 0, 98.5 );

					g_Draw.String(FONT_INDICATORS, DTBox.x + 50, DTBox.y - fontHeight, { 255, 255, 255, 255 }, ALIGN_CENTERHORIZONTAL, bucketstr.c_str());

					if (G::WaitForShift)
						g_Draw.String(FONT_INDICATORS, DTBox.x + 50, DTBox.y + fontHeight, { 255, 255, 255, 255 }, ALIGN_CENTERHORIZONTAL, "Not Ready");

					if (G::ShiftedTicks && chargeWidth > 0)
						g_Draw.RoundedBoxStatic(DTBox.x + 2, DTBox.y + 2, chargeWidth, 8, 4, Vars::Menu::Colors::MenuAccent);
					
					break;
				}
				}
	
			}
		}
	}
}

inline bool vis_pos(const Vector& from, const Vector& to)
{
	CGameTrace game_trace = {};
	CTraceFilterHitscan filter = {};
	Utils::Trace(from, to, (MASK_SHOT | CONTENTS_GRATE), &filter, &game_trace); //MASK_SOLID
	return game_trace.flFraction > 0.99f;
}

void CVisuals::DrawMovesimLine()
{
	if (Vars::Visuals::MoveSimLine.Value)
	{
		if (!G::PredLinesBackup.empty())
		{
			for (size_t i = 1; i < G::PredLinesBackup.size(); i++)
			{
				RenderLine(G::PredLinesBackup.at(i - 1), G::PredLinesBackup.at(i), Vars::Menu::Colors::MenuAccent, false);
			}
		}

		if (!G::projectile_lines.empty())
		{
			for (size_t i = 1; i < G::projectile_lines.size(); i++)
			{
				if (!vis_pos(G::projectile_lines.at(i - 1), G::projectile_lines.at(i)))
				{
					break;
				}
				RenderLine(G::projectile_lines.at(i - 1), G::projectile_lines.at(i), Vars::Menu::Colors::MenuAccent, false);
			}
		}
	}
}

void CVisuals::RenderLine(const Vector& v1, const Vector& v2, Color_t c, bool bZBuffer)
{
	static auto RenderLineFn = reinterpret_cast<void(__cdecl*)(const Vector&, const Vector&, Color_t, bool)>(g_Pattern.Find(L"engine.dll", L"55 8B EC 81 EC ? ? ? ? 56 E8 ? ? ? ? 8B 0D ? ? ? ? 8B 01 FF 90 ? ? ? ? 8B F0 85 F6"));
	RenderLineFn(v1, v2, c, bZBuffer);
}

void CVisuals::DrawAimbotFOV(CBaseEntity* pLocal)
{
	//Current Active Aimbot FOV
	if (Vars::Visuals::AimFOVAlpha.Value && Vars::Aimbot::Global::AimFOV.Value)
	{
		const float flFOV = static_cast<float>(Vars::Visuals::FieldOfView.Value);
		const float flR = tanf(DEG2RAD(Vars::Aimbot::Global::AimFOV.Value) / 2.0f) / tanf(DEG2RAD((pLocal->IsScoped() && !Vars::Visuals::RemoveZoom.Value) ? 30.0f : flFOV) / 2.0f) * g_ScreenSize.w;
		const Color_t clr = Colors::FOVCircle;
		g_Draw.OutlinedCircle(g_ScreenSize.w / 2, g_ScreenSize.h / 2, flR, 70, clr);
	}

	if (!pLocal->IsAlive() || pLocal->IsAGhost()) { return; }

	CTFPlayerResource* cResource = g_EntityCache.GetPR();
	if (!cResource) { return; }

	const INetChannel* iNetChan = I::EngineClient->GetNetChannelInfo();
	if (!iNetChan) { return; }

	bool FakeLatency = Vars::Backtrack::FakeLatency.Value && Vars::Backtrack::Latency.Value > 200;

	const float flLatencyReal = (iNetChan->GetLatency(FLOW_INCOMING) + iNetChan->GetLatency(FLOW_OUTGOING)) * 1000;
	const int flLatencyScoreBoard = cResource->GetPing(pLocal->GetIndex());

	const int x = Vars::Visuals::OnScreenPing.x;
	const int y = Vars::Visuals::OnScreenPing.y;
	const int h = Vars::Visuals::OnScreenPing.h;
	bool nigger = flLatencyReal > 100;
	float offset = nigger ? 70 : 55;

	const int nTextOffset = g_Draw.m_vecFonts[FONT_MENU].nTall;
	{
		g_Draw.String(FONT_MENU, x, y, { 255, 255, 255, 255 }, ALIGN_DEFAULT, "Real %.0f ms", FakeLatency ? flLatencyReal - Vars::Backtrack::Latency.Value : flLatencyReal);

		if (FakeLatency)
		{
			g_Draw.String(FONT_MENU, x + offset, y, { 255, 255, 255, 255 }, ALIGN_DEFAULT, "(+%.0f) ms", Vars::Backtrack::Latency.Value);
		}

		g_Draw.String(FONT_MENU, x, y - nTextOffset, { 255, 255, 255, 255 }, ALIGN_DEFAULT, "Scoreboard %d ms", flLatencyScoreBoard);
	}
}
