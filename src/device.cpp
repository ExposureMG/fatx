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

// Implémentation des méthodes de la classe device

bool					device::chgfile::	load(std::fstream& iod) {
	streamptr o = 0;
	streamptr p = 0;
	streamptr s = 0;
	iod.clear();
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
bool					device::chgfile::	read(std::fstream& iod, streamptr p, const size_t s, std::string &buf) {
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
	iod.clear();
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
			.fail()
		) {
			console::write("Can't read diffile at 0x{:016X}.\n", true, p);
			iod.clear();
			return true;
		}
		c += siz;
	}
	return false;
}
bool					device::chgfile::	write(std::fstream& iod, streamptr p, const std::string &buf) {
	#if !defined NDEBUG && defined DBG_DIFF
		dbglog("=>  Write diffile at 0x{:016X}({})", p, buf.size());
	#endif
	iod.clear();
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
			if(addseg(iod, l, buf.substr(l - p)))
				return true;
			break;
		}
		if(b->first > l) {
			if(addseg(iod, l, buf.substr(l - p, b->first - l)))
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
		if(iod
			.seekp(static_cast<std::basic_istream<char>::off_type>(pos))
			.write(&buf[l - p], static_cast<std::streamsize>(siz))
			.fail()
		) {
			console::write("Can't write diffile at 0x{:016X}.\n", true, p);
			iod.clear();
			return true;
		}
		l += siz;
	}
	return false;
}
bool					device::chgfile::	addseg(std::fstream& iod, streamptr p, const std::string& buf) {
	streamptr s = buf.size();
	device::segment seg;
	iod.clear();
	seg.size = s;
	seg.position = static_cast<streamptr>(iod.seekg(0, std::ios::end).tellg()) + sizeof(streamptr) + sizeof(filesize);
	bool status = false;
	status = status || iod
		.seekp(0, std::ios::end)
		.write(reinterpret_cast<char*>(&p), sizeof(p))
		.write(reinterpret_cast<char*>(&s), sizeof(s))
		.write(&buf[0], static_cast<std::streamsize>(s))
		.fail()
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
	tot_size(0), changes(false), authd("DEV"), chgf() {
}
						device::			~device() {
	if(io.is_open())
		io.close();
	if(iod.is_open())
		iod.close();
	for(auto &i: usbd)
		i.second.close();
	usbd.clear();
}
int						device::			setup() {
	bool err = false;
	if(fatx_context::get()->mmi.backend) {
		backend = fatx_context::get()->mmi.backend;
		tot_size = backend->size();
		err = fatx_context::get()->mmi.writeable() && fatx_context::get()->mmi.diffile.empty() && !backend->writable();
	}
	else if(fatx_context::get()->mmi.table != "usb") {
		err = err || (io = std::fstream(fatx_context::get()->mmi.input, std::ios::binary | (
			fatx_context::get()->mmi.writeable() && fatx_context::get()->mmi.diffile.empty() ? (std::ios::out | std::ios::in) : std::ios::in
		)))
			.seekg(0)
			.seekg(0, std::ios::end)
			.fail()
		;
		tot_size = err ? 0 : static_cast<streamptr>(io.tellg());
		if(fatx_context::get()->mmi.writeable() && fatx_context::get()->mmi.diffile.empty()) {
			err = err || io
				.seekp(0)
				.seekp(0, std::ios::end)
				.fail()
			;
		}
	}
	else {
		auto path = fatx_context::get()->mmi.input + sepdir;
		streamptr p = 0;
		std::fstream f;
		f.open(path + usb_data + "0000", std::ios::binary | std::ios::in);
		if(f.good())
			path = path + usb_data;
		else {
			f.close();
			f.clear();
			path = path + usb_dir + sepdir + usb_data;
			f.open(path + "0000", std::ios::binary | std::ios::in);
			err = err || f.fail();
		}
		char buf[8];
		err = err || f
			.seekg(static_cast<std::basic_istream<char>::off_type>(0x240))
			.read(&buf[0], static_cast<std::streamsize>(8))
			.fail()
		;
		streamptr dts = err ? 0 : byte_order<8>::bigend(&buf[0])();
		#if !defined NDEBUG && defined DBG_INIT
			dbglog("USB Data of size: {}.", dts);
		#endif
		f.close();
		size_t i = 0;
		streamptr s = 0;
		streamptr ts = 0;
		streamptr cts = 0;
		while(!err && (i <= 1 || cts < dts)) {
			sprintf(&buf[0], "%04lu", i);
			f.open(path + buf, std::ios::binary | (
				fatx_context::get()->mmi.writeable() && fatx_context::get()->mmi.diffile.empty() ? (std::ios::out | std::ios::in) : std::ios::in
			));
			err = err || f.fail();
			err = err || f				
				.seekg(0)
				.seekg(0, std::ios::end)
				.fail()
			;
			if(fatx_context::get()->mmi.writeable() && fatx_context::get()->mmi.diffile.empty()) {
				err = err || f
					.seekp(0)
					.seekp(0, std::ios::end)
					.fail()
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
			.fail()
		;
		if(fatx_context::get()->mmi.writeable()) {
			err = err || iod
				.seekp(0)
				.seekp(0, std::ios::end)
				.fail()
			;
			iod.close();
			iod.open(fatx_context::get()->mmi.diffile, std::ios::binary | std::ios::out | std::ios::in);
			err = err || iod.fail();
		}
		err = err || chgf.load(iod);
	}
	if(err) {
		if(io.is_open())
			io.close();
		if(iod.is_open())
			iod.close();
		for(auto &i: usbd)
			i.second.close();
		usbd.clear();
		backend.reset();
		console::write("Error opening {}{} for read{}\n", true,
			fatx_context::get()->mmi.backend ? std::string("device") : fatx_context::get()->mmi.input,
			fatx_context::get()->mmi.diffile.empty() ? "" : (":" + fatx_context::get()->mmi.diffile),
			fatx_context::get()->mmi.writeable() ? "/write" : ""
		);
	}
	#if !defined NDEBUG && defined DBG_INIT
		dbglog("::EODEV");
	#endif
	return err ? EIO : 0;
}
byte_buffer             device::            read_bytes(streamptr p, size_t s) {
	byte_buffer res;
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
	res.resize(s, std::byte{0});
	bool status = false;
	authd.lock();
	if(backend)
		status = !backend->read_at(p, std::span<std::byte>(res.data(), res.size()));
	else if(usbd.empty()) {
		status = status || io
			.seekg(static_cast<std::basic_istream<char>::off_type>(p))
			.read(reinterpret_cast<char*>(res.data()), static_cast<std::streamsize>(s))
			.fail()
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
				.read(reinterpret_cast<char*>(res.data()) + c, static_cast<std::streamsize>(siz))
				.fail()
			;
			if(status) {
				i->second.clear();
				break;
			}
			c += siz;
		}
	}
	if(iod.is_open()) {
		std::string overlay(res.size(), '\0');
		for(size_t i = 0; i < res.size(); i++)
			overlay[i] = static_cast<char>(res[i]);
		status = status || chgf.read(iod, p, s, overlay);
		if(!status)
			for(size_t i = 0; i < res.size(); i++)
				res[i] = static_cast<std::byte>(static_cast<unsigned char>(overlay[i]));
	}
	if(status) {
		console::write("Unreadable block at 0x{:016X}.\n", true, p);
		authd.unlock();
		res.clear();
		return res;
	}
	#if !defined NDEBUG && defined DBG_READ
		std::string dbg(res.size(), '\0');
		for(size_t i = 0; i < res.size(); i++)
			dbg[i] = static_cast<char>(res[i]);
		devlog(true, p, dbg);
	#endif
	authd.unlock();
	return res;
}
int                     device::            write_bytes(streamptr p, byte_view bytes) {
	if(bytes.empty())
		return 0;
	if(p + bytes.size() > size()) {
		console::write("Blocks out of bounds ([0x{:016X};0x{:016X}] > 0x{:016X}).\n", true, p, p + bytes.size() - 1, size());
		return EOVERFLOW;
	}
	if(!fatx_context::get()->mmi.writeable())
		return 0;
	bool status = false;
	authd.lock();
	#ifndef NO_WRITE
		if(iod.is_open()) {
			std::string raw(bytes.size(), '\0');
			for(size_t i = 0; i < bytes.size(); i++)
				raw[i] = static_cast<char>(bytes[i]);
			status = status || chgf.write(iod, p, raw);
		}
		else if(backend)
			status = !backend->write_at(p, bytes);
		else if(usbd.empty()) {
			status = status || io
				.seekp(static_cast<std::basic_istream<char>::off_type>(p))
				.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()))
				.fail()
			;
			if(status)
				io.clear();
		}
		else {
			auto b = usbd.lower_bound(p);
			if(b != usbd.begin() && b->first > p)
				b--;
			auto e = usbd.lower_bound(p + bytes.size());
			streamptr c = 0;
			for(auto i = b; i != e; i++) {
				auto pos = p > i->first ? p - i->first : 0;
				auto siz = std::min<filesize>(bytes.size() - c, (next(i, 1) == usbd.end() ? size() : next(i, 1)->first) - i->first - (p > i->first ? pos : 0));
				status = status || i->second
					.seekp(static_cast<std::basic_ostream<char>::off_type>(pos))
					.write(reinterpret_cast<const char*>(bytes.data()) + c, static_cast<std::streamsize>(siz))
					.fail()
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
	#if !defined NDEBUG && defined DBG_WRITE
		std::string dbg(bytes.size(), '\0');
		for(size_t i = 0; i < bytes.size(); i++)
			dbg[i] = static_cast<char>(bytes[i]);
		devlog(false, p, dbg);
	#endif
	authd.unlock();
	return 0;
}
int						device::			sync() {
	bool status = false;
	authd.lock();
	if(backend)
		status = !backend->flush();
	if(io.is_open())
		status = io.flush().fail() || status;
	if(iod.is_open())
		status = iod.flush().fail() || status;
	for(auto &i: usbd)
		status = i.second.flush().fail() || status;
	authd.unlock();
	return status ? EIO : 0;
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
	for(const std::byte c: read_bytes(p, s)) {
		const char rc = static_cast<char>(c);
		buf += rc;
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
