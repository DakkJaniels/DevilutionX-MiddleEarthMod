/**
 * @file sound.cpp
 *
 * Implementation of functions setting up the audio pipeline.
 */
#include "sound.h"

#include <cstdint>
#include <list>
#include <memory>
#include <mutex>

#include <SDL.h>

#include "engine/assets.hpp"
#include "init.h"
#include "options.h"
#include "utils/log.hpp"
#include "utils/math.h"
#include "utils/sdl_mutex.h"
#include "utils/stdcompat/algorithm.hpp"
#include "utils/stdcompat/optional.hpp"
#include "utils/stdcompat/shared_ptr_array.hpp"
#include "utils/stubs.h"

namespace devilution {

bool gbSndInited;
/** The active background music track id. */
_music_id sgnMusicTrack = NUM_MUSIC;

bool gbMusicOn = true;
/** Specifies whether sound effects are enabled. */
bool gbSoundOn = true;

namespace {

SoundSample music;

std::string GetMp3Path(const char *path)
{
	std::string mp3Path = path;
	const std::string::size_type dot = mp3Path.find_last_of('.');
	mp3Path.replace(dot + 1, mp3Path.size() - (dot + 1), "mp3");
	return mp3Path;
}

bool LoadAudioFile(const char *path, bool stream, bool errorDialog, SoundSample &result)
{
#ifndef STREAM_ALL_AUDIO
	if (stream) {
#endif
		if (result.SetChunkStream(GetMp3Path(path), /*isMp3=*/true, /*logErrors=*/false) != 0) {
			SDL_ClearError();
			if (result.SetChunkStream(path, /*isMp3=*/false, /*logErrors=*/true) != 0) {
				if (errorDialog)
					ErrSdl();
				return false;
			}
		}
#ifndef STREAM_ALL_AUDIO
	} else {
		bool isMp3 = true;
		SDL_RWops *file = OpenAsset(GetMp3Path(path).c_str());
		if (file == nullptr) {
			SDL_ClearError();
			isMp3 = false;
			file = OpenAsset(path);
			if (file == nullptr) {
				if (errorDialog)
					ErrDlg("OpenAsset failed", path, __FILE__, __LINE__);
				return false;
			}
		}
		size_t dwBytes = SDL_RWsize(file);
		auto waveFile = MakeArraySharedPtr<std::uint8_t>(dwBytes);
		if (SDL_RWread(file, waveFile.get(), dwBytes, 1) == 0) {
			if (errorDialog)
				ErrDlg("Failed to read file", fmt::format("{}: {}", path, SDL_GetError()), __FILE__, __LINE__);
			return false;
		}
		int error = result.SetChunk(waveFile, dwBytes, isMp3);
		SDL_RWclose(file);
		if (error != 0) {
			if (errorDialog)
				ErrSdl();
			return false;
		}
	}
#endif
	return true;
}

std::list<std::unique_ptr<SoundSample>> duplicateSounds;
std::optional<SdlMutex> duplicateSoundsMutex;

SoundSample *DuplicateSound(const SoundSample &sound)
{
	auto duplicate = std::make_unique<SoundSample>();
	if (duplicate->DuplicateFrom(sound) != 0)
		return nullptr;
	auto *result = duplicate.get();
	decltype(duplicateSounds.begin()) it;
	{
		const std::lock_guard<SdlMutex> lock(*duplicateSoundsMutex);
		duplicateSounds.push_back(std::move(duplicate));
		it = duplicateSounds.end();
		--it;
	}
	result->SetFinishCallback([it]([[maybe_unused]] Aulib::Stream &stream) {
		const std::lock_guard<SdlMutex> lock(*duplicateSoundsMutex);
		duplicateSounds.erase(it);
	});
	return result;
}

/** Maps from track ID to track name in spawn. */
const char *const SpawnMusicTracks[NUM_MUSIC] = {
	"Music\\sTowne.wav",
	"Music\\sLvlA.wav",
	"Music\\sLvlA.wav",
	"Music\\sLvlA.wav",
	"Music\\sLvlA.wav",
	"Music\\DLvlE.wav",
	"Music\\DLvlF.wav",
	"Music\\sintro.wav",
};
/** Maps from track ID to track name. */
const char *const MusicTracks[NUM_MUSIC] = {
	"Music\\DTowne.wav",
	"Music\\DLvlA.wav",
	"Music\\DLvlB.wav",
	"Music\\DLvlC.wav",
	"Music\\DLvlD.wav",
	"Music\\DLvlE.wav",
	"Music\\DLvlF.wav",
	"Music\\Dintro.wav",
};

int CapVolume(int volume)
{
	return clamp(volume, VOLUME_MIN, VOLUME_MAX);
}

} // namespace

void ClearDuplicateSounds()
{
	const std::lock_guard<SdlMutex> lock(*duplicateSoundsMutex);
	duplicateSounds.clear();
}

void snd_play_snd(TSnd *pSnd, int lVolume, int lPan)
{
	if (pSnd == nullptr || !gbSoundOn) {
		return;
	}

	uint32_t tc = SDL_GetTicks();
	if (tc - pSnd->start_tc < 80) {
		return;
	}

	SoundSample *sound = &pSnd->DSB;
	if (sound->IsPlaying()) {
		sound = DuplicateSound(*sound);
		if (sound == nullptr)
			return;
	}

	sound->PlayWithVolumeAndPan(lVolume, *sgOptions.Audio.soundVolume, lPan);
	pSnd->start_tc = tc;
}

std::unique_ptr<TSnd> sound_file_load(const char *path, bool stream)
{
	auto snd = std::make_unique<TSnd>();
	snd->start_tc = SDL_GetTicks() - 80 - 1;
#ifndef NOSOUND
	LoadAudioFile(path, stream, /*errorDialog=*/true, snd->DSB);
#endif
	return snd;
}

TSnd::~TSnd()
{
	if (DSB.IsLoaded())
		DSB.Stop();
	DSB.Release();
}

void snd_init()
{
	sgOptions.Audio.soundVolume.SetValue(CapVolume(*sgOptions.Audio.soundVolume));
	gbSoundOn = *sgOptions.Audio.soundVolume > VOLUME_MIN;
	sgbSaveSoundOn = gbSoundOn;

	sgOptions.Audio.musicVolume.SetValue(CapVolume(*sgOptions.Audio.musicVolume));
	gbMusicOn = *sgOptions.Audio.musicVolume > VOLUME_MIN;

	// Initialize the SDL_audiolib library. Set the output sample rate to
	// 22kHz, the audio format to 16-bit signed, use 2 output channels
	// (stereo), and a 2KiB output buffer.
	if (!Aulib::init(*sgOptions.Audio.sampleRate, AUDIO_S16, *sgOptions.Audio.channels, *sgOptions.Audio.bufferSize)) {
		LogError(LogCategory::Audio, "Failed to initialize audio (Aulib::init): {}", SDL_GetError());
		return;
	}
	LogVerbose(LogCategory::Audio, "Aulib sampleRate={} channels={} frameSize={} format={:#x}",
	    Aulib::sampleRate(), Aulib::channelCount(), Aulib::frameSize(), Aulib::sampleFormat());

	duplicateSoundsMutex.emplace();
	gbSndInited = true;
}

void snd_deinit()
{
	if (gbSndInited) {
		Aulib::quit();
		duplicateSoundsMutex = std::nullopt;
	}

	gbSndInited = false;
}

void music_stop()
{
	music.Release();
	sgnMusicTrack = NUM_MUSIC;
}

void music_start(uint8_t nTrack)
{
	const char *trackPath;

	assert(nTrack < NUM_MUSIC);
	music_stop();
	if (!gbMusicOn)
		return;
	if (spawn_mpq)
		trackPath = SpawnMusicTracks[nTrack];
	else
		trackPath = MusicTracks[nTrack];

#ifdef DISABLE_STREAMING_MUSIC
	const bool stream = false;
#else
	const bool stream = true;
#endif
	if (!LoadAudioFile(trackPath, stream, /*errorDialog=*/false, music)) {
		music_stop();
		return;
	}

	music.SetVolume(*sgOptions.Audio.musicVolume, VOLUME_MIN, VOLUME_MAX);
	if (!diablo_is_focused())
		music_mute();
	if (!music.Play(/*numIterations=*/0)) {
		LogError(LogCategory::Audio, "Aulib::Stream::play (from music_start): {}", SDL_GetError());
		music_stop();
		return;
	}

	sgnMusicTrack = (_music_id)nTrack;
}

void sound_disable_music(bool disable)
{
	if (disable) {
		music_stop();
	} else if (sgnMusicTrack != NUM_MUSIC) {
		music_start(sgnMusicTrack);
	}
}

int sound_get_or_set_music_volume(int volume)
{
	if (volume == 1)
		return *sgOptions.Audio.musicVolume;

	sgOptions.Audio.musicVolume.SetValue(volume);

	if (music.IsLoaded())
		music.SetVolume(*sgOptions.Audio.musicVolume, VOLUME_MIN, VOLUME_MAX);

	return *sgOptions.Audio.musicVolume;
}

int sound_get_or_set_sound_volume(int volume)
{
	if (volume == 1)
		return *sgOptions.Audio.soundVolume;

	sgOptions.Audio.soundVolume.SetValue(volume);

	return *sgOptions.Audio.soundVolume;
}

void music_mute()
{
	if (music.IsLoaded())
		music.Mute();
}

void music_unmute()
{
	if (music.IsLoaded())
		music.Unmute();
}

} // namespace devilution
