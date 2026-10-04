/*
 *	FATX filesystem support (Xbox 360)
 *
 *  Copyright (C) 2026 ExposureMG
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

#include "io.hpp"

#include <algorithm>
#include <cstring>

namespace fatx {

std::shared_ptr<file_io>	file_io::		open(const std::filesystem::path &path, bool writable) {
	std::shared_ptr<file_io> res(new file_io());
	res->rw = writable;
	res->stream.open(path, std::ios::binary | std::ios::in | (writable ? std::ios::out : std::ios::openmode{}));
	const bool ok = res->stream.is_open() && !res->stream.seekg(0, std::ios::end).fail();
	const auto end = ok ? res->stream.tellg() : std::streampos(-1);
	if(end < 0)
		res.reset();
	else
		res->bytes = static_cast<uint64_t>(end);
	return res;
}
bool						file_io::		read_at(uint64_t offset, std::span<std::byte> out) {
	if(offset > bytes || out.size() > bytes - offset)
		return false;
	stream.clear();
	if(stream
		.seekg(static_cast<std::streamoff>(offset))
		.read(reinterpret_cast<char*>(out.data()), static_cast<std::streamsize>(out.size()))
		.fail()
	) {
		stream.clear();
		return false;
	}
	return true;
}
bool						file_io::		write_at(uint64_t offset, std::span<const std::byte> in) {
	if(!rw || offset > bytes || in.size() > bytes - offset)
		return false;
	stream.clear();
	if(stream
		.seekp(static_cast<std::streamoff>(offset))
		.write(reinterpret_cast<const char*>(in.data()), static_cast<std::streamsize>(in.size()))
		.fail()
	) {
		stream.clear();
		return false;
	}
	return true;
}
bool						file_io::		flush() {
	if(!rw)
		return true;
	stream.clear();
	return !stream.flush().fail();
}

							memory_io::		memory_io(uint64_t size, bool writable, std::byte fill) :
	bytes(static_cast<size_t>(size), fill), rw(writable) {
}
bool						memory_io::		read_at(uint64_t offset, std::span<std::byte> out) {
	if(offset > bytes.size() || out.size() > bytes.size() - offset)
		return false;
	std::copy_n(bytes.begin() + static_cast<std::ptrdiff_t>(offset), out.size(), out.begin());
	return true;
}
bool						memory_io::		write_at(uint64_t offset, std::span<const std::byte> in) {
	if(!rw || offset > bytes.size() || in.size() > bytes.size() - offset)
		return false;
	std::copy(in.begin(), in.end(), bytes.begin() + static_cast<std::ptrdiff_t>(offset));
	return true;
}

}
