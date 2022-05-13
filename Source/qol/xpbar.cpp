/**
 * @file xpbar.cpp
 *
 * Adds XP bar QoL feature
 */
#include "xpbar.h"

#include <array>

#include <fmt/format.h>

#include "DiabloUI/art_draw.h"
#include "control.h"
#include "engine/point.hpp"
#include "options.h"
#include "utils/language.h"

namespace devilution {

namespace {

constexpr int BarWidth = 307;

using ColorGradient = std::array<Uint8, 12>;
constexpr ColorGradient GoldGradient = { 0xCF, 0xCE, 0xCD, 0xCC, 0xCB, 0xCA, 0xC9, 0xC8, 0xC7, 0xC6, 0xC5, 0xC4 };
constexpr ColorGradient SilverGradient = { 0xFE, 0xFD, 0xFC, 0xFB, 0xFA, 0xF9, 0xF8, 0xF7, 0xF6, 0xF5, 0xF4, 0xF3 };

constexpr int BackWidth = 313;
constexpr int BackHeight = 9;

Art xpbarArt;

void DrawBar(const Surface &out, Point screenPosition, int width, const ColorGradient &gradient)
{
	UnsafeDrawHorizontalLine(out, screenPosition + Displacement { 0, 1 }, width, gradient[gradient.size() * 3 / 4 - 1]);
	UnsafeDrawHorizontalLine(out, screenPosition + Displacement { 0, 2 }, width, gradient[gradient.size() - 1]);
	UnsafeDrawHorizontalLine(out, screenPosition + Displacement { 0, 3 }, width, gradient[gradient.size() / 2 - 1]);
}

void DrawEndCap(const Surface &out, Point point, int idx, const ColorGradient &gradient)
{
	out.SetPixel({ point.x, point.y + 1 }, gradient[idx * 3 / 4]);
	out.SetPixel({ point.x, point.y + 2 }, gradient[idx]);
	out.SetPixel({ point.x, point.y + 3 }, gradient[idx / 2]);
}

/**
 * @brief Prints integer with thousands separator.
 */
std::string PrintWithSeparator(int n)
{
	std::string number = fmt::format("{:d}", n);
	std::string out = "";

	int length = number.length();
	int mlength = length % 3;
	if (mlength == 0)
		mlength = 3;
	out.append(number.substr(0, mlength));
	for (int i = mlength; i < length; i += 3) {
		out.append(_(/* TRANSLATORS: Thousands separator */ ","));
		out.append(number.substr(i, 3));
	}

	return out;
}

} // namespace

void InitXPBar()
{
	if (*sgOptions.Gameplay.experienceBar) {
		LoadMaskedArt("data\\xpbar.pcx", &xpbarArt, 1, 1);

		if (xpbarArt.surface == nullptr) {
			app_fatal("%s", _("Failed to load UI resources.\n"
			                  "\n"
			                  "Make sure devilutionx.mpq is in the game folder and that it is up to date.")
			                    .c_str());
		}
	}
}

void FreeXPBar()
{
	xpbarArt.Unload();
}

void DrawXPBar(const Surface &out)
{
	if (!*sgOptions.Gameplay.experienceBar || talkflag)
		return;

	const auto &player = Players[MyPlayerId];

	const Point back = { PANEL_LEFT + PANEL_WIDTH / 2 - 155, PANEL_TOP + PANEL_HEIGHT - 11 };
	const Point position = back + Displacement { 3, 2 };

	DrawArt(out, back, &xpbarArt);

	const int8_t charLevel = player._pLevel;

	if (charLevel == MAXCHARLEVEL) {
		// Draw a nice golden bar for max level characters.
		DrawBar(out, position, BarWidth, GoldGradient);

		return;
	}

	const uint64_t prevXp = ExpLvlsTbl[charLevel - 1];
	if (player._pExperience < prevXp)
		return;

	uint64_t prevXpDelta1 = player._pExperience - prevXp;
	uint64_t prevXpDelta = ExpLvlsTbl[charLevel] - prevXp;
	uint64_t fullBar = BarWidth * prevXpDelta1 / prevXpDelta;

	// Figure out how much to fill the last pixel of the XP bar, to make it gradually appear with gained XP
	uint64_t onePx = prevXpDelta / BarWidth + 1;
	uint64_t lastFullPx = fullBar * prevXpDelta / BarWidth;

	const uint64_t fade = (prevXpDelta1 - lastFullPx) * (SilverGradient.size() - 1) / onePx;

	// Draw beginning of bar full brightness
	DrawBar(out, position, fullBar, SilverGradient);

	// End pixels appear gradually
	DrawEndCap(out, position + Displacement { static_cast<int>(fullBar), 0 }, fade, SilverGradient);
}

bool CheckXPBarInfo()
{
	if (!*sgOptions.Gameplay.experienceBar)
		return false;

	const int backX = PANEL_LEFT + PANEL_WIDTH / 2 - 155;
	const int backY = PANEL_TOP + PANEL_HEIGHT - 11;

	if (MousePosition.x < backX || MousePosition.x >= backX + BackWidth || MousePosition.y < backY || MousePosition.y >= backY + BackHeight)
		return false;

	const auto &player = Players[MyPlayerId];

	const int8_t charLevel = player._pLevel;

	AddPanelString(fmt::format(_("Level {:d}"), charLevel));

	if (charLevel == MAXCHARLEVEL) {
		// Show a maximum level indicator for max level players.
		InfoColor = UiFlags::ColorWhitegold;

		AddPanelString(fmt::format(_("Experience: {:s}"), PrintWithSeparator(ExpLvlsTbl[charLevel - 1])));
		AddPanelString(_("Maximum Level"));

		return true;
	}

	InfoColor = UiFlags::ColorWhite;

	AddPanelString(fmt::format(_("Experience: {:s}"), PrintWithSeparator(player._pExperience)));
	AddPanelString(fmt::format(_("Next Level: {:s}"), PrintWithSeparator(ExpLvlsTbl[charLevel])));
	AddPanelString(fmt::format(_("{:s} to Level {:d}"), PrintWithSeparator(ExpLvlsTbl[charLevel] - player._pExperience), charLevel + 1));

	return true;
}

} // namespace devilution
