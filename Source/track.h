/**
 * @file track.h
 *
 * Interface of functionality tracking what the mouse cursor is pointing at.
 */
#pragma once

namespace devilution {

void InvalidateTargets();
void RepeatMouseAction();
bool track_isscrolling();

} // namespace devilution
