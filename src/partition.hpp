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

#include <cstdint>
#include <string>

#include "utils.hpp"

class								partition {
private:
    class							bootsect {
    public:
        uint32_t					id;
        uint32_t					spc;
        uint32_t					root;
                                    bootsect(const char buf[blksize]) :
            id	(byte_order<4>::litend(&buf[4])()),
            spc	(byte_order<4>::litend(&buf[8])()),
            root(byte_order<4>::litend(&buf[12])()) { }
                                    bootsect(const uint32_t i, const uint32_t s, const uint32_t r) :
            id(i), spc(s), root(r) { }
        void						write(char buf[blksize]) {
            memcpy(&buf[0], &fsid[0], 4);
            memcpy(&buf[4], byte_order<4>::litend(id).data(), 4);
            memcpy(&buf[8], byte_order<4>::litend(spc).data(), 4);
            memcpy(&buf[12], byte_order<4>::litend(root).data(), 4);
        }
    };
    class							devheader {
    public:
        uint32_t					id;
        uint32_t					unkn;
        uint32_t					p2_start;
        uint32_t					p2_size;
        uint32_t					p1_start;
        uint32_t					p1_size;
                                    devheader(char const buf [blksize]) :
            id			(byte_order<4>::litend(&buf[0])()),
            unkn		(byte_order<4>::litend(&buf[4])()),
            p2_start	(byte_order<4>::litend(&buf[8])()),
            p2_size		(byte_order<4>::litend(&buf[12])()),
            p1_start	(byte_order<4>::litend(&buf[16])()),
            p1_size		(byte_order<4>::litend(&buf[20])()) { }
                                    devheader(const uint64_t s) :
            id(0x00020000),
            unkn(0),
            p2_start(0x00633000),
            p2_size(static_cast<uint32_t>((s - 0xC6600000ULL) >> 9)),
            p1_start(0x005B3000),
            p1_size(0x00080000) { }
        void						write(char buf[blksize]) {
            memcpy(&buf[0], byte_order<4>::litend(id).data(), 4);
            memcpy(&buf[8], byte_order<4>::litend(p2_start).data(), 4);
            memcpy(&buf[12], byte_order<4>::litend(p2_size).data(), 4);
            memcpy(&buf[16], byte_order<4>::litend(p1_start).data(), 4);
            memcpy(&buf[20], byte_order<4>::litend(p1_size).data(), 4);
        }
    };
    class							usbheader {
    public:
    };
public:
    uint32_t						par_id;
    std::string						par_label;
    streamptr						par_start;
    streamptr						par_size;
    uint32_t						clus_size;
    uint16_t						clus_pow;
    uint32_t						clus_num;
    uint32_t						clus_fat;
    uint16_t						chain_size;
    uint16_t						chain_pow;
    streamptr						fat_start;
    streamptr						fat_size;
    streamptr						root_start;
    clusptr							root_clus;

                                    partition();
    [[nodiscard]] int				setup();
    [[nodiscard]] int				write();
    size_t							label(unsigned char [slab]) const;
    void							label(const unsigned char [slab], const size_t size);
};