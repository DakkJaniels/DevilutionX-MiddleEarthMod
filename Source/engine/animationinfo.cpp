/**
 * @file animationinfo.cpp
 *
 * Contains the core animation information and related logic
 */

#include "animationinfo.h"
#include "appfat.h"
#include "nthread.h"
#include "utils/log.hpp"
#include "utils/stdcompat/algorithm.hpp"

namespace devilution {

int AnimationInfo::GetFrameToUseForRendering() const
{
	// Normal logic is used,
	// - if no frame-skipping is required and so we have exactly one Animationframe per game tick
	// or
	// - if we load from a savegame where the new variables are not stored (we don't want to break savegame compatiblity because of smoother rendering of one animation)
	if (RelevantFramesForDistributing <= 0)
		return std::max(0, CurrentFrame);

	if (CurrentFrame >= RelevantFramesForDistributing)
		return CurrentFrame;

	float ticksSinceSequenceStarted = TicksSinceSequenceStarted;
	if (TicksSinceSequenceStarted < 0) {
		ticksSinceSequenceStarted = 0.0F;
		Log("GetFrameToUseForRendering: Invalid TicksSinceSequenceStarted {}", TicksSinceSequenceStarted);
	}

	// we don't use the processed game ticks alone but also the fraction of the next game tick (if a rendering happens between game ticks). This helps to smooth the animations.
	float totalTicksForCurrentAnimationSequence = GetProgressToNextGameTick() + ticksSinceSequenceStarted;

	int absoluteAnimationFrame = static_cast<int>(totalTicksForCurrentAnimationSequence * TickModifier);
	if (SkippedFramesFromPreviousAnimation > 0) {
		// absoluteAnimationFrames contains also the Frames from the previous Animation, so if we want to get the current Frame we have to remove them
		absoluteAnimationFrame -= SkippedFramesFromPreviousAnimation;
		if (absoluteAnimationFrame < 0) {
			// We still display the remains of the previous Animation
			absoluteAnimationFrame = NumberOfFrames + absoluteAnimationFrame;
		}
	} else if (absoluteAnimationFrame >= RelevantFramesForDistributing) {
		// this can happen if we are at the last frame and the next game tick is due (gfProgressToNextGameTick >= 1.0f)
		if (absoluteAnimationFrame >= (RelevantFramesForDistributing + 1)) {
			// we should never have +2 frames even if next game tick is due
			Log("GetFrameToUseForRendering: Calculated an invalid Animation Frame (Calculated {} MaxFrame {})", absoluteAnimationFrame, RelevantFramesForDistributing);
		}
		return RelevantFramesForDistributing - 1;
	}
	if (absoluteAnimationFrame < 0) {
		Log("GetFrameToUseForRendering: Calculated an invalid Animation Frame (Calculated {})", absoluteAnimationFrame);
		return 0;
	}
	return absoluteAnimationFrame;
}

float AnimationInfo::GetAnimationProgress() const
{
	float ticksSinceSequenceStarted = TicksSinceSequenceStarted;
	float tickModifier = TickModifier;

	if (RelevantFramesForDistributing <= 0) {
		// This logic is used if animation distribution is not active (see GetFrameToUseForRendering).
		// In this case the variables calculated with animation distribution are not initialized and we have to calculate them on the fly with the given information.
		ticksSinceSequenceStarted = static_cast<float>((CurrentFrame * TicksPerFrame) + TickCounterOfCurrentFrame);
		tickModifier = 1.0F / static_cast<float>(TicksPerFrame);
	}

	float totalTicksForCurrentAnimationSequence = GetProgressToNextGameTick() + ticksSinceSequenceStarted;
	float progressInAnimationFrames = totalTicksForCurrentAnimationSequence * tickModifier;
	float animationFraction = progressInAnimationFrames / static_cast<float>(NumberOfFrames);
	return animationFraction;
}

void AnimationInfo::SetNewAnimation(std::optional<CelSprite> celSprite, int numberOfFrames, int ticksPerFrame, AnimationDistributionFlags flags /*= AnimationDistributionFlags::None*/, int numSkippedFrames /*= 0*/, int distributeFramesBeforeFrame /*= 0*/, float previewShownGameTickFragments /*= 0.F*/)
{
	if ((flags & AnimationDistributionFlags::RepeatedAction) == AnimationDistributionFlags::RepeatedAction && distributeFramesBeforeFrame != 0 && NumberOfFrames == numberOfFrames && CurrentFrame + 1 >= distributeFramesBeforeFrame && CurrentFrame != NumberOfFrames - 1) {
		// We showed the same Animation (for example a melee attack) before but truncated the Animation.
		// So now we should add them back to the new Animation. This increases the speed of the current Animation but the game logic/ticks isn't affected.
		SkippedFramesFromPreviousAnimation = NumberOfFrames - CurrentFrame - 1;
	} else {
		SkippedFramesFromPreviousAnimation = 0;
	}

	if (ticksPerFrame <= 0) {
		Log("SetNewAnimation: Invalid ticksPerFrame {}", ticksPerFrame);
		ticksPerFrame = 1;
	}

	this->celSprite = celSprite;
	NumberOfFrames = numberOfFrames;
	CurrentFrame = numSkippedFrames;
	TickCounterOfCurrentFrame = 0;
	TicksPerFrame = ticksPerFrame;
	TicksSinceSequenceStarted = 0.F;
	RelevantFramesForDistributing = 0;
	TickModifier = 0.0F;
	IsPetrified = false;

	if (numSkippedFrames != 0 || flags != AnimationDistributionFlags::None) {
		// Animation Frames that will be adjusted for the skipped Frames/game ticks
		int relevantAnimationFramesForDistributing = numberOfFrames;
		if (distributeFramesBeforeFrame != 0) {
			// After an attack hits (_pAFNum or _pSFNum) it can be canceled or another attack can be queued and this means the animation is canceled.
			// In normal attacks frame skipping always happens before the attack actual hit.
			// This has the advantage that the sword or bow always points to the enemy when the hit happens (_pAFNum or _pSFNum).
			// Our distribution logic must also regard this behaviour, so we are not allowed to distribute the skipped animations after the actual hit (_pAnimStopDistributingAfterFrame).
			relevantAnimationFramesForDistributing = distributeFramesBeforeFrame - 1;
		}

		// Game ticks that will be adjusted for the skipped Frames/game ticks
		int relevantAnimationTicksForDistribution = relevantAnimationFramesForDistributing * ticksPerFrame;

		// How many game ticks will the Animation be really shown (skipped Frames and game ticks removed)
		float relevantAnimationTicksWithSkipping = relevantAnimationTicksForDistribution - (numSkippedFrames * ticksPerFrame);

		if ((flags & AnimationDistributionFlags::ProcessAnimationPending) == AnimationDistributionFlags::ProcessAnimationPending) {
			// If ProcessAnimation will be called after SetNewAnimation (in same game tick as SetNewAnimation), we increment the Animation-Counter.
			// If no delay is specified, this will result in complete skipped frame (see ProcessAnimation).
			// But if we have a delay specified, this would only result in a reduced time the first frame is shown (one skipped delay).
			// Because of that, we only the remove one game tick from the time the Animation is shown
			relevantAnimationTicksWithSkipping -= 1.F;
			// The Animation Distribution Logic needs to account how many game ticks passed since the Animation started.
			// Because ProcessAnimation will increase this later (in same game tick as SetNewAnimation), we correct this upfront.
			// This also means Rendering should never hapen with TicksSinceSequenceStarted < 0.
			TicksSinceSequenceStarted = -1.F;
		}

		// The preview animation was shown some times (less then one game tick)
		// So we overall have a longer time the animation is shown
		TicksSinceSequenceStarted += previewShownGameTickFragments;
		relevantAnimationTicksWithSkipping += previewShownGameTickFragments;

		if ((flags & AnimationDistributionFlags::SkipsDelayOfLastFrame) == AnimationDistributionFlags::SkipsDelayOfLastFrame) {
			// The logic for player/monster/... (not ProcessAnimation) only checks the frame not the delay.
			// That means if a delay is specified, the last-frame is shown less than the other frames
			// Example:
			// If we have a animation with 3 frames and with a delay of 1 (ticksPerFrame = 2).
			// The logic checks "if (frame == 3) { start_new_animation(); }"
			// This will result that frame 4 is the last shown Animation Frame.
			// GameTick		Frame		Cnt
			// 1			1			0
			// 2			1			1
			// 3			2			0
			// 3			2			1
			// 4			3			0
			// 5			-			-
			// in game tick 5 ProcessPlayer sees Frame = 3 and stops the animation.
			// But Frame 3 is only shown 1 game tick and all other Frames are shown 2 game ticks.
			// Thats why we need to remove the Delay of the last Frame from the time (game ticks) the Animation is shown
			relevantAnimationTicksWithSkipping -= (ticksPerFrame - 1);
		}

		// The truncated Frames from previous Animation will also be shown, so we also have to distribute them for the given time (game ticks)
		relevantAnimationTicksForDistribution += (SkippedFramesFromPreviousAnimation * ticksPerFrame);

		// if we skipped Frames we need to expand the game ticks to make one game tick for this Animation "faster"
		float tickModifier = static_cast<float>(relevantAnimationTicksForDistribution) / relevantAnimationTicksWithSkipping;

		// tickModifier specifies the Animation fraction per game tick, so we have to remove the delay from the variable
		tickModifier /= static_cast<float>(ticksPerFrame);

		RelevantFramesForDistributing = relevantAnimationFramesForDistributing;
		TickModifier = tickModifier;
	}
}

void AnimationInfo::ChangeAnimationData(std::optional<CelSprite> celSprite, int numberOfFrames, int ticksPerFrame)
{
	if (numberOfFrames != NumberOfFrames || ticksPerFrame != TicksPerFrame) {
		// Ensure that the CurrentFrame is still valid and that we disable ADL cause the calculcated values (for example TickModifier) could be wrong
		if (numberOfFrames >= 1)
			CurrentFrame = clamp(CurrentFrame, 0, numberOfFrames - 1);
		else
			CurrentFrame = -1;

		NumberOfFrames = numberOfFrames;
		TicksPerFrame = ticksPerFrame;
		TicksSinceSequenceStarted = 0.F;
		RelevantFramesForDistributing = 0;
		TickModifier = 0.0F;
	}
	this->celSprite = celSprite;
}

void AnimationInfo::ProcessAnimation(bool reverseAnimation /*= false*/, bool dontProgressAnimation /*= false*/)
{
	TickCounterOfCurrentFrame++;
	if (dontProgressAnimation)
		return;
	TicksSinceSequenceStarted++;
	if (TickCounterOfCurrentFrame >= TicksPerFrame) {
		TickCounterOfCurrentFrame = 0;
		if (reverseAnimation) {
			--CurrentFrame;
			if (CurrentFrame == -1) {
				CurrentFrame = NumberOfFrames - 1;
				TicksSinceSequenceStarted = 0.F;
			}
		} else {
			++CurrentFrame;
			if (CurrentFrame >= NumberOfFrames) {
				CurrentFrame = 0;
				TicksSinceSequenceStarted = 0.F;
			}
		}
	}
}

float AnimationInfo::GetProgressToNextGameTick() const
{
	if (IsPetrified)
		return 0.0F;
	return gfProgressToNextGameTick;
}

} // namespace devilution
