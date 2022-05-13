/**
 * @file player.cpp
 *
 * Implementation of player functionality, leveling, actions, creation, loading, etc.
 */
#include <algorithm>
#include <cstdint>

#include "control.h"
#include "controls/plrctrls.h"
#include "cursor.h"
#include "dead.h"
#ifdef _DEBUG
#include "debug.h"
#endif
#include "engine/cel_header.hpp"
#include "engine/load_file.hpp"
#include "engine/random.hpp"
#include "gamemenu.h"
#include "init.h"
#include "inv_iterators.hpp"
#include "lighting.h"
#include "loadsave.h"
#include "minitext.h"
#include "missiles.h"
#include "nthread.h"
#include "options.h"
#include "player.h"
#include "qol/autopickup.h"
#include "qol/stash.h"
#include "spells.h"
#include "stores.h"
#include "towners.h"
#include "utils/language.h"
#include "utils/log.hpp"
#include "utils/utf8.hpp"

namespace devilution {

int MyPlayerId;
Player *MyPlayer;
Player Players[MAX_PLRS];
bool MyPlayerIsDead;

/** Specifies the X-coordinate delta from the player start location in Tristram. */
int plrxoff[9] = { 0, 2, 0, 2, 1, 0, 1, 2, 1 };
/** Specifies the Y-coordinate delta from the player start location in Tristram. */
int plryoff[9] = { 0, 2, 2, 0, 1, 1, 0, 1, 2 };
/** Specifies the X-coordinate delta from a player, used for instance when casting resurrect. */
int plrxoff2[9] = { 0, 1, 0, 1, 2, 0, 1, 2, 2 };
/** Specifies the Y-coordinate delta from a player, used for instance when casting resurrect. */
int plryoff2[9] = { 0, 0, 1, 1, 0, 2, 2, 1, 2 };

/** Maps from player_class to starting stat in strength. */
int StrengthTbl[enum_size<HeroClass>::value] = {
	30,
	15,
	0,
	25,
	20,
	40,
};
/** Maps from player_class to starting stat in magic. */
int MagicTbl[enum_size<HeroClass>::value] = {
	// clang-format off
	0,
	15,
	40,
	15,
	20,
	 0,
	// clang-format on
};
/** Maps from player_class to starting stat in dexterity. */
int DexterityTbl[enum_size<HeroClass>::value] = {
	20,
	35,
	10,
	25,
	25,
	20,
};
/** Maps from player_class to starting stat in vitality. */
int VitalityTbl[enum_size<HeroClass>::value] = {
	30,
	20,
	20,
	20,
	20,
	25,
};
/** Specifies the chance to block bonus of each player class.*/
int BlockBonuses[enum_size<HeroClass>::value] = {
	30,
	20,
	10,
	25,
	25,
	30,
};

/** Specifies the experience point limit of each level. */
uint32_t ExpLvlsTbl[MAXCHARLEVEL + 1] = {
	0,
	2000,
	4620,
	8040,
	12489,
	18258,
	25712,
	35309,
	47622,
	63364,
	83419,
	108879,
	141086,
	181683,
	231075,
	313656,
	424067,
	571190,
	766569,
	1025154,
	1366227,
	1814568,
	2401895,
	3168651,
	4166200,
	5459523,
	7130496,
	9281874,
	12042092,
	15571031,
	20066900,
	25774405,
	32994399,
	42095202,
	53525811,
	67831218,
	85670061,
	107834823,
	135274799,
	169122009,
	210720231,
	261657253,
	323800420,
	399335440,
	490808349,
	601170414,
	733825617,
	892680222,
	1082908612,
	1310707109,
	1583495809
};

namespace {

struct DirectionSettings {
	Direction dir;
	Displacement tileAdd;
	Displacement offset;
	Displacement map;
	ScrollDirection scrollDir;
	PLR_MODE walkMode;
	void (*walkModeHandler)(int, const DirectionSettings &);
};

/** Specifies the frame of each animation for which an action is triggered, for each player class. */
const int PlrGFXAnimLens[enum_size<HeroClass>::value][11] = {
	{ 10, 16, 8, 2, 20, 20, 6, 20, 8, 9, 14 },
	{ 8, 18, 8, 4, 20, 16, 7, 20, 8, 10, 12 },
	{ 8, 16, 8, 6, 20, 12, 8, 20, 8, 12, 8 },
	{ 8, 16, 8, 3, 20, 18, 6, 20, 8, 12, 13 },
	{ 8, 18, 8, 4, 20, 16, 7, 20, 8, 10, 12 },
	{ 10, 16, 8, 2, 20, 20, 6, 20, 8, 9, 14 },
};
/** Maps from player class to player velocity. */
int PWVel[enum_size<HeroClass>::value][3] = {
	{ 2048, 1024, 512 },
	{ 2048, 1024, 512 },
	{ 2048, 1024, 512 },
	{ 2048, 1024, 512 },
	{ 2048, 1024, 512 },
	{ 2048, 1024, 512 },
};
const char *const ClassPathTbl[] = {
	"Warrior",
	"Rogue",
	"Sorceror",
	"Monk",
	"Rogue",
	"Warrior",
};

void PmChangeLightOff(Player &player)
{
	if (player._plid == NO_LIGHT)
		return;

	const Light *l = &Lights[player._plid];
	int x = 2 * player.position.offset.deltaY + player.position.offset.deltaX;
	int y = 2 * player.position.offset.deltaY - player.position.offset.deltaX;

	x = (x / 8) * (x < 0 ? 1 : -1);
	y = (y / 8) * (y < 0 ? 1 : -1);
	int lx = x + (l->position.tile.x * 8);
	int ly = y + (l->position.tile.y * 8);
	int offx = l->position.offset.x + (l->position.tile.x * 8);
	int offy = l->position.offset.y + (l->position.tile.y * 8);

	if (abs(lx - offx) < 3 && abs(ly - offy) < 3)
		return;

	ChangeLightOffset(player._plid, { x, y });
}

void WalkUpwards(int pnum, const DirectionSettings &walkParams)
{
	auto &player = Players[pnum];
	dPlayer[player.position.future.x][player.position.future.y] = -(pnum + 1);
	player.position.temp = { walkParams.tileAdd.deltaX, walkParams.tileAdd.deltaY };
}

void WalkDownwards(int pnum, const DirectionSettings & /*walkParams*/)
{
	auto &player = Players[pnum];
	dPlayer[player.position.tile.x][player.position.tile.y] = -(pnum + 1);
	player.position.temp = player.position.tile;
	player.position.tile = player.position.future; // Move player to the next tile to maintain correct render order
	dPlayer[player.position.tile.x][player.position.tile.y] = pnum + 1;
	// BUGFIX: missing `if (leveltype != DTYPE_TOWN) {` for call to ChangeLightXY and PM_ChangeLightOff.
	ChangeLightXY(player._plid, player.position.tile);
	PmChangeLightOff(player);
}

void WalkSides(int pnum, const DirectionSettings &walkParams)
{
	auto &player = Players[pnum];

	Point const nextPosition = player.position.tile + walkParams.map;

	dPlayer[player.position.tile.x][player.position.tile.y] = -(pnum + 1);
	dPlayer[player.position.future.x][player.position.future.y] = pnum + 1;

	if (leveltype != DTYPE_TOWN) {
		ChangeLightXY(player._plid, nextPosition);
		PmChangeLightOff(player);
	}

	player.position.temp = player.position.future;
}

_sfx_id herosounds[enum_size<HeroClass>::value][enum_size<HeroSpeech>::value] = {
	// clang-format off
	{ PS_WARR1,  PS_WARR2,  PS_WARR3,  PS_WARR4,  PS_WARR5,  PS_WARR6,  PS_WARR7,  PS_WARR8,  PS_WARR9,  PS_WARR10,  PS_WARR11,  PS_WARR12,  PS_WARR13,  PS_WARR14,  PS_WARR15,  PS_WARR16,  PS_WARR17,  PS_WARR18,  PS_WARR19,  PS_WARR20,  PS_WARR21,  PS_WARR22,  PS_WARR23,  PS_WARR24,  PS_WARR25,  PS_WARR26,  PS_WARR27,  PS_WARR28,  PS_WARR29,  PS_WARR30,  PS_WARR31,  PS_WARR32,  PS_WARR33,  PS_WARR34,  PS_WARR35,  PS_WARR36,  PS_WARR37,  PS_WARR38,  PS_WARR39,  PS_WARR40,  PS_WARR41,  PS_WARR42,  PS_WARR43,  PS_WARR44,  PS_WARR45,  PS_WARR46,  PS_WARR47,  PS_WARR48,  PS_WARR49,  PS_WARR50,  PS_WARR51,  PS_WARR52,  PS_WARR53,  PS_WARR54,  PS_WARR55,  PS_WARR56,  PS_WARR57,  PS_WARR58,  PS_WARR59,  PS_WARR60,  PS_WARR61,  PS_WARR62,  PS_WARR63,  PS_WARR64,  PS_WARR65,  PS_WARR66,  PS_WARR67,  PS_WARR68,  PS_WARR69,  PS_WARR70,  PS_WARR71,  PS_WARR72,  PS_WARR73,  PS_WARR74,  PS_WARR75,  PS_WARR76,  PS_WARR77,  PS_WARR78,  PS_WARR79,  PS_WARR80,  PS_WARR81,  PS_WARR82,  PS_WARR83,  PS_WARR84,  PS_WARR85,  PS_WARR86,  PS_WARR87,  PS_WARR88,  PS_WARR89,  PS_WARR90,  PS_WARR91,  PS_WARR92,  PS_WARR93,  PS_WARR94,  PS_WARR95,  PS_WARR96B,  PS_WARR97,  PS_WARR98,  PS_WARR99,  PS_WARR100,  PS_WARR101,  PS_WARR102,  PS_DEAD    },
	{ PS_ROGUE1, PS_ROGUE2, PS_ROGUE3, PS_ROGUE4, PS_ROGUE5, PS_ROGUE6, PS_ROGUE7, PS_ROGUE8, PS_ROGUE9, PS_ROGUE10, PS_ROGUE11, PS_ROGUE12, PS_ROGUE13, PS_ROGUE14, PS_ROGUE15, PS_ROGUE16, PS_ROGUE17, PS_ROGUE18, PS_ROGUE19, PS_ROGUE20, PS_ROGUE21, PS_ROGUE22, PS_ROGUE23, PS_ROGUE24, PS_ROGUE25, PS_ROGUE26, PS_ROGUE27, PS_ROGUE28, PS_ROGUE29, PS_ROGUE30, PS_ROGUE31, PS_ROGUE32, PS_ROGUE33, PS_ROGUE34, PS_ROGUE35, PS_ROGUE36, PS_ROGUE37, PS_ROGUE38, PS_ROGUE39, PS_ROGUE40, PS_ROGUE41, PS_ROGUE42, PS_ROGUE43, PS_ROGUE44, PS_ROGUE45, PS_ROGUE46, PS_ROGUE47, PS_ROGUE48, PS_ROGUE49, PS_ROGUE50, PS_ROGUE51, PS_ROGUE52, PS_ROGUE53, PS_ROGUE54, PS_ROGUE55, PS_ROGUE56, PS_ROGUE57, PS_ROGUE58, PS_ROGUE59, PS_ROGUE60, PS_ROGUE61, PS_ROGUE62, PS_ROGUE63, PS_ROGUE64, PS_ROGUE65, PS_ROGUE66, PS_ROGUE67, PS_ROGUE68, PS_ROGUE69, PS_ROGUE70, PS_ROGUE71, PS_ROGUE72, PS_ROGUE73, PS_ROGUE74, PS_ROGUE75, PS_ROGUE76, PS_ROGUE77, PS_ROGUE78, PS_ROGUE79, PS_ROGUE80, PS_ROGUE81, PS_ROGUE82, PS_ROGUE83, PS_ROGUE84, PS_ROGUE85, PS_ROGUE86, PS_ROGUE87, PS_ROGUE88, PS_ROGUE89, PS_ROGUE90, PS_ROGUE91, PS_ROGUE92, PS_ROGUE93, PS_ROGUE94, PS_ROGUE95, PS_ROGUE96,  PS_ROGUE97, PS_ROGUE98, PS_ROGUE99, PS_ROGUE100, PS_ROGUE101, PS_ROGUE102, PS_ROGUE71 },
	{ PS_MAGE1,  PS_MAGE2,  PS_MAGE3,  PS_MAGE4,  PS_MAGE5,  PS_MAGE6,  PS_MAGE7,  PS_MAGE8,  PS_MAGE9,  PS_MAGE10,  PS_MAGE11,  PS_MAGE12,  PS_MAGE13,  PS_MAGE14,  PS_MAGE15,  PS_MAGE16,  PS_MAGE17,  PS_MAGE18,  PS_MAGE19,  PS_MAGE20,  PS_MAGE21,  PS_MAGE22,  PS_MAGE23,  PS_MAGE24,  PS_MAGE25,  PS_MAGE26,  PS_MAGE27,  PS_MAGE28,  PS_MAGE29,  PS_MAGE30,  PS_MAGE31,  PS_MAGE32,  PS_MAGE33,  PS_MAGE34,  PS_MAGE35,  PS_MAGE36,  PS_MAGE37,  PS_MAGE38,  PS_MAGE39,  PS_MAGE40,  PS_MAGE41,  PS_MAGE42,  PS_MAGE43,  PS_MAGE44,  PS_MAGE45,  PS_MAGE46,  PS_MAGE47,  PS_MAGE48,  PS_MAGE49,  PS_MAGE50,  PS_MAGE51,  PS_MAGE52,  PS_MAGE53,  PS_MAGE54,  PS_MAGE55,  PS_MAGE56,  PS_MAGE57,  PS_MAGE58,  PS_MAGE59,  PS_MAGE60,  PS_MAGE61,  PS_MAGE62,  PS_MAGE63,  PS_MAGE64,  PS_MAGE65,  PS_MAGE66,  PS_MAGE67,  PS_MAGE68,  PS_MAGE69,  PS_MAGE70,  PS_MAGE71,  PS_MAGE72,  PS_MAGE73,  PS_MAGE74,  PS_MAGE75,  PS_MAGE76,  PS_MAGE77,  PS_MAGE78,  PS_MAGE79,  PS_MAGE80,  PS_MAGE81,  PS_MAGE82,  PS_MAGE83,  PS_MAGE84,  PS_MAGE85,  PS_MAGE86,  PS_MAGE87,  PS_MAGE88,  PS_MAGE89,  PS_MAGE90,  PS_MAGE91,  PS_MAGE92,  PS_MAGE93,  PS_MAGE94,  PS_MAGE95,  PS_MAGE96,   PS_MAGE97,  PS_MAGE98,  PS_MAGE99,  PS_MAGE100,  PS_MAGE101,  PS_MAGE102,  PS_MAGE71  },
	{ PS_MONK1,  PS_MONK2,  PS_MONK3,  PS_MONK4,  PS_MONK5,  PS_MONK6,  PS_MONK7,  PS_MONK8,  PS_MONK9,  PS_MONK10,  PS_MONK11,  PS_MONK12,  PS_MONK13,  PS_MONK14,  PS_MONK15,  PS_MONK16,  PS_MONK17,  PS_MONK18,  PS_MONK19,  PS_MONK20,  PS_MONK21,  PS_MONK22,  PS_MONK23,  PS_MONK24,  PS_MONK25,  PS_MONK26,  PS_MONK27,  PS_MONK28,  PS_MONK29,  PS_MONK30,  PS_MONK31,  PS_MONK32,  PS_MONK33,  PS_MONK34,  PS_MONK35,  PS_MONK36,  PS_MONK37,  PS_MONK38,  PS_MONK39,  PS_MONK40,  PS_MONK41,  PS_MONK42,  PS_MONK43,  PS_MONK44,  PS_MONK45,  PS_MONK46,  PS_MONK47,  PS_MONK48,  PS_MONK49,  PS_MONK50,  PS_MONK51,  PS_MONK52,  PS_MONK53,  PS_MONK54,  PS_MONK55,  PS_MONK56,  PS_MONK57,  PS_MONK58,  PS_MONK59,  PS_MONK60,  PS_MONK61,  PS_MONK62,  PS_MONK63,  PS_MONK64,  PS_MONK65,  PS_MONK66,  PS_MONK67,  PS_MONK68,  PS_MONK69,  PS_MONK70,  PS_MONK71,  PS_MONK72,  PS_MONK73,  PS_MONK74,  PS_MONK75,  PS_MONK76,  PS_MONK77,  PS_MONK78,  PS_MONK79,  PS_MONK80,  PS_MONK81,  PS_MONK82,  PS_MONK83,  PS_MONK84,  PS_MONK85,  PS_MONK86,  PS_MONK87,  PS_MONK88,  PS_MONK89,  PS_MONK90,  PS_MONK91,  PS_MONK92,  PS_MONK93,  PS_MONK94,  PS_MONK95,  PS_MONK96,   PS_MONK97,  PS_MONK98,  PS_MONK99,  PS_MONK100,  PS_MONK101,  PS_MONK102,  PS_MONK71  },
	{ PS_ROGUE1, PS_ROGUE2, PS_ROGUE3, PS_ROGUE4, PS_ROGUE5, PS_ROGUE6, PS_ROGUE7, PS_ROGUE8, PS_ROGUE9, PS_ROGUE10, PS_ROGUE11, PS_ROGUE12, PS_ROGUE13, PS_ROGUE14, PS_ROGUE15, PS_ROGUE16, PS_ROGUE17, PS_ROGUE18, PS_ROGUE19, PS_ROGUE20, PS_ROGUE21, PS_ROGUE22, PS_ROGUE23, PS_ROGUE24, PS_ROGUE25, PS_ROGUE26, PS_ROGUE27, PS_ROGUE28, PS_ROGUE29, PS_ROGUE30, PS_ROGUE31, PS_ROGUE32, PS_ROGUE33, PS_ROGUE34, PS_ROGUE35, PS_ROGUE36, PS_ROGUE37, PS_ROGUE38, PS_ROGUE39, PS_ROGUE40, PS_ROGUE41, PS_ROGUE42, PS_ROGUE43, PS_ROGUE44, PS_ROGUE45, PS_ROGUE46, PS_ROGUE47, PS_ROGUE48, PS_ROGUE49, PS_ROGUE50, PS_ROGUE51, PS_ROGUE52, PS_ROGUE53, PS_ROGUE54, PS_ROGUE55, PS_ROGUE56, PS_ROGUE57, PS_ROGUE58, PS_ROGUE59, PS_ROGUE60, PS_ROGUE61, PS_ROGUE62, PS_ROGUE63, PS_ROGUE64, PS_ROGUE65, PS_ROGUE66, PS_ROGUE67, PS_ROGUE68, PS_ROGUE69, PS_ROGUE70, PS_ROGUE71, PS_ROGUE72, PS_ROGUE73, PS_ROGUE74, PS_ROGUE75, PS_ROGUE76, PS_ROGUE77, PS_ROGUE78, PS_ROGUE79, PS_ROGUE80, PS_ROGUE81, PS_ROGUE82, PS_ROGUE83, PS_ROGUE84, PS_ROGUE85, PS_ROGUE86, PS_ROGUE87, PS_ROGUE88, PS_ROGUE89, PS_ROGUE90, PS_ROGUE91, PS_ROGUE92, PS_ROGUE93, PS_ROGUE94, PS_ROGUE95, PS_ROGUE96,  PS_ROGUE97, PS_ROGUE98, PS_ROGUE99, PS_ROGUE100, PS_ROGUE101, PS_ROGUE102, PS_ROGUE71 },
	{ PS_WARR1,  PS_WARR2,  PS_WARR3,  PS_WARR4,  PS_WARR5,  PS_WARR6,  PS_WARR7,  PS_WARR8,  PS_WARR9,  PS_WARR10,  PS_WARR11,  PS_WARR12,  PS_WARR13,  PS_WARR14,  PS_WARR15,  PS_WARR16,  PS_WARR17,  PS_WARR18,  PS_WARR19,  PS_WARR20,  PS_WARR21,  PS_WARR22,  PS_WARR23,  PS_WARR24,  PS_WARR25,  PS_WARR26,  PS_WARR27,  PS_WARR28,  PS_WARR29,  PS_WARR30,  PS_WARR31,  PS_WARR32,  PS_WARR33,  PS_WARR34,  PS_WARR35,  PS_WARR36,  PS_WARR37,  PS_WARR38,  PS_WARR39,  PS_WARR40,  PS_WARR41,  PS_WARR42,  PS_WARR43,  PS_WARR44,  PS_WARR45,  PS_WARR46,  PS_WARR47,  PS_WARR48,  PS_WARR49,  PS_WARR50,  PS_WARR51,  PS_WARR52,  PS_WARR53,  PS_WARR54,  PS_WARR55,  PS_WARR56,  PS_WARR57,  PS_WARR58,  PS_WARR59,  PS_WARR60,  PS_WARR61,  PS_WARR62,  PS_WARR63,  PS_WARR64,  PS_WARR65,  PS_WARR66,  PS_WARR67,  PS_WARR68,  PS_WARR69,  PS_WARR70,  PS_WARR71,  PS_WARR72,  PS_WARR73,  PS_WARR74,  PS_WARR75,  PS_WARR76,  PS_WARR77,  PS_WARR78,  PS_WARR79,  PS_WARR80,  PS_WARR81,  PS_WARR82,  PS_WARR83,  PS_WARR84,  PS_WARR85,  PS_WARR86,  PS_WARR87,  PS_WARR88,  PS_WARR89,  PS_WARR90,  PS_WARR91,  PS_WARR92,  PS_WARR93,  PS_WARR94,  PS_WARR95,  PS_WARR96B,  PS_WARR97,  PS_WARR98,  PS_WARR99,  PS_WARR100,  PS_WARR101,  PS_WARR102,  PS_WARR71  },
	// clang-format on
};

constexpr std::array<const DirectionSettings, 8> WalkSettings { {
	// clang-format off
	{ Direction::South,     {  1,  1 }, {   0, -32 }, { 0, 0 }, ScrollDirection::South,     PM_WALK2, WalkDownwards },
	{ Direction::SouthWest, {  0,  1 }, {  32, -16 }, { 0, 0 }, ScrollDirection::SouthWest, PM_WALK2, WalkDownwards },
	{ Direction::West,      { -1,  1 }, {  32, -16 }, { 0, 1 }, ScrollDirection::West,      PM_WALK3, WalkSides     },
	{ Direction::NorthWest, { -1,  0 }, {   0,   0 }, { 0, 0 }, ScrollDirection::NorthWest, PM_WALK,  WalkUpwards   },
	{ Direction::North,     { -1, -1 }, {   0,   0 }, { 0, 0 }, ScrollDirection::North,     PM_WALK,  WalkUpwards   },
	{ Direction::NorthEast, {  0, -1 }, {   0,   0 }, { 0, 0 }, ScrollDirection::NorthEast, PM_WALK,  WalkUpwards   },
	{ Direction::East,      {  1, -1 }, { -32, -16 }, { 1, 0 }, ScrollDirection::East,      PM_WALK3, WalkSides     },
	{ Direction::SouthEast, {  1,  0 }, { -32, -16 }, { 0, 0 }, ScrollDirection::SouthEast, PM_WALK2, WalkDownwards }
	// clang-format on
} };

void ScrollViewPort(const Player &player, ScrollDirection dir)
{
	ScrollInfo.tile = Point { 0, 0 } + (player.position.tile - ViewPosition);

	if (zoomflag) {
		if (abs(ScrollInfo.tile.x) >= 3 || abs(ScrollInfo.tile.y) >= 3) {
			ScrollInfo._sdir = ScrollDirection::None;
		} else {
			ScrollInfo._sdir = dir;
		}
	} else if (abs(ScrollInfo.tile.x) >= 2 || abs(ScrollInfo.tile.y) >= 2) {
		ScrollInfo._sdir = ScrollDirection::None;
	} else {
		ScrollInfo._sdir = dir;
	}
}

bool PlrDirOK(const Player &player, Direction dir)
{
	Point position = player.position.tile;
	Point futurePosition = position + dir;
	if (futurePosition.x < 0 || dPiece[futurePosition.x][futurePosition.y] == 0 || !PosOkPlayer(player, futurePosition)) {
		return false;
	}

	if (dir == Direction::East) {
		return !IsTileSolid(position + Direction::SouthEast);
	}

	if (dir == Direction::West) {
		return !IsTileSolid(position + Direction::SouthWest);
	}

	return true;
}

void HandleWalkMode(int pnum, Displacement vel, Direction dir)
{
	auto &player = Players[pnum];
	const auto &dirModeParams = WalkSettings[static_cast<size_t>(dir)];
	SetPlayerOld(player);
	if (!PlrDirOK(player, dir)) {
		return;
	}

	player.position.offset = dirModeParams.offset; // Offset player sprite to align with their previous tile position
	// The player's tile position after finishing this movement action
	player.position.future = player.position.tile + dirModeParams.tileAdd;

	if (pnum == MyPlayerId) {
		ScrollViewPort(player, dirModeParams.scrollDir);
	}

	dirModeParams.walkModeHandler(pnum, dirModeParams);

	player.position.velocity = vel;
	player.tempDirection = dirModeParams.dir;
	player._pmode = dirModeParams.walkMode;
	player.position.offset2 = dirModeParams.offset * 256;

	player._pdir = dir;
}

void StartWalkAnimation(Player &player, Direction dir, bool pmWillBeCalled)
{
	int skippedFrames = -2;
	if (currlevel == 0 && sgGameInitInfo.bRunInTown != 0)
		skippedFrames = 2;
	if (pmWillBeCalled)
		skippedFrames += 1;
	NewPlrAnim(player, player_graphic::Walk, dir, player._pWFrames, 1, AnimationDistributionFlags::ProcessAnimationPending, skippedFrames);
}

/**
 * @brief Start moving a player to a new tile
 */
void StartWalk(int pnum, Displacement vel, Direction dir, bool pmWillBeCalled)
{
	auto &player = Players[pnum];

	if (player._pInvincible && player._pHitPoints == 0 && pnum == MyPlayerId) {
		SyncPlrKill(pnum, -1);
		return;
	}

	HandleWalkMode(pnum, vel, dir);
	StartWalkAnimation(player, dir, pmWillBeCalled);
}

void SetPlayerGPtrs(const char *path, std::unique_ptr<byte[]> &data, std::array<std::optional<CelSprite>, 8> &anim, int width)
{
	data = nullptr;
	data = LoadFileInMem(path);
	if (data == nullptr && gbQuietMode)
		return;

	const byte *directionFrames[8];
	CelGetDirectionFrames(data.get(), directionFrames);
	for (size_t i = 0; i < 8; i++) {
		anim[i].emplace(directionFrames[i], width);
	}
}

void ClearStateVariables(Player &player)
{
	player.position.temp = { 0, 0 };
	player.tempDirection = Direction::South;
	player.spellLevel = 0;
	player.position.offset2 = { 0, 0 };
}

void StartWalkStand(int pnum)
{
	if ((DWORD)pnum >= MAX_PLRS) {
		app_fatal("StartWalkStand: illegal player %i", pnum);
	}
	auto &player = Players[pnum];

	player._pmode = PM_STAND;
	player.position.future = player.position.tile;
	player.position.offset = { 0, 0 };

	if (pnum == MyPlayerId) {
		ScrollInfo.offset = { 0, 0 };
		ScrollInfo._sdir = ScrollDirection::None;
		ViewPosition = player.position.tile;
	}
}

void ChangeOffset(int pnum)
{
	if ((DWORD)pnum >= MAX_PLRS) {
		app_fatal("PM_ChangeOffset: illegal player %i", pnum);
	}
	auto &player = Players[pnum];

	int px = player.position.offset2.deltaX / 256;
	int py = player.position.offset2.deltaY / 256;

	player.position.offset2 += player.position.velocity;

	if (currlevel == 0 && sgGameInitInfo.bRunInTown != 0) {
		player.position.offset2 += player.position.velocity;
	}

	player.position.offset = { player.position.offset2.deltaX >> 8, player.position.offset2.deltaY >> 8 };

	px -= player.position.offset2.deltaX >> 8;
	py -= player.position.offset2.deltaY >> 8;

	if (pnum == MyPlayerId && ScrollInfo._sdir != ScrollDirection::None) {
		ScrollInfo.offset += { px, py };
	}

	PmChangeLightOff(player);
}

void StartAttack(int pnum, Direction d)
{
	if ((DWORD)pnum >= MAX_PLRS) {
		app_fatal("StartAttack: illegal player %i", pnum);
	}
	auto &player = Players[pnum];

	if (player._pInvincible && player._pHitPoints == 0 && pnum == MyPlayerId) {
		SyncPlrKill(pnum, -1);
		return;
	}

	int skippedAnimationFrames = 0;
	if (HasAnyOf(player._pIFlags, ItemSpecialEffect::FasterAttack)) {
		// The combination of Faster and Fast Attack doesn't result in more skipped skipped frames, cause the secound frame skip of Faster Attack is not triggered.
		skippedAnimationFrames = 2;
	} else if (HasAnyOf(player._pIFlags, ItemSpecialEffect::FastAttack)) {
		skippedAnimationFrames = 1;
	} else if (HasAnyOf(player._pIFlags, ItemSpecialEffect::FastestAttack)) {
		// Fastest Attack is skipped if Fast or Faster Attack is also specified, cause both skip the frame that triggers fastest attack skipping
		skippedAnimationFrames = 3;
	}

	auto animationFlags = AnimationDistributionFlags::ProcessAnimationPending;
	if (player._pmode == PM_ATTACK)
		animationFlags = static_cast<AnimationDistributionFlags>(animationFlags | AnimationDistributionFlags::RepeatedAction);
	NewPlrAnim(player, player_graphic::Attack, d, player._pAFrames, 1, animationFlags, skippedAnimationFrames, player._pAFNum);
	player._pmode = PM_ATTACK;
	FixPlayerLocation(pnum, d);
	SetPlayerOld(player);
}

void StartRangeAttack(int pnum, Direction d, int cx, int cy)
{
	if ((DWORD)pnum >= MAX_PLRS) {
		app_fatal("StartRangeAttack: illegal player %i", pnum);
	}
	auto &player = Players[pnum];

	if (player._pInvincible && player._pHitPoints == 0 && pnum == MyPlayerId) {
		SyncPlrKill(pnum, -1);
		return;
	}

	// ME mod gives xbow and heavy xbow faster and fastest attack speed
	int skippedAnimationFrames = 0;
	if (!gbIsHellfire) {
		if (HasAnyOf(player._pIFlags, ItemSpecialEffect::FastestAttack)) {
			skippedAnimationFrames = 3;
		} else if (HasAnyOf(player._pIFlags, ItemSpecialEffect::FasterAttack)) {
			skippedAnimationFrames = 2;
		} else if (HasAnyOf(player._pIFlags, ItemSpecialEffect::FastAttack)) {
			skippedAnimationFrames = 1;
		}
	}

	auto animationFlags = AnimationDistributionFlags::ProcessAnimationPending;
	if (player._pmode == PM_RATTACK)
		animationFlags = static_cast<AnimationDistributionFlags>(animationFlags | AnimationDistributionFlags::RepeatedAction);
	NewPlrAnim(player, player_graphic::Attack, d, player._pAFrames, 1, animationFlags, skippedAnimationFrames, player._pAFNum);

	player._pmode = PM_RATTACK;
	FixPlayerLocation(pnum, d);
	SetPlayerOld(player);
	player.position.temp = { cx, cy };
}

player_graphic GetPlayerGraphicForSpell(spell_id spellId)
{
	switch (spelldata[spellId].sType) {
	case STYPE_FIRE:
		return player_graphic::Fire;
	case STYPE_LIGHTNING:
		return player_graphic::Lightning;
	default:
		return player_graphic::Magic;
	}
}

void StartSpell(int pnum, Direction d, int cx, int cy)
{
	if ((DWORD)pnum >= MAX_PLRS)
		app_fatal("StartSpell: illegal player %i", pnum);
	auto &player = Players[pnum];

	if (player._pInvincible && player._pHitPoints == 0 && pnum == MyPlayerId) {
		SyncPlrKill(pnum, -1);
		return;
	}

	auto animationFlags = AnimationDistributionFlags::ProcessAnimationPending;
	if (player._pmode == PM_SPELL)
		animationFlags = static_cast<AnimationDistributionFlags>(animationFlags | AnimationDistributionFlags::RepeatedAction);
	NewPlrAnim(player, GetPlayerGraphicForSpell(player._pSpell), d, player._pSFrames, 1, animationFlags, 0, player._pSFNum);

	PlaySfxLoc(spelldata[player._pSpell].sSFX, player.position.tile);

	player._pmode = PM_SPELL;

	FixPlayerLocation(pnum, d);
	SetPlayerOld(player);

	player.position.temp = { cx, cy };
	player.spellLevel = GetSpellLevel(pnum, player._pSpell);
}

void RespawnDeadItem(Item &&itm, Point target)
{
	if (ActiveItemCount >= MAXITEMS)
		return;

	int ii = AllocateItem();

	dItem[target.x][target.y] = ii + 1;

	Items[ii] = itm;
	Items[ii].position = target;
	RespawnItem(Items[ii], true);
	NetSendCmdPItem(false, CMD_RESPAWNITEM, target, Items[ii]);
}

void DeadItem(Player &player, Item &&itm, Displacement direction)
{
	if (itm.isEmpty())
		return;

	Point target = player.position.tile + direction;
	if (direction != Displacement { 0, 0 } && ItemSpaceOk(target)) {
		RespawnDeadItem(std::move(itm), target);
		return;
	}

	for (int k = 1; k < 50; k++) {
		for (int j = -k; j <= k; j++) {
			for (int i = -k; i <= k; i++) {
				Point next = player.position.tile + Displacement { i, j };
				if (ItemSpaceOk(next)) {
					RespawnDeadItem(std::move(itm), next);
					return;
				}
			}
		}
	}
}

int DropGold(Player &player, int amount, bool skipFullStacks)
{
	for (int i = 0; i < player._pNumInv && amount > 0; i++) {
		auto &item = player.InvList[i];

		if (item._itype != ItemType::Gold || (skipFullStacks && item._ivalue == MaxGold))
			continue;

		if (amount < item._ivalue) {
			Item goldItem;
			MakeGoldStack(goldItem, amount);
			DeadItem(player, std::move(goldItem), { 0, 0 });

			item._ivalue -= amount;

			return 0;
		}

		amount -= item._ivalue;
		DeadItem(player, std::move(item), { 0, 0 });
		player.RemoveInvItem(i);
		i = -1;
	}

	return amount;
}

void DropHalfPlayersGold(int pnum)
{
	if ((DWORD)pnum >= MAX_PLRS) {
		app_fatal("DropHalfPlayersGold: illegal player %i", pnum);
	}
	auto &player = Players[pnum];

	int remainingGold = DropGold(player, player._pGold / 2, true);
	if (remainingGold > 0) {
		DropGold(player, remainingGold, false);
	}

	player._pGold /= 2;
}

void InitLevelChange(int pnum)
{
	auto &player = Players[pnum];
	auto &myPlayer = Players[MyPlayerId];

	RemovePlrMissiles(pnum);
	player.pManaShield = false;
	player.wReflections = 0;
	player.wEtherealize = 0;
	// share info about your manashield when another player joins the level
	if (pnum != MyPlayerId && myPlayer.pManaShield)
		NetSendCmd(true, CMD_SETSHIELD);
	// share info about your reflect charges when another player joins the level
	if (pnum != MyPlayerId)
		NetSendCmdParam1(true, CMD_SETREFLECT, myPlayer.wReflections);
	// share info about your etherealize when another player joins the level
	if (pnum != MyPlayerId) {
		NetSendCmdParam1(true, CMD_SETETHEREALIZE, myPlayer.wEtherealize);
	}
	if (pnum == MyPlayerId && qtextflag) {
		qtextflag = false;
		stream_stop();
	}

	RemovePlrFromMap(pnum);
	SetPlayerOld(player);
	if (pnum == MyPlayerId) {
		dPlayer[player.position.tile.x][player.position.tile.y] = pnum + 1;
	} else {
		player._pLvlVisited[player.plrlevel] = true;
	}

	ClrPlrPath(player);
	player.destAction = ACTION_NONE;
	player._pLvlChanging = true;

	if (pnum == MyPlayerId) {
		player.pLvlLoad = 10;
	}
}

/**
 * @brief Continue movement towards new tile
 */
bool DoWalk(int pnum, int variant)
{
	if ((DWORD)pnum >= MAX_PLRS) {
		app_fatal("PM_DoWalk: illegal player %i", pnum);
	}
	auto &player = Players[pnum];

	// Play walking sound effect on certain animation frames
	if (*sgOptions.Audio.walkingSound && (currlevel != 0 || sgGameInitInfo.bRunInTown == 0)) {
		if (player.AnimInfo.CurrentFrame == 0
		    || player.AnimInfo.CurrentFrame == 4) {
			PlaySfxLoc(PS_WALK1, player.position.tile);
		}
	}

	// Check if we reached new tile
	if (player.AnimInfo.CurrentFrame >= player._pWFrames - 1) {

		// Update the player's tile position
		switch (variant) {
		case PM_WALK:
			dPlayer[player.position.tile.x][player.position.tile.y] = 0;
			player.position.tile += { player.position.temp.x, player.position.temp.y };
			dPlayer[player.position.tile.x][player.position.tile.y] = pnum + 1;
			break;
		case PM_WALK2:
			dPlayer[player.position.temp.x][player.position.temp.y] = 0;
			break;
		case PM_WALK3:
			dPlayer[player.position.tile.x][player.position.tile.y] = 0;
			player.position.tile = player.position.temp;
			// dPlayer is set here for backwards comparability, without it the player would be invisible if loaded from a vanilla save.
			dPlayer[player.position.tile.x][player.position.tile.y] = pnum + 1;
			break;
		}

		// Update the coordinates for lighting and vision entries for the player
		if (leveltype != DTYPE_TOWN) {
			ChangeLightXY(player._plid, player.position.tile);
			ChangeVisionXY(player._pvid, player.position.tile);
		}

		// Update the "camera" tile position
		if (pnum == MyPlayerId && ScrollInfo._sdir != ScrollDirection::None) {
			ViewPosition = Point { 0, 0 } + (player.position.tile - ScrollInfo.tile);
		}

		if (player.walkpath[0] != WALK_NONE) {
			StartWalkStand(pnum);
		} else {
			StartStand(pnum, player.tempDirection);
		}

		ClearStateVariables(player);

		// Reset the "sub-tile" position of the player's light entry to 0
		if (leveltype != DTYPE_TOWN) {
			ChangeLightOffset(player._plid, { 0, 0 });
		}

		AutoPickup(pnum);
		return true;
	} // We didn't reach new tile so update player's "sub-tile" position
	ChangeOffset(pnum);
	return false;
}

bool WeaponDecay(Player &player, int ii)
{
	if (!player.InvBody[ii].isEmpty() && player.InvBody[ii]._iClass == ICLASS_WEAPON && HasAnyOf(player.InvBody[ii]._iDamAcFlags, ItemSpecialEffectHf::Decay)) {
		player.InvBody[ii]._iPLDam -= 5;
		if (player.InvBody[ii]._iPLDam <= -100) {
			RemoveEquipment(player, static_cast<inv_body_loc>(ii), true);
			CalcPlrInv(player, true);
			return true;
		}
		CalcPlrInv(player, true);
	}
	return false;
}

bool DamageWeapon(int pnum, int durrnd)
{
	if (pnum != MyPlayerId) {
		return false;
	}

	if ((DWORD)pnum >= MAX_PLRS) {
		app_fatal("DamageWeapon: illegal player %i", pnum);
	}
	auto &player = Players[pnum];

	if (WeaponDecay(player, INVLOC_HAND_LEFT))
		return true;
	if (WeaponDecay(player, INVLOC_HAND_RIGHT))
		return true;

	if (GenerateRnd(durrnd) != 0) {
		return false;
	}

	if (!player.InvBody[INVLOC_HAND_LEFT].isEmpty() && player.InvBody[INVLOC_HAND_LEFT]._iClass == ICLASS_WEAPON) {
		if (player.InvBody[INVLOC_HAND_LEFT]._iDurability == DUR_INDESTRUCTIBLE) {
			return false;
		}

		player.InvBody[INVLOC_HAND_LEFT]._iDurability--;
		if (player.InvBody[INVLOC_HAND_LEFT]._iDurability <= 0) {
			RemoveEquipment(player, INVLOC_HAND_LEFT, true);
			CalcPlrInv(player, true);
			return true;
		}
	}

	if (!player.InvBody[INVLOC_HAND_RIGHT].isEmpty() && player.InvBody[INVLOC_HAND_RIGHT]._iClass == ICLASS_WEAPON) {
		if (player.InvBody[INVLOC_HAND_RIGHT]._iDurability == DUR_INDESTRUCTIBLE) {
			return false;
		}

		player.InvBody[INVLOC_HAND_RIGHT]._iDurability--;
		if (player.InvBody[INVLOC_HAND_RIGHT]._iDurability == 0) {
			RemoveEquipment(player, INVLOC_HAND_RIGHT, true);
			CalcPlrInv(player, true);
			return true;
		}
	}

	if (player.InvBody[INVLOC_HAND_LEFT].isEmpty() && player.InvBody[INVLOC_HAND_RIGHT]._itype == ItemType::Shield) {
		if (player.InvBody[INVLOC_HAND_RIGHT]._iDurability == DUR_INDESTRUCTIBLE) {
			return false;
		}

		player.InvBody[INVLOC_HAND_RIGHT]._iDurability--;
		if (player.InvBody[INVLOC_HAND_RIGHT]._iDurability == 0) {
			RemoveEquipment(player, INVLOC_HAND_RIGHT, true);
			CalcPlrInv(player, true);
			return true;
		}
	}

	if (player.InvBody[INVLOC_HAND_RIGHT].isEmpty() && player.InvBody[INVLOC_HAND_LEFT]._itype == ItemType::Shield) {
		if (player.InvBody[INVLOC_HAND_LEFT]._iDurability == DUR_INDESTRUCTIBLE) {
			return false;
		}

		player.InvBody[INVLOC_HAND_LEFT]._iDurability--;
		if (player.InvBody[INVLOC_HAND_LEFT]._iDurability == 0) {
			RemoveEquipment(player, INVLOC_HAND_LEFT, true);
			CalcPlrInv(player, true);
			return true;
		}
	}

	return false;
}

bool PlrHitMonst(int pnum, int m, bool adjacentDamage = false)
{
	int hper = 0;

	if ((DWORD)m >= MAXMONSTERS) {
		app_fatal("PlrHitMonst: illegal monster %i", m);
	}
	auto &monster = Monsters[m];

	if ((DWORD)pnum >= MAX_PLRS) {
		app_fatal("PlrHitMonst: illegal player %i", pnum);
	}
	auto &player = Players[pnum];

	if ((monster._mhitpoints >> 6) <= 0) {
		return false;
	}

	if (monster.MType->mtype == MT_ILLWEAV && monster._mgoal == MGOAL_RETREAT) {
		return false;
	}

	if (monster._mmode == MonsterMode::Charge) {
		return false;
	}

	if (adjacentDamage) {
		if (player._pLevel > 20)
			hper -= 30;
		else
			hper -= (35 - player._pLevel) * 2;
	}

	int hit = GenerateRnd(100);
	if (monster._mmode == MonsterMode::Petrified) {
		hit = 0;
	}

	hper += player.GetMeleePiercingToHit() - player.CalculateArmorPierce(monster.mArmorClass, true);
	hper = clamp(hper, 5, 95);

	bool ret = false;
	if (CheckMonsterHit(monster, &ret)) {
		return ret;
	}

	if (hit >= hper) {
#ifdef _DEBUG
		if (!DebugGodMode)
#endif
			return false;
	}

	if (HasAnyOf(player._pIFlags, ItemSpecialEffect::FireDamage) && HasAnyOf(player._pIFlags, ItemSpecialEffect::LightningDamage)) {
		int midam = player._pIFMinDam + GenerateRnd(player._pIFMaxDam - player._pIFMinDam);
		AddMissile(player.position.tile, player.position.temp, player._pdir, MIS_SPECARROW, TARGET_MONSTERS, pnum, midam, 0);
	}
	if (((player._pIFlags & ISPL_NOHEALMON) != 0))
		monster._mFlags |= MFLAG_NOHEAL;
	int mind = player._pIMinDam;
	int maxd = player._pIMaxDam;
	int dam = GenerateRnd(maxd - mind + 1) + mind;
	dam += dam * player._pIBonusDam / 100;
	dam += player._pIBonusDamMod;
	int dam2 = dam << 6;
	dam += player._pDamageMod;
	if (player._pClass == HeroClass::Warrior || player._pClass == HeroClass::Barbarian) {
		if (GenerateRnd(100) < player._pLevel) {
			dam *= 2;
		}
	}

	ItemType phanditype = ItemType::None;
	if (player.InvBody[INVLOC_HAND_LEFT]._itype == ItemType::Sword || player.InvBody[INVLOC_HAND_RIGHT]._itype == ItemType::Sword) {
		phanditype = ItemType::Sword;
	}
	if (player.InvBody[INVLOC_HAND_LEFT]._itype == ItemType::Mace || player.InvBody[INVLOC_HAND_RIGHT]._itype == ItemType::Mace) {
		phanditype = ItemType::Mace;
	}

	switch (monster.MData->mMonstClass) {
	case MonsterClass::Undead:
		if (phanditype == ItemType::Sword) {
			dam -= dam / 2;
		} else if (phanditype == ItemType::Mace) {
			dam += dam / 2;
		}
		break;
	case MonsterClass::Animal:
		if (phanditype == ItemType::Mace) {
			dam -= dam / 2;
		} else if (phanditype == ItemType::Sword) {
			dam += dam / 2;
		}
		break;
	case MonsterClass::Demon:
		if (HasAnyOf(player._pIFlags, ItemSpecialEffect::TripleDemonDamage)) {
			dam *= 3;
		}
		break;
	}

	if (HasAnyOf(player.pDamAcFlags, ItemSpecialEffectHf::Devastation) && GenerateRnd(100) < 5) {
		dam *= 3;
	}

	if (HasAnyOf(player.pDamAcFlags, ItemSpecialEffectHf::Doppelganger) && monster.MType->mtype != MT_DIABLO && monster._uniqtype == 0 && GenerateRnd(100) < 10) {
		AddDoppelganger(monster);
	}

	dam <<= 6;
	if (HasAnyOf(player.pDamAcFlags, ItemSpecialEffectHf::Jesters)) {
		int r = GenerateRnd(201);
		if (r >= 100)
			r = 100 + (r - 100) * 5;
		dam = dam * r / 100;
	}

	if (adjacentDamage)
		dam >>= 2;

	if (pnum == MyPlayerId) {
		if (HasAnyOf(player.pDamAcFlags, ItemSpecialEffectHf::Peril)) {
			dam2 += player._pIGetHit << 6;
			if (dam2 >= 0) {
				ApplyPlrDamage(pnum, 0, 1, dam2);
			}
			dam *= 2;
		}
		monster._mhitpoints -= dam;
	}

	int skdam = 0;
	if (HasAnyOf(player._pIFlags, ItemSpecialEffect::RandomStealLife)) {
		skdam = GenerateRnd(dam / 8);
		player._pHitPoints += skdam;
		if (player._pHitPoints > player._pMaxHP) {
			player._pHitPoints = player._pMaxHP;
		}
		player._pHPBase += skdam;
		if (player._pHPBase > player._pMaxHPBase) {
			player._pHPBase = player._pMaxHPBase;
		}
		drawhpflag = true;
	}
	if (HasAnyOf(player._pIFlags, ItemSpecialEffect::StealMana3 | ItemSpecialEffect::StealMana5) && HasNoneOf(player._pIFlags, ItemSpecialEffect::NoMana)) {
		if (HasAnyOf(player._pIFlags, ItemSpecialEffect::StealMana3)) {
			skdam = 3 * dam / 100;
		}
		if (HasAnyOf(player._pIFlags, ItemSpecialEffect::StealMana5)) {
			skdam = 5 * dam / 100;
		}
		player._pMana += skdam;
		if (player._pMana > player._pMaxMana) {
			player._pMana = player._pMaxMana;
		}
		player._pManaBase += skdam;
		if (player._pManaBase > player._pMaxManaBase) {
			player._pManaBase = player._pMaxManaBase;
		}
		drawmanaflag = true;
	}
	if (HasAnyOf(player._pIFlags, ItemSpecialEffect::StealLife3 | ItemSpecialEffect::StealLife5)) {
		if (HasAnyOf(player._pIFlags, ItemSpecialEffect::StealLife3)) {
			skdam = 3 * dam / 100;
		}
		if (HasAnyOf(player._pIFlags, ItemSpecialEffect::StealLife5)) {
			skdam = 5 * dam / 100;
		}
		player._pHitPoints += skdam;
		if (player._pHitPoints > player._pMaxHP) {
			player._pHitPoints = player._pMaxHP;
		}
		player._pHPBase += skdam;
		if (player._pHPBase > player._pMaxHPBase) {
			player._pHPBase = player._pMaxHPBase;
		}
		drawhpflag = true;
	}
	if (HasAnyOf(player._pIFlags, ItemSpecialEffect::NoHealOnPlayer)) { // Why is there a different ItemSpecialEffect here? (see missile.cpp) is this a BUG?
		monster._mFlags |= MFLAG_NOHEAL;
	}
#ifdef _DEBUG
	if (DebugGodMode) {
		monster._mhitpoints = 0; /* double check */
	}
#endif
	if ((monster._mhitpoints >> 6) <= 0) {
		if (monster._mmode == MonsterMode::Petrified) {
			M_StartKill(m, pnum);
			monster.Petrify();
		} else {
			M_StartKill(m, pnum);
		}
	} else {
		if (monster._mmode == MonsterMode::Petrified) {
			M_StartHit(m, pnum, dam);
			monster.Petrify();
		} else {
			if (HasAnyOf(player._pIFlags, ItemSpecialEffect::Knockback)) {
				M_GetKnockback(m);
			}
			M_StartHit(m, pnum, dam);
		}
	}

	return true;
}

bool PlrHitPlr(int pnum, int8_t p)
{
	if ((DWORD)p >= MAX_PLRS) {
		app_fatal("PlrHitPlr: illegal target player %i", p);
	}
	auto &target = Players[p];

	if (target._pInvincible) {
		return false;
	}

	if (HasAnyOf(target._pSpellFlags, SpellFlag::Etherealize)) {
		return false;
	}

	if ((DWORD)pnum >= MAX_PLRS) {
		app_fatal("PlrHitPlr: illegal attacking player %i", pnum);
	}
	auto &attacker = Players[pnum];

	int hit = GenerateRnd(100);

	int hper = attacker.GetMeleeToHit() - target.GetArmor();
	hper = clamp(hper, 5, 95);

	int blk = 100;
	if ((target._pmode == PM_STAND || target._pmode == PM_ATTACK) && target._pBlockFlag) {
		blk = GenerateRnd(100);
	}

	int blkper = target.GetBlockChance() - (attacker._pLevel * 2);
	blkper = clamp(blkper, 0, 100);

	if (hit >= hper) {
		return false;
	}

	if (blk < blkper) {
		Direction dir = GetDirection(target.position.tile, attacker.position.tile);
		StartPlrBlock(p, dir);
		return true;
	}

	int mind = attacker._pIMinDam;
	int maxd = attacker._pIMaxDam;
	int dam = GenerateRnd(maxd - mind + 1) + mind;
	dam += (dam * attacker._pIBonusDam) / 100;
	dam += attacker._pIBonusDamMod + attacker._pDamageMod;

	if (attacker._pClass == HeroClass::Warrior || attacker._pClass == HeroClass::Barbarian) {
		if (GenerateRnd(100) < attacker._pLevel) {
			dam *= 2;
		}
	}
	int skdam = dam << 6;
	if (HasAnyOf(attacker._pIFlags, ItemSpecialEffect::RandomStealLife)) {
		int tac = GenerateRnd(skdam / 8);
		attacker._pHitPoints += tac;
		if (attacker._pHitPoints > attacker._pMaxHP) {
			attacker._pHitPoints = attacker._pMaxHP;
		}
		attacker._pHPBase += tac;
		if (attacker._pHPBase > attacker._pMaxHPBase) {
			attacker._pHPBase = attacker._pMaxHPBase;
		}
		drawhpflag = true;
	}
	if (pnum == MyPlayerId) {
		NetSendCmdDamage(true, p, skdam);
	}
	StartPlrHit(p, skdam, false);

	return true;
}

bool PlrHitObj(int pnum, Object &targetObject)
{
	if (targetObject.IsBreakable()) {
		BreakObject(pnum, targetObject);
		return true;
	}

	return false;
}

bool DoAttack(int pnum)
{
	if ((DWORD)pnum >= MAX_PLRS) {
		app_fatal("PM_DoAttack: illegal player %i", pnum);
	}
	auto &player = Players[pnum];

	if (player.AnimInfo.CurrentFrame == player._pAFNum - 2) {
		PlaySfxLoc(PS_SWING, player.position.tile);
	}

	bool didhit = false;

	if (player.AnimInfo.CurrentFrame == player._pAFNum - 1) {
		Point position = player.position.tile + player._pdir;
		int dx = position.x;
		int dy = position.y;

		if (dMonster[dx][dy] != 0) {
			int m = -1;
			if (dMonster[dx][dy] > 0) {
				m = dMonster[dx][dy] - 1;
			} else {
				m = -(dMonster[dx][dy] + 1);
			}
			if (CanTalkToMonst(Monsters[m])) {
				player.position.temp.x = 0; /** @todo Looks to be irrelevant, probably just remove it */
				return false;
			}
		}

		if (!HasAllOf(player._pIFlags, ItemSpecialEffect::FireDamage | ItemSpecialEffect::LightningDamage)) {
			if (HasAnyOf(player._pIFlags, ItemSpecialEffect::FireDamage)) {
				AddMissile(position, { 1, 0 }, Direction::South, MIS_WEAPEXP, TARGET_MONSTERS, pnum, 0, 0);
			} else if (HasAnyOf(player._pIFlags, ItemSpecialEffect::LightningDamage)) {
				AddMissile(position, { 2, 0 }, Direction::South, MIS_WEAPEXP, TARGET_MONSTERS, pnum, 0, 0);
			}
		}

		if (dMonster[dx][dy] != 0) {
			int m = dMonster[dx][dy];
			if (dMonster[dx][dy] > 0) {
				m = dMonster[dx][dy] - 1;
			} else {
				m = -(dMonster[dx][dy] + 1);
			}
			didhit = PlrHitMonst(pnum, m);
		} else if (dPlayer[dx][dy] != 0 && !gbFriendlyMode) {
			BYTE p = dPlayer[dx][dy];
			if (dPlayer[dx][dy] > 0) {
				p = dPlayer[dx][dy] - 1;
			} else {
				p = -(dPlayer[dx][dy] + 1);
			}
			didhit = PlrHitPlr(pnum, p);
		} else {
			Object *object = ObjectAtPosition(position, false);
			if (object != nullptr) {
				didhit = PlrHitObj(pnum, *object);
			}
		}
		if ((player._pClass == HeroClass::Monk
		        && (player.InvBody[INVLOC_HAND_LEFT]._itype == ItemType::Staff || player.InvBody[INVLOC_HAND_RIGHT]._itype == ItemType::Staff))
		    || (player._pClass == HeroClass::Bard
		        && player.InvBody[INVLOC_HAND_LEFT]._itype == ItemType::Sword && player.InvBody[INVLOC_HAND_RIGHT]._itype == ItemType::Sword)
		    || (player._pClass == HeroClass::Barbarian
		        && (player.InvBody[INVLOC_HAND_LEFT]._itype == ItemType::Axe || player.InvBody[INVLOC_HAND_RIGHT]._itype == ItemType::Axe
		            || (((player.InvBody[INVLOC_HAND_LEFT]._itype == ItemType::Mace && player.InvBody[INVLOC_HAND_LEFT]._iLoc == ILOC_TWOHAND)
		                    || (player.InvBody[INVLOC_HAND_RIGHT]._itype == ItemType::Mace && player.InvBody[INVLOC_HAND_RIGHT]._iLoc == ILOC_TWOHAND)
		                    || (player.InvBody[INVLOC_HAND_LEFT]._itype == ItemType::Sword && player.InvBody[INVLOC_HAND_LEFT]._iLoc == ILOC_TWOHAND)
		                    || (player.InvBody[INVLOC_HAND_RIGHT]._itype == ItemType::Sword && player.InvBody[INVLOC_HAND_RIGHT]._iLoc == ILOC_TWOHAND))
		                && !(player.InvBody[INVLOC_HAND_LEFT]._itype == ItemType::Shield || player.InvBody[INVLOC_HAND_RIGHT]._itype == ItemType::Shield))))) {
			// playing as a class/weapon with cleave
			position = player.position.tile + Right(player._pdir);
			if (dMonster[position.x][position.y] != 0) {
				int m = abs(dMonster[position.x][position.y]) - 1;
				auto &monster = Monsters[m];
				if (!CanTalkToMonst(monster) && monster.position.old == position) {
					if (PlrHitMonst(pnum, m, true))
						didhit = true;
				}
			}
			position = player.position.tile + Left(player._pdir);
			if (dMonster[position.x][position.y] != 0) {
				int m = abs(dMonster[position.x][position.y]) - 1;
				auto &monster = Monsters[m];
				if (!CanTalkToMonst(monster) && monster.position.old == position) {
					if (PlrHitMonst(pnum, m, true))
						didhit = true;
				}
			}
		}

		if (didhit && DamageWeapon(pnum, 30)) {
			StartStand(pnum, player._pdir);
			ClearStateVariables(player);
			return true;
		}
	}

	if (player.AnimInfo.CurrentFrame == player._pAFrames - 1) {
		StartStand(pnum, player._pdir);
		ClearStateVariables(player);
		return true;
	}

	return false;
}

bool DoRangeAttack(int pnum)
{
	if ((DWORD)pnum >= MAX_PLRS) {
		app_fatal("PM_DoRangeAttack: illegal player %i", pnum);
	}
	auto &player = Players[pnum];

	int arrows = 0;
	if (player.AnimInfo.CurrentFrame == player._pAFNum - 1) {
		arrows = 1;
	}

	if (HasAnyOf(player._pIFlags, ItemSpecialEffect::MultipleArrows) && player.AnimInfo.CurrentFrame == player._pAFNum + 1) {
		arrows = 2;
	}

	for (int arrow = 0; arrow < arrows; arrow++) {
		int xoff = 0;
		int yoff = 0;
		if (arrows != 1) {
			int angle = arrow == 0 ? -1 : 1;
			int x = player.position.temp.x - player.position.tile.x;
			if (x != 0)
				yoff = x < 0 ? angle : -angle;
			int y = player.position.temp.y - player.position.tile.y;
			if (y != 0)
				xoff = y < 0 ? -angle : angle;
		}

		int dmg = 4;
		missile_id mistype = MIS_ARROW;
		if (HasAnyOf(player._pIFlags, ItemSpecialEffect::FireArrows)) {
			mistype = MIS_FARROW;
		}
		if (HasAnyOf(player._pIFlags, ItemSpecialEffect::LightningArrows)) {
			mistype = MIS_LARROW;
		}
		if (HasAllOf(player._pIFlags, ItemSpecialEffect::FireArrows | ItemSpecialEffect::LightningArrows)) {
			dmg = player._pIFMinDam + GenerateRnd(player._pIFMaxDam - player._pIFMinDam);
			mistype = MIS_SPECARROW;
		}

		AddMissile(
		    player.position.tile,
		    player.position.temp + Displacement { xoff, yoff },
		    player._pdir,
		    mistype,
		    TARGET_MONSTERS,
		    pnum,
		    dmg,
		    0);

		if (arrow == 0 && mistype != MIS_SPECARROW) {
			PlaySfxLoc(arrows != 1 ? IS_STING1 : PS_BFIRE, player.position.tile);
		}

		if (DamageWeapon(pnum, 40)) {
			StartStand(pnum, player._pdir);
			ClearStateVariables(player);
			return true;
		}
	}

	if (player.AnimInfo.CurrentFrame >= player._pAFrames - 1) {
		StartStand(pnum, player._pdir);
		ClearStateVariables(player);
		return true;
	}
	return false;
}

void DamageParryItem(int pnum)
{
	if (pnum != MyPlayerId) {
		return;
	}

	if ((DWORD)pnum >= MAX_PLRS) {
		app_fatal("DamageParryItem: illegal player %i", pnum);
	}
	auto &player = Players[pnum];

	if (player.InvBody[INVLOC_HAND_LEFT]._itype == ItemType::Shield || player.InvBody[INVLOC_HAND_LEFT]._itype == ItemType::Staff) {
		if (player.InvBody[INVLOC_HAND_LEFT]._iDurability == DUR_INDESTRUCTIBLE) {
			return;
		}

		player.InvBody[INVLOC_HAND_LEFT]._iDurability--;
		if (player.InvBody[INVLOC_HAND_LEFT]._iDurability == 0) {
			RemoveEquipment(player, INVLOC_HAND_LEFT, true);
			CalcPlrInv(player, true);
		}
	}

	if (player.InvBody[INVLOC_HAND_RIGHT]._itype == ItemType::Shield) {
		if (player.InvBody[INVLOC_HAND_RIGHT]._iDurability != DUR_INDESTRUCTIBLE) {
			player.InvBody[INVLOC_HAND_RIGHT]._iDurability--;
			if (player.InvBody[INVLOC_HAND_RIGHT]._iDurability == 0) {
				RemoveEquipment(player, INVLOC_HAND_RIGHT, true);
				CalcPlrInv(player, true);
			}
		}
	}
}

bool DoBlock(int pnum)
{
	if ((DWORD)pnum >= MAX_PLRS) {
		app_fatal("PM_DoBlock: illegal player %i", pnum);
	}
	auto &player = Players[pnum];

	if (player.AnimInfo.CurrentFrame >= player._pBFrames - 1) {
		StartStand(pnum, player._pdir);
		ClearStateVariables(player);

		if (GenerateRnd(10) == 0) {
			DamageParryItem(pnum);
		}
		return true;
	}

	return false;
}

void DamageArmor(int pnum)
{
	int a;
	Item *pi;

	if (pnum != MyPlayerId) {
		return;
	}

	if ((DWORD)pnum >= MAX_PLRS) {
		app_fatal("DamageArmor: illegal player %i", pnum);
	}
	auto &player = Players[pnum];

	if (player.InvBody[INVLOC_CHEST].isEmpty() && player.InvBody[INVLOC_HEAD].isEmpty()) {
		return;
	}

	a = GenerateRnd(3);
	if (!player.InvBody[INVLOC_CHEST].isEmpty() && player.InvBody[INVLOC_HEAD].isEmpty()) {
		a = 1;
	}
	if (player.InvBody[INVLOC_CHEST].isEmpty() && !player.InvBody[INVLOC_HEAD].isEmpty()) {
		a = 0;
	}

	if (a != 0) {
		pi = &player.InvBody[INVLOC_CHEST];
	} else {
		pi = &player.InvBody[INVLOC_HEAD];
	}
	if (pi->_iDurability == DUR_INDESTRUCTIBLE) {
		return;
	}

	pi->_iDurability--;
	if (pi->_iDurability != 0) {
		return;
	}

	if (a != 0) {
		RemoveEquipment(player, INVLOC_CHEST, true);
	} else {
		RemoveEquipment(player, INVLOC_HEAD, true);
	}
	CalcPlrInv(player, true);
}

bool DoSpell(int pnum)
{
	if ((DWORD)pnum >= MAX_PLRS) {
		app_fatal("PM_DoSpell: illegal player %i", pnum);
	}
	auto &player = Players[pnum];

	if (player.AnimInfo.CurrentFrame == player._pSFNum) {
		CastSpell(
		    pnum,
		    player._pSpell,
		    player.position.tile.x,
		    player.position.tile.y,
		    player.position.temp.x,
		    player.position.temp.y,
		    player.spellLevel);

		if (player._pSplFrom == 0) {
			EnsureValidReadiedSpell(player);
		}
	}

	if (player.AnimInfo.CurrentFrame >= player._pSFrames - 1) {
		StartStand(pnum, player._pdir);
		ClearStateVariables(player);
		return true;
	}

	return false;
}

bool DoGotHit(int pnum)
{
	if ((DWORD)pnum >= MAX_PLRS) {
		app_fatal("PM_DoGotHit: illegal player %i", pnum);
	}
	auto &player = Players[pnum];

	if (player.AnimInfo.CurrentFrame >= player._pHFrames - 1) {
		StartStand(pnum, player._pdir);
		ClearStateVariables(player);
		if (GenerateRnd(4) != 0) {
			DamageArmor(pnum);
		}

		return true;
	}

	return false;
}

bool DoDeath(int pnum)
{
	if ((DWORD)pnum >= MAX_PLRS) {
		app_fatal("PM_DoDeath: illegal player %i", pnum);
	}
	auto &player = Players[pnum];

	if (player.AnimInfo.CurrentFrame == player.AnimInfo.NumberOfFrames - 1) {
		if (player.AnimInfo.TickCounterOfCurrentFrame == 0) {
			player.AnimInfo.TicksPerFrame = 1000000000;
			dFlags[player.position.tile.x][player.position.tile.y] |= DungeonFlag::DeadPlayer;
		} else if (pnum == MyPlayerId && player.AnimInfo.TickCounterOfCurrentFrame == 30) {
			MyPlayerIsDead = true;
			if (!gbIsMultiplayer) {
				gamemenu_on();
			}
		}
	}

	return false;
}

bool IsPlayerAdjacentToObject(Player &player, Object &object)
{
	int x = abs(player.position.tile.x - object.position.x);
	int y = abs(player.position.tile.y - object.position.y);
	if (y > 1 && object.position.y >= 1 && ObjectAtPosition(object.position + Direction::NorthEast) == &object) {
		// special case for activating a large object from the north-east side
		y = abs(player.position.tile.y - object.position.y + 1);
	}
	return x <= 1 && y <= 1;
}

void CheckNewPath(int pnum, bool pmWillBeCalled)
{
	if ((DWORD)pnum >= MAX_PLRS) {
		app_fatal("CheckNewPath: illegal player %i", pnum);
	}
	auto &player = Players[pnum];

	int x = 0;
	int y = 0;

	Monster *monster;
	Player *target;
	Object *object;
	Item *item;

	int targetId = player.destParam1;

	switch (player.destAction) {
	case ACTION_ATTACKMON:
	case ACTION_RATTACKMON:
	case ACTION_SPELLMON:
		monster = &Monsters[targetId];
		if ((monster->_mhitpoints >> 6) <= 0) {
			player.Stop();
			return;
		}
		if (player.destAction == ACTION_ATTACKMON)
			MakePlrPath(player, monster->position.future, false);
		break;
	case ACTION_ATTACKPLR:
	case ACTION_RATTACKPLR:
	case ACTION_SPELLPLR:
		target = &Players[targetId];
		if ((target->_pHitPoints >> 6) <= 0) {
			player.Stop();
			return;
		}
		if (player.destAction == ACTION_ATTACKPLR)
			MakePlrPath(player, target->position.future, false);
		break;
	case ACTION_OPERATE:
	case ACTION_DISARM:
	case ACTION_OPERATETK:
		object = &Objects[targetId];
		break;
	case ACTION_PICKUPITEM:
	case ACTION_PICKUPAITEM:
		item = &Items[targetId];
		break;
	default:
		break;
	}

	Direction d;
	if (player.walkpath[0] != WALK_NONE) {
		if (player._pmode == PM_STAND) {
			if (pnum == MyPlayerId) {
				if (player.destAction == ACTION_ATTACKMON || player.destAction == ACTION_ATTACKPLR) {
					if (player.destAction == ACTION_ATTACKMON) {
						x = abs(player.position.future.x - monster->position.future.x);
						y = abs(player.position.future.y - monster->position.future.y);
						d = GetDirection(player.position.future, monster->position.future);
					} else {
						x = abs(player.position.future.x - target->position.future.x);
						y = abs(player.position.future.y - target->position.future.y);
						d = GetDirection(player.position.future, target->position.future);
					}

					if (x < 2 && y < 2) {
						ClrPlrPath(player);
						if (player.destAction == ACTION_ATTACKMON && monster->mtalkmsg != TEXT_NONE && monster->mtalkmsg != TEXT_VILE14) {
							TalktoMonster(*monster);
						} else {
							StartAttack(pnum, d);
						}
						player.destAction = ACTION_NONE;
					}
				}
			}

			int xvel3 = 2048;
			int xvel = 1024;
			int yvel = 512;
			if (currlevel != 0) {
				xvel3 = PWVel[static_cast<std::size_t>(player._pClass)][0];
				xvel = PWVel[static_cast<std::size_t>(player._pClass)][1];
				yvel = PWVel[static_cast<std::size_t>(player._pClass)][2];
			}

			switch (player.walkpath[0]) {
			case WALK_N:
				StartWalk(pnum, { 0, -xvel }, Direction::North, pmWillBeCalled);
				break;
			case WALK_NE:
				StartWalk(pnum, { xvel, -yvel }, Direction::NorthEast, pmWillBeCalled);
				break;
			case WALK_E:
				StartWalk(pnum, { xvel3, 0 }, Direction::East, pmWillBeCalled);
				break;
			case WALK_SE:
				StartWalk(pnum, { xvel, yvel }, Direction::SouthEast, pmWillBeCalled);
				break;
			case WALK_S:
				StartWalk(pnum, { 0, xvel }, Direction::South, pmWillBeCalled);
				break;
			case WALK_SW:
				StartWalk(pnum, { -xvel, yvel }, Direction::SouthWest, pmWillBeCalled);
				break;
			case WALK_W:
				StartWalk(pnum, { -xvel3, 0 }, Direction::West, pmWillBeCalled);
				break;
			case WALK_NW:
				StartWalk(pnum, { -xvel, -yvel }, Direction::NorthWest, pmWillBeCalled);
				break;
			}

			for (int j = 1; j < MAX_PATH_LENGTH; j++) {
				player.walkpath[j - 1] = player.walkpath[j];
			}

			player.walkpath[MAX_PATH_LENGTH - 1] = WALK_NONE;

			if (player._pmode == PM_STAND) {
				StartStand(pnum, player._pdir);
				player.destAction = ACTION_NONE;
			}
		}

		return;
	}
	if (player.destAction == ACTION_NONE) {
		return;
	}

	if (player._pmode == PM_STAND) {
		switch (player.destAction) {
		case ACTION_ATTACK:
			d = GetDirection(player.position.tile, { player.destParam1, player.destParam2 });
			StartAttack(pnum, d);
			break;
		case ACTION_ATTACKMON:
			x = abs(player.position.tile.x - monster->position.future.x);
			y = abs(player.position.tile.y - monster->position.future.y);
			if (x <= 1 && y <= 1) {
				d = GetDirection(player.position.future, monster->position.future);
				if (monster->mtalkmsg != TEXT_NONE && monster->mtalkmsg != TEXT_VILE14) {
					TalktoMonster(*monster);
				} else {
					StartAttack(pnum, d);
				}
			}
			break;
		case ACTION_ATTACKPLR:
			x = abs(player.position.tile.x - target->position.future.x);
			y = abs(player.position.tile.y - target->position.future.y);
			if (x <= 1 && y <= 1) {
				d = GetDirection(player.position.future, target->position.future);
				StartAttack(pnum, d);
			}
			break;
		case ACTION_RATTACK:
			d = GetDirection(player.position.tile, { player.destParam1, player.destParam2 });
			StartRangeAttack(pnum, d, player.destParam1, player.destParam2);
			break;
		case ACTION_RATTACKMON:
			d = GetDirection(player.position.future, monster->position.future);
			if (monster->mtalkmsg != TEXT_NONE && monster->mtalkmsg != TEXT_VILE14) {
				TalktoMonster(*monster);
			} else {
				StartRangeAttack(pnum, d, monster->position.future.x, monster->position.future.y);
			}
			break;
		case ACTION_RATTACKPLR:
			d = GetDirection(player.position.future, target->position.future);
			StartRangeAttack(pnum, d, target->position.future.x, target->position.future.y);
			break;
		case ACTION_SPELL:
			d = GetDirection(player.position.tile, { player.destParam1, player.destParam2 });
			StartSpell(pnum, d, player.destParam1, player.destParam2);
			player.spellLevel = player.destParam3;
			break;
		case ACTION_SPELLWALL:
			StartSpell(pnum, static_cast<Direction>(player.destParam3), player.destParam1, player.destParam2);
			player.tempDirection = static_cast<Direction>(player.destParam3);
			player.spellLevel = player.destParam4;
			break;
		case ACTION_SPELLMON:
			d = GetDirection(player.position.tile, monster->position.future);
			StartSpell(pnum, d, monster->position.future.x, monster->position.future.y);
			player.spellLevel = player.destParam2;
			break;
		case ACTION_SPELLPLR:
			d = GetDirection(player.position.tile, target->position.future);
			StartSpell(pnum, d, target->position.future.x, target->position.future.y);
			player.spellLevel = player.destParam2;
			break;
		case ACTION_OPERATE:
			if (IsPlayerAdjacentToObject(player, *object)) {
				if (object->_oBreak == 1) {
					d = GetDirection(player.position.tile, object->position);
					StartAttack(pnum, d);
				} else {
					OperateObject(pnum, targetId, false);
				}
			}
			break;
		case ACTION_DISARM:
			if (IsPlayerAdjacentToObject(player, *object)) {
				if (object->_oBreak == 1) {
					d = GetDirection(player.position.tile, object->position);
					StartAttack(pnum, d);
				} else {
					TryDisarm(pnum, targetId);
					OperateObject(pnum, targetId, false);
				}
			}
			break;
		case ACTION_OPERATETK:
			if (object->_oBreak != 1) {
				OperateObject(pnum, targetId, true);
			}
			break;
		case ACTION_PICKUPITEM:
			if (pnum == MyPlayerId) {
				x = abs(player.position.tile.x - item->position.x);
				y = abs(player.position.tile.y - item->position.y);
				if (x <= 1 && y <= 1 && pcurs == CURSOR_HAND && !item->_iRequest) {
					NetSendCmdGItem(true, CMD_REQUESTGITEM, pnum, pnum, targetId);
					item->_iRequest = true;
				}
			}
			break;
		case ACTION_PICKUPAITEM:
			if (pnum == MyPlayerId) {
				x = abs(player.position.tile.x - item->position.x);
				y = abs(player.position.tile.y - item->position.y);
				if (x <= 1 && y <= 1 && pcurs == CURSOR_HAND) {
					NetSendCmdGItem(true, CMD_REQUESTAGITEM, pnum, pnum, targetId);
				}
			}
			break;
		case ACTION_TALK:
			if (pnum == MyPlayerId) {
				TalkToTowner(player, player.destParam1);
			}
			break;
		default:
			break;
		}

		FixPlayerLocation(pnum, player._pdir);
		player.destAction = ACTION_NONE;

		return;
	}

	if (player._pmode == PM_ATTACK && player.AnimInfo.CurrentFrame >= player._pAFNum) {
		if (player.destAction == ACTION_ATTACK) {
			d = GetDirection(player.position.future, { player.destParam1, player.destParam2 });
			StartAttack(pnum, d);
			player.destAction = ACTION_NONE;
		} else if (player.destAction == ACTION_ATTACKMON) {
			x = abs(player.position.tile.x - monster->position.future.x);
			y = abs(player.position.tile.y - monster->position.future.y);
			if (x <= 1 && y <= 1) {
				d = GetDirection(player.position.future, monster->position.future);
				StartAttack(pnum, d);
			}
			player.destAction = ACTION_NONE;
		} else if (player.destAction == ACTION_ATTACKPLR) {
			x = abs(player.position.tile.x - target->position.future.x);
			y = abs(player.position.tile.y - target->position.future.y);
			if (x <= 1 && y <= 1) {
				d = GetDirection(player.position.future, target->position.future);
				StartAttack(pnum, d);
			}
			player.destAction = ACTION_NONE;
		} else if (player.destAction == ACTION_OPERATE) {
			if (IsPlayerAdjacentToObject(player, *object)) {
				if (object->_oBreak == 1) {
					d = GetDirection(player.position.tile, object->position);
					StartAttack(pnum, d);
				}
			}
		}
	}

	if (player._pmode == PM_RATTACK && player.AnimInfo.CurrentFrame >= player._pAFNum) {
		if (player.destAction == ACTION_RATTACK) {
			d = GetDirection(player.position.tile, { player.destParam1, player.destParam2 });
			StartRangeAttack(pnum, d, player.destParam1, player.destParam2);
			player.destAction = ACTION_NONE;
		} else if (player.destAction == ACTION_RATTACKMON) {
			d = GetDirection(player.position.tile, monster->position.future);
			StartRangeAttack(pnum, d, monster->position.future.x, monster->position.future.y);
			player.destAction = ACTION_NONE;
		} else if (player.destAction == ACTION_RATTACKPLR) {
			d = GetDirection(player.position.tile, target->position.future);
			StartRangeAttack(pnum, d, target->position.future.x, target->position.future.y);
			player.destAction = ACTION_NONE;
		}
	}

	if (player._pmode == PM_SPELL && player.AnimInfo.CurrentFrame >= player._pSFNum) {
		if (player.destAction == ACTION_SPELL) {
			d = GetDirection(player.position.tile, { player.destParam1, player.destParam2 });
			StartSpell(pnum, d, player.destParam1, player.destParam2);
			player.destAction = ACTION_NONE;
		} else if (player.destAction == ACTION_SPELLMON) {
			d = GetDirection(player.position.tile, monster->position.future);
			StartSpell(pnum, d, monster->position.future.x, monster->position.future.y);
			player.destAction = ACTION_NONE;
		} else if (player.destAction == ACTION_SPELLPLR) {
			d = GetDirection(player.position.tile, target->position.future);
			StartSpell(pnum, d, target->position.future.x, target->position.future.y);
			player.destAction = ACTION_NONE;
		}
	}
}

bool PlrDeathModeOK(int p)
{
	if (p != MyPlayerId) {
		return true;
	}

	if ((DWORD)p >= MAX_PLRS) {
		app_fatal("PlrDeathModeOK: illegal player %i", p);
	}
	auto &player = Players[p];

	if (player._pmode == PM_DEATH) {
		return true;
	}
	if (player._pmode == PM_QUIT) {
		return true;
	}
	if (player._pmode == PM_NEWLVL) {
		return true;
	}

	return false;
}

void ValidatePlayer()
{
	if ((DWORD)MyPlayerId >= MAX_PLRS) {
		app_fatal("ValidatePlayer: illegal player %i", MyPlayerId);
	}
	auto &myPlayer = Players[MyPlayerId];

	if (myPlayer._pLevel > MAXCHARLEVEL)
		myPlayer._pLevel = MAXCHARLEVEL;
	if (myPlayer._pExperience > myPlayer._pNextExper) {
		myPlayer._pExperience = myPlayer._pNextExper;
		if (*sgOptions.Gameplay.experienceBar) {
			force_redraw = 255;
		}
	}

	int gt = 0;
	for (int i = 0; i < myPlayer._pNumInv; i++) {
		if (myPlayer.InvList[i]._itype == ItemType::Gold) {
			int maxGold = GOLD_MAX_LIMIT;
			if (gbIsHellfire) {
				maxGold *= 2;
			}
			if (myPlayer.InvList[i]._ivalue > maxGold) {
				myPlayer.InvList[i]._ivalue = maxGold;
			}
			gt += myPlayer.InvList[i]._ivalue;
		}
	}
	if (gt != myPlayer._pGold)
		myPlayer._pGold = gt;

	if (myPlayer._pBaseStr > myPlayer.GetMaximumAttributeValue(CharacterAttribute::Strength)) {
		myPlayer._pBaseStr = myPlayer.GetMaximumAttributeValue(CharacterAttribute::Strength);
	}
	if (myPlayer._pBaseMag > myPlayer.GetMaximumAttributeValue(CharacterAttribute::Magic)) {
		myPlayer._pBaseMag = myPlayer.GetMaximumAttributeValue(CharacterAttribute::Magic);
	}
	if (myPlayer._pBaseDex > myPlayer.GetMaximumAttributeValue(CharacterAttribute::Dexterity)) {
		myPlayer._pBaseDex = myPlayer.GetMaximumAttributeValue(CharacterAttribute::Dexterity);
	}
	if (myPlayer._pBaseVit > myPlayer.GetMaximumAttributeValue(CharacterAttribute::Vitality)) {
		myPlayer._pBaseVit = myPlayer.GetMaximumAttributeValue(CharacterAttribute::Vitality);
	}

	uint64_t msk = 0;
	for (int b = SPL_FIREBOLT; b < MAX_SPELLS; b++) {
		if (GetSpellBookLevel((spell_id)b) != -1) {
			msk |= GetSpellBitmask(b);
			if (myPlayer._pSplLvl[b] > MAX_SPELL_LEVEL)
				myPlayer._pSplLvl[b] = MAX_SPELL_LEVEL;
		}
	}

	myPlayer._pMemSpells &= msk;
}

void CheckCheatStats(Player &player)
{
	if (player._pStrength > 750) {
		player._pStrength = 750;
	}

	if (player._pDexterity > 750) {
		player._pDexterity = 750;
	}

	if (player._pMagic > 750) {
		player._pMagic = 750;
	}

	if (player._pVitality > 750) {
		player._pVitality = 750;
	}

	if (player._pHitPoints > 128000) {
		player._pHitPoints = 128000;
	}

	if (player._pMana > 128000) {
		player._pMana = 128000;
	}
}

} // namespace

void Player::CalcScrolls()
{
	_pScrlSpells = 0;
	for (Item &item : InventoryAndBeltPlayerItemsRange { *this }) {
		if (item.IsScroll() && item._iStatFlag) {
			_pScrlSpells |= GetSpellBitmask(item._iSpell);
		}
	}
	EnsureValidReadiedSpell(*this);
}

bool Player::HasItem(int item, int *idx) const
{
	for (int i = 0; i < _pNumInv; i++) {
		if (InvList[i].IDidx == item) {
			if (idx != nullptr)
				*idx = i;
			return true;
		}
	}

	return false;
}

void Player::RemoveInvItem(int iv, bool calcScrolls)
{
	// Iterate through invGrid and remove every reference to item
	for (int8_t &itemIndex : InvGrid) {
		if (abs(itemIndex) - 1 == iv) {
			itemIndex = 0;
		}
	}

	InvList[iv].Clear();

	_pNumInv--;

	// If the item at the end of inventory array isn't the one we removed, we need to swap its position in the array with the removed item
	if (_pNumInv > 0 && _pNumInv != iv) {
		InvList[iv] = std::move(InvList[_pNumInv]);
		InvList[_pNumInv].Clear();

		for (int8_t &itemIndex : InvGrid) {
			if (itemIndex == _pNumInv + 1) {
				itemIndex = iv + 1;
			}
			if (itemIndex == -(_pNumInv + 1)) {
				itemIndex = -(iv + 1);
			}
		}
	}

	if (calcScrolls) {
		CalcScrolls();
	}
}

bool Player::TryRemoveInvItemById(int item)
{
	int idx;
	if (HasItem(item, &idx)) {
		RemoveInvItem(idx);
		return true;
	}
	return false;
}

void Player::RemoveSpdBarItem(int iv)
{
	SpdList[iv].Clear();

	CalcScrolls();
	force_redraw = 255;
}

int Player::GetBaseAttributeValue(CharacterAttribute attribute) const
{
	switch (attribute) {
	case CharacterAttribute::Dexterity:
		return this->_pBaseDex;
	case CharacterAttribute::Magic:
		return this->_pBaseMag;
	case CharacterAttribute::Strength:
		return this->_pBaseStr;
	case CharacterAttribute::Vitality:
		return this->_pBaseVit;
	default:
		app_fatal("Unsupported attribute");
	}
}

int Player::GetCurrentAttributeValue(CharacterAttribute attribute) const
{
	switch (attribute) {
	case CharacterAttribute::Dexterity:
		return this->_pDexterity;
	case CharacterAttribute::Magic:
		return this->_pMagic;
	case CharacterAttribute::Strength:
		return this->_pStrength;
	case CharacterAttribute::Vitality:
		return this->_pVitality;
	default:
		app_fatal("Unsupported attribute");
	}
}

int Player::GetMaximumAttributeValue(CharacterAttribute attribute) const
{
	static const int MaxStats[enum_size<HeroClass>::value][enum_size<CharacterAttribute>::value] = {
		// clang-format off
		{ 255,  30,  60, 120 },
		{  50,  75, 255,  85 },
		{   5, 255,  30,  80 },
		{ 150,  80, 150,  80 },
		{ 120, 120, 120, 100 },
		{ 255,   0,  55, 150 },
		// clang-format on
	};

	return MaxStats[static_cast<std::size_t>(_pClass)][static_cast<std::size_t>(attribute)];
}

Point Player::GetTargetPosition() const
{
	// clang-format off
	constexpr int DirectionOffsetX[8] = {  0,-1, 1, 0,-1, 1, 1,-1 };
	constexpr int DirectionOffsetY[8] = { -1, 0, 0, 1,-1,-1, 1, 1 };
	// clang-format on
	Point target = position.future;
	for (auto step : walkpath) {
		if (step == WALK_NONE)
			break;
		if (step > 0) {
			target.x += DirectionOffsetX[step - 1];
			target.y += DirectionOffsetY[step - 1];
		}
	}
	return target;
}

void Player::Say(HeroSpeech speechId) const
{
	_sfx_id soundEffect = herosounds[static_cast<size_t>(_pClass)][static_cast<size_t>(speechId)];

	PlaySfxLoc(soundEffect, position.tile);
}

void Player::SaySpecific(HeroSpeech speechId) const
{
	_sfx_id soundEffect = herosounds[static_cast<size_t>(_pClass)][static_cast<size_t>(speechId)];

	if (effect_is_playing(soundEffect))
		return;

	PlaySfxLoc(soundEffect, position.tile, false);
}

void Player::Say(HeroSpeech speechId, int delay) const
{
	sfxdelay = delay;
	sfxdnum = herosounds[static_cast<size_t>(_pClass)][static_cast<size_t>(speechId)];
}

void Player::Stop()
{
	ClrPlrPath(*this);
	destAction = ACTION_NONE;
}

bool Player::IsWalking() const
{
	return IsAnyOf(_pmode, PM_WALK, PM_WALK2, PM_WALK3);
}

void Player::Reset()
{
	// Create empty default initialized Player on heap to avoid excessive stack usage
	auto emptyPlayer = std::make_unique<Player>();
	*this = std::move(*emptyPlayer);
}

int Player::GetManaShieldDamageReduction()
{
	constexpr int8_t Max = 7;
	return 24 - std::min(_pSplLvl[SPL_MANASHIELD], Max) * 3;
}

void Player::RestorePartialLife()
{
	int wholeHitpoints = _pMaxHP >> 6;
	int l = ((wholeHitpoints / 8) + GenerateRnd(wholeHitpoints / 4)) << 6;
	if (IsAnyOf(_pClass, HeroClass::Warrior, HeroClass::Barbarian))
		l *= 2;
	if (IsAnyOf(_pClass, HeroClass::Rogue, HeroClass::Monk, HeroClass::Bard))
		l += l / 2;
	_pHitPoints = std::min(_pHitPoints + l, _pMaxHP);
	_pHPBase = std::min(_pHPBase + l, _pMaxHPBase);
}

void Player::RestorePartialMana()
{
	int wholeManaPoints = _pMaxMana >> 6;
	int l = ((wholeManaPoints / 8) + GenerateRnd(wholeManaPoints / 4)) << 6;
	if (_pClass == HeroClass::Sorcerer)
		l *= 2;
	if (IsAnyOf(_pClass, HeroClass::Rogue, HeroClass::Monk, HeroClass::Bard))
		l += l / 2;
	if (HasNoneOf(_pIFlags, ItemSpecialEffect::NoMana)) {
		_pMana = std::min(_pMana + l, _pMaxMana);
		_pManaBase = std::min(_pManaBase + l, _pMaxManaBase);
	}
}

void Player::UpdatePreviewCelSprite(_cmd_id cmdId, Point point, uint16_t wParam1, uint16_t wParam2)
{
	// if game is not running don't show a preview
	if (!gbRunGame || PauseMode != 0 || !gbProcessPlayers)
		return;

	// we can only show a preview if our command is executed in the next game tick
	if (_pmode != PM_STAND)
		return;

	std::optional<player_graphic> graphic;
	Direction dir = Direction::South;
	int minimalWalkDistance = -1;

	switch (cmdId) {
	case _cmd_id::CMD_RATTACKID: {
		auto &monster = Monsters[wParam1];
		dir = GetDirection(position.future, monster.position.future);
		graphic = player_graphic::Attack;
		break;
	}
	case _cmd_id::CMD_SPELLID:
	case _cmd_id::CMD_TSPELLID: {
		auto &monster = Monsters[wParam1];
		dir = GetDirection(position.future, monster.position.future);
		graphic = GetPlayerGraphicForSpell(static_cast<spell_id>(wParam1));
		break;
	}
	case _cmd_id::CMD_ATTACKID: {
		auto &monster = Monsters[wParam1];
		point = monster.position.future;
		minimalWalkDistance = 2;
		if (!CanTalkToMonst(monster)) {
			dir = GetDirection(position.future, monster.position.future);
			graphic = player_graphic::Attack;
		}
		break;
	}
	case _cmd_id::CMD_RATTACKPID: {
		auto &targetPlayer = Players[wParam1];
		dir = GetDirection(position.future, targetPlayer.position.future);
		graphic = player_graphic::Attack;
		break;
	}
	case _cmd_id::CMD_SPELLPID:
	case _cmd_id::CMD_TSPELLPID: {
		auto &targetPlayer = Players[wParam1];
		dir = GetDirection(position.future, targetPlayer.position.future);
		graphic = GetPlayerGraphicForSpell(static_cast<spell_id>(wParam1));
		break;
	}
	case _cmd_id::CMD_ATTACKPID: {
		auto &targetPlayer = Players[wParam1];
		point = targetPlayer.position.future;
		minimalWalkDistance = 2;
		dir = GetDirection(position.future, targetPlayer.position.future);
		graphic = player_graphic::Attack;
		break;
	}
	case _cmd_id::CMD_ATTACKXY:
	case _cmd_id::CMD_SATTACKXY:
		dir = GetDirection(position.tile, point);
		graphic = player_graphic::Attack;
		minimalWalkDistance = 2;
		break;
	case _cmd_id::CMD_RATTACKXY:
		dir = GetDirection(position.tile, point);
		graphic = player_graphic::Attack;
		break;
	case _cmd_id::CMD_SPELLXY:
	case _cmd_id::CMD_TSPELLXY:
		dir = GetDirection(position.tile, point);
		graphic = GetPlayerGraphicForSpell(static_cast<spell_id>(wParam1));
		break;
	case _cmd_id::CMD_SPELLXYD:
		dir = static_cast<Direction>(wParam1);
		graphic = GetPlayerGraphicForSpell(static_cast<spell_id>(wParam2));
		break;
	case _cmd_id::CMD_WALKXY:
		minimalWalkDistance = 1;
		break;
	case _cmd_id::CMD_TALKXY:
	case _cmd_id::CMD_DISARMXY:
	case _cmd_id::CMD_OPOBJXY:
	case _cmd_id::CMD_GOTOGETITEM:
	case _cmd_id::CMD_GOTOAGETITEM:
		minimalWalkDistance = 2;
		break;
	default:
		return;
	}

	if (minimalWalkDistance >= 0 && position.future != point) {
		int8_t testWalkPath[MAX_PATH_LENGTH];
		int steps = FindPath([this](Point position) { return PosOkPlayer(*this, position); }, position.future, point, testWalkPath);
		if (steps == 0) {
			// Can't walk to desired location => stand still
			return;
		}
		if (steps >= minimalWalkDistance) {
			graphic = player_graphic::Walk;
			switch (testWalkPath[0]) {
			case WALK_N:
				dir = Direction::North;
				break;
			case WALK_NE:
				dir = Direction::NorthEast;
				break;
			case WALK_E:
				dir = Direction::East;
				break;
			case WALK_SE:
				dir = Direction::SouthEast;
				break;
			case WALK_S:
				dir = Direction::South;
				break;
			case WALK_SW:
				dir = Direction::SouthWest;
				break;
			case WALK_W:
				dir = Direction::West;
				break;
			case WALK_NW:
				dir = Direction::NorthWest;
				break;
			}
			if (!PlrDirOK(*this, dir))
				return;
		}
	}

	if (!graphic)
		return;

	LoadPlrGFX(*this, *graphic);
	std::optional<CelSprite> celSprites = AnimationData[static_cast<size_t>(*graphic)].GetCelSpritesForDirection(dir);
	if (celSprites && previewCelSprite != celSprites) {
		previewCelSprite = celSprites;
		progressToNextGameTickWhenPreviewWasSet = gfProgressToNextGameTick;
	}
}

void LoadPlrGFX(Player &player, player_graphic graphic)
{
	auto &animationData = player.AnimationData[static_cast<size_t>(graphic)];
	if (animationData.RawData != nullptr)
		return;

	char prefix[16];
	char pszName[256];
	const char *szCel;

	HeroClass c = player._pClass;
	if (c == HeroClass::Bard && !hfbard_mpq) {
		c = HeroClass::Rogue;
	} else if (c == HeroClass::Barbarian && !hfbarb_mpq) {
		c = HeroClass::Warrior;
	}

	auto animWeaponId = static_cast<PlayerWeaponGraphic>(player._pgfxnum & 0xF);
	int animationWidth = 96;
	bool useUnarmedAnimationInTown = false;

	const char *cs = ClassPathTbl[static_cast<std::size_t>(c)];

	switch (graphic) {
	case player_graphic::Stand:
		szCel = "AS";
		if (leveltype == DTYPE_TOWN)
			szCel = "ST";
		if (c == HeroClass::Monk)
			animationWidth = 112;
		break;
	case player_graphic::Walk:
		szCel = "AW";
		if (leveltype == DTYPE_TOWN)
			szCel = "WL";
		if (c == HeroClass::Monk)
			animationWidth = 112;
		break;
	case player_graphic::Attack:
		if (leveltype == DTYPE_TOWN)
			return;
		szCel = "AT";
		if (c == HeroClass::Monk)
			animationWidth = 130;
		else if (animWeaponId != PlayerWeaponGraphic::Bow || !(c == HeroClass::Warrior || c == HeroClass::Barbarian))
			animationWidth = 128;
		break;
	case player_graphic::Hit:
		if (leveltype == DTYPE_TOWN)
			return;
		szCel = "HT";
		if (c == HeroClass::Monk)
			animationWidth = 98;
		break;
	case player_graphic::Lightning:
		szCel = "LM";
		useUnarmedAnimationInTown = true;
		if (c == HeroClass::Monk)
			animationWidth = 114;
		else if (c == HeroClass::Sorcerer)
			animationWidth = 128;
		break;
	case player_graphic::Fire:
		szCel = "FM";
		useUnarmedAnimationInTown = true;
		if (c == HeroClass::Monk)
			animationWidth = 114;
		else if (c == HeroClass::Sorcerer)
			animationWidth = 128;
		break;
	case player_graphic::Magic:
		szCel = "QM";
		useUnarmedAnimationInTown = true;
		if (c == HeroClass::Monk)
			animationWidth = 114;
		else if (c == HeroClass::Sorcerer)
			animationWidth = 128;
		break;
	case player_graphic::Death:
		if (animWeaponId != PlayerWeaponGraphic::Unarmed)
			return;
		szCel = "DT";
		animationWidth = (c == HeroClass::Monk) ? 160 : 128;
		break;
	case player_graphic::Block:
		if (leveltype == DTYPE_TOWN)
			return;
		if (!player._pBlockFlag)
			return;
		szCel = "BL";
		if (c == HeroClass::Monk)
			animationWidth = 98;
		break;
	default:
		app_fatal("PLR:2");
	}

	if (leveltype == DTYPE_TOWN && useUnarmedAnimationInTown) {
		// If the hero don't hold the weapon in town then we should use the unarmed animation for casting
		switch (animWeaponId) {
		case PlayerWeaponGraphic::Mace:
		case PlayerWeaponGraphic::Sword:
			animWeaponId = PlayerWeaponGraphic::Unarmed;
			break;
		case PlayerWeaponGraphic::SwordShield:
		case PlayerWeaponGraphic::MaceShield:
			animWeaponId = PlayerWeaponGraphic::UnarmedShield;
			break;
		}
	}

	sprintf(prefix, "%c%c%c", CharChar[static_cast<std::size_t>(c)], ArmourChar[player._pgfxnum >> 4], WepChar[static_cast<std::size_t>(animWeaponId)]);
	sprintf(pszName, R"(PlrGFX\%s\%s\%s%s.CL2)", cs, prefix, prefix, szCel);
	SetPlayerGPtrs(pszName, animationData.RawData, animationData.CelSpritesForDirections, animationWidth);
}

void InitPlayerGFX(Player &player)
{
	ResetPlayerGFX(player);

	if (player._pHitPoints >> 6 == 0) {
		player._pgfxnum &= ~0xF;
		LoadPlrGFX(player, player_graphic::Death);
		return;
	}

	for (size_t i = 0; i < enum_size<player_graphic>::value; i++) {
		auto graphic = static_cast<player_graphic>(i);
		if (graphic == player_graphic::Death)
			continue;
		LoadPlrGFX(player, graphic);
	}
}

void ResetPlayerGFX(Player &player)
{
	player.AnimInfo.celSprite = std::nullopt;
	for (auto &animData : player.AnimationData) {
		for (auto &celSprite : animData.CelSpritesForDirections)
			celSprite = std::nullopt;
		animData.RawData = nullptr;
	}
}

void NewPlrAnim(Player &player, player_graphic graphic, Direction dir, int numberOfFrames, int delayLen, AnimationDistributionFlags flags /*= AnimationDistributionFlags::None*/, int numSkippedFrames /*= 0*/, int distributeFramesBeforeFrame /*= 0*/)
{
	LoadPlrGFX(player, graphic);

	std::optional<CelSprite> celSprite = player.AnimationData[static_cast<size_t>(graphic)].GetCelSpritesForDirection(dir);

	float previewShownGameTickFragments = 0.F;
	if (celSprite == player.previewCelSprite && !player.IsWalking())
		previewShownGameTickFragments = clamp(1.F - player.progressToNextGameTickWhenPreviewWasSet, 0.F, 1.F);
	player.AnimInfo.SetNewAnimation(celSprite, numberOfFrames, delayLen, flags, numSkippedFrames, distributeFramesBeforeFrame, previewShownGameTickFragments);
}

void SetPlrAnims(Player &player)
{
	HeroClass pc = player._pClass;

	if (leveltype == DTYPE_TOWN) {
		player._pNFrames = PlrGFXAnimLens[static_cast<std::size_t>(pc)][7];
		player._pWFrames = PlrGFXAnimLens[static_cast<std::size_t>(pc)][8];
		player._pDFrames = PlrGFXAnimLens[static_cast<std::size_t>(pc)][4];
		player._pSFrames = PlrGFXAnimLens[static_cast<std::size_t>(pc)][5];
	} else {
		player._pNFrames = PlrGFXAnimLens[static_cast<std::size_t>(pc)][0];
		player._pWFrames = PlrGFXAnimLens[static_cast<std::size_t>(pc)][2];
		player._pAFrames = PlrGFXAnimLens[static_cast<std::size_t>(pc)][1];
		player._pHFrames = PlrGFXAnimLens[static_cast<std::size_t>(pc)][6];
		player._pSFrames = PlrGFXAnimLens[static_cast<std::size_t>(pc)][5];
		player._pDFrames = PlrGFXAnimLens[static_cast<std::size_t>(pc)][4];
		player._pBFrames = PlrGFXAnimLens[static_cast<std::size_t>(pc)][3];
		player._pAFNum = PlrGFXAnimLens[static_cast<std::size_t>(pc)][9];
	}
	player._pSFNum = PlrGFXAnimLens[static_cast<std::size_t>(pc)][10];

	auto gn = static_cast<PlayerWeaponGraphic>(player._pgfxnum & 0xF);
	int armorGraphicIndex = player._pgfxnum & ~0xF;
	if (pc == HeroClass::Warrior) {
		if (gn == PlayerWeaponGraphic::Bow) {
			if (leveltype != DTYPE_TOWN) {
				player._pNFrames = 8;
			}
			player._pAFNum = 11;
		} else if (gn == PlayerWeaponGraphic::Axe) {
			player._pAFrames = 20;
			player._pAFNum = 10;
		} else if (gn == PlayerWeaponGraphic::Staff) {
			player._pAFrames = 16;
			player._pAFNum = 11;
		}
		if (armorGraphicIndex > 0)
			player._pDFrames = 15;
	} else if (pc == HeroClass::Rogue) {
		if (gn == PlayerWeaponGraphic::Axe) {
			player._pAFrames = 22;
			player._pAFNum = 13;
		} else if (gn == PlayerWeaponGraphic::Bow) {
			player._pAFrames = 12;
			player._pAFNum = 7;
		} else if (gn == PlayerWeaponGraphic::Staff) {
			player._pAFrames = 16;
			player._pAFNum = 11;
		}
	} else if (pc == HeroClass::Sorcerer) {
		if (gn == PlayerWeaponGraphic::Unarmed) {
			player._pAFrames = 20;
		} else if (gn == PlayerWeaponGraphic::UnarmedShield) {
			player._pAFNum = 9;
		} else if (gn == PlayerWeaponGraphic::Bow) {
			player._pAFrames = 20;
			player._pAFNum = 16;
		} else if (gn == PlayerWeaponGraphic::Axe) {
			player._pAFrames = 24;
			player._pAFNum = 16;
		}
	} else if (pc == HeroClass::Monk) {
		switch (gn) {
		case PlayerWeaponGraphic::Unarmed:
		case PlayerWeaponGraphic::UnarmedShield:
			player._pAFrames = 12;
			player._pAFNum = 7;
			break;
		case PlayerWeaponGraphic::Bow:
			player._pAFrames = 20;
			player._pAFNum = 14;
			break;
		case PlayerWeaponGraphic::Axe:
			player._pAFrames = 23;
			player._pAFNum = 14;
			break;
		case PlayerWeaponGraphic::Staff:
			player._pAFrames = 13;
			player._pAFNum = 8;
			break;
		default:
			break;
		}
	} else if (pc == HeroClass::Bard) {
		if (gn == PlayerWeaponGraphic::Axe) {
			player._pAFrames = 22;
			player._pAFNum = 13;
		} else if (gn == PlayerWeaponGraphic::Bow) {
			player._pAFrames = 12;
			player._pAFNum = 11;
		} else if (gn == PlayerWeaponGraphic::Staff) {
			player._pAFrames = 16;
			player._pAFNum = 11;
		}
	} else if (pc == HeroClass::Barbarian) {
		if (gn == PlayerWeaponGraphic::Axe) {
			player._pAFrames = 20;
			player._pAFNum = 8;
		} else if (gn == PlayerWeaponGraphic::Bow) {
			if (leveltype != DTYPE_TOWN) {
				player._pNFrames = 8;
			}
			player._pAFNum = 11;
		} else if (gn == PlayerWeaponGraphic::Staff) {
			player._pAFrames = 16;
			player._pAFNum = 11;
		} else if (gn == PlayerWeaponGraphic::Mace || gn == PlayerWeaponGraphic::MaceShield) {
			player._pAFNum = 8;
		}
		if (armorGraphicIndex > 0)
			player._pDFrames = 15;
	}
}

/**
 * @param c The hero class.
 */
void CreatePlayer(int playerId, HeroClass c)
{
	if ((DWORD)playerId >= MAX_PLRS) {
		app_fatal("CreatePlayer: illegal player %i", playerId);
	}
	auto &player = Players[playerId];

	player.Reset();
	SetRndSeed(SDL_GetTicks());

	player._pClass = c;

	player._pBaseStr = StrengthTbl[static_cast<std::size_t>(c)];
	player._pStrength = player._pBaseStr;

	player._pBaseMag = MagicTbl[static_cast<std::size_t>(c)];
	player._pMagic = player._pBaseMag;

	player._pBaseDex = DexterityTbl[static_cast<std::size_t>(c)];
	player._pDexterity = player._pBaseDex;

	player._pBaseVit = VitalityTbl[static_cast<std::size_t>(c)];
	player._pVitality = player._pBaseVit;

	player._pStatPts = 0;
	player.pTownWarps = 0;
	player.pDungMsgs = 0;
	player.pDungMsgs2 = 0;
	player.pLvlLoad = 0;
	player.pDiabloKillLevel = 0;
	player.pDifficulty = DIFF_NORMAL;

	player._pLevel = 1;

	player._pBaseToBlk = BlockBonuses[static_cast<std::size_t>(c)];

	player._pHitPoints = (player._pVitality + 10) << 6;
	if (player._pClass == HeroClass::Warrior || player._pClass == HeroClass::Barbarian) {
		player._pHitPoints *= 2;
	} else if (player._pClass == HeroClass::Rogue || player._pClass == HeroClass::Monk || player._pClass == HeroClass::Bard) {
		player._pHitPoints += player._pHitPoints / 2;
	}

	player._pMaxHP = player._pHitPoints;
	player._pHPBase = player._pHitPoints;
	player._pMaxHPBase = player._pHitPoints;

	player._pMana = player._pMagic << 6;
	if (player._pClass == HeroClass::Sorcerer) {
		player._pMana *= 2;
	} else if (player._pClass == HeroClass::Bard) {
		player._pMana += player._pMana * 3 / 4;
	} else if (player._pClass == HeroClass::Rogue || player._pClass == HeroClass::Monk) {
		player._pMana += player._pMana / 2;
	}

	player._pMaxMana = player._pMana;
	player._pManaBase = player._pMana;
	player._pMaxManaBase = player._pMana;

	player._pMaxLvl = player._pLevel;
	player._pExperience = 0;
	player._pNextExper = ExpLvlsTbl[1];
	player._pArmorClass = 0;
	player._pLightRad = 10;
	player._pInfraFlag = false;

	player._pRSplType = RSPLTYPE_SKILL;
	if (c == HeroClass::Warrior) {
		player._pAblSpells = GetSpellBitmask(SPL_REPAIR);
		player._pRSpell = SPL_REPAIR;
	} else if (c == HeroClass::Rogue) {
		player._pAblSpells = GetSpellBitmask(SPL_DISARM);
		player._pRSpell = SPL_DISARM;
	} else if (c == HeroClass::Sorcerer) {
		player._pAblSpells = GetSpellBitmask(SPL_RECHARGE);
		player._pRSpell = SPL_RECHARGE;
	} else if (c == HeroClass::Monk) {
		player._pAblSpells = GetSpellBitmask(SPL_SEARCH);
		player._pRSpell = SPL_SEARCH;
	} else if (c == HeroClass::Bard) {
		player._pAblSpells = GetSpellBitmask(SPL_IDENTIFY);
		player._pRSpell = SPL_IDENTIFY;
	} else if (c == HeroClass::Barbarian) {
		player._pAblSpells = GetSpellBitmask(SPL_BLODBOIL);
		player._pRSpell = SPL_BLODBOIL;
	}

	if (c == HeroClass::Sorcerer) {
		player._pMemSpells = GetSpellBitmask(SPL_FIREBOLT);
		player._pRSplType = RSPLTYPE_SPELL;
		player._pRSpell = SPL_FIREBOLT;
	} else {
		player._pMemSpells = 0;
	}

	for (int8_t &spellLevel : player._pSplLvl) {
		spellLevel = 0;
	}

	player._pSpellFlags = SpellFlag::None;

	if (player._pClass == HeroClass::Sorcerer) {
		player._pSplLvl[SPL_FIREBOLT] = 2;
	}

	// Initializing the hotkey bindings to no selection
	std::fill(player._pSplHotKey, player._pSplHotKey + NumHotkeys, SPL_INVALID);

	PlayerWeaponGraphic animWeaponId = PlayerWeaponGraphic::Unarmed;
	switch (c) {
	case HeroClass::Warrior:
	case HeroClass::Bard:
	case HeroClass::Barbarian:
		animWeaponId = PlayerWeaponGraphic::SwordShield;
		break;
	case HeroClass::Rogue:
		animWeaponId = PlayerWeaponGraphic::Bow;
		break;
	case HeroClass::Sorcerer:
	case HeroClass::Monk:
		animWeaponId = PlayerWeaponGraphic::Staff;
		break;
	}
	player._pgfxnum = static_cast<int>(animWeaponId);

	for (bool &levelVisited : player._pLvlVisited) {
		levelVisited = false;
	}

	for (int i = 0; i < 10; i++) {
		player._pSLvlVisited[i] = false;
	}

	player._pLvlChanging = false;
	player.pTownWarps = 0;
	player.pLvlLoad = 0;
	player.pBattleNet = false;
	player.pManaShield = false;
	player.pDamAcFlags = ItemSpecialEffectHf::None;
	player.wReflections = 0;
	player.wEtherealize = 0;

	InitDungMsgs(player);
	CreatePlrItems(playerId);
	SetRndSeed(0);
}

int CalcStatDiff(Player &player)
{
	int diff = 0;
	for (auto attribute : enum_values<CharacterAttribute>()) {
		diff += player.GetMaximumAttributeValue(attribute);
		diff -= player.GetBaseAttributeValue(attribute);
	}
	return diff;
}

void NextPlrLevel(int pnum)
{
	if ((DWORD)pnum >= MAX_PLRS) {
		app_fatal("NextPlrLevel: illegal player %i", pnum);
	}
	auto &player = Players[pnum];

	player._pLevel++;
	player._pMaxLvl++;

	CalcPlrInv(player, true);

	if (CalcStatDiff(player) < 5) {
		player._pStatPts = CalcStatDiff(player);
	} else {
		player._pStatPts += 5;
	}

	player._pNextExper = ExpLvlsTbl[player._pLevel];

	int hp = player._pClass == HeroClass::Sorcerer ? 64 : 128;

	player._pMaxHP += hp;
	player._pHitPoints = player._pMaxHP;
	player._pMaxHPBase += hp;
	player._pHPBase = player._pMaxHPBase;

	if (pnum == MyPlayerId) {
		drawhpflag = true;
	}

	int mana = 128;
	if (player._pClass == HeroClass::Warrior)
		mana = 64;
	else if (player._pClass == HeroClass::Barbarian)
		mana = 0;

	player._pMaxMana += mana;
	player._pMaxManaBase += mana;

	if (HasNoneOf(player._pIFlags, ItemSpecialEffect::NoMana)) {
		player._pMana = player._pMaxMana;
		player._pManaBase = player._pMaxManaBase;
	}

	if (pnum == MyPlayerId) {
		drawmanaflag = true;
	}

	if (ControlMode != ControlTypes::KeyboardAndMouse)
		FocusOnCharInfo();

	CalcPlrInv(player, true);
}

void AddPlrExperience(int pnum, int lvl, int exp)
{
	if (pnum != MyPlayerId) {
		return;
	}

	if (pnum >= MAX_PLRS || pnum < 0) {
		app_fatal("AddPlrExperience: illegal player %i", pnum);
	}
	auto &player = Players[pnum];

	if (player._pHitPoints <= 0) {
		return;
	}

	// Adjust xp based on difference in level between player and monster
	uint32_t clampedExp = std::max(static_cast<int>(exp * (1 + (lvl - player._pLevel) / 10.0)), 0);

	// Prevent power leveling
	//if (gbIsMultiplayer) {
	//	const uint32_t clampedPlayerLevel = clamp(static_cast<int>(player._pLevel), 1, MAXCHARLEVEL);

	//	// for low level characters experience gain is capped to 1/20 of current levels xp
	//	// for high level characters experience gain is capped to 200 * current level - this is a smaller value than 1/20 of the exp needed for the next level after level 5.
	//	clampedExp = std::min({ clampedExp, /* level 0-5: */ ExpLvlsTbl[clampedPlayerLevel] / 20U, /* level 6-50: */ 200U * clampedPlayerLevel });
	//}

	constexpr uint32_t MaxExperience = 2000000000U;

	// Overflow is only possible if a kill grants more than (2^32-1 - MaxExperience) XP in one go, which doesn't happen in normal gameplay
	player._pExperience = std::min(player._pExperience + clampedExp, MaxExperience);

	if (*sgOptions.Gameplay.experienceBar) {
		force_redraw = 255;
	}

	if (player._pExperience >= ExpLvlsTbl[49]) {
		player._pLevel = 50;
		return;
	}

	// Increase player level if applicable
	int newLvl = 0;
	while (player._pExperience >= ExpLvlsTbl[newLvl]) {
		newLvl++;
	}
	if (newLvl != player._pLevel) {
		for (int i = newLvl - player._pLevel; i > 0; i--) {
			NextPlrLevel(pnum);
		}
	}

	NetSendCmdParam1(false, CMD_PLRLEVEL, player._pLevel);
}

void AddPlrMonstExper(int lvl, int exp, char pmask)
{
	int totplrs = 0;
	for (int i = 0; i < MAX_PLRS; i++) {
		if (((1 << i) & pmask) != 0) {
			totplrs++;
		}
	}

	if (totplrs != 0) {
		int e = exp / totplrs;
		if ((pmask & (1 << MyPlayerId)) != 0)
			AddPlrExperience(MyPlayerId, lvl, e);
	}
}

void InitPlayer(Player &player, bool firstTime)
{
	auto &myPlayer = Players[MyPlayerId];

	if (firstTime) {
		player._pRSplType = RSPLTYPE_INVALID;
		player._pRSpell = SPL_INVALID;
		if (&player == &myPlayer)
			LoadHotkeys();
		player._pSBkSpell = SPL_INVALID;
		player._pSpell = player._pRSpell;
		player._pSplType = player._pRSplType;
		player.pManaShield = false;
		player.wReflections = 0;
		player.wEtherealize = 0;
	}

	if (player.plrlevel == currlevel) {

		SetPlrAnims(player);

		player.position.offset = { 0, 0 };
		player.position.velocity = { 0, 0 };

		ClearStateVariables(player);

		if (player._pHitPoints >> 6 > 0) {
			player._pmode = PM_STAND;
			NewPlrAnim(player, player_graphic::Stand, Direction::South, player._pNFrames, 4);
			player.AnimInfo.CurrentFrame = GenerateRnd(player._pNFrames - 1);
			player.AnimInfo.TickCounterOfCurrentFrame = GenerateRnd(3);
		} else {
			player._pmode = PM_DEATH;
			NewPlrAnim(player, player_graphic::Death, Direction::South, player._pDFrames, 2);
			player.AnimInfo.CurrentFrame = player.AnimInfo.NumberOfFrames - 2;
		}

		player._pdir = Direction::South;

		if (&player == &myPlayer) {
			if (!firstTime || currlevel != 0) {
				player.position.tile = ViewPosition;
			}
		} else {
			unsigned i;
			for (i = 0; i < 8 && !PosOkPlayer(player, player.position.tile + Displacement { plrxoff2[i], plryoff2[i] }); i++)
				;
			player.position.tile.x += plrxoff2[i];
			player.position.tile.y += plryoff2[i];
		}

		player.position.future = player.position.tile;
		player.walkpath[0] = WALK_NONE;
		player.destAction = ACTION_NONE;

		if (&player == &myPlayer) {
			player._plid = AddLight(player.position.tile, player._pLightRad);
			ChangeLightXY(player._plid, player.position.tile); // fix for a bug where old light is still visible at the entrance after reentering level
		} else {
			player._plid = NO_LIGHT;
		}
		player._pvid = AddVision(player.position.tile, player._pLightRad, &player == &myPlayer);
	}

	if (player._pClass == HeroClass::Warrior) {
		player._pAblSpells = GetSpellBitmask(SPL_REPAIR);
	} else if (player._pClass == HeroClass::Rogue) {
		player._pAblSpells = GetSpellBitmask(SPL_DISARM);
	} else if (player._pClass == HeroClass::Sorcerer) {
		player._pAblSpells = GetSpellBitmask(SPL_RECHARGE);
	} else if (player._pClass == HeroClass::Monk) {
		player._pAblSpells = GetSpellBitmask(SPL_SEARCH);
	} else if (player._pClass == HeroClass::Bard) {
		player._pAblSpells = GetSpellBitmask(SPL_IDENTIFY);
	} else if (player._pClass == HeroClass::Barbarian) {
		player._pAblSpells = GetSpellBitmask(SPL_BLODBOIL);
	}

	player._pNextExper = ExpLvlsTbl[player._pLevel];
	player._pInvincible = false;

	if (&player == &myPlayer) {
		MyPlayerIsDead = false;
		ScrollInfo.offset = { 0, 0 };
		ScrollInfo._sdir = ScrollDirection::None;
	}
}

void InitMultiView()
{
	if ((DWORD)MyPlayerId >= MAX_PLRS) {
		app_fatal("InitPlayer: illegal player %i", MyPlayerId);
	}
	auto &myPlayer = Players[MyPlayerId];

	ViewPosition = myPlayer.position.tile;
}

void PlrClrTrans(Point position)
{
	for (int i = position.y - 1; i <= position.y + 1; i++) {
		for (int j = position.x - 1; j <= position.x + 1; j++) {
			TransList[dTransVal[j][i]] = false;
		}
	}
}

void PlrDoTrans(Point position)
{
	if (leveltype != DTYPE_CATHEDRAL && leveltype != DTYPE_CATACOMBS) {
		TransList[1] = true;
		return;
	}

	for (int i = position.y - 1; i <= position.y + 1; i++) {
		for (int j = position.x - 1; j <= position.x + 1; j++) {
			if (IsTileNotSolid({ j, i }) && dTransVal[j][i] != 0) {
				TransList[dTransVal[j][i]] = true;
			}
		}
	}
}

void SetPlayerOld(Player &player)
{
	player.position.old = player.position.tile;
}

void FixPlayerLocation(int pnum, Direction bDir)
{
	if ((DWORD)pnum >= MAX_PLRS) {
		app_fatal("FixPlayerLocation: illegal player %i", pnum);
	}
	auto &player = Players[pnum];

	player.position.future = player.position.tile;
	player.position.offset = { 0, 0 };
	player._pdir = bDir;
	if (pnum == MyPlayerId) {
		ScrollInfo.offset = { 0, 0 };
		ScrollInfo._sdir = ScrollDirection::None;
		ViewPosition = player.position.tile;
	}
	ChangeLightXY(player._plid, player.position.tile);
	ChangeVisionXY(player._pvid, player.position.tile);
}

void StartStand(int pnum, Direction dir)
{
	if ((DWORD)pnum >= MAX_PLRS) {
		app_fatal("StartStand: illegal player %i", pnum);
	}
	auto &player = Players[pnum];

	if (player._pInvincible && player._pHitPoints == 0 && pnum == MyPlayerId) {
		SyncPlrKill(pnum, -1);
		return;
	}

	NewPlrAnim(player, player_graphic::Stand, dir, player._pNFrames, 4);
	player._pmode = PM_STAND;
	FixPlayerLocation(pnum, dir);
	FixPlrWalkTags(pnum);
	dPlayer[player.position.tile.x][player.position.tile.y] = pnum + 1;
	SetPlayerOld(player);
}

void StartPlrBlock(int pnum, Direction dir)
{
	if ((DWORD)pnum >= MAX_PLRS) {
		app_fatal("StartPlrBlock: illegal player %i", pnum);
	}
	auto &player = Players[pnum];

	if (player._pInvincible && player._pHitPoints == 0 && pnum == MyPlayerId) {
		SyncPlrKill(pnum, -1);
		return;
	}

	PlaySfxLoc(IS_ISWORD, player.position.tile);

	int skippedAnimationFrames = 0;
	if (HasAnyOf(player._pIFlags, ItemSpecialEffect::FastBlock)) {
		skippedAnimationFrames = (player._pBFrames - 2); // ISPL_FASTBLOCK means we cancel the animation if frame 2 was shown
	}

	NewPlrAnim(player, player_graphic::Block, dir, player._pBFrames, 3, AnimationDistributionFlags::SkipsDelayOfLastFrame, skippedAnimationFrames);

	player._pmode = PM_BLOCK;
	FixPlayerLocation(pnum, dir);
	SetPlayerOld(player);
}

void FixPlrWalkTags(int pnum)
{
	if ((DWORD)pnum >= MAX_PLRS) {
		app_fatal("FixPlrWalkTags: illegal player %i", pnum);
	}
	auto &player = Players[pnum];

	int pp = pnum + 1;
	int pn = -(pnum + 1);
	int dx = player.position.old.x;
	int dy = player.position.old.y;
	for (int y = dy - 1; y <= dy + 1; y++) {
		for (int x = dx - 1; x <= dx + 1; x++) {
			if (InDungeonBounds({ x, y }) && (dPlayer[x][y] == pp || dPlayer[x][y] == pn)) {
				dPlayer[x][y] = 0;
			}
		}
	}
}

void RemovePlrFromMap(int pnum)
{
	int pp = pnum + 1;
	int pn = -(pnum + 1);

	for (int y = 0; y < MAXDUNY; y++) {
		for (int x = 0; x < MAXDUNX; x++) // NOLINT(modernize-loop-convert)
			if (dPlayer[x][y] == pp || dPlayer[x][y] == pn)
				dPlayer[x][y] = 0;
	}
}

void StartPlrHit(int pnum, int dam, bool forcehit)
{
	if ((DWORD)pnum >= MAX_PLRS) {
		app_fatal("StartPlrHit: illegal player %i", pnum);
	}
	auto &player = Players[pnum];

	if (player._pInvincible && player._pHitPoints == 0 && pnum == MyPlayerId) {
		SyncPlrKill(pnum, -1);
		return;
	}

	player.Say(HeroSpeech::ArghClang);

	drawhpflag = true;
	if (player._pClass == HeroClass::Barbarian) {
		if (dam >> 6 < player._pLevel + player._pLevel / 4 && !forcehit) {
			return;
		}
	} else if (dam >> 6 < player._pLevel && !forcehit) {
		return;
	}

	Direction pd = player._pdir;

	int skippedAnimationFrames = 0;
	constexpr ItemSpecialEffect ZenFlags = ItemSpecialEffect::FastHitRecovery | ItemSpecialEffect::FasterHitRecovery | ItemSpecialEffect::FastestHitRecovery;
	if (HasAllOf(player._pIFlags, ZenFlags)) { // if multiple hitrecovery modes are present the skipping of frames can go so far, that they skip frames that would skip. so the additional skipping thats skipped. that means we can't add the different modes together.
		skippedAnimationFrames = 4;
	} else if (HasAnyOf(player._pIFlags, ItemSpecialEffect::FastestHitRecovery)) {
		skippedAnimationFrames = 3;
	} else if (HasAnyOf(player._pIFlags, ItemSpecialEffect::FasterHitRecovery)) {
		skippedAnimationFrames = 2;
	} else if (HasAnyOf(player._pIFlags, ItemSpecialEffect::FastHitRecovery)) {
		skippedAnimationFrames = 1;
	} else {
		skippedAnimationFrames = 0;
	}

	NewPlrAnim(player, player_graphic::Hit, pd, player._pHFrames, 1, AnimationDistributionFlags::None, skippedAnimationFrames);

	player._pmode = PM_GOTHIT;
	FixPlayerLocation(pnum, pd);
	FixPlrWalkTags(pnum);
	dPlayer[player.position.tile.x][player.position.tile.y] = pnum + 1;
	SetPlayerOld(player);
}

#if defined(__clang__) || defined(__GNUC__)
__attribute__((no_sanitize("shift-base")))
#endif
void
StartPlayerKill(int pnum, int earflag)
{
	if ((DWORD)pnum >= MAX_PLRS) {
		app_fatal("StartPlayerKill: illegal player %i", pnum);
	}
	auto &player = Players[pnum];

	if (player._pHitPoints <= 0 && player._pmode == PM_DEATH) {
		return;
	}

	if (MyPlayerId == pnum) {
		NetSendCmdParam1(true, CMD_PLRDEAD, earflag);
	}

	bool diablolevel = gbIsMultiplayer && player.plrlevel == 16;

	player.Say(HeroSpeech::AuughUh);

	if (player._pgfxnum != 0) {
		if (diablolevel || earflag != 0)
			player._pgfxnum &= ~0xF;
		else
			player._pgfxnum = 0;
		ResetPlayerGFX(player);
		SetPlrAnims(player);
	}

	NewPlrAnim(player, player_graphic::Death, player._pdir, player._pDFrames, 2);

	player._pBlockFlag = false;
	player._pmode = PM_DEATH;
	player._pInvincible = true;
	SetPlayerHitPoints(player, 0);

	if (pnum != MyPlayerId && earflag == 0 && !diablolevel) {
		for (auto &item : player.InvBody) {
			item.Clear();
		}
		CalcPlrInv(player, false);
	}

	if (player.plrlevel == currlevel) {
		FixPlayerLocation(pnum, player._pdir);
		RemovePlrFromMap(pnum);
		dFlags[player.position.tile.x][player.position.tile.y] |= DungeonFlag::DeadPlayer;
		SetPlayerOld(player);

		if (pnum == MyPlayerId) {
			drawhpflag = true;

			if (!player.HoldItem.isEmpty()) {
				DeadItem(player, std::move(player.HoldItem), { 0, 0 });
				NewCursor(CURSOR_HAND);
			}

			if (!diablolevel) {
				DropHalfPlayersGold(pnum);
				if (earflag != -1) {
					if (earflag != 0) {
						Item ear;
						InitializeItem(ear, IDI_EAR);
						CopyUtf8(ear._iName, fmt::format(_("Ear of {:s}"), player._pName), sizeof(ear._iName));
						switch (player._pClass) {
						case HeroClass::Sorcerer:
							ear._iCurs = ICURS_EAR_SORCERER;
							break;
						case HeroClass::Warrior:
							ear._iCurs = ICURS_EAR_WARRIOR;
							break;
						case HeroClass::Rogue:
						case HeroClass::Monk:
						case HeroClass::Bard:
						case HeroClass::Barbarian:
							ear._iCurs = ICURS_EAR_ROGUE;
							break;
						}

						ear._iCreateInfo = player._pName[0] << 8 | player._pName[1];
						ear._iSeed = player._pName[2] << 24 | player._pName[3] << 16 | player._pName[4] << 8 | player._pName[5];
						ear._ivalue = player._pLevel;

						if (FindGetItem(ear._iSeed, IDI_EAR, ear._iCreateInfo) == -1) {
							DeadItem(player, std::move(ear), { 0, 0 });
						}
					} else {
						Direction pdd = player._pdir;
						for (auto &item : player.InvBody) {
							pdd = Left(pdd);
							DeadItem(player, std::move(item), Displacement(pdd));
							item.Clear();
						}

						CalcPlrInv(player, false);
					}
				}
			}
		}
	}
	SetPlayerHitPoints(player, 0);
}

void StripTopGold(Player &player)
{
	for (Item &item : InventoryPlayerItemsRange { player }) {
		if (item._itype == ItemType::Gold) {
			if (item._ivalue > MaxGold) {
				Item excessGold;
				MakeGoldStack(excessGold, item._ivalue - MaxGold);
				item._ivalue = MaxGold;

				if (!GoldAutoPlace(player, excessGold)) {
					DeadItem(player, std::move(excessGold), { 0, 0 });
				}
			}
		}
	}
	player._pGold = CalculateGold(player);
}

void ApplyPlrDamage(int pnum, int dam, int minHP /*= 0*/, int frac /*= 0*/, int earflag /*= 0*/)
{
	auto &player = Players[pnum];

	int totalDamage = (dam << 6) + frac;
	if (totalDamage > 0 && player.pManaShield) {
		int8_t manaShieldLevel = player._pSplLvl[SPL_MANASHIELD];
		if (manaShieldLevel > 0) {
			totalDamage += totalDamage / -player.GetManaShieldDamageReduction();
		}
		if (pnum == MyPlayerId)
			drawmanaflag = true;
		if (player._pMana >= totalDamage) {
			player._pMana -= totalDamage;
			player._pManaBase -= totalDamage;
			totalDamage = 0;
		} else {
			totalDamage -= player._pMana;
			if (manaShieldLevel > 0) {
				totalDamage += totalDamage / (player.GetManaShieldDamageReduction() - 1);
			}
			player._pMana = 0;
			player._pManaBase = player._pMaxManaBase - player._pMaxMana;
			if (pnum == MyPlayerId)
				NetSendCmd(true, CMD_REMSHIELD);
		}
	}

	if (totalDamage == 0)
		return;

	drawhpflag = true;
	player._pHitPoints -= totalDamage;
	player._pHPBase -= totalDamage;
	if (player._pHitPoints > player._pMaxHP) {
		player._pHitPoints = player._pMaxHP;
		player._pHPBase = player._pMaxHPBase;
	}
	int minHitPoints = minHP << 6;
	if (player._pHitPoints < minHitPoints) {
		SetPlayerHitPoints(player, minHitPoints);
	}
	if (player._pHitPoints >> 6 <= 0) {
		SyncPlrKill(pnum, earflag);
	}
}

void SyncPlrKill(int pnum, int earflag)
{
	auto &player = Players[pnum];

	if (player._pHitPoints <= 0 && currlevel == 0) {
		SetPlayerHitPoints(player, 64);
		return;
	}

	SetPlayerHitPoints(player, 0);
	StartPlayerKill(pnum, earflag);
}

void RemovePlrMissiles(int pnum)
{
	if (currlevel != 0 && pnum == MyPlayerId) {
		auto &golem = Monsters[MyPlayerId];
		if (golem.position.tile.x != 1 || golem.position.tile.y != 0) {
			M_StartKill(MyPlayerId, MyPlayerId);
			AddCorpse(golem.position.tile, golem.MType->mdeadval, golem._mdir);
			int mx = golem.position.tile.x;
			int my = golem.position.tile.y;
			dMonster[mx][my] = 0;
			golem._mDelFlag = true;
			DeleteMonsterList();
		}
	}

	for (auto &missile : Missiles) {
		if (missile._mitype == MIS_STONE && missile._misource == pnum) {
			Monsters[missile.var2]._mmode = static_cast<MonsterMode>(missile.var1);
		}
	}
}

#if defined(__clang__) || defined(__GNUC__)
__attribute__((no_sanitize("shift-base")))
#endif
void
StartNewLvl(int pnum, interface_mode fom, int lvl)
{
	InitLevelChange(pnum);

	if ((DWORD)pnum >= MAX_PLRS) {
		app_fatal("StartNewLvl: illegal player %i", pnum);
	}
	auto &player = Players[pnum];
	auto &myPlayer = Players[MyPlayerId];

	switch (fom) {
	case WM_DIABNEXTLVL:
	case WM_DIABPREVLVL:
	case WM_DIABRTNLVL:
	case WM_DIABTOWNWARP:
		player.plrlevel = lvl;
		break;
	case WM_DIABSETLVL:
		setlvlnum = (_setlevels)lvl;
		break;
	case WM_DIABTWARPUP:
		myPlayer.pTownWarps |= 1 << (leveltype - 2);
		player.plrlevel = lvl;
		break;
	case WM_DIABRETOWN:
		break;
	default:
		app_fatal("StartNewLvl");
	}

	if (pnum == MyPlayerId) {
		player._pmode = PM_NEWLVL;
		player._pInvincible = true;
		PostMessage(fom, 0, 0);
		if (gbIsMultiplayer) {
			NetSendCmdParam2(true, CMD_NEWLVL, fom, lvl);
		}
	}
}

void RestartTownLvl(int pnum)
{
	InitLevelChange(pnum);
	if ((DWORD)pnum >= MAX_PLRS) {
		app_fatal("RestartTownLvl: illegal player %i", pnum);
	}
	auto &player = Players[pnum];

	player.plrlevel = 0;
	player._pInvincible = false;

	SetPlayerHitPoints(player, 64);

	player._pMana = 0;
	player._pManaBase = player._pMana - (player._pMaxMana - player._pMaxManaBase);

	CalcPlrInv(player, false);

	if (pnum == MyPlayerId) {
		player._pmode = PM_NEWLVL;
		player._pInvincible = true;
		PostMessage(WM_DIABRETOWN, 0, 0);
	}
}

void StartWarpLvl(int pnum, int pidx)
{
	auto &player = Players[pnum];

	InitLevelChange(pnum);

	if (gbIsMultiplayer) {
		if (player.plrlevel != 0) {
			player.plrlevel = 0;
		} else {
			player.plrlevel = Portals[pidx].level;
		}
	}

	if (pnum == MyPlayerId) {
		SetCurrentPortal(pidx);
		player._pmode = PM_NEWLVL;
		player._pInvincible = true;
		PostMessage(WM_DIABWARPLVL, 0, 0);
	}
}

void ProcessPlayers()
{
	if ((DWORD)MyPlayerId >= MAX_PLRS) {
		app_fatal("ProcessPlayers: illegal player %i", MyPlayerId);
	}
	auto &myPlayer = Players[MyPlayerId];

	if (myPlayer.pLvlLoad > 0) {
		myPlayer.pLvlLoad--;
	}

	if (sfxdelay > 0) {
		sfxdelay--;
		if (sfxdelay == 0) {
			switch (sfxdnum) {
			case USFX_DEFILER1:
				InitQTextMsg(TEXT_DEFILER1);
				break;
			case USFX_DEFILER2:
				InitQTextMsg(TEXT_DEFILER2);
				break;
			case USFX_DEFILER3:
				InitQTextMsg(TEXT_DEFILER3);
				break;
			case USFX_DEFILER4:
				InitQTextMsg(TEXT_DEFILER4);
				break;
			default:
				PlaySFX(sfxdnum);
			}
		}
	}

	ValidatePlayer();

	for (int pnum = 0; pnum < MAX_PLRS; pnum++) {
		auto &player = Players[pnum];
		if (player.plractive && currlevel == player.plrlevel && (pnum == MyPlayerId || !player._pLvlChanging)) {
			CheckCheatStats(player);

			if (!PlrDeathModeOK(pnum) && (player._pHitPoints >> 6) <= 0) {
				SyncPlrKill(pnum, -1);
			}

			if (pnum == MyPlayerId) {
				if (HasAnyOf(player._pIFlags, ItemSpecialEffect::DrainLife) && currlevel != 0) {
					ApplyPlrDamage(pnum, 0, 0, 4);
				}
				if (HasAnyOf(player._pIFlags, ItemSpecialEffect::NoMana) && player._pManaBase > 0) {
					player._pManaBase -= player._pMana;
					player._pMana = 0;
					drawmanaflag = true;
				}
			}

			bool tplayer = false;
			do {
				switch (player._pmode) {
				case PM_STAND:
				case PM_NEWLVL:
				case PM_QUIT:
					tplayer = false;
					break;
				case PM_WALK:
				case PM_WALK2:
				case PM_WALK3:
					tplayer = DoWalk(pnum, player._pmode);
					break;
				case PM_ATTACK:
					tplayer = DoAttack(pnum);
					break;
				case PM_RATTACK:
					tplayer = DoRangeAttack(pnum);
					break;
				case PM_BLOCK:
					tplayer = DoBlock(pnum);
					break;
				case PM_SPELL:
					tplayer = DoSpell(pnum);
					break;
				case PM_GOTHIT:
					tplayer = DoGotHit(pnum);
					break;
				case PM_DEATH:
					tplayer = DoDeath(pnum);
					break;
				}
				CheckNewPath(pnum, tplayer);
			} while (tplayer);

			player.previewCelSprite = std::nullopt;
			player.AnimInfo.ProcessAnimation();
		}
	}
}

void ClrPlrPath(Player &player)
{
	memset(player.walkpath, WALK_NONE, sizeof(player.walkpath));
}

/**
 * @brief Determines if the target position is clear for the given player to stand on.
 *
 * This requires an ID instead of a Player& to compare with the dPlayer lookup table values.
 *
 * @param position Dungeon tile coordinates.
 * @return False if something (other than the player themselves) is blocking the tile.
 */
bool PosOkPlayer(const Player &player, Point position)
{
	if (!InDungeonBounds(position))
		return false;
	if (dPiece[position.x][position.y] == 0)
		return false;
	if (!IsTileWalkable(position))
		return false;
	if (dPlayer[position.x][position.y] != 0) {
		auto &otherPlayer = Players[abs(dPlayer[position.x][position.y]) - 1];
		if (&otherPlayer != &player && otherPlayer._pHitPoints != 0) {
			return false;
		}
	}

	if (dMonster[position.x][position.y] != 0) {
		if (currlevel == 0) {
			return false;
		}
		if (dMonster[position.x][position.y] <= 0) {
			return false;
		}
		if ((Monsters[dMonster[position.x][position.y] - 1]._mhitpoints >> 6) > 0) {
			return false;
		}
	}

	return true;
}

void MakePlrPath(Player &player, Point targetPosition, bool endspace)
{
	if (player.position.future == targetPosition) {
		return;
	}

	int path = FindPath([&player](Point position) { return PosOkPlayer(player, position); }, player.position.future, targetPosition, player.walkpath);
	if (path == 0) {
		return;
	}

	if (!endspace) {
		path--;
	}

	player.walkpath[path] = WALK_NONE;
}

void CalcPlrStaff(Player &player)
{
	player._pISpells = 0;
	if (!player.InvBody[INVLOC_HAND_LEFT].isEmpty()
	    && player.InvBody[INVLOC_HAND_LEFT]._iStatFlag
	    && player.InvBody[INVLOC_HAND_LEFT]._iCharges > 0) {
		player._pISpells |= GetSpellBitmask(player.InvBody[INVLOC_HAND_LEFT]._iSpell);
	}
}

void CheckPlrSpell(bool isShiftHeld, spell_id spellID, spell_type spellType)
{
	bool addflag = false;
	int sl;

	if ((DWORD)MyPlayerId >= MAX_PLRS) {
		app_fatal("CheckPlrSpell: illegal player %i", MyPlayerId);
	}
	auto &myPlayer = Players[MyPlayerId];

	if (spellID == SPL_INVALID) {
		myPlayer.Say(HeroSpeech::IDontHaveASpellReady);
		return;
	}

	if (leveltype == DTYPE_TOWN && !spelldata[spellID].sTownSpell) {
		myPlayer.Say(HeroSpeech::ICantCastThatHere);
		return;
	}

	if (ControlMode == ControlTypes::KeyboardAndMouse) {
		if (pcurs != CURSOR_HAND)
			return;

		if (GetMainPanel().Contains(MousePosition)) // inside main panel
			return;

		if (
		    ((chrflag || QuestLogIsOpen || IsStashOpen) && GetLeftPanel().Contains(MousePosition)) // inside left panel
		    || ((invflag || sbookflag) && GetRightPanel().Contains(MousePosition))                 // inside right panel
		) {
			if (spellID != SPL_HEAL
			    && spellID != SPL_IDENTIFY
			    && spellID != SPL_REPAIR
			    && spellID != SPL_INFRA
			    && spellID != SPL_RECHARGE)
				return;
		}
	}
	SpellCheckResult spellcheck = SpellCheckResult::Success;
	switch (spellType) {
	case RSPLTYPE_SKILL:
	case RSPLTYPE_SPELL:
		spellcheck = CheckSpell(MyPlayerId, spellID, spellType, false);
		addflag = spellcheck == SpellCheckResult::Success;
		break;
	case RSPLTYPE_SCROLL:
		addflag = UseScroll(spellID);
		break;
	case RSPLTYPE_CHARGES:
		addflag = UseStaff(spellID);
		break;
	case RSPLTYPE_INVALID:
		return;
	}

	if (!addflag) {
		if (spellType == RSPLTYPE_SPELL) {
			switch (spellcheck) {
			case SpellCheckResult::Fail_NoMana:
				myPlayer.Say(HeroSpeech::NotEnoughMana);
				break;
			case SpellCheckResult::Fail_Level0:
				myPlayer.Say(HeroSpeech::ICantCastThatYet);
				break;
			default:
				myPlayer.Say(HeroSpeech::ICantDoThat);
				break;
			}
			LastMouseButtonAction = MouseActionType::None;
		}
		return;
	}

	if (IsWallSpell(spellID)) {
		LastMouseButtonAction = MouseActionType::Spell;
		Direction sd = GetDirection(myPlayer.position.tile, cursPosition);
		sl = GetSpellLevel(MyPlayerId, spellID);
		NetSendCmdLocParam4(true, CMD_SPELLXYD, cursPosition, spellID, spellType, static_cast<uint16_t>(sd), sl);
	} else if (pcursmonst != -1 && !isShiftHeld) {
		LastMouseButtonAction = MouseActionType::SpellMonsterTarget;
		sl = GetSpellLevel(MyPlayerId, spellID);
		NetSendCmdParam4(true, CMD_SPELLID, pcursmonst, spellID, spellType, sl);
	} else if (pcursplr != -1 && !isShiftHeld && !gbFriendlyMode) {
		LastMouseButtonAction = MouseActionType::SpellPlayerTarget;
		sl = GetSpellLevel(MyPlayerId, spellID);
		NetSendCmdParam4(true, CMD_SPELLPID, pcursplr, spellID, spellType, sl);
	} else {
		LastMouseButtonAction = MouseActionType::Spell;
		sl = GetSpellLevel(MyPlayerId, spellID);
		NetSendCmdLocParam3(true, CMD_SPELLXY, cursPosition, spellID, spellType, sl);
	}
}

void SyncPlrAnim(int pnum)
{
	if ((DWORD)pnum >= MAX_PLRS) {
		app_fatal("SyncPlrAnim: illegal player %i", pnum);
	}
	auto &player = Players[pnum];

	player_graphic graphic;
	switch (player._pmode) {
	case PM_STAND:
	case PM_NEWLVL:
	case PM_QUIT:
		graphic = player_graphic::Stand;
		break;
	case PM_WALK:
	case PM_WALK2:
	case PM_WALK3:
		graphic = player_graphic::Walk;
		break;
	case PM_ATTACK:
	case PM_RATTACK:
		graphic = player_graphic::Attack;
		break;
	case PM_BLOCK:
		graphic = player_graphic::Block;
		break;
	case PM_SPELL:
		graphic = player_graphic::Fire;
		if (pnum == MyPlayerId) {
			switch (spelldata[player._pSpell].sType) {
			case STYPE_FIRE:
				graphic = player_graphic::Fire;
				break;
			case STYPE_LIGHTNING:
				graphic = player_graphic::Lightning;
				break;
			case STYPE_MAGIC:
				graphic = player_graphic::Magic;
				break;
			}
		}
		break;
	case PM_GOTHIT:
		graphic = player_graphic::Hit;
		break;
	case PM_DEATH:
		graphic = player_graphic::Death;
		break;
	default:
		app_fatal("SyncPlrAnim");
	}

	player.AnimInfo.celSprite = player.AnimationData[static_cast<size_t>(graphic)].GetCelSpritesForDirection(player._pdir);
	// Ensure ScrollInfo is initialized correctly
	ScrollViewPort(player, WalkSettings[static_cast<size_t>(player._pdir)].scrollDir);
}

void SyncInitPlrPos(int pnum)
{
	auto &player = Players[pnum];

	if (!gbIsMultiplayer || player.plrlevel != currlevel) {
		return;
	}

	Point position = [&]() {
		for (int i = 0; i < 8; i++) {
			Point position = player.position.tile + Displacement { plrxoff2[i], plryoff2[i] };
			if (PosOkPlayer(player, position))
				return position;
		}

		std::optional<Point> nearPosition = FindClosestValidPosition(
		    [&player](Point testPosition) {
			    return PosOkPlayer(player, testPosition) && !PosOkPortal(currlevel, testPosition.x, testPosition.y);
		    },
		    player.position.tile,
		    1, // skip the starting tile since that was checked in the previous loop
		    50);

		return nearPosition.value_or(Point { 0, 0 });
	}();

	player.position.tile = position;
	dPlayer[position.x][position.y] = pnum + 1;

	if (pnum == MyPlayerId) {
		player.position.future = position;
		ViewPosition = position;
	}
}

void SyncInitPlr(int pnum)
{
	if ((DWORD)pnum >= MAX_PLRS) {
		app_fatal("SyncInitPlr: illegal player %i", pnum);
	}
	auto &player = Players[pnum];

	SetPlrAnims(player);
	SyncInitPlrPos(pnum);
	if (pnum != MyPlayerId)
		player._plid = NO_LIGHT;
}

void CheckStats(Player &player)
{
	for (auto attribute : enum_values<CharacterAttribute>()) {
		int maxStatPoint = player.GetMaximumAttributeValue(attribute);
		switch (attribute) {
		case CharacterAttribute::Strength:
			player._pBaseStr = clamp(player._pBaseStr, 0, maxStatPoint);
			break;
		case CharacterAttribute::Magic:
			player._pBaseMag = clamp(player._pBaseMag, 0, maxStatPoint);
			break;
		case CharacterAttribute::Dexterity:
			player._pBaseDex = clamp(player._pBaseDex, 0, maxStatPoint);
			break;
		case CharacterAttribute::Vitality:
			player._pBaseVit = clamp(player._pBaseVit, 0, maxStatPoint);
			break;
		}
	}
}

void ModifyPlrStr(int p, int l)
{
	if ((DWORD)p >= MAX_PLRS) {
		app_fatal("ModifyPlrStr: illegal player %i", p);
	}
	auto &player = Players[p];

	l = clamp(l, 0 - player._pBaseStr, player.GetMaximumAttributeValue(CharacterAttribute::Strength) - player._pBaseStr);

	player._pStrength += l;
	player._pBaseStr += l;

	CalcPlrInv(player, true);

	if (p == MyPlayerId) {
		NetSendCmdParam1(false, CMD_SETSTR, player._pBaseStr);
	}
}

void ModifyPlrMag(int p, int l)
{
	if ((DWORD)p >= MAX_PLRS) {
		app_fatal("ModifyPlrMag: illegal player %i", p);
	}
	auto &player = Players[p];

	l = clamp(l, 0 - player._pBaseStr, player.GetMaximumAttributeValue(CharacterAttribute::Magic) - player._pBaseMag);

	player._pMagic += l;
	player._pBaseMag += l;

	int ms = l << 6;
	if (player._pClass == HeroClass::Sorcerer) {
		ms *= 2;
	} else if (player._pClass == HeroClass::Bard) {
		ms += ms / 2;
	}

	player._pMaxManaBase += ms;
	player._pMaxMana += ms;
	if (HasNoneOf(player._pIFlags, ItemSpecialEffect::NoMana)) {
		player._pManaBase += ms;
		player._pMana += ms;
	}

	CalcPlrInv(player, true);

	if (p == MyPlayerId) {
		NetSendCmdParam1(false, CMD_SETMAG, player._pBaseMag);
	}
}

void ModifyPlrDex(int p, int l)
{
	if ((DWORD)p >= MAX_PLRS) {
		app_fatal("ModifyPlrDex: illegal player %i", p);
	}
	auto &player = Players[p];

	l = clamp(l, 0 - player._pBaseDex, player.GetMaximumAttributeValue(CharacterAttribute::Dexterity) - player._pBaseDex);

	player._pDexterity += l;
	player._pBaseDex += l;
	CalcPlrInv(player, true);

	if (p == MyPlayerId) {
		NetSendCmdParam1(false, CMD_SETDEX, player._pBaseDex);
	}
}

void ModifyPlrVit(int p, int l)
{
	if ((DWORD)p >= MAX_PLRS) {
		app_fatal("ModifyPlrVit: illegal player %i", p);
	}
	auto &player = Players[p];

	l = clamp(l, 0 - player._pBaseVit, player.GetMaximumAttributeValue(CharacterAttribute::Vitality) - player._pBaseVit);

	player._pVitality += l;
	player._pBaseVit += l;

	int ms = l << 6;
	if (player._pClass == HeroClass::Warrior || player._pClass == HeroClass::Barbarian) {
		ms *= 2;
	}

	player._pHPBase += ms;
	player._pMaxHPBase += ms;
	player._pHitPoints += ms;
	player._pMaxHP += ms;

	CalcPlrInv(player, true);

	if (p == MyPlayerId) {
		NetSendCmdParam1(false, CMD_SETVIT, player._pBaseVit);
	}
}

void SetPlayerHitPoints(Player &player, int val)
{
	player._pHitPoints = val;
	player._pHPBase = val + player._pMaxHPBase - player._pMaxHP;

	if (&player == &Players[MyPlayerId]) {
		drawhpflag = true;
	}
}

void SetPlrStr(Player &player, int v)
{
	player._pBaseStr = v;
	CalcPlrInv(player, true);
}

void SetPlrMag(Player &player, int v)
{
	player._pBaseMag = v;

	int m = v << 6;
	if (player._pClass == HeroClass::Sorcerer) {
		m *= 2;
	} else if (player._pClass == HeroClass::Bard) {
		m += m / 2;
	}

	player._pMaxManaBase = m;
	player._pMaxMana = m;
	CalcPlrInv(player, true);
}

void SetPlrDex(Player &player, int v)
{
	player._pBaseDex = v;
	CalcPlrInv(player, true);
}

void SetPlrVit(Player &player, int v)
{
	player._pBaseVit = v;

	int hp = v << 6;
	if (player._pClass == HeroClass::Warrior || player._pClass == HeroClass::Barbarian) {
		hp *= 2;
	}

	player._pHPBase = hp;
	player._pMaxHPBase = hp;
	CalcPlrInv(player, true);
}

void InitDungMsgs(Player &player)
{
	player.pDungMsgs = 0;
	player.pDungMsgs2 = 0;
}

enum {
	// clang-format off
	DungMsgCathedral = 1 << 0,
	DungMsgCatacombs = 1 << 1,
	DungMsgCaves     = 1 << 2,
	DungMsgHell      = 1 << 3,
	DungMsgDiablo    = 1 << 4,
	// clang-format on
};

void PlayDungMsgs()
{
	if ((DWORD)MyPlayerId >= MAX_PLRS) {
		app_fatal("PlayDungMsgs: illegal player %i", MyPlayerId);
	}
	auto &myPlayer = Players[MyPlayerId];

	if (currlevel == 1 && !myPlayer._pLvlVisited[1] && (myPlayer.pDungMsgs & DungMsgCathedral) == 0) {
		myPlayer.Say(HeroSpeech::TheSanctityOfThisPlaceHasBeenFouled, 40);
		myPlayer.pDungMsgs = myPlayer.pDungMsgs | DungMsgCathedral;
	} else if (currlevel == 5 && !myPlayer._pLvlVisited[5] && (myPlayer.pDungMsgs & DungMsgCatacombs) == 0) {
		myPlayer.Say(HeroSpeech::TheSmellOfDeathSurroundsMe, 40);
		myPlayer.pDungMsgs |= DungMsgCatacombs;
	} else if (currlevel == 9 && !myPlayer._pLvlVisited[9] && (myPlayer.pDungMsgs & DungMsgCaves) == 0) {
		myPlayer.Say(HeroSpeech::ItsHotDownHere, 40);
		myPlayer.pDungMsgs |= DungMsgCaves;
	} else if (currlevel == 13 && !myPlayer._pLvlVisited[13] && (myPlayer.pDungMsgs & DungMsgHell) == 0) {
		myPlayer.Say(HeroSpeech::IMustBeGettingClose, 40);
		myPlayer.pDungMsgs |= DungMsgHell;
	} else if (currlevel == 16 && !myPlayer._pLvlVisited[16] && (myPlayer.pDungMsgs & DungMsgDiablo) == 0) {
		sfxdelay = 40;
		sfxdnum = PS_DIABLVLINT;
		myPlayer.pDungMsgs |= DungMsgDiablo;
	} else if (currlevel == 17 && !myPlayer._pLvlVisited[17] && (myPlayer.pDungMsgs2 & 1) == 0) {
		sfxdelay = 10;
		sfxdnum = USFX_DEFILER1;
		Quests[Q_DEFILER]._qactive = QUEST_ACTIVE;
		Quests[Q_DEFILER]._qlog = true;
		Quests[Q_DEFILER]._qmsg = TEXT_DEFILER1;
		myPlayer.pDungMsgs2 |= 1;
	} else if (currlevel == 19 && !myPlayer._pLvlVisited[19] && (myPlayer.pDungMsgs2 & 4) == 0) {
		sfxdelay = 10;
		sfxdnum = USFX_DEFILER3;
		myPlayer.pDungMsgs2 |= 4;
	} else if (currlevel == 21 && !myPlayer._pLvlVisited[21] && (myPlayer.pDungMsgs & 32) == 0) {
		myPlayer.Say(HeroSpeech::ThisIsAPlaceOfGreatPower, 30);
		myPlayer.pDungMsgs |= 32;
	} else {
		sfxdelay = 0;
	}
}

#ifdef BUILD_TESTING
bool TestPlayerDoGotHit(int pnum)
{
	return DoGotHit(pnum);
}
#endif

} // namespace devilution
