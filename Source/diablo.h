/**
 * @file diablo.h
 *
 * Interface of the main game initialization functions.
 */
#pragma once

#include <cstdint>

#ifdef _DEBUG
#include "monstdat.h"
#endif
#include "gendung.h"
#include "init.h"
#include "utils/attributes.h"
#include "utils/endian.hpp"

namespace devilution {

constexpr uint32_t GameIdDiabloFull = LoadBE32("DRTL");
constexpr uint32_t GameIdDiabloSpawn = LoadBE32("DSHR");
constexpr uint32_t GameIdHellfireFull = LoadBE32("HRTL");
constexpr uint32_t GameIdHellfireSpawn = LoadBE32("HSHR");
constexpr uint32_t GameIdMiddleEarth = LoadBE32("MEMD");
#define GAME_ID GameIdMiddleEarth //(gbIsHellfire ? (gbIsSpawn ? GameIdHellfireSpawn : GameIdHellfireFull) : (gbIsSpawn ? GameIdDiabloSpawn : GameIdDiabloFull))

#define NUMLEVELS 25

enum clicktype : int8_t {
	CLICK_NONE,
	CLICK_LEFT,
	CLICK_RIGHT,
};

/**
 * @brief Specifices what game logic step is currently executed
 */
enum class GameLogicStep {
	None,
	ProcessPlayers,
	ProcessMonsters,
	ProcessObjects,
	ProcessMissiles,
	ProcessItems,
	ProcessTowners,
	ProcessItemsTown,
	ProcessMissilesTown,
};

enum class MouseActionType : int {
	None,
	Walk,
	Spell,
	SpellMonsterTarget,
	SpellPlayerTarget,
	Attack,
	AttackMonsterTarget,
	AttackPlayerTarget,
	OperateObject,
};

extern SDL_Window *ghMainWnd;
extern uint32_t glSeedTbl[NUMLEVELS];
extern dungeon_type gnLevelTypeTbl[NUMLEVELS];
extern Point MousePosition;
extern bool gbRunGame;
extern bool gbRunGameResult;
extern bool ReturnToMainMenu;
extern DVL_API_FOR_TEST bool zoomflag;
extern bool gbProcessPlayers;
extern bool gbLoadGame;
extern bool cineflag;
extern int force_redraw;
/* These are defined in fonts.h */
extern void FontsCleanup();
extern DVL_API_FOR_TEST int PauseMode;
extern bool gbBard;
extern bool gbBarbarian;
/**
 * @brief Don't show Messageboxes or other user-interaction. Needed for UnitTests.
 */
extern DVL_API_FOR_TEST bool gbQuietMode;
extern clicktype sgbMouseDown;
extern uint16_t gnTickDelay;
extern char gszProductName[64];

extern MouseActionType LastMouseButtonAction;

void FreeGameMem();
bool StartGame(bool bNewGame, bool bSinglePlayer);
[[noreturn]] void diablo_quit(int exitStatus);
int DiabloMain(int argc, char **argv);
bool TryIconCurs();
void diablo_pause_game();
bool diablo_is_focused();
void diablo_focus_pause();
void diablo_focus_unpause();
bool PressEscKey();
void DisableInputWndProc(uint32_t uMsg, int32_t wParam, int32_t lParam);
void LoadGameLevel(bool firstflag, lvl_entry lvldir);

/**
 * @param bStartup Process additional ticks before returning
 */
void game_loop(bool bStartup);
void diablo_color_cyc_logic();

/* rdata */

#ifdef _DEBUG
extern bool DebugDisableNetworkTimeout;
#endif

struct QuickMessage {
	/** Config variable names for quick message */
	const char *const key;
	/** Default quick message */
	const char *const message;
};

constexpr size_t QUICK_MESSAGE_OPTIONS = 4;
extern QuickMessage QuickMessages[QUICK_MESSAGE_OPTIONS];
extern bool gbFriendlyMode;
/**
 * @brief Specifices what game logic step is currently executed
 */
extern GameLogicStep gGameLogicStep;

#ifdef __UWP__
void setOnInitialized(void (*)());
#endif

} // namespace devilution
