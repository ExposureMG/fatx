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

#include "types.hpp"
#include "device.hpp"
#include "frontend.hpp"
#include "partition.hpp"
#include "diskmap.hpp"
#include "entry.hpp"
#include "fuse_ops.hpp"

#include <cassert>

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

inline fatx_context*                require_context();

inline clusptr						siz2cls(filesize);
inline streamptr					cls2ptr(clusptr );
inline clusptr						ptr2cls(streamptr);
inline streamptr					cls2fat(clusptr);
inline std::string					clsprint(clusptr, clusptr);

inline fatx_context*                require_context() {
	auto* ctx = fatx_context::get();
	assert(ctx != nullptr);
	return ctx;
}

inline clusptr						siz2cls(filesize s) {
	auto* const ctx = require_context();
	return
		(s >> ctx->par.clus_pow) +
		(s % ctx->par.clus_size != 0 ? 1 : 0)
	;
}
inline streamptr					cls2ptr(clusptr p) {
	auto* const ctx = require_context();
	if (p < ctx->par.root_clus || p > ctx->par.clus_fat) {
		console::write("Cluster pointer in data out of bounds (0x{:08X}).\n", true, p);
		return 0;
	}
	return ctx->par.root_start + (p - 1) * ctx->par.clus_size;
}
inline clusptr						ptr2cls(streamptr p) {
	auto* const ctx = require_context();
	return ((p - ctx->par.root_start) >> ctx->par.clus_pow) + 1;
}
inline streamptr					cls2fat(clusptr p) {
	auto* const ctx = require_context();
	if(p < ctx->par.root_clus || p > ctx->par.clus_fat) {
		console::write("Cluster pointer in FAT out of bounds (0x{:08X}).\n", true, p);
		return 0;
	}
	return ctx->par.fat_start + p * ctx->par.chain_size;
}
inline std::string					clsprint(clusptr p, clusptr r) {
	return (p == r + 1) ? "next" : ((p == FLK) ? "free" : ((p == EOC) ? "end" : std::format("0x{:08X}", p)));
}

}
