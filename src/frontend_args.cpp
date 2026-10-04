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

// Command-line parsing for the fatx tools (needs Boost.Program_options).
// Kept out of fatx_core so that embedding applications do not need it.

#include "context.hpp"

#include <boost/program_options.hpp>
#include <boost/tokenizer.hpp>

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
			("recover,R", "mount with deleted files")
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
		console::write("{} v{}.\nCopyright (C) 2012 - 2026 Christophe Duverger.\n\n",
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
