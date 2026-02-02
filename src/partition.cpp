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

#include "fatx.hpp"

#include <bitset>

// Implémentation des méthodes de la classe partition

							partition::		partition() :
	par_id(0),				par_label(),			par_start(0),			par_size(0),			clus_size(0),			clus_pow(0),
	clus_num(0),			clus_fat(0),			chain_size(0),			chain_pow(0),			fat_start(0),			fat_size(0),
	root_start(0),			root_clus(0) {
}

int							partition::		setup() {
	uint64_t ts = fatx_context::get()->dev.size();
	bool found = false;
	if(fatx_context::get()->mmi.verbose)
		console::write("Support size: {}.\n", ts);
	std::map<const std::string, std::map<const std::string, const std::pair<streamptr, streamptr>>> sch = {
		{ "file", {
			{ "x2",		{	0x0,			0x0			}}
		}},
		{ "mu", {
			{ "sc",		{	0x0,			0x7FF000	}},
			{ "x2",		{	0x7FF000,		0x0			}}
		}},
		{ "usb", {
			{ "sc",		{	0x8000400,		0x12000400	}},
			{ "se1",	{	0x8115200,		0x8000000	}},
			{ "se2",	{	0x12000400,		0xDFFFC00	}},
			{ "x2",		{	0x20000000,		0x0			}}
		}},
		{ "hd", {
			{ "sc",		{	0x80000,		0x80000000	}},
			{ "gc",		{	0x80080000,		0xA0E30000	}},
			{ "se1",	{	0x10C080000,	0xCE30000	}},
			{ "se2",	{	0x118EB0000,	0x8000000	}},
			{ "x1",		{	0x120eb0000,	0x10000000	}},
			{ "x2",		{	0x130eb0000,	0x0			}}
		}}
	};
	std::map<const std::string, const std::string> names = {
		{ "file",	"plain file" },
		{ "mu",		"memory unit" },
		{ "usb",	"USB drive" },
		{ "hd",		"Xbox 360 retail hard disk" },
		{ "kit",	"devkit hard disk" },
		{ "sc",		"system cache" },
		{ "gc",		"game cache" },
		{ "se1",	"sysext" },
		{ "se2",	"sysext 2" },
		{ "xdv",	"Xbox 360 dashboard"},
		{ "x1",		"original Xbox compatibility" },
		{ "x2",		"data" }
	};
	std::map<const std::string, std::set<std::string>> cap;
	for(const auto &i: sch) {
		if(ts <= std::ranges::find_if(i.second, [] (const auto j) noexcept { return j.second.second == 0; })->second.first)
			continue;
		cap.emplace(i.first, std::set<std::string>{});
		for(const auto &j: i.second) {
			if(fatx_context::get()->dev.read(j.second.first).find(fsid, 0) == 0) {
				cap[i.first].insert(j.first);
				if(fatx_context::get()->mmi.verbose)
					console::write("Found FATX filesystem in {} partition in {} table.\n",
						names[j.first],
						names[i.first]
					);
			}
		}
	}
	if(cap.find("mu") != cap.end() && cap.find("mu")->second.size() == sch["mu"].size() && cap.find("file") != cap.end())
		cap.erase("file");
	#ifndef NDEBUG
		dbglog("CAPACITY:");
		for(const auto &i: cap) {
			dbglog("  " + i.first);
			for(const auto &j: i.second)
				dbglog("    " + j);
		}
	#endif
	if(
		fatx_context::get()->mmi.offset != 0 &&
		(fatx_context::get()->mmi.prog == frontend::mkfs || fatx_context::get()->dev.read(fatx_context::get()->mmi.offset).find(fsid, 0) == 0)
	) {
		par_start = fatx_context::get()->mmi.offset;
		par_size = ts - par_start;
		if(fatx_context::get()->mmi.verbose && fatx_context::get()->dev.read(fatx_context::get()->mmi.offset).find(fsid, 0) == 0)
			console::write("Found FATX partition at 0x{:016X}.\n", par_start);
		found = fatx_context::get()->dev.read(fatx_context::get()->mmi.offset).find(fsid, 0) == 0;
	}
	else if(
		cap.find(fatx_context::get()->mmi.table) != cap.end() && (
			fatx_context::get()->mmi.prog == frontend::mkfs ||
			cap[fatx_context::get()->mmi.table].find(fatx_context::get()->mmi.partition) != cap[fatx_context::get()->mmi.table].end()
		)
	) {
		par_start = sch[fatx_context::get()->mmi.table][fatx_context::get()->mmi.partition].first;
		par_size = sch[fatx_context::get()->mmi.table][fatx_context::get()->mmi.partition].second != 0 ? sch[fatx_context::get()->mmi.table][fatx_context::get()->mmi.partition].second : ts - par_start;
		found = cap[fatx_context::get()->mmi.table].find(fatx_context::get()->mmi.partition) != cap[fatx_context::get()->mmi.table].end();
	}
	else if(fatx_context::get()->mmi.table == "kit") {
		devheader dh(fatx_context::get()->dev.read(0).data());
		if(dh.id != 0x00020000 && fatx_context::get()->mmi.prog == frontend::mkfs)
			dh = devheader(ts);
		if(fatx_context::get()->dev.read(dh.p2_start * blksize).find(fsid, 0) == 0) {
			if(fatx_context::get()->mmi.verbose)
				console::write("Found FATX filesystem in {} partition in {} table.\n",
					names["x2"],
					names["kit"]
				);
			if(fatx_context::get()->mmi.partition == "x2") {
				par_start	= dh.p2_start * blksize;
				par_size	= dh.p2_size * blksize;
				found = true;
			}
		}
		if(fatx_context::get()->dev.read(dh.p1_start * blksize).find(fsid, 0) == 0) {
			if(fatx_context::get()->mmi.verbose)
				console::write("Found FATX filesystem in {} partition in {} table.\n",
					names["xdv"],
					names["kit"]
				);
			if(fatx_context::get()->mmi.partition == "xdv") {
				par_start	= dh.p1_start * blksize;
				par_size	= dh.p1_size * blksize;
				found = true;
			}
		}
	}
	else {
		if(fatx_context::get()->mmi.prog == frontend::mkfs) {
			console::write("No space for partition in {} table.\n", true, names[fatx_context::get()->mmi.table]);
			return ENOSPC;
		}
		else {
			console::write("No FATX partition found.\n", true);
			return ENODATA;
		}
	}
	if(fatx_context::get()->mmi.size != 0) {
		if(fatx_context::get()->mmi.offset == 0 && fatx_context::get()->mmi.table != "file")
			console::write("Can't force size for a partition in {} table. Ignoring.\n", true, names[fatx_context::get()->mmi.partition]);
		else if(fatx_context::get()->mmi.size >= par_size)
			console::write("Can't force a greater size than possible (limit is {}). Ignoring.\n", true, par_size);
		else
			par_size = fatx_context::get()->mmi.size;
	}
	if(found) {
		if(fatx_context::get()->mmi.verbose)
			console::write("Using {} partition in {} table.\n", names[fatx_context::get()->mmi.partition], names[fatx_context::get()->mmi.table]);
		bootsect	bs(fatx_context::get()->dev.read(par_start).data());
		par_id		= bs.id;
		root_clus	= bs.root;
		clus_size	= static_cast<uint32_t>(blksize * (fatx_context::get()->mmi.clus_size ? fatx_context::get()->mmi.clus_size : (bs.spc == 0 || bs.spc > 0xFFFF) ? 1 : bs.spc));
	}
	else {
		par_id		= 0;
		root_clus	= 1;
		clus_size	= static_cast<uint32_t>(blksize * (fatx_context::get()->mmi.clus_size ? fatx_context::get()->mmi.clus_size : (
			par_size > 0x200000000ULL ?	512	:
			par_size > 0x100000000ULL ?	256	:
			par_size > 0x080000000ULL ?	128	:
			par_size > 0x040000000ULL ?	64	:
			par_size > 0x020000000ULL ?	32	:
			par_size > 0x010000000ULL ?	16	:
			par_size > 0x008000000ULL ?	8	:
			par_size > 0x001000000ULL ?	4	:
			par_size > 0x000800000ULL ?	8	:
			par_size > 0x000400000ULL ?	4	:
			par_size > 0x000200000ULL ?	2	:
										1
		)));
	}
	std::string s(std::bitset<64>(clus_size).to_string());
	if(s.find('1') != s.rfind('1')) {
		console::write("Size of clusters is not a power of 2.\n", true);
		return EINVAL;
	}
	clus_pow	= static_cast<unsigned short>(63 - s.find('1'));
	clus_num	= static_cast<uint32_t>(par_size >> clus_pow);
	chain_size	= clus_num < 0xFFF0 ? 2 : 4;
	chain_pow	= chain_size == 2 ? 1 : 2;
	fat_start	= par_start + 0x1000;
	fat_size	= clus_num * chain_size;
	fat_size	+= (0x1000 - (fat_size % 0x1000));
	root_start	= fat_start + fat_size;
	clus_fat	= static_cast<uint32_t>(((par_size - (root_start - par_start)) >> clus_pow) - 1);
	if(root_clus < 1 || root_clus > clus_fat) {
		root_clus = 1;
		if(fatx_context::get()->mmi.prog != frontend::mkfs) {
			console::write("Bad root cluster number.", fatx_context::get()->mmi.dialog);
			if(fatx_context::get()->mmi.prog == frontend::fsck) {
				console::write(" Correct it ?", fatx_context::get()->mmi.dialog);
				if(fatx_context::get()->mmi.getanswer(true)) {
					int res;
					if((res = write()) != 0)
						return res;
				}
			}
			else
				console::write("\n", fatx_context::get()->mmi.dialog);
		}
	}
	#ifndef NDEBUG
		dbglog("PAR size  : {}", par_size);
		dbglog("CLS size  : {} ({})", clus_size, clus_pow);
		dbglog("CLS num   : {}", clus_num);
		dbglog("FAT start : 0x{:016X}", fat_start);
		dbglog("FAT cls   : {}", clus_fat);
		dbglog("ROOT start: 0x{:016X} (0x{:08X})", root_start, root_clus);
	#endif
	#if !defined NDEBUG && defined DBG_INIT
		dbglog("::EOPAR");
	#endif
	return 0;
}
int							partition::		write() {
	if(fatx_context::get()->mmi.table == "kit") {
		int res;
		std::string buf(blksize, '\0');
		devheader(fatx_context::get()->dev.size()).write(&buf[0]);
		if((res = fatx_context::get()->dev.write(0, buf)))
			return res;
	}
	std::string buf(blksize, '\0');
	bootsect(par_id, static_cast<uint32_t>(clus_size / blksize), static_cast<uint32_t>(root_clus)).write(&buf[0]);
	return fatx_context::get()->dev.write(par_start, buf);
}
size_t						partition::		label(unsigned char buf[slab]) const {
	size_t res = 2;
	memset(buf, 0, slab);
	buf[0] = 0xFE;
	buf[1] = 0xFF;
	for(size_t i = 0; i < par_label.size(); i++) {
		if(res == slab)
			break;
		res += 2;
		buf[res - 1] = static_cast<unsigned char>(par_label[i]);
	}
	return res;
}
void						partition::		label(const unsigned char buf[slab], const size_t size) {
	for(size_t i = 3; i < slab; i += 2) {
		if(i >= size)
			break;
		par_label += static_cast<char>(buf[i]);
	}
}
