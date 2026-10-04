#pragma once
/*
 *	FATX filesystem support (Xbox 360)
 *
 *  Copyright (C) 2026 ExposureMG
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

// Embedding API of fatx_core.
//
// A volume is one FATX partition on an io_backend. It owns its own library
// context, so several volumes can be open at once, each used from one thread
// at a time (calls on one volume must not overlap). Errors are errno values
// (0 = success); messages from the library go to the volume's log sink.
//
// This header only needs the C++20 standard library.

#include <cstddef>
#include <cstdint>
#include <ctime>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include "io.hpp"

namespace fatx {

enum class							log_level { info, error, debug };
using log_sink = std::function<void(log_level, const std::string &)>;

// Where the filesystem is on the device. Tables and partitions are those of
// the command-line tools (--table / --partition / --offset / --size).
struct								location {
	std::string						table = "file";	// "file" (partition image), "mu", "hd", "kit"
	std::string						partition = "x2";	// "sc", "gc", "se1", "se2", "xdv", "x1", "x2"
	uint64_t						offset = 0;		// force the partition offset (0: from the table)
	uint64_t						size = 0;		// force the partition size (0: from the table / up to the end)
};

// A FATX partition found on a device.
struct								partition_info {
	location						where;
	std::string						table_name;		// "Xbox 360 retail hard disk", ...
	std::string						name;			// "data", "system cache", ...
	uint64_t						offset = 0;
	uint64_t						size = 0;
};

struct								entry_info {
	std::string						name;
	bool							directory = false;
	bool							read_only = false;
	bool							hidden = false;
	bool							system = false;
	bool							archive = false;
	bool							label = false;	// volume label file (name.txt)
	uint64_t						size = 0;
	uint32_t						first_cluster = 0;
	uint64_t						entry_offset = 0;	// device offset of its 64-byte directory record (0: root)
	std::time_t						created = 0;	// local time as stored, converted with mktime()
	std::time_t						accessed = 0;
	std::time_t						modified = 0;
};

struct								volume_info {
	std::string						label;
	uint32_t						serial = 0;		// volume id from the boot sector
	uint64_t						partition_offset = 0;
	uint64_t						partition_size = 0;
	uint32_t						cluster_size = 0;	// bytes
	uint64_t						cluster_count = 0;	// data clusters
	uint64_t						free_clusters = 0;
	uint32_t						root_cluster = 0;
	unsigned						fat_entry_size = 0;	// 2 (FATX16) or 4 (FATX32) bytes
	uint64_t						fat_offset = 0;
	uint64_t						fat_size = 0;
	uint64_t						data_offset = 0;
	bool							writable = false;
	bool							inconsistent = false;	// opening found errors; run check()
};

// Lists the FATX partitions present on a device for every known layout:
// Xbox 360 retail hard disk, devkit hard disk, memory unit, partition image.
[[nodiscard]] std::vector<partition_info>	probe(const std::shared_ptr<io_backend> &, const log_sink & = {});

// Checks a name for a new entry: 0, EINVAL (empty, ".", "..", a character
// FATX does not allow) or ENAMETOOLONG (more than 42 characters).
[[nodiscard]] int					validate_name(const std::string &);
// The characters FATX allows in names.
[[nodiscard]] const char*			allowed_name_characters();
// Maximum length of a name.
[[nodiscard]] std::size_t			max_name_length();

class								volume {
public:
	// Opens the filesystem at `where` on `device`. Write operations need
	// `writable` (and a writable device). Returns nullptr and sets *error.
	[[nodiscard]] static std::unique_ptr<volume>	open(std::shared_ptr<io_backend> device, const location &where,
										bool writable, log_sink sink, int *error);
									~volume();
									volume(const volume &) = delete;
	volume&							operator = (const volume &) = delete;

	// Volume information (free_clusters scans the FAT on first use).
	[[nodiscard]] volume_info		info();
	[[nodiscard]] int				lookup(const std::string &path, entry_info &out);	// (not "stat": a macro on MinGW)
	// Lists a directory (valid entries only, in on-disk order).
	[[nodiscard]] int				list(const std::string &path, std::vector<entry_info> &out);
	// Reads up to out.size() bytes of a file at offset; *got receives the count.
	[[nodiscard]] int				read(const std::string &path, uint64_t offset, std::span<std::byte> out, std::size_t *got);

	// Writing (volume opened writable). Names are checked with validate_name();
	// a name that differs only in case from an existing one is refused (EEXIST)
	// because the console does not tell them apart.
	//
	// Creates a file of `size` bytes, its clusters allocated up front (contents
	// undefined until written). EEXIST if the name is taken, ENOSPC if full.
	[[nodiscard]] int				create(const std::string &path, uint64_t size);
	// Writes data at offset; the file grows if needed.
	[[nodiscard]] int				write(const std::string &path, uint64_t offset, std::span<const std::byte> data);
	// Changes the size of a file.
	[[nodiscard]] int				truncate(const std::string &path, uint64_t size);
	// The clusters of a file or directory in chain order (empty for an empty file).
	[[nodiscard]] int				clusters(const std::string &path, std::vector<uint32_t> &out);
	// In-place replacement. The room a replacement may have: the clusters the
	// file has now, ceil(size / cluster size) clusters, in bytes.
	[[nodiscard]] int				replace_capacity(const std::string &path, uint64_t *bytes);
	// Starts an in-place replacement: the file keeps its directory record (same
	// index in its directory), name, attributes and creation date, and its first
	// clusters; then write() puts the new contents over them. new_size must fit
	// replace_capacity() (EFBIG otherwise, nothing changed). A smaller file keeps
	// the start of its chain and frees the clusters it no longer needs; an empty
	// one frees them all (an empty FATX file has no cluster).
	[[nodiscard]] int				replace(const std::string &path, uint64_t new_size);
	[[nodiscard]] int				mkdir(const std::string &path);
	// Renames or moves `from` to the full path `to`. With `replace`, an existing
	// file at `to` is deleted first; otherwise EEXIST.
	[[nodiscard]] int				rename(const std::string &from, const std::string &to, bool replace = false);
	// Deletes a file, or a directory and everything in it.
	[[nodiscard]] int				remove(const std::string &path);
	[[nodiscard]] int				set_label(const std::string &label);
	// Pushes pending writes to the device.
	[[nodiscard]] int				flush();

private:
	struct							impl;
	std::unique_ptr<impl>			d;
									volume();
};

// Creates a new, empty filesystem at `where` (mkfs.fatx). Everything on that
// partition is lost. cluster_blocks: 512-byte blocks per cluster, 0 = default
// for the size. An empty label gives the default "XBOX". No volume may be open
// on that partition meanwhile.
[[nodiscard]] int					format(std::shared_ptr<io_backend> device, const location &where,
										const std::string &label, uint32_t cluster_blocks, log_sink sink);

struct								check_report {
	unsigned						problems = 0;	// questions fsck.fatx would ask
	bool							repaired = false;	// repairs were written
	std::vector<std::string>		messages;		// what fsck.fatx would print
};

// Checks the filesystem at `where` like fsck.fatx: without `repair` nothing is
// written (fsck.fatx -n); with it every problem gets fsck.fatx's default answer
// (fsck.fatx -a). No volume may be open on that partition meanwhile (reopen it
// after a repair).
[[nodiscard]] int					check(std::shared_ptr<io_backend> device, const location &where,
										bool repair, check_report &report, log_sink sink = {});

}
