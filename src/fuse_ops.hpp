#pragma once
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

#define FUSE_USE_VERSION 31

#include <fuse3/fuse.h>

extern struct fuse_operations				fatx_ops;

inline void                                 fatx_ops_init();

int     									fatx_open(const char*, fuse_file_info*);
int											fatx_read(const char*, char*, size_t, off_t, fuse_file_info*);
int											fatx_write(const char*, const char*, size_t, off_t, fuse_file_info*);
int											fatx_flush(const char*, fuse_file_info*);
int											fatx_close(const char*, fuse_file_info*);
int											fatx_readdir(const char*, void*, fuse_fill_dir_t, off_t, fuse_file_info*, fuse_readdir_flags);
int											fatx_create(const char*, mode_t);
int											fatx_creope(const char*, mode_t, fuse_file_info*);
int											fatx_remove(const char*);
int											fatx_rename(const char*, const char*, unsigned int);
int											fatx_getattr(const char*, struct stat*, fuse_file_info*);
int											fatx_chmod(const char*, mode_t, fuse_file_info*);
int											fatx_chown(const char*, uid_t, gid_t, fuse_file_info*);
int											fatx_truncate(const char*, off_t, fuse_file_info*);
int											fatx_utimens(const char*, const timespec[2], fuse_file_info*);
int											fatx_statfs(const char*, struct statvfs*);
void*										fatx_init(fuse_conn_info*, fuse_config*);
void								        fatx_destroy(void*);

inline void                                 fatx_ops_init() {
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
}