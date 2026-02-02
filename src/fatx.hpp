#pragma once
/*
 *	FATX filesystem support (Xbox 360)
 *
 *  Copyright (C) 2012-2024 Christophe Duverger
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

/*
 *	Compile with:
 *	-D NDEBUG		to avoid produce debug informations output
 *	-D DBG_INIT		to produce debug on initialisation sequence
 *	-D DBG_READ		to print bytes read at device level
 *	-D DBG_WRITE	to print bytes written at device level
 *	-D DBG_DIFF		to print accesses to diff file
 *	-D DBG_SEM		to print accesses to semaphores
 *	-D DBGSEM=\"x\"	to print only semaphore named x
 *	-D DBG_BUFFER	to print buffer operations
 *	-D DBGBUFDMP=x	to print x bytes of buffer at each change
 *	-D DBG_CACHE	to print cache operations
 *	-D DBG_CACHDMP	to dump cache at each change
 *	-D DBG_AREAS	to print fat areas()
 *	-D DBG_GUESS	to print guesses
 *	-D DBGCR=x		to limit to x bytes per line
 *	-D DBGLIMIT=x	to limit to x bytes the printing of read/write
 *	-D DBG_FAT		to print the FAT
 *	-D DBG_GAPS		to print gaps
 *	-D NO_WRITE		to fake writing but no modification is done
 *	-D NO_CACHE		to disable FAT cache
 *
 *	Make symlink to executable with names:
 *	"fusefatx"		for fuse filesystem support
 *	"mkfs.fatx"		for filesystem creation
 *	"fsck.fatx"		for filesystem check and repair
 *	"unrm.fatx"		for recovery of deleted files
 *	"label.fatx"	for display or change volume label
 *
 *	Use -h option for each symlink call to find syntax and options list
 */

#include "types.hpp"
#include "device.hpp"
#include "frontend.hpp"
#include "partition.hpp"
#include "diskmap.hpp"
#include "entry.hpp"
#include "fuse_ops.hpp"

#ifndef PACKAGE_VERSION
	#define PACKAGE_VERSION "1.18"
#endif

#if !defined NDEBUG && !defined DBGCR
	#define DBGCR					48
#endif

class								fatx_context {
private:
	static fatx_context*			fatxc;
public:
	frontend&						mmi;
	device							dev;
	partition						par;
	dskmap*							fat;
	entry*							root;
	bool							ready;

									fatx_context(frontend&);
									~fatx_context();
	[[nodiscard]] int				setup();
	void							destroy();
	static fatx_context*			get() { return fatxc; }
	static void						set(fatx_context* const fc) { fatxc = fc; }
};

namespace clsarithm {

inline clusptr						siz2cls(filesize);
inline streamptr					cls2ptr(clusptr );
inline clusptr						ptr2cls(streamptr);
inline streamptr					cls2fat(clusptr);
inline std::string					clsprint(clusptr, clusptr);

inline clusptr						siz2cls(filesize s) {
	return
		(s >> fatx_context::get()->par.clus_pow) +
		(s % fatx_context::get()->par.clus_size != 0 ? 1 : 0)
	;
}
inline streamptr					cls2ptr(clusptr p) {
	if (p < fatx_context::get()->par.root_clus || p > fatx_context::get()->par.clus_fat) {
		console::write("Cluster pointer in data out of bounds (0x{:08X}).\n", true, p);
		return 0;
	}
	return fatx_context::get()->par.root_start + (p - 1) * fatx_context::get()->par.clus_size;
}
inline clusptr						ptr2cls(streamptr p) {
	return ((p - fatx_context::get()->par.root_start) >> fatx_context::get()->par.clus_pow) + 1;
}
inline streamptr					cls2fat(clusptr p) {
	if(p < fatx_context::get()->par.root_clus || p > fatx_context::get()->par.clus_fat) {
		console::write("Cluster pointer in FAT out of bounds (0x{:08X}).\n", true, p);
		return 0;
	}
	return fatx_context::get()->par.fat_start + p * fatx_context::get()->par.chain_size;
}
inline std::string					clsprint(clusptr p, clusptr r) {
	return (p == r + 1) ? "next" : ((p == FLK) ? "free" : ((p == EOC) ? "end" : std::format("0x{:08X}", p)));
}

}
