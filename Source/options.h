#pragma once

#include <cstddef>
#include <cstdint>
#include <unordered_map>

#include <SDL_version.h>

#include "pack.h"
#include "utils/enum_traits.h"
#include "utils/stdcompat/string_view.hpp"

namespace devilution {

enum class StartUpGameMode {
	/** @brief If hellfire is present, asks the user what game they want to start. */
	Ask = 0,
	Hellfire = 1,
	Diablo = 2,
};

enum class StartUpIntro {
	Off = 0,
	Once = 1,
	On = 2,
};

/** @brief Defines what splash screen should be shown at startup. */
enum class StartUpSplash {
	/** @brief Show no splash screen. */
	None = 0,
	/** @brief Show only TitleDialog. */
	TitleDialog = 1,
	/** @brief Show Logo and TitleDialog. */
	LogoAndTitleDialog = 2,
};

enum class ScalingQuality {
	NearestPixel,
	BilinearFiltering,
	AnisotropicFiltering,
};

enum class OptionEntryType {
	Boolean,
	List,
	Key,
};

enum class OptionEntryFlags {
	/** @brief No special logic. */
	None = 0,
	/** @brief Shouldn't be shown in settings dialog. */
	Invisible = 1 << 0,
	/** @brief Need to restart the current running game (single- or multiplayer) to take effect. */
	CantChangeInGame = 1 << 1,
	/** @brief Need to restart the current running multiplayer game to take effect. */
	CantChangeInMultiPlayer = 1 << 2,
	/** @brief Option is only relevant for Hellfire. */
	OnlyHellfire = 1 << 3,
	/** @brief Option is only relevant for Diablo. */
	OnlyDiablo = 1 << 4,
	/** @brief After option is changed the UI needs to be recreated. */
	RecreateUI = 1 << 5,
	/** @brief diablo.mpq must be present. */
	NeedDiabloMpq = 1 << 6,
	/** @brief hellfire.mpq must be present. */
	NeedHellfireMpq = 1 << 7,
};
use_enum_as_flags(OptionEntryFlags);

class OptionEntryBase {
public:
	OptionEntryBase(string_view key, OptionEntryFlags flags, string_view name, string_view description)
	    : key(key)
	    , flags(flags)
	    , name(name)
	    , description(description)
	{
	}
	[[nodiscard]] virtual string_view GetName() const;
	[[nodiscard]] string_view GetDescription() const;
	[[nodiscard]] virtual OptionEntryType GetType() const = 0;
	[[nodiscard]] OptionEntryFlags GetFlags() const;

	void SetValueChangedCallback(std::function<void()> callback);

	[[nodiscard]] virtual string_view GetValueDescription() const = 0;
	virtual void LoadFromIni(string_view category) = 0;
	virtual void SaveToIni(string_view category) const = 0;

protected:
	string_view key;
	OptionEntryFlags flags;
	string_view name;
	string_view description;
	void NotifyValueChanged();

private:
	std::function<void()> callback;
};

class OptionEntryBoolean : public OptionEntryBase {
public:
	OptionEntryBoolean(string_view key, OptionEntryFlags flags, string_view name, string_view description, bool defaultValue)
	    : OptionEntryBase(key, flags, name, description)
	    , defaultValue(defaultValue)
	    , value(defaultValue)
	{
	}
	[[nodiscard]] bool operator*() const
	{
		return value;
	}
	void SetValue(bool value);

	[[nodiscard]] OptionEntryType GetType() const override;
	[[nodiscard]] string_view GetValueDescription() const override;
	void LoadFromIni(string_view category) override;
	void SaveToIni(string_view category) const override;

private:
	bool defaultValue;
	bool value;
};

class OptionEntryListBase : public OptionEntryBase {
public:
	[[nodiscard]] virtual size_t GetListSize() const = 0;
	[[nodiscard]] virtual string_view GetListDescription(size_t index) const = 0;
	[[nodiscard]] virtual size_t GetActiveListIndex() const = 0;
	virtual void SetActiveListIndex(size_t index) = 0;

	[[nodiscard]] OptionEntryType GetType() const override;
	[[nodiscard]] string_view GetValueDescription() const override;

protected:
	OptionEntryListBase(string_view key, OptionEntryFlags flags, string_view name, string_view description)
	    : OptionEntryBase(key, flags, name, description)
	{
	}
};

class OptionEntryEnumBase : public OptionEntryListBase {
public:
	void LoadFromIni(string_view category) override;
	void SaveToIni(string_view category) const override;

	[[nodiscard]] size_t GetListSize() const override;
	[[nodiscard]] string_view GetListDescription(size_t index) const override;
	[[nodiscard]] size_t GetActiveListIndex() const override;
	void SetActiveListIndex(size_t index) override;

protected:
	OptionEntryEnumBase(string_view key, OptionEntryFlags flags, string_view name, string_view description, int defaultValue)
	    : OptionEntryListBase(key, flags, name, description)
	    , defaultValue(defaultValue)
	    , value(defaultValue)
	{
	}

	[[nodiscard]] int GetValueInternal() const
	{
		return value;
	}
	void SetValueInternal(int value);

	void AddEntry(int value, string_view name);

private:
	int defaultValue;
	int value;
	std::vector<string_view> entryNames;
	std::vector<int> entryValues;
};

template <typename T>
class OptionEntryEnum : public OptionEntryEnumBase {
public:
	OptionEntryEnum(string_view key, OptionEntryFlags flags, string_view name, string_view description, T defaultValue, std::initializer_list<std::pair<T, string_view>> entries)
	    : OptionEntryEnumBase(key, flags, name, description, static_cast<int>(defaultValue))
	{
		for (auto entry : entries) {
			AddEntry(static_cast<int>(entry.first), entry.second);
		}
	}
	[[nodiscard]] T operator*() const
	{
		return static_cast<T>(GetValueInternal());
	}
	void SetValue(T value)
	{
		SetValueInternal(static_cast<int>(value));
	}
};

class OptionEntryIntBase : public OptionEntryListBase {
public:
	void LoadFromIni(string_view category) override;
	void SaveToIni(string_view category) const override;

	[[nodiscard]] size_t GetListSize() const override;
	[[nodiscard]] string_view GetListDescription(size_t index) const override;
	[[nodiscard]] size_t GetActiveListIndex() const override;
	void SetActiveListIndex(size_t index) override;

protected:
	OptionEntryIntBase(string_view key, OptionEntryFlags flags, string_view name, string_view description, int defaultValue)
	    : OptionEntryListBase(key, flags, name, description)
	    , defaultValue(defaultValue)
	    , value(defaultValue)
	{
	}

	[[nodiscard]] int GetValueInternal() const
	{
		return value;
	}
	void SetValueInternal(int value);

	void AddEntry(int value);

private:
	int defaultValue;
	int value;
	mutable std::vector<std::string> entryNames;
	std::vector<int> entryValues;
};

template <typename T>
class OptionEntryInt : public OptionEntryIntBase {
public:
	OptionEntryInt(string_view key, OptionEntryFlags flags, string_view name, string_view description, T defaultValue, std::initializer_list<T> entries)
	    : OptionEntryIntBase(key, flags, name, description, static_cast<int>(defaultValue))
	{
		for (auto entry : entries) {
			AddEntry(static_cast<int>(entry));
		}
	}
	OptionEntryInt(string_view key, OptionEntryFlags flags, string_view name, string_view description, T defaultValue)
	    : OptionEntryInt(key, flags, name, description, defaultValue, { defaultValue })
	{
	}
	[[nodiscard]] T operator*() const
	{
		return static_cast<T>(GetValueInternal());
	}
	void SetValue(T value)
	{
		SetValueInternal(static_cast<int>(value));
	}
};

class OptionEntryLanguageCode : public OptionEntryListBase {
public:
	OptionEntryLanguageCode();

	void LoadFromIni(string_view category) override;
	void SaveToIni(string_view category) const override;

	[[nodiscard]] size_t GetListSize() const override;
	[[nodiscard]] string_view GetListDescription(size_t index) const override;
	[[nodiscard]] size_t GetActiveListIndex() const override;
	void SetActiveListIndex(size_t index) override;

	string_view operator*() const
	{
		return szCode;
	}

private:
	/** @brief Language code (ISO-15897) for text. */
	char szCode[6];
	mutable std::vector<std::pair<std::string, std::string>> languages;

	void CheckLanguagesAreInitialized() const;
};

class OptionEntryResolution : public OptionEntryListBase {
public:
	OptionEntryResolution();

	void LoadFromIni(string_view category) override;
	void SaveToIni(string_view category) const override;

	[[nodiscard]] size_t GetListSize() const override;
	[[nodiscard]] string_view GetListDescription(size_t index) const override;
	[[nodiscard]] size_t GetActiveListIndex() const override;
	void SetActiveListIndex(size_t index) override;

	Size operator*() const
	{
		return size;
	}

private:
	/** @brief View size. */
	Size size;
	mutable std::vector<std::pair<Size, std::string>> resolutions;

	void CheckResolutionsAreInitialized() const;
};

struct OptionCategoryBase {
	OptionCategoryBase(string_view key, string_view name, string_view description);

	[[nodiscard]] string_view GetKey() const;
	[[nodiscard]] string_view GetName() const;
	[[nodiscard]] string_view GetDescription() const;

	virtual std::vector<OptionEntryBase *> GetEntries() = 0;

protected:
	string_view key;
	string_view name;
	string_view description;
};

struct StartUpOptions : OptionCategoryBase {
	StartUpOptions();
	std::vector<OptionEntryBase *> GetEntries() override;

	OptionEntryEnum<StartUpGameMode> gameMode;
	/** @brief Play game intro video on diablo startup. */
	OptionEntryEnum<StartUpIntro> diabloIntro;
	/** @brief Play game intro video on hellfire startup. */
	OptionEntryEnum<StartUpIntro> hellfireIntro;
	OptionEntryEnum<StartUpSplash> splash;
};

struct DiabloOptions : OptionCategoryBase {
	DiabloOptions();
	std::vector<OptionEntryBase *> GetEntries() override;

	/** @brief Remembers what singleplayer hero/save was last used. */
	OptionEntryInt<std::uint32_t> lastSinglePlayerHero;
	/** @brief Remembers what multiplayer hero/save was last used. */
	OptionEntryInt<std::uint32_t> lastMultiplayerHero;
};

struct HellfireOptions : OptionCategoryBase {
	HellfireOptions();
	std::vector<OptionEntryBase *> GetEntries() override;

	/** @brief Cornerstone of the world item. */
	char szItem[sizeof(ItemPack) * 2 + 1];
	/** @brief Remembers what singleplayer hero/save was last used. */
	OptionEntryInt<std::uint32_t> lastSinglePlayerHero;
	/** @brief Remembers what multiplayer hero/save was last used. */
	OptionEntryInt<std::uint32_t> lastMultiplayerHero;
};

struct AudioOptions : OptionCategoryBase {
	AudioOptions();
	std::vector<OptionEntryBase *> GetEntries() override;

	/** @brief Movie and SFX volume. */
	OptionEntryInt<int> soundVolume;
	/** @brief Music volume. */
	OptionEntryInt<int> musicVolume;
	/** @brief Player emits sound when walking. */
	OptionEntryBoolean walkingSound;
	/** @brief Automatically equipping items on pickup emits the equipment sound. */
	OptionEntryBoolean autoEquipSound;
	/** @brief Picking up items emits the items pickup sound. */
	OptionEntryBoolean itemPickupSound;

	/** @brief Output sample rate (Hz) */
	OptionEntryInt<std::uint32_t> sampleRate;
	/** @brief The number of output channels (1 or 2) */
	OptionEntryInt<std::uint8_t> channels;
	/** @brief Buffer size (number of frames per channel) */
	OptionEntryInt<std::uint32_t> bufferSize;
	/** @brief Quality of the resampler, from 0 (lowest) to 10 (highest) */
	OptionEntryInt<std::uint8_t> resamplingQuality;
};

struct GraphicsOptions : OptionCategoryBase {
	GraphicsOptions();
	std::vector<OptionEntryBase *> GetEntries() override;

	OptionEntryResolution resolution;
	/** @brief Run in fullscreen or windowed mode. */
	OptionEntryBoolean fullscreen;
#if !defined(USE_SDL1) || defined(__3DS__)
	/** @brief Expand the aspect ratio to match the screen. */
	OptionEntryBoolean fitToScreen;
#endif
#ifndef USE_SDL1
	/** @brief Scale the image after rendering. */
	OptionEntryBoolean upscale;
	/** @brief See SDL_HINT_RENDER_SCALE_QUALITY. */
	OptionEntryEnum<ScalingQuality> scaleQuality;
	/** @brief Only scale by values divisible by the width and height. */
	OptionEntryBoolean integerScaling;
	/** @brief Enable vsync on the output. */
	OptionEntryBoolean vSync;
#endif
	/** @brief Gamma correction level. */
	OptionEntryInt<int> gammaCorrection;
	/** @brief Enable color cycling animations. */
	OptionEntryBoolean colorCycling;
	/** @brief Use alternate nest palette. */
	OptionEntryBoolean alternateNestArt;
#if SDL_VERSION_ATLEAST(2, 0, 0)
	/** @brief Use a hardware cursor (SDL2 only). */
	OptionEntryBoolean hardwareCursor;
	/** @brief Use a hardware cursor for items. */
	OptionEntryBoolean hardwareCursorForItems;
	/** @brief Maximum width / height for the hardware cursor. Larger cursors fall back to software. */
	OptionEntryInt<int> hardwareCursorMaxSize;
#endif
	/** @brief Enable FPS Limiter. */
	OptionEntryBoolean limitFPS;
	/** @brief Show FPS, even without the -f command line flag. */
	OptionEntryBoolean showFPS;
	/** @brief Display current/max health values on health globe. */
	OptionEntryBoolean showHealthValues;
	/** @brief Display current/max mana values on mana globe. */
	OptionEntryBoolean showManaValues;
};

struct GameplayOptions : OptionCategoryBase {
	GameplayOptions();
	std::vector<OptionEntryBase *> GetEntries() override;

	/** @brief Gameplay ticks per second. */
	OptionEntryInt<int> tickRate;
	/** @brief Enable double walk speed when in town. */
	OptionEntryBoolean runInTown;
	/** @brief Do not let the mouse leave the application window. */
	OptionEntryBoolean grabInput;
	/** @brief Enable the Theo quest. */
	OptionEntryBoolean theoQuest;
	/** @brief Enable the cow quest. */
	OptionEntryBoolean cowQuest;
	/** @brief Will players still damage other players in non-PvP mode. */
	OptionEntryBoolean friendlyFire;
	/** @brief Enable the bard hero class. */
	OptionEntryBoolean testBard;
	/** @brief Enable the babarian hero class. */
	OptionEntryBoolean testBarbarian;
	/** @brief Show the current level progress. */
	OptionEntryBoolean experienceBar;
	/** @brief Show enemy health at the top of the screen. */
	OptionEntryBoolean enemyHealthBar;
	/** @brief Automatically pick up gold when walking over it. */
	OptionEntryBoolean autoGoldPickup;
	/** @brief Auto-pickup elixirs */
	OptionEntryBoolean autoElixirPickup;
	/** @brief Enable or Disable auto-pickup in town */
	OptionEntryBoolean autoPickupInTown;
	/** @brief Recover mana when talking to Adria. */
	OptionEntryBoolean adriaRefillsMana;
	/** @brief Automatically attempt to equip weapon-type items when picking them up. */
	OptionEntryBoolean autoEquipWeapons;
	/** @brief Automatically attempt to equip armor-type items when picking them up. */
	OptionEntryBoolean autoEquipArmor;
	/** @brief Automatically attempt to equip helm-type items when picking them up. */
	OptionEntryBoolean autoEquipHelms;
	/** @brief Automatically attempt to equip shield-type items when picking them up. */
	OptionEntryBoolean autoEquipShields;
	/** @brief Automatically attempt to equip jewelry-type items when picking them up. */
	OptionEntryBoolean autoEquipJewelry;
	/** @brief Only enable 2/3 quests in each game session */
	OptionEntryBoolean randomizeQuests;
	/** @brief Indicates whether or not monster type (Animal, Demon, Undead) is shown along with other monster information. */
	OptionEntryBoolean showMonsterType;
	/** @brief Displays item labels for items on the ground.  */
	OptionEntryBoolean showItemLabels;
	/** @brief Refill belt from inventory, or rather, use potions/scrolls from inventory first when belt item is consumed.  */
	OptionEntryBoolean autoRefillBelt;
	/** @brief Locally disable clicking on shrines which permanently cripple character. */
	OptionEntryBoolean disableCripplingShrines;
	/** @brief Spell hotkeys instantly cast the spell. */
	OptionEntryBoolean quickCast;
	/** @brief Number of Healing potions to pick up automatically */
	OptionEntryInt<int> numHealPotionPickup;
	/** @brief Number of Full Healing potions to pick up automatically */
	OptionEntryInt<int> numFullHealPotionPickup;
	/** @brief Number of Mana potions to pick up automatically */
	OptionEntryInt<int> numManaPotionPickup;
	/** @brief Number of Full Mana potions to pick up automatically */
	OptionEntryInt<int> numFullManaPotionPickup;
	/** @brief Number of Rejuvenating potions to pick up automatically */
	OptionEntryInt<int> numRejuPotionPickup;
	/** @brief Number of Full Rejuvenating potions to pick up automatically */
	OptionEntryInt<int> numFullRejuPotionPickup;
};

struct ControllerOptions : OptionCategoryBase {
	ControllerOptions();
	std::vector<OptionEntryBase *> GetEntries() override;

	/** @brief SDL Controller mapping, see SDL_GameControllerDB. */
	char szMapping[1024];
	/** @brief Use dpad for spell hotkeys without holding "start" */
	bool bDpadHotkeys;
	/** @brief Shoulder gamepad shoulder buttons act as potions by default */
	bool bSwapShoulderButtonMode;
	/** @brief Configure gamepad joysticks deadzone */
	float fDeadzone;
#ifdef __vita__
	/** @brief Enable input via rear touchpad */
	bool bRearTouch;
#endif
};

struct NetworkOptions : OptionCategoryBase {
	NetworkOptions();
	std::vector<OptionEntryBase *> GetEntries() override;

	/** @brief Optionally bind to a specific network interface. */
	char szBindAddress[129];
	/** @brief Most recently entered ZeroTier Game ID. */
	char szPreviousZTGame[129];
	/** @brief Most recently entered Hostname in join dialog. */
	char szPreviousHost[129];
	/** @brief What network port to use. */
	OptionEntryInt<uint16_t> port;
};

struct ChatOptions : OptionCategoryBase {
	ChatOptions();
	std::vector<OptionEntryBase *> GetEntries() override;

	/** @brief Quick chat messages. */
	std::vector<std::string> szHotKeyMsgs[QUICK_MESSAGE_OPTIONS];
};

struct LanguageOptions : OptionCategoryBase {
	LanguageOptions();
	std::vector<OptionEntryBase *> GetEntries() override;

	OptionEntryLanguageCode code;
};

/** The Keymapper maps keys to actions. */
struct KeymapperOptions : OptionCategoryBase {
	/**
	 * Action represents an action that can be triggered using a keyboard
	 * shortcut.
	 */
	class Action final : public OptionEntryBase {
	public:
		[[nodiscard]] string_view GetName() const override;
		[[nodiscard]] OptionEntryType GetType() const override
		{
			return OptionEntryType::Key;
		}

		void LoadFromIni(string_view category) override;
		void SaveToIni(string_view category) const override;

		[[nodiscard]] string_view GetValueDescription() const override;

		bool SetValue(int value);

	private:
		Action(string_view key, string_view name, string_view description, int defaultKey, std::function<void()> actionPressed, std::function<void()> actionReleased, std::function<bool()> enable, unsigned index);
		int defaultKey;
		std::function<void()> actionPressed;
		std::function<void()> actionReleased;
		std::function<bool()> enable;
		int boundKey = DVL_VK_INVALID;
		unsigned dynamicIndex;
		std::string dynamicKey;
		mutable std::string dynamicName;

		friend struct KeymapperOptions;
	};

	KeymapperOptions();
	std::vector<OptionEntryBase *> GetEntries() override;

	void AddAction(
	    string_view key, string_view name, string_view description, int defaultKey,
	    std::function<void()> actionPressed,
	    std::function<void()> actionReleased = nullptr,
	    std::function<bool()> enable = nullptr,
	    unsigned index = 0);
	void KeyPressed(int key) const;
	void KeyReleased(int key) const;
	string_view KeyNameForAction(string_view actionName) const;
	uint32_t KeyForAction(string_view actionName) const;

private:
	std::vector<std::unique_ptr<Action>> actions;
	std::unordered_map<int, std::reference_wrapper<Action>> keyIDToAction;
	std::unordered_map<int, std::string> keyIDToKeyName;
	std::unordered_map<std::string, int> keyNameToKeyID;
};

struct Options {
	StartUpOptions StartUp;
	DiabloOptions Diablo;
	HellfireOptions Hellfire;
	AudioOptions Audio;
	GameplayOptions Gameplay;
	GraphicsOptions Graphics;
	ControllerOptions Controller;
	NetworkOptions Network;
	ChatOptions Chat;
	LanguageOptions Language;
	KeymapperOptions Keymapper;

	[[nodiscard]] std::vector<OptionCategoryBase *> GetCategories()
	{
		return {
			&Language,
			&StartUp,
			&Graphics,
			&Audio,
			&Diablo,
			&Hellfire,
			&Gameplay,
			&Controller,
			&Network,
			&Chat,
			&Keymapper,
		};
	}
};

extern DVL_API_FOR_TEST Options sgOptions;

bool HardwareCursorSupported();

/**
 * @brief Save game configurations to ini file
 */
void SaveOptions();

/**
 * @brief Load game configurations from ini file
 */
void LoadOptions();

} // namespace devilution
