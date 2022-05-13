/**
 * @file restrict.cpp
 *
 * Implementation of functionality for checking if the game will be able run on the system.
 */

#include "appfat.h"
#include "utils/file_util.h"
#include "utils/paths.h"

namespace devilution {

void ReadOnlyTest()
{
	const std::string path = paths::PrefPath() + "Diablo1ReadOnlyTest.foo";
	FILE *f = FOpen(path.c_str(), "wt");
	if (f == nullptr) {
		DirErrorDlg(paths::PrefPath());
	}

	fclose(f);
	RemoveFile(path);
}

} // namespace devilution
