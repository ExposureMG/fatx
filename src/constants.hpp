#pragma once
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

#include "types.hpp"

static const size_t					blksize			= 512;					// standard block size
static const clusptr				EOC				= 0xFFFFFFFF;			// fat: end of chain
static const clusptr				FLK				= 0x00000000;			// fat: free link
static const char					EOD				= -1;					// entry mark for end of directory
static const unsigned char			name_size		= 0x2A;					// maximum size of entry names
static const unsigned char			deleted_size	= 0xE5;					// name size used in entry to mark entry as deleted
static const size_t					slab			= name_size * 2 + 2;	// maximum size of label name file
static const int					max_fuse_args	= 20;					// maximum number of unrecognized arguments passed to fuse
static const unsigned int			max_buf			= 1*1024*1024;			// buffer maximum size
static const unsigned int			max_cache_div	= 1000;					// fat size divider for cache maximum size
static const unsigned int			nb_cache_div	= 10;					// cache size divider for nuber of read ahead operations
static const unsigned int			timeout			= 60;					// timeout in seconds
static const int					code_noerr		= 0;					// no error code
static const int					code_corrd		= 1<<0;					// errors corrected code
static const int					code_ncorr		= 1<<2;					// errors remaining code
static const int					code_operr		= 1<<3;					// internal error code
static const int					code_usage		= 1<<4;					// usage error code

const char * const					sepdir			= "/";		    		// using unix directories
const char * const					fsid			= "XTAF";		    	// filesystem id
const char * const					flab			= "name.txt";	    	// file used for label name
const char * const					def_landf		= "lost+found";	    	// default directory for lost & founds

const char * const					def_fpre		= "FILE";		    	// default file prefix for lost & founds
const char * const					def_label		= "XBOX";		    	// default label name
const char * const					usb_dir			= "Xbox360";	    	// USB drive directory
const char * const					usb_data		= "Data";		    	// USB drive data file prefix

const char * const					mutex_buff		= "Buffer:";	    	// mutex for buffer
const char * const					mutex_data		= "Data:";		    	// mutex for data
const char * const					mutex_entr		= "Entry:";	    	    // mutex for entry
