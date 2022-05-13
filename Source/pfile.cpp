/**
 * @file pfile.cpp
 *
 * Implementation of the save game encoding functionality.
 */
#include "pfile.h"

#include <string>

#include "codec.h"
#include "engine.h"
#include "init.h"
#include "loadsave.h"
#include "menu.h"
#include "mpq/mpq_reader.hpp"
#include "pack.h"
#include "qol/stash.h"
#include "utils/endian.hpp"
#include "utils/file_util.h"
#include "utils/language.h"
#include "utils/paths.h"
#include "utils/utf8.hpp"

namespace devilution {

#define PASSWORD_SPAWN_SINGLE "adslhfb1"
#define PASSWORD_SPAWN_MULTI "lshbkfg1"
#define PASSWORD_SINGLE "xrgyrkj1"
#define PASSWORD_MULTI "szqnlsk1"

bool gbValidSaveFile;

namespace {

MpqWriter SaveWriter;
MpqWriter StashWriter;

/** List of character names for the character selection screen. */
char hero_names[MAX_CHARACTERS][PLR_NAME_LEN];

std::string GetSavePath(uint32_t saveNum)
{
	std::string path = paths::PrefPath();
	const char *ext = ".sv";
	if (gbIsHellfire)
		ext = ".hsv";

	if (gbIsSpawn) {
		if (!gbIsMultiplayer) {
			path.append("spawn_");
		} else {
			path.append("share_");
		}
	} else {
		if (!gbIsMultiplayer) {
			path.append("single_");
		} else {
			path.append("multi_");
		}
	}

	char saveNumStr[21];
	snprintf(saveNumStr, sizeof(saveNumStr) / sizeof(char), "%i", saveNum);
	path.append(saveNumStr);
	path.append(ext);
	return path;
}

std::string GetStashSavePath()
{
	std::string path = paths::PrefPath();
	const char *ext = ".sv";
	if (gbIsHellfire)
		ext = ".hsv";

	if (gbIsSpawn) {
		path.append("stash_spawn");
	} else {
		path.append("stash");
	}
	path.append(ext);
	return path;
}

bool GetPermSaveNames(uint8_t dwIndex, char *szPerm)
{
	const char *fmt;

	if (dwIndex < giNumberOfLevels)
		fmt = "perml%02d";
	else if (dwIndex < giNumberOfLevels * 2) {
		dwIndex -= giNumberOfLevels;
		fmt = "perms%02d";
	} else
		return false;

	sprintf(szPerm, fmt, dwIndex);
	return true;
}

bool GetTempSaveNames(uint8_t dwIndex, char *szTemp)
{
	const char *fmt;

	if (dwIndex < giNumberOfLevels)
		fmt = "templ%02d";
	else if (dwIndex < giNumberOfLevels * 2) {
		dwIndex -= giNumberOfLevels;
		fmt = "temps%02d";
	} else
		return false;

	sprintf(szTemp, fmt, dwIndex);
	return true;
}

void RenameTempToPerm()
{
	char szTemp[MAX_PATH];
	char szPerm[MAX_PATH];

	uint32_t dwIndex = 0;
	while (GetTempSaveNames(dwIndex, szTemp)) {
		[[maybe_unused]] bool result = GetPermSaveNames(dwIndex, szPerm); // DO NOT PUT DIRECTLY INTO ASSERT!
		assert(result);
		dwIndex++;
		if (SaveWriter.HasFile(szTemp)) {
			if (SaveWriter.HasFile(szPerm))
				SaveWriter.RemoveHashEntry(szPerm);
			SaveWriter.RenameFile(szTemp, szPerm);
		}
	}
	assert(!GetPermSaveNames(dwIndex, szPerm));
}

bool ReadHero(MpqArchive &archive, PlayerPack *pPack)
{
	size_t read;

	auto buf = ReadArchive(archive, "hero", &read);
	if (buf == nullptr)
		return false;

	bool ret = false;
	if (read == sizeof(*pPack)) {
		memcpy(pPack, buf.get(), sizeof(*pPack));
		ret = true;
	}

	return ret;
}

void EncodeHero(const PlayerPack *pack)
{
	size_t packedLen = codec_get_encoded_len(sizeof(*pack));
	std::unique_ptr<byte[]> packed { new byte[packedLen] };

	memcpy(packed.get(), pack, sizeof(*pack));
	codec_encode(packed.get(), sizeof(*pack), packedLen, pfile_get_password());
	SaveWriter.WriteFile("hero", packed.get(), packedLen);
}

bool OpenArchive(uint32_t saveNum)
{
	return SaveWriter.Open(GetSavePath(saveNum).c_str());
}

void Game2UiPlayer(const Player &player, _uiheroinfo *heroinfo, bool bHasSaveFile)
{
	CopyUtf8(heroinfo->name, player._pName, sizeof(heroinfo->name));
	heroinfo->level = player._pLevel;
	heroinfo->heroclass = player._pClass;
	heroinfo->strength = player._pStrength;
	heroinfo->magic = player._pMagic;
	heroinfo->dexterity = player._pDexterity;
	heroinfo->vitality = player._pVitality;
	heroinfo->hassaved = bHasSaveFile;
	heroinfo->herorank = player.pDiabloKillLevel;
	heroinfo->spawned = gbIsSpawn;
}

bool GetFileName(uint8_t lvl, char *dst)
{
	const char *fmt;

	if (gbIsMultiplayer) {
		if (lvl != 0)
			return false;
		fmt = "hero";
	} else {
		if (lvl < giNumberOfLevels)
			fmt = "perml%02d";
		else if (lvl < giNumberOfLevels * 2) {
			lvl -= giNumberOfLevels;
			fmt = "perms%02d";
		} else if (lvl == giNumberOfLevels * 2)
			fmt = "game";
		else if (lvl == giNumberOfLevels * 2 + 1)
			fmt = "hero";
		else
			return false;
	}
	sprintf(dst, fmt, lvl);
	return true;
}

bool ArchiveContainsGame(MpqArchive &hsArchive)
{
	if (gbIsMultiplayer)
		return false;

	auto gameData = ReadArchive(hsArchive, "game");
	if (gameData == nullptr)
		return false;

	uint32_t hdr = LoadLE32(gameData.get());

	return IsHeaderValid(hdr);
}

} // namespace

std::optional<MpqArchive> OpenSaveArchive(uint32_t saveNum)
{
	std::int32_t error;
	return MpqArchive::Open(GetSavePath(saveNum).c_str(), error);
}

std::optional<MpqArchive> OpenStashArchive()
{
	std::int32_t error;
	return MpqArchive::Open(GetStashSavePath().c_str(), error);
}

std::unique_ptr<byte[]> ReadArchive(MpqArchive &archive, const char *pszName, size_t *pdwLen)
{
	int32_t error;
	std::size_t length;

	std::unique_ptr<byte[]> result = archive.ReadFile(pszName, length, error);
	if (error != 0)
		return nullptr;

	std::size_t decodedLength = codec_decode(result.get(), length, pfile_get_password());
	if (decodedLength == 0)
		return nullptr;

	if (pdwLen != nullptr)
		*pdwLen = decodedLength;

	return result;
}

const char *pfile_get_password()
{
	if (gbIsSpawn)
		return gbIsMultiplayer ? PASSWORD_SPAWN_MULTI : PASSWORD_SPAWN_SINGLE;
	return gbIsMultiplayer ? PASSWORD_MULTI : PASSWORD_SINGLE;
}

PFileScopedArchiveWriter::PFileScopedArchiveWriter(bool clearTables)
    : save_num_(gSaveNumber)
    , clear_tables_(clearTables)
{
	if (!OpenArchive(save_num_))
		app_fatal("%s", _("Failed to open player archive for writing.").c_str());
}

PFileScopedArchiveWriter::~PFileScopedArchiveWriter()
{
	SaveWriter.Close(clear_tables_);
}

MpqWriter &CurrentSaveArchive()
{
	return SaveWriter;
}

MpqWriter &StashArchive()
{
	return StashWriter;
}

void pfile_write_hero(bool writeGameData, bool clearTables)
{
	PFileScopedArchiveWriter scopedWriter(clearTables);
	if (writeGameData) {
		SaveGameData();
		RenameTempToPerm();
	}
	PlayerPack pkplr;
	auto &myPlayer = Players[MyPlayerId];

	PackPlayer(&pkplr, myPlayer, !gbIsMultiplayer, false);
	EncodeHero(&pkplr);
	if (!gbVanilla) {
		SaveHotkeys();
		SaveHeroItems(myPlayer);
	}
}

void sfile_write_stash()
{
	if (!Stash.dirty)
		return;

	if (!StashWriter.Open(GetStashSavePath().c_str()))
		app_fatal("%s", _("Failed to open stash archive for writing.").c_str());

	SaveStash();

	StashWriter.Close();

	Stash.dirty = false;
}

bool pfile_ui_set_hero_infos(bool (*uiAddHeroInfo)(_uiheroinfo *))
{
	memset(hero_names, 0, sizeof(hero_names));

	for (uint32_t i = 0; i < MAX_CHARACTERS; i++) {
		std::optional<MpqArchive> archive = OpenSaveArchive(i);
		if (archive) {
			PlayerPack pkplr;
			if (ReadHero(*archive, &pkplr)) {
				_uiheroinfo uihero;
				uihero.saveNumber = i;
				strcpy(hero_names[i], pkplr.pName);
				bool hasSaveGame = ArchiveContainsGame(*archive);
				if (hasSaveGame)
					pkplr.bIsHellfire = gbIsHellfireSaveGame ? 1 : 0;

				auto &player = Players[0];

				player = {};

				if (UnPackPlayer(&pkplr, player, false)) {
					LoadHeroItems(player);
					RemoveEmptyInventory(player);
					CalcPlrInv(player, false);

					Game2UiPlayer(player, &uihero, hasSaveGame);
					uiAddHeroInfo(&uihero);
				}
			}
		}
	}

	return true;
}

void pfile_ui_set_class_stats(unsigned int playerClass, _uidefaultstats *classStats)
{
	classStats->strength = StrengthTbl[playerClass];
	classStats->magic = MagicTbl[playerClass];
	classStats->dexterity = DexterityTbl[playerClass];
	classStats->vitality = VitalityTbl[playerClass];
}

uint32_t pfile_ui_get_first_unused_save_num()
{
	uint32_t saveNum;
	for (saveNum = 0; saveNum < MAX_CHARACTERS; saveNum++) {
		if (hero_names[saveNum][0] == '\0')
			break;
	}
	return saveNum;
}

bool pfile_ui_save_create(_uiheroinfo *heroinfo)
{
	PlayerPack pkplr;

	uint32_t saveNum = heroinfo->saveNumber;
	if (saveNum >= MAX_CHARACTERS)
		return false;
	if (!OpenArchive(saveNum))
		return false;
	heroinfo->saveNumber = saveNum;

	giNumberOfLevels = gbIsHellfire ? 25 : 17;

	SaveWriter.RemoveHashEntries(GetFileName);
	CopyUtf8(hero_names[saveNum], heroinfo->name, sizeof(hero_names[saveNum]));

	auto &player = Players[0];
	CreatePlayer(0, heroinfo->heroclass);
	CopyUtf8(player._pName, heroinfo->name, PLR_NAME_LEN);
	PackPlayer(&pkplr, player, true, false);
	EncodeHero(&pkplr);
	Game2UiPlayer(player, heroinfo, false);
	if (!gbVanilla) {
		SaveHotkeys();
		SaveHeroItems(player);
	}

	SaveWriter.Close();
	return true;
}

bool pfile_delete_save(_uiheroinfo *heroInfo)
{
	uint32_t saveNum = heroInfo->saveNumber;
	if (saveNum < MAX_CHARACTERS) {
		hero_names[saveNum][0] = '\0';
		RemoveFile(GetSavePath(saveNum));
	}
	return true;
}

void pfile_read_player_from_save(uint32_t saveNum, Player &player)
{
	player = {};

	PlayerPack pkplr;
	{
		std::optional<MpqArchive> archive = OpenSaveArchive(saveNum);
		if (!archive)
			app_fatal("%s", _("Unable to open archive").c_str());
		if (!ReadHero(*archive, &pkplr))
			app_fatal("%s", _("Unable to load character").c_str());

		gbValidSaveFile = ArchiveContainsGame(*archive);
		if (gbValidSaveFile)
			pkplr.bIsHellfire = gbIsHellfireSaveGame ? 1 : 0;
	}

	if (!UnPackPlayer(&pkplr, player, false)) {
		return;
	}

	LoadHeroItems(player);
	RemoveEmptyInventory(player);
	CalcPlrInv(player, false);
}

bool LevelFileExists()
{
	char szName[MAX_PATH];

	GetPermLevelNames(szName);

	uint32_t saveNum = gSaveNumber;
	if (!OpenArchive(saveNum))
		app_fatal("%s", _("Unable to read to save file archive").c_str());

	bool hasFile = SaveWriter.HasFile(szName);
	SaveWriter.Close();
	return hasFile;
}

void GetTempLevelNames(char *szTemp)
{
	if (setlevel)
		sprintf(szTemp, "temps%02d", setlvlnum);
	else
		sprintf(szTemp, "templ%02d", currlevel);
}

void GetPermLevelNames(char *szPerm)
{
	uint32_t saveNum = gSaveNumber;
	GetTempLevelNames(szPerm);
	if (!OpenArchive(saveNum))
		app_fatal("%s", _("Unable to read to save file archive").c_str());

	bool hasFile = SaveWriter.HasFile(szPerm);
	SaveWriter.Close();
	if (!hasFile) {
		if (setlevel)
			sprintf(szPerm, "perms%02d", setlvlnum);
		else
			sprintf(szPerm, "perml%02d", currlevel);
	}
}

void pfile_remove_temp_files()
{
	if (gbIsMultiplayer)
		return;

	uint32_t saveNum = gSaveNumber;
	if (!OpenArchive(saveNum))
		app_fatal("%s", _("Unable to write to save file archive").c_str());
	SaveWriter.RemoveHashEntries(GetTempSaveNames);
	SaveWriter.Close();
}

void pfile_update(bool forceSave)
{
	static Uint32 prevTick;

	if (!gbIsMultiplayer)
		return;

	Uint32 tick = SDL_GetTicks();
	if (!forceSave && tick - prevTick <= 60000)
		return;

	prevTick = tick;
	pfile_write_hero();
	sfile_write_stash();
}

} // namespace devilution
