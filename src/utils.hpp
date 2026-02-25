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


#include <unordered_set>
#include <string>
#include <sstream>
#include <vector>
#include <thread>
#include <iostream>
#include <format>
#include <shared_mutex>
 
#include <boost/integer.hpp>
#include <boost/bimap.hpp>
#include <boost/bimap/list_of.hpp>
 
#include "types.hpp"
#include "constants.hpp"

// Big / Little byte_order (de)formatter
//
template<int bytes = 1>
class								byte_order {
private:
	template<int big>
	class							byte_order_format : public std::string {
	public:
		using value_type = typename boost::uint_t<bytes * 8>::least;
	private:
		size_t						pos(const size_t i) const { return (big == 0) ? i : bytes - 1 - i; }
		std::string::value_type		getbyte(const value_type n, const size_t i = 0) const {
			return std::string::value_type((n & (0xFFULL << (pos(i) * 8))) >> (pos(i) * 8));
		}
		value_type					setbyte(const std::string &s, const size_t i = 0) const {
			return static_cast<value_type>((1ULL << (8 * pos(i))) * static_cast<uint8_t>(s[i]));
		}
		value_type					getvalue() const {
			value_type res = 0;
			for(size_t i = 0; i < bytes; i++)
				res = static_cast<value_type>(res + setbyte((*this), i));
			return res;
		}
		value_type					setvalue(const value_type n) {
			(*this).erase();
			for(size_t i = 0; i < bytes; i++)
				(*this) += getbyte(n, i);
			return getvalue();
		}
	public:
		value_type					operator () () const { return getvalue(); }
		value_type					operator () (const value_type n) { return setvalue(n); }
									byte_order_format(const value_type n) { setvalue(n); }
									byte_order_format(const std::string::value_type* const s) : std::string(s, bytes) { }
									byte_order_format(const std::string& s) : std::string(s) { }
	};
public:
	using value_type = typename byte_order_format<0>::value_type;
	using bigend = byte_order_format<1>;
	using litend = byte_order_format<0>;
};

// Data areas()
//
class								area {
public:
	streamptr						offset;
	streamptr						pointer;
	streamptr						size;
	clusptr							start;
	clusptr							stop;
									area(streamptr o, streamptr p, streamptr s, clusptr rt, clusptr op) noexcept : offset(o), pointer(p), size(s), start(rt), stop(op) { }
};
class								vareas : public std::vector<area> {
public:
	[[gnu::pure]] clusptr			first() const;
	[[gnu::pure]] clusptr			last() const;
	[[gnu::pure]] size_t			nbcls(clusptr = 0) const;
	[[gnu::pure]] clusptr			at(size_t) const;
	[[gnu::pure]] iterator			in(size_t);
	vareas							sub(filesize, filesize = 0) const;
	void							add(vareas);
	void							add(clusptr);
	std::string						print() const;
};

// Data buffers
//
class								buffer : public std::string {
public:
	bool							touched;
	streamptr						offset;
									buffer(const streamptr = 0, const streamptr = 0);
									~buffer();
	void							enlarge(const streamptr);
	void							operator () (size_t);
};

// Mutex management
//
class								mymutx : public std::shared_mutex {
private:
	std::string						nam;
	int								cpt;
public:
									mymutx(const std::string &n = "???") : nam(n), cpt(0) { }
									~mymutx() { nam.clear(); }
	void							name(const std::string &n) { nam = n; }
	void							lock();
	void							unlock();
	void							lock_shared();
	void							unlock_shared();
};

// LRU read cache
//
template<typename key_t, typename value_t>
class								read_cache {
public:
	using key_type = key_t;
	using value_type = value_t;
	using container_type = boost::bimaps::bimap<
		boost::bimaps::set_of<key_type>,
		boost::bimaps::list_of<value_type>
	>;
	using pair_type = std::pair<
		value_type,
		key_type
	>;
	using lkval_t = std::vector<pair_type>;
	using fread_t = std::function<
		lkval_t(
			const key_type&,
			const size_t&
		)
	>;
	using fwrite_t = std::function<
		int(
			const key_type&,
			const value_type&
		)
	>;
protected:
	const fread_t					read;
	const fwrite_t					write;
	const size_t					capacity;
	const size_t					readahead;
	container_type					container;
	mymutx							access;
public:
									read_cache(const fread_t&, const fwrite_t&, size_t, size_t);
	void							clear();
	value_type						operator () (const key_type&);
	int								operator () (const key_type&, const value_type&);
	void							print();
};
template<typename key_t, typename value_t>
			read_cache<key_t, value_t>::
									read_cache(const fread_t& r, const fwrite_t& w, size_t c, size_t a) :
	read(r), write(w), capacity(c), readahead(a), access("cache") {
	assert(capacity != 0);
}
template<typename key_t, typename value_t>
void		read_cache<key_t, value_t>::
									clear() {

	access.lock();
	container.clear();
	access.unlock();
}
template<typename key_t, typename value_t>
typename read_cache<key_t, value_t>::value_type
			read_cache<key_t, value_t>::
									operator () (const key_type& k) {
	#ifndef NO_CACHE
		access.lock();
		const typename container_type::left_iterator it = container.left.find(k);
		if(it != container.left.end()) {
			container.right.relocate(container.right.end(), container.project_right(it));
			access.unlock();
			return it->second;
		}
		else {
			assert(container.size() <= capacity);
			lkval_t vv = read(k, readahead);
			if(vv.empty()) {
				#if !defined NDEBUG && defined DBG_CACHE
					dbglog("... fatbuf: nothing for 0x%08X", k);
				#endif
				access.unlock();
				return 0;
			}
			if(container.size() + vv.size() > capacity) {
				typename container_type::right_iterator b = container.right.begin();
				std::advance(b, container.size() + vv.size() - capacity);
				#if !defined NDEBUG && defined DBG_CACHE
					dbglog("Xx. fatbuf: reduce (%d)", container.size());
				#endif
				container.right.erase(container.right.begin(), b);
				#if !defined NDEBUG && defined DBG_CACHE
					#ifdef DBG_CACHDMP
						print();
					#endif
				#endif
			}
			container.insert(typename container_type::value_type(vv.front().second, vv.front().first));
			container.right.insert(container.right.begin(), vv.begin() + 1, vv.end());
			#if !defined NDEBUG && defined DBG_CACHE
				dbglog(".xX fatbuf: 0x%08X - 0x%08X (%d/%d)", k, k + vv.size() - 1, vv.size(), container.size());
				#ifdef DBG_CACHDMP
					print();
				#endif
			#endif
			access.unlock();
			return vv.front().first;
		}
	#else
		return read(k, 1).front().first;
	#endif
}
template<typename key_t, typename value_t>
int			read_cache<key_t, value_t>::
									operator () (const key_type& k, const value_type& v) {
	#ifndef NO_CACHE
		access.lock();
		const typename container_type::left_iterator it = container.left.find(k);
		if(it != container.left.end()) {
			it->second = v;
			container.right.relocate(container.right.end(), container.project_right(it));
		}
		else {
			assert(container.size() <= capacity);
			if(container.size() == capacity)
				container.right.erase(container.right.begin());
			container.insert(typename container_type::value_type(k,v));
		}
		#if !defined NDEBUG && defined DBG_CACHE
			dbglog("XXX fatbuf: 0x%08X (%d)", k, container.size());
		#endif
		access.unlock();
	#endif
	return write(k, v);
}
#if !defined NDEBUG && defined DBG_CACHDMP
template<typename key_t, typename value_t>
void		read_cache<key_t, value_t>::
									print() {
	string res;
	size_t j = 0;
	for(const auto& i: container.right) {
		res += std::format(" {:08X}", i.second);
		if(++j % (DBGCR / 4) == 0) {
			dbglog(res + "\n");
			res.clear();
		}
	}
	dbglog(res + "\n");
}
#endif

class								console {
public:
	template <typename... Targs>
	static void						write(const std::string &, Targs...);
	template <typename... Targs>
	static void						write(const std::string &, bool, Targs...);
	static std::pair<bool, bool>	read();
};
template <typename... Targs>
void					console::	write(const std::string &s, Targs... args) {
	std::cout << std::vformat(s, std::make_format_args(args...));
}
template <typename... Targs>
void					console::	write(const std::string &s, bool err, Targs... args) {
	(err ? std::cerr : std::cout) << std::vformat(s, std::make_format_args(args...));
}

// File name validation
//
class								nameval {
private:
    static const std::unordered_set<char> fchar;
public:
    [[gnu::pure]] static int		is_valid(std::string);
};

// Macro for debug output
//
#ifndef NDEBUG
template <typename... Targs>
inline void							dbglog(const std::string &, Targs...);
#endif

#ifndef NDEBUG
template <typename... Targs>
inline void							dbglog(const std::string &s, Targs... args) {
	std::istringstream flux(std::vformat(s, std::make_format_args(args...)));
	for(std::string line; std::getline(flux, line);)
		console::write("# {:016X} {}\n", true, std::hash<std::thread::id>{}(std::this_thread::get_id()), line);
}
#endif
