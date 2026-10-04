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

#include <memory>
#include <string>
#include <vector>
#include <sys/stat.h>

#include "types.hpp"
#include "io.hpp"

#ifdef _WIN32
using uid_t                         = unsigned int;
using gid_t                         = unsigned int;
inline uid_t                        fatx_getuid() { return 0; }
inline gid_t                        fatx_getgid() { return 0; }
#else
#include <unistd.h>
inline uid_t                        fatx_getuid() { return getuid(); }
inline gid_t                        fatx_getgid() { return getgid(); }
#endif

class								frontend {
    private:
        bool							readonly;
    public:
        enum							call_t {
            unknown	= 0,
            fuse,
            mkfs,
            fsck,
            unrm,
            label
        };
    
        call_t							prog;
        bool							force_y;
        bool							force_n;
        bool							force_a;
        bool							verbose;
        bool							recover;
        bool							local;
        bool							deldate;
        bool							dellost;
        bool							fuse_debug;
        bool							fuse_foregrd;
        bool							fuse_singlethr;
        bool							nofat;
        bool							cutname;
        int								argc;
        const char *const *	const		argv;
        std::string						progname;
        bool							dialog;
        std::string						lostfound;
        std::string						foundfile;
        unsigned int					filecount;
        std::string						mount;
        std::string						volname;
        std::string						fuse_option;
        std::vector<std::string>		unkopt;
        std::string						partition;
        std::string						table;
        streamptr						clus_size;
        uid_t							uid;
        gid_t							gid;
        mode_t							mask;
        bool							allyes;
        streamptr						offset;
        streamptr						size;
        std::string						input;
        std::string						script;
        std::string						diffile;
        std::shared_ptr<fatx::io_backend>	backend;	// when set, used instead of opening input
        unsigned int					asked;		// number of questions asked (problems found by fsck)
    
                                        frontend(int, const char *const * const);
        std::string						name() const;
        [[nodiscard]] int				setup();
        void							parser();
        bool							getanswer(bool = false);
        bool							writeable() const { return !readonly; }
        void							set_readonly(bool v) { readonly = v; }
    };