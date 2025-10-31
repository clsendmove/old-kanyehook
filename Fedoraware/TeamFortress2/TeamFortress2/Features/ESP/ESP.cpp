#include "ESP.h"

#include "../Vars.h"
#include "../Visuals/Visuals.h"
#include "../Menu/Playerlist/Playerlist.h"

#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string/predicate.hpp>

#include "../Backtrack/Backtrack.h"


bool CESP::ShouldRun()
{
	if (!Vars::ESP::Main::Active.Value)
	{
		return false;
	}

	return true;
}

void CESP::Run()
{
	if (const auto& pLocal = g_EntityCache.GetLocal())
	{
		if (ShouldRun())
		{
			DrawWorld();
			DrawBuildings(pLocal);
			DrawPlayers(pLocal);
		}
	}
}


std::wstring CESP::ConvertUtf8ToWide(const std::string& str)
{
	int count = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), str.length(), NULL, 0);
	std::wstring wstr(count, 0);
	MultiByteToWideChar(CP_UTF8, 0, str.c_str(), str.length(), &wstr[0], count);
	return wstr;
}

bool CESP::GetDrawBounds(CBaseEntity* pEntity, Vec3* vTrans, int& x, int& y, int& w, int& h)
{
	bool bIsPlayer = false;
	Vec3 vMins, vMaxs;

	if (pEntity->IsPlayer())
	{
		bIsPlayer = true;
		const bool bIsDucking = pEntity->IsDucking();
		vMins = I::GameMovement->GetPlayerMins(bIsDucking);
		vMaxs = I::GameMovement->GetPlayerMaxs(bIsDucking);
	}

	else
	{
		vMins = pEntity->GetCollideableMins();
		vMaxs = pEntity->GetCollideableMaxs();
	}

	const matrix3x4& transform = pEntity->GetRgflCoordinateFrame();

	const Vec3 vPoints[] =
	{
		Vec3(vMins.x, vMins.y, vMins.z),
		Vec3(vMins.x, vMaxs.y, vMins.z),
		Vec3(vMaxs.x, vMaxs.y, vMins.z),
		Vec3(vMaxs.x, vMins.y, vMins.z),
		Vec3(vMaxs.x, vMaxs.y, vMaxs.z),
		Vec3(vMins.x, vMaxs.y, vMaxs.z),
		Vec3(vMins.x, vMins.y, vMaxs.z),
		Vec3(vMaxs.x, vMins.y, vMaxs.z)
	};

	for (int n = 0; n < 8; n++)
	{
		Math::VectorTransform(vPoints[n], transform, vTrans[n]);
	}

	Vec3 flb, brt, blb, frt, frb, brb, blt, flt;

	if (Utils::W2S(vTrans[3], flb) && Utils::W2S(vTrans[5], brt)
		&& Utils::W2S(vTrans[0], blb) && Utils::W2S(vTrans[4], frt)
		&& Utils::W2S(vTrans[2], frb) && Utils::W2S(vTrans[1], brb)
		&& Utils::W2S(vTrans[6], blt) && Utils::W2S(vTrans[7], flt)
		&& Utils::W2S(vTrans[6], blt) && Utils::W2S(vTrans[7], flt))
	{
		const Vec3 arr[] = { flb, brt, blb, frt, frb, brb, blt, flt };

		float left = flb.x;
		float top = flb.y;
		float righ = flb.x;
		float bottom = flb.y;

		for (int n = 1; n < 8; n++)
		{
			if (left > arr[n].x)
			{
				left = arr[n].x;
			}

			if (top < arr[n].y)
			{
				top = arr[n].y;
			}

			if (righ < arr[n].x)
			{
				righ = arr[n].x;
			}

			if (bottom > arr[n].y)
			{
				bottom = arr[n].y;
			}
		}

		float x_ = left;
		const float y_ = bottom;
		float w_ = righ - left;
		const float h_ = top - bottom;


		x = static_cast<int>(x_);
		y = static_cast<int>(y_);
		w = static_cast<int>(w_);
		h = static_cast<int>(h_);

		return !(x > g_ScreenSize.w || x + w < 0 || y > g_ScreenSize.h || y + h < 0);
	}

	return false;
}

Vec3 CESP::Predictor_t::Extrapolate(float time)
{
	Vec3 vecOut = {};

	if (m_pEntity->IsOnGround())
		vecOut = (m_vPosition + (m_vVelocity * time));

	else vecOut = (m_vPosition + (m_vVelocity * time) - m_vAcceleration * 0.5f * time * time);

	return vecOut;
}



int Choke(CBaseEntity* Player)
{
	if (Player->GetSimulationTime() > Player->GetOldSimulationTime())
		return TIME_TO_TICKS(fabs(Player->GetSimulationTime() - Player->GetOldSimulationTime()));
	return 0;
}

void DrawBackTrack(CBaseEntity* pEntity, Color_t colourface, Color_t colouredge, float time)
{

	if (const auto& pLastTick = F::Backtrack.GetRecord(pEntity->GetIndex(), BacktrackMode::Last))
	{
		const model_t* model = pLastTick->Model;
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
			Math::ConcatTransforms(pLastTick->BoneMatrix.BoneMatrix[bbox->bone], rotMatrix, matrix);

			const Vec3 vPos = (bbox->bbmin + bbox->bbmax) * 0.5f;
			Vec3 vOut;
			const matrix3x4& bone = pLastTick->BoneMatrix.BoneMatrix[bbox->bone];
			Math::VectorTransform(vPos, bone, vOut);

			Vec3 bboxAngle;
			Math::MatrixAngles(matrix, bboxAngle);

			Math::GetMatrixOrigin(matrix, vOut);

			I::DebugOverlay->AddBoxOverlay2(vOut, bbox->bbmin, bbox->bbmax, bboxAngle, colourface, colouredge, time);

		}
	}
}


void CESP::DrawPlayers(CBaseEntity* pLocal)
{


	if (!Vars::ESP::Main::Active.Value && !I::EngineClient->IsConnected() || !I::EngineClient->IsInGame())
	{
		return;
	}

	for (const auto& Player : g_EntityCache.GetGroup(EGroupType::PLAYERS_ALL))
	{
		if (!Player->IsAlive() || Player->IsAGhost())
		{
			continue;
		}

		int nIndex = Player->GetIndex();
		bool bIsLocal = nIndex == I::EngineClient->GetLocalPlayer();

		if (!bIsLocal)
		{
			switch (Vars::ESP::Players::IgnoreCloaked.Value)
			{
			case 0: { break; }
			case 1:
			{
				if (Player->IsCloaked()) { continue; }
				break;
			}
			case 2:
			{
				if (Player->IsCloaked() && Player->GetTeamNum() != pLocal->GetTeamNum()) { continue; }
				break;
			}
			}

			switch (Vars::ESP::Players::IgnoreTeammates.Value)
			{
			case 0: break;
			case 1:
			{
				if (Player->GetTeamNum() == pLocal->GetTeamNum()) { continue; }
				break;
			}
			case 2:
			{
				if (Player->GetTeamNum() == pLocal->GetTeamNum() && !g_EntityCache.IsFriend(nIndex)) { continue; }
				break;
			}
			}
		}
		else
		{
			continue;
		}

		Color_t drawColor = Utils::GetEntityDrawColor(Player, false);

		int x = 0, y = 0, w = 0, h = 0;
		Vec3 vTrans[8];
		if (GetDrawBounds(Player, vTrans, x, y, w, h))
		{
			int nHealth = Player->GetHealth(), nMaxHealth = Player->GetMaxHealth();
			Color_t healthColor = Utils::GetHealthColor(nHealth, nMaxHealth);

			size_t FONT = FONT_ESP, FONT_NAME = FONT_ESP_NAME;

			int nTextX = x + w + 3, nTextOffset = -1, nClassNum = Player->GetClassNum();

			I::VGuiSurface->DrawSetAlphaMultiplier(1.f);

			if (nClassNum == CLASS_MEDIC)
			{
				if (const auto& pMedGun = Player->GetWeaponFromSlot(SLOT_SECONDARY))
				{
					if (pMedGun->GetUberCharge())
					{
						x += w + 1;

						float flUber = pMedGun->GetUberCharge() * (pMedGun->GetItemDefIndex() == Medic_s_TheVaccinator
							? 400.0f
							: 100.0f);

						float flMaxUber = (pMedGun->GetItemDefIndex() == Medic_s_TheVaccinator ? 400.0f : 100.0f);

						if (flUber > flMaxUber)
						{
							flUber = flMaxUber;
						}
						float flHealth = static_cast<float>(nHealth);
						float flMaxHealth = static_cast<float>(nMaxHealth);


						static constexpr int RECT_WIDTH = 2;
						int nHeight = h + (flUber < flMaxUber ? 2 : 1);
						int nHeight2 = h + 1;

						float ratio = flUber / flMaxUber;

						g_Draw.Rect((x + RECT_WIDTH), (y + nHeight - (nHeight * ratio)), RECT_WIDTH, (nHeight * ratio), Colors::UberColor);
						g_Draw.OutlinedRect(x + RECT_WIDTH - 2, y + nHeight - nHeight * ratio - 1, RECT_WIDTH + 2, nHeight * ratio + 2, Colors::OutlineESP);


						x -= w + 1;
					}
				}
			}

			PlayerInfo_t pi{};
			if (I::EngineClient->GetPlayerInfo(nIndex, &pi))
			{
				int middle = x + w / 2;

				int offset = (g_Draw.m_vecFonts[FONT_NAME].nTall + (g_Draw.m_vecFonts[FONT_NAME].nTall / 6));
				std::string attackString = std::string(pi.name); 
				g_Draw.String(FONT_NAME, middle, y - offset, drawColor, ALIGN_CENTERHORIZONTAL, Utils::ConvertUtf8ToWide(attackString).data());

			}

		
			int offset = g_Draw.m_vecFonts[FONT].nTall / 8;


			Colors::Cond = { 255,225, 255,255 };

			const int nCond = Player->GetCond();
			const int nCondEx = Player->GetCondEx();
			const int nCondEx2 = Player->GetCondEx2();
			const Color_t teamColors = Utils::GetTeamColor(Player->GetTeamNum(), Vars::ESP::Main::EnableTeamEnemyColors.Value);
			{
				const float flSimTime = Player->GetSimulationTime(), flOldSimTime = Player->GetOldSimulationTime();
				if (flSimTime != flOldSimTime) //stolen from CBacktrack::MakeRecords()
				{
					if (!F::Backtrack.Records[Player->GetIndex()].empty())
					{
						const Vec3 vPrevOrigin = F::Backtrack.Records[Player->GetIndex()].front().AbsOrigin;
						const Vec3 vDelta = Player->m_vecOrigin() - vPrevOrigin;
						if (vDelta.Length2DSqr() > 4096.f)
						{
							g_Draw.String(FONT_ESP_COND, nTextX, y + nTextOffset, { 255, 95, 95, 255 }, ALIGN_DEFAULT, "LAGCOMP");
							nTextOffset += g_Draw.m_vecFonts[FONT_ESP_COND].nTall;
						}
					}
				}

				if (Player->GetDormant())
				{
					g_Draw.String(FONT_ESP_COND, nTextX, y + nTextOffset, { 128, 128, 128, 255 }, ALIGN_DEFAULT, "DORMANT");
					nTextOffset += g_Draw.m_vecFonts[FONT_ESP_COND].nTall;
				}

				if (nCond & TFCond_Ubercharged || nCondEx & TFCondEx_UberchargedHidden || nCondEx & TFCondEx_UberchargedCanteen)
				{
					g_Draw.String(FONT_ESP_COND, nTextX, y + nTextOffset, Colors::UberColor, ALIGN_DEFAULT, "UBER");
					nTextOffset += g_Draw.m_vecFonts[FONT_ESP_COND].nTall;
				}
				else if (nCond & TFCond_Bonked)
				{
					g_Draw.String(FONT_ESP_COND, nTextX, y + nTextOffset, Colors::Cond, ALIGN_DEFAULT, "BONK");
					nTextOffset += g_Draw.m_vecFonts[FONT_ESP_COND].nTall;
				} // no need to show bonk effect if they are ubered, right?

				/* vaccinator effects */
				if (nCondEx & TFCondEx_BulletCharge)
				{
					g_Draw.String(FONT_ESP_COND, nTextX, y + nTextOffset, { 255, 0, 0, 255 }, ALIGN_DEFAULT, "BULLET (UBER)");
					nTextOffset += g_Draw.m_vecFonts[FONT_ESP_COND].nTall;
				}
				if (nCondEx & TFCondEx_ExplosiveCharge)
				{
					g_Draw.String(FONT_ESP_COND, nTextX, y + nTextOffset, { 255, 0, 0, 255 }, ALIGN_DEFAULT, "BLAST (UBER)");
					nTextOffset += g_Draw.m_vecFonts[FONT_ESP_COND].nTall;
				}
				if (nCondEx & TFCondEx_FireCharge)
				{
					g_Draw.String(FONT_ESP_COND, nTextX, y + nTextOffset, { 255, 0, 0, 255 }, ALIGN_DEFAULT, "FIRE (UBER)");
					nTextOffset += g_Draw.m_vecFonts[FONT_ESP_COND].nTall;
				}
				if (nCondEx & TFCondEx_BulletResistance)
				{
					g_Draw.String(FONT_ESP_COND, nTextX, y + nTextOffset, { 255, 255, 255, 255 }, ALIGN_DEFAULT, "BULLET (RES)");
					nTextOffset += g_Draw.m_vecFonts[FONT_ESP_COND].nTall;
				}
				if (nCondEx & TFCondEx_ExplosiveResistance)
				{
					g_Draw.String(FONT_ESP_COND, nTextX, y + nTextOffset, { 255, 255, 255, 255 }, ALIGN_DEFAULT, "BLAST(RES)");
					nTextOffset += g_Draw.m_vecFonts[FONT_ESP_COND].nTall;
				}
				if (nCondEx & TFCondEx_FireResistance)
				{
					g_Draw.String(FONT_ESP_COND, nTextX, y + nTextOffset, { 255, 255, 255, 255 }, ALIGN_DEFAULT, "FIRE (RES)");
					nTextOffset += g_Draw.m_vecFonts[FONT_ESP_COND].nTall;
				}

				if (nCond & TFCond_MegaHeal)
				{
					g_Draw.String(FONT_ESP_COND, nTextX, y + nTextOffset, { 255, 107, 108, 255 }, ALIGN_DEFAULT, "MEGAHEAL");
					nTextOffset += g_Draw.m_vecFonts[FONT_ESP_COND].nTall;
				}

				if (Player->IsCritBoosted())
				{															//light red
					g_Draw.String(FONT_ESP_COND, nTextX, y + nTextOffset, { 255, 107, 108, 255 }, ALIGN_DEFAULT, "CRITS");
					nTextOffset += g_Draw.m_vecFonts[FONT_ESP_COND].nTall;
				}

				if (nCond & TFCond_Buffed)
				{
					g_Draw.String(FONT_ESP_COND, nTextX, y + nTextOffset, {255,255,255,255}, ALIGN_DEFAULT, "BUFF BANNER");
					nTextOffset += g_Draw.m_vecFonts[FONT_ESP_COND].nTall;
				}

				if (nCond & TFCond_CritCola || nCond & TFCond_NoHealingDamageBuff)
				{
					g_Draw.String(FONT_ESP_COND, nTextX, y + nTextOffset, Colors::Cond, ALIGN_DEFAULT, "MINI-CRITS");
					nTextOffset += g_Draw.m_vecFonts[FONT_ESP_COND].nTall;
				}

				if (nCond & TFCond_DefenseBuffed)
				{
					g_Draw.String(FONT_ESP_COND, nTextX, y + nTextOffset, {255,255,255,255}, ALIGN_DEFAULT, "BATTALIONS");
					nTextOffset += g_Draw.m_vecFonts[FONT_ESP_COND].nTall;
				}

				if (nCond & TFCond_RegenBuffed)
				{
					g_Draw.String(FONT_ESP_COND, nTextX, y + nTextOffset, {255,255,255,255}, ALIGN_DEFAULT, "CONCHEROR");
					nTextOffset += g_Draw.m_vecFonts[FONT_ESP_COND].nTall;
				}

				if (nCond & TFCondEx_FocusBuff)
				{
					g_Draw.String(FONT_ESP_COND, nTextX, y + nTextOffset, Colors::Cond, ALIGN_DEFAULT, "FOCUS");
					nTextOffset += g_Draw.m_vecFonts[FONT_ESP_COND].nTall;
				}

				if (nCond & TFCond_Healing || nCond & TFCond_MegaHeal || Player->IsKingBuffed())
				{
					g_Draw.String(FONT_ESP_COND, nTextX, y + nTextOffset, { 255,255,255,255 }, ALIGN_DEFAULT, "HP++");
					nTextOffset += g_Draw.m_vecFonts[FONT_ESP_COND].nTall;
				}
				else if (Player->GetHealth() > Player->GetMaxHealth())
				{
					g_Draw.String(FONT_ESP_COND, nTextX, y + nTextOffset, {255,255,255,255}, ALIGN_DEFAULT, "HP+");
					nTextOffset += g_Draw.m_vecFonts[FONT_ESP_COND].nTall;
				}

				if (nCond & TFCond_Jarated)
				{
					g_Draw.String(FONT_ESP_COND, nTextX, y + nTextOffset, Colors::Cond, ALIGN_DEFAULT, "JARATE");
					nTextOffset += g_Draw.m_vecFonts[FONT_ESP_COND].nTall;
				}

				if (nCond & TFCond_MarkedForDeath || nCondEx & TFCondEx_MarkedForDeathSilent)
				{
					g_Draw.String(FONT_ESP_COND, nTextX, y + nTextOffset, Colors::Cond, ALIGN_DEFAULT, "MARKED");
					nTextOffset += g_Draw.m_vecFonts[FONT_ESP_COND].nTall;
				}

				if (nCond & TFCond_Milked)
				{
					g_Draw.String(FONT_ESP_COND, nTextX, y + nTextOffset, Colors::White, ALIGN_DEFAULT, "MILK");
					nTextOffset += g_Draw.m_vecFonts[FONT_ESP_COND].nTall;
				}

				
				if (nCond & TFCond_Taunting)
				{
					g_Draw.String(FONT_ESP_COND, nTextX, y + nTextOffset, { 255, 255, 255, 255 }, ALIGN_DEFAULT, "TAUNTING");
					nTextOffset += g_Draw.m_vecFonts[FONT_ESP_COND].nTall;
				}
				
				if (Player->GetFeignDeathReady())
				{
					g_Draw.String(FONT_ESP_COND, nTextX, y + nTextOffset, Colors::Cond, ALIGN_DEFAULT, "DR");
					nTextOffset += g_Draw.m_vecFonts[FONT_ESP_COND].nTall;
				}

				if (nCond & TFCond_Slowed)
				{
					if (const auto& pWeapon = Player->GetActiveWeapon())
					{
						if (pWeapon->GetWeaponID() == TF_WEAPON_MINIGUN)
						{															//gray
							g_Draw.String(FONT_ESP_COND, nTextX, y + nTextOffset, { 255, 255, 255, 255 }, ALIGN_DEFAULT, "REV");
							nTextOffset += g_Draw.m_vecFonts[FONT_ESP_COND].nTall;
						}

						if (pWeapon->GetWeaponID() == TF_WEAPON_COMPOUND_BOW || pWeapon->GetWeaponID() == TF_WEAPON_PIPEBOMBLAUNCHER)
						{
							g_Draw.String(FONT_ESP_COND, nTextX, y + nTextOffset, Colors::Cond, ALIGN_DEFAULT, L"CHARGING", pWeapon->GetChargeBeginTime());
							nTextOffset += g_Draw.m_vecFonts[FONT_ESP_COND].nTall; //just put this here since it should draw something regardless
						}

						if (pWeapon->GetWeaponID() == TF_WEAPON_PARTICLE_CANNON)
						{
							g_Draw.String(FONT_ESP_COND, nTextX, y + nTextOffset, Colors::Cond, ALIGN_DEFAULT, "CHARGING");
							nTextOffset += g_Draw.m_vecFonts[FONT_ESP_COND].nTall;
						}
					}
				}

				if (g_EntityCache.IsFriend(nIndex))
				{
					g_Draw.String(FONT_ESP_COND, nTextX, y + nTextOffset, { 127,255,0, 255}, ALIGN_DEFAULT, "FRIEND");
					nTextOffset += g_Draw.m_vecFonts[FONT_ESP_COND].nTall;
				}

				if (G::PlayerPriority[pi.friendsID].Mode == 4)
				{
					g_Draw.String(FONT_ESP_COND, nTextX, y + nTextOffset, { 255, 0,0, 255 }, ALIGN_DEFAULT, "CHEATER");
					nTextOffset += g_Draw.m_vecFonts[FONT_ESP_COND].nTall;
				}

				if (nCond & TFCond_Zoomed)
				{															//aqua
					g_Draw.String(FONT_ESP_COND, nTextX, y + nTextOffset, Colors::Cond, ALIGN_DEFAULT, "ZOOMED");
					nTextOffset += g_Draw.m_vecFonts[FONT_ESP_COND].nTall;
				}

				if (g_EntityCache.GetWeapon())
				{
					if (nCond & TFCond_Cloaked || nCond & TFCond_CloakFlicker || nCondEx2 & TFCondEx2_Stealthed || nCondEx2 & TFCondEx2_StealthedUserBuffFade)
					{
						g_Draw.String(FONT_ESP_COND, nTextX, y + nTextOffset, Colors::Cloak, ALIGN_DEFAULT, L"INVIS %.0f%%", g_EntityCache.GetWeapon()->GetInvisPercentage());
						nTextOffset += g_Draw.m_vecFonts[FONT_ESP_COND].nTall;
					}
				}

				if (nCond & TFCond_Disguising || nCondEx & TFCondEx_DisguisedRemoved || nCond & TFCond_Disguised)
				{															//gray
					g_Draw.String(FONT_ESP_COND, nTextX, y + nTextOffset, Colors::Cond, ALIGN_DEFAULT, "DISGUISED");
					nTextOffset += g_Draw.m_vecFonts[FONT_ESP_COND].nTall;
				}


			}

			x -= 1;

			if (Player->GetVelocity().Length() > 0)
			{
				DrawBackTrack(Player, { 0,0,0,0 }, { 255, 255, 255, 70 }, 0.02);
			}

			auto flHealth = static_cast<float>(nHealth);
			auto flMaxHealth = static_cast<float>(nMaxHealth);

			Gradient_t clr = flHealth > flMaxHealth ? Colors::GradientOverhealBar : Colors::GradientHealthBar;

			Color_t HealthColor = flHealth > flMaxHealth ? Colors::Health : Utils::GetHealthColor(nHealth, nMaxHealth);

			if (!Player->IsVulnerable())
			{
				clr = { Colors::Invuln, Colors::Invuln };
			}

			if (flHealth > flMaxHealth)
			{
				flHealth = flMaxHealth;
			}
			bool d = true;
			float ratio = flHealth / flMaxHealth;

			float SPEED_FREQ = 160 / 0.6;

			int player_hp = flHealth;
			int player_hp_max = flMaxHealth;

			static float prev_player_hp[75];

			if (prev_player_hp[Player->GetIndex()] > player_hp)
				prev_player_hp[Player->GetIndex()] -= SPEED_FREQ * I::GlobalVars->frametime;
			else
				prev_player_hp[Player->GetIndex()] = player_hp;

			g_Draw.RectOverlay(x - 2 - 3, y + h, 3, h, prev_player_hp[Player->GetIndex()] / player_hp_max, HealthColor, Colors::OutlineESP, false);

			x += 1;

			if (nHealth > nMaxHealth)
			{
				g_Draw.String(FONT, x - 6, (y + h) - (ratio * h) - 2, Colors::White, ALIGN_REVERSE, "+%d", nHealth - nMaxHealth);
			}
			else
			{
				g_Draw.String(FONT, x - 6, (y + h) - (ratio * h) - 2, Colors::White, ALIGN_REVERSE, "%d", nHealth);
			}


			I::VGuiSurface->DrawSetAlphaMultiplier(1.0f);
		}
	}
}

void CESP::DrawBuildings(CBaseEntity* pLocal) const
{
	if (!Vars::ESP::Buildings::Active.Value || !Vars::ESP::Main::Active.Value)
	{
		return;
	}

	for (const auto& pBuilding : g_EntityCache.GetGroup(EGroupType::BUILDINGS_ALL))
	{
		if (!pBuilding->IsAlive())
		{
			continue;
		}

		size_t FONT = FONT_ESP, FONT_NAME = FONT_ESP_NAME;

		const auto& building = reinterpret_cast<CBaseObject*>(pBuilding);

		Color_t drawColor = Utils::GetTeamColor(building->GetTeamNum(), Vars::ESP::Main::EnableTeamEnemyColors.Value);

		int x = 0, y = 0, w = 0, h = 0;
		Vec3 vTrans[8];
		if (GetDrawBounds(building, vTrans, x, y, w, h))
		{
			const auto nHealth = building->GetHealth();
			const auto nMaxHealth = building->GetMaxHealth();
			auto nTextOffset = 0, nTextTopOffset = 0;
			const auto nTextX = x + w + 3;

			Color_t healthColor = Utils::GetHealthColor(nHealth, nMaxHealth);

			const auto nType = static_cast<EBuildingType>(building->GetType());

			const bool bIsMini = building->GetMiniBuilding();

			// Name ESP
			if (Vars::ESP::Buildings::Name.Value)
			{
				const wchar_t* szName;

				switch (nType)
				{
				case EBuildingType::SENTRY:
				{
					if (bIsMini)
					{
						szName = L"Mini Sentry";
					}
					else
					{
						szName = L"Sentry";
					}
					break;
				}
				case EBuildingType::DISPENSER:
				{
					szName = L"Dispenser";
					break;
				}
				case EBuildingType::TELEPORTER:
				{
					if (building->GetObjectMode())
					{
						szName = L"Teleporter Out";
					}
					else
					{
						szName = L"Teleporter In";
					}
					break;
				}
				default:
				{
					szName = L"Unknown";
					break;
				}
				}

				const char level = building->GetLevel();


				nTextTopOffset += g_Draw.m_vecFonts[FONT_NAME].nTall + g_Draw.m_vecFonts[FONT_NAME].nTall / 4;
				g_Draw.String(FONT_NAME, x + w / 2, y - nTextTopOffset, drawColor, ALIGN_CENTERHORIZONTAL, szName);
			}

			// Building conditions
			if (Vars::ESP::Buildings::Cond.Value)
			{
				std::vector<std::wstring> condStrings{};

				if (nType == EBuildingType::SENTRY && building->GetControlled())
				{
					condStrings.emplace_back(L"Wrangled");
				}

				if (building->IsSentrygun() && !building->GetConstructing())
				{
					int iShells;
					int iMaxShells;
					int iRockets;
					int iMaxRockets;

					building->GetAmmoCount(iShells, iMaxShells, iRockets, iMaxRockets);

					if (iShells == 0)
						condStrings.emplace_back(L"No Ammo");

					if (!bIsMini && iRockets == 0)
						condStrings.emplace_back(L"No Rockets");
				}

				if (!condStrings.empty())
				{
					for (auto& condString : condStrings)
					{
						g_Draw.String(FONT_NAME, nTextX, y + nTextOffset, { 255,255,255,255 }, ALIGN_DEFAULT, condString.data());
						nTextOffset += g_Draw.m_vecFonts[FONT_NAME].nTall;
					}
				}
			}


			x -= 1;

			auto flHealth = static_cast<float>(nHealth);
			const auto flMaxHealth = static_cast<float>(nMaxHealth);

			if (flHealth > flMaxHealth)
			{
				flHealth = flMaxHealth;
			}

			static constexpr int RECT_WIDTH = 2;
			const int nHeight = h + (flHealth < flMaxHealth ? 2 : 1);
			int nHeight2 = h + 1;

			const float ratio = flHealth / flMaxHealth;

			g_Draw.Rect(x - RECT_WIDTH - 2, y + nHeight - nHeight * ratio, RECT_WIDTH, nHeight * ratio,
				healthColor);


			g_Draw.OutlinedRect(x - RECT_WIDTH - 2 - 1, y + nHeight - nHeight * ratio - 1, RECT_WIDTH + 2, nHeight * ratio + 2, Colors::OutlineESP);


			x += 1;

		}
		I::VGuiSurface->DrawSetAlphaMultiplier(1.0f);
	}
}

void CESP::DrawWorld() const
{
	if (!Vars::ESP::World::Active.Value || !Vars::ESP::Main::Active.Value)
	{
		return;
	}

	Vec3 vScreen = {};
	constexpr size_t FONT = FONT_ESP_PICKUPS;

	I::VGuiSurface->DrawSetAlphaMultiplier(Vars::ESP::World::Alpha.Value);

	
		for (const auto& health : g_EntityCache.GetGroup(EGroupType::WORLD_HEALTH))
		{
			int x = 0, y = 0, w = 0, h = 0;
			Vec3 vTrans[8];
			if (GetDrawBounds(health, vTrans, x, y, w, h))
			{
				if (Utils::W2S(health->GetVecOrigin(), vScreen))
				{
					g_Draw.String(FONT, vScreen.x, y + h, Colors::Health, ALIGN_CENTER, _(L"HEALTH"));
				}
			} // obviously a health pack isn't going to be upside down, this just looks nicer.
		}
	

	
		for (const auto& ammo : g_EntityCache.GetGroup(EGroupType::WORLD_AMMO))
		{
			int x = 0, y = 0, w = 0, h = 0;
			Vec3 vTrans[8];
			if (GetDrawBounds(ammo, vTrans, x, y, w, h))
			{
				if (Utils::W2S(ammo->GetVecOrigin(), vScreen))
				{
					g_Draw.String(FONT, vScreen.x, y + h, Colors::Ammo, ALIGN_CENTER, _(L"AMMO"));
				}
			}
		}
	

	I::VGuiSurface->DrawSetAlphaMultiplier(1.0f);
}



const wchar_t* CESP::GetPlayerClass(int nClassNum)
{
	static const wchar_t* szClasses[] = {
		L"unknown", L"Scout", L"Sniper", L"Soldier", L"Demoman",
		L"Medic", L"Heavy", L"Pyro", L"Spy", L"Engineer"
	};

	return nClassNum < 10 && nClassNum > 0 ? szClasses[nClassNum] : szClasses[0];
}

void CESP::CreateDLight(int nIndex, Color_t DrawColor, const Vec3& vOrigin, float flRadius)
{
	
}

//Got this from dude719, who got it from somewhere else
void CESP::Draw3DBox(const Vec3* vPoints, Color_t clr)
{
	
}

void CESP::DrawBones(CBaseEntity* pPlayer, const std::vector<int>& vecBones, Color_t clr)
{
	const size_t nMax = vecBones.size(), nLast = nMax - 1;
	for (size_t n = 0; n < nMax; n++)
	{
		if (n == nLast)
		{
			continue;
		}

		const auto vBone = pPlayer->GetHitboxPos(vecBones[n]);
		const auto vParent = pPlayer->GetHitboxPos(vecBones[n + 1]);

		Vec3 vScreenBone, vScreenParent;

		if (Utils::W2S(vBone, vScreenBone) && Utils::W2S(vParent, vScreenParent))
		{
			g_Draw.Line(vScreenBone.x, vScreenBone.y, vScreenParent.x, vScreenParent.y, clr);
		}
	}
}
