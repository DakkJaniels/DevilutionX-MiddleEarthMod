#pragma once

#include <cstdint>
#include <cstring>

#include <SDL_endian.h>

#include "utils/endian.hpp"
#include "utils/stdcompat/cstddef.hpp"

namespace devilution {

/*
 * When a CEL is a multi-direction animation, it begins with 8 offsets to the start of
 * the animation for each direction.
 *
 * Fills the `out` array with a pointer to each direction.
 */
inline void CelGetDirectionFrames(const byte *data, const byte **out)
{
	for (size_t i = 0; i < 8; ++i) {
		out[i] = &data[LoadLE32(&data[i * 4])];
	}
}

inline void CelGetDirectionFrames(byte *data, byte **out)
{
	for (size_t i = 0; i < 8; ++i) {
		out[i] = &data[LoadLE32(&data[i * 4])];
	}
}

/**
 * Returns the pointer to the start of the frame data (often a header) and sets `frameSize` to the size of the data in bytes.
 */
inline byte *CelGetFrame(byte *data, int frame, int *frameSize)
{
	const std::uint32_t begin = LoadLE32(&data[(frame + 1) * sizeof(std::uint32_t)]);
	*frameSize = static_cast<int>(LoadLE32(&data[(frame + 2) * sizeof(std::uint32_t)]) - begin);
	return &data[begin];
}

/**
 * Returns the pointer to the start of the frame data (often a header) and sets `frameSize` to the size of the data in bytes.
 */
inline const byte *CelGetFrame(const byte *data, int frame, int *frameSize)
{
	const std::uint32_t begin = LoadLE32(&data[(frame + 1) * sizeof(std::uint32_t)]);
	*frameSize = static_cast<int>(LoadLE32(&data[(frame + 2) * sizeof(std::uint32_t)]) - begin);
	return &data[begin];
}

/**
 * Returns the pointer to the start of the frame's pixel data and sets `frameSize` to the size of the data in bytes.
 */
inline const byte *CelGetFrameClipped(const byte *data, int frame, int *frameSize)
{
	const byte *frameData = CelGetFrame(data, frame, frameSize);

	// The frame begins with a header that consists of 5 little-endian 16-bit integers
	// pointing to the start of the pixel data for rows 0, 32, 64, 96, and 128.
	const std::uint16_t begin = LoadLE16(frameData);

	*frameSize -= begin;
	return &frameData[begin];
}

} // namespace devilution
