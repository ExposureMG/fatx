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

#include <string>
#include <vector>

#include "types.hpp"

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
    
                                        frontend(int, const char *const * const);
        std::string						name() const;
        [[nodiscard]] int				setup();
        void							parser();
        bool							getanswer(bool = false);
        bool							writeable() const { return !readonly; }
        void							set_readonly(bool v) { readonly = v; }
    };