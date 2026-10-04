#pragma once
/*
 *	FATX filesystem support (Xbox 360)
 *
 *  Copyright (C) 2012-2026 Christophe Duverger
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

// Whole-filesystem operations shared by the command-line tools (fatx.cpp)
// and the embedding API (volume.hpp). They act on the current context.

#include <string>

namespace fatx::actions {

// mkfs.fatx: writes the boot sector, clears the FAT and creates an empty root
// directory. The context must have been set up for mkfs. Returns 0 or an errno.
[[nodiscard]] int					make_filesystem();
// label.fatx: writes the volume label (name.txt in the root directory).
// Returns 0 or an errno.
[[nodiscard]] int					write_label(const std::string &);

}
