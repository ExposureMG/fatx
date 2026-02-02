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

#include <fstream>
#include <map>
#include <string>

#include "types.hpp"
#include "utils.hpp"

class								device {
private:
    struct							segment {
        filesize					size;
        streamptr					position;
    };
    class							chgfile : public std::map<streamptr, segment> {
    private:
        [[nodiscard]] bool			addseg(streamptr, const std::string&);
    public:
        [[nodiscard]] bool			load();
        [[nodiscard]] bool			read(streamptr, const size_t, std::string &);
        [[nodiscard]] bool			write(streamptr, const std::string &);
    };
    std::fstream					io;
    std::fstream					iod;
    streamptr						tot_size;
    bool							changes;
    mymutx							authd;
    chgfile							chgf;
    std::map<streamptr, std::fstream>	usbd;
public:
                                    device();
                                    ~device();
    [[nodiscard]] int				setup();
    streamptr						size() const { return tot_size; }
    bool							modified() const { return changes; }
    std::string						read(streamptr, size_t = blksize);
    [[nodiscard]] int				write(streamptr, const std::string &);
    static std::string				address(streamptr);
    void							devlog(bool, streamptr, const std::string&) const;
    std::string						print(streamptr, size_t = blksize, size_t = 32);
};