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

#include "context.hpp"

// Implémentation des méthodes de la classe dskmap

							dskmap::		dskmap(const partition& par) :
	memnext(
		bind(&dskmap::real_read, this, std::placeholders::_1, std::placeholders::_2),
		bind(&dskmap::real_write, this, std::placeholders::_1, std::placeholders::_2),
		#define CACHESIZE (par.clus_fat * par.chain_size / max_cache_div > par.clus_size ? par.clus_fat * par.chain_size / max_cache_div : par.clus_size)
		CACHESIZE,
		(CACHESIZE / nb_cache_div > par.clus_size ? CACHESIZE / nb_cache_div : par.clus_size)
		#undef CACHESIZE
	), authm("FAT"), scanned(false) { }
							dskmap::		~dskmap() {
	memnext.clear();
}
void						dskmap::		forfat(const lbdfat_t &lbd) {
	clusptr c = fatx_context::get()->par.root_clus;
	for(
		streamptr p = fatx_context::get()->par.fat_start;
		p < fatx_context::get()->par.fat_start + fatx_context::get()->par.fat_size;
		p += fatx_context::get()->par.clus_size
	) {
		byte_buffer buf = fatx_context::get()->dev.read_bytes(p, fatx_context::get()->par.clus_size);
		if(buf.size() != fatx_context::get()->par.clus_size)
			break;		// unreadable FAT (the error is reported by device)
		for(
			uint16_t i = static_cast<uint16_t>(p == fatx_context::get()->par.fat_start ? fatx_context::get()->par.root_clus : 0);
			i < (fatx_context::get()->par.clus_size >> fatx_context::get()->par.chain_pow) && c < fatx_context::get()->par.clus_fat;
			i++, c++
		)
			lbd(c,
				(fatx_context::get()->par.chain_size == 4) ?
				byte_order<4>::bigend(reinterpret_cast<const char*>(&buf[static_cast<byte_buffer::size_type>(i * fatx_context::get()->par.chain_size)]))() :
				byte_order<2>::bigend(reinterpret_cast<const char*>(&buf[static_cast<byte_buffer::size_type>(i * fatx_context::get()->par.chain_size)]))()
			);
	}
}
dskmap::memnext_t::lkval_t  dskmap::		real_read(clusptr p, size_t s) {
	memnext_t::lkval_t res;
	s = std::min<size_t>(s, fatx_context::get()->par.clus_fat - p);
	byte_buffer buf = fatx_context::get()->dev.read_bytes(clsarithm::cls2fat(p), fatx_context::get()->par.chain_size * s);
	for(size_t i = 0; i < buf.size(); i += fatx_context::get()->par.chain_size) {
		clusptr a = (fatx_context::get()->par.chain_size == 4) ?
			byte_order<4>::bigend(reinterpret_cast<const char*>(&buf[i]))() :
			byte_order<2>::bigend(reinterpret_cast<const char*>(&buf[i]))();
		if(fatx_context::get()->par.chain_size == 2 && a == (EOC & 0xFFFF))
			a = EOC;
		if(a != FLK && a != EOC && a > fatx_context::get()->par.clus_fat && bad.find(p + i) == bad.end()) {
			console::write("Cluster value in FAT out of bounds (0x{:08X}) for cluster 0x{:08X}.", fatx_context::get()->mmi.dialog, a, p + i);
			bad.insert(p + i);
			if(fatx_context::get()->mmi.prog == frontend::fsck) {
				console::write(" Free it ?", fatx_context::get()->mmi.dialog);
				if(fatx_context::get()->mmi.getanswer(true)) {
					if(write((p + i), FLK))
						return res;
					a = FLK;
				}
			}
			else
				console::write("\n", fatx_context::get()->mmi.dialog);
		}
		res.push_back(memnext_t::pair_type(a, p + (i >> fatx_context::get()->par.chain_pow)));
	}
	return res;
}
int							dskmap::		real_write(clusptr p, clusptr v) {
	byte_buffer buf(static_cast<byte_buffer::size_type>(fatx_context::get()->par.chain_size), std::byte{0});
	if(fatx_context::get()->par.chain_size == 4) {
		std::string raw = byte_order<4>::bigend(static_cast<byte_order<4>::value_type>(v));
		for(size_t i = 0; i < raw.size(); i++)
			buf[i] = static_cast<std::byte>(static_cast<unsigned char>(raw[i]));
	}
	else {
		std::string raw = byte_order<2>::bigend(static_cast<byte_order<2>::value_type>(v));
		for(size_t i = 0; i < raw.size(); i++)
			buf[i] = static_cast<std::byte>(static_cast<unsigned char>(raw[i]));
	}
	return fatx_context::get()->dev.write_bytes(clsarithm::cls2fat(p), byte_view(buf.data(), buf.size()));
}
vareas						dskmap::		getareas(clusptr orig, const lbdarea_t &lbd) {
	if(lbd == nullptr)
			authm.lock_shared();
	std::set<clusptr> sc;
	vareas res;
	if(orig == EOC || orig == FLK) {
		if(lbd == nullptr)
			authm.unlock_shared();
		return res;
	}
	streamptr	area_off	= 0;
	streamptr	area_ptr	= 0;
	streamptr	area_siz	= 0;
	clusptr		area_start	= 0;
	clusptr cur_cls = orig, prv_cls = 0;
	while(true) {
		if(cur_cls != EOC && cur_cls != FLK) {
			if(sc.find(cur_cls) == sc.end())
				sc.insert(cur_cls);
			else {
				console::write("Circular reference in FAT chain starting at 0x{:08X}.", fatx_context::get()->mmi.dialog, orig);
				if(fatx_context::get()->mmi.prog == frontend::fsck) {
					console::write(" Cut it ?", fatx_context::get()->mmi.dialog);
					if(fatx_context::get()->mmi.getanswer(true)) {
						if(write(prv_cls, EOC)) {
							if(lbd == nullptr)
								authm.unlock_shared();
							return res;
						}
					}
				}
				else
					console::write(" Ignoring.\n", fatx_context::get()->mmi.dialog);
				break;
			}
			if(area_ptr == 0) {
				area_ptr = clsarithm::cls2ptr(cur_cls);
				area_start = cur_cls;
			}
			area_siz += fatx_context::get()->par.clus_size;
		}
		if((prv_cls != 0 && prv_cls != cur_cls - 1) || cur_cls == EOC || cur_cls == FLK) {
			if(cur_cls != EOC && cur_cls != FLK)
				area_siz -= fatx_context::get()->par.clus_size;
			res.push_back(area(area_off, area_ptr, area_siz, area_start, prv_cls));
			area_off += area_siz;
			if(cur_cls != EOC && cur_cls != FLK)
				area_ptr = clsarithm::cls2ptr(cur_cls);
			area_siz = fatx_context::get()->par.clus_size;
			area_start = cur_cls;
		}
		if(cur_cls == EOC || cur_cls == FLK)
			break;
		cur_cls = read(prv_cls = cur_cls);
		if(lbd)
			lbd(prv_cls, cur_cls);
	}
	#if !defined NDEBUG && defined DBG_AREAS
		dbglog((lbd ? "Write" : "Get") + res.print());
	#endif
	if(lbd == nullptr)
			authm.unlock_shared();
	return res;
}
clusptr						dskmap::		clsavail() {
	clusptr res = 0;
	if(!scanned)
		gapcheck();
	for(const auto& i: freegaps.right)
		res += i.first;
	return res;
}
void						dskmap::		erase() {
	authm.lock();
	freegaps.clear();
	byte_buffer zeros(
		(fatx_context::get()->par.clus_fat - fatx_context::get()->par.root_clus) * fatx_context::get()->par.chain_size,
		std::byte{0}
	);
	void(fatx_context::get()->dev.write_bytes(
		clsarithm::cls2fat(fatx_context::get()->par.root_clus),
		byte_view(zeros.data(), zeros.size())
	));
	memnext.clear();
	gapcheck();
	authm.unlock();
}
void						dskmap::		gapcheck() {
	if(scanned)
		return;
	authm.lock();
	#ifndef NDEBUG
		dbglog("Calculating free gaps out of {} FAT entries...", fatx_context::get()->par.clus_fat);
	#endif
	freegaps.clear();
	mapptr_t b = 0;
	mapsiz_t s = 0;
	forfat([this, &b, &s](clusptr o, clusptr v) -> void {
		if(v == FLK) {
			if(b != 0)
				s++;
			else {
				b = o;
				s = 1;
			}
		}
		else if(b != 0) {
			freegaps.insert(gap_t::value_type(b, s));
			b = 0;
		}
	});
	if(b != 0)
		freegaps.insert(gap_t::value_type(b, s));
	#if !defined NDEBUG && defined DBG_GAPS
		dbglog("Gaps:");
		printgaps();
	#endif
	scanned = true;
	authm.unlock();
}
clusptr						dskmap::		read(clusptr p) {
	if(p == FLK || p == EOC) {
		console::write("Can't read FAT at special cluster value (0x{:08X}).\n", true, p);
		return 0;
	}
	if(p > fatx_context::get()->par.clus_fat) {
		console::write("Cluster pointer to FAT out of bounds (0x{:08X}).\n", true, p);
		return 0;
	}
	return memnext(p);
}
int							dskmap::		write(clusptr p, clusptr v) {
	if (p == FLK || p == EOC) {
		console::write("Can't write FAT at special cluster value (0x{:08X}).\n", true, p);
		return EOVERFLOW;
	}
	if(p > fatx_context::get()->par.clus_fat) {
		console::write("Cluster pointer to FAT out of bounds (0x{:08X}).\n", true, p);
		return EOVERFLOW;
	}
	if(v != FLK && v != EOC && v > fatx_context::get()->par.clus_fat) {
		console::write("Cluster value to FAT out of bounds (0x{:08X}) for cluster 0x{:08X}.\n", true, v, p);
		return EOVERFLOW;
	}
	return memnext(p, v);
}
vareas						dskmap::		allocfat(clusptr s, clusptr o) {
	vareas res;
	clusptr			gap_clus = 0;
	clusptr			gap_size = 0;
	if(s == 0)
		return res;
	authm.lock();
	if(!scanned)
		gapcheck();
	if(freegaps.empty()) {
		console::write("No space left on device, disk full.\n", true);
		authm.unlock();
		return res;
	}
	if(o != 0) {
		// we first try to allocate in continuity with o
		gap_t::left_map::iterator			prev = freegaps.left.find(o);
		if(prev != freegaps.left.end() && prev->second >= s) {
			gap_clus	= prev->first;
			gap_size	= prev->second;
		}
	}
	if(gap_clus == 0) {
		// we try to allocate at end of used cluster
		gap_t::left_map::reverse_iterator	end = freegaps.left.rbegin();
		if(end != freegaps.left.rend() && end->second >= s) {
			gap_clus	= end->first;
			gap_size	= end->second;
		}
	}
	if(gap_clus == 0) {
		// we find smallest gap that fits
		gap_t::right_map::iterator			fit = freegaps.right.lower_bound(s);
		if(fit != freegaps.right.end() && fit->first == s) {
			gap_clus	= fit->second;
			gap_size	= fit->first;
		}
	}
	if(gap_clus != 0) {
		// contiguous case
		for(clusptr i = gap_clus; i < gap_clus + s; i++) {
			if(write(i, (i == gap_clus + s - 1) ? EOC : i + 1)) {
				authm.unlock();
				return res;
			}
		}
		freegaps.left.erase(gap_clus);
		if(gap_size != s)
			freegaps.insert(gap_t::value_type(gap_clus + s, gap_size - s));
		res.push_back(area(
			0,
			clsarithm::cls2ptr(gap_clus),
			s * fatx_context::get()->par.clus_size,
			gap_clus,
			gap_clus + s - 1
		));
	}
	else {
		// first, evaluate the number of free clusters
		gap_size	= 0;
		for(const auto& i: freegaps.right)
			gap_size += i.first;
		if(gap_size >= s) {
			// we take gaps in decreasing size order
			gap_t::right_map::reverse_iterator gap;
			clusptr	tot_size = s;
			clusptr old_clus = 0;
			do {
				gap = freegaps.right.rbegin();
				assert(gap != freegaps.right.rend());
				gap_clus = gap->second;
				gap_size = gap->first;
				if(old_clus != 0) {
					if(write(old_clus, gap_clus)) {
						authm.unlock();
						return res;
					}
				}
				for(clusptr i = gap_clus; i < gap_clus + std::min<clusptr>(gap_size, tot_size); i++) {
					if(write(i, (i == gap_clus + std::min<clusptr>(gap_size, tot_size) - 1) ? EOC : i + 1)) {
						authm.unlock();
						return res;
					}
				}
				res.push_back(area(
					res.empty() ? 0 : res.back().offset + res.back().size,
					clsarithm::cls2ptr(gap_clus),
					std::min<clusptr>(gap_size, tot_size) * fatx_context::get()->par.clus_size,
					gap_clus,
					gap_clus + std::min<clusptr>(gap_size, tot_size) - 1
				));
				freegaps.left.erase(gap_clus);	// by start cluster (right is by size)
				if(tot_size < gap_size) {
					// the rest of the gap stays free
					freegaps.insert(gap_t::value_type(gap_clus + tot_size, gap_size - tot_size));
					tot_size = 0;
				}
				else
					tot_size -= gap_size;
				old_clus = gap_clus;
			} while(tot_size != 0);
		}
		else {
			console::write("Not enough disk space for {} cluster allocation.\n", true, s);
			authm.unlock();
			return res;
		}
	}
	#ifndef NDEBUG
		dbglog(
			gap_clus ? (
				std::format("*** FAT alloc: {} cluster{} starting at 0x{:08X}.\n", s, (s > 1 ? "s" : ""), gap_clus)
			) : (
				std::format("*** FAT alloc: failed to allocate {} clusters.\n", s)
			)
		);
	#endif
	authm.unlock();
	return res;
}
void						dskmap::		freefat(clusptr o) {
	if(o == FLK || o == EOC)
		return;
	authm.lock();
	if(!scanned)
		gapcheck();
	vareas va = getareas(o, [this](clusptr c, clusptr) -> void { void(write(c, FLK)); });
	if(fatx_context::get()->mmi.prog == frontend::fsck) {
		authm.unlock();
		return;
	}
	for(area i: va) {
		#ifndef NDEBUG
			dbglog("*** FAT free:  {} cluster{} starting at 0x{:08X}.", i.stop - i.start + 1, i.stop - i.start > 0 ? "s" : "", i.start);
		#endif
		gap_t::left_map::iterator next = freegaps.left.upper_bound(i.start);
		gap_t::left_map::iterator prev = next;
		// the freed area may come before the first gap or after the last one
		const bool join_prev = next != freegaps.left.begin() && (--prev, prev->first + prev->second == i.start);
		const bool join_next = next != freegaps.left.end() && i.stop + 1 == next->first;
		if(join_prev && join_next) {
			// new gap is adjascent with previous and next gap
			clusptr prev_clus = prev->first;
			clusptr prev_size = prev->second;
			clusptr next_size = next->second;
			freegaps.left.erase(prev);
			freegaps.left.erase(next);
			freegaps.insert(gap_t::value_type(prev_clus, prev_size + i.stop - i.start + 1 + next_size));
		}
		else if(join_prev) {
			// new gap is adjascent with a previous gap
			clusptr prev_clus = prev->first;
			clusptr prev_size = prev->second;
			freegaps.left.erase(prev);
			freegaps.insert(gap_t::value_type(prev_clus, prev_size + i.stop - i.start + 1));
		}
		else if(join_next) {
			// new gap is adjascent with a next gap
			clusptr next_size = next->second;
			freegaps.left.erase(next);
			freegaps.insert(gap_t::value_type(i.start, i.stop - i.start + 1 + next_size));
		}
		else {
			// new gap is not adjascent with another gap
			freegaps.insert(gap_t::value_type(i.start, i.stop - i.start + 1));
		}
	}
	authm.unlock();
}
int							dskmap::		resizefat(ptr_vareas o, clusptr s) {
	if(!o)
		return EFAULT;
	if(o->empty()) {
		if(s == 0)
			return 0;
		else
			return EFAULT;
	}
	if(s == 0) {
		freefat(o->first());
		return 0;
	}
	// the last area may have been cut to the size of the file: count it in
	// whole clusters, as the areas added or removed below
	o->back().size = (o->back().stop - o->back().start + 1) << fatx_context::get()->par.clus_pow;
	int res = 0;
	if(o->nbcls() < s) {
		// we need to extend the chain
		vareas &&extend = allocfat(s - o->nbcls(), o->last() + 1);
		if(extend.empty())
			return ENOSPC;
		authm.lock();
		res = write(o->last(), extend.first());
		o->add(extend);
		authm.unlock();
	}
	else if(o->nbcls() > s) {
		// we need to reduce the chain
		authm.lock();
		const clusptr cut = o->at(s + 1);
		res = write(o->at(s), EOC);
		authm.unlock();
		if(!res)
			freefat(cut);
		authm.lock();
		o->erase(o->in(s) + 1, o->end());
		o->back().size -= (o->nbcls() - s) << fatx_context::get()->par.clus_pow;
		o->back().stop = o->back().start + (o->back().size >> fatx_context::get()->par.clus_pow) - 1;
		authm.unlock();
	}
	return res;
}

// Implémentation des méthodes de la classe fatmap

							fatmap::		fatmap(const partition& par) : dskmap(par) {
}
void						fatmap::		change(clusptr, entry*, clusptr, status_t) {
}
[[gnu::const]] dskmap::status_t	fatmap::	status(clusptr) const {
	return disk;
}
[[gnu::const]] entry*			fatmap::	getentry(clusptr) const {
	return nullptr;
}
void						fatmap::		fatlost() {
}
void						fatmap::		fatcheck() {
}
std::string					dskmap::		printchain(clusptr orig) {
	std::string res;
	while(orig != FLK && orig != EOC) {
		res += std::format("->0x{:08X}", orig);
		orig = dskmap::read(orig);
	}
	return res;
}
#ifndef NDEBUG
void						dskmap::		printgaps() const {
	for(const auto& i: freegaps.left)
		dbglog("{:08X}: {} cluster{} free", i.first, i.second, i.second > 1 ? "s": "");
}
void						fatmap::		printfat() {
	for(clusptr i = fatx_context::get()->par.root_clus; i < fatx_context::get()->par.clus_fat; i++) {
		dbglog("0x{:08X} -> {}", i, clsarithm::clsprint(dskmap::read(i), i));
	}
}
#endif

// Implémentation des méthodes de la classe memmap

							memmap::		memmap(const partition& par) : dskmap(par) {
}
clusptr						memmap::		read(clusptr p) {
	return (memchain.find(p) != memchain.end()) ? memchain.find(p)->second.next : (fatx_context::get()->mmi.dellost ? dskmap::read(p) : FLK);
}
void						memmap::		change(clusptr p, entry *e, clusptr n, status_t s) {
	if(memchain.find(p) != memchain.end()) {
		if(n != FLK)
			memchain.find(p)->second.next	= n;
		memchain.find(p)->second.ent		= e;
		memchain.find(p)->second.status		= s;
	}
	else
		memchain.insert(std::make_pair(p, link(e, n, s)));
}
memmap::status_t			memmap::		status(clusptr p) const {
	return (memchain.find(p) == memchain.end()) ? disk : memchain.find(p)->second.status;
}
entry*						memmap::		getentry(clusptr p) const {
	return (memchain.find(p) == memchain.end()) ? nullptr : memchain.find(p)->second.ent;
}
void						memmap::		fatlost() {
	lost.clear();
	std::set<clusptr> l;
	forfat([this, &l](clusptr o, clusptr v) -> void {
		if(v != FLK && status(o) == disk && l.find(o) == l.end()) {
			auto f = std::find_if(lost.begin(), lost.end(), [v] (const vareas& i) -> bool { return i.first() == v; });
			if(f != lost.end()) {
				f->add(o);
				l.insert(o);
			}
			else {
				vareas va = getareas(o, [&l] (const clusptr i, const clusptr) -> void { l.insert(i); });
				lost.push_back(va);
			}
		}
	});
}
void						memmap::		fatcheck() {
	if(fatx_context::get()->mmi.prog == frontend::fsck) {
		// we propose to correct each erroneous entry in fat
		for(auto it = memchain.begin(); it != memchain.end(); ) {
			const auto& p = *it;
			if(status(p.first) == modified) {
				console::write("Cluster number in FAT 0x{:08X} shall be {} instead of {}. Correct it ?", fatx_context::get()->mmi.dialog,
					p.first,
					clsarithm::clsprint(p.second.next, p.first),
					clsarithm::clsprint(dskmap::read(p.first), p.first)
				);
				if(fatx_context::get()->mmi.getanswer(true)) {
					if(write(p.first, p.second.next))
						return;
					it = memchain.erase(it);
					continue;
				}
			}
			++it;
		}
	}
	if(fatx_context::get()->mmi.prog == frontend::fsck || fatx_context::get()->mmi.prog == frontend::unrm) {
		// we propose to rescue or to clear each chain with no reference found in FAT
		for(const vareas& va: lost) {
			if(fatx_context::get()->mmi.prog == frontend::fsck) {
				console::write("Found unknown chain at 0x{:08X} ({}). Free it ?", fatx_context::get()->mmi.dialog,
					va.first(),
					va.nbcls() * fatx_context::get()->par.clus_size
				);
				if(fatx_context::get()->mmi.getanswer(true))
					freefat(va.first());
			}
			else {
				console::write("Found unknown chain at 0x{:08X} ({}). Recover in {} ?", fatx_context::get()->mmi.dialog,
					va.first(),
					va.nbcls() * fatx_context::get()->par.clus_size,
					fatx_context::get()->mmi.lostfound
				);
				if(fatx_context::get()->mmi.getanswer(false)) {
					if(fatx_context::get()->mmi.local) {
						ptr_entry f(new entry(std::format("{}{:03d}", fatx_context::get()->mmi.foundfile, fatx_context::get()->mmi.filecount++), 0, false));
						f->cluster = va.first();
						f->size = va.nbcls() * fatx_context::get()->par.clus_size;
						f->recover();
					}
					else {
						entry *lf = fatx_context::get()->root->find(fatx_context::get()->mmi.lostfound.data());
						if(lf == nullptr) {
							// we first have to create lost+found directory
							if(fatx_context::get()->root->addtodir((lf = new entry(fatx_context::get()->mmi.lostfound, 0, true)))) {
								console::write("Unable to create directory {}.\n", fatx_context::get()->mmi.dialog, fatx_context::get()->mmi.lostfound);
								return;
							}
						}
						else {
							// we find the latest file number
							for(const ptr_entry& e: lf->childs) {
								unsigned int n;
								if(sscanf(e->name, (std::string(def_fpre) + "%3d").data(), &n) == 1)
									fatx_context::get()->mmi.filecount = std::max<unsigned int>(fatx_context::get()->mmi.filecount, n + 1);
							}
						}
						// we create the file and put it in lost+found
						entry *f = new entry(std::format("{}{:03d}", fatx_context::get()->mmi.foundfile, fatx_context::get()->mmi.filecount++), 0);
						f->size = va.nbcls() * fatx_context::get()->par.clus_size;
						f->cluster = va.first();
						if(lf->addtodir(f))
							console::write("Unable to create file {}.\n", fatx_context::get()->mmi.dialog, f->name);
					}
				}
			}
		}
	}
}
#ifndef NDEBUG
void						memmap::		printfat() {
	bool empty = false;
	status_t s = marked;
	entry *ent = fatx_context::get()->root;
	clusptr l = 1;
	for(clusptr i = fatx_context::get()->par.root_clus; i < fatx_context::get()->par.clus_fat; i++) {
		if(
			empty != ((read(i) == FLK) && (dskmap::read(i) == FLK)) ||
			(!empty && (
				(s != status(i)) ||
				(status(i) != disk && ent != getentry(i))
			)) ||
			i == fatx_context::get()->par.clus_fat
		) {
			dbglog("{} {} {}",
				(
					!empty ? (
						(s == disk) ? "LOST" :
						(s == marked) ? ((getentry(ent->cluster) == ent) ? ((ent->cluster == l) ? "OK  " : "OK->") : "*ERR") :
						(s == deleted) ? ((getentry(ent->cluster) == ent) ? ((ent->cluster == l) ? "DEL " : "DEL>") : "*BAD") :
						"?MOD"
					) : "...."
				),
				(
					(l == (i - 1)) ?
					std::format("0x{:08X}:			", l) :
					std::format("0x{:08X}-0x{:08X}: ", l, (i - 1))
				),
				(
					(!empty && s != disk) ?
					std::format("(0x{:08X}: {})", ent->cluster, ent->path()) : ""
				)
			);
			empty = (read(i) == FLK) && (dskmap::read(i) == FLK);
			s = status(i);
			ent = (status(i) == disk) ? nullptr : getentry(i);
			l = i;
		}
	}
}
#endif