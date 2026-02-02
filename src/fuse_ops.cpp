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

#include <mutex>

struct fuse_operations			        	fatx_ops;

int							        		fatx_open		(const char *path, fuse_file_info* fi) {
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
int											fatx_read		(const char *path, char *buf, size_t size, off_t offset, fuse_file_info* fi) {
	#ifndef NDEBUG
		dbglog("READ: {} ({}@{})", path, size, offset);
	#endif
	entry *f = fatx_context::get()->root->find(path);
	if(f == nullptr || f->status == entry::invalid || fi->fh != uint64_t(f))
		return -ENOENT;
	return static_cast<int>(f->bufread(buf, static_cast<filesize>(offset), size));
}
int											fatx_write		(const char *path, const char *buf, size_t size, off_t offset, fuse_file_info* fi) {
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
int											fatx_flush		(const char *path, fuse_file_info* fi) {
	#ifndef NDEBUG
		dbglog("FLUSH: {}", path);
	#endif
	entry *f = fatx_context::get()->root->find(path);
	if(f == nullptr || f->status == entry::invalid || fi->fh != uint64_t(f))
		return -ENOENT;
	return -f->flush();
}
int											fatx_close		(const char *path, fuse_file_info* fi) {
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
int											fatx_readdir	(const char *path, void* buf, fuse_fill_dir_t ff, off_t, fuse_file_info*fi, fuse_readdir_flags) {
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
int											fatx_create		(const char *path, mode_t mode) {
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
int											fatx_creope		(const char *path, mode_t mode, fuse_file_info* fi) {
	#ifndef NDEBUG
		dbglog("CREOPE: {}", path);
	#endif
	int res = 0;
	if((res = fatx_create(path, mode)) != 0)
		return res;
	return fatx_open(path, fi);
}
int											fatx_remove		(const char *path) {
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
int											fatx_rename		(const char *from, const char *to, unsigned int) {
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
int											fatx_getattr	(const char *path, struct stat* st, fuse_file_info*) {
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
int											fatx_chmod		(const char *path, mode_t mode, fuse_file_info*) {
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
int											fatx_chown		(const char *path, uid_t, gid_t, fuse_file_info*) {
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
int											fatx_truncate	(const char *path, off_t size, fuse_file_info*) {
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
int											fatx_utimens	(const char *path, const timespec tv[2], fuse_file_info*) {
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
int											fatx_statfs		(const char *path, struct statvfs* sfs) {
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
void*										fatx_init		(fuse_conn_info* fci, fuse_config*) {
	#ifndef NDEBUG
		dbglog("INIT");
	#endif
	fci->want = FUSE_CAP_DONT_MASK;
	return nullptr;
}
void										fatx_destroy	(void*) {
	#ifndef NDEBUG
		dbglog("DESTROY");
	#endif
	fatx_context::get()->destroy();
	delete fatx_context::get();
}
