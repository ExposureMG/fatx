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

#include <filesystem>

#include <boost/tokenizer.hpp>

namespace {
[[nodiscard]] std::string bytes_to_raw(const byte_buffer& bytes) {
	std::string raw(bytes.size(), '\0');
	for(size_t idx = 0; idx < bytes.size(); idx++)
		raw[idx] = static_cast<char>(bytes[idx]);
	return raw;
}
[[nodiscard]] byte_buffer raw_to_bytes(const std::string& raw) {
	byte_buffer bytes(raw.size(), std::byte{0});
	for(size_t idx = 0; idx < raw.size(); idx++)
		bytes[idx] = static_cast<std::byte>(static_cast<unsigned char>(raw[idx]));
	return bytes;
}
}

// Implémentation des méthodes de la classe entry

							// root entry constructor
							entry::			entry() :
	mux_B(std::string() + mutex_buff + "/"),
	mux_D(std::string() + mutex_data + "/"),
	mux_E(std::string() + mutex_entr + "/"),
	exclusive(1),
	entbuf(),
	opened(false),
	cptacc(0),
	writeopened(false),
	status(valid),
	namesize(0),
	flags(),
	cluster(fatx_context::get()->par.root_clus),
	size(0),
	creation(),
	access(),
	update(),
	loc(0),
	parent(this),
	childs(),
	areas() {
	std::memset(name, '\0', name_size + 1);
	flags.dir = true;
	touch();
	opendir();
	entry *elab = find(flab);
	if(elab != nullptr && elab->size <= slab) {
		creation = elab->creation;
		access = elab->access;
		update = elab->update;
		unsigned char lab[slab];
		if(!elab->data(reinterpret_cast<char*>(lab), true, 0, elab->size))
			fatx_context::get()->par.label(lab, elab->size);
	}
}
							// existing entry constructor
							entry::			entry(streamptr s, const char buf[ent_size]) :
	mux_B(),
	mux_D(),
	mux_E(),
	exclusive(1),
	entbuf(),
	opened(false),
	cptacc(0),
	writeopened(false),
	status(invalid),
	namesize(static_cast<uint8_t>(buf != nullptr ? buf[0] : 0)),
	flags(buf != nullptr ? buf[1] : '\0'),
	cluster(buf != nullptr ? byte_order<4>::bigend(&buf[0x2C])() : 0),
	size(buf != nullptr ? byte_order<4>::bigend(&buf[0x30])() : 0),
	creation(reinterpret_cast<const unsigned char *>((buf != nullptr ? &buf[0x34] : "\0\0\0\0"))),
	access(reinterpret_cast<const unsigned char *>((buf != nullptr ? &buf[0x38] : "\0\0\0\0"))),
	update(reinterpret_cast<const unsigned char *>((buf != nullptr ? &buf[0x3C] : "\0\0\0\0"))),
	loc(s),
	parent(nullptr),
	childs(),
	areas() {
	const std::string nchar = name_chars;
	status = (
		// 0xFF or 0x00 on 2 firsts bytes = end of entries
		(buf == nullptr || (buf[0] == EOD && buf[1] == EOD) || (buf[0] == 0 && buf[1] == 0)) ? end : (
			(
				// invalid = invalid cluster (we scan something else than an entry) or invalid name
				cluster > fatx_context::get()->par.clus_fat) || std::any_of(buf + 2, buf + 2 + (namesize <= name_size ? namesize : 2), [&] (char c) { return nchar.find(c) == std::string::npos; }) ? invalid : (
				// valid entry = good name size + cluster not free in FAT
				(namesize <= name_size && ((size == 0 && cluster == 0) || (cluster != 0 && fatx_context::get()->fat->dskmap::read(cluster) != FLK))) ? valid : (
					(
						// recoverable entry = entry with cluster not allocated to something else
						(cluster == 0 && !flags.dir) || (
							cluster != 0 && fatx_context::get()->fat->dskmap::read(cluster) == FLK && (
								([&]() -> bool {
									auto* mmap = dynamic_cast<memmap*>(fatx_context::get()->fat);
									if(mmap == nullptr)
										return true;
									if(mmap->status(cluster) == dskmap::disk)
										return true;
									if(mmap->status(cluster) != dskmap::deleted)
										return false;
									auto* prev = mmap->getentry(cluster);
									return prev != nullptr && !prev->flags.dir && prev->update.seq() < update.seq();
								}())
							)
						)
					) ? delwdata : (flags.dir ? invalid : delnodata)
				)
			)
		)
	);
	// cleaning name
	memset(name, '\0', name_size + 1);
	if(buf != nullptr)
		memcpy(name, &buf[2], (namesize <= name_size) ? namesize : name_size);
	if(status != end && ((namesize != deleted_size && namesize != strlen(name)) || strlen(name) == 0 || name[0] == sepdir[0]))
		status = invalid;
	if(status != end && status != invalid) {
		for(size_t i = 0; i < strlen(name); i++)
			name[i] = (name[i] == EOD) ? '\0' : nchar.find(name[i]) == std::string::npos ? '~' : name[i];
	}
	mux_B.name(mutex_buff + path());
	mux_D.name(mutex_data + path());
	mux_E.name(mutex_entr + path());
}
							// new entry constructor
							entry::			entry(const std::string &n, filesize s, const bool d) :
	mux_B(),
	mux_D(),
	mux_E(),
	exclusive(1),
	entbuf(),
	opened(false),
	cptacc(0),
	writeopened(false),
	status(invalid),
	namesize(0),
	flags(),
	cluster(0),
	size(d ? 0 : s),
	creation(),
	access(),
	update(),
	loc(0),
	parent(nullptr),
	childs(),
	areas() {
	areas = make_shared<vareas>(fatx_context::get()->fat->allocfat(d ? 1 : clsarithm::siz2cls(s)));
	cluster = (!d && s == 0) ? FLK : areas->first();
	if(cluster == 0)
		size = 0;
	namesize = static_cast<uint8_t>((n.length() <= name_size) ? n.length() : name_size);
	memset(name, '\0', name_size + 1);
	strncpy(name, &n[0], namesize);
	mux_B.name(mutex_buff + std::string(name));
	mux_D.name(mutex_data + std::string(name));
	mux_E.name(mutex_entr + std::string(name));
	flags.dir = d;
	touch();
	if(d && cluster != 0)
		void(entry(clsarithm::cls2ptr(cluster)).write());
}
							entry::			~entry() {
	void(flush(false));
	mux_D.lock();
	entbuf.reset();
	childs.clear();
	parent = nullptr;
	areas.reset();
	mux_D.unlock();
}

#ifndef NDEBUG
std::string					entry::			print() {
	return "-> entry " + ((status == end) ? "*END*\n" : std::format("'{}' : {}, 0x{:08X}->0x{:08X} ({}) at {}\n\t\t({}, C=>{} A=>{} U=>{}, 0x{:02X})\n",
		((status == invalid) ? "*INVALID*" : path()),
		((status == invalid) ? "bad" : (status == valid) ? "ok" : (status == delwdata) ? "del" : "ko"),
		cluster,
		((cluster != FLK && cluster != EOC && cluster <= fatx_context::get()->par.clus_fat) ? fatx_context::get()->fat->dskmap::read(cluster) : 0),
		size,
		fatx_context::get()->dev.address(loc),
		flags.print(),
		creation.print(),
		access.print(),
		update.print(),
		static_cast<unsigned int>(namesize)
	));
}
#endif
std::string					entry::			path() {
	return (
		(cluster == fatx_context::get()->par.root_clus) ? std::string(sepdir) : (
		((parent != nullptr && parent != this) ? parent->path() : (std::string(def_landf) + sepdir)) + name + (flags.dir ? sepdir : "")
	));
}
void						entry::			opendir() {
	if(status == end || status == invalid || !flags.dir || cluster == FLK || cluster == EOC) {
		#ifndef NDEBUG
			dbglog("**> can't opendir {} (status {})", path(), uint32_t(status));
		#endif
		return;
	}
	streamptr mark = 0;
	streamptr last = 0;
	std::vector<ptr_entry> bad;
	for(clusptr clus_curr = cluster; clus_curr != EOC && clus_curr != FLK && !(mark != 0 && !fatx_context::get()->mmi.recover); clus_curr = fatx_context::get()->fat->read(clus_curr)) {
		std::string buf = bytes_to_raw(fatx_context::get()->dev.read_bytes(clsarithm::cls2ptr(clus_curr), fatx_context::get()->par.clus_size));
		if(buf.size() != fatx_context::get()->par.clus_size) {
			console::write("Unreadable cluster in directory {}.\n", true, path());
			break;
		}
		for(size_t i = 0; i < fatx_context::get()->par.clus_size && !(mark != 0 && !fatx_context::get()->mmi.recover); i += ent_size) {
			entry *ent = new entry(clsarithm::cls2ptr(clus_curr) + i, &buf[i]);
			ent->parent = this;
			ent->mux_B.name(mutex_buff + ent->path());
			ent->mux_D.name(mutex_data + ent->path());
			ent->mux_E.name(mutex_entr + ent->path());
			if(ent->status == end) {
				mark = ent->loc;
				for(ptr_entry& j: bad)
					j->status = delnodata;
			}
			if(ent->status == invalid && mark == 0) {
				bad.emplace_back(ent);
				ent = nullptr;
				continue;
			}
			if(mark != 0 && ent->status == valid)
				ent->status = delwdata;
			if(
				(!fatx_context::get()->mmi.recover && ent->status != valid) ||
				ent->status == end || ent->status == invalid
			) {
				delete ent;
				ent = nullptr;
				continue;
			}
			last = ent->loc;
			if(mark == 0) {
				for(ptr_entry& j: bad)
					j->status = delnodata;
			}
			#ifndef NDEBUG
				dbglog(ent->print());
			#endif
			if(ent->status == valid) {
				for(ptr_entry& e: childs) {
					if(e->status == valid && ent->namesize == e->namesize && strncmp(ent->name, e->name, ent->namesize) == 0) {
						// duplicate reference case
						if(fatx_context::get()->mmi.prog != frontend::fsck && !fatx_context::get()->mmi.recover)
							console::write("Duplicate reference in same directory {} for entry {}.\n", fatx_context::get()->mmi.dialog, path(), ent->name);
						ent->status = duplicate;
						e->status = duplicate;
						break;
					}
				}
			}
			if(ent->flags.dir) {
				// check if no circular reference
				for(entry *e = this; ; e = e->parent) {
					if(e == nullptr || e->loc == 0)
						break;
					if(ent->cluster == e->cluster) {
						// circular reference case
						if(e->status == valid) {
							console::write("Circular reference for entry {} found in {}.", fatx_context::get()->mmi.dialog, ent->path(), path());
							if(fatx_context::get()->mmi.prog == frontend::fsck) {
								console::write(" Remove it ?", fatx_context::get()->mmi.dialog);
								if(fatx_context::get()->mmi.getanswer(true)) {
									childs.emplace_back(ent);
									remfrdir(ent);
									ent = nullptr;
									break;
								}
							}
							else
								console::write(" Skipping.\n", fatx_context::get()->mmi.dialog);
						}
						delete ent;
						ent = nullptr;
						break;
					}
				}
				if(ent == nullptr)
					continue;
			}
			childs.emplace_back(ent);
			ent = nullptr;
			if(childs.back()->flags.dir && childs.back()->status != delnodata) {
				// go one step deep
				childs.back()->opendir();
			}
		}
	}
	if(!areas || areas->empty()) {
		areas = make_shared<vareas>(fatx_context::get()->fat->getareas(cluster));
		assert(!areas->empty());
	}
	if(childs.empty() && mark == 0)
		status = delnodata;
	if(status == valid && (!bad.empty() || mark == 0)) {
		console::write("Directory {} needs corrections.", fatx_context::get()->mmi.dialog, path());
		if(fatx_context::get()->mmi.prog == frontend::fsck) {
			console::write(" Correct it ?", fatx_context::get()->mmi.dialog);
			if(fatx_context::get()->mmi.getanswer(true)) {
				if(mark == 0) {
					mark = last + ent_size;
					if((mark - fatx_context::get()->par.root_start) % fatx_context::get()->par.clus_size == 0) {
						if(fatx_context::get()->fat->resizefat(areas, areas->nbcls() + 1)) {
							console::write("Can't extend directory.\n", fatx_context::get()->mmi.dialog);
							return;
						}
						mark = clsarithm::cls2ptr(areas->last());
					}
					if(entry(mark).write())
						return;
				}
				for(ptr_entry& i: bad) {
					if(i->status == delnodata)
						if(i->write())
							return;
				}
			}
		}
		else {
			console::write("\n", fatx_context::get()->mmi.dialog);
			fatx_context::get()->ready = true;
		}
	}
	if(status == valid && mark != 0 && clsarithm::ptr2cls(mark) != areas->last()) {
		console::write("Directory {} can be shrinked.", fatx_context::get()->mmi.dialog, path());
		if(fatx_context::get()->mmi.prog == frontend::fsck) {
			console::write(" Correct it ?", fatx_context::get()->mmi.dialog);
			if(fatx_context::get()->mmi.getanswer(true))
				if(fatx_context::get()->fat->resizefat(areas, areas->nbcls(clsarithm::ptr2cls(mark)))) {
					console::write("Can't shrink directory.\n", fatx_context::get()->mmi.dialog);
					return;
				}
		}
		else
			console::write("\n", fatx_context::get()->mmi.dialog);
	}
}
int							entry::			addtodir(entry *e) {
	if(!flags.dir || e == nullptr || e == fatx_context::get()->root || e == this || e->flags.lab || cluster == 0 || (e->flags.dir && e->cluster == 0)) {
		#ifndef NDEBUG
			dbglog("**> can't add {}", path());
		#endif
		return EFAULT;
	}
	mux_E.lock();
	mux_D.lock();
	for(const ptr_entry& i: childs) {
		if(i->namesize == e->namesize && strncmp(i->name, e->name, i->namesize) == 0 && (
			i->status == entry::valid || (fatx_context::get()->mmi.recover && i->status == entry::delwdata)
		)) {
			mux_D.unlock();
			mux_E.unlock();
			return EEXIST;
		}
	}
	int res = 0;
	streamptr endp = 0;
	streamptr del = 0;
	for(clusptr i = cluster; endp == 0 && i != EOC && i != FLK; i = fatx_context::get()->fat->read(i)) {
		byte_buffer buf = fatx_context::get()->dev.read_bytes(clsarithm::cls2ptr(i), fatx_context::get()->par.clus_size);
		if(buf.size() != fatx_context::get()->par.clus_size) {
			mux_D.unlock();
			mux_E.unlock();
			return EIO;
		}
		for(size_t j = 0; j < fatx_context::get()->par.clus_size; j += ent_size) {
			const auto first = static_cast<unsigned char>(buf[j]);
			const auto second = static_cast<unsigned char>(buf[j + 1]);
			if((first == static_cast<unsigned char>(EOD) && second == static_cast<unsigned char>(EOD)) || (first == 0 && second == 0)) {
				endp = clsarithm::cls2ptr(i) + j;
				break;
			}
			if(del == 0 && first == deleted_size)
				del = clsarithm::cls2ptr(i) + j;
		}
	}
	if(endp == 0) {
		// no end of directory mark: the directory is damaged
		console::write("Directory {} has no end mark, run fsck.fatx.\n", true, path());
		mux_D.unlock();
		mux_E.unlock();
		return EIO;
	}
	// we search a deleted entry
	if(del != 0) {
		// we found one, and use it
		e->loc = del;
		e->status = entry::valid;
	}
	else {
		e->loc = endp;
		e->status = entry::valid;
		if((endp + ent_size - fatx_context::get()->par.root_start) % fatx_context::get()->par.clus_size == 0) {
			// we have to allocate one cluster more in the directory
			if(!areas || areas->empty()) {
				areas = make_shared<vareas>(fatx_context::get()->fat->getareas(cluster));
				if(areas->empty()) {
					mux_D.unlock();
					mux_E.unlock();
					return EFAULT;
				}
			}
			if((res = fatx_context::get()->fat->resizefat(areas, areas->nbcls() + 1))) {
				mux_D.unlock();
				mux_E.unlock();
				return res;
			}
			if((res = entry(clsarithm::cls2ptr(areas->last())).write())) {
				mux_D.unlock();
				mux_E.unlock();
				return res;
			}
		}
		else
			if((res = entry(e->loc + ent_size).write())) {
				mux_D.unlock();
				mux_E.unlock();
				return res;
			}
	}
	e->parent = this;
	childs.emplace_back(e);
	if((res = e->write())) {
		mux_D.unlock();
		mux_E.unlock();
		return res;
	}
	touch(false, false, true);
	res = write(true);
	mux_D.unlock();
	mux_E.unlock();
	return res;
}
void						entry::			remfrdir(entry *e) {
	mux_E.lock();
	if(!flags.dir || e == nullptr || e == fatx_context::get()->root || e == this || e->flags.lab || (e->status != valid && e->status != duplicate)) {
		mux_E.unlock();
		#ifndef NDEBUG
			dbglog("**> can't remove {}", path());
		#endif
		return;
	}
	while(!e->childs.empty())
		e->remfrdir(e->childs.front().get());
	mux_D.lock();
	if(e->cluster != FLK)
		fatx_context::get()->fat->freefat(e->cluster);
	e->status = e->cluster == FLK ? delnodata : delwdata;
	if(e->write()) {
		mux_D.unlock();
		mux_E.unlock();
		return;
	}
	auto i = find_if(childs.begin(), childs.end(), [e] (const ptr_entry& a) -> bool { return a.get() == e; });
	assert(i != childs.end());
	childs.erase(i);
	touch(false, false, true);
	void(write(true));
	mux_D.unlock();
	mux_E.unlock();
	return;
}
entry*						entry::			find(const char *path) {
	entry*									res = this;
	boost::char_separator<char>				sep(sepdir);
	std::string								src(path);
	if(src.rfind(sepdir, src.size()) != std::string::npos && src.rfind(sepdir, src.size()) == (src.size() - 1))
		src.erase(src.size() - 1);
	boost::tokenizer<boost::char_separator<char> >		dirs(src, sep);
	bool									found = true;
	for(const std::string &d : dirs) {
		found = false;
		res->mux_E.lock_shared();
		for(ptr_entry& e: res->childs) {
			if(e->status == entry::valid && ((fatx_context::get()->mmi.cutname) ? e->name == d.substr(0, name_size) : e->name == d)) {
				found = true;
				res->mux_E.unlock_shared();
				res	= e.get();
				break;
			}
		}
		if(!found && fatx_context::get()->mmi.recover) {
			for(ptr_entry& e: res->childs) {
				if(e->name == d.substr(0, name_size)) {
					found = true;
					res->mux_E.unlock_shared();
					res	= e.get();
					break;
				}
			}
		}
		if(!found) {
			res->mux_E.unlock_shared();
			break;
		}
	}
	return !found ? nullptr : res;
}
void						entry::			touch(bool cre, bool acc, bool upd) {
	time_t t = time(nullptr);
	if(cre)
		creation = date(t);
	if(acc)
		access = date(t);
	if(upd)
		update = date(t);
}
int							entry::			write(bool l) {
	if(l && parent != nullptr && parent != this)
		parent->mux_D.lock();
	std::string buf(ent_size, '\0');
	if(status == end)
		memset(&buf[0], EOD, ent_size);
	else {
		touch(false, true, false);
		buf[0] = static_cast<char>((status == delwdata || status == delnodata) ? deleted_size : strlen(name));
		flags.write(&buf[1]);
		memcpy(&buf[2], name, name_size);
		memcpy(&buf[0x2C], byte_order<4>::bigend(static_cast<byte_order<4>::value_type>(cluster)).data(), 4);
		memcpy(&buf[0x30], byte_order<4>::bigend(static_cast<byte_order<4>::value_type>(size)).data(), 4);
		creation.write(reinterpret_cast<unsigned char*>(&buf[0x34]));
		access.write(reinterpret_cast<unsigned char*>(&buf[0x38]));
		update.write(reinterpret_cast<unsigned char*>(&buf[0x3C]));
	}
	byte_buffer raw = raw_to_bytes(buf);
	int res = loc == 0 ? 0 : fatx_context::get()->dev.write_bytes(loc, byte_view(raw.data(), raw.size()));
	if(l && parent != nullptr && parent != this)
		parent->mux_D.unlock();
	return res;
}
int							entry::			rename(const char *n) {
	if(status != valid)
		return EFAULT;
	std::string nam;
	std::string dir;
	if(std::string(n).rfind(sepdir, std::string(n).size()) != std::string::npos) {
		nam = std::string(n).substr(std::string(n).rfind(sepdir, std::string(n).size()) + 1).data();
		dir = std::string(n).substr(0, std::string(n).rfind(sepdir, std::string(n).size())).data();
		if(dir.empty())
			dir = sepdir;	// "/name" is in the root directory
	}
	else
		nam = n;
	entry *dst = fatx_context::get()->root->find(n);
	if(std::string(n).empty() || flags.lab || dst == this)
		return 0;
	if(flags.dir && dst != nullptr)
		return ENOTEMPTY;
	if(dst != nullptr)
		dst->parent->remfrdir(dst);
	mux_E.lock();
	if(dir.size() != 0) {
		assert(parent != nullptr);
		entry *newpar = fatx_context::get()->root->find(dir.data());
		entry *oldpar = parent;
		if(newpar == nullptr) {
			mux_E.unlock();
			return ENOENT;
		}
		if(oldpar != newpar) {
			// std::move
			oldpar->mux_D.lock();
			status = delwdata;
			oldpar->mux_E.lock_shared();
			auto i = find_if(oldpar->childs.begin(), oldpar->childs.end(), [this] (const ptr_entry& a) -> bool { return a.get() == this; });
			assert(i != oldpar->childs.end());
			oldpar->mux_E.unlock_shared();
			oldpar->mux_E.lock();
			entry* me = i->release();
			oldpar->childs.erase(i);
			oldpar->mux_E.unlock();
			int res = 0;
			// the old record becomes a deleted empty file: as a deleted
			// directory still pointing to clusters in use, it would be seen
			// as invalid
			entry gone("_none", 0);
			gone.loc = loc;
			gone.status = delwdata;
			if((res = gone.write())) {
				oldpar->mux_D.unlock();
				mux_E.unlock();
				return res;
			}
			oldpar->touch(false, false, true);
			oldpar->mux_D.unlock();
			if((res = oldpar->write(true))) {
				mux_E.unlock();
				return res;
			}
			status = valid;
			if((res = newpar->addtodir(me))) {
				oldpar->childs.emplace_back(me);
				parent = oldpar;
				mux_E.unlock();
				return res;
			}
		}
	}
	memset(name, '\0', name_size + 1);
	strncpy(name, nam.c_str(), name_size);
	namesize = static_cast<uint8_t>((status == valid) ? ((nam.size() <= name_size) ? nam.size() : name_size) : namesize);
	touch(false, false, true);
	auto res = write(true);
	mux_E.unlock();
	return res;
}
void						entry::			recover() {
	if(fatx_context::get()->mmi.local) {
		if(!flags.dir) {
			std::ifstream fr;
			std::filesystem::create_directories(std::filesystem::path("./" + path()).remove_filename());
			fr.open("./" + path());
			if(fr) {
				fr.close();
				console::write("Can't open file for writing, file already exists locally.\n", true);
			}
			else {
				std::ofstream f("./" + path(), std::ios::binary | std::ios::trunc);
				std::string s(size, '\0');
				if(data(&s[0], true, 0, size)) {
					f.close();
					return;
				}
				f << s;
				f.close();
			}
		}
	}
	else {
		const entry *e = parent->find(name);
		if(e != nullptr && e->status == entry::valid) {
			console::write("Can't restore file. Another valid file with same name exists in this directory.\n", true);
		}
		else {
			byte_buffer buf = fatx_context::get()->dev.read_bytes(clsarithm::cls2ptr(clsarithm::ptr2cls(loc)), fatx_context::get()->par.clus_size);
			streamptr mark = 0;
			for(size_t i = 0; i < buf.size(); i+= ent_size) {
				if(static_cast<unsigned char>(buf[i]) == static_cast<unsigned char>(EOD)) {
					mark = clsarithm::cls2ptr(clsarithm::ptr2cls(loc)) + i;
					break;
				}
			}
			if(mark != 0 && loc > mark) {
				entry no("_none", 0);
				no.loc = mark;
				no.status = delwdata;
				if(no.write())
					return;
				for(size_t i = mark + ent_size; i < clsarithm::cls2ptr(clsarithm::ptr2cls(loc)) + fatx_context::get()->par.clus_size; i+= ent_size) {
					std::byte deleted = static_cast<std::byte>(deleted_size);
					if(fatx_context::get()->dev.write_bytes(i, byte_view(&deleted, 1)))
						return;
				}
			}
			status = entry::valid;
			if(write())
				return;
			fatx_context::get()->fat->getareas(cluster, [](clusptr c, clusptr v) -> void {
				if(fatx_context::get()->fat->write(c, v))
					return;
				dynamic_cast<memmap*>(fatx_context::get()->fat)->memchain.find(c)->second.status = memmap::modified;
			});
		}
	}
}
void						entry::			guess() {
	clusptr	p = cluster;
	clusptr s = 0;
	std::set<entry*> old;
	#if !defined NDEBUG && defined DBG_GUESS
		dbglog("GUESS: {} 0x{:08X} ({})", path(), cluster, clsarithm::siz2cls(size()));
	#endif
	for(
		clusptr q = cluster, nb = flags.dir ? 1 : clsarithm::siz2cls(size);
		nb > 0;
		q = (q <= fatx_context::get()->par.clus_fat) ? q + 1 : 2
	) {
		// is it free ?
		if(fatx_context::get()->fat->read(q) != FLK) {
			// check if first cluster is not available
			if(q == cluster) {
				// if it is the case, give up, it's unrecoverable
				status = delnodata;
				if(fatx_context::get()->mmi.verbose)
					console::write("{} {} not recoverable\n", path(), flags.dir ? sepdir : std::format(" ({})", size));
				return;
			}
			// check if it's a previous guess, if it is the case, use it
			if(fatx_context::get()->fat->getentry(q) != this) {
				// check if cluster is occupied by a file deleted older than ours
				if(
					fatx_context::get()->fat->status(q) == memmap::deleted &&
					!fatx_context::get()->fat->getentry(q)->flags.dir &&
					fatx_context::get()->mmi.deldate &&
					fatx_context::get()->fat->getentry(q)->update.seq() < update.seq()
				) {
					// we remember it and use that cluster
					old.insert(fatx_context::get()->fat->getentry(q));
				}
				else {
					// check if it belongs to a lost chain (occupied but not marked)
					if(!fatx_context::get()->fat->getentry(q)) {
						memmap::lost_t& mlost = dynamic_cast<memmap*>(fatx_context::get()->fat)->lost;
						memmap::lost_t::iterator lc = find_if(mlost.begin(), mlost.end(), [q] (const vareas& i) -> bool { return i.first() == q; });
						// if it's the beginning of a lost chain not exeeding the remaining size, use it
						if(lc != mlost.end() && lc->nbcls() <= nb) {
							// using lost chain
							mlost.erase(lc);
							// linking to lost chain
							fatx_context::get()->fat->change(p, this, q, memmap::deleted);
							// marking lost chain
							vareas va = fatx_context::get()->fat->getareas(
								q, [this](clusptr c, clusptr v) -> void {
									fatx_context::get()->fat->change(c, this, v, memmap::deleted);
								});
							// changing to last cluster of lost chain
							q = va.last();
							nb -= va.nbcls();
							p = q;
							continue;
						}
					}
					// cluster is occuped, skip it
					if(s == 0)
						s = q;
					continue;
				}
			}
		}
		// q is free
		if(s != 0) {
			#if !defined NDEBUG && defined DBG_GUESS
				dbglog(" skip: 0x{:08X}-0x{:08X} [belongs to {} [{}]]",
					s, q - 1
					fatx_context::get()->fat->getentry(s) == nullptr ? "lost chain" : fatx_context::get()->fat->getentry(s)->path(),
					fatx_context::get()->fat->status(s) == memmap::deleted ? 'D' : 'X'
				);
			#endif
		}
		// we suppose that it belongs to our entry
		fatx_context::get()->fat->change(p, this, q, memmap::deleted);
		nb--;
		p = q;
	}
	fatx_context::get()->fat->change(p, this, EOC, memmap::deleted);
	#ifndef NDEBUG
	dbglog("{}  Guessed clusters: {:06d} out of {:06d} for size {} ({})",
		(clsarithm::siz2cls(size) != 0 && fatx_context::get()->fat->getareas(cluster).nbcls() != clsarithm::siz2cls(size)) ? "!!!" : "   ",
		fatx_context::get()->fat->getareas(cluster).nbcls(),
		flags.dir ? 1 : clsarithm::siz2cls(size),
		size,
		path()
	);
	#endif
	// we guess older deleted things pushed by our entry
	for(entry *i: old)
		i->guess();
}
bool						entry::			analyse(pass_t step, const std::string &header) {
	bool recovered	= false;
	if(step != findfile && flags.dir && status == delnodata) {
		console::write("Entry {} points to invalid data. Skipping.\n", fatx_context::get()->mmi.dialog, header + name);
		return false;
	}
	if(step == findfile && (status == valid || status == duplicate)) {
		// sanity checks
		if(flags.dir && cluster == FLK) {
			console::write("Entry {} has invalid cluster pointer.", fatx_context::get()->mmi.dialog, header + name);
			if(fatx_context::get()->mmi.prog == frontend::fsck) {
				console::write(" Remove it ?", fatx_context::get()->mmi.dialog);
				if(fatx_context::get()->mmi.getanswer(true)) {
					status = delnodata;
					cluster = FLK;
					if(write())
						return false;
				}
			}
			else {
				console::write("\n", fatx_context::get()->mmi.dialog);
				status = delnodata;
			}
			return false;
		}
		bool dl = fatx_context::get()->mmi.dellost;
		fatx_context::get()->mmi.dellost = true;
		clusptr cnt	= fatx_context::get()->fat->getareas(cluster).nbcls();
		fatx_context::get()->mmi.dellost = dl;
		if(!flags.dir && cnt != clsarithm::siz2cls(size)) {
			console::write("{}ntry {} has wrong size: declared {}, found {}.", fatx_context::get()->mmi.dialog,
				status == duplicate ? "Duplicate e" : "E",
				path(),
				size,
				cnt * fatx_context::get()->par.clus_size
			);
			if(fatx_context::get()->mmi.prog == frontend::fsck) {
				if(status == duplicate) {
					console::write(" Remove it ?", fatx_context::get()->mmi.dialog);
					if(fatx_context::get()->mmi.getanswer(true)) {
						status = delnodata;
						cluster = FLK;
						if(write())
							return false;
						return false;
					}
				}
				console::write(" Possible {} data, correct it ?", fatx_context::get()->mmi.dialog, cnt > clsarithm::siz2cls(size) ? "extra" : "loss of");
				if(fatx_context::get()->mmi.getanswer(true)) {
					size = cnt * fatx_context::get()->par.clus_size;
					if(write())
						return false;
				}
			}
			else
				console::write("\n", fatx_context::get()->mmi.dialog);
		}
		entry *s = nullptr;
		fatx_context::get()->fat->getareas(cluster, [&s] (clusptr c, clusptr) -> void {
			if(s == nullptr)
				s = fatx_context::get()->fat->getentry(c);
		});
		if(s != nullptr) {
			console::write("Conflict between {}entry {} and {}entry {} (shares same blocks).", fatx_context::get()->mmi.dialog,
				status == duplicate ? "duplicate " : "",
				header + name,
				s->status == duplicate || s->status == validupl ? "duplicate " : "",
				s->path().data()
			);
			if(fatx_context::get()->mmi.prog == frontend::fsck) {
				console::write(" Remove {} (part of the other one) ?", path().data(), fatx_context::get()->mmi.dialog);
				if(fatx_context::get()->mmi.getanswer(true)) {
					if(cluster != s->cluster) {
						if(cluster != FLK)
							fatx_context::get()->fat->freefat(cluster);
						status = delnodata;
						cluster = FLK;
						if(write())
							return false;
					}
					else {
						status = delnodata;
						cluster = FLK;
						if(write())
							return false;
					}
				}
			}
			else {
				console::write("\n", fatx_context::get()->mmi.dialog);
				status = delnodata;
			}
			return false;
		}
		if(status == duplicate) {
			auto e = find_if(parent->childs.begin(), parent->childs.end(), [this] (const ptr_entry& i) -> bool { return namesize == i->namesize && strncmp(name, i->name, namesize) == 0; });
			if(e == parent->childs.end() || ((*e)->status != duplicate && (*e)->status != validupl))
				status = valid;		// problem already solved
			else if((*e)->status == duplicate)
				status = validupl;	// waiting for analysis of the other entry
			else {
				// both entries are valid and different
				console::write("Duplicate entry {}.", header + name, fatx_context::get()->mmi.dialog);
				if(fatx_context::get()->mmi.prog == frontend::unrm) {
					// consider it if different from its brother
					std::string t(name);
					t += "~";
					strncpy(name, t.c_str(), t.size());
					namesize = std::max<uint8_t>(static_cast<uint8_t>(namesize + 1), name_size);
					console::write(" Reading it as {}.\n", name, fatx_context::get()->mmi.dialog);
				}
				else if(fatx_context::get()->mmi.prog == frontend::fsck) {
					// interactive way
					std::string t(name);
					t += "~";
					strncpy(name, t.c_str(), t.size());
					namesize = std::max<uint8_t>(static_cast<uint8_t>(namesize + 1), name_size);
					console::write(" Create it (as {}) ?", name, fatx_context::get()->mmi.dialog);
					if(fatx_context::get()->mmi.getanswer(true)) {
						if(write())
							return false;
					}
					else {
						console::write(" Skipping.\n", fatx_context::get()->mmi.dialog);
						return false;
					}
				}
				else {
					// forget it
					console::write(" Skipping.\n", fatx_context::get()->mmi.dialog);
					return false;
				}
			}
		}
		// we mark valid entries
		if(fatx_context::get()->mmi.verbose)
			console::write(header + name + (flags.dir ? sepdir : std::format(" ({})", size)) + "\n");
		fatx_context::get()->mmi.dellost = true;
		areas = make_shared<vareas>(
			fatx_context::get()->fat->getareas(cluster, [this](clusptr c, clusptr) -> void {
				fatx_context::get()->fat->change(c, this);
			}));
		fatx_context::get()->mmi.dellost = dl;
	}
	if(step == finddel && (status == delwdata || status == delnodata)) {
		if(fatx_context::get()->mmi.verbose) {
			console::write(header + name + (flags.dir ? sepdir : std::format(" ({})", size))
				+ " " + (status == delwdata ? "deleted" : "not recoverable") + "\n"
			);
		}
		// we guess recoverable entries
		if(status == delwdata)
			guess();
	}
	if(step == tryrecov && status == delwdata && !flags.dir) {
		// we recover file sub-entries
		console::write(header + name + std::format(" ({})", size) + " recover ?");
		if(fatx_context::get()->mmi.getanswer()) {
			recovered = true;
			recover();
		}
	}
	if(flags.dir) {
		for(ptr_entry& ent: childs) {
			// we go one step deeper
			recovered = ent->analyse(step, header + name + sepdir) || recovered;
		}
	}
	if(recovered && flags.dir && status != valid && !fatx_context::get()->mmi.local) {
		console::write("Recovering parent directory {}.\n", header + name, fatx_context::get()->mmi.dialog);
		recover();
	}
	return recovered;
}
int							entry::			resize(const filesize s) {
	#ifndef NDEBUG
		dbglog("RESIZE: {} size:{}->{}", path(), size, s);
	#endif
	if(flags.dir)
		return EISDIR;
	if(s == size)
		return 0;
	if(s == 0) {
		areas = nullptr;
		fatx_context::get()->fat->freefat(cluster);
		cluster = 0;
		size = s;
	}
	else if(size == 0) {
		vareas &&v = fatx_context::get()->fat->allocfat(clsarithm::siz2cls(s));
		if(v.empty())
			return ENOSPC;
		cluster = v.first();
		size = s;
		areas = make_shared<vareas>(v.sub(size));
	}
	if(s != size) {
		int res = 0;
		if((res = fatx_context::get()->fat->resizefat(areas, clsarithm::siz2cls(s))))
			return res;
		size = s;
		areas = make_shared<vareas>(areas->sub(size));
	}
	return write(true);
}
int							entry::			data(char *buf, bool r, filesize offset, filesize s) {
	s = (s == 0) ? (r ? size - offset : size) : (r ? std::min<filesize>(s, size - offset) : s);
	#ifndef NDEBUG
		dbglog("DATA: {}({}) [{}:0x{:08X}({})]", path(), size, r ? 'R' : 'W', offset, s);
	#endif
	if(flags.dir)
		return EISDIR;
	int res = 0;
	if(!r && (size == 0 || (offset + s) > size)) {
		if((res = resize(offset + s))) {
			#ifndef NDEBUG
				dbglog("DATA:{} resize failed.", path());
			#endif
			return res;
		}
	}
	if(size != 0) {
		if(!areas || areas->empty()) {
			areas = make_shared<vareas>(fatx_context::get()->fat->getareas(cluster).sub(size));
			if(areas->empty())
				return EFAULT;
		}
		for(const area& i: areas->sub(s, offset)) {
			if(r) {
				byte_buffer raw = fatx_context::get()->dev.read_bytes(i.pointer, i.size);
				if(raw.size() != i.size)
					return EIO;
				for(size_t idx = 0; idx < i.size; idx++)
					buf[i.offset - offset + idx] = static_cast<char>(raw[idx]);
			}
			else {
				const auto* first = reinterpret_cast<const std::byte*>(buf + i.offset - offset);
				if((res = fatx_context::get()->dev.write_bytes(i.pointer, byte_view(first, i.size))))
					return res;
			}
		}
	}
	if(!r) {
		touch(false, false, true);
		res = write(true);
	}
	return res;
}
size_t						entry::			bufread(char *buf, filesize offset, filesize s) {
	mux_E.lock();
	s = std::min<filesize>(size, offset + s) - offset;
	if(offset >= size || s == 0) {
		mux_E.unlock();
		return 0;
	}
	mux_B.lock();
	if(entbuf && (offset < entbuf->offset || (offset + s) > (entbuf->offset + entbuf->size()))) {
		if(flush(false)) {
			#ifndef NDEBUG
				dbglog("**> flush buffer failed ({})", path());
			#endif
			mux_B.unlock();
			mux_E.unlock();
			return 0;
		}
		entbuf.reset();
	}
	if(!entbuf) {
		entbuf.reset(new buffer(offset, size - offset));
		if(entbuf->size() == 0 || data(entbuf->data(), true, entbuf->offset, entbuf->size())) {
			#ifndef NDEBUG
				dbglog("**> alloc buffer or read operation failed ({}: 0x{:08X} {})", path(), offset, s);
			#endif
			mux_B.unlock();
			mux_E.unlock();
			return 0;
		}
		#ifndef NDEBUG
			dbglog("--> new read buffer ({} at 0x{:016X}: {})", path(), uint64_t(entbuf.get()), entbuf->size());
		#endif
	}
	s = std::min<filesize>(s, entbuf->size());
	memcpy(buf, &(*entbuf)[offset - entbuf->offset], s);
	#ifndef NDEBUG
		dbglog("--> read buffer ({} at 0x{:016X}({}): {}({}))", path(), uint64_t(entbuf.get()), entbuf->size(), offset - entbuf->offset, s);
	#endif
	#if !defined NDEBUG && defined DBGBUFDMP
		(*entbuf)(offset - entbuf->offset);
	#endif
	mux_B.unlock();
	mux_E.unlock();
	return s;
}
size_t						entry::			bufwrite(const char *buf, filesize offset, filesize s) {
	mux_E.lock();
	if(!writeopened) {
		#ifndef NDEBUG
			dbglog("**> writing a file not opened for write ({})", path());
		#endif
		mux_E.unlock();
		return false;
	}
	mux_B.lock();
	if(size < offset + s && resize(offset + s)) {
		#ifndef NDEBUG
			dbglog("**> file resize failed ({}: 0x{:08X} {})", path(), offset, s);
		#endif
		mux_B.unlock();
		mux_E.unlock();
		return 0;
	}
	int res = 0;
	if(entbuf) {
		if(entbuf->offset + entbuf->size() == offset) {
			entbuf->enlarge(entbuf->size() + s);
			if(entbuf->offset + entbuf->size() < offset + s) {
				res = flush(false);
				entbuf.reset();
			}
		}
		else {
			res = flush(false);
			entbuf.reset();
		}
		if(res) {
			#ifndef NDEBUG
				dbglog("**> flush buffer failed ({})", path());
			#endif
			mux_B.unlock();
			mux_E.unlock();
			return 0;
		}
	}
	if(!entbuf) {
		entbuf.reset(new buffer(offset, s));
		if(entbuf->size() < s) {
			#ifndef NDEBUG
				dbglog("**> alloc buffer failed ({}: 0x{:08X} {})", path(), offset, s);
			#endif
			mux_B.unlock();
			mux_E.unlock();
			return 0;
		}
	}
	s = std::min<filesize>(s, entbuf->size());
	memcpy(&(*entbuf)[offset - entbuf->offset], buf, s);
	entbuf->touched = true;
	#ifndef NDEBUG
		dbglog("--> write buffer ({}) at 0x{:08X}({}): {}({})", path(), uint64_t(entbuf.get()), entbuf->size(), offset - entbuf->offset, s);
	#endif
	#if !defined NDEBUG && defined DBGBUFDMP
		(*entbuf)(offset - entbuf->offset);
	#endif
	mux_B.unlock();
	mux_E.unlock();
	return s;
}
int							entry::			flush(bool l) {
	if(flags.dir || !entbuf)
		return 0;
	int res = 0;
	if(l)
		mux_B.lock();
	#ifndef NDEBUG
	if(entbuf->touched)
		dbglog("<-> flush buffer ({} at 0x{:016X}: {})", path(), uint64_t(entbuf.get()), entbuf->size());
	else
		dbglog("<-> no need to flush buffer ({} at 0x{:016X}: {})", path(), uint64_t(entbuf.get()), entbuf->size());
	#endif
	if(entbuf->touched) {
		if(writeopened) {
			res = data(entbuf.get()->data(), false, entbuf->offset, entbuf->size());
			if(!res)
				entbuf.reset();
		}
		else
			res = EACCES;
	}
	if(l)
		mux_B.unlock();
	return res;
}
void						entry::			open(bool w) {
	if(flags.dir)
		return;
	mux_E.lock();
	if(w) {
		exclusive.acquire();
		writeopened = true;
		if(cluster != 0 && size != 0)
			areas = make_shared<vareas>(fatx_context::get()->fat->getareas(cluster).sub(size));
	}
	else {
		if(cptacc == 0)
			exclusive.acquire();
		if(cptacc == 0 && cluster != 0 && size != 0)
			areas = make_shared<vareas>(fatx_context::get()->fat->getareas(cluster).sub(size));
		cptacc++;
	}
	opened = true;
	mux_E.unlock();
}
void						entry::			close(bool w) {
	if(flags.dir)
		return;
	mux_E.lock();
	if(w) {
		if(!writeopened) {
			#ifndef NDEBUG
				dbglog("**> closing a file not opened in write mode ({})", path());
			#endif
			mux_E.unlock();
			return;
		}
		if(flush(true)) {
			mux_E.unlock();
			return;
		}
		writeopened = false;
		exclusive.release();
	}
	else {
		if(cptacc == 0) {
			#ifndef NDEBUG
				dbglog("**> closing a file not opened in read mode ({})", path());
			#endif
			mux_E.unlock();
			return;
		}
		cptacc--;
		if(cptacc == 0) {
			areas = nullptr;
			if(entbuf) {
				mux_B.lock();
				entbuf.reset();
				mux_B.unlock();
			}
		}
		if(cptacc == 0)
			exclusive.release();
	}
	opened = writeopened || cptacc > 0;
	mux_E.unlock();
}
