#ifndef FATX_HPP
#define FATX_HPP
/*
 *	FATX filesystem support
 *
 *  Copyright (C) 2012-2023 Christophe Duverger
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

/*
 *  Compile with:
 *  -D NDEBUG		to avoid produce debug informations output
 *	-D DBG_INIT		to produce debug on initialisation sequence
 *	-D DBG_READ		to print bytes read at device level
 *	-D DBG_WRITE	to print bytes written at device level
 *	-D DBG_DIFF		to print accesses to diff file
 *	-D DBG_SEM		to print accesses to semaphores
 *	-D DBGSEM=\"x\"	to print only semaphore named x
 *	-D DBG_BUFFER	to print buffer operations
 *	-D DBGBUFDMP=x	to print x bytes of buffer at each change
 *	-D DBG_CACHE	to print cache operations
 *	-D DBG_CACHDMP	to dump cache at each change
 *	-D DBG_AREAS	to print fat areas
 *	-D DBG_GUESS	to print guesses
 *	-D DBGCR=x		to limit to x bytes per line
 *	-D DBGLIMIT=x	to limit to x bytes the printing of read/write
 *	-D DBG_FAT		to print the FAT
 *	-D DBG_GAPS		to print gaps
 *	-D NO_WRITE		to fake writing but no modification is done
 *	-D NO_CACHE		to disable FAT cache
 *	-D NO_SPLICE	to disable splice calls by fuse
 *
 *  Make symlink to executable with names:
 *	 "fusefatx"		for fuse filesystem support
 *	 "mkfs.fatx"	for filesystem creation
 *	 "fsck.fatx"	for filesystem check and repair
 *	 "unrm.fatx"	for recovery of deleted files
 *	 "label.fatx"	for display or change volume name
 *
 *  Use -h option for each symlink call to find syntax and options list
 */

#include <fstream>
#include <iostream>
#include <fcntl.h>
#include <time.h>
#include <filesystem>
#include <boost/program_options.hpp>
#include <boost/interprocess/sync/interprocess_upgradable_mutex.hpp>
#define FUSE_USE_VERSION 29
#include <fuse.h>

#include <string>
#include <vector>
#include <memory>
#include <list>
#include <map>
#include <set>
#include <cassert>
#include <algorithm>
#include <bitset>
#include <functional>

#include <string.h>
#include <math.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <boost/cstdint.hpp>
#include <boost/integer.hpp>

#include <boost/format.hpp>
#include <boost/tokenizer.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/bimap.hpp>
#include <boost/bimap/list_of.hpp>
#include <boost/bimap/set_of.hpp>
#include <boost/bimap/multiset_of.hpp>
#include <boost/date_time/posix_time/posix_time_types.hpp>
#include <boost/ptr_container/ptr_vector.hpp>

using namespace std;
using namespace boost;
using namespace boost::program_options;

typedef uint64_t					streamptr;		// pointers in device
typedef uint64_t					clusptr;		// cluster numbers
typedef uint64_t					filesize;		// file size

class								fatx_context;	// context that contains pointers on used instances of following classes
class								frontend;		// arguments management & options values
class								device;			// read/write to the device
class								fatxpar;		// partition identification & partition usefull values
class								dskmap;			// device file allocation table management
class								memmap;			// memory file allocation table used to handle deleted entries
class								entry;			// file or directory entry
class								vareas;			// vector of areas in fat
class								buffer;			// file buffer

typedef std::shared_ptr<vareas>		ptr_vareas;
typedef std::unique_ptr<buffer>		ptr_buffer;
typedef std::unique_ptr<entry>		ptr_entry;

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

extern const char * const			sepdir;									// using unix directories
extern const char * const			fsid;									// filesystem id
extern const char * const			fidx;									// file used for label name
extern const char * const			def_landf;								// default directory for lost & founds
extern const char * const			def_fpre;								// default file prefix for lost & founds
extern const char * const			def_label;								// default label name

// Macro for debug output
//
#ifndef NDEBUG
void								dbglog(const string &);
#endif

#if !defined NDEBUG && !defined DBGCR
	#define DBGCR					48
#endif

// Big / Little byte_order (de)formatter
//
template<int bytes = 1>
class								byte_order {
private:
	template<int big>
	class							byte_order_format : public std::string {
	public:
		typedef typename uint_t<bytes * 8>::least
									value_type;
	private:
		size_t						pos(const size_t i) const { return (big == 0) ? bytes - 1 - i : i; }
		std::string::value_type		getbyte(const value_type n, const size_t i = 0) const {
			return std::string::value_type((n & (0xFFU << (pos(i) * 8))) >> (pos(i) * 8));
		}
		value_type					setbyte(const std::string &s, const size_t i = 0) const {
			return static_cast<value_type>((1 << (8 * pos(i))) * static_cast<uint8_t>(s[i]));
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
	typedef typename byte_order_format<0>::value_type
									value_type;
	typedef byte_order_format<1>	bigend;
	typedef byte_order_format<0>	litend;
};

// Mutex management
//
class								mymutx
	: public boost::interprocess::interprocess_upgradable_mutex
{
private:
	string							nam;
	int								cpt;
public:
									mymutx(const string &n = "???") : nam(n), cpt(0) { }
									~mymutx() { nam.clear(); }
	void							name(const string &n) { nam = n; }
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
	typedef key_t					key_type;
	typedef value_t					value_type;
	typedef bimaps::bimap<
		bimaps::set_of<key_type>,
		bimaps::list_of<value_type>
	>								container_type;
	typedef pair<
		value_type,
		key_type
	>								pair_type;
	typedef vector<pair_type>		lkval_t;
	typedef std::function<
		lkval_t(
			const key_type&,
			const size_t&
		)
	>								fread_t;
	typedef std::function<
		int(
			const key_type&,
			const value_type&
		)
	>								fwrite_t;
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
	void							operator () ();
};

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
	string							print() const {
		return (format("%c%c%c%c%c%c%c-")
			% (ro	? 'R' : '-')
			% (hid	? 'H' : '-')
			% (sys	? 'S' : '-')
			% (lab	? 'L' : '-')
			% (dir	? 'D' : '-')
			% (arc	? 'A' : '-')
			% (dev	? 'V' : '-')
		).str();
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
	mode_t							operator () () {
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
	typedef uint64_t				date_t;
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
	string							print() const {
		return (format("%02d:%02d:%02d %02d/%02d/%04d")
			% hour		% min		% sec
			% day		% month		% year
		).str();
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
	time_t							operator () () {
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
		struct tm *st(localtime(&t));
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

// Data areas
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
class								vareas : public vector<area> {
public:
	clusptr							first() const;
	clusptr							last() const;
	size_t							nbcls() const;
	clusptr							at(size_t) const;
	iterator						in(size_t);
	vareas							sub(filesize, filesize = 0) const;
	void							add(vareas);
	void							add(clusptr);
	string							print() const;
};
// Data buffers
//
class								buffer : public string {
public:
	bool							touched;
	streamptr						offset;
									buffer(const streamptr = 0, const streamptr = 0);
									buffer(const buffer &) = default;
									~buffer();
	void							enlarge(const streamptr);
	void							operator () (size_t);
	buffer							&operator = (const buffer &) = default;
};

class								console {
public:
	static void						write(const string &, bool = false);
	static pair<bool, bool>			read();
};
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
	const char* const *	const		argv;
	string							progname;
	bool							dialog;
	string							lostfound;
	string							foundfile;
	unsigned int					filecount;
	string							mount;
	string							volname;
	string							fuse_option;
	vector<string>					unkopt;
	string							partition;
	string							table;
	streamptr						clus_size;
	uid_t							uid;
	gid_t							gid;
	mode_t							mask;
	bool							allyes;
	streamptr						offset;
	streamptr						size;
	string							input;
	string							script;
	string							diffile;

									frontend(int, const char* const * const);
	string							name();
	int								setup();
	void							parser();
	bool							getanswer(bool = false);
	bool							writeable() const { return !readonly; }
};
class								device {
private:
	struct							segment {
		filesize					size;
		streamptr					position;
	};
	class							chgfile : public map<streamptr, segment> {
	private:
		fstream*					iod;

		bool						addseg(streamptr, const string&);
	public:
									chgfile() : iod(nullptr) { }
		bool						load(fstream*);
		bool						read(streamptr, const size_t, string &) const;
		bool						write(streamptr, const string &);
	};
	fstream*						io;
	fstream*						iod;
	#ifndef NO_FD
	FILE*							fd;
	FILE*							fdd;
	#endif
	streamptr						tot_size;
	bool							changes;
	mymutx							authd;
	chgfile							chgf;
public:
									device();
									~device();
	int								setup();
	streamptr						size() const {
		return tot_size;
	}
	bool							modified() const {
		return changes;
	}
	#ifndef NO_FD
	FILE*							getfd() const {
		return fd;
	}
	#endif
	string							read(streamptr, size_t = blksize);
	int								write(streamptr, const string &);
	string							address(streamptr) const;
	void							devlog(bool, streamptr, const string&) const;
	string							print(streamptr, size_t = blksize, size_t = 32);
};
class								fatxpar {
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
			memcpy(&buf[4], &byte_order<4>::litend(id)[0], 4);
			memcpy(&buf[8], &byte_order<4>::litend(spc)[0], 4);
			memcpy(&buf[12], &byte_order<4>::litend(root)[0], 4);
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
									devheader(char buf[blksize]) :
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
			memcpy(&buf[0], &byte_order<4>::litend(id)[0], 4);
			memcpy(&buf[8], &byte_order<4>::litend(p2_start)[0], 4);
			memcpy(&buf[12], &byte_order<4>::litend(p2_size)[0], 4);
			memcpy(&buf[16], &byte_order<4>::litend(p1_start)[0], 4);
			memcpy(&buf[20], &byte_order<4>::litend(p1_size)[0], 4);
		}
	};
public:
	uint32_t						par_id;
	string							par_label;
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

									fatxpar();
	int								setup();
	int								write();
	size_t							label(unsigned char [slab]) const;
	void							label(const unsigned char [slab], const size_t size);
};
namespace clsarithm {
inline clusptr						siz2cls(filesize);
inline clusptr						inccls(clusptr);
inline streamptr					cls2ptr(clusptr);
inline clusptr						ptr2cls(streamptr);
inline streamptr					cls2fat(clusptr);
inline string						clsprint(clusptr, clusptr);
} // namespace clsarithm
class								dskmap
{
protected:
	typedef clusptr					mapptr_t;
	typedef clusptr					mapsiz_t;
	typedef bimaps::bimap<
		bimaps::set_of<mapptr_t>,
		bimaps::multiset_of<mapsiz_t>
	>								gap_t;
	typedef read_cache<
		clusptr,
		clusptr
	>								memnext_t;
	typedef std::function<
		void(const clusptr&, const clusptr&)
	>								lbdarea_t;
	typedef std::function<
		void(const clusptr&, const clusptr&)
	>								lbdfat_t;

	memnext_t						memnext;
	gap_t							freegaps;
	mymutx							authm;
	set<clusptr>					bad;

	void							forfat(const lbdfat_t &);
	memnext_t::lkval_t				real_read(clusptr, size_t);
	int								real_write(clusptr, clusptr);

public:
	enum							status_t {
		disk,
		deleted,
		modified,
		marked
	};
									dskmap(const fatxpar&);
	virtual							~dskmap();
	clusptr							clsavail();
	void							erase();
	void							gapcheck();
	vareas							getareas(clusptr, lbdarea_t = nullptr);
	virtual clusptr					read(clusptr);
	int								write(clusptr, clusptr);
	vareas							alloc(clusptr, clusptr = 0);
	void							freefat(clusptr);
	int								resize(ptr_vareas, clusptr);
	string							printchain(clusptr);
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
									link(entry* e, clusptr n = FLK, status_t s = marked) : next(n), ent(e), status(s) { }
	};
	typedef map<
		clusptr,
		class link
	>								memchain_t;
	typedef vector<vareas>			lost_t;
	memchain_t						memchain;
	lost_t							lost;

									memmap(const fatxpar&);
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
class								entry : boost::noncopyable {
private:
	mymutx							authb;
	mymutx							authw;
	mymutx							authe;
	ptr_buffer						entbuf;

	void							opendir();
	void							closedir();
	int								write();
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
	static const size_t				ent_pow	= 6;
private:
	template<class T>
	T								protected_read(T r) {
		authe.lock_shared();
		T res = r;
		authe.unlock_shared();
		return res;
	}
	template<class T>
	void							protected_write(T& r, const T &v) {
		authe.lock();
		r = v;
		authe.unlock();
	}
private:
	char							v_name[name_size + 1];
public:
	const char*						name() {
		return protected_read<const char* const>(v_name);
	}
	void							name(const char* a) {
		authe.lock();
		strncpy(v_name, a, name_size);
		authe.unlock();
	}
#define PROTECTED_VAR(name, type, cref) \
private: \
	type v_##name; \
public: \
	type							name() { return protected_read<cref>(v_##name); } \
	void							name(type a) { protected_write<type>(v_##name, a); }
	PROTECTED_VAR(cptacc,		int,				const int&)
	PROTECTED_VAR(writeopened,	bool,				const bool&)
	PROTECTED_VAR(status,		status_t,			const status_t&)
	PROTECTED_VAR(namesize,		uint8_t,			const uint8_t&)
	PROTECTED_VAR(flags,		attrib,				const attrib&)
	PROTECTED_VAR(cluster,		clusptr,			const clusptr &)
	PROTECTED_VAR(size,			filesize,			const filesize &)
	PROTECTED_VAR(creation,		date,				const date &)
	PROTECTED_VAR(access,		date,				const date&)
	PROTECTED_VAR(update,		date,				const date&)
	PROTECTED_VAR(loc,			streamptr,			const streamptr&)
	PROTECTED_VAR(parent,		entry*,				entry* const)
public:
	ptr_vector<entry>				childs;
	PROTECTED_VAR(areas,		ptr_vareas,			const ptr_vareas)
#undef PROTECTED_VAR
public:
									entry();
									entry(streamptr, const char[ent_size] = nullptr);
									entry(const string &, filesize = 0, const bool = false);
									~entry();

	string							print();
	string							path();
	int								addtodir(entry*);
	void							remfrdir(entry*, bool = true);
	entry*							find(const char*);
	void							touch(bool = true, bool = true, bool = true);
	int								save();
	int								rename(const char*);
	void							recover();
	void							guess();
	bool							analyse(pass_t, const string & = string(""));
	int								resize(const filesize);
	int								data(char*, bool, filesize, filesize);
	size_t							bufread(char*, filesize, filesize);
	size_t							bufwrite(const char*, filesize, filesize);
	int								flush(bool = true);
	void							open(bool w);
	void							close(bool w);
	struct fuse_bufvec*				getbufvec(streamptr, filesize);
	bool							operator == (entry&);
};
class								fatx_context {
private:
	static fatx_context*			fatxc;
public:
	frontend&						mmi;
	device							dev;
	fatxpar							par;
	dskmap*							fat;
	entry*							root;
	bool							ready;

									fatx_context(frontend&);
									~fatx_context();
	int								setup();
	void							destroy();
	static fatx_context*			get() { return fatxc; }
	static void						set(fatx_context* const fc) { fatxc = fc; }
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
					dbglog((format("... fatbuf: nothing for 0x%08X\n") % k).str());
				#endif
				access.unlock();
				return 0;
			}
			if(container.size() + vv.size() > capacity) {
				typename container_type::right_iterator b = container.right.begin();
				std::advance(b, container.size() + vv.size() - capacity);
				#if !defined NDEBUG && defined DBG_CACHE
					dbglog((format("Xx. fatbuf: reduce (%d)\n") % container.size()).str());
				#endif
				container.right.erase(container.right.begin(), b);
				#if !defined NDEBUG && defined DBG_CACHE
					#ifdef DBG_CACHDMP
						(*this)();
					#endif
				#endif
			}
			container.insert(typename container_type::value_type(vv.front().second, vv.front().first));
			container.right.insert(container.right.begin(), vv.begin() + 1, vv.end());
			#if !defined NDEBUG && defined DBG_CACHE
				dbglog((format(".xX fatbuf: 0x%08X - 0x%08X (%d/%d)\n") % k % (k + vv.size() - 1) % vv.size() % container.size()).str());
				#ifdef DBG_CACHDMP
					(*this)();
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
			dbglog((format("XXX fatbuf: 0x%08X (%d)\n") % k % container.size()).str());
		#endif
		access.unlock();
	#endif
	return write(k, v);
}
#if !defined NDEBUG && defined DBG_CACHDMP
template<typename key_t, typename value_t>
void		read_cache<key_t, value_t>::
									operator () () {
	string res;
	size_t j = 0;
	access.lock();
	for(const auto& i: container.right) {
		res += (format(" %08X") % i.second).str();
		if(++j % (DBGCR / 4) == 0) {
			dbglog(res + "\n");
			res.clear();
		}
	}
	dbglog(res + "\n");
	access.unlock();
}
#endif

#endif
