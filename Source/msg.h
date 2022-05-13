/**
 * @file msg.h
 *
 * Interface of function for sending and reciving network messages.
 */
#pragma once

#include <cstdint>

#include "engine/point.hpp"
#include "items.h"
#include "monster.h"
#include "objects.h"
#include "portal.h"
#include "quests.h"

namespace devilution {

#define MAX_SEND_STR_LEN 80
#define MAXMULTIQUESTS 10

enum _cmd_id : uint8_t {
	// Player mode standing.
	//
	// body (TCmd)
	CMD_STAND,
	// Walk to location.
	//
	// body (TCmdLoc)
	CMD_WALKXY,
	// Acknowledge receive of player info.
	//
	// body (TCmdPlrInfoHdr)
	CMD_ACK_PLRINFO,
	// Increment player strength.
	//
	// body (TCmdParam1):
	//    int16_t delta
	CMD_ADDSTR,
	// Increment player magic.
	//
	// body (TCmdParam1):
	//    int16_t delta
	CMD_ADDMAG,
	// Increment player dexterity.
	//
	// body (TCmdParam1):
	//    int16_t delta
	CMD_ADDDEX,
	// Increment player vitality.
	//
	// body (TCmdParam1):
	//    int16_t delta
	CMD_ADDVIT,
	// Lift item to hand.
	//
	// body (TCmdGItem)
	CMD_GETITEM,
	// Loot item to inventory.
	//
	// body (TCmdGItem)
	CMD_AGETITEM,
	// Drop item from hand on ground.
	//
	// body (TCmdPItem)
	CMD_PUTITEM,
	// Spawn item on ground (place quest items).
	//
	// body (TCmdPItem)
	CMD_SPAWNITEM,
	// Respawn item on ground (drop dead player item, or drop attempted loot item
	// when inventory is full).
	//
	// body (TCmdPItem)
	CMD_RESPAWNITEM,
	// Attack target location.
	//
	// body (TCmdLoc)
	CMD_ATTACKXY,
	// Range attack target location.
	//
	// body (TCmdLoc)
	CMD_RATTACKXY,
	// Cast spell at target location.
	//
	// body (TCmdLocParam2):
	//    int8_t x
	//    int8_t y
	//    int16_t spell_id
	//    int16_t spell_lvl
	CMD_SPELLXY,
	// Cast targetted spell at target location.
	//
	// body (TCmdLocParam2):
	//    int8_t x
	//    int8_t y
	//    int16_t spell_id
	//    int16_t spell_lvl
	CMD_TSPELLXY,
	// Operate object at location.
	//
	// body (TCmdLocParam1):
	//    int8_t x
	//    int8_t y
	//    int16_t object_num
	CMD_OPOBJXY,
	// Disarm trap at location.
	//
	// body (TCmdLocParam1):
	//    int8_t x
	//    int8_t y
	//    int16_t object_num
	CMD_DISARMXY,
	// Attack target monster.
	//
	// body (TCmdParam1):
	//    int16_t monster_num
	CMD_ATTACKID,
	// Attack target player.
	//
	// body (TCmdParam1):
	//    int16_t player_num
	CMD_ATTACKPID,
	// Range attack target monster.
	//
	// body (TCmdParam1):
	//    int16_t monster_num
	CMD_RATTACKID,
	// Range attack target player.
	//
	// body (TCmdParam1):
	//    int16_t player_num
	CMD_RATTACKPID,
	// Cast spell on target monster.
	//
	// body (TCmdParam3):
	//    int16_t monster_num
	//    int16_t spell_id
	//    int16_t spell_lvl
	CMD_SPELLID,
	// Cast spell on target player.
	//
	// body (TCmdParam3):
	//    int16_t player_num
	//    int16_t spell_id
	//    int16_t spell_lvl
	CMD_SPELLPID,
	// Cast targetted spell on target monster.
	//
	// body (TCmdParam3):
	//    int16_t monster_num
	//    int16_t spell_id
	//    int16_t spell_lvl
	CMD_TSPELLID,
	// Cast targetted spell on target player.
	//
	// body (TCmdParam3):
	//    int16_t player_num
	//    int16_t spell_id
	//    int16_t spell_lvl
	CMD_TSPELLPID,
	// Cast resurrect spell on target player.
	//
	// body (TCmdParam1):
	//    int16_t player_num
	CMD_RESURRECT,
	// Operate object using telekinesis.
	//
	// body (TCmdParam1):
	//    int16_t object_num
	CMD_OPOBJT,
	// Knockback target monster using telekinesis.
	//
	// body (TCmdParam1):
	//    int16_t monster_num
	CMD_KNOCKBACK,
	// Talk with towner at location.
	//
	// body (TCmdLocParam1):
	//    int8_t x
	//    int8_t y
	//    int16_t towner_num
	CMD_TALKXY,
	// Enter new dungeon level.
	//
	// body (TCmdParam2):
	//    int16_t trig_msg
	//    int16_t level
	CMD_NEWLVL,
	// Enter target portal.
	//
	// body (TCmdParam1):
	//    int16_t portal_num
	CMD_WARP,
	// Cheat: give player level up.
	//
	// body (TCmd)
	CMD_CHEAT_EXPERIENCE,
	// Cheat: increase active spell level of player.
	//
	// body (TCmd)
	CMD_CHEAT_SPELL_LEVEL,
	// Debug command (nop).
	//
	// body (TCmd)
	CMD_DEBUG,
	// Synchronize data of unvisited dungeon level (state of objects, items and
	// monsters).
	//
	// body (TSyncHeader, TSyncMonster+)
	CMD_SYNCDATA,
	// Monster death at location.
	//
	// body (TCmdLocParam1):
	//    int8_t x
	//    int8_t y
	//    int16_t monster_num
	CMD_MONSTDEATH,
	// Damage target monster.
	//
	// body (TCmdParam2):
	//    int16_t monster_num
	//    int16_t damage
	CMD_MONSTDAMAGE,
	// Player death.
	//
	// body (TCmdParam1):
	//    int16_t ear_flag
	CMD_PLRDEAD,
	// Lift item to hand request.
	//
	// body (TCmdGItem)
	CMD_REQUESTGITEM,
	// Loot item to inventory request.
	//
	// body (TCmdGItem)
	CMD_REQUESTAGITEM,
	// Lift item to hand at location.
	//
	// body (TCmdLocParam1):
	//    int8_t x
	//    int8_t y
	//    int16_t item_num
	CMD_GOTOGETITEM,
	// Loot item to inventory at location.
	//
	// body (TCmdLocParam1):
	//    int8_t x
	//    int8_t y
	//    int16_t item_num
	CMD_GOTOAGETITEM,
	// Open target door.
	//
	// body (TCmdParam1):
	//    int16_t object_num
	CMD_OPENDOOR,
	// Close target door.
	//
	// body (TCmdParam1):
	//    int16_t object_num
	CMD_CLOSEDOOR,
	// Operate object.
	//
	// body (TCmdParam1):
	//    int16_t object_num
	CMD_OPERATEOBJ,
	// Player operate object.
	//
	// body (TCmdParam2):
	//    int16_t player_num
	//    int16_t object_num
	CMD_PLROPOBJ,
	// Break object.
	//
	// body (TCmdParam2):
	//    int16_t player_num
	//    int16_t object_num
	CMD_BREAKOBJ,
	// Equip item for player.
	//
	// body (TCmdChItem)
	CMD_CHANGEPLRITEMS,
	// Remove equipped item (destroy equipped item, swap equipped item, unequip
	// equipped item).
	//
	// body (TCmdDelItem)
	CMD_DELPLRITEMS,
	// Damage target player.
	//
	// body (TCmdDamage)
	CMD_PLRDAMAGE,
	// Set player level.
	//
	// body (TCmdParam1):
	//    int16_t clvl
	CMD_PLRLEVEL,
	// Place item on ground (e.g. monster item drop, chest/barrel/sarcophagus
	// item drop, etc).
	//
	// body (TCmdPItem)
	CMD_DROPITEM,
	// Player join dungeon level at location.
	//
	// body (TCmdLocParam1):
	//    int8_t x
	//    int8_t y
	//    int16_t dlvl
	CMD_PLAYER_JOINLEVEL,
	// Acknowledge receive of player info.
	//
	// body (TCmdPlrInfoHdr)
	CMD_SEND_PLRINFO,
	// Shift attack target location.
	//
	// body (TCmdLoc)
	CMD_SATTACKXY,
	// Activate town portal at location.
	//
	// body (TCmdLocParam3):
	//    int8_t x
	//    int8_t y
	//    int16_t level
	//    int16_t dtype
	//    int16_t is_setlevel
	CMD_ACTIVATEPORTAL,
	// Deactivate portal of player.
	//
	// body (TCmd)
	CMD_DEACTIVATEPORTAL,
	// Delta information for dungeon level 0 through 24.
	//
	// body (TCmdPlrInfoHdr)
	CMD_DLEVEL_0,
	CMD_DLEVEL_1,
	CMD_DLEVEL_2,
	CMD_DLEVEL_3,
	CMD_DLEVEL_4,
	CMD_DLEVEL_5,
	CMD_DLEVEL_6,
	CMD_DLEVEL_7,
	CMD_DLEVEL_8,
	CMD_DLEVEL_9,
	CMD_DLEVEL_10,
	CMD_DLEVEL_11,
	CMD_DLEVEL_12,
	CMD_DLEVEL_13,
	CMD_DLEVEL_14,
	CMD_DLEVEL_15,
	CMD_DLEVEL_16,
	CMD_DLEVEL_17,
	CMD_DLEVEL_18,
	CMD_DLEVEL_19,
	CMD_DLEVEL_20,
	CMD_DLEVEL_21,
	CMD_DLEVEL_22,
	CMD_DLEVEL_23,
	CMD_DLEVEL_24,
	// Delta information of quest and portal states.
	//
	// body (TCmdPlrInfoHdr)
	CMD_DLEVEL_JUNK,
	// Delta information end marker.
	//
	// body (TCmdPlrInfoHdr)
	CMD_DLEVEL_END,
	// Cast heal other spell on target player.
	//
	// body (TCmdParam1):
	//    int16_t player_num
	CMD_HEALOTHER,
	// Chat message.
	//
	// body (TCmdString)
	CMD_STRING,
	// Set player strength.
	//
	// body (TCmdParam1):
	//    int16_t str
	CMD_SETSTR,
	// Set player magic.
	//
	// body (TCmdParam1):
	//    int16_t mag
	CMD_SETMAG,
	// Set player dexterity.
	//
	// body (TCmdParam1):
	//    int16_t dex
	CMD_SETDEX,
	// Set player vitality.
	//
	// body (TCmdParam1):
	//    int16_t vit
	CMD_SETVIT,
	// Restart in town.
	//
	// body (TCmd)
	CMD_RETOWN,
	// Cast spell with direction at target location (e.g. firewall).
	//
	// body (TCmdLocParam3):
	//    int8_t x
	//    int8_t y
	//    int16_t spell_id
	//    int16_t dir
	//    int16_t spell_lvl
	CMD_SPELLXYD,
	// Track (dungeon generated) item looted by other player on dungeon level not
	// yet visited by player. The item is tracked as "already taken" in the delta
	// table, so it is not generated twice on the same dungeon level.
	//
	// body (TCmdGItem)
	CMD_ITEMEXTRA,
	// Synchronize item drop state.
	//
	// body (TCmdPItem)
	CMD_SYNCPUTITEM,
	// Golem death at location.
	//
	// body (TCmdLocParam1):
	//    int8_t x
	//    int8_t y
	//    int16_t dlvl
	CMD_KILLGOLEM,
	// Synchronize quest state.
	//
	// body (TCmdQuest)
	CMD_SYNCQUEST,
	// Spawn golem at target location.
	//
	// body (TCmdGolem)
	CMD_AWAKEGOLEM,
	// Cast nova spell at target location.
	//
	// body (TCmdLoc)
	CMD_NOVA,
	// Enable mana shield of player (render).
	//
	// body (TCmd)
	CMD_SETSHIELD,
	// Disable mana shield of player (don't render).
	//
	// body (TCmd)
	CMD_REMSHIELD,
	CMD_SETREFLECT,
	// Set Etherealize Duration of Player (render).
	//
	// body (TCmd)
	CMD_SETETHEREALIZE,
	CMD_NAKRUL,
	CMD_OPENHIVE,
	CMD_OPENCRYPT,
	// Fake command; set current player for succeeding mega pkt buffer messages.
	//
	// body (TFakeCmdPlr)
	FAKE_CMD_SETID,
	// Fake command; drop mega pkt buffer messages of specified player.
	//
	// body (TFakeDropPlr)
	FAKE_CMD_DROPID,
	NUM_CMDS,

	CMD_INVALID = 0xFF,
};

#pragma pack(push, 1)
struct TCmd {
	_cmd_id bCmd;
};

struct TCmdLoc {
	_cmd_id bCmd;
	uint8_t x;
	uint8_t y;
};

struct TCmdLocParam1 {
	_cmd_id bCmd;
	uint8_t x;
	uint8_t y;
	uint16_t wParam1;
};

struct TCmdLocParam2 {
	_cmd_id bCmd;
	uint8_t x;
	uint8_t y;
	uint16_t wParam1;
	uint16_t wParam2;
};

struct TCmdLocParam3 {
	_cmd_id bCmd;
	uint8_t x;
	uint8_t y;
	uint16_t wParam1;
	uint16_t wParam2;
	uint16_t wParam3;
};

struct TCmdLocParam4 {
	_cmd_id bCmd;
	uint8_t x;
	uint8_t y;
	uint16_t wParam1;
	uint16_t wParam2;
	uint16_t wParam3;
	uint16_t wParam4;
};

struct TCmdParam1 {
	_cmd_id bCmd;
	uint16_t wParam1;
};

struct TCmdParam2 {
	_cmd_id bCmd;
	uint16_t wParam1;
	uint16_t wParam2;
};

struct TCmdParam3 {
	_cmd_id bCmd;
	uint16_t wParam1;
	uint16_t wParam2;
	uint16_t wParam3;
};

struct TCmdParam4 {
	_cmd_id bCmd;
	uint16_t wParam1;
	uint16_t wParam2;
	uint16_t wParam3;
	uint16_t wParam4;
};

struct TCmdGolem {
	_cmd_id bCmd;
	uint8_t _mx;
	uint8_t _my;
	Direction _mdir;
	int8_t _menemy;
	int32_t _mhitpoints;
	uint8_t _currlevel;
};

struct TCmdQuest {
	_cmd_id bCmd;
	int8_t q;
	quest_state qstate;
	uint8_t qlog;
	uint8_t qvar1;
};

/**
 * Represents an item being picked up from the ground
 */
struct TCmdGItem {
	_cmd_id bCmd;
	uint8_t bMaster;
	uint8_t bPnum;
	uint8_t bCursitem;
	uint8_t bLevel;
	uint8_t x;
	uint8_t y;
	_item_indexes wIndx;
	uint16_t wCI;
	int32_t dwSeed;
	uint8_t bId;
	uint8_t bDur;
	uint8_t bMDur;
	uint8_t bCh;
	uint8_t bMCh;
	uint16_t wValue;
	uint32_t dwBuff;
	int32_t dwTime;
	uint16_t wToHit;
	uint16_t wMaxDam;
	uint8_t bMinStr;
	uint8_t bMinMag;
	uint8_t bMinDex;
	int16_t bAC;
};

/**
 * Represents an item being dropped onto the ground
 */
struct TCmdPItem {
	_cmd_id bCmd;
	uint8_t x;
	uint8_t y;
	_item_indexes wIndx;
	uint16_t wCI;
	/**
	 * Item identifier
	 * @see Item::_iSeed
	 */
	int32_t dwSeed;
	uint8_t bId;
	uint8_t bDur;
	uint8_t bMDur;
	uint8_t bCh;
	uint8_t bMCh;
	uint16_t wValue;
	uint32_t dwBuff;
	uint16_t wToHit;
	uint16_t wMaxDam;
	uint8_t bMinStr;
	uint8_t bMinMag;
	uint8_t bMinDex;
	int16_t bAC;

	/**
	 * Items placed during dungeon generation
	 */
	static constexpr _cmd_id FloorItem = CMD_STAND;

	/**
	 * Floor items that have already been picked up
	 */
	static constexpr _cmd_id PickedUpItem = CMD_WALKXY;

	/**
	 * Items dropped by players, monsters, or objects and left on the floor of the dungeon
	 */
	static constexpr _cmd_id DroppedItem = CMD_ACK_PLRINFO;
};

struct TCmdChItem {
	_cmd_id bCmd;
	uint8_t bLoc;
	uint16_t wIndx;
	uint16_t wCI;
	int32_t dwSeed;
	uint8_t bId;
	uint32_t dwBuff;
};

struct TCmdDelItem {
	_cmd_id bCmd;
	uint8_t bLoc;
};

struct TCmdDamage {
	_cmd_id bCmd;
	uint8_t bPlr;
	uint32_t dwDam;
};

struct TCmdMonDamage {
	_cmd_id bCmd;
	uint16_t wMon;
	uint32_t dwDam;
};

struct TCmdPlrInfoHdr {
	_cmd_id bCmd;
	uint16_t wOffset;
	uint16_t wBytes;
};

struct TCmdString {
	_cmd_id bCmd;
	char str[MAX_SEND_STR_LEN];
};

struct TFakeCmdPlr {
	_cmd_id bCmd;
	uint8_t bPlr;
};

struct TFakeDropPlr {
	_cmd_id bCmd;
	uint8_t bPlr;
	uint32_t dwReason;
};

struct TSyncHeader {
	_cmd_id bCmd;
	uint8_t bLevel;
	uint16_t wLen;
	uint8_t bItemI;
	uint8_t bItemX;
	uint8_t bItemY;
	uint16_t wItemIndx;
	uint16_t wItemCI;
	uint32_t dwItemSeed;
	uint8_t bItemId;
	uint8_t bItemDur;
	uint8_t bItemMDur;
	uint8_t bItemCh;
	uint8_t bItemMCh;
	uint16_t wItemVal;
	uint32_t dwItemBuff;
	uint8_t bPInvLoc;
	uint16_t wPInvIndx;
	uint16_t wPInvCI;
	uint32_t dwPInvSeed;
	uint8_t bPInvId;
};

struct TSyncMonster {
	uint8_t _mndx;
	uint8_t _mx;
	uint8_t _my;
	uint8_t _menemy;
	uint8_t _mdelta;
	int32_t _mhitpoints;
	int8_t mWhoHit;
};

struct TPktHdr {
	uint8_t px;
	uint8_t py;
	uint8_t targx;
	uint8_t targy;
	int32_t php;
	int32_t pmhp;
	int32_t mana;
	int32_t maxmana;
	uint8_t bstr;
	uint8_t bmag;
	uint8_t bdex;
	uint16_t wCheck;
	uint16_t wLen;
};

struct TPkt {
	TPktHdr hdr;
	byte body[493];
};

struct DMonsterStr {
	uint8_t _mx;
	uint8_t _my;
	Direction _mdir;
	uint8_t _menemy;
	uint8_t _mactive;
	int32_t _mhitpoints;
	int8_t mWhoHit;
};

struct DObjectStr {
	_cmd_id bCmd;
};

struct DLevel {
	TCmdPItem item[MAXITEMS];
	DObjectStr object[MAXOBJECTS];
	DMonsterStr monster[MAXMONSTERS];
};

struct LocalLevel {
	uint8_t automapsv[DMAXX][DMAXY];
};

struct DPortal {
	uint8_t x;
	uint8_t y;
	uint8_t level;
	uint8_t ltype;
	uint8_t setlvl;
};

struct MultiQuests {
	quest_state qstate;
	uint8_t qlog;
	uint8_t qvar1;
};

struct DJunk {
	DPortal portal[MAXPORTAL];
	MultiQuests quests[MAXMULTIQUESTS];
};
#pragma pack(pop)

struct TBuffer {
	uint32_t dwNextWriteOffset;
	byte bData[4096];
};

extern bool deltaload;
extern uint8_t gbBufferMsgs;
extern int dwRecCount;

void msg_send_drop_pkt(int pnum, int reason);
bool msg_wait_resync();
void run_delta_info();
void DeltaExportData(int pnum);
void DeltaSyncJunk();
void delta_init();
void delta_kill_monster(int mi, Point position, uint8_t bLevel);
void delta_monster_hp(int mi, int hp, uint8_t bLevel);
void delta_sync_monster(const TSyncMonster &monsterSync, uint8_t level);
void DeltaAddItem(int ii);
void DeltaSaveLevel();
void DeltaLoadLevel();
void NetSendCmd(bool bHiPri, _cmd_id bCmd);
void NetSendCmdGolem(uint8_t mx, uint8_t my, Direction dir, uint8_t menemy, int hp, uint8_t cl);
void NetSendCmdLoc(int playerId, bool bHiPri, _cmd_id bCmd, Point position);
void NetSendCmdLocParam1(bool bHiPri, _cmd_id bCmd, Point position, uint16_t wParam1);
void NetSendCmdLocParam2(bool bHiPri, _cmd_id bCmd, Point position, uint16_t wParam1, uint16_t wParam2);
void NetSendCmdLocParam3(bool bHiPri, _cmd_id bCmd, Point position, uint16_t wParam1, uint16_t wParam2, uint16_t wParam3);
void NetSendCmdLocParam4(bool bHiPri, _cmd_id bCmd, Point position, uint16_t wParam1, uint16_t wParam2, uint16_t wParam3, uint16_t wParam4);
void NetSendCmdParam1(bool bHiPri, _cmd_id bCmd, uint16_t wParam1);
void NetSendCmdParam2(bool bHiPri, _cmd_id bCmd, uint16_t wParam1, uint16_t wParam2);
void NetSendCmdParam3(bool bHiPri, _cmd_id bCmd, uint16_t wParam1, uint16_t wParam2, uint16_t wParam3);
void NetSendCmdParam4(bool bHiPri, _cmd_id bCmd, uint16_t wParam1, uint16_t wParam2, uint16_t wParam3, uint16_t wParam4);
void NetSendCmdQuest(bool bHiPri, const Quest &quest);
void NetSendCmdGItem(bool bHiPri, _cmd_id bCmd, uint8_t mast, uint8_t pnum, uint8_t ii);
void NetSendCmdPItem(bool bHiPri, _cmd_id bCmd, Point position, const Item &item);
void NetSendCmdChItem(bool bHiPri, uint8_t bLoc);
void NetSendCmdDelItem(bool bHiPri, uint8_t bLoc);
void NetSendCmdDamage(bool bHiPri, uint8_t bPlr, uint32_t dwDam);
void NetSendCmdMonDmg(bool bHiPri, uint16_t wMon, uint32_t dwDam);
void NetSendCmdString(uint32_t pmask, const char *pszStr);
void delta_close_portal(int pnum);
uint32_t ParseCmd(int pnum, const TCmd *pCmd);

} // namespace devilution
