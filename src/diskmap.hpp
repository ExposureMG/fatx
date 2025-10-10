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

#include <cstdint>
#include <string>
#include <set>
#include <map>
#include <vector>

#include <boost/bimap.hpp>
#include <boost/bimap/list_of.hpp>
#include <boost/bimap/set_of.hpp>
#include <boost/bimap/multiset_of.hpp>

#include "types.hpp"
#include "utils.hpp"

class								dskmap {
protected:
    using mapptr_t = clusptr;
    using mapsiz_t = clusptr;
    using gap_t = boost::bimaps::bimap<
        boost::bimaps::set_of<mapptr_t>,
        boost::bimaps::multiset_of<mapsiz_t>
    >;
    using memnext_t = read_cache<
        clusptr,
        clusptr
    >;
    using lbdarea_t = std::function<
        void(const clusptr&, const clusptr&)
    >;
    using lbdfat_t = std::function<
        void(const clusptr&, const clusptr&)
    >;

    memnext_t						memnext;
    gap_t							freegaps;
    mymutx							authm;
    std::set<clusptr>				bad;
    bool							scanned;

    static void						forfat(const lbdfat_t &);
    memnext_t::lkval_t				real_read(clusptr, size_t);
    int								real_write(clusptr, clusptr);

public:
    enum							status_t {
        disk,
        deleted,
        modified,
        marked
    };
                                    dskmap(const partition &);
    virtual							~dskmap();
    clusptr							clsavail();
    void							erase();
    void							gapcheck();
    vareas							getareas(clusptr, const lbdarea_t & = nullptr);
    virtual clusptr					read(clusptr);
    [[nodiscard]] int				write(clusptr, clusptr);
    vareas							allocfat(clusptr, clusptr = 0);
    void							freefat(clusptr);
    [[nodiscard]] int				resizefat(ptr_vareas, clusptr);
    std::string						printchain(clusptr);
    void							printgaps() const;
    virtual void					change [[noreturn]] (clusptr, entry*, clusptr = FLK, status_t = marked);
    virtual status_t				status [[noreturn]] (clusptr) const;
    virtual entry*					getentry [[noreturn]] (clusptr) const;
    virtual void					fatlost [[noreturn]] ();
    virtual void					fatcheck [[noreturn]] ();
    #ifndef NDEBUG
    virtual void					printfat [[noreturn]] ();
    #endif
};
class								memmap final : public dskmap {
private:
public:
    class							link {
    public:
        clusptr						next;
        entry*						ent;
        status_t					status;
                                    link(entry *e, clusptr n = FLK, status_t s = marked) : next(n), ent(e), status(s) { }
    };
    using memchain_t = std::map<
        clusptr,
        class link
    >;
    using lost_t = std::vector<vareas>;
    memchain_t						memchain;
    lost_t							lost;

                                    memmap(const partition&);
    clusptr							read(clusptr) override;
    void							change(clusptr, entry*, clusptr = FLK, status_t = marked) override;
    status_t						status(clusptr) const override;
    entry*							getentry(clusptr) const override;
    void							fatlost() override;
    void							fatcheck() override;
    #ifndef NDEBUG
    void							printfat() override;
    #endif
};