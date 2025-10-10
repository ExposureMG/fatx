/*
 *	FATX filesystem support (Xbox 360)
 *
 *  Copyright (C) 2012-2025 Christophe Duverger
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

#include <boost/program_options.hpp>
#include <boost/tokenizer.hpp>

// Implémentation des méthodes de la classe frontend

							frontend::		frontend(int ac, const char *const * const av) :
	readonly(false),		prog(unknown),			force_y(false),			force_n(false),			force_a(false),
	verbose(false),			recover(false),			local(false),			deldate(true),			dellost(true),
	fuse_debug(false),		fuse_foregrd(false),	fuse_singlethr(false),	nofat(false),			cutname(false),
	argc(ac),				argv(av),				progname(av[0]),		dialog(true),			lostfound(def_landf),
	foundfile(def_fpre),	filecount(0),			mount(),				volname(),				fuse_option(),
	unkopt(),				partition("x2"),		table("hd"),			clus_size(0),			uid(getuid()),
	gid(getgid()),
	mask(
		S_IRUSR | S_IWUSR | S_IXUSR |
		S_IRGRP | S_IXGRP |
		S_IROTH | S_IXOTH
	), allyes(true), offset(0), size(0), input(), script(), diffile() {
}
bool						frontend::		getanswer(bool def) {
	bool res = false;
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
int							frontend::		setup() {
	size_t pos;
	if((pos = progname.rfind(sepdir, progname.size())) != std::string::npos)
		progname = progname.substr(pos + 1);
	if(progname.compare("fusefatx") == 0)
		prog	= fuse;
	if(progname.compare("mkfs.fatx") == 0) {
		prog	= mkfs;
		dialog	= false;
	}
	if(progname.compare("fsck.fatx") == 0) {
		prog	= fsck;
		dialog	= false;
	}
	if(progname.compare("unrm.fatx") == 0) {
		prog	= unrm;
		recover	= true;
		dialog	= false;
	}
	if(progname.compare("label.fatx") == 0) {
		prog	= label;
		dialog	= false;
	}
	boost::program_options::options_description hidden;
	boost::program_options::variables_map varmap;
	hidden.add_options()
		("default", "display options default values")
		("as", boost::program_options::value<std::string>(),
			"choose program behavior\n"
			"\"fuse\"  for fusefatx,\n"
			"\"fsck\"  for fsck.fatx,\n"
			"\"mkfs\"  for mkfs.fatx,\n"
			"\"unrm\"  for unrm.fatx,\n"
			"\"label\" for label.fatx"
		)
		("do", boost::program_options::value<std::string>(), "send a script to the program")
	;
	std::vector<std::string> visopt;
	try {
		boost::program_options::parsed_options parsed(boost::program_options::command_line_parser(argc, argv)
			.options(hidden)
			.allow_unregistered()
			.run()
		);
		store(parsed, varmap);
		visopt = collect_unrecognized(parsed.options, boost::program_options::include_positional);
		notify(varmap);
	}
	catch(std::exception& e) {
		std::ostringstream s;
		s << e.what();
		console::write(s.str() + "\n");
		prog = unknown;
	}
	if(varmap.count("as")) {
		if(varmap["as"].as<std::string>() == "fuse")
			prog = fuse;
		if(varmap["as"].as<std::string>() == "mkfs")
			prog = mkfs;
		if(varmap["as"].as<std::string>() == "fsck")
			prog = fsck;
		if(varmap["as"].as<std::string>() == "unrm")
			prog = unrm;
		if(varmap["as"].as<std::string>() == "label")
			prog = label;
	}
	if(varmap.count("do"))
		script = varmap["do"].as<std::string>();
	if(prog == mkfs || prog == fsck || prog == unrm || prog == label)
		dialog	= false;
	if(prog == unrm)
		recover	= true;
	#ifndef NDEBUG
		dbglog("=> CALL: {}", name());
	#endif
	if(prog != fuse && prog != fsck && prog != mkfs && prog != unrm && prog != label) {
		console::write(
			"Invalid usage.\n"
			"Please use a link to this executable named:\n"
			"- fusefatx\tto mount a filesystem with fuse\n"
			"- mkfs.fatx\tto create a new filesystem\n"
			"- fsck.fatx\tto check a filesystem\n"
			"- unrm.fatx\tto try to recover deleted files\n"
			"- label.fatx\tto display or change a filesystem label\n"
		);
		return EINVAL;
	}
	boost::program_options::options_description visible(std::format("Usage: {} [options] device{}",
		name(),
		(prog == fuse ? " mountpoint" : (prog == label ? " [label]" : ""))
	));
	visible.add_options()
		("help,h", "display this help message")
		("version", "display the version number")
		("verbose,v", "verbose output")
		("input,i", boost::program_options::value<std::string>(), "set input device/file")
		("diff", boost::program_options::value<std::string>(), "set separate write file")
		("offset", boost::program_options::value<streamptr>(), "force partition offset")
		("size", boost::program_options::value<streamptr>(), "force partition size")
		("partition,p", boost::program_options::value<std::string>()->default_value("x2"),
			"select partition:\n"
			"\"sc\"  for system cache partition,\n"
			"\"gc\"  for game cache partition,\n"
			"\"se1\" for sysext partition,\n"
			"\"se2\" for sysext2 partition,\n"
			"\"xdv\" for Xbox 360 dashboard partition,\n"
			"\"x1\"  for original Xbox compatibility partition,\n"
			"\"x2\"  for data partition (default)"
		)
	;
	if(prog == mkfs) {
		visible.add_options()
			("table,b", boost::program_options::value<std::string>()->default_value("hd"),
				"select partition table:\n"
				"\"file\" for plain file,\n"
				"\"mu\"   for memory unit,\n"
				"\"hd\"   for Xbox 360 retail hard disk (default),\n"
				"\"kit\"  for devkit hard disk"
			 )
		;
	}
	else {
		visible.add_options()
			("table,b", boost::program_options::value<std::string>()->default_value("hd"),
				"select partition table:\n"
				"\"file\" for plain file,\n"
				"\"mu\"   for memory unit,\n"
				"\"usb\"  for USB drive,\n"
				"\"hd\"   for Xbox 360 retail hard disk (default),\n"
				"\"kit\"  for devkit hard disk"
			 )
		;
	}
	if(prog == fuse) {
		visible.add_options()
			("mount,m", boost::program_options::value<std::string>(), "set mountpoint")
			("recover,r", "mount with deleted files")
			("option,o", boost::program_options::value<std::string>(), "mount options")
			("cutname,c", "enable use of long names")
			("debug,d", "enable debug output (implies -f)")
			("foregrd,f", "foreground operation")
			("singlethr,s", "fuse on single thread")
			("uid",  boost::program_options::value<uid_t>(), "sets uid of the filesystem")
			("gid",  boost::program_options::value<gid_t>(), "sets gid of the filesystem")
			("mask",  boost::program_options::value<std::string>(), "sets mask for entries modes")
		;
	}
	if(prog == label || prog == mkfs) {
		visible.add_options()
			("label,l", boost::program_options::value<std::string>(), "set volume label")
		;
	}
	if(prog == mkfs) {
		visible.add_options()
			("cls-size,c", boost::program_options::value<streamptr>(), "set num of blocks per cluster")
		;
	}
	if(prog == fsck || prog == unrm || prog == mkfs) {
		visible.add_options()
			("yes,y", "answer yes to everything")
			("no,n", "answer no to everything")
			("auto,a", "default answer to everything")
		;
	}
	if(prog == fsck || prog == unrm || prog == mkfs || prog == fuse) {
		visible.add_options()
			("test,t", "test mode, no modification done")
		;
	}
	if(prog == unrm) {
		visible.add_options()
			("local,l", "recover files in local filesystem")
		;
	}
	if(prog == fsck || prog == unrm) {
		visible.add_options()
			("nofat,f", "disable FAT sanity check and recovery")
		;
	}
	if(prog == fuse || prog == unrm) {
		visible.add_options()
			("nodate", "disable dates precedence of deleted files")
			("nolost", "disable preservation of lost chains")
		;
	}
	boost::program_options::positional_options_description	podesc;
	podesc.add("input", 1);
	if(prog == fuse)
		podesc.add("mount", 1);
	if(prog == label)
		podesc.add("label", 1);
	try {
		if(prog == fuse) {
			boost::program_options::parsed_options parsed(boost::program_options::command_line_parser(visopt)
				.options(visible)
				.positional(podesc)
				.allow_unregistered()
				.run()
			);
			store(parsed, varmap);
			unkopt = collect_unrecognized(parsed.options, boost::program_options::exclude_positional);
		}
		else {
			boost::program_options::parsed_options parsed(boost::program_options::command_line_parser(visopt)
				.options(visible)
				.positional(podesc)
				.run()
			);
			store(parsed, varmap);
		}
		notify(varmap);
	}
	catch(std::exception& e) {
		std::ostringstream s;
		s << e.what();
		console::write(s.str() + "\n");
		prog = unknown;
	}
	if(varmap.count("yes"))
		force_y		= true;
	if(varmap.count("no"))
		force_n		= true;
	if(varmap.count("auto"))
		force_a		= true;
	if(varmap.count("test"))
		readonly	= true;
	if(varmap.count("verbose"))
		verbose		= true;
	if(varmap.count("cutname"))
		cutname		= true;
	if(varmap.count("recover")) {
		recover		= true;
		readonly	= true;
	}
	if(varmap.count("local")) {
		local		= true;
		readonly	= true;
	}
	if(varmap.count("nofat")) {
		nofat		= true;
	}
	if(varmap.count("nodate")) {
		deldate		= false;
		if(prog == fuse) {
			recover		= true;
			readonly	= true;
		}
	}
	if(varmap.count("nolost")) {
		dellost		= false;
		if(prog == fuse) {
			recover		= true;
			readonly	= true;
		}
	}
	if(varmap.count("debug")) {
		fuse_debug		= true;
		fuse_foregrd	= true;
	}
	if(varmap.count("singlethr"))
		fuse_singlethr	= true;
	if(varmap.count("foregrd"))
		fuse_foregrd	= true;
	if(varmap.count("mount"))
		mount			= varmap["mount"].as<std::string>();
	if(varmap.count("label"))
		volname			= varmap["label"].as<std::string>();
	if(varmap.count("partition"))
		partition		= varmap["partition"].as<std::string>();
	if(varmap.count("table"))
		table			= varmap["table"].as<std::string>();
	if(varmap.count("cls-size"))
		clus_size		= varmap["cls-size"].as<streamptr>();
	if(varmap.count("uid"))
		uid				= varmap["uid"].as<uid_t>();
	if(varmap.count("gid"))
		gid				= varmap["gid"].as<gid_t>();
	if(varmap.count("mask")) {
		std::stringstream ss;
		ss << std::oct << varmap["mask"].as<std::string>();
		ss >> mask;
	}
	if(varmap.count("offset"))
		offset			= varmap["offset"].as<streamptr>();
	if(varmap.count("size"))
		size			= varmap["size"].as<streamptr>();
	if(varmap.count("input"))
		input			= varmap["input"].as<std::string>();
	if(varmap.count("diff"))
		diffile			= varmap["diff"].as<std::string>();
	if(prog == label)
		readonly		= !varmap.count("label");
	if(varmap.count("option")) {
		boost::tokenizer<boost::char_separator<char> > opts(varmap["option"].as<std::string>(), boost::char_separator<char>(","));
		for(const std::string& o: opts) {
			if(o == "ro")
				readonly = true;
			else {
				if(!fuse_option.empty())
					fuse_option += ",";
				fuse_option += o;
			}
		}
	}
	if(varmap.count("version")) {
		console::write("{} v{}.\nCopyright (C) 2012 - 2023 Christophe Duverger.\n\n",
			name(),
			PACKAGE_VERSION
		);
		console::write(
			"This program comes with ABSOLUTELY NO WARRANTY.\n"
			"This is free software, and you are welcome to redistribute it\n"
			"under certain conditions.\n"
		);
		prog = unknown;
		return 0;
	}
	if(varmap.count("default")) {
		console::write(
			std::format("diffile\t\t{}\n",			diffile.empty() ? "none" : diffile) +
			std::format("force yes\t{}\n",			force_y ? "yes" : "no") +
			std::format("force no\t{}\n",			force_n ? "yes" : "no") +
			std::format("force auto\t{}\n",			force_a ? "yes" : "no") +
			std::format("verbose\t\t{}\n",			verbose ? "yes" : "no") +
			std::format("read only\t{}\n",			readonly ? "yes" : "no") +
			std::format("fuse recover\t{}\n",		recover ? "yes" : "no") +
			std::format("local recover\t{}\n",		local ? "yes" : "no") +
			std::format("deleted dates\t{}\n",		deldate ? "yes" : "no") +
			std::format("preserve losts\t{}\n",		dellost ? "yes" : "no") +
			std::format("ignore fat\t{}\n",			nofat ? "yes" : "no") +
			std::format("cut long names\t{}\n",		cutname ? "yes" : "no") +
			std::format("partition\t{}\n",			partition) +
			std::format("table\t\t{}\n",				table.empty() ? "file" : table) +
			std::format("offset\t\t{}\n",			offset) +
			std::format("size\t\t{}\n",				size) +
			std::format("cluster size\t{}\n",		clus_size) +
			std::format("fuse debug\t{}\n",			fuse_debug ? "yes" : "no") +
			std::format("fuse foregrd\t{}\n",		fuse_foregrd ? "yes" : "no") +
			std::format("fuse singlethr\t{}\n",		fuse_singlethr ? "yes" : "no") +
			std::format("uid\t\t{}\n",				uid) +
			std::format("gid\t\t{}\n",				gid) +
			std::format("mask\t\t{:03o}\n",			mask)
		);
		return EPERM;
	}
	if(
		partition != "sc" &&
		partition != "gc" &&
		partition != "se1" &&
		partition != "se2" &&
		partition != "xdv" &&
		partition != "x1" &&
		partition != "x2"
	) {
		console::write("Bad partition type.\n");
		return EINVAL;
	}
	if(
		table != "file" &&
		table != "mu" &&
		table != "hd" &&
		(prog == mkfs || table != "usb") &&
		table != "kit"
	) {
		console::write("Bad partition table type.\n");
		return EINVAL;
	}
	if(varmap.count("help") || !varmap.count("input") || (prog == fuse && !varmap.count("mount"))) {
		std::ostringstream s;
		s << visible;
		console::write(s.str());
		return EINVAL;
	}
	#if !defined NDEBUG && defined DBG_INIT
		dbglog("::EOMMI");
	#endif
	return 0;
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
