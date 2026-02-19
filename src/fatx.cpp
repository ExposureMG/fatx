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

/*
 *	Compile with:
 *	-D NDEBUG		to avoid produce debug informations output
 *	-D DBG_INIT		to produce debug on initialisation sequence
 *	-D DBG_READ		to print bytes read at device level
 *	-D DBG_WRITE	to print bytes written at device level
 *	-D DBG_DIFF		to print accesses to diff file
 *	-D DBG_SEM		to print accesses to semaphores
 *	-D DBGSEM=\"x\"	to print only semaphore named x
 *	-D DBG_BUFFER	to print buffer operations
 *	-D DBGBUFDMP=x	to print x bytes of buffer at each change
 *	-D DBG_CACHE	to print cache operations
 *	-D DBG_CACHDMP	to dump cache at each change
 *	-D DBG_AREAS	to print fat areas()
 *	-D DBG_GUESS	to print guesses
 *	-D DBGCR=x		to limit to x bytes per line
 *	-D DBGLIMIT=x	to limit to x bytes the printing of read/write
 *	-D DBG_FAT		to print the FAT
 *	-D DBG_GAPS		to print gaps
 *	-D NO_WRITE		to fake writing but no modification is done
 *	-D NO_CACHE		to disable FAT cache
 *
 *	Make symlink to executable with names:
 *	"fusefatx"		for fuse filesystem support
 *	"mkfs.fatx"		for filesystem creation
 *	"fsck.fatx"		for filesystem check and repair
 *	"unrm.fatx"		for recovery of deleted files
 *	"label.fatx"	for display or change volume label
 *
 *	Use -h option for each symlink call to find syntax and options list
 */

#include "context.hpp"

#include <iostream>
#include <vector>
#include <map>
#include <set>
#include <bitset>
#include <shared_mutex>
#include <mutex>
#include <cstring>

int main(int argc, char *argv[]) {
	int err = 0;
	frontend mmi(argc, argv);
	if((err = mmi.setup()))
		return err;
	if(mmi.prog != frontend::unknown && mmi.prog != frontend::label)
		console::write("Analysing filesystem, please wait.\n");
	if(mmi.prog != frontend::unknown) {
		fatx_context::set(new fatx_context(mmi));
		if((err = fatx_context::get()->setup())) {
			delete fatx_context::get();
			return err;
		}
	}
	if(mmi.prog == frontend::fsck || (mmi.prog != frontend::unknown && mmi.recover)) {
		console::write("Finding all files and directories.\n");
		fatx_context::get()->root->analyse(entry::findfile);
		#if !defined NDEBUG && defined DBG_FAT
			fatx_context::get()->fat->printfat();
		#endif
	}
	if(mmi.prog != frontend::unknown && mmi.recover) {
		console::write("Finding all deleted files and directories.\n");
		fatx_context::get()->fat->fatlost();
		fatx_context::get()->root->analyse(entry::finddel);
		#if !defined NDEBUG && defined DBG_FAT
			fatx_context::get()->fat->printfat();
		#endif
	}
	if(mmi.prog != frontend::mkfs && !mmi.script.empty()) {
		fatx_context::get()->fat->gapcheck();
		mmi.parser();
	}
	if(mmi.prog == frontend::unrm) {
		console::write("Trying to recover deleted files and directories.\n");
		if(!mmi.local)
			fatx_context::get()->fat->gapcheck();
		fatx_context::get()->root->analyse(entry::tryrecov);
	}
	if(!mmi.nofat && (mmi.prog == frontend::fsck || mmi.prog == frontend::unrm)) {
		console::write("Checking FAT consistency.\n");
		fatx_context::get()->fat->fatlost();
		fatx_context::get()->fat->fatcheck();
	}
	if(mmi.prog == frontend::fuse) {
		int fuse_argc = 0;
		char *fuse_argv[max_fuse_args];
		std::vector<std::unique_ptr<std::string>> vp;
		fuse_argv[fuse_argc++] = &mmi.progname[0];
		fuse_argv[fuse_argc++] = &mmi.mount[0];
		if(!mmi.writeable()) {
			vp.push_back(std::make_unique<std::string>("-o"));
			fuse_argv[fuse_argc++] = vp.back().get()->data();
			vp.push_back(std::make_unique<std::string>("ro"));
			fuse_argv[fuse_argc++] = vp.back().get()->data();
		}
		if(mmi.fuse_debug) {
			vp.push_back(std::make_unique<std::string>("-d"));
			fuse_argv[fuse_argc++] = vp.back().get()->data();
		}
		if(mmi.fuse_foregrd) {
			vp.push_back(std::make_unique<std::string>("-f"));
			fuse_argv[fuse_argc++] = vp.back().get()->data();
		}
		if(mmi.fuse_singlethr) {
			vp.push_back(std::make_unique<std::string>("-s"));
			fuse_argv[fuse_argc++] = vp.back().get()->data();
		}
		if(!mmi.fuse_option.empty()) {
			vp.push_back(std::make_unique<std::string>("-o"));
			fuse_argv[fuse_argc++] = vp.back().get()->data();
			fuse_argv[fuse_argc++] = &mmi.fuse_option[0];
		}
		for(const std::string& i: mmi.unkopt) {
			if(fuse_argc < (max_fuse_args - 1)) {
				vp.push_back(std::make_unique<std::string>(i));
				fuse_argv[fuse_argc++] = vp.back().get()->data();
			}
			else {
				console::write("Too many arguments for fuse. Ignoring last arguments.\n", true);
				break;
			}
		}
		fuse_argv[fuse_argc] = nullptr;
		if(!mmi.recover)
			fatx_context::get()->fat->gapcheck();
		std::memset(&fatx_ops, 0, sizeof(fatx_ops));
		fatx_ops_init();
		#ifndef NDEBUG
			std::string s;
			for(int i = 0; i < fuse_argc; i++)
				s += std::string(" ") + fuse_argv[i];
			dbglog("Fuse called with{}\n:", s);
		#endif
		console::write("Ready.\n");
		fatx_context::get()->ready = true;

		err = fuse_main(fuse_argc, fuse_argv, &fatx_ops, nullptr);
		#ifndef NDEBUG
			dbglog("Fuse returned: {}", err);
		#endif
	}
	bool answ = false;
	if(mmi.prog == frontend::mkfs) {
		console::write("Are you sure you want to erase all data in {} ?", mmi.input);
		if((answ = mmi.getanswer(false))) {
			bool status = false;
			console::write("Creating new FATX filesystem");
			status = status || fatx_context::get()->par.write();
			console::write(".");
			fatx_context::get()->fat->erase();
			console::write(".");
			status = status || fatx_context::get()->dev.write(fatx_context::get()->par.root_start, std::string(fatx_context::get()->par.clus_size, '\0'));
			console::write(".");
			if(!status) {
				fatx_context::get()->root = new entry("", 0, true);
				fatx_context::get()->root->parent = fatx_context::get()->root;
				fatx_context::get()->root->status = entry::valid;
				console::write("done.\n");
				console::write("FATX filesystem created with {} clusters.\n", fatx_context::get()->par.clus_fat);
			}
			else
				console::write("Unable to create FATX filesystem.\n");
			if(mmi.volname.empty())
				mmi.volname = def_label;
		}
	}
	if((mmi.prog == frontend::mkfs && answ) || (mmi.prog == frontend::label && !mmi.volname.empty())) {
		if(mmi.prog == frontend::label)
			fatx_context::get()->fat->gapcheck();
		fatx_context::get()->par.par_label = fatx_context::get()->mmi.volname;
		unsigned char lab[slab];
		entry *idx = fatx_context::get()->root->find(flab);
		filesize s = fatx_context::get()->par.label(lab);
		bool status = false;
		if(idx == nullptr) {
			status = status || fatx_context::get()->root->addtodir(new entry(flab, 0, false));
			idx = fatx_context::get()->root->find(flab);
			assert(idx != nullptr);
			idx->flags.lab = true;
			status = status || idx->write(true);
		}
		if(!status && !idx->resize(s) && !idx->data(reinterpret_cast<char*>(lab), false, 0, s))
			console::write("Volume label has been changed to {}\n", fatx_context::get()->par.par_label);
		else
			console::write("Unable to change volume label.\n");
	}
	if(mmi.prog == frontend::fsck) {
		if(fatx_context::get()->par.par_label.empty())
			console::write("Warning: volume has no label.\n");
		if(mmi.verbose) {
			console::write(
					"Volume label:\t{}\n"
					"Clusters size:\t{}\n"
					"Total clusters:\t{}\n"
					"Clusters free:\t{}\n"
				,
				fatx_context::get()->par.par_label.empty() ? "none" : fatx_context::get()->par.par_label,
				fatx_context::get()->par.clus_size,
				fatx_context::get()->par.clus_fat,
				fatx_context::get()->fat->clsavail()
			);
		}
	}
	if(mmi.prog == frontend::label && mmi.volname.empty())
		console::write((fatx_context::get()->par.par_label.empty() ? "No volume label." : fatx_context::get()->par.par_label) + "\n");
	if(mmi.verbose) {
		if(fatx_context::get()->dev.modified())
			console::write("Changes have been made.\n");
		else
			console::write("No change has been made.\n");
	}
	if(err != 0)
		err = mmi.prog != frontend::fsck ? code_noerr : !fatx_context::get()->dev.modified() ? code_noerr : fatx_context::get()->mmi.allyes ? code_corrd : code_ncorr;
	delete fatx_context::get();
	#ifndef NDEBUG
		dbglog("<= ENDC: {}", mmi.name());
	#endif
	return err;
}
