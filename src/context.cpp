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

#include "context.hpp"

#include <iostream>
#include <vector>
#include <map>
#include <set>
#include <bitset>
#include <shared_mutex>
#include <mutex>

fatx_context*				fatx_context::	fatxc		= nullptr;

							fatx_context::	fatx_context(frontend& m): mmi(m), fat(nullptr), root(nullptr), ready(false) {
	set(this);
}
							fatx_context::	~fatx_context() {
	ready = false;
	destroy();
	set(nullptr);
}
int							fatx_context::	setup() {
	int res = 0;
	if((res = dev.setup()))
		return res;
	if((res = par.setup()))
		return res;
	if(mmi.prog == frontend::fsck || mmi.prog == frontend::unrm || (mmi.prog == frontend::fuse && mmi.recover))
		fat = new memmap(par);
	else
		fat = new dskmap(par);
	#if !defined NDEBUG && defined DBG_INIT
		dbglog("::EOMAP");
	#endif
	if(mmi.prog != frontend::mkfs) {
		root = new entry();
		if(mmi.prog == frontend::fuse && ready) {
			console::write("Errors found, please run fsck.fatx to correct.\n", mmi.dialog);
			return ECANCELED;
		}
		else
			ready = false;
		#if !defined NDEBUG && defined DBG_INIT
			dbglog("::EOENT");
		#endif
	}
	return res;
}
void						fatx_context::	destroy() {
	delete root;
	root = nullptr;
	delete fat;
	fat = nullptr;
}
