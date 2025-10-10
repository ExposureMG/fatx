#pragma once
/*
 *	FATX filesystem support (Xbox 360)
 *
 *  Copyright (C) 2012-2025 Christophe Duverger
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

#include <cstdint>
#include <memory>

using streamptr = uint64_t;  		// pointers in device
using clusptr = uint64_t;    		// cluster numbers
using filesize = uint64_t;   		// file size

class								fatx_context;	// context that contains pointers on used instances of following classes
class								frontend;		// arguments management & options values
class								device;			// read/write to the device
class								partition;		// partition identification & partition usefull values
class								dskmap;			// device file allocation table management
class								memmap;			// memory file allocation table used to handle deleted entries
class								entry;			// file or directory entry
class								vareas;			// vector of areas() in fat
class								buffer;			// file buffer

using ptr_vareas = std::shared_ptr<vareas>;
using ptr_buffer = std::unique_ptr<buffer>;
using ptr_entry = std::unique_ptr<entry>;
