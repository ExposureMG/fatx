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

// Implémentation des méthodes de la classe vareas

clusptr						vareas::		first() const {
	return empty() ? 0 : begin()->start;
}
clusptr						vareas::		last() const {
	return empty() ? 0 : rbegin()->stop;
}
size_t						vareas::		nbcls(clusptr c) const {
	size_t res = 0;
	for(const area& i: *this) {
		res += (c >= i.start && c <= i.stop) ? c - i.start + 1 : i.stop - i.start + 1;
		if(c >= i.start && c <= i.stop)
			break;
	}
	return res;
}
clusptr						vareas::		at(size_t s) const {
	if(s == 0)
		return last();
	for(const area& i: *this) {
		if(s <= i.stop - i.start + 1)
			return i.start + s - 1;
		else
			s -= i.stop - i.start + 1;
	}
	return 0;
}
vareas::iterator			vareas::		in(size_t s) {
	if(s == 0)
		return end() - 1;
	for(vareas::iterator i = begin(); i != end(); ++i) {
		if(s <= i->stop - i->start + 1)
			return i;
		else
			s -= i->stop - i->start + 1;
	}
	return end();
}
vareas						vareas::		sub(filesize s, filesize o) const {
	vareas res(*this);
	res.erase(remove_if(
		res.begin(), res.end(),
		[s, o] (const area& a) -> bool {
			return a.offset > o + s - 1 || a.offset + a.size - 1 < o;
		}
	), res.end());
	#if !defined NDEBUG && defined DBG_AREAS
		if(res.empty())
			dbglog("NO SUBAREA.");
	#endif
	for(area& i: res) {
		filesize ns = i.size;
		filesize no = i.offset;
		if(o > i.offset && o < i.offset + i.size - 1) {
			no			= o;
			i.pointer	+= o - i.offset;
			ns			-= o - i.offset;
			i.start		+= (o - i.offset) >> fatx_context::get()->par.clus_pow;
		}
		if(o + s > i.offset && o + s < i.offset + i.size) {
			ns			-= i.offset + i.size - o - s;
			i.stop		-= (i.offset + i.size - o - s) >> fatx_context::get()->par.clus_pow;
		}
		i.offset = no;
		i.size = ns;
	}
	#if !defined NDEBUG && defined DBG_AREAS
		if(o > nbcls() << fatx_context::get()->par.clus_pow)
			dbglog("AREAS: bad parameters, offset:0x{:016X} size:{}", o, s);
		else
			dbglog("Sub" + res.print());
	#endif
	return o > nbcls() << fatx_context::get()->par.clus_pow ? vareas() : res;
}
void						vareas::		add(vareas va) {
	if(va.empty())
		return;
	if(!empty()) {
		if(va.first() == last() + 1) {
			(end() - 1)->size += va.begin()->size;
			(end() - 1)->stop = va.begin()->stop;
			va.erase(va.begin());
		}
		streamptr o = (end() - 1)->offset + (end() - 1)->size;
		for(area& i: va) {
			i.offset = o;
			o += i.size;
		}
	}
	insert(end(), va.begin(), va.end());
}
void						vareas::		add(clusptr c) {
	if(empty())
		return;
	if(c + 1 == first()) {
		begin()->size++;
		begin()->start = c;
		begin()->pointer = clsarithm::cls2ptr(c);
	}
	else {
		insert(begin(), area(0, clsarithm::cls2ptr(c), 1, c, c));
	}
	for(vareas::iterator i = begin() + 1; i != end(); ++i) {
		i->pointer += fatx_context::get()->par.clus_size;
	}
}
#if !defined NDEBUG && defined DBG_AREAS
string						vareas::		print() const {
	string sink = std::format("Area: {} subarea{} with {} cluster{} [0x{:08X}-0x{:08X}]\n",
		size(), (size() > 1 ? "s" : ""), nbcls(), (nbcls() > 1 ? "s" : ""), first(), last()
	);
	for(const area& i: *this)
		sink += std::format(" [0x{:08X}-0x{:08X}] off:0x{:016X} ptr:0x{:016X} len:{}\n",
			i.start, i.stop, i.offset, i.pointer, i.size
		);
	return sink;
}
#endif

// Implémentation des méthodes de la classe buffer

							buffer::		buffer(const streamptr o, const streamptr s) : touched(false), offset(o) {
	if(s == 0) {
		#if !defined NDEBUG && defined DBG_BUFFER
			dbglog("... buffer: empty allocation try");
		#endif
		return;
	}
	size_t siz = std::min<streamptr>(static_cast<streamptr>(max_buf), s);
	while(true) {
		resize(siz);
		if(size() >= siz)
			break;
		siz /= 2;
		if(siz < fatx_context::get()->par.clus_size) {
			clear();
			#if !defined NDEBUG && defined DBG_BUFFER
				dbglog("... buffer: no way to alloc 0x{:08X} 0x{:016X} {}", data(), offset, siz);
			#endif
			return;
		}
	}
	#if !defined NDEBUG && defined DBG_BUFFER
		dbglog(".oO buffer: 0x{:08X} 0x{:016X} {}", this, offset, (*this).size());
	#endif
}
							buffer::		~buffer() {
	#if !defined NDEBUG && defined DBG_BUFFER
		dbglog("Oo. buffer: 0x{:08X} 0x{:016X} {}", this, offset, (*this).size());
	#endif
}
void						buffer::		enlarge(const streamptr s) {
	if(s <= size() || s > max_buf)
		return;
	resize(s);
	if(size() < s) {
		clear();
		#if !defined NDEBUG && defined DBG_BUFFER
			dbglog("... buffer: no way to alloc 0x{:08X} 0x{:016X} {}", data(), offset, s);
		#endif
		return;
	}
	#if !defined NDEBUG && defined DBG_BUFFER
		dbglog("OOO buffer: 0x{:08X} 0x{:016X} {}"), this, offset, (*this).size());
	#endif
}
#if !defined NDEBUG && defined DBGBUFDMP
void						buffer::		operator () (size_t p) {
	std::string res;
	for(
		size_t i = p;
		i <
			#ifdef DBGBUFDMP
				p + std::min<std::string::size_type>(std::string::size_type(DBGBUFDMP), size());
			#else
				p + size();
			#endif
		i++
	) {
		res += std::format(" {:02X}", static_cast<unsigned int>(static_cast<unsigned char>((*this)[i])));
		if(((i + 1 - p) % DBGCR) == 0) {
			dbglog(res + "\n");
			res.clear();
		}
	}
	dbglog(res + "\n");
}
#endif

// Implémentation des méthodes de la classe mymutx

void						mymutx::		lock() {
	if(fatx_context::get()->mmi.prog == frontend::fuse && fatx_context::get()->ready) {
		#if !defined NDEBUG && defined DBG_SEM
			int nb = cpt++;
			#ifdef DBGSEM
				if(nam == DBGSEM)
			#endif
			dbglog(">>> Lock request [eXclu:{} {}]", nb, nam);
		#endif
		shared_mutex::lock();
		#if !defined NDEBUG && defined DBG_SEM
			#ifdef DBGSEM
				if(nam == DBGSEM)
			#endif
			dbglog("	Lock done [eXclu:{} {}].", nb, nam);
		#endif
	}
}
void						mymutx::		unlock() {
	if(fatx_context::get()->mmi.prog == frontend::fuse && fatx_context::get()->ready) {
		#if !defined NDEBUG && defined DBG_SEM
			int nb = --cpt;
			#ifdef DBGSEM
				if(nam == DBGSEM)
			#endif
			dbglog("<<< Lock release [eXclu:{} {}]", nb, nam);
		#endif
		shared_mutex::unlock();
		#if !defined NDEBUG && defined DBG_SEM
			#ifdef DBGSEM
				if(nam == DBGSEM)
			#endif
			dbglog("	Lock done [eXclu:{} {}].", nb, nam);
		#endif
	}
}
void						mymutx::		lock_shared() {
	if(fatx_context::get()->mmi.prog == frontend::fuse && fatx_context::get()->ready) {
		#if !defined NDEBUG && defined DBG_SEM
			int nb = cpt++;
			#ifdef DBGSEM
				if(nam == DBGSEM)
			#endif
			dbglog(">>> Lock request [Share:{} {}]", nb, nam);
		#endif
		shared_mutex::lock_shared();
		#if !defined NDEBUG && defined DBG_SEM
			#ifdef DBGSEM
				if(nam == DBGSEM)
			#endif
			dbglog("	Lock done [Share:{} {}].", nb, nam);
		#endif
	}
}
void						mymutx::		unlock_shared() {
	if(fatx_context::get()->mmi.prog == frontend::fuse && fatx_context::get()->ready) {
		#if !defined NDEBUG && defined DBG_SEM
			int nb = --cpt;
			#ifdef DBGSEM
				if(nam == DBGSEM)
			#endif
			dbglog("<<< Lock release [Share:{} {}]", nb, nam);
		#endif
		shared_mutex::unlock_shared();
		#if !defined NDEBUG && defined DBG_SEM
			#ifdef DBGSEM
				if(nam == DBGSEM)
			#endif
			dbglog("	Lock done [Share:{} {}].", nb, nam);
		#endif
	}
}

// Implémentation des méthodes de la classe console

namespace {
console::sink_t					global_sink;					// process-wide sink (empty: stdout/stderr)
thread_local const console::sink_t*	thread_sink = nullptr;		// per-thread sink (console::scoped_sink)
}

							console::scoped_sink::	scoped_sink(const sink_t &s) : previous(thread_sink) {
	thread_sink = &s;
}
							console::scoped_sink::	~scoped_sink() {
	thread_sink = previous;
}
void						console::		set_sink(sink_t s) {
	global_sink = std::move(s);
}
void						console::		emit(level l, const std::string &s) {
	if(thread_sink != nullptr && *thread_sink)
		(*thread_sink)(l, s);
	else if(global_sink)
		global_sink(l, s);
	else
		(l == level::info ? std::cout : std::cerr) << s;
}
std::pair<bool, bool>		console::		read() {
	char c, d;
	c = d = static_cast<char>(std::cin.get());
	while(d != '\n' && std::cin)
		d = static_cast<char>(std::cin.get());
	return ((c == 'y') || (c == 'Y')) ? std::make_pair(true, true) : ((c == 'n') || (c == 'N')) ? std::make_pair(true, false) : std::make_pair(false, false);
}

// Implémentation de la classe nameval

const std::unordered_set<char> nameval::	fchar = {
    '/', '\\', ':', '*', '?', '"', '<', '>', '|', '\0', ' ', '\x7F',
    // Caractères de contrôle ASCII (0x00-0x1F)
    '\x01', '\x02', '\x03', '\x04', '\x05', '\x06', '\a', '\b',
    '\t', '\n', '\v', '\f', '\r', '\x0E', '\x0F',
    '\x10', '\x11', '\x12', '\x13', '\x14', '\x15', '\x16', '\x17',
    '\x18', '\x19', '\x1A', '\x1B', '\x1C', '\x1D', '\x1E', '\x1F'
};
int 						nameval::		is_valid(std::string name) {
    if(name.length() == 0)
		return -EINVAL;
    if(name.length() > name_size)
		return -ENAMETOOLONG;
    return std::ranges::all_of(
		name,
        [] (char c) { return fchar.find(c) == fchar.end(); }
	) ? 0 : -EINVAL;
}
