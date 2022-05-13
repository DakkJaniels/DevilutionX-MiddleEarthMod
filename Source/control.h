/**
 * @file control.h
 *
 * Interface of the character and main control panels
 */
#pragma once

#include <cstddef>
#include <cstdint>

#include "DiabloUI/ui_flags.hpp"
#include "engine.h"
#include "engine/point.hpp"
#include "engine/rectangle.hpp"
#include "engine/render/text_render.hpp"
#include "panels/ui_panels.hpp"
#include "spelldat.h"
#include "spells.h"
#include "utils/attributes.h"
#include "utils/stdcompat/optional.hpp"
#include "utils/stdcompat/string_view.hpp"
#include "utils/ui_fwd.h"

namespace devilution {

#define PANEL_WIDTH 640
#define PANEL_HEIGHT 128
#define PANEL_TOP (gnScreenHeight - PANEL_HEIGHT)
#define PANEL_LEFT (gnScreenWidth - PANEL_WIDTH) / 2
#define PANEL_X PANEL_LEFT
#define PANEL_Y PANEL_TOP

#define SPANEL_WIDTH 320
#define SPANEL_HEIGHT 352

extern bool drawhpflag;
extern bool dropGoldFlag;
extern bool chrbtn[4];
extern bool lvlbtndown;
extern int dropGoldValue;
extern bool drawmanaflag;
extern bool chrbtnactive;
extern DVL_API_FOR_TEST int pnumlines;
extern UiFlags InfoColor;
extern int sbooktab;
extern int8_t initialDropGoldIndex;
extern bool talkflag;
extern bool sbookflag;
extern bool chrflag;
extern bool drawbtnflag;
extern std::string InfoString;
extern bool panelflag;
extern int initialDropGoldValue;
extern bool panbtndown;
extern bool spselflag;
const Rectangle &GetMainPanel();
const Rectangle &GetLeftPanel();
const Rectangle &GetRightPanel();
extern std::optional<OwnedSurface> pBtmBuff;
extern std::optional<OwnedCelSprite> pGBoxBuff;
extern SDL_Rect PanBtnPos[8];

void CalculatePanelAreas();
bool IsChatAvailable();
/**
 * @brief Check if the UI can cover the game area entierly
 */
inline bool CanPanelsCoverView()
{
	return GetScreenWidth() <= PANEL_WIDTH && GetScreenHeight() <= SPANEL_HEIGHT + PANEL_HEIGHT;
}
void DrawSpellList(const Surface &out);
void SetSpell();
void SetSpeedSpell(size_t slot);
void ToggleSpell(size_t slot);

void AddPanelString(string_view str);
void ClearPanel();
void DrawPanelBox(const Surface &out, SDL_Rect srcRect, Point targetPosition);
Point GetPanelPosition(UiPanels panel, Point offset = { 0, 0 });

/**
 * Draws the top dome of the life flask (that part that protrudes out of the control panel).
 * The empty flask cel is drawn from the top of the flask to the fill level (there is always a 2 pixel "air gap") and
 * the filled flask cel is drawn from that level to the top of the control panel if required.
 */
void DrawLifeFlaskUpper(const Surface &out);

/**
 * Controls the drawing of the area of the life flask within the control panel.
 * First sets the fill amount then draws the empty flask cel portion then the filled
 * flask portion.
 */
void DrawLifeFlaskLower(const Surface &out);

/**
 * Draws the top dome of the mana flask (that part that protrudes out of the control panel).
 * The empty flask cel is drawn from the top of the flask to the fill level (there is always a 2 pixel "air gap") and
 * the filled flask cel is drawn from that level to the top of the control panel if required.
 */
void DrawManaFlaskUpper(const Surface &out);

/**
 * Controls the drawing of the area of the mana flask within the control panel.
 */
void DrawManaFlaskLower(const Surface &out);

/**
 * Controls drawing of current / max values (health, mana) within the control panel.
 */
void DrawFlaskValues(const Surface &out, Point pos, int currValue, int maxValue);

/**
 * @brief calls on the active player object to update HP/Mana percentage variables
 *
 * This is used to ensure that DrawFlask routines display an accurate representation of the players health/mana
 *
 * @see Player::UpdateHitPointPercentage() and Player::UpdateManaPercentage()
 */
void control_update_life_mana();

/**
 * @brief draws the current right mouse button spell.
 * @param out screen buffer representing the main UI panel
 */
void DrawSpell(const Surface &out);

void InitControlPan();
void DrawCtrlPan(const Surface &out);

/**
 * Draws the control panel buttons in their current state. If the button is in the default
 * state draw it from the panel cel(extract its sub-rect). Else draw it from the buttons cel.
 */
void DrawCtrlBtns(const Surface &out);

/**
 * Clears panel button flags.
 */
void ClearPanBtn();

/**
 * Checks if the mouse cursor is within any of the panel buttons and flag it if so.
 */
void DoPanBtn();

void control_check_btn_press();
void DoAutoMap();

/**
 * Checks the mouse cursor position within the control panel and sets information
 * strings if needed.
 */
void CheckPanelInfo();

/**
 * Check if the mouse is within a control panel button that's flagged.
 * Takes apropiate action if so.
 */
void CheckBtnUp();
void FreeControlPan();

/**
 * Sets a string to be drawn in the info box and then draws it.
 */
void DrawInfoBox(const Surface &out);
void CheckLvlBtn();
void ReleaseLvlBtn();
void DrawLevelUpIcon(const Surface &out);
void CheckChrBtns();
void ReleaseChrBtns(bool addAllStatPoints);
void DrawDurIcon(const Surface &out);
void RedBack(const Surface &out);
void DrawSpellBook(const Surface &out);
void DrawGoldSplit(const Surface &out, int amount);
void control_drop_gold(char vkey);
void DrawTalkPan(const Surface &out);
bool control_check_talk_btn();
void control_release_talk_btn();
void control_type_message();
void control_reset_talk();
bool IsTalkActive();
void control_new_text(string_view text);
bool control_presskeys(int vkey);
void DiabloHotkeyMsg(uint32_t dwMsg);
void CloseGoldDrop();
void GoldDropNewText(string_view text);
extern Rectangle ChrBtnsRect[4];

} // namespace devilution
