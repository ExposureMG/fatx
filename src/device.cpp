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

// Implémentation des méthodes de la classe device

bool					device::chgfile::	load() {
	streamptr o = 0;
	streamptr p = 0;
	streamptr s = 0;
	auto& iod = fatx_context::get()->dev.iod;
	while(iod
		.seekg(static_cast<std::basic_istream<char>::off_type>(o))
		.read(reinterpret_cast<char*>(&p), sizeof(p))
		.good()
	) {
		if(contains(p)) {
			console::write("Duplicate segment in diffile at address 0x{:016X}.\n", true, p);
			return true;
		}
		if(!iod.read(reinterpret_cast<char*>(&s), sizeof(s)) || s == 0) {
			console::write("Can't read size in diffile at 0x{:016X}.\n", true, o);
			return true;
		}
		device::segment seg = { s, (o += 2 * sizeof(streamptr)) };
		insert(std::make_pair(p, seg));
		o += s;
	}
	#if !defined NDEBUG && defined DBG_INIT
		dbglog("Diff file loaded {}", size() == 0 ? "(0 segment)." : std::format("({} segment{})", size(), size() == 1 ? "" : "s"));
		for(auto i: *this)
			dbglog("0x{:016X} ({} at 0x{:016X})", i.first, i.second.size, i.second.position);
	#endif
	iod.clear();
	return false;
}
bool					device::chgfile::	read(streamptr p, const size_t s, std::string &buf) {
	#if !defined NDEBUG && defined DBG_DIFF
		dbglog("=>  Read diffile at 0x{:016X}({})", p, s);
	#endif
	auto b = lower_bound(p);		// *b >= p
	if(b != begin() && (b->first > p || b == end())) {
		b--;						// *b < p && *b + b->s > p
		if(b->first + b->second.size <= p)
			b++;
	}
	if(b == end())
		return false;
	auto e = lower_bound(p + s);	// *e > p + s - 1
	#if !defined NDEBUG && defined DBG_DIFF
		dbglog(
			"	match: {} - {}\n",
			std::format("0x{:016X}[0x{:016X}({})]", b->second.position, b->first, b->second.size),
			(e == end() ? "end" : std::format("0x{:016X}[0x{:016X}({})]", e->second.position, e->first, e->second.size))
		);
	#endif
	auto& iod = fatx_context::get()->dev.iod;
	streamptr c = 0;
	for(auto i = b; i != e; i++) {
		auto pos = i->second.position;
		if(i->first < p)
			pos += p - i->first;
		if(i->first > p + c)
			c = i->first - p;
		auto siz = std::min<filesize>(buf.size() - c, i->second.size - (i->first < p ? p - i->first : 0));
		#if !defined NDEBUG && defined DBG_DIFF
			dbglog("... read diffile at 0x{:016X} [0x{:016X}({})]", pos, (p + c), siz);
		#endif
		if(iod
			.seekg(static_cast<std::basic_istream<char>::off_type>(pos))
			.read(&buf[c], static_cast<std::streamsize>(siz))
			.bad()
		) {
			console::write("Can't read diffile at 0x{:016X}.\n", true, p);
			iod.clear();
			return true;
		}
		c += siz;
	}
	return false;
}
bool					device::chgfile::	write(streamptr p, const std::string &buf) {
	#if !defined NDEBUG && defined DBG_DIFF
		dbglog("=>  Write diffile at 0x{:016X}({})", p, buf.size());
	#endif
	for(auto l = p; l < p + buf.size();) {
		auto b = lower_bound(l); // *b >= l
		if(l == p) {
			if(b != begin() && (b->first > l || b == end())) {
				b--;						// *b < p && *b + b->s > p
				if(b->first + b->second.size <= l)
					b++;
			}
		}
		if(b == end()) {
			if(addseg(l, buf.substr(l - p)))
				return true;
			break;
		}
		if(b->first > l) {
			if(addseg(l, buf.substr(l - p, b->first - l)))
				return true;
			l = b->first;
			continue;
		}
		#if !defined NDEBUG && defined DBG_DIFF
			dbglog(
				"	match: " +
				(b == end() ? "end" : std::format("0x{:016X}[0x{:016X}({})]", b->second.position, b->first, b->second.size)) +
				"\n"
			);
		#endif
		auto pos = b->second.position + (b->first < l ? l - b->first : 0);
		auto siz = std::min<filesize>(buf.size() - (l - p), b->second.size - (b->first < l ? l - b->first : 0));
		assert(siz != 0);
		#if !defined NDEBUG && defined DBG_DIFF
			dbglog("... write diffile at 0x{:016X} [0x{:016X}({})]", pos, l, siz);
		#endif
		auto& iod = fatx_context::get()->dev.iod;
		if(iod
			.seekp(static_cast<std::basic_istream<char>::off_type>(pos))
			.write(&buf[l - p], static_cast<std::streamsize>(siz))
			.bad()
		) {
			console::write("Can't write diffile at 0x{:016X}.\n", true, p);
			iod.clear();
			return true;
		}
		l += siz;
	}
	return false;
}
bool					device::chgfile::	addseg(streamptr p, const std::string& buf) {
	streamptr s = buf.size();
	device::segment seg;
	auto& iod = fatx_context::get()->dev.iod;
	seg.size = s;
	seg.position = static_cast<streamptr>(iod.seekg(0, std::ios::end).tellg()) + 2 * sizeof(char*);
	bool status = false;
	status = status || iod
		.seekp(0, std::ios::end)
		.write(reinterpret_cast<char*>(&p), sizeof(p))
		.write(reinterpret_cast<char*>(&s), sizeof(s))
		.write(&buf[0], static_cast<std::streamsize>(s))
		.bad()
	;
	if(status) {
		console::write("Can't add segment in diffile at 0x{:016X}.\n", true, p);
		iod.clear();
		return true;
	}
	#if !defined NDEBUG && defined DBG_DIFF
		dbglog("... add segment at 0x{:016X} [0x{:016X}({})]", seg.position, p, s);
	#endif
	insert(std::make_pair(p, seg));
	return false;
}

						device::			device() :
	io(nullptr), iod(nullptr),
	tot_size(0), changes(false), authd("DEV"), chgf() {
}
						device::			~device() {
	if(io)
		io.close();
	if(iod)
		iod.close();
	for(auto &i: usbd)
		i.second.close();
	usbd.clear();
}
int						device::			setup() {
	bool err = false;
	if(fatx_context::get()->mmi.table != "usb") {
		err = err || (io = std::fstream(fatx_context::get()->mmi.input, std::ios::binary | (
			fatx_context::get()->mmi.writeable() && fatx_context::get()->mmi.diffile.empty() ? (std::ios::out | std::ios::in) : std::ios::in
		)))
			.seekg(0)
			.seekg(0, std::ios::end)
			.bad()
		;
		tot_size = err ? 0 : static_cast<streamptr>(io.tellg());
		if(fatx_context::get()->mmi.writeable() && fatx_context::get()->mmi.diffile.empty()) {
			err = err || io
				.seekp(0)
				.seekp(0, std::ios::end)
				.bad()
			;
		}
	}
	else {
		auto path = fatx_context::get()->mmi.input + sepdir;
		streamptr p = 0;
		std::fstream f;
		if(f = std::fstream(path + usb_data + "0000", std::ios::binary | std::ios::in), f.good())
			path = path + usb_data;
		else {
			path = path + usb_dir + sepdir + usb_data;
			f.open(path + "0000", std::ios::binary | std::ios::in);
			err = err || f.bad();
		}
		char buf[8];
		err = err || f
			.seekg(static_cast<std::basic_istream<char>::off_type>(0x240))
			.read(&buf[0], static_cast<std::streamsize>(8))
			.bad()
		;
		streamptr dts = err ? 0 : byte_order<8>::litend(&buf[0])();
		#if !defined NDEBUG && defined DBG_INIT
			dbglog("USB Data of size: {}.", dts);
		#endif
		f.close();
		size_t i = 0;
		streamptr s = 0;
		streamptr ts = 0;
		streamptr cts = 0;
		while(!err && (i <= 1 || p < dts)) {
			sprintf(&buf[0], "%04lu", i);
			f.open(path + buf, std::ios::binary | (
				fatx_context::get()->mmi.writeable() && fatx_context::get()->mmi.diffile.empty() ? (std::ios::out | std::ios::in) : std::ios::in
			));
			err = err || f.bad();
			err = err || f				
				.seekg(0)
				.seekg(0, std::ios::end)
				.bad()
			;
			if(fatx_context::get()->mmi.writeable() && fatx_context::get()->mmi.diffile.empty()) {
				err = err || f
					.seekp(0)
					.seekp(0, std::ios::end)
					.bad()
				;
			}
			if(!err) {
				s = static_cast<streamptr>(f.tellg());
				usbd.emplace(p, std::move(f));
				ts += s;
				if(i > 1)
					cts += s;
			}
			#if !defined NDEBUG && defined DBG_INIT
				dbglog("USB Data file {} at {} -> {} ({}).", path + buf, p, p + s, s);
			#endif
			p += err ? 0 : s;
			i++;
		}
		err = err || dts != cts;
		tot_size = ts;
	}
	if(!err && !fatx_context::get()->mmi.diffile.empty()) {
		#if !defined NDEBUG && defined DBG_INIT
			dbglog("Diff file requested in {} mode.", fatx_context::get()->mmi.writeable() ? "write" : "read");
		#endif
		err = err || (iod = std::fstream(fatx_context::get()->mmi.diffile, std::ios::binary | (
			fatx_context::get()->mmi.writeable() ? (std::ios::out | std::ios::in | std::ios::app) : std::ios::in
		)))
			.seekg(0)
			.seekg(0, std::ios::end)
			.bad()
		;
		if(fatx_context::get()->mmi.writeable()) {
			err = err || iod
				.seekp(0)
				.seekp(0, std::ios::end)
				.bad()
			;
			iod.close();
			iod.open(fatx_context::get()->mmi.diffile, std::ios::binary | std::ios::out | std::ios::in);
			err = err || iod.bad();
		}
		err = err || chgf.load();
	}
	if(err) {
		if(io)
			io.close();
		if(iod)
			iod.close();
		for(auto &i: usbd)
			i.second.close();
		usbd.clear();
		console::write("Error opening {}{} for read{}\n", true,
			fatx_context::get()->mmi.input,
			fatx_context::get()->mmi.diffile.empty() ? "" : (":" + fatx_context::get()->mmi.diffile),
			fatx_context::get()->mmi.writeable() ? "/write" : ""
		);
	}
	#if !defined NDEBUG && defined DBG_INIT
		dbglog("::EODEV");
	#endif
	return err ? EIO : 0;
}
std::string				device::			read(streamptr p, size_t s) {
	std::string res;
	if(s == 0)
		return res;
	if(size() && p + s > size()) {
		console::write("Blocks out of bounds ([0x{:016X} ; 0x{:016X}] > 0x{:016X}).\n", true,
			p,
			p + s - 1,
			size()
		);
		return res;
	}
	res.resize(s);
	bool status = false;
	authd.lock();
	if(usbd.empty()) {
		status = status || io
			.seekg(static_cast<std::basic_istream<char>::off_type>(p))
			.read(&res[0], static_cast<std::streamsize>(s))
			.bad()
		;
		if(status)
			io.clear();
	}
	else {
		auto b = usbd.lower_bound(p);		// *b >= p
		if(b != usbd.begin() && b->first > p)
			b--;						// *b < p && *b + b->s > p
		auto e = usbd.lower_bound(p + s);	// *e > p + s - 1
		streamptr c = 0;
		for(auto i = b; i != e; i++) {
			auto pos = p > i->first ? p - i->first : 0;
			auto siz = std::min<filesize>(res.size() - c, (next(i, 1) == usbd.end() ? size() : next(i, 1)->first) - i->first - (p > i->first ? pos : 0));
			status = status || i->second
				.seekg(static_cast<std::basic_istream<char>::off_type>(pos))
				.read(&res[c], static_cast<std::streamsize>(siz))
				.bad()
			;
			if(status) {
				i->second.clear();
				break;
			}
			c += siz;
		}
	}
	if(iod)
		status = status || chgf.read(p, s, res);
	if(status) {
		console::write("Unreadable block at 0x{:016X}.\n", true, p);
		authd.unlock();
		res.clear();  // Clear instead of returning a new string
		return res;
	} else {
		#if !defined NDEBUG && defined DBG_READ
			devlog(true, p, res);
		#endif
	}
	authd.unlock();
	return res;
}
int						device::			write(streamptr p, const std::string &s) {
	if(s.empty())
		return 0;
	if(p + s.size() > size()) {
		console::write("Blocks out of bounds ([0x{:016X};0x{:016X}] > 0x{:016X}).\n", true, p, p + s.size() - 1, size());
		return EOVERFLOW;
	}
	if(!fatx_context::get()->mmi.writeable())
		return 0;
	bool status = false;
	authd.lock();
	#ifndef NO_WRITE
		if(iod)
			status = status || chgf.write(p, s);
		else
			if(usbd.empty()) {
				status = status || io
					.seekp(static_cast<std::basic_istream<char>::off_type>(p))
					.write(&s[0], static_cast<std::streamsize>(s.size()))
					.bad()
				;
				if(status)
					io.clear();
			}
			else {
				auto b = usbd.lower_bound(p);		// *b >= p
				if(b != usbd.begin() && b->first > p)
					b--;						// *b < p && *b + b->s > p
				auto e = usbd.lower_bound(p + s.size());	// *e > p + s - 1
				streamptr c = 0;
				for(auto i = b; i != e; i++) {
					auto pos = p > i->first ? p - i->first : 0;
					auto siz = std::min<filesize>(s.size() - c, (next(i, 1) == usbd.end() ? size() : next(i, 1)->first) - i->first - (p > i->first ? pos : 0));
					status = status || i->second
						.seekp(static_cast<std::basic_ostream<char>::off_type>(pos))
						.write(&s[c], static_cast<std::streamsize>(siz))
						.bad()
					;
					if(status) {
						i->second.clear();
						break;
					}
					c += siz;
				}
			}
		changes = true;
	#endif
	if(status) {
		console::write("Unwriteable block at 0x{:016X}.\n", true, p);
		authd.unlock();
		return EIO;
	}
	else {
		#if !defined NDEBUG && defined DBG_WRITE
			devlog(false, p, s);
		#endif
	}
	authd.unlock();
	return 0;
}
#ifndef NDEBUG
std::string				device::			address(streamptr p) {
	return std::format("0x{:016X}[{}:0x{:08X}]",
		p,
		((p < fatx_context::get()->par.fat_start) ? "PAR" : (p < fatx_context::get()->par.root_start) ? "FAT" : "CLS"),
		((p < fatx_context::get()->par.fat_start) ? p : (p < fatx_context::get()->par.root_start) ? ((p - fatx_context::get()->par.fat_start) / fatx_context::get()->par.chain_size) : clsarithm::ptr2cls(p))
	);
}
void					device::			devlog(bool r, streamptr p, const std::string& s) const {
	dbglog("->  {} at {} ({}) :{}",
		r ? "read" : "write",
		address(p),
		s.size(),
		s.size() <= DBGCR ? "" : "\n"
	);
	std::string sl;
	for(
		size_t i = 0;
		i <
			#ifdef DBGLIMIT
				std::min<std::string::size_type>(std::string::size_type(DBGLIMIT), s.size());
			#else
				s.size();
			#endif
		i++
	) {
		sl += std::format(" {:02X}", static_cast<unsigned int>(static_cast<unsigned char>(s[i])));
		if(((i + 1) % DBGCR) == 0) {
			dbglog(sl + "\n");
			sl.clear();
		}
	}
	dbglog(sl + "\n");
}
std::string				device::			print(streamptr p, size_t s, size_t g) {
	std::string res;
	std::string buf;
	size_t i = 1;
	for(const char c: read(p, s)) {
		buf += c;
		if((i % g) == 0) {
			for(size_t j = 0; j < g; j++)
				res += std::format("{:02X} ", static_cast<unsigned int>(static_cast<unsigned char>(buf[j])));
			for(size_t j = 0; j < g; j++)
				res += (buf[j] >= ' ' && buf[j] <= '~') ? buf[j] : '.';
			res += '\n';
			buf.erase();
		}
		i++;
	}
	return res;
}
#endif
