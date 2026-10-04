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

#include <fstream>

#include <boost/lexical_cast.hpp>
#include <boost/tokenizer.hpp>

// Implémentation des méthodes de la classe frontend

							frontend::		frontend(int ac, const char *const * const av) :
	readonly(false),		prog(unknown),			force_y(false),			force_n(false),			force_a(false),
	verbose(false),			recover(false),			local(false),			deldate(true),			dellost(true),
	fuse_debug(false),		fuse_foregrd(false),	fuse_singlethr(false),	nofat(false),			cutname(false),
	argc(ac),				argv(av),				progname(av[0]),		dialog(true),			lostfound(def_landf),
	foundfile(def_fpre),	filecount(0),			mount(),				volname(),				fuse_option(),
	unkopt(),				partition("x2"),		table("hd"),			clus_size(0),			uid(fatx_getuid()),
	gid(fatx_getgid()),
	mask(
		S_IRUSR | S_IWUSR | S_IXUSR |
		S_IRGRP | S_IXGRP |
		S_IROTH | S_IXOTH
	), allyes(true), offset(0), size(0), input(), script(), diffile(), backend(), asked(0) {
}
bool						frontend::		getanswer(bool def) {
	bool res = false;
	asked++;
	console::write(def ? " [Y/n] :" : " [y/N] :");
	if(force_n) {
		console::write("n\n");
		res = false;
	}
	else if(force_y) {
		console::write("y\n");
		res = true;
	}
	else if(force_a) {
		console::write(def ? "y\n" : "n\n");
		res = def;
	}
	else {
		std::pair<bool, bool> r = console::read();
		res = r.first ? r.second : def;
	}
	allyes = allyes && res;
	return res;
}
std::string					frontend::		name() const {
	return
		prog == fuse ? "fusefatx" :
		prog == mkfs ? "mkfs.fatx" :
		prog == fsck ? "fsck.fatx" :
		prog == unrm ? "unrm.fatx" :
		prog == label ? "label.fatx" :
		"fatx"
	;
}
void						frontend::		parser() {
	script.erase(remove_if(script.begin(), script.end(), [] (const char c) ->bool { return c == ' ' || c == '\t' || c == '\n'; }), script.end());
	boost::tokenizer<boost::char_separator<char> > cmds(script, boost::char_separator<char>(";"));
	for (const std::string &cmd : cmds) {
		boost::tokenizer<boost::char_separator<char> > args(cmd, boost::char_separator<char>(","));
		boost::tokenizer<boost::char_separator<char> >::iterator i = args.begin();
		if(i->empty())
			break;
		else if((*i)[0] == '#')
			continue;
		else if(*i == "mkdir" && ++i != args.end() && !i->empty()) {
			console::write("mkdir:");
			if(!writeable()) {
				console::write("read-only\n");
				continue;
			}
			size_t l = i->find_last_of(sepdir);
			if(l == std::string::npos || l == i->length() - 1) {
				console::write("syntax error\n");
				continue;
			}
			entry *s = fatx_context::get()->root->find(i->substr(0, l).data());
			if(s == nullptr) {
				console::write("not found\n");
				continue;
			}
			entry *n = new entry(i->substr(l + 1), 0, true);
			if(s->addtodir(n)) {
				delete n;
				console::write("failed\n");
				continue;
			}
			console::write(n->path() + "\n");
		}
		else if(*i == "rmdir" && ++i != args.end() && !i->empty()) {
			console::write("rmdir:");
			if(!writeable()) {
				console::write("read-only\n");
				continue;
			}
			entry *n = fatx_context::get()->root->find(i->data());
			if(n == nullptr || !n->flags.dir) {
				console::write("not found\n");
				continue;
			}
			if(n->childs.size() != 0) {
				console::write("not empty\n");
				continue;
			}
			console::write(n->path() + "\n");
			n->parent->remfrdir(n);
		}
		else if(*i == "cp" && ++i != args.end() && !i->empty()) {
			console::write("cp:");
			if(!writeable()) {
				console::write("read-only\n");
				continue;
			}
			entry *s = fatx_context::get()->root->find(i++->data());
			if(s == nullptr) {
				console::write("not found\n");
				continue;
			}
			size_t l = i->find_last_of(sepdir);
			if(l == std::string::npos || l == i->length() - 1) {
				console::write("syntax error\n");
				continue;
			}
			entry *d = fatx_context::get()->root->find(i->substr(0, l).data());
			if(d == nullptr) {
				console::write("not found\n");
				continue;
			}
			entry *n = new entry(i->substr(l + 1), s->size);
			if(d->addtodir(n)) {
				delete n;
				console::write("*ERR*\n");
				continue;
			}
			std::string b(s->size, '\0');
			if(s->data(&b[0], true, 0, s->size)) {
				console::write("*ERR*\n");
				continue;
			}
			if(n->data(&b[0], false, 0, n->size)) {
				console::write("*ERR*\n");
				continue;
			}
			console::write(n->path() + "\n");
		}
		else if(*i == "rcp" && ++i != args.end() && !i->empty()) {
			console::write("rcp:");
			std::ifstream s(i++->data(), std::ios::binary);
			if(!s) {
				console::write("can't open\n");
				continue;
			}
			s.seekg(0, std::ios::end);
			streamptr siz = static_cast<streamptr>(s.tellg());
			s.seekg(0);
			size_t l = i->find_last_of(sepdir);
			if(l == std::string::npos || l == i->length() - 1) {
				console::write("syntax error\n");
				continue;
			}
			entry *d = fatx_context::get()->root->find(i->substr(0, l).data());
			if(d == nullptr) {
				console::write("not found\n");
				continue;
			}
			entry *n = new entry(i->substr(l + 1), siz);
			if(d->addtodir(n)) {
				delete n;
				console::write("*ERR*\n");
				continue;
			}
			std::string b(siz, '\0');
			s.read(&b[0], static_cast<std::streamsize>(siz));
			s.close();
			console::write("({})", siz);
			if(n->data(&b[0], false, 0, n->size)) {
				console::write("*ERR*\n");
				continue;
			}
			console::write(n->path() + "\n");
		}
		else if(*i == "lcp" && ++i != args.end() && !i->empty()) {
			console::write("lcp:");
			entry *s = fatx_context::get()->root->find(i++->data());
			if(s == nullptr) {
				console::write("not found\n");
				continue;
			}
			std::ifstream t;
			t.open(i->data());
			if(t) {
				t.close();
				console::write("local file exists\n");
				continue;
			}
			std::ofstream d(i->data(), std::ios::binary | std::ios::trunc);
			d.seekp(0);
			std::string b(s->size, '\0');
			if(s->data(&b[0], true, 0, s->size)) {
				console::write("*ERR*\n");
				continue;
			}
			console::write("({})", s->size);
			d.write(&b[0], static_cast<std::streamsize>(s->size));
			d.close();
			console::write(*i + "\n");
		}
		else if(*i == "mv" && ++i != args.end() && !i->empty()) {
			console::write("mv:");
			if(!writeable()) {
				console::write("read-only\n");
				continue;
			}
			entry *n = fatx_context::get()->root->find(i++->data());
			if(n == nullptr) {
				console::write("not found\n");
				continue;
			}
			if(n->rename(i->data())) {
				console::write("*ERR*\n");
				continue;
			}
			console::write(n->path() + "\n");
		}
		else if(*i == "rm" && ++i != args.end() && !i->empty()) {
			console::write("rm:");
			if(!writeable()) {
				console::write("read-only\n");
				continue;
			}
			entry *n = fatx_context::get()->root->find(i->data());
			if(n == nullptr || n->flags.dir) {
				console::write("not found\n");
				continue;
			}
			console::write(n->path() + "\n");
			n->parent->remfrdir(n);
		}
		else if(*i == "lsfat" && ++i != args.end() && !i->empty()) {
			console::write(*i + ":");
			const entry *e = fatx_context::get()->root->find(i->data());
			if(e != nullptr)
				console::write(fatx_context::get()->fat->printchain(e->cluster));
			else
				console::write("not found");
			console::write("\n");
		}
		else if(*i == "mklost") {
			clusptr p = 0;
			console::write(*i + ":");
			if(!writeable()) {
				console::write("read-only\n");
				continue;
			}
			while(++i != args.end()) {
				clusptr s = 0;
				clusptr e = 0;
				size_t l = i->find_last_of(":");
				if(l != std::string::npos) {
					try {
						s = boost::lexical_cast<clusptr>(i->substr(0, l));
					}
					catch(boost::bad_lexical_cast &) {
						console::write("*ERR*");
						break;
					}
					try {
						e = boost::lexical_cast<clusptr>(i->substr(l + 1));
					}
					catch(boost::bad_lexical_cast &) {
						console::write("*ERR*");
						break;
					}
				}
				else {
					try {
						s = boost::lexical_cast<clusptr>(*i);
					}
					catch(boost::bad_lexical_cast &) {
						console::write("*ERR*");
						break;
					}
				}
				do {
					if(p != 0) {
						console::write("0x{:08X}->", p);
						if(fatx_context::get()->fat->write(p, s)) {
							console::write("*ERR*");
							break;
						}
					}
					p = s++;
				} while(e != 0 && p != e);
			}
			if(p != 0) {
				console::write("0x{:08X}->EOC", p);
				if(fatx_context::get()->fat->write(p, EOC)) {
					console::write("*ERR*");
					break;
				}
			}
			console::write("\n");
		}
		else if(*i == "rmfat") {
			console::write(*i + ":");
			if(!writeable()) {
				console::write("read-only\n");
				continue;
			}
			while(++i != args.end()) {
				clusptr s = 0;
				clusptr e = 0;
				size_t l = i->find_last_of(":");
				if(l != std::string::npos) {
					try {
						s = boost::lexical_cast<clusptr>(i->substr(0, l));
					}
					catch(boost::bad_lexical_cast &) {
						console::write("*ERR*");
						break;
					}
					try {
						e = boost::lexical_cast<clusptr>(i->substr(l + 1));
					}
					catch(boost::bad_lexical_cast &) {
						console::write("*ERR*");
						break;
					}
				}
				else {
					try {
						s = boost::lexical_cast<clusptr>(*i);
					}
					catch(boost::bad_lexical_cast &) {
						console::write("*ERR*");
						break;
					}
				}
				do {
					console::write("0x{:08X} ", s);
					if(fatx_context::get()->fat->write(s++, FLK)) {
						console::write("*ERR*");
						break;
					}
				} while(e != 0 && s != e);

			}
			console::write("\n");
		}
		else if(*i == "chcls" && ++i != args.end() && !i->empty()) {
			entry *e = fatx_context::get()->root->find(i->data());
			if(e != nullptr) {
				console::write(*i + ":");
				if(!writeable()) {
					console::write("read-only\n");
					continue;
				}
				if(++i != args.end()) {
					clusptr n = 0;
					try {
						n = boost::lexical_cast<clusptr>(*i);
					}
					catch(boost::bad_lexical_cast &) {
						console::write("*ERR*");
						break;
					}
					e->cluster = n;
					if(e->write(true)) {
						console::write("*ERR*");
						break;
					}
					console::write("{}->0x{:08X}", e->path(), e->cluster);
				}
			}
			else
				console::write("not found");
			console::write("\n");
		}
		else if(*i == "help") {
			console::write(
				"syntax: cmd, arg1, arg2, ...[; cmd, arg1, ...[; ...]]\n"
				"\tmkdir,\t/path/to/newdir\n"
				"\trmdir,\t/path/to/dir\n"
				"\tcp,\t/path/to/src, /path/to/dst\n"
				"\trcp,\t/path/to/local/src, /path/to/dst\n"
				"\tlcp,\t/path/to/src, /path/to/local/dst\n"
				"\tmv,\t/path/to/src, /path/to/dst\n"
				"\trm,\t/path/to/file\n"
				"\tlsfat,\t/path/to/file\n"
				"\tmklost,\tclus, start:end, ...\n"
				"\trmfat,\tclus, start:end, ...\n"
				"\tchcls,\t/path/to/file, clus\n"
				"\t#comment, ...\n"
			);
		}
		else
			console::write(*i + ":unknown" + "\n");
	}
}
