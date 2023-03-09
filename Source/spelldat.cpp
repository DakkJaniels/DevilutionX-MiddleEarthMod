/**
 * @file spelldat.cpp
 *
 * Implementation of all spell data.
 */
#include "spelldat.h"
#include "utils/language.h"

namespace devilution {

/** Data related to each spell ID. */
const SpellData spelldata[] = {
	// clang-format off
		// sName,    sManaCost,    sType,               sNameText,                       sSkillText,                       sBookLvl, sStaffLvl, sTargeted, sTownSpell, sMinInt,   sSFX,    sMissiles[3],                                           sManaAdj,  sMinMana, sStaffMin, sStaffMax, sBookCost, sStaffCost
		{SPL_NULL,           0,    STYPE_FIRE,          nullptr,                         nullptr,                                 0,         0, false,     false,          0, SFX_NONE,  { MIS_NULL,         MIS_NULL,                MIS_NULL },         0,         0,        40,        80,         0,       0 },
		{SPL_FIREBOLT,       6,    STYPE_FIRE,          P_("spell", "Firebolt"),         P_("spell", "Firebolt"),                 1,         1, true,      false,         15, IS_CAST2,  { MIS_FIREBOLT,     MIS_NULL,                MIS_NULL },         1,         3,        40,        80,      1000,       50},
		{SPL_HEAL,           5,    STYPE_MAGIC,         P_("spell", "Healing"),          nullptr,                                 1,         1, false,     true,          17, IS_CAST8,  { MIS_HEAL,         MIS_NULL,                MIS_NULL },         3,         1,        20,        40,      1000,       50},
		{SPL_LIGHTNING,      10,   STYPE_LIGHTNING,     P_("spell", "Lightning"),        nullptr,                                 4,         3, true,      false,         20, IS_CAST4,  { MIS_LIGHTCTRL,    MIS_NULL,                MIS_NULL },         1,         6,        20,        60,      3000,      150},
		{SPL_FLASH,          30,   STYPE_LIGHTNING,     P_("spell", "Flash"),            nullptr,                                 5,         4, false,     false,         33, IS_CAST4,  { MIS_FLASH,        MIS_FLASH2,              MIS_NULL },         2,        16,        20,        40,      7500,      500},
		{SPL_IDENTIFY,       13,   STYPE_MAGIC,         P_("spell", "Identify"),         P_("spell", "Identify"),                -1,        -1, false,     true,          23, IS_CAST6,  { MIS_IDENTIFY,     MIS_NULL,                MIS_NULL },         2,         1,         8,        12,         0,      100},
		{SPL_FIREWALL,       28,   STYPE_FIRE,          P_("spell", "Fire Wall"),        nullptr,                                 3,         2, true,      false,         27, IS_CAST2,  { MIS_FIREWALLC,    MIS_NULL,                MIS_NULL },         2,        16,         8,        16,      6000,      400},
		{SPL_TOWN,           35,   STYPE_MAGIC,         P_("spell", "Town Portal"),      nullptr,                                 3,         3, true,      false,         20, IS_CAST6,  { MIS_TOWN,         MIS_NULL,                MIS_NULL },         3,        18,         8,        12,      3000,      200},
		{SPL_STONE,          60,   STYPE_MAGIC,         P_("spell", "Stone Curse"),      nullptr,                                 6,         5, true,      false,         51, IS_CAST2,  { MIS_STONE,        MIS_NULL,                MIS_NULL },         3,        40,         8,        16,     12000,      800},
		{SPL_INFRA,          40,   STYPE_MAGIC,         P_("spell", "Infravision"),      nullptr,                                -1,        -1, false,     false,         36, IS_CAST8,  { MIS_INFRA,        MIS_NULL,                MIS_NULL },         5,        20,         0,         0,         0,      600},
		{SPL_RNDTELEPORT,    40,   STYPE_MAGIC,         P_("spell", "Escape"),           nullptr,                                13,        12, false,     false,        110, IS_CAST3,  { MIS_TOWN,         MIS_TELEPORT,            MIS_NULL },         2,        20,        10,        20,     25000,     3000},
		{SPL_MANASHIELD,     33,   STYPE_MAGIC,         P_("spell", "Mana Shield"),      nullptr,                                 6,         5, false,     false,         25, IS_CAST2,  { MIS_MANASHIELD,   MIS_NULL,                MIS_NULL },         0,        33,         4,        10,     16000,     1200},
		{SPL_FIREBALL,       16,   STYPE_FIRE,          P_("spell", "Fireball"),         nullptr,                                 8,         7, true,      false,         48, IS_CAST2,  { MIS_FIREBALL,     MIS_NULL,                MIS_NULL },         1,        10,        40,        80,      8000,      300},
		{SPL_GUARDIAN,       50,   STYPE_FIRE,          P_("spell", "Guardian"),         nullptr,                                 9,         8, true,      false,         61, IS_CAST2,  { MIS_GUARDIAN,     MIS_NULL,                MIS_NULL },         2,        30,        16,        32,     14000,      950},
		{SPL_CHAIN,          30,   STYPE_LIGHTNING,     P_("spell", "Chain Lightning"),  nullptr,                                 8,         7, false,     false,         54, IS_CAST2,  { MIS_CHAIN,        MIS_NULL,                MIS_NULL },         1,        18,        20,        60,     11000,      750},
		{SPL_WAVE,           35,   STYPE_FIRE,          P_("spell", "Flame wave"),       nullptr,                                 9,         8, true,      false,         54, IS_CAST2,  { MIS_WAVE,         MIS_NULL,                MIS_NULL },         3,        20,        20,        40,     10000,      650},
/*fake*/{SPL_DOOMSERP,       0,    STYPE_LIGHTNING,     P_("spell", "umenorean Sword"),  nullptr,                                -1,        -1, false,     false,          0, IS_CAST2,  { MIS_NULL,         MIS_NULL,                MIS_NULL },         0,         0,        40,        80,         0,        0},
/*fake*/{SPL_BLODRIT,        0,    STYPE_MAGIC,         P_("spell", "Mallorn Bow"),      nullptr,                                -1,        -1, false,     false,          0, IS_CAST2,  { MIS_NULL,         MIS_NULL,                MIS_NULL },         0,         0,        40,        80,         0,        0},
		{SPL_NOVA,           60,   STYPE_MAGIC,         P_("spell", "Nova"),             nullptr,                                -1,        10, false,     false,         87, IS_CAST4,  { MIS_NOVA,         MIS_NULL,                MIS_NULL },         3,        35,        16,        32,     21000,     1300},
/*fake*/{SPL_INVISIBIL,      0,    STYPE_MAGIC,         P_("spell", "War Axe"),          nullptr,                                -1,        -1, false,     false,          0, IS_CAST2,  { MIS_NULL,         MIS_NULL,                MIS_NULL },         0,         0,        40,        80,         0,        0},
		{SPL_FLAME,          11,   STYPE_FIRE,          P_("spell", "Inferno"),          nullptr,                                 3,         2, true,      false,         20, IS_CAST2,  { MIS_FLAMEC,       MIS_NULL,                MIS_NULL },         1,         6,        20,        40,      2000,      100},
		{SPL_GOLEM,          100,  STYPE_FIRE,          P_("spell", "Golem"),            nullptr,                                11,         9, false,     false,         81, IS_CAST2,  { MIS_GOLEM,        MIS_NULL,                MIS_NULL },         6,        60,        16,        32,     18000,     1100},
		{SPL_THUNDER,        30,   STYPE_LIGHTNING,     P_("spell", "Thunder"),          nullptr,                                12,        10, true,      false,         55, IS_CAST4,  { MIS_LIGHTCTRL,    MIS_LIGHTCTRL,      MIS_LIGHTCTRL },         2,        18,        15,        30,     10000,      500},
		{SPL_TELEPORT,       35,   STYPE_MAGIC,         P_("spell", "Teleport"),         nullptr,                                14,        12, true,      true,         105, IS_CAST6,  { MIS_TELEPORT,     MIS_NULL,                MIS_NULL },         2,        15,        16,        32,     20000,     1250},
		{SPL_APOCA,          150,  STYPE_FIRE,          P_("spell", "Apocalypse"),       nullptr,                                -1,        15, false,     false,        149, IS_CAST2,  { MIS_APOCA,        MIS_NULL,                MIS_NULL },         6,        90,         8,        12,     30000,     2000},
		{SPL_ETHEREALIZE,    100,  STYPE_MAGIC,         P_("spell", "Etherealize"),      nullptr,                                -1,        -1, false,     false,         93, IS_CAST2,  { MIS_ETHEREALIZE,  MIS_NULL,                MIS_NULL },         0,       100,         2,         6,     26000,     1600},
		{SPL_REPAIR,         0,    STYPE_MAGIC,         P_("spell", "Item Repair"),      P_("spell", "Item Repair"),             -1,        -1, false,     true,          -1, IS_CAST6,  { MIS_REPAIR,       MIS_NULL,                MIS_NULL },         0,         0,        40,        80,         0,        0},
		{SPL_RECHARGE,       0,    STYPE_MAGIC,         P_("spell", "Staff Recharge"),   P_("spell", "Staff Recharge"),          -1,        -1, false,     true,          -1, IS_CAST6,  { MIS_RECHARGE,     MIS_NULL,                MIS_NULL },         0,         0,        40,        80,         0,        0},
		{SPL_DISARM,         0,    STYPE_MAGIC,         P_("spell", "Trap Disarm"),      P_("spell", "Trap Disarm"),             -1,        -1, false,     false,         -1, IS_CAST6,  { MIS_DISARM,       MIS_NULL,                MIS_NULL },         0,         0,        40,        80,         0,        0},
		{SPL_ELEMENT,        35,   STYPE_FIRE,          P_("spell", "Elemental"),        nullptr,                                 8,         6, false,     false,         68, IS_CAST2,  { MIS_ELEMENT,      MIS_NULL,                MIS_NULL },         2,        20,        20,        60,     10500,      700},
		{SPL_CBOLT,          6,    STYPE_LIGHTNING,     P_("spell", "Charged Bolt"),     nullptr,                                 1,         1, true,      false,         25, IS_CAST2,  { MIS_CBOLT,        MIS_NULL,                MIS_NULL },         1,         6,        40,        80,      1000,       50},
		{SPL_HBOLT,          7,    STYPE_MAGIC,         P_("spell", "Holy Bolt"),        nullptr,                                 1,         1, true,      false,         20, IS_CAST2,  { MIS_HBOLT,        MIS_NULL,                MIS_NULL },         1,         3,        40,        80,      1000,       50},
		{SPL_RESURRECT,      30,   STYPE_MAGIC,         P_("spell", "Resurrect"),        nullptr,                                 7,         5, false,     true,          50, IS_CAST8,  { MIS_RESURRECT,    MIS_NULL,                MIS_NULL },         0,         2,         4,        10,      7500,      250},
		{SPL_TELEKINESIS,    15,   STYPE_MAGIC,         P_("spell", "Telekinesis"),      nullptr,                                 2,         2, false,     false,         33, IS_CAST2,  { MIS_TELEKINESIS,  MIS_NULL,                MIS_NULL },         2,         8,        20,        40,      2500,      200},
		{SPL_HEALOTHER,      5,    STYPE_MAGIC,         P_("spell", "Heal Other"),       nullptr,                                 1,         1, false,     true,          17, IS_CAST8,  { MIS_HEALOTHER,    MIS_NULL,                MIS_NULL },         3,         1,        20,        40,      1000,       50},
		{SPL_BSTAR,          20,   STYPE_MAGIC,         P_("spell", "Blood Star"),       nullptr,                                14,        13, false,     false,         70, IS_CAST2,  { MIS_BSTAR,        MIS_NULL,                MIS_NULL },         1,         5,        20,        60,     27500,     1800},
		{SPL_BONESPIRIT,     24,   STYPE_MAGIC,         P_("spell", "Bone Spirit"),      nullptr,                                 9,         7, false,     false,         34, IS_CAST2,  { MIS_BONESPIRIT,   MIS_NULL,                MIS_NULL },         1,        12,        20,        60,     11500,      800},
	// clang-format on
};

} // namespace devilution
