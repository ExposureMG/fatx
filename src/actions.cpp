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

#include "actions.hpp"
#include "context.hpp"

namespace fatx::actions {

int							make_filesystem() {
	auto* const ctx = fatx_context::get();
	bool status = false;
	console::write("Creating new FATX filesystem");
	status = status || ctx->par.write();
	console::write(".");
	ctx->fat->erase();
	console::write(".");
	byte_buffer zeros(static_cast<byte_buffer::size_type>(ctx->par.clus_size), std::byte{0});
	status = status || ctx->dev.write_bytes(
		ctx->par.root_start,
		byte_view(zeros.data(), zeros.size())
	);
	console::write(".");
	if(status) {
		console::write("Unable to create FATX filesystem.\n");
		return EIO;
	}
	delete ctx->root;
	ctx->root = new entry("", 0, true);
	ctx->root->parent = ctx->root;
	ctx->root->status = entry::valid;
	if(ctx->root->cluster != ctx->par.root_clus) {
		console::write("Unable to create FATX filesystem.\n");
		return EIO;
	}
	console::write("done.\n");
	console::write("FATX filesystem created with {} clusters.\n", ctx->par.clus_fat);
	return 0;
}

int							write_label(const std::string &name) {
	auto* const ctx = fatx_context::get();
	if(ctx->root == nullptr)
		return EIO;
	ctx->par.par_label = name;
	unsigned char lab[slab];
	entry *idx = ctx->root->find(flab);
	filesize s = ctx->par.label(lab);
	int res = 0;
	if(idx == nullptr) {
		entry *n = new entry(flab, 0, false);
		if((res = ctx->root->addtodir(n))) {
			delete n;
			return res;
		}
		idx = ctx->root->find(flab);
		if(idx == nullptr)
			return EIO;
		idx->flags.lab = true;
		if((res = idx->write(true)))
			return res;
	}
	if((!idx->areas || idx->areas->empty()) && idx->cluster != FLK && idx->size != 0)
		// an existing label file: its clusters are needed to resize it
		idx->areas = std::make_shared<vareas>(ctx->fat->getareas(idx->cluster).sub(idx->size));
	if((res = idx->resize(s)))
		return res;
	return idx->data(reinterpret_cast<char*>(lab), false, 0, s);
}

}
