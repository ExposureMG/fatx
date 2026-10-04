#pragma once
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

// Pluggable backing store for a FATX device.
//
// The command-line tools open their input with std::fstream. An embedding
// application can instead give the library an io_backend (frontend::backend):
// a disk image, a raw drive opened by a privileged helper, an Android file
// descriptor, ... The library never opens or closes it itself.

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace fatx {

class								io_backend {
public:
	virtual							~io_backend() = default;
	// Size of the device in bytes.
	[[nodiscard]] virtual uint64_t	size() const = 0;
	// Whether write_at() may be used.
	[[nodiscard]] virtual bool		writable() const = 0;
	// Read exactly out.size() bytes at offset. Returns false on any error.
	[[nodiscard]] virtual bool		read_at(uint64_t offset, std::span<std::byte> out) = 0;
	// Write all of in at offset. Returns false on any error.
	[[nodiscard]] virtual bool		write_at(uint64_t offset, std::span<const std::byte> in) = 0;
	// Push buffered writes to the device. Returns false on error.
	[[nodiscard]] virtual bool		flush() = 0;
};

// A file (disk image or device node) opened through the C++ standard library.
class								file_io final : public io_backend {
private:
	std::fstream					stream;
	uint64_t						bytes;
	bool							rw;
									file_io() : bytes(0), rw(false) { }
public:
	// Returns nullptr when the file can't be opened (in write mode if asked).
	static std::shared_ptr<file_io>	open(const std::filesystem::path &, bool writable);
	uint64_t						size() const override { return bytes; }
	bool							writable() const override { return rw; }
	bool							read_at(uint64_t, std::span<std::byte>) override;
	bool							write_at(uint64_t, std::span<const std::byte>) override;
	bool							flush() override;
};

// A device held in memory (tests, scratch volumes).
class								memory_io final : public io_backend {
private:
	std::vector<std::byte>			bytes;
	bool							rw;
public:
	explicit						memory_io(uint64_t size, bool writable = true, std::byte fill = std::byte{0});
	uint64_t						size() const override { return bytes.size(); }
	bool							writable() const override { return rw; }
	void							set_writable(bool w) { rw = w; }
	bool							read_at(uint64_t, std::span<std::byte>) override;
	bool							write_at(uint64_t, std::span<const std::byte>) override;
	bool							flush() override { return true; }
	std::vector<std::byte>&			data() { return bytes; }
};

}
