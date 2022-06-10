#include "controls/modifier_hints.h"

#include <cstddef>

#include "DiabloUI/art_draw.h"
#include "DiabloUI/ui_flags.hpp"
#include "control.h"
#include "controls/controller_motion.h"
#include "controls/game_controls.h"
#include "controls/plrctrls.h"
#include "engine/load_cel.hpp"
#include "engine/render/text_render.hpp"
#include "options.h"
#include "panels/spell_icons.hpp"
#include "utils/language.h"

namespace devilution {

extern std::optional<OwnedCelSprite> pSBkIconCels;

namespace {

/** Vertical distance between text lines. */
constexpr int LineHeight = 25;

/** Horizontal margin of the hints circle from panel edge. */
constexpr int CircleMarginX = 16;

/** Distance between the panel top and the circle top. */
constexpr int CircleTop = 101;

/** Spell icon side size. */
constexpr int IconSize = 37;

/** Spell icon text right margin. */
constexpr int IconSizeTextMarginRight = 3;

/** Spell icon text top margin. */
constexpr int IconSizeTextMarginTop = 2;

constexpr int HintBoxSize = 39;
constexpr int HintBoxMargin = 5;

Art hintBox;
Art hintBoxBackground;
Art hintIcons;

enum HintIcon : uint8_t {
	IconChar,
	IconInv,
	IconQuests,
	IconSpells,
	IconMap,
	IconMenu,
	IconNull
};

struct CircleMenuHint {
	CircleMenuHint(HintIcon top, HintIcon right, HintIcon bottom, HintIcon left)
	    : top(top)
	    , right(right)
	    , bottom(bottom)
	    , left(left)
	{
	}

	HintIcon top;
	HintIcon right;
	HintIcon bottom;
	HintIcon left;
};

/**
 * @brief Draws hint text for a four button layout with the top/left edge of the bounding box at the position given by origin.
 * @param out The output buffer to draw on.
 * @param hint Struct describing the icon to draw.
 * @param origin Top left corner of the layout (relative to the output buffer).
 */
void DrawCircleMenuHint(const Surface &out, const CircleMenuHint &hint, const Point &origin)
{
	const Displacement backgroundDisplacement = { (HintBoxSize - IconSize) / 2 + 1, (HintBoxSize - IconSize) / 2 - 1 };
	Point hintBoxPositions[4] = {
		origin + Displacement { 0, LineHeight - HintBoxSize },
		origin + Displacement { HintBoxSize + HintBoxMargin, LineHeight - HintBoxSize * 2 - HintBoxMargin },
		origin + Displacement { HintBoxSize + HintBoxMargin, LineHeight + HintBoxMargin },
		origin + Displacement { HintBoxSize * 2 + HintBoxMargin * 2, LineHeight - HintBoxSize }
	};
	Point iconPositions[4] = {
		hintBoxPositions[0] + backgroundDisplacement,
		hintBoxPositions[1] + backgroundDisplacement,
		hintBoxPositions[2] + backgroundDisplacement,
		hintBoxPositions[3] + backgroundDisplacement,
	};
	uint8_t iconIndices[4] { hint.left, hint.top, hint.bottom, hint.right };

	for (int slot = 0; slot < 4; ++slot) {
		if (iconIndices[slot] == HintIcon::IconNull)
			continue;

		DrawArt(out, iconPositions[slot], &hintBoxBackground);
		DrawArt(out, iconPositions[slot], &hintIcons, iconIndices[slot], 37, 38);
		DrawArt(out, hintBoxPositions[slot], &hintBox);
	}
}

/**
 * @brief Draws hint text for a four button layout with the top/left edge of the bounding box at the position given by origin plus the icon for the spell mapped to that entry.
 * @param out The output buffer to draw on.
 * @param origin Top left corner of the layout (relative to the output buffer).
 */
void DrawSpellsCircleMenuHint(const Surface &out, const Point &origin)
{
	const auto &myPlayer = Players[MyPlayerId];
	const Displacement spellIconDisplacement = { (HintBoxSize - IconSize) / 2 + 1, HintBoxSize - (HintBoxSize - IconSize) / 2 - 1 };
	Point hintBoxPositions[4] = {
		origin + Displacement { 0, LineHeight - HintBoxSize },
		origin + Displacement { HintBoxSize + HintBoxMargin, LineHeight - HintBoxSize * 2 - HintBoxMargin },
		origin + Displacement { HintBoxSize + HintBoxMargin, LineHeight + HintBoxMargin },
		origin + Displacement { HintBoxSize * 2 + HintBoxMargin * 2, LineHeight - HintBoxSize }
	};
	Point spellIconPositions[4] = {
		hintBoxPositions[0] + spellIconDisplacement,
		hintBoxPositions[1] + spellIconDisplacement,
		hintBoxPositions[2] + spellIconDisplacement,
		hintBoxPositions[3] + spellIconDisplacement,
	};
	uint64_t spells = myPlayer._pAblSpells | myPlayer._pMemSpells | myPlayer._pScrlSpells | myPlayer._pISpells;
	spell_id splId;
	spell_type splType;

	for (int slot = 0; slot < 4; ++slot) {
		splId = myPlayer._pSplHotKey[slot];

		if (splId != SPL_INVALID && splId != SPL_NULL && (spells & GetSpellBitmask(splId)) != 0)
			splType = (currlevel == 0 && !spelldata[splId].sTownSpell) ? RSPLTYPE_INVALID : myPlayer._pSplTHotKey[slot];
		else {
			splType = RSPLTYPE_INVALID;
			splId = SPL_NULL;
		}

		SetSpellTrans(splType);
		DrawSpellCel(out, spellIconPositions[slot], *pSBkIconCels, SpellITbl[splId]);
		DrawArt(out, hintBoxPositions[slot], &hintBox);
	}
}

void DrawStartModifierMenu(const Surface &out)
{
	if (!start_modifier_active)
		return;
	static const CircleMenuHint DPad(/*top=*/HintIcon::IconMenu, /*right=*/HintIcon::IconInv, /*bottom=*/HintIcon::IconMap, /*left=*/HintIcon::IconChar);
	static const CircleMenuHint Buttons(/*top=*/HintIcon::IconNull, /*right=*/HintIcon::IconNull, /*bottom=*/HintIcon::IconSpells, /*left=*/HintIcon::IconQuests);
	DrawCircleMenuHint(out, DPad, { PANEL_LEFT + CircleMarginX, PANEL_TOP - CircleTop });
	DrawCircleMenuHint(out, Buttons, { PANEL_LEFT + PANEL_WIDTH - HintBoxSize * 3 - CircleMarginX - HintBoxMargin * 2, PANEL_TOP - CircleTop });
}

void DrawSelectModifierMenu(const Surface &out)
{
	if (!select_modifier_active || SimulatingMouseWithSelectAndDPad)
		return;

	if (sgOptions.Controller.bDpadHotkeys) {
		DrawSpellsCircleMenuHint(out, { PANEL_LEFT + CircleMarginX, PANEL_TOP - CircleTop });
	}
	DrawSpellsCircleMenuHint(out, { PANEL_LEFT + PANEL_WIDTH - HintBoxSize * 3 - CircleMarginX - HintBoxMargin * 2, PANEL_TOP - CircleTop });
}

} // namespace

void InitModifierHints()
{
	LoadMaskedArt("data\\hintbox.pcx", &hintBox, 1, 1);
	LoadMaskedArt("data\\hintboxbackground.pcx", &hintBoxBackground, 1, 1);
	LoadMaskedArt("data\\hinticons.pcx", &hintIcons, 6, 1);

	if (hintBox.surface == nullptr || hintBoxBackground.surface == nullptr) {
		app_fatal("%s", _("Failed to load UI resources.\n"
		                  "\n"
		                  "Make sure devilutionx.mpq is in the game folder and that it is up to date.")
		                    .c_str());
	}
}

void FreeModifierHints()
{
	hintBox.Unload();
	hintBoxBackground.Unload();
	hintIcons.Unload();
}

void DrawControllerModifierHints(const Surface &out)
{
	DrawStartModifierMenu(out);
	DrawSelectModifierMenu(out);
}

} // namespace devilution
