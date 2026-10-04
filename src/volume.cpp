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

#include "volume.hpp"
#include "actions.hpp"
#include "context.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <set>

namespace fatx {

namespace {

const char* const				prog_argv[] = { "fatx", nullptr };

// Turns the library's console output into whole lines for a log_sink.
class								line_sink {
private:
	log_sink						out;
	std::function<void(const std::string &)>	collect;
	std::string						pending;
	log_level						level = log_level::debug;
	static int						rank(log_level l) { return l == log_level::error ? 2 : l == log_level::info ? 1 : 0; }
public:
	explicit						line_sink(log_sink s, std::function<void(const std::string &)> c = {}) : out(std::move(s)), collect(std::move(c)) { }
									~line_sink() { finish(); }
									line_sink(const line_sink &) = delete;
	line_sink&						operator = (const line_sink &) = delete;
	void							add(console::level l, const std::string &text) {
		const log_level ll = l == console::level::error ? log_level::error : l == console::level::info ? log_level::info : log_level::debug;
		if(pending.empty() || rank(ll) > rank(level))
			level = ll;
		for(char c: text) {
			if(c == '\n')
				emit();
			else
				pending += c;
		}
	}
	void							emit() {
		if(collect && level != log_level::debug)
			collect(pending);
		if(out)
			out(level, pending);
		pending.clear();
		level = log_level::debug;
	}
	void							finish() {
		if(!pending.empty())
			emit();
	}
	console::sink_t					sink() { return [this](console::level l, const std::string &s) { add(l, s); }; }
};

// Errors inside the library must not escape as exceptions into the application.
template<typename F>
int									guarded(F &&f) noexcept {
	try {
		return f();
	}
	catch(const std::bad_alloc &) {
		return ENOMEM;
	}
	catch(...) {
		return EIO;
	}
}

// Every call into the library runs inside a session: the volume's context is
// the current one and the library's messages go to the volume's sink.
class								session {
private:
	console::scoped_sink			sink;
	fatx_context::scope				scope;
public:
									session(const console::sink_t &s, fatx_context *c) : sink(s), scope(c) { }
};

const std::set<std::string>		known_tables = { "file", "mu", "hd", "kit" };
const std::set<std::string>		known_partitions = { "sc", "gc", "se1", "se2", "xdv", "x1", "x2" };

int									configure(frontend &m, const location &where, const std::shared_ptr<io_backend> &device) {
	if(!device)
		return EINVAL;
	if(!known_tables.contains(where.table) || !known_partitions.contains(where.partition))
		return EINVAL;
	m.table = where.table;
	m.partition = where.partition;
	m.offset = where.offset;
	m.size = where.size;
	m.input = "(device)";
	m.backend = device;
	m.force_n = true;			// never wait for an answer on stdin
	return 0;
}

uint32_t							bigend32(const std::byte *p) {
	return
		(static_cast<uint32_t>(p[0]) << 24) |
		(static_cast<uint32_t>(p[1]) << 16) |
		(static_cast<uint32_t>(p[2]) << 8) |
		static_cast<uint32_t>(p[3]);
}

bool								has_fsid(io_backend &dev, uint64_t offset) {
	std::byte buf[4];
	if(offset > dev.size() || dev.size() - offset < blksize || !dev.read_at(offset, buf))
		return false;
	return std::memcmp(buf, fsid, 4) == 0;
}

bool								same_name(const std::string &a, const std::string &b) {
	return std::ranges::equal(a, b, [] (char x, char y) noexcept {
		return std::tolower(static_cast<unsigned char>(x)) == std::tolower(static_cast<unsigned char>(y));
	});
}

// Splits "/a/b/c" into "/a/b" and "c".
bool								split(const std::string &path, std::string &dir, std::string &name) {
	std::string p = path;
	while(p.size() > 1 && p.back() == sepdir[0])
		p.pop_back();
	const auto pos = p.rfind(sepdir[0]);
	if(pos == std::string::npos || p.size() <= 1)
		return false;
	dir = pos == 0 ? std::string(sepdir) : p.substr(0, pos);
	name = p.substr(pos + 1);
	return true;
}

entry_info							to_info(const entry &e) {
	entry_info i;
	i.name = e.name;
	i.directory = e.flags.dir;
	i.read_only = e.flags.ro;
	i.hidden = e.flags.hid;
	i.system = e.flags.sys;
	i.archive = e.flags.arc;
	i.label = e.flags.lab;
	i.size = e.flags.dir ? 0 : e.size;
	i.first_cluster = static_cast<uint32_t>(e.cluster);
	i.created = e.creation();
	i.accessed = e.access();
	i.modified = e.update();
	return i;
}

entry*								child(entry *dir, const std::string &name, bool any_case) {
	for(const ptr_entry &c: dir->childs) {
		if((c->status == entry::valid || c->status == entry::duplicate) &&
			(any_case ? same_name(c->name, name) : name == c->name))
			return c.get();
	}
	return nullptr;
}

bool								has_label_entry(const entry *e) noexcept {
	if(e->flags.lab)
		return true;
	return std::ranges::any_of(e->childs, [] (const ptr_entry &c) noexcept { return has_label_entry(c.get()); });
}

// Loads the cluster areas of a file, needed before resizing it.
int									load_areas(entry *e) {
	if(e->flags.dir || e->size == 0 || e->cluster == FLK || (e->areas && !e->areas->empty()))
		return 0;
	e->areas = std::make_shared<vareas>(fatx_context::get()->fat->getareas(e->cluster).sub(e->size));
	return e->areas->empty() ? EIO : 0;
}

void								discard(entry *e) {
	if(e->cluster != FLK && e->cluster != 0)
		fatx_context::get()->fat->freefat(e->cluster);
	e->cluster = FLK;
	e->parent = nullptr;
	delete e;
}

}

// --- names ------------------------------------------------------------------

const char*							allowed_name_characters() {
	return name_chars;
}
std::size_t							max_name_length() {
	return name_size;
}
int									validate_name(const std::string &name) {
	if(name.empty() || name == "." || name == "..")
		return EINVAL;
	if(name.size() > name_size)
		return ENAMETOOLONG;
	const std::string allowed = name_chars;
	return std::ranges::all_of(name, [&allowed] (char c) noexcept { return allowed.find(c) != std::string::npos; }) ? 0 : EINVAL;
}

// --- probe ------------------------------------------------------------------

std::vector<partition_info>			probe(const std::shared_ptr<io_backend> &device, const log_sink &) {
	std::vector<partition_info> res;
	if(!device)
		return res;
	const uint64_t ts = device->size();
	const auto sch = partition::scheme();
	const auto names = partition::labels();
	std::map<std::string, std::vector<partition_info>> found;
	for(const auto &[table, parts]: sch) {
		if(table == "usb")
			continue;		// a USB drive is a set of files, not one device
		const auto open_ended = std::ranges::find_if(parts, [] (const auto &p) noexcept { return p.second.second == 0; });
		if(open_ended != parts.end() && ts <= open_ended->second.first)
			continue;		// the same rule as partition::setup()
		for(const auto &[part, geometry]: parts) {
			if(!has_fsid(*device, geometry.first))
				continue;
			partition_info i;
			i.where.table = table;
			i.where.partition = part;
			i.table_name = names.at(table);
			i.name = names.at(part);
			i.offset = geometry.first;
			i.size = geometry.second != 0 ? std::min<uint64_t>(geometry.second, ts - geometry.first) : ts - geometry.first;
			found[table].push_back(i);
		}
	}
	// devkit hard disk: partitions described by a header in the first sector
	std::byte header[blksize];
	if(ts >= blksize && device->read_at(0, header) && bigend32(&header[0]) == 0x00020000) {
		const std::pair<const char*, std::pair<uint64_t, uint64_t>> kit[] = {
			{ "xdv", { uint64_t(bigend32(&header[16])) * blksize, uint64_t(bigend32(&header[20])) * blksize } },
			{ "x2",  { uint64_t(bigend32(&header[8])) * blksize,  uint64_t(bigend32(&header[12])) * blksize } },
		};
		for(const auto &[part, geometry]: kit) {
			if(geometry.first >= ts || !has_fsid(*device, geometry.first))
				continue;
			partition_info i;
			i.where.table = "kit";
			i.where.partition = part;
			i.table_name = names.at("kit");
			i.name = names.at(part);
			i.offset = geometry.first;
			i.size = std::min<uint64_t>(geometry.second, ts - geometry.first);
			found["kit"].push_back(i);
		}
	}
	// the most specific layout wins: a whole disk, then a memory unit, then a
	// bare partition
	if(found.contains("hd"))
		res = std::move(found["hd"]);
	else if(found.contains("kit"))
		res = std::move(found["kit"]);
	else if(found.contains("mu") && found["mu"].size() == sch.at("mu").size())
		res = std::move(found["mu"]);
	else if(found.contains("file"))
		res = std::move(found["file"]);
	return res;
}

// --- volume -----------------------------------------------------------------

struct								volume::impl {
	std::shared_ptr<io_backend>		device;
	std::unique_ptr<line_sink>		lines;
	console::sink_t					sink;
	std::unique_ptr<frontend>		mmi;
	std::unique_ptr<fatx_context>	ctx;
	bool							writable = false;
};

									volume::		volume() : d(std::make_unique<impl>()) { }
									volume::		~volume() {
	if(d->ctx) {
		session s(d->sink, d->ctx.get());
		void(d->ctx->dev.sync());
		d->ctx.reset();
	}
}

std::unique_ptr<volume>				volume::		open(std::shared_ptr<io_backend> device, const location &where,
														bool writable, log_sink sink, int *error) {
	int dummy = 0;
	int &err = error ? *error : dummy;
	std::unique_ptr<volume> v(new volume());
	impl &d = *v->d;
	d.device = device;
	d.writable = writable;
	d.lines = std::make_unique<line_sink>(std::move(sink));
	d.sink = d.lines->sink();
	d.mmi = std::make_unique<frontend>(1, prog_argv);
	err = configure(*d.mmi, where, device);
	if(err == 0 && writable && !device->writable())
		err = EROFS;
	if(err == 0) {
		d.mmi->set_readonly(!writable);
		session s(d.sink, nullptr);
		err = guarded([&d] () -> int {
			d.ctx = std::make_unique<fatx_context>(*d.mmi);
			int res = d.ctx->setup();
			return res == 0 && d.ctx->root == nullptr ? EIO : res;
		});
		if(err)
			d.ctx.reset();
	}
	if(err)
		v.reset();
	return v;
}

volume_info							volume::		info() {
	session s(d->sink, d->ctx.get());
	uint64_t free_clusters = 0;
	void(guarded([this, &free_clusters] () -> int { free_clusters = d->ctx->fat->clsavail(); return 0; }));
	const partition &p = d->ctx->par;
	volume_info i;
	i.label = p.par_label;
	i.serial = p.par_id;
	i.partition_offset = p.par_start;
	i.partition_size = p.par_size;
	i.cluster_size = p.clus_size;
	i.cluster_count = p.clus_fat;
	i.free_clusters = free_clusters;
	i.root_cluster = static_cast<uint32_t>(p.root_clus);
	i.fat_entry_size = p.chain_size;
	i.fat_offset = p.fat_start;
	i.fat_size = p.fat_size;
	i.data_offset = p.root_start;
	i.writable = d->writable;
	i.inconsistent = d->ctx->inconsistent;
	return i;
}

int									volume::		stat(const std::string &path, entry_info &out) {
	return guarded([&] () -> int {
		session s(d->sink, d->ctx.get());
		entry *e = d->ctx->root->find(path.c_str());
		if(e == nullptr)
			return ENOENT;
		out = to_info(*e);
		if(e == d->ctx->root)
			out.name.clear();
		return 0;
	});
}

int									volume::		list(const std::string &path, std::vector<entry_info> &out) {
	return guarded([&] () -> int {
		session s(d->sink, d->ctx.get());
		out.clear();
		entry *e = d->ctx->root->find(path.c_str());
		if(e == nullptr)
			return ENOENT;
		if(!e->flags.dir)
			return ENOTDIR;
		for(const ptr_entry &c: e->childs) {
			if(c->status == entry::valid)
				out.push_back(to_info(*c));
		}
		return 0;
	});
}

int									volume::		read(const std::string &path, uint64_t offset, std::span<std::byte> out, std::size_t *got) {
	return guarded([&] () -> int {
		session s(d->sink, d->ctx.get());
		if(got)
			*got = 0;
		entry *e = d->ctx->root->find(path.c_str());
		if(e == nullptr)
			return ENOENT;
		if(e->flags.dir)
			return EISDIR;
		if(offset >= e->size || out.empty())
			return 0;
		const filesize n = std::min<filesize>(out.size(), e->size - offset);
		int res = e->data(reinterpret_cast<char*>(out.data()), true, offset, n);
		if(res)
			return res;
		if(got)
			*got = static_cast<std::size_t>(n);
		return 0;
	});
}

int									volume::		create(const std::string &path, uint64_t size) {
	return guarded([&] () -> int {
		session s(d->sink, d->ctx.get());
		if(!d->writable)
			return EROFS;
		std::string dir, name;
		if(!split(path, dir, name))
			return EINVAL;
		if(int res = validate_name(name))
			return res;
		if(size > 0xFFFFFFFFULL)
			return EFBIG;		// sizes are 32-bit on disk
		entry *p = d->ctx->root->find(dir.c_str());
		if(p == nullptr)
			return ENOENT;
		if(!p->flags.dir)
			return ENOTDIR;
		if(child(p, name, true) != nullptr)
			return EEXIST;
		entry *n = new entry(name, size, false);
		if(size != 0 && (n->cluster == 0 || n->cluster == FLK || n->size != size)) {
			discard(n);
			return ENOSPC;
		}
		if(int res = p->addtodir(n)) {
			discard(n);
			return res == EFAULT ? EIO : res;
		}
		return 0;
	});
}

int									volume::		write(const std::string &path, uint64_t offset, std::span<const std::byte> data) {
	return guarded([&] () -> int {
		session s(d->sink, d->ctx.get());
		if(!d->writable)
			return EROFS;
		entry *e = d->ctx->root->find(path.c_str());
		if(e == nullptr)
			return ENOENT;
		if(e->flags.dir)
			return EISDIR;
		if(data.empty())
			return 0;
		if(offset + data.size() > 0xFFFFFFFFULL)
			return EFBIG;
		if(int res = load_areas(e))
			return res;
		// entry::data() takes a non-const buffer but does not modify it on write
		return e->data(const_cast<char*>(reinterpret_cast<const char*>(data.data())), false, offset, data.size());
	});
}

int									volume::		truncate(const std::string &path, uint64_t size) {
	return guarded([&] () -> int {
		session s(d->sink, d->ctx.get());
		if(!d->writable)
			return EROFS;
		entry *e = d->ctx->root->find(path.c_str());
		if(e == nullptr)
			return ENOENT;
		if(e->flags.dir)
			return EISDIR;
		if(size > 0xFFFFFFFFULL)
			return EFBIG;
		if(int res = load_areas(e))
			return res;
		return e->resize(size);
	});
}

int									volume::		mkdir(const std::string &path) {
	return guarded([&] () -> int {
		session s(d->sink, d->ctx.get());
		if(!d->writable)
			return EROFS;
		std::string dir, name;
		if(!split(path, dir, name))
			return EINVAL;
		if(int res = validate_name(name))
			return res;
		entry *p = d->ctx->root->find(dir.c_str());
		if(p == nullptr)
			return ENOENT;
		if(!p->flags.dir)
			return ENOTDIR;
		if(child(p, name, true) != nullptr)
			return EEXIST;
		entry *n = new entry(name, 0, true);
		if(n->cluster == 0 || n->cluster == FLK) {
			discard(n);
			return ENOSPC;
		}
		if(int res = p->addtodir(n)) {
			discard(n);
			return res == EFAULT ? EIO : res;
		}
		return 0;
	});
}

int									volume::		rename(const std::string &from, const std::string &to, bool replace) {
	return guarded([&] () -> int {
		session s(d->sink, d->ctx.get());
		if(!d->writable)
			return EROFS;
		entry *e = d->ctx->root->find(from.c_str());
		if(e == nullptr)
			return ENOENT;
		if(e == d->ctx->root)
			return EINVAL;
		if(e->flags.lab)
			return EPERM;
		std::string dir, name;
		if(!split(to, dir, name))
			return EINVAL;
		if(int res = validate_name(name))
			return res;
		entry *p = d->ctx->root->find(dir.c_str());
		if(p == nullptr)
			return ENOENT;
		if(!p->flags.dir)
			return ENOTDIR;
		for(entry *a = p; a != nullptr && a != d->ctx->root; a = a->parent) {
			if(a == e)
				return EINVAL;		// into itself
		}
		if(entry *existing = child(p, name, true); existing != nullptr && existing != e) {
			if(!replace || existing->flags.dir || e->flags.dir || existing->flags.lab)
				return EEXIST;
			entry *parent = existing->parent;
			parent->remfrdir(existing);
			if(std::ranges::any_of(parent->childs, [existing] (const ptr_entry &c) noexcept { return c.get() == existing; }))
				return EIO;
		}
		const std::string target = (dir == sepdir ? std::string() : dir) + sepdir + name;
		int res = e->rename(target.c_str());
		if(res)
			return res == EFAULT ? EIO : res;
		return std::string(e->name) == name ? 0 : EIO;
	});
}

int									volume::		remove(const std::string &path) {
	return guarded([&] () -> int {
		session s(d->sink, d->ctx.get());
		if(!d->writable)
			return EROFS;
		entry *e = d->ctx->root->find(path.c_str());
		if(e == nullptr)
			return ENOENT;
		if(e == d->ctx->root)
			return EINVAL;
		if(has_label_entry(e))
			return EPERM;		// the volume label file; entry::remfrdir() refuses it
		entry *parent = e->parent;
		parent->remfrdir(e);
		if(std::ranges::any_of(parent->childs, [e] (const ptr_entry &c) noexcept { return c.get() == e; }))
			return EIO;
		return 0;
	});
}

int									volume::		set_label(const std::string &label) {
	return guarded([&] () -> int {
		session s(d->sink, d->ctx.get());
		if(!d->writable)
			return EROFS;
		if(label.size() > name_size)
			return ENAMETOOLONG;
		if(!std::ranges::all_of(label, [] (char c) noexcept { return c >= ' ' && c <= '~'; }))
			return EINVAL;
		return actions::write_label(label);
	});
}

int									volume::		flush() {
	return guarded([&] () -> int {
		session s(d->sink, d->ctx.get());
		return d->ctx->dev.sync();
	});
}

// --- tools ------------------------------------------------------------------

int									format(std::shared_ptr<io_backend> device, const location &where,
										const std::string &label, uint32_t cluster_blocks, log_sink sink) {
	if(label.size() > name_size || !std::ranges::all_of(label, [] (char c) noexcept { return c >= ' ' && c <= '~'; }))
		return EINVAL;
	if(cluster_blocks != 0 && (cluster_blocks & (cluster_blocks - 1)) != 0)
		return EINVAL;
	frontend m(1, prog_argv);
	if(int res = configure(m, where, device))
		return res;
	if(!device->writable())
		return EROFS;
	m.prog = frontend::mkfs;
	m.dialog = false;
	m.clus_size = cluster_blocks;
	m.set_readonly(false);
	line_sink lines(std::move(sink));
	const console::sink_t cs = lines.sink();
	session s(cs, nullptr);
	return guarded([&] () -> int {
		fatx_context ctx(m);
		int res = ctx.setup();
		if(res == 0)
			res = actions::make_filesystem();
		if(res == 0)
			res = actions::write_label(label.empty() ? std::string(def_label) : label);
		if(int r = ctx.dev.sync(); res == 0)
			res = r;
		return res;
	});
}

int									check(std::shared_ptr<io_backend> device, const location &where,
										bool repair, check_report &report, log_sink sink) {
	report = check_report();
	frontend m(1, prog_argv);
	if(int res = configure(m, where, device))
		return res;
	if(repair && !device->writable())
		return EROFS;
	m.prog = frontend::fsck;
	m.dialog = false;
	m.force_n = !repair;
	m.force_a = repair;
	m.set_readonly(!repair);
	line_sink lines(std::move(sink), [&report] (const std::string &l) { report.messages.push_back(l); });
	const console::sink_t cs = lines.sink();
	session s(cs, nullptr);
	const int res = guarded([&] () -> int {
		fatx_context ctx(m);
		int r = ctx.setup();
		if(r == 0 && ctx.root == nullptr)
			r = EIO;
		if(r == 0) {
			ctx.root->analyse(entry::findfile);
			ctx.fat->fatlost();
			ctx.fat->fatcheck();
		}
		report.repaired = repair && ctx.dev.modified();
		if(int sr = ctx.dev.sync(); r == 0)
			r = sr;
		return r;
	});
	report.problems = m.asked;
	lines.finish();
	return res;
}

}
