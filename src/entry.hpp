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
#include <sys/stat.h>
#include <boost/ptr_container/ptr_vector.hpp>

#include "types.hpp"
#include "utils.hpp"

// Entries attributes
//
class								attrib {
    public:
        bool							ro;
        bool							hid;
        bool							sys;
        bool							lab;
        bool							dir;
        bool							arc;
        bool							dev;
        bool							na;
                                        attrib() :
            ro	(false),
            hid	(false),
            sys	(false),
            lab	(false),
            dir	(false),
            arc	(false),
            dev	(false),
            na	(false) { }
                                        attrib(char c) :
            ro(c & (1 << 0)),
            hid(c & (1 << 1)),
            sys(c & (1 << 2)),
            lab(c & (1 << 3)),
            dir(c & (1 << 4)),
            arc(c & (1 << 5)),
            dev(c & (1 << 6)),
            na(c & (1 << 7)) { }
    #ifndef NDEBUG
        std::string						print() const {
            return std::format("{}{}{}{}{}{}{}-",
                (ro		? 'R' : '-'),
                (hid	? 'H' : '-'),
                (sys	? 'S' : '-'),
                (lab	? 'L' : '-'),
                (dir	? 'D' : '-'),
                (arc	? 'A' : '-'),
                (dev	? 'V' : '-')
            );
        }
        #endif
        char*							write(char buf[1]) {
            buf[0] = static_cast<char>(
                (ro	 ? (1 << 0) : 0) |
                (hid	? (1 << 1) : 0) |
                (sys	? (1 << 2) : 0) |
                (lab	? (1 << 3) : 0) |
                (dir	? (1 << 4) : 0) |
                (arc	? (1 << 5) : 0) |
                (dev	? (1 << 6) : 0) |
                (na	 ? (1 << 7) : 0)
            );
            return buf;
        }
        mode_t							operator () () const {
            return static_cast<mode_t>(
                S_IRUSR | S_IXUSR | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH |
                (!ro ? (S_IWUSR | S_IWGRP | S_IWOTH) : 0) |
                (dir ? S_IFDIR : S_IFREG)
            );
        }
        void							operator () (mode_t m) { ro = ((m & (S_IWUSR | S_IWGRP | S_IWOTH)) == 0); }
    };
    
    // FATX date stamps
    //
    class								date {
    public:
        unsigned int					year;
        unsigned int					month;
        unsigned int					day;
        unsigned int					hour;
        unsigned int					min;
        unsigned int					sec;
        using date_t = uint64_t;
                                        date() :
            year	(1980),
            month	(1),
            day		(1),
            hour	(0),
            min		(0),
            sec		(0) { }
                                        date(const unsigned char buf[4]) :
            year	(static_cast<unsigned int>((buf[0] >> 1) + 1980)),
            month	(static_cast<unsigned int>((((buf[0] & 1) << 3) | ((buf[1] & 0xE0) >> 5)) + 1)),
            day		(static_cast<unsigned int>((buf[1] & 0x1F) + 1)),
            hour	(static_cast<unsigned int>(buf[2] >> 3)),
            min		(static_cast<unsigned int>(((buf[2] & 0x07) << 3) | ((buf[3] & 0xE0) >> 5))),
            sec		(static_cast<unsigned int>(buf[3] & 0x1F)) { }
                                        date(time_t t) : date() { (*this)(t); }
        #ifndef NDEBUG
            std::string					print() const {
            return std::format("{:02d}:{:02d}:{:02d} {:02d}/{:02d}/{:04d}",
                hour, min, sec, day, month, year
            );
        }
        #endif
        unsigned char*					write(unsigned char buf[4]) {
            buf[0] = static_cast<unsigned char>((((year - 1980) & 0x7F) << 1) | (((month - 1) & 0x08) >> 3));
            buf[1] = static_cast<unsigned char>((((month - 1) & 0x07) << 5) | ((day - 1) & 0x1F));
            buf[2] = static_cast<unsigned char>(((hour & 0x1F) << 3) | ((min & 0x38) >> 3));
            buf[3] = static_cast<unsigned char>(((min & 0x07) << 5) | (sec & 0x1F));
            return buf;
        }
        date_t							seq() const {
            return
                (date_t((year - 1980)	& 0xFFFF)	<< 48) |
                (date_t(month			& 0xFF)		<< 40) |
                (date_t(day				& 0xFF)		<< 32) |
                (date_t(hour			& 0xFF)		<< 16) |
                (date_t(min				& 0xFF)		<< 8)  |
                (date_t(sec				& 0xFF)		<< 0)
            ;
        }
        time_t							operator () () const {
            struct tm st;
            st.tm_isdst	= -1;
            st.tm_yday	= 0;
            st.tm_wday	= 0;
            st.tm_year	= static_cast<int>(year - 1900);
            st.tm_mon	= static_cast<int>(month - 1);
            st.tm_mday	= static_cast<int>(day);
            st.tm_hour	= static_cast<int>(hour);
            st.tm_min	= static_cast<int>(min);
            st.tm_sec	= static_cast<int>(sec);
            return mktime(&st);
        }
        void							operator () (time_t t)
        {
            const struct tm *st(localtime(&t));
            if(st != nullptr) {
                year	= static_cast<unsigned int>(st->tm_year + 1900);
                month	= static_cast<unsigned int>(st->tm_mon + 1);
                day		= static_cast<unsigned int>(st->tm_mday);
                hour	= static_cast<unsigned int>(st->tm_hour);
                min		= static_cast<unsigned int>(st->tm_min);
                sec		= static_cast<unsigned int>(st->tm_sec);
            }
        }
    };
    class								entry : boost::noncopyable {
    public:
        enum							status_t {
            valid,
            delwdata,
            delnodata,
            lost,
            duplicate,
            validupl,
            end,
            invalid
        };
        enum							pass_t {
            findfile,
            finddel,
            tryrecov
        };
        static const size_t				ent_size = 64;
    public:
        mymutx							mux_B;
        mymutx							mux_D;
        mymutx							mux_E;
        std::binary_semaphore			exclusive;
        ptr_buffer						entbuf;
        bool							opened;
        char							name[name_size + 1];
        int								cptacc;
        bool							writeopened;
        status_t						status;
        uint8_t							namesize;
        attrib							flags;
        clusptr							cluster;
        filesize						size;
        date							creation;
        date							access;
        date							update;
        streamptr						loc;
        entry*							parent;
        boost::ptr_vector<entry>		childs;
        ptr_vareas						areas;
    public:
                                        entry();
                                        entry(streamptr, const char[ent_size] = nullptr);
                                        entry(const std::string &, filesize = 0, const bool = false);
                                        ~entry();
    
        // Locks entry
        [[nodiscard]] int				rename(const char*);
        void							remfrdir(entry*);
        [[nodiscard]] int				addtodir(entry*);
        void							open(bool w);
        void							close(bool w);
        size_t							bufread(char*, filesize, filesize);
        size_t							bufwrite(const char*, filesize, filesize);
    
        // Shared locks entry
        entry*                          find(const char*);
    
        // No entry lock
        [[nodiscard]] int               write(bool = false);
        std::string                     print();
        void                            touch(bool = true, bool = true, bool = true);
        std::string                     path();
        [[nodiscard]] int               resize(const filesize);
        [[nodiscard]] int               flush(bool = true);
        [[nodiscard]] int               data(char*, bool, filesize, filesize);
    
        // Not used by FUSE
        bool			            	analyse(pass_t, const std::string & = std::string(""));
        void							guess();
        void							recover();
        void							opendir();
    };