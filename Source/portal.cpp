/**
 * @file portal.cpp
 *
 * Implementation of functionality for handling town portals.
 */
#include "portal.h"

#include "lighting.h"
#include "misdat.h"
#include "missiles.h"
#include "multi.h"
#include "player.h"

namespace devilution {

/** In-game state of portals. */
Portal Portals[MAXPORTAL];

namespace {

/** Current portal number (a portal array index). */
int portalindex;

/** Coordinate of each players portal in town. */
Point WarpDrop[MAXPORTAL] = {
	{ 57, 67 },
	{ 57, 74 },
	{ 64, 67 },
	{ 64, 74 },
};

} // namespace

void InitPortals()
{
	for (auto &portal : Portals) {
		portal.open = false;
	}
}

void SetPortalStats(int i, bool o, int x, int y, int lvl, dungeon_type lvltype)
{
	Portals[i].open = o;
	Portals[i].position = { x, y };
	Portals[i].level = lvl;
	Portals[i].ltype = lvltype;
	Portals[i].setlvl = false;
}

void AddWarpMissile(int i, Point position)
{
	MissilesData[MIS_TOWN].mlSFX = SFX_NONE;

	auto *missile = AddMissile({ 0, 0 }, position, Direction::South, MIS_TOWN, TARGET_MONSTERS, i, 0, 0);
	if (missile != nullptr) {
		SetMissDir(*missile, 1);

		if (currlevel != 0)
			missile->_mlid = AddLight(missile->position.tile, 15);
	}

	MissilesData[MIS_TOWN].mlSFX = LS_SENTINEL;
}

void SyncPortals()
{
	for (int i = 0; i < MAXPORTAL; i++) {
		if (!Portals[i].open)
			continue;
		if (currlevel == 0)
			AddWarpMissile(i, WarpDrop[i]);
		else {
			int lvl = currlevel;
			if (setlevel)
				lvl = setlvlnum;
			if (Portals[i].level == lvl && Portals[i].setlvl == setlevel)
				AddWarpMissile(i, Portals[i].position);
		}
	}
}

void AddInTownPortal(int i)
{
	AddWarpMissile(i, WarpDrop[i]);
}

void ActivatePortal(int i, Point position, int lvl, dungeon_type dungeonType, bool isSetLevel)
{
	Portals[i].open = true;

	if (lvl != 0) {
		Portals[i].position = position;
		Portals[i].level = lvl;
		Portals[i].ltype = dungeonType;
		Portals[i].setlvl = isSetLevel;
	}
}

void DeactivatePortal(int i)
{
	Portals[i].open = false;
}

bool PortalOnLevel(int i)
{
	if (Portals[i].level == currlevel)
		return true;

	return currlevel == 0;
}

void RemovePortalMissile(int id)
{
	Missiles.remove_if([id](Missile &missile) {
		if (missile._mitype == MIS_TOWN && missile._misource == id) {
			dFlags[missile.position.tile.x][missile.position.tile.y] &= ~DungeonFlag::Missile;

			if (Portals[id].level != 0)
				AddUnLight(missile._mlid);

			return true;
		}
		return false;
	});
}

void SetCurrentPortal(int p)
{
	portalindex = p;
}

void GetPortalLevel()
{
	if (currlevel != 0) {
		setlevel = false;
		currlevel = 0;
		Players[MyPlayerId].plrlevel = 0;
		leveltype = DTYPE_TOWN;
		return;
	}

	if (Portals[portalindex].setlvl) {
		setlevel = true;
		setlvlnum = (_setlevels)Portals[portalindex].level;
		currlevel = Portals[portalindex].level;
		Players[MyPlayerId].plrlevel = setlvlnum;
		leveltype = Portals[portalindex].ltype;
	} else {
		setlevel = false;
		currlevel = Portals[portalindex].level;
		Players[MyPlayerId].plrlevel = currlevel;
		leveltype = Portals[portalindex].ltype;
	}

	if (portalindex == MyPlayerId) {
		NetSendCmd(true, CMD_DEACTIVATEPORTAL);
		DeactivatePortal(portalindex);
	}
}

void GetPortalLvlPos()
{
	if (currlevel == 0) {
		ViewPosition = WarpDrop[portalindex] + Displacement { 1, 1 };
	} else {
		ViewPosition = Portals[portalindex].position;

		if (portalindex != MyPlayerId) {
			ViewPosition.x++;
			ViewPosition.y++;
		}
	}
}

bool PosOkPortal(int lvl, int x, int y)
{
	for (auto &portal : Portals) {
		if (portal.open && portal.level == lvl && ((portal.position.x == x && portal.position.y == y) || (portal.position.x == x - 1 && portal.position.y == y - 1)))
			return true;
	}
	return false;
}

} // namespace devilution
