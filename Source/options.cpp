/**
 * @file options.cpp
 *
 * Load and save options from the diablo.ini file.
 */

#include <cstdint>
#include <fstream>

#include <fmt/format.h>

#define SI_SUPPORT_IOSTREAMS
#include <SimpleIni.h>

#include "control.h"
#include "diablo.h"
#include "discord/discord.h"
#include "engine/demomode.h"
#include "hwcursor.hpp"
#include "options.h"
#include "platform/locale.hpp"
#include "qol/monhealthbar.h"
#include "qol/xpbar.h"
#include "sound_defs.hpp"
#include "utils/file_util.h"
#include "utils/language.h"
#include "utils/log.hpp"
#include "utils/paths.h"
#include "utils/stdcompat/algorithm.hpp"
#include "utils/utf8.hpp"

namespace devilution {

#ifndef DEFAULT_WIDTH
#define DEFAULT_WIDTH 640
#endif
#ifndef DEFAULT_HEIGHT
#define DEFAULT_HEIGHT 480
#endif
#ifndef DEFAULT_AUDIO_SAMPLE_RATE
#if defined(_WIN64) || defined(_WIN32)
// The sound API used by SDL in Windows (WASAPI) isn't great
// at upsampling from 22050 Hz on some drivers.
//
// Upsample ourselves on Windows by default.
// See https://github.com/diasurgical/devilutionX/issues/1390
#define DEFAULT_AUDIO_SAMPLE_RATE 48000
#else
#define DEFAULT_AUDIO_SAMPLE_RATE 22050
#endif
#endif
#ifndef DEFAULT_AUDIO_CHANNELS
#define DEFAULT_AUDIO_CHANNELS 2
#endif
#ifndef DEFAULT_AUDIO_BUFFER_SIZE
#define DEFAULT_AUDIO_BUFFER_SIZE 2048
#endif
#ifndef DEFAULT_AUDIO_RESAMPLING_QUALITY
#define DEFAULT_AUDIO_RESAMPLING_QUALITY 3
#endif

namespace {

#if defined(__ANDROID__) || defined(__APPLE__)
constexpr OptionEntryFlags OnlyIfNoImplicitRenderer = OptionEntryFlags::Invisible;
#else
constexpr OptionEntryFlags OnlyIfNoImplicitRenderer = OptionEntryFlags::None;
#endif

#if defined(__ANDROID__) || (defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE == 1)
constexpr OptionEntryFlags OnlyIfSupportsWindowed = OptionEntryFlags::Invisible;
#else
constexpr OptionEntryFlags OnlyIfSupportsWindowed = OptionEntryFlags::None;
#endif

std::string GetIniPath()
{
	auto path = paths::ConfigPath() + std::string("diablo.ini");
	return path;
}

CSimpleIni &GetIni()
{
	static CSimpleIni ini;
	static bool isIniLoaded = false;
	if (!isIniLoaded) {
		auto path = GetIniPath();
		auto stream = CreateFileStream(path.c_str(), std::fstream::in | std::fstream::binary);
		ini.SetSpaces(false);
		ini.SetMultiKey();
		if (stream)
			ini.LoadData(*stream);
		isIniLoaded = true;
	}
	return ini;
}

bool IniChanged = false;

/**
 * @brief Checks if a ini entry is changed by comparing value before and after
 */
class IniChangedChecker {
public:
	IniChangedChecker(const char *sectionName, const char *keyName)
	{
		this->sectionName_ = sectionName;
		this->keyName_ = keyName;
		oldValue_ = GetValue();
		if (!oldValue_) {
			// No entry found in original ini => new entry => changed
			IniChanged = true;
		}
	}
	~IniChangedChecker()
	{
		auto newValue = GetValue();
		if (oldValue_ != newValue)
			IniChanged = true;
	}

private:
	std::optional<std::string> GetValue()
	{
		std::list<CSimpleIni::Entry> values;
		if (!GetIni().GetAllValues(sectionName_, keyName_, values))
			return std::nullopt;
		std::string ret;
		for (auto &entry : values) {
			if (entry.pItem != nullptr)
				ret.append(entry.pItem);
			ret.append("\n");
		}
		return ret;
	}

	std::optional<std::string> oldValue_;
	const char *sectionName_;
	const char *keyName_;
};

int GetIniInt(const char *keyname, const char *valuename, int defaultValue)
{
	return GetIni().GetLongValue(keyname, valuename, defaultValue);
}

bool GetIniBool(const char *sectionName, const char *keyName, bool defaultValue)
{
	return GetIni().GetBoolValue(sectionName, keyName, defaultValue);
}

float GetIniFloat(const char *sectionName, const char *keyName, float defaultValue)
{
	return (float)GetIni().GetDoubleValue(sectionName, keyName, defaultValue);
}

bool GetIniValue(const char *sectionName, const char *keyName, char *string, int stringSize, const char *defaultString = "")
{
	const char *value = GetIni().GetValue(sectionName, keyName);
	if (value == nullptr) {
		CopyUtf8(string, defaultString, stringSize);
		return false;
	}
	CopyUtf8(string, value, stringSize);
	return true;
}

bool GetIniStringVector(const char *sectionName, const char *keyName, std::vector<std::string> &stringValues)
{
	std::list<CSimpleIni::Entry> values;
	if (!GetIni().GetAllValues(sectionName, keyName, values)) {
		return false;
	}
	for (auto &entry : values) {
		stringValues.emplace_back(entry.pItem);
	}
	return true;
}

void SetIniValue(const char *keyname, const char *valuename, int value)
{
	IniChangedChecker changedChecker(keyname, valuename);
	GetIni().SetLongValue(keyname, valuename, value, nullptr, false, true);
}

void SetIniValue(const char *keyname, const char *valuename, bool value)
{
	IniChangedChecker changedChecker(keyname, valuename);
	GetIni().SetLongValue(keyname, valuename, value ? 1 : 0, nullptr, false, true);
}

void SetIniValue(const char *keyname, const char *valuename, float value)
{
	IniChangedChecker changedChecker(keyname, valuename);
	GetIni().SetDoubleValue(keyname, valuename, value, nullptr, true);
}

void SetIniValue(const char *sectionName, const char *keyName, const char *value)
{
	IniChangedChecker changedChecker(sectionName, keyName);
	auto &ini = GetIni();
	ini.SetValue(sectionName, keyName, value, nullptr, true);
}

void SetIniValue(const char *keyname, const char *valuename, const std::vector<std::string> &stringValues)
{
	IniChangedChecker changedChecker(keyname, valuename);
	bool firstSet = true;
	for (auto &value : stringValues) {
		GetIni().SetValue(keyname, valuename, value.c_str(), nullptr, firstSet);
		firstSet = false;
	}
	if (firstSet)
		GetIni().SetValue(keyname, valuename, "", nullptr, true);
}

void SaveIni()
{
	if (!IniChanged)
		return;
	auto iniPath = GetIniPath();
	auto stream = CreateFileStream(iniPath.c_str(), std::fstream::out | std::fstream::trunc | std::fstream::binary);
	GetIni().Save(*stream, true);
	IniChanged = false;
}

#if SDL_VERSION_ATLEAST(2, 0, 0)
bool HardwareCursorDefault()
{
#if defined(__ANDROID__) || (defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE == 1)
	// See https://github.com/diasurgical/devilutionX/issues/2502
	return false;
#else
	return HardwareCursorSupported();
#endif
}
#endif

void OptionGrabInputChanged()
{
#ifdef USE_SDL1
	SDL_WM_GrabInput(*sgOptions.Gameplay.grabInput ? SDL_GRAB_ON : SDL_GRAB_OFF);
#else
	if (ghMainWnd != nullptr)
		SDL_SetWindowGrab(ghMainWnd, *sgOptions.Gameplay.grabInput ? SDL_TRUE : SDL_FALSE);
#endif
}

void OptionExperienceBarChanged()
{
	if (!gbRunGame)
		return;
	if (*sgOptions.Gameplay.experienceBar)
		InitXPBar();
	else
		FreeXPBar();
}

void OptionEnemyHealthBarChanged()
{
	if (!gbRunGame)
		return;
	if (*sgOptions.Gameplay.enemyHealthBar)
		InitMonsterHealthBar();
	else
		FreeMonsterHealthBar();
}

void OptionShowFPSChanged()
{
	if (*sgOptions.Graphics.showFPS)
		EnableFrameCount();
	else
		frameflag = false;
}

void OptionLanguageCodeChanged()
{
	LanguageInitialize();
	LoadLanguageArchive();
}

void OptionGameModeChanged()
{
	gbIsHellfire = *sgOptions.StartUp.gameMode == StartUpGameMode::Hellfire;
	discord_manager::UpdateMenu(true);
}

void OptionSharewareChanged()
{
	gbIsSpawn = *sgOptions.StartUp.shareware;
}

void OptionAudioChanged()
{
	effects_cleanup_sfx();
	music_stop();
	snd_deinit();
	snd_init();
	music_start(TMUSIC_INTRO);
	if (gbRunGame)
		sound_init();
	else
		ui_sound_init();
}

} // namespace

/** Game options */
Options sgOptions;

#if SDL_VERSION_ATLEAST(2, 0, 0)
bool HardwareCursorSupported()
{
#if (defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE == 1)
	return false;
#else
	SDL_version v;
	SDL_GetVersion(&v);
	return SDL_VERSIONNUM(v.major, v.minor, v.patch) >= SDL_VERSIONNUM(2, 0, 12);
#endif
}
#endif

void LoadOptions()
{
	for (OptionCategoryBase *pCategory : sgOptions.GetCategories()) {
		for (OptionEntryBase *pEntry : pCategory->GetEntries()) {
			pEntry->LoadFromIni(pCategory->GetKey());
		}
	}

	GetIniValue("Hellfire", "SItem", sgOptions.Hellfire.szItem, sizeof(sgOptions.Hellfire.szItem), "");

	GetIniValue("Network", "Bind Address", sgOptions.Network.szBindAddress, sizeof(sgOptions.Network.szBindAddress), "0.0.0.0");
	GetIniValue("Network", "Previous Game ID", sgOptions.Network.szPreviousZTGame, sizeof(sgOptions.Network.szPreviousZTGame), "");
	GetIniValue("Network", "Previous Host", sgOptions.Network.szPreviousHost, sizeof(sgOptions.Network.szPreviousHost), "");

	for (size_t i = 0; i < QUICK_MESSAGE_OPTIONS; i++)
		GetIniStringVector("NetMsg", QuickMessages[i].key, sgOptions.Chat.szHotKeyMsgs[i]);

	GetIniValue("Controller", "Mapping", sgOptions.Controller.szMapping, sizeof(sgOptions.Controller.szMapping), "");
	sgOptions.Controller.bSwapShoulderButtonMode = GetIniBool("Controller", "Swap Shoulder Button Mode", false);
	sgOptions.Controller.bDpadHotkeys = GetIniBool("Controller", "Dpad Hotkeys", false);
	sgOptions.Controller.fDeadzone = GetIniFloat("Controller", "deadzone", 0.07F);
#ifdef __vita__
	sgOptions.Controller.bRearTouch = GetIniBool("Controller", "Enable Rear Touchpad", true);
#endif

	if (demo::IsRunning())
		demo::OverrideOptions();
}

void SaveOptions()
{
	if (demo::IsRunning())
		return;

	for (OptionCategoryBase *pCategory : sgOptions.GetCategories()) {
		for (OptionEntryBase *pEntry : pCategory->GetEntries()) {
			pEntry->SaveToIni(pCategory->GetKey());
		}
	}

	SetIniValue("Hellfire", "SItem", sgOptions.Hellfire.szItem);

	SetIniValue("Network", "Bind Address", sgOptions.Network.szBindAddress);
	SetIniValue("Network", "Previous Game ID", sgOptions.Network.szPreviousZTGame);
	SetIniValue("Network", "Previous Host", sgOptions.Network.szPreviousHost);

	for (size_t i = 0; i < QUICK_MESSAGE_OPTIONS; i++)
		SetIniValue("NetMsg", QuickMessages[i].key, sgOptions.Chat.szHotKeyMsgs[i]);

	SetIniValue("Controller", "Mapping", sgOptions.Controller.szMapping);
	SetIniValue("Controller", "Swap Shoulder Button Mode", sgOptions.Controller.bSwapShoulderButtonMode);
	SetIniValue("Controller", "Dpad Hotkeys", sgOptions.Controller.bDpadHotkeys);
	SetIniValue("Controller", "deadzone", sgOptions.Controller.fDeadzone);
#ifdef __vita__
	SetIniValue("Controller", "Enable Rear Touchpad", sgOptions.Controller.bRearTouch);
#endif

	SaveIni();
}

string_view OptionEntryBase::GetName() const
{
	return _(name.data());
}
string_view OptionEntryBase::GetDescription() const
{
	return _(description.data());
}
OptionEntryFlags OptionEntryBase::GetFlags() const
{
	return flags;
}
void OptionEntryBase::SetValueChangedCallback(std::function<void()> callback)
{
	this->callback = std::move(callback);
}
void OptionEntryBase::NotifyValueChanged()
{
	if (callback)
		callback();
}

void OptionEntryBoolean::LoadFromIni(string_view category)
{
	value = GetIniBool(category.data(), key.data(), defaultValue);
}
void OptionEntryBoolean::SaveToIni(string_view category) const
{
	SetIniValue(category.data(), key.data(), value);
}
void OptionEntryBoolean::SetValue(bool value)
{
	this->value = value;
	this->NotifyValueChanged();
}
OptionEntryType OptionEntryBoolean::GetType() const
{
	return OptionEntryType::Boolean;
}
string_view OptionEntryBoolean::GetValueDescription() const
{
	return value ? _("ON") : _("OFF");
}

OptionEntryType OptionEntryListBase::GetType() const
{
	return OptionEntryType::List;
}
string_view OptionEntryListBase::GetValueDescription() const
{
	return GetListDescription(GetActiveListIndex());
}

void OptionEntryEnumBase::LoadFromIni(string_view category)
{
	value = GetIniInt(category.data(), key.data(), defaultValue);
}
void OptionEntryEnumBase::SaveToIni(string_view category) const
{
	SetIniValue(category.data(), key.data(), value);
}
void OptionEntryEnumBase::SetValueInternal(int value)
{
	this->value = value;
	this->NotifyValueChanged();
}
void OptionEntryEnumBase::AddEntry(int value, string_view name)
{
	entryValues.push_back(value);
	entryNames.push_back(name);
}
size_t OptionEntryEnumBase::GetListSize() const
{
	return entryValues.size();
}
string_view OptionEntryEnumBase::GetListDescription(size_t index) const
{
	return _(entryNames[index].data());
}
size_t OptionEntryEnumBase::GetActiveListIndex() const
{
	auto iterator = std::find(entryValues.begin(), entryValues.end(), value);
	if (iterator == entryValues.end())
		return 0;
	return std::distance(entryValues.begin(), iterator);
}
void OptionEntryEnumBase::SetActiveListIndex(size_t index)
{
	this->value = entryValues[index];
	this->NotifyValueChanged();
}

void OptionEntryIntBase::LoadFromIni(string_view category)
{
	value = GetIniInt(category.data(), key.data(), defaultValue);
	if (std::find(entryValues.begin(), entryValues.end(), value) == entryValues.end()) {
		entryValues.push_back(value);
		std::sort(entryValues.begin(), entryValues.end());
		entryNames.clear();
	}
}
void OptionEntryIntBase::SaveToIni(string_view category) const
{
	SetIniValue(category.data(), key.data(), value);
}
void OptionEntryIntBase::SetValueInternal(int value)
{
	this->value = value;
	this->NotifyValueChanged();
}
void OptionEntryIntBase::AddEntry(int value)
{
	entryValues.push_back(value);
}
size_t OptionEntryIntBase::GetListSize() const
{
	return entryValues.size();
}
string_view OptionEntryIntBase::GetListDescription(size_t index) const
{
	if (entryNames.empty()) {
		for (auto value : entryValues) {
			entryNames.push_back(fmt::format("{}", value));
		}
	}
	return entryNames[index].data();
}
size_t OptionEntryIntBase::GetActiveListIndex() const
{
	auto iterator = std::find(entryValues.begin(), entryValues.end(), value);
	if (iterator == entryValues.end())
		return 0;
	return std::distance(entryValues.begin(), iterator);
}
void OptionEntryIntBase::SetActiveListIndex(size_t index)
{
	this->value = entryValues[index];
	this->NotifyValueChanged();
}

OptionCategoryBase::OptionCategoryBase(string_view key, string_view name, string_view description)
    : key(key)
    , name(name)
    , description(description)
{
}
string_view OptionCategoryBase::GetKey() const
{
	return key;
}
string_view OptionCategoryBase::GetName() const
{
	return _(name.data());
}
string_view OptionCategoryBase::GetDescription() const
{
	return _(description.data());
}

StartUpOptions::StartUpOptions()
    : OptionCategoryBase("StartUp", N_("Start Up"), N_("Start Up Settings"))
    , gameMode("Game", OptionEntryFlags::NeedHellfireMpq | OptionEntryFlags::RecreateUI, N_("Game Mode"), N_("Play Diablo or Hellfire."), StartUpGameMode::Ask,
          {
              { StartUpGameMode::Diablo, N_("Diablo") },
              // Ask is missing, cause we want to hide it from UI-Settings.
              //{ StartUpGameMode::Hellfire, N_("Hellfire") },
          })
    , shareware("Shareware", OptionEntryFlags::NeedDiabloMpq | OptionEntryFlags::RecreateUI, N_("Restrict to Shareware"), N_("Makes the game compatible with the demo. Enables multiplayer with friends who don't own a full copy of Diablo."), false)
    , diabloIntro("Diablo Intro", OptionEntryFlags::OnlyDiablo, N_("Intro"), N_("Shown Intro cinematic."), StartUpIntro::Once,
          {
              { StartUpIntro::Off, N_("OFF") },
              // Once is missing, cause we want to hide it from UI-Settings.
              { StartUpIntro::On, N_("ON") },
          })
    , hellfireIntro("Hellfire Intro", OptionEntryFlags::OnlyHellfire, N_("Intro"), N_("Shown Intro cinematic."), StartUpIntro::Once,
          {
              { StartUpIntro::Off, N_("OFF") },
              // Once is missing, cause we want to hide it from UI-Settings.
              { StartUpIntro::On, N_("ON") },
          })
    , splash("Splash", OptionEntryFlags::None, N_("Splash"), N_("Shown splash screen."), StartUpSplash::LogoAndTitleDialog,
          {
              { StartUpSplash::LogoAndTitleDialog, N_("Logo and Title Screen") },
              { StartUpSplash::TitleDialog, N_("Title Screen") },
              { StartUpSplash::None, N_("None") },
          })
{
	gameMode.SetValueChangedCallback(OptionGameModeChanged);
	shareware.SetValueChangedCallback(OptionSharewareChanged);
}
std::vector<OptionEntryBase *> StartUpOptions::GetEntries()
{
	return {
		&gameMode,
		&shareware,
		&diabloIntro,
		&hellfireIntro,
		&splash,
	};
}

DiabloOptions::DiabloOptions()
    : OptionCategoryBase("Diablo", N_("Diablo"), N_("Diablo specific Settings"))
    , lastSinglePlayerHero("LastSinglePlayerHero", OptionEntryFlags::Invisible | OptionEntryFlags::OnlyDiablo, "Sample Rate", "Remembers what singleplayer hero/save was last used.", 0)
    , lastMultiplayerHero("LastMultiplayerHero", OptionEntryFlags::Invisible | OptionEntryFlags::OnlyDiablo, "Sample Rate", "Remembers what multiplayer hero/save was last used.", 0)
{
}
std::vector<OptionEntryBase *> DiabloOptions::GetEntries()
{
	return {
		&lastSinglePlayerHero,
		&lastMultiplayerHero,
	};
}

HellfireOptions::HellfireOptions()
    : OptionCategoryBase("Hellfire", N_("Hellfire"), N_("Hellfire specific Settings"))
    , lastSinglePlayerHero("LastSinglePlayerHero", OptionEntryFlags::Invisible | OptionEntryFlags::OnlyHellfire, "Sample Rate", "Remembers what singleplayer hero/save was last used.", 0)
    , lastMultiplayerHero("LastMultiplayerHero", OptionEntryFlags::Invisible | OptionEntryFlags::OnlyHellfire, "Sample Rate", "Remembers what multiplayer hero/save was last used.", 0)
{
}
std::vector<OptionEntryBase *> HellfireOptions::GetEntries()
{
	return {
		&lastSinglePlayerHero,
		&lastMultiplayerHero,
	};
}

AudioOptions::AudioOptions()
    : OptionCategoryBase("Audio", N_("Audio"), N_("Audio Settings"))
    , soundVolume("Sound Volume", OptionEntryFlags::Invisible, "Sound Volume", "Movie and SFX volume.", VOLUME_MAX)
    , musicVolume("Music Volume", OptionEntryFlags::Invisible, "Music Volume", "Music Volume.", VOLUME_MAX)
    , walkingSound("Walking Sound", OptionEntryFlags::None, N_("Walking Sound"), N_("Player emits sound when walking."), true)
    , autoEquipSound("Auto Equip Sound", OptionEntryFlags::None, N_("Auto Equip Sound"), N_("Automatically equipping items on pickup emits the equipment sound."), false)
    , itemPickupSound("Item Pickup Sound", OptionEntryFlags::None, N_("Item Pickup Sound"), N_("Picking up items emits the items pickup sound."), false)
    , sampleRate("Sample Rate", OptionEntryFlags::CantChangeInGame, N_("Sample Rate"), N_("Output sample rate (Hz)."), DEFAULT_AUDIO_SAMPLE_RATE, { 22050, 44100, 48000 })
    , channels("Channels", OptionEntryFlags::CantChangeInGame, N_("Channels"), N_("Number of output channels."), DEFAULT_AUDIO_CHANNELS, { 1, 2 })
    , bufferSize("Buffer Size", OptionEntryFlags::CantChangeInGame, N_("Buffer Size"), N_("Buffer size (number of frames per channel)."), DEFAULT_AUDIO_BUFFER_SIZE, { 1024, 2048, 5120 })
    , resamplingQuality("Resampling Quality",
          OptionEntryFlags::CantChangeInGame |
#ifdef DVL_AULIB_SUPPORTS_SDL_RESAMPLER
              OptionEntryFlags::Invisible,
#else
              OptionEntryFlags::None,
#endif
          N_("Resampling Quality"), N_("Quality of the resampler, from 0 (lowest) to 10 (highest)."), DEFAULT_AUDIO_RESAMPLING_QUALITY, { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 })
{
	sampleRate.SetValueChangedCallback(OptionAudioChanged);
	channels.SetValueChangedCallback(OptionAudioChanged);
	bufferSize.SetValueChangedCallback(OptionAudioChanged);
	resamplingQuality.SetValueChangedCallback(OptionAudioChanged);
}
std::vector<OptionEntryBase *> AudioOptions::GetEntries()
{
	return {
		&soundVolume,
		&musicVolume,
		&walkingSound,
		&autoEquipSound,
		&itemPickupSound,
		&sampleRate,
		&channels,
		&bufferSize,
		&resamplingQuality,
	};
}

OptionEntryResolution::OptionEntryResolution()
    : OptionEntryListBase("", OptionEntryFlags::CantChangeInGame | OptionEntryFlags::RecreateUI, N_("Resolution"), N_("Affect the game's internal resolution and determine your view area. Note: This can differ from screen resolution, when Upscaling, Integer Scaling or Fit to Screen is used."))
{
}
void OptionEntryResolution::LoadFromIni(string_view category)
{
	size = { GetIniInt(category.data(), "Width", DEFAULT_WIDTH), GetIniInt(category.data(), "Height", DEFAULT_HEIGHT) };
}
void OptionEntryResolution::SaveToIni(string_view category) const
{
	SetIniValue(category.data(), "Width", size.width);
	SetIniValue(category.data(), "Height", size.height);
}

void OptionEntryResolution::CheckResolutionsAreInitialized() const
{
	if (!resolutions.empty())
		return;

	std::vector<Size> sizes;
	float scaleFactor = GetDpiScalingFactor();

	// Add resolutions
#ifdef USE_SDL1
	auto *modes = SDL_ListModes(nullptr, SDL_FULLSCREEN | SDL_HWPALETTE);
	// SDL_ListModes returns -1 if any resolution is allowed (for example returned on 3DS)
	if (modes != nullptr && modes != (SDL_Rect **)-1) {
		for (size_t i = 0; modes[i] != nullptr; i++) {
			if (modes[i]->w < modes[i]->h) {
				std::swap(modes[i]->w, modes[i]->h);
			}
			sizes.emplace_back(Size {
			    static_cast<int>(modes[i]->w * scaleFactor),
			    static_cast<int>(modes[i]->h * scaleFactor) });
		}
	}
#else
	int displayModeCount = SDL_GetNumDisplayModes(0);
	for (int i = 0; i < displayModeCount; i++) {
		SDL_DisplayMode mode;
		if (SDL_GetDisplayMode(0, i, &mode) != 0) {
			ErrSdl();
		}
		if (mode.w < mode.h) {
			std::swap(mode.w, mode.h);
		}
		sizes.emplace_back(Size {
		    static_cast<int>(mode.w * scaleFactor),
		    static_cast<int>(mode.h * scaleFactor) });
	}
#endif

	// Ensures that the ini specified resolution is present in resolution list even if it doesn't match a monitor resolution (for example if played in window mode)
	sizes.push_back(this->size);
	// Ensures that the vanilla/default resolution is always present
	sizes.emplace_back(Size { DEFAULT_WIDTH, DEFAULT_HEIGHT });

	// Sort by width then by height
	std::sort(sizes.begin(), sizes.end(),
	    [](const Size &x, const Size &y) -> bool {
		    if (x.width == y.width)
			    return x.height > y.height;
		    return x.width > y.width;
	    });
	// Remove duplicate entries
	sizes.erase(std::unique(sizes.begin(), sizes.end()), sizes.end());

	for (auto &size : sizes) {
		resolutions.emplace_back(size, fmt::format("{}x{}", size.width, size.height));
	}
}

size_t OptionEntryResolution::GetListSize() const
{
	CheckResolutionsAreInitialized();
	return resolutions.size();
}
string_view OptionEntryResolution::GetListDescription(size_t index) const
{
	CheckResolutionsAreInitialized();
	return resolutions[index].second;
}
size_t OptionEntryResolution::GetActiveListIndex() const
{
	CheckResolutionsAreInitialized();
	auto found = std::find_if(resolutions.begin(), resolutions.end(), [this](const auto &x) { return x.first == this->size; });
	if (found == resolutions.end())
		return 0;
	return std::distance(resolutions.begin(), found);
}
void OptionEntryResolution::SetActiveListIndex(size_t index)
{
	size = resolutions[index].first;
	NotifyValueChanged();
}

GraphicsOptions::GraphicsOptions()
    : OptionCategoryBase("Graphics", N_("Graphics"), N_("Graphics Settings"))
    , fullscreen("Fullscreen", OnlyIfSupportsWindowed | OptionEntryFlags::CantChangeInGame | OptionEntryFlags::RecreateUI, N_("Fullscreen"), N_("Display the game in windowed or fullscreen mode."), true)
#if !defined(USE_SDL1) || defined(__3DS__)
    , fitToScreen("Fit to Screen", OptionEntryFlags::CantChangeInGame | OptionEntryFlags::RecreateUI, N_("Fit to Screen"), N_("Automatically adjust the game window to your current desktop screen aspect ratio and resolution."), true)
#endif
#ifndef USE_SDL1
    , upscale("Upscale", OnlyIfNoImplicitRenderer | OptionEntryFlags::CantChangeInGame | OptionEntryFlags::RecreateUI, N_("Upscale"), N_("Enables image scaling from the game resolution to your monitor resolution. Prevents changing the monitor resolution and allows window resizing."), true)
    , scaleQuality("Scaling Quality", OptionEntryFlags::None, N_("Scaling Quality"), N_("Enables optional filters to the output image when upscaling."), ScalingQuality::AnisotropicFiltering,
          {
              { ScalingQuality::NearestPixel, N_("Nearest Pixel") },
              { ScalingQuality::BilinearFiltering, N_("Bilinear") },
              { ScalingQuality::AnisotropicFiltering, N_("Anisotropic") },
          })
    , integerScaling("Integer Scaling", OptionEntryFlags::CantChangeInGame | OptionEntryFlags::RecreateUI, N_("Integer Scaling"), N_("Scales the image using whole number pixel ratio."), false)
    , vSync("Vertical Sync", OptionEntryFlags::RecreateUI, N_("Vertical Sync"), N_("Forces waiting for Vertical Sync. Prevents tearing effect when drawing a frame. Disabling it can help with mouse lag on some systems."), true)
#endif
    , gammaCorrection("Gamma Correction", OptionEntryFlags::Invisible, "Gamma Correction", "Gamma correction level.", 100)
    , colorCycling("Color Cycling", OptionEntryFlags::None, N_("Color Cycling"), N_("Color cycling effect used for water, lava, and acid animation."), true)
    , alternateNestArt("Alternate nest art", OptionEntryFlags::OnlyHellfire | OptionEntryFlags::CantChangeInGame, N_("Alternate nest art"), N_("The game will use an alternative palette for Hellfire’s nest tileset."), false)
#if SDL_VERSION_ATLEAST(2, 0, 0)
    , hardwareCursor("Hardware Cursor", OptionEntryFlags::CantChangeInGame | OptionEntryFlags::RecreateUI | (HardwareCursorSupported() ? OptionEntryFlags::None : OptionEntryFlags::Invisible), N_("Hardware Cursor"), N_("Use a hardware cursor"), HardwareCursorDefault())
    , hardwareCursorForItems("Hardware Cursor For Items", OptionEntryFlags::CantChangeInGame | (HardwareCursorSupported() ? OptionEntryFlags::None : OptionEntryFlags::Invisible), N_("Hardware Cursor For Items"), N_("Use a hardware cursor for items."), false)
    , hardwareCursorMaxSize("Hardware Cursor Maximum Size", OptionEntryFlags::CantChangeInGame | OptionEntryFlags::RecreateUI | (HardwareCursorSupported() ? OptionEntryFlags::None : OptionEntryFlags::Invisible), N_("Hardware Cursor Maximum Size"), N_("Maximum width / height for the hardware cursor. Larger cursors fall back to software."), 128, { 0, 64, 128, 256, 512 })
#endif
    , limitFPS("FPS Limiter", OptionEntryFlags::None, N_("FPS Limiter"), N_("FPS is limited to avoid high CPU load. Limit considers refresh rate."), true)
    , showFPS("Show FPS", OptionEntryFlags::None, N_("Show FPS"), N_("Displays the FPS in the upper left corner of the screen."), false)
    , showHealthValues("Show health values", OptionEntryFlags::None, N_("Show health values"), N_("Displays current / max health value on health globe."), false)
    , showManaValues("Show mana values", OptionEntryFlags::None, N_("Show mana values"), N_("Displays current / max mana value on mana globe."), false)
{
	resolution.SetValueChangedCallback(ResizeWindow);
	fullscreen.SetValueChangedCallback(SetFullscreenMode);
#if !defined(USE_SDL1) || defined(__3DS__)
	fitToScreen.SetValueChangedCallback(ResizeWindow);
#endif
#ifndef USE_SDL1
	upscale.SetValueChangedCallback(ResizeWindow);
	scaleQuality.SetValueChangedCallback(ReinitializeTexture);
	integerScaling.SetValueChangedCallback(ReinitializeIntegerScale);
	vSync.SetValueChangedCallback(ReinitializeRenderer);
#endif
	showFPS.SetValueChangedCallback(OptionShowFPSChanged);
}
std::vector<OptionEntryBase *> GraphicsOptions::GetEntries()
{
	// clang-format off
	return {
		&resolution,
#ifndef __vita__
		&fullscreen,
#endif
#if !defined(USE_SDL1) || defined(__3DS__)
		&fitToScreen,
#endif
#ifndef USE_SDL1
		&upscale,
		&scaleQuality,
		&integerScaling,
		&vSync,
#endif
		&gammaCorrection,
		&limitFPS,
		&showFPS,
		&showHealthValues,
		&showManaValues,
		&colorCycling,
		&alternateNestArt,
#if SDL_VERSION_ATLEAST(2, 0, 0)
		&hardwareCursor,
		&hardwareCursorForItems,
		&hardwareCursorMaxSize,
#endif
	};
	// clang-format on
}

GameplayOptions::GameplayOptions()
    : OptionCategoryBase("Game", N_("Gameplay"), N_("Gameplay Settings"))
    , tickRate("Speed", OptionEntryFlags::Invisible, "Speed", "Gameplay ticks per second.", 20)
    , runInTown("Run in Town", OptionEntryFlags::CantChangeInMultiPlayer, N_("Run in Town"), N_("Enable jogging/fast walking in town for Diablo and Hellfire. This option was introduced in the expansion."), false)
    , grabInput("Grab Input", OptionEntryFlags::None, N_("Grab Input"), N_("When enabled mouse is locked to the game window."), false)
    , theoQuest("Theo Quest", OptionEntryFlags::CantChangeInGame | OptionEntryFlags::OnlyHellfire, N_("Theo Quest"), N_("Enable Little Girl quest."), false)
    , cowQuest("Cow Quest", OptionEntryFlags::CantChangeInGame | OptionEntryFlags::OnlyHellfire, N_("Cow Quest"), N_("Enable Jersey's quest. Lester the farmer is replaced by the Complete Nut."), false)
    , friendlyFire("Friendly Fire", OptionEntryFlags::CantChangeInMultiPlayer, N_("Friendly Fire"), N_("Allow arrow/spell damage between players in multiplayer even when the friendly mode is on."), true)
    , testBard("Test Bard", OptionEntryFlags::Invisible, N_("Test Bard"), N_("Force the Bard character type to appear in the hero selection menu."), false)
    , testBarbarian("Test Barbarian", OptionEntryFlags::Invisible, N_("Test Barbarian"), N_("Force the Barbarian character type to appear in the hero selection menu."), false)
    , experienceBar("Experience Bar", OptionEntryFlags::None, N_("Experience Bar"), N_("Experience Bar is added to the UI at the bottom of the screen."), false)
    , enemyHealthBar("Enemy Health Bar", OptionEntryFlags::None, N_("Enemy Health Bar"), N_("Enemy Health Bar is displayed at the top of the screen."), false)
    , autoGoldPickup("Auto Gold Pickup", OptionEntryFlags::None, N_("Auto Gold Pickup"), N_("Gold is automatically collected when in close proximity to the player."), false)
    , autoElixirPickup("Auto Elixir Pickup", OptionEntryFlags::None, N_("Auto Elixir Pickup"), N_("Elixirs are automatically collected when in close proximity to the player."), false)
    , autoPickupInTown("Auto Pickup in Town", OptionEntryFlags::None, N_("Auto Pickup in Town"), N_("Automatically pickup items in town."), false)
    , adriaRefillsMana("Adria Refills Mana", OptionEntryFlags::None, N_("Adria Refills Mana"), N_("Adria will refill your mana when you visit her shop."), false)
    , autoEquipWeapons("Auto Equip Weapons", OptionEntryFlags::None, N_("Auto Equip Weapons"), N_("Weapons will be automatically equipped on pickup or purchase if enabled."), true)
    , autoEquipArmor("Auto Equip Armor", OptionEntryFlags::None, N_("Auto Equip Armor"), N_("Armor will be automatically equipped on pickup or purchase if enabled."), false)
    , autoEquipHelms("Auto Equip Helms", OptionEntryFlags::None, N_("Auto Equip Helms"), N_("Helms will be automatically equipped on pickup or purchase if enabled."), false)
    , autoEquipShields("Auto Equip Shields", OptionEntryFlags::None, N_("Auto Equip Shields"), N_("Shields will be automatically equipped on pickup or purchase if enabled."), false)
    , autoEquipJewelry("Auto Equip Jewelry", OptionEntryFlags::None, N_("Auto Equip Jewelry"), N_("Jewelry will be automatically equipped on pickup or purchase if enabled."), false)
    , randomizeQuests("Randomize Quests", OptionEntryFlags::CantChangeInGame, N_("Randomize Quests"), N_("Randomly selecting available quests for new games."), true)
    , showMonsterType("Show Monster Type", OptionEntryFlags::None, N_("Show Monster Type"), N_("Hovering over a monster will display the type of monster in the description box in the UI."), false)
    , showItemLabels("Show Item Labels", OptionEntryFlags::None, N_("Show Item Labels"), N_("Enables item labels for items on the ground."), false)
    , autoRefillBelt("Auto Refill Belt", OptionEntryFlags::None, N_("Auto Refill Belt"), N_("Refill belt from inventory when belt item is consumed."), false)
    , disableCripplingShrines("Disable Crippling Shrines", OptionEntryFlags::None, N_("Disable Crippling Shrines"), N_("When enabled Cauldrons, Fascinating Shrines, Goat Shrines, Ornate Shrines and Sacred Shrines are not able to be clicked on and labeled as disabled."), false)
    , quickCast("Quick Cast", OptionEntryFlags::None, N_("Quick Cast"), N_("Spell hotkeys instantly cast the spell, rather than switching the readied spell."), false)
    , numHealPotionPickup("Heal Potion Pickup", OptionEntryFlags::None, N_("Heal Potion Pickup"), N_("Number of Healing potions to pick up automatically."), 0, { 0, 1, 2, 4, 8, 16 })
    , numFullHealPotionPickup("Full Heal Potion Pickup", OptionEntryFlags::None, N_("Full Heal Potion Pickup"), N_("Number of Full Healing potions to pick up automatically."), 0, { 0, 1, 2, 4, 8, 16 })
    , numManaPotionPickup("Mana Potion Pickup", OptionEntryFlags::None, N_("Mana Potion Pickup"), N_("Number of Mana potions to pick up automatically."), 0, { 0, 1, 2, 4, 8, 16 })
    , numFullManaPotionPickup("Full Mana Potion Pickup", OptionEntryFlags::None, N_("Full Mana Potion Pickup"), N_("Number of Full Mana potions to pick up automatically."), 0, { 0, 1, 2, 4, 8, 16 })
    , numRejuPotionPickup("Rejuvenation Potion Pickup", OptionEntryFlags::None, N_("Rejuvenation Potion Pickup"), N_("Number of Rejuvenation potions to pick up automatically."), 0, { 0, 1, 2, 4, 8, 16 })
    , numFullRejuPotionPickup("Full Rejuvenation Potion Pickup", OptionEntryFlags::None, N_("Full Rejuvenation Potion Pickup"), N_("Number of Full Rejuvenation potions to pick up automatically."), 0, { 0, 1, 2, 4, 8, 16 })
{
	grabInput.SetValueChangedCallback(OptionGrabInputChanged);
	experienceBar.SetValueChangedCallback(OptionExperienceBarChanged);
	enemyHealthBar.SetValueChangedCallback(OptionEnemyHealthBarChanged);
}
std::vector<OptionEntryBase *> GameplayOptions::GetEntries()
{
	return {
		&tickRate,
		&grabInput,
		&runInTown,
		&adriaRefillsMana,
		&randomizeQuests,
		&theoQuest,
		&cowQuest,
		&friendlyFire,
		&testBard,
		&testBarbarian,
		&experienceBar,
		&enemyHealthBar,
		&showMonsterType,
		&showItemLabels,
		&disableCripplingShrines,
		&quickCast,
		&autoRefillBelt,
		&autoPickupInTown,
		&autoGoldPickup,
		&autoElixirPickup,
		&autoEquipWeapons,
		&autoEquipArmor,
		&autoEquipHelms,
		&autoEquipShields,
		&autoEquipJewelry,
		&numHealPotionPickup,
		&numFullHealPotionPickup,
		&numManaPotionPickup,
		&numFullManaPotionPickup,
		&numRejuPotionPickup,
		&numFullRejuPotionPickup,
	};
}

ControllerOptions::ControllerOptions()
    : OptionCategoryBase("Controller", N_("Controller"), N_("Controller Settings"))
{
}
std::vector<OptionEntryBase *> ControllerOptions::GetEntries()
{
	return {};
}

NetworkOptions::NetworkOptions()
    : OptionCategoryBase("Network", N_("Network"), N_("Network Settings"))
    , port("Port", OptionEntryFlags::Invisible, "Port", "What network port to use.", 6112)
{
}
std::vector<OptionEntryBase *> NetworkOptions::GetEntries()
{
	return {
		&port,
	};
}

ChatOptions::ChatOptions()
    : OptionCategoryBase("NetMsg", N_("Chat"), N_("Chat Settings"))
{
}
std::vector<OptionEntryBase *> ChatOptions::GetEntries()
{
	return {};
}

OptionEntryLanguageCode::OptionEntryLanguageCode()
    : OptionEntryListBase("Code", OptionEntryFlags::CantChangeInGame | OptionEntryFlags::RecreateUI, N_("Language"), N_("Define what language to use in game."))
{
}
void OptionEntryLanguageCode::LoadFromIni(string_view category)
{
	if (GetIniValue(category.data(), key.data(), szCode, sizeof(szCode))) {
		if (HasTranslation(szCode)) {
			// User preferred language is available
			return;
		}
	}

	// Might be a first run or the user has attempted to load a translation that doesn't exist via manual ini edit. Try
	//  find a best fit from the platform locale information.
	std::vector<std::string> locales = GetLocales();

	// So that the correct language is shown in the settings menu for users with US english set as a preferred language
	//  we need to replace the "en_US" locale code with the neutral string "en" as expected by the available options
	std::replace(locales.begin(), locales.end(), std::string { "en_US" }, std::string { "en" });

	// Insert non-regional locale codes after the last regional variation so we fallback to neutral translations if no
	//  regional translation exists that meets user preferences.
	for (auto localeIter = locales.rbegin(); localeIter != locales.rend(); localeIter++) {
		auto regionSeparator = localeIter->find('_');
		if (regionSeparator != std::string::npos) {
			std::string neutralLocale = localeIter->substr(0, regionSeparator);
			if (std::find(locales.rbegin(), localeIter, neutralLocale) == localeIter) {
				localeIter = std::make_reverse_iterator(locales.insert(localeIter.base(), neutralLocale));
			}
		}
	}

	LogVerbose("Found {} user preferred locales", locales);

	for (const auto &locale : locales) {
		LogVerbose("Trying to load translation: {}", locale);
		if (HasTranslation(locale)) {
			LogVerbose("Best match locale: {}", locale);
			CopyUtf8(szCode, locale, sizeof(szCode));
			return;
		}
	}

	LogVerbose("No suitable translation found");
	strcpy(szCode, "en");
}
void OptionEntryLanguageCode::SaveToIni(string_view category) const
{
	SetIniValue(category.data(), key.data(), szCode);
}

void OptionEntryLanguageCode::CheckLanguagesAreInitialized() const
{
	if (!languages.empty())
		return;

	// Add well-known supported languages
	languages.emplace_back("bg", "Български");
	languages.emplace_back("cs", "Čeština");
	languages.emplace_back("da", "Dansk");
	languages.emplace_back("de", "Deutsch");
	languages.emplace_back("el", "Ελληνικά");
	languages.emplace_back("en", "English");
	languages.emplace_back("es", "Español");
	languages.emplace_back("fr", "Français");
	languages.emplace_back("hr", "Hrvatski");
	languages.emplace_back("it", "Italiano");

	if (font_mpq) {
		languages.emplace_back("ja", "日本語");
		languages.emplace_back("ko", "한국어");
	}

	languages.emplace_back("pl", "Polski");
	languages.emplace_back("pt_BR", "Português do Brasil");
	languages.emplace_back("ro", "Română");
	languages.emplace_back("ru", "Русский");
	languages.emplace_back("sv", "Svenska");
	languages.emplace_back("uk", "Українська");

	if (font_mpq) {
		languages.emplace_back("zh_CN", "汉语");
		languages.emplace_back("zh_TW", "漢語");
	}

	// Ensures that the ini specified language is present in languages list even if unknown (for example if someone starts to translate a new language)
	if (std::find_if(languages.begin(), languages.end(), [this](const auto &x) { return x.first == this->szCode; }) == languages.end()) {
		languages.emplace_back(szCode, szCode);
	}
}

size_t OptionEntryLanguageCode::GetListSize() const
{
	CheckLanguagesAreInitialized();
	return languages.size();
}
string_view OptionEntryLanguageCode::GetListDescription(size_t index) const
{
	CheckLanguagesAreInitialized();
	return languages[index].second;
}
size_t OptionEntryLanguageCode::GetActiveListIndex() const
{
	CheckLanguagesAreInitialized();
	auto found = std::find_if(languages.begin(), languages.end(), [this](const auto &x) { return x.first == this->szCode; });
	if (found == languages.end())
		return 0;
	return std::distance(languages.begin(), found);
}
void OptionEntryLanguageCode::SetActiveListIndex(size_t index)
{
	CopyUtf8(szCode, languages[index].first, sizeof(szCode));
	NotifyValueChanged();
}

LanguageOptions::LanguageOptions()
    : OptionCategoryBase("Language", N_("Language"), N_("Language Settings"))
{
	code.SetValueChangedCallback(OptionLanguageCodeChanged);
}
std::vector<OptionEntryBase *> LanguageOptions::GetEntries()
{
	return {
		&code,
	};
}

KeymapperOptions::KeymapperOptions()
    : OptionCategoryBase("Keymapping", N_("Keymapping"), N_("Keymapping Settings"))
{
	// Insert all supported keys: a-z, 0-9 and F1-F12.
	keyIDToKeyName.reserve(('Z' - 'A' + 1) + ('9' - '0' + 1) + 12);
	for (char c = 'A'; c <= 'Z'; ++c) {
		keyIDToKeyName.emplace(c, std::string(1, c));
	}
	for (char c = '0'; c <= '9'; ++c) {
		keyIDToKeyName.emplace(c, std::string(1, c));
	}
	for (int i = 0; i < 12; ++i) {
		keyIDToKeyName.emplace(DVL_VK_F1 + i, fmt::format("F{}", i + 1));
	}

	keyIDToKeyName.emplace(DVL_VK_LMENU, "LALT");
	keyIDToKeyName.emplace(DVL_VK_RMENU, "RALT");
	keyIDToKeyName.emplace(DVL_VK_SPACE, "SPACE");
	keyIDToKeyName.emplace(DVL_VK_RCONTROL, "RCONTROL");
	keyIDToKeyName.emplace(DVL_VK_LCONTROL, "LCONTROL");
	keyIDToKeyName.emplace(DVL_VK_SNAPSHOT, "PRINT");
	keyIDToKeyName.emplace(DVL_VK_PAUSE, "PAUSE");
	keyIDToKeyName.emplace(DVL_VK_TAB, "TAB");
	keyIDToKeyName.emplace(DVL_VK_MBUTTON, "MMOUSE");
	keyIDToKeyName.emplace(DVL_VK_X1BUTTON, "X1MOUSE");
	keyIDToKeyName.emplace(DVL_VK_X2BUTTON, "X2MOUSE");

	keyNameToKeyID.reserve(keyIDToKeyName.size());
	for (const auto &kv : keyIDToKeyName) {
		keyNameToKeyID.emplace(kv.second, kv.first);
	}
}

std::vector<OptionEntryBase *> KeymapperOptions::GetEntries()
{
	std::vector<OptionEntryBase *> entries;
	for (auto &action : actions) {
		entries.push_back(action.get());
	}
	return entries;
}

KeymapperOptions::Action::Action(string_view key, string_view name, string_view description, int defaultKey, std::function<void()> actionPressed, std::function<void()> actionReleased, std::function<bool()> enable, unsigned index)
    : OptionEntryBase(key, OptionEntryFlags::None, name, description)
    , defaultKey(defaultKey)
    , actionPressed(std::move(actionPressed))
    , actionReleased(std::move(actionReleased))
    , enable(std::move(enable))
    , dynamicIndex(index)
{
	if (index != 0) {
		dynamicKey = fmt::format(fmt::string_view(key.data(), key.size()), index);
		this->key = dynamicKey;
	}
}

string_view KeymapperOptions::Action::GetName() const
{
	if (dynamicIndex == 0)
		return _(name.data());
	dynamicName = fmt::format(_(name.data()), dynamicIndex);
	return dynamicName;
}

void KeymapperOptions::Action::LoadFromIni(string_view category)
{
	std::array<char, 64> result;
	if (!GetIniValue(category.data(), key.data(), result.data(), result.size())) {
		SetValue(defaultKey);
		return; // Use the default key if no key has been set.
	}

	std::string readKey = result.data();
	if (readKey.empty()) {
		SetValue(DVL_VK_INVALID);
		return;
	}

	auto keyIt = sgOptions.Keymapper.keyNameToKeyID.find(readKey);
	if (keyIt == sgOptions.Keymapper.keyNameToKeyID.end()) {
		// Use the default key if the key is unknown.
		Log("Keymapper: unknown key '{}'", readKey);
		SetValue(defaultKey);
		return;
	}

	// Store the key in action.key and in the map so we can save() the
	// actions while keeping the same order as they have been added.
	SetValue(keyIt->second);
}
void KeymapperOptions::Action::SaveToIni(string_view category) const
{
	if (boundKey == DVL_VK_INVALID) {
		// Just add an empty config entry if the action is unbound.
		SetIniValue(category.data(), key.data(), "");
	}
	auto keyNameIt = sgOptions.Keymapper.keyIDToKeyName.find(boundKey);
	if (keyNameIt == sgOptions.Keymapper.keyIDToKeyName.end()) {
		LogVerbose("Keymapper: no name found for key '{}'", key);
		return;
	}
	SetIniValue(category.data(), key.data(), keyNameIt->second.c_str());
}

string_view KeymapperOptions::Action::GetValueDescription() const
{
	if (boundKey == DVL_VK_INVALID)
		return "";
	auto keyNameIt = sgOptions.Keymapper.keyIDToKeyName.find(boundKey);
	if (keyNameIt == sgOptions.Keymapper.keyIDToKeyName.end()) {
		return "";
	}
	return keyNameIt->second;
}

bool KeymapperOptions::Action::SetValue(int value)
{
	if (value != DVL_VK_INVALID && sgOptions.Keymapper.keyIDToKeyName.find(value) == sgOptions.Keymapper.keyIDToKeyName.end()) {
		// Ignore invalid key values
		return false;
	}

	// Remove old key
	if (boundKey != DVL_VK_INVALID) {
		sgOptions.Keymapper.keyIDToAction.erase(boundKey);
		boundKey = DVL_VK_INVALID;
	}

	// Add new key
	if (value != DVL_VK_INVALID) {
		auto it = sgOptions.Keymapper.keyIDToAction.find(value);
		if (it != sgOptions.Keymapper.keyIDToAction.end()) {
			// Warn about overwriting keys.
			Log("Keymapper: key '{}' is already bound to action '{}', overwriting", value, it->second.get().name);
			it->second.get().boundKey = DVL_VK_INVALID;
		}

		sgOptions.Keymapper.keyIDToAction.insert_or_assign(value, *this);
		boundKey = value;
	}

	return true;
}

void KeymapperOptions::AddAction(string_view key, string_view name, string_view description, int defaultKey, std::function<void()> actionPressed, std::function<void()> actionReleased, std::function<bool()> enable, unsigned index)
{
	actions.push_back(std::unique_ptr<Action>(new Action(key, name, description, defaultKey, std::move(actionPressed), std::move(actionReleased), std::move(enable), index)));
}

void KeymapperOptions::KeyPressed(int key) const
{
	auto it = keyIDToAction.find(key);
	if (it == keyIDToAction.end())
		return; // Ignore unmapped keys.

	const Action &action = it->second.get();

	// Check that the action can be triggered and that the chat textbox is not
	// open.
	if (!action.actionPressed || (action.enable && !action.enable()) || talkflag)
		return;

	action.actionPressed();
}

void KeymapperOptions::KeyReleased(int key) const
{
	auto it = keyIDToAction.find(key);
	if (it == keyIDToAction.end())
		return; // Ignore unmapped keys.

	const Action &action = it->second.get();

	// Check that the action can be triggered and that the chat textbox is not
	// open.
	if (!action.actionReleased || (action.enable && !action.enable()) || talkflag)
		return;

	action.actionReleased();
}

string_view KeymapperOptions::KeyNameForAction(string_view actionName) const
{
	for (const auto &action : actions) {
		if (action->key == actionName && action->boundKey != DVL_VK_INVALID) {
			return action->GetValueDescription();
		}
	}
	return "";
}

uint32_t KeymapperOptions::KeyForAction(string_view actionName) const
{
	for (const auto &action : actions) {
		if (action->key == actionName && action->boundKey != DVL_VK_INVALID) {
			return action->boundKey;
		}
	}
	return DVL_VK_INVALID;
}

} // namespace devilution
