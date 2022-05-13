#include "utils/sdl_bilinear_scale.hpp"

#include <cstdint>
#include <memory>

// Performs bilinear scaling using fixed-width integer math.

namespace devilution {

namespace {

int Frac(int fixedPoint)
{
	return fixedPoint & 0xffff;
}

int ToInt(int fixedPoint)
{
	return fixedPoint >> 16;
}

std::unique_ptr<int[]> CreateMixFactors(unsigned srcSize, unsigned dstSize)
{
	std::unique_ptr<int[]> result { new int[dstSize + 1] };
	const auto scale = static_cast<int>(65536.0 * static_cast<float>(srcSize - 1) / dstSize);
	int mix = 0;
	for (unsigned i = 0; i <= dstSize; ++i) {
		result[i] = mix;
		mix = Frac(mix) + scale;
	}
	return result;
};

uint8_t MixColors(uint8_t first, uint8_t second, int ratio)
{
	return ToInt((second - first) * ratio) + first;
}

uint8_t MixColorsWithAlpha(uint8_t first, uint8_t firstAlpha,
    uint8_t second, uint8_t secondAlpha,
    uint8_t mixedAlpha, int ratio)
{
	if (mixedAlpha == 0)
		return 0;
	if (mixedAlpha == 255)
		return MixColors(first, second, ratio);

	const int firstWithAlpha = first * firstAlpha;
	const int secondWithAlpha = second * secondAlpha;

	// We want to calculate:
	//
	//    (ToInt((secondWithAlpha - firstWithAlpha) * ratio) + firstWithAlpha) / mixedAlpha
	//
	// However, the above written as-is can overflow in the argument to `ToInt`.
	// To avoid the overflow we divide each term by `mixedAlpha` separately.
	//
	// This would be lower precision and could result in a negative overall result,
	// so we do the rounding integer division for each term (instead of a truncating one):
	//
	//    (a + (a - 1)) / b`
	return ToInt((secondWithAlpha - firstWithAlpha) * ((ratio + (mixedAlpha - 1)) / mixedAlpha)) + (firstWithAlpha + (mixedAlpha - 1)) / mixedAlpha;
}

} // namespace

void BilinearScale32(SDL_Surface *src, SDL_Surface *dst)
{
	const std::unique_ptr<int[]> mixXs = CreateMixFactors(src->w, dst->w);
	const std::unique_ptr<int[]> mixYs = CreateMixFactors(src->h, dst->h);

	const unsigned dgap = dst->pitch - dst->w * 4;

	auto *srcPixels = static_cast<uint8_t *>(src->pixels);
	auto *dstPixels = static_cast<uint8_t *>(dst->pixels);

	int *curMixY = &mixYs[0];
	unsigned srcY = 0;
	for (unsigned y = 0; y < static_cast<unsigned>(dst->h); ++y) {
		uint8_t *s[4] = {
			srcPixels,                 // Self
			srcPixels + 4,             // Right
			srcPixels + src->pitch,    // Bottom
			srcPixels + src->pitch + 4 // Bottom right
		};

		int *curMixX = &mixXs[0];
		unsigned srcX = 0;
		for (unsigned x = 0; x < static_cast<unsigned>(dst->w); ++x) {
			const int mixX = Frac(*curMixX);
			const int mixY = Frac(*curMixY);

			const uint8_t alpha0 = MixColors(s[0][3], s[1][3], mixX);
			const uint8_t alpha1 = MixColors(s[2][3], s[3][3], mixX);
			const uint8_t finalAlpha = MixColors(alpha0, alpha1, mixY);

			if (finalAlpha == 0) {
				dstPixels[0] = 0;
				dstPixels[1] = 0;
				dstPixels[2] = 0;
				dstPixels[3] = 0;
			} else if (finalAlpha == 255) {
				for (unsigned channel = 0; channel < 3; ++channel) {
					dstPixels[channel] = MixColors(
					    MixColors(s[0][channel], s[1][channel], mixX),
					    MixColors(s[2][channel], s[3][channel], mixX),
					    mixY);
				}
				dstPixels[3] = 255;
			} else {
				for (unsigned channel = 0; channel < 3; ++channel) {
					dstPixels[channel] = MixColorsWithAlpha(
					    MixColorsWithAlpha(s[0][channel], s[0][3], s[1][channel], s[1][3], alpha0, mixX),
					    alpha0,
					    MixColorsWithAlpha(s[2][channel], s[2][3], s[3][channel], s[3][3], alpha1, mixX),
					    alpha1,
					    finalAlpha,
					    mixY);
				}
				dstPixels[3] = finalAlpha;
			}

			++curMixX;
			if (*curMixX > 0) {
				unsigned step = ToInt(*curMixX);
				srcX += step;
				if (srcX <= static_cast<unsigned>(src->w)) {
					step *= 4;
					for (auto &v : s) {
						v += step;
					}
				}
			}

			dstPixels += 4;
		}

		++curMixY;
		if (*curMixY > 0) {
			const unsigned step = ToInt(*curMixY);
			srcY += step;
			if (srcY < static_cast<unsigned>(src->h)) {
				srcPixels += step * src->pitch;
			}
		}

		dstPixels += dgap;
	}
}

} // namespace devilution
