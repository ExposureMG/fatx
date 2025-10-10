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

#include <fuse3/fuse.h>
#include <iostream>
#include <vector>
#include <map>
#include <set>
#include <bitset>
#include <shared_mutex>
#include <mutex>

fatx_context*				fatx_context::	fatxc		= nullptr;

static int									fatx_open(const char*, fuse_file_info*);
static int									fatx_read(const char*, char*, size_t, off_t, fuse_file_info*);
static int									fatx_write(const char*, const char*, size_t, off_t, fuse_file_info*);
static int									fatx_flush(const char*, fuse_file_info*);
static int									fatx_close(const char*, fuse_file_info*);
static int									fatx_readdir(const char*, void*, fuse_fill_dir_t, off_t, fuse_file_info*, fuse_readdir_flags);
static int									fatx_create(const char*, mode_t);
static int									fatx_creope(const char*, mode_t, fuse_file_info*);
static int									fatx_remove(const char*);
static int									fatx_rename(const char*, const char*, unsigned int);
static int									fatx_getattr(const char*, struct stat*, fuse_file_info*);
static int									fatx_chmod(const char*, mode_t, fuse_file_info*);
static int									fatx_chown(const char*, uid_t, gid_t, fuse_file_info*);
static int									fatx_truncate(const char*, off_t, fuse_file_info*);
static int									fatx_utimens(const char*, const timespec[2], fuse_file_info*);
static int									fatx_statfs(const char*, struct statvfs*);
static void*								fatx_init(fuse_conn_info*, fuse_config*);
static void									fatx_destroy(void*);

							fatx_context::	fatx_context(frontend& m): mmi(m), fat(nullptr), root(nullptr), ready(false) {
}
							fatx_context::	~fatx_context() {
	ready = false;
	destroy();
	set(nullptr);
}
int							fatx_context::	setup() {
	int res = 0;
	if((res = dev.setup()))
		return res;
	if((res = par.setup()))
		return res;
	if(mmi.prog == frontend::fsck || mmi.prog == frontend::unrm || (mmi.prog == frontend::fuse && mmi.recover))
		fat = new memmap(par);
	else
		fat = new dskmap(par);
	if(fat == nullptr)
		return ENOMEM;
	#if !defined NDEBUG && defined DBG_INIT
		dbglog("::EOMAP");
	#endif
	if(mmi.prog != frontend::mkfs) {
		root = new entry();
		if(root == nullptr)
			return ENOMEM;
		if(mmi.prog == frontend::fuse && ready) {
			console::write("Errors found, please run fsck.fatx to correct.\n", mmi.dialog);
			return ECANCELED;
		}
		else
			ready = false;
		#if !defined NDEBUG && defined DBG_INIT
			dbglog("::EOENT");
		#endif
	}
	return res;
}
void						fatx_context::	destroy() {
	delete root;
	root = nullptr;
	delete fat;
	fat = nullptr;
}

static int									fatx_open		(const char *path, fuse_file_info* fi) {
	#ifndef NDEBUG
		dbglog("OPEN: {} [{}]", path, fi->flags & (S_IWUSR | S_IWGRP | S_IWOTH) ? 'w' : 'r' );
	#endif
	entry *f = fatx_context::get()->root->find(path);
	if(f == nullptr || f->status == entry::invalid)
		return -ENOENT;
	if((fi->flags & (O_WRONLY | O_RDWR)) != 0 && !fatx_context::get()->mmi.writeable())
		return -EROFS;
	if((fi->flags & (O_WRONLY | O_RDWR)) != 0 && f->flags.ro)
		return -EPERM;
	f->open((fi->flags & (O_WRONLY | O_RDWR)) != 0);
	fi->fh = uint64_t(f);
	return 0;
}
static int									fatx_read		(const char *path, char *buf, size_t size, off_t offset, fuse_file_info* fi) {
	#ifndef NDEBUG
		dbglog("READ: {} ({}@{})", path, size, offset);
	#endif
	entry *f = fatx_context::get()->root->find(path);
	if(f == nullptr || f->status == entry::invalid || fi->fh != uint64_t(f))
		return -ENOENT;
	return static_cast<int>(f->bufread(buf, static_cast<filesize>(offset), size));
}
static int									fatx_write		(const char *path, const char *buf, size_t size, off_t offset, fuse_file_info* fi) {
	#ifndef NDEBUG
		dbglog("WRITE: {} ({}@{})", path, size, offset);
	#endif
	entry *f = fatx_context::get()->root->find(path);
	if(f == nullptr || f->status == entry::invalid || fi->fh != uint64_t(f))
		return -ENOENT;
	if(!fatx_context::get()->mmi.writeable())
		return -EROFS;
	if(f->flags.ro)
		return -EACCES;
	return static_cast<int>(f->bufwrite(buf, static_cast<filesize>(offset), size));
}
static int									fatx_flush		(const char *path, fuse_file_info* fi) {
	#ifndef NDEBUG
		dbglog("FLUSH: {}", path);
	#endif
	entry *f = fatx_context::get()->root->find(path);
	if(f == nullptr || f->status == entry::invalid || fi->fh != uint64_t(f))
		return -ENOENT;
	return -f->flush();
}
static int									fatx_close		(const char *path, fuse_file_info* fi) {
	#ifndef NDEBUG
		dbglog("CLOSE: {}", path);
	#endif
	entry *f = fatx_context::get()->root->find(path);
	if(f == nullptr || f->status == entry::invalid || fi->fh != uint64_t(f))
		return -ENOENT;
	f->close((fi->flags & (O_WRONLY | O_RDWR)) != 0);
	fi->fh = 0;
	return 0;
}
static int									fatx_readdir	(const char *path, void* buf, fuse_fill_dir_t ff, off_t, fuse_file_info*fi, fuse_readdir_flags) {
	#ifndef NDEBUG
		dbglog("READDIR: {}", path);
	#endif
	entry *f = fatx_context::get()->root->find(path);
	if(f == nullptr || f->status == entry::invalid)
		return -ENOENT;
	struct stat st;
	int res;
	if((res = fatx_getattr(f->path().data(), &st, fi)) != 0)
		return res;
	ff(buf, ".", &st, 0, FUSE_FILL_DIR_PLUS);
	if((res = fatx_getattr(f->parent->path().data(), &st, fi)) != 0)
		return res;
	ff(buf, "..", &st, 0, FUSE_FILL_DIR_PLUS);
	for(entry& i: f->childs) {
		if(i.status == entry::valid || (fatx_context::get()->mmi.recover && i.status == entry::delwdata)) {
			if((res = fatx_getattr(i.path().data(), &st, fi)) != 0)
				return res;
			if(ff(buf, i.name, &st, 0, FUSE_FILL_DIR_PLUS) != 0)
				return -EBADF;
			#ifndef NDEBUG
				dbglog(" {}", i.path());
			#endif
		}
	}
	return 0;
}
static int									fatx_create		(const char *path, mode_t mode) {
	#ifndef NDEBUG
		dbglog("CREATE: {}", path);
	#endif
	if(!fatx_context::get()->mmi.writeable())
		return -EACCES;
	std::string p(path);
	size_t l = p.find_last_of(sepdir);
	if(l == std::string::npos || l == p.length() - 1)
		return -ENOENT;
	if(fatx_context::get()->mmi.cutname)
		p.resize(l + 1 + name_size);
	else {
		if(p.length() > name_size + l + 1)
			return -ENAMETOOLONG;
	}
	if(fatx_context::get()->root->find(path) != nullptr) {
		if((mode & S_IFREG) == 0)
			return -EEXIST;
		else
			fatx_remove(path);
	}
	entry *n = new entry(p.substr(l + 1), 0, ((mode & S_IFREG) == 0));
	if(n->flags.dir && n->cluster == 0) {
		fatx_context::get()->fat->freefat(n->cluster);
		delete n;
		return -ENOSPC;
	}
	entry *s = fatx_context::get()->root->find(p.substr(0, l).data());
	if(s == nullptr) {
		fatx_context::get()->fat->freefat(n->cluster);
		delete n;
		return -ENOENT;
	}
	int res = 0;
	if((res = s->addtodir(n))) {
		fatx_context::get()->fat->freefat(n->cluster);
		delete n;
		return -res;
	}
	return fatx_chmod(path, mode, nullptr);
}
static int									fatx_creope		(const char *path, mode_t mode, fuse_file_info* fi) {
	#ifndef NDEBUG
		dbglog("CREOPE: {}", path);
	#endif
	int res = 0;
	if((res = fatx_create(path, mode)) != 0)
		return res;
	return fatx_open(path, fi);
}
static int									fatx_remove		(const char *path) {
	#ifndef NDEBUG
		dbglog("REMOVE: {}", path);
	#endif
	entry *f = fatx_context::get()->root->find(path);
	if(f == nullptr || f->status == entry::invalid)
		return -ENOENT;
	if(!fatx_context::get()->mmi.writeable())
		return -EROFS;
	if(f->flags.ro)
		return -EACCES;
	if(f == fatx_context::get()->root)
		return -EBUSY;
	if(f->flags.dir && f->childs.size() != 0)
		return -ENOTEMPTY;
	assert(f->parent != nullptr);
	f->parent->remfrdir(f);
	return 0;
}
static int									fatx_rename		(const char *from, const char *to, unsigned int) {
	#ifndef NDEBUG
		dbglog("RENAME: {} to {}", from, to);
	#endif
	entry *f = fatx_context::get()->root->find(from);
	if(f == nullptr || f->status == entry::invalid)
		return -ENOENT;
	if(!fatx_context::get()->mmi.writeable())
		return -EROFS;
	if(f->flags.ro)
		return -EACCES;
	return -f->rename(to);
}
static int									fatx_getattr	(const char *path, struct stat* st, fuse_file_info*) {
	#ifndef NDEBUG
		dbglog("GETATTR: {}", path);
	#endif
	entry *f = fatx_context::get()->root->find(path);
	if(f == nullptr || f->status == entry::invalid || (f->flags.dir && f->cluster == 0))
		return -ENOENT;
	memset(st, 0, sizeof(struct stat));
	std::shared_lock lock(f->mux_E);
	st->st_dev		= fatx_context::get()->par.par_id;
	st->st_mode		= f->flags() & (fatx_context::get()->mmi.mask | S_IFDIR | S_IFREG);
	st->st_nlink	= f->childs.size() + 1;
	st->st_size		= static_cast<__off_t>(f->flags.dir ? f->childs.size() : f->size);
	st->st_blksize	= fatx_context::get()->par.clus_size;
	st->st_blocks	= static_cast<__blkcnt_t>(clsarithm::siz2cls(f->size) * fatx_context::get()->par.clus_size / blksize);
	st->st_atime	= f->access();
	st->st_mtime	= f->update();
	st->st_ctime	= f->creation();
	st->st_uid		= fatx_context::get()->mmi.uid;
	st->st_gid		= fatx_context::get()->mmi.gid;
	#ifndef NDEBUG
		dbglog(f->print());
	#endif
	return 0;
}
static int									fatx_chmod		(const char *path, mode_t mode, fuse_file_info*) {
	#ifndef NDEBUG
		dbglog("CHMOD: {}", path);
	#endif
	entry *f = fatx_context::get()->root->find(path);
	if(f == nullptr || f->status == entry::invalid)
		return -ENOENT;
	if(!fatx_context::get()->mmi.writeable())
		return -EROFS;
	std::unique_lock lock(f->mux_E);
	f->flags(mode);
	auto res = -f->write(true);
	return res;
}
static int									fatx_chown		(const char *path, uid_t, gid_t, fuse_file_info*) {
	#ifndef NDEBUG
		dbglog("CHOWN: {}", path);
	#endif
	const entry *f = fatx_context::get()->root->find(path);
	if(f == nullptr || f->status == entry::invalid)
		return -ENOENT;
	if(!fatx_context::get()->mmi.writeable())
		return -EROFS;
	// nothing done
	return 0;
}
static int									fatx_truncate	(const char *path, off_t size, fuse_file_info*) {
	#ifndef NDEBUG
		dbglog("TRUNCATE: {}", path);
	#endif
	entry *f = fatx_context::get()->root->find(path);
	if(f == nullptr || f->status == entry::invalid)
		return -ENOENT;
	if(!fatx_context::get()->mmi.writeable())
		return -EROFS;
	if(f->flags.ro)
		return -EACCES;
	std::unique_lock lock(f->mux_E);
	auto res = -f->resize(static_cast<filesize>(size));
	return res;
}
static int									fatx_utimens	(const char *path, const timespec tv[2], fuse_file_info*) {
	#ifndef NDEBUG
		dbglog("UTIMENS: {}", path);
	#endif
	entry *f = fatx_context::get()->root->find(path);
	if(f == nullptr || f->status == entry::invalid)
		return -ENOENT;
	if(!fatx_context::get()->mmi.writeable())
		return -EROFS;
	if(f->flags.ro)
		return -EACCES;
	date d;
	d(tv[0].tv_sec);
	std::unique_lock lock(f->mux_E);
	f->access = d;
	d(tv[1].tv_sec);
	f->update = d;
	auto res = -f->write(true);
	return res;
}
static int									fatx_statfs		(const char *path, struct statvfs* sfs) {
	#ifndef NDEBUG
		dbglog("STATFS: {}", path);
	#else
		(void)path;
	#endif
	sfs->f_bsize    = fatx_context::get()->par.clus_size;
	sfs->f_frsize   = fatx_context::get()->par.clus_size;
	sfs->f_blocks   = (fatx_context::get()->par.clus_fat - fatx_context::get()->par.root_clus);
	sfs->f_bfree    = fatx_context::get()->fat->clsavail();
	sfs->f_bavail   = fatx_context::get()->fat->clsavail();
	sfs->f_files	= 0;
	sfs->f_ffree	= 0;
	sfs->f_favail	= 0;
	sfs->f_fsid		= fatx_context::get()->par.par_id;
	sfs->f_flag	= ST_NOSUID | (fatx_context::get()->mmi.writeable() ? 0 : ST_RDONLY);
	sfs->f_namemax	= name_size;
	#ifndef NDEBUG
		dbglog(
			" f_bsize:\t{:d}\n"
			" f_frsize:\t{:d}\n"
			" f_blocks:\t{:d}\n"
			" f_bfree:\t{:d}\n"
			" f_bavail:\t{:d}\n"
			" f_files:\t{:d}\n"
			" f_ffree:\t{:d}\n"
			" f_favail:\t{:d}\n"
			" f_fsid:\t0x{:08X}\n"
			" f_flag:\t0x{:08X}\n"
			" f_namemax:\t{:d}\n"
		,
			sfs->f_bsize,
			sfs->f_frsize,
			sfs->f_blocks,
			sfs->f_bfree,
			sfs->f_bavail,
			sfs->f_files,
			sfs->f_ffree,
			sfs->f_favail,
			sfs->f_fsid,
			sfs->f_flag,
			sfs->f_namemax
		);
	#endif
	return 0;
}
static void*								fatx_init		(fuse_conn_info* fci, fuse_config*) {
	#ifndef NDEBUG
		dbglog("INIT");
	#endif
	fci->want = FUSE_CAP_DONT_MASK;
	return nullptr;
}
static void									fatx_destroy	(void*) {
	#ifndef NDEBUG
		dbglog("DESTROY");
	#endif
	fatx_context::get()->destroy();
	delete fatx_context::get();
}

static struct fuse_operations				fatx_ops;

int											main(int argc, char *argv[]) {
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
		fatx_ops.getattr		= fatx_getattr;
		fatx_ops.utimens		= fatx_utimens;
		fatx_ops.chmod			= fatx_chmod;
		fatx_ops.chown			= fatx_chown;
		fatx_ops.create			= fatx_creope;
		fatx_ops.open			= fatx_open;
		fatx_ops.read			= fatx_read;
		fatx_ops.write			= fatx_write;
		fatx_ops.flush			= fatx_flush;
		fatx_ops.release		= fatx_close;
		fatx_ops.truncate		= fatx_truncate;
		fatx_ops.unlink			= fatx_remove;
		fatx_ops.mkdir			= fatx_create;
		fatx_ops.opendir		= fatx_open;
		fatx_ops.readdir		= fatx_readdir;
		fatx_ops.releasedir		= fatx_close;
		fatx_ops.rmdir			= fatx_remove;
		fatx_ops.rename			= fatx_rename;
		fatx_ops.statfs			= fatx_statfs;
		fatx_ops.init			= fatx_init;
		fatx_ops.destroy		= fatx_destroy;
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
