// Tests of the embedding seams of fatx_core: log sink, per-thread contexts,
// io_backend and the volume API. They link fatx_core only (no FUSE, no
// Boost.Program_options).

#include <gtest/gtest.h>

#include <atomic>
#include <cstdlib>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include "context.hpp"
#include "volume.hpp"

namespace {

std::vector<std::byte> bytes_of(const std::string &s) {
	std::vector<std::byte> b(s.size());
	for(size_t i = 0; i < s.size(); i++)
		b[i] = static_cast<std::byte>(s[i]);
	return b;
}

std::vector<std::byte> random_bytes(size_t n, unsigned seed) {
	std::mt19937 gen(seed);
	std::vector<std::byte> b(n);
	for(auto &x: b)
		x = static_cast<std::byte>(gen() & 0xFF);
	return b;
}

std::shared_ptr<fatx::memory_io> formatted(uint64_t size, const std::string &label = "") {
	auto dev = std::make_shared<fatx::memory_io>(size);
	EXPECT_EQ(fatx::format(dev, fatx::location{}, label, 0, {}), 0);
	return dev;
}

std::unique_ptr<fatx::volume> open_rw(const std::shared_ptr<fatx::io_backend> &dev, std::vector<std::string> *log = nullptr) {
	int err = -1;
	auto v = fatx::volume::open(dev, fatx::location{}, true, [log] (fatx::log_level, const std::string &l) {
		if(log)
			log->push_back(l);
	}, &err);
	EXPECT_EQ(err, 0);
	return v;
}

std::vector<std::byte> read_all(fatx::volume &v, const std::string &path) {
	fatx::entry_info info;
	EXPECT_EQ(v.lookup(path, info), 0);
	std::vector<std::byte> out(info.size);
	size_t got = 0;
	EXPECT_EQ(v.read(path, 0, out, &got), 0);
	EXPECT_EQ(got, info.size);
	return out;
}

// A file in the temporary directory, removed at the end of the test.
struct temp_file {
	std::filesystem::path path;
	explicit temp_file(const std::string &name, uint64_t size) :
		path(std::filesystem::temp_directory_path() / (name + "." + std::to_string(::testing::UnitTest::GetInstance()->random_seed()) + ".img")) {
		std::ofstream(path, std::ios::binary | std::ios::trunc).close();
		std::filesystem::resize_file(path, size);	// sparse where supported
	}
	~temp_file() { std::error_code ec; std::filesystem::remove(path, ec); }
};

}

// --- log sink ---------------------------------------------------------------

TEST(LogSink, DefaultWritesToStdoutAndStderr) {
	testing::internal::CaptureStdout();
	testing::internal::CaptureStderr();
	console::write("info {}\n", 1);
	console::write("error {}\n", true, 2);
	EXPECT_EQ(testing::internal::GetCapturedStdout(), "info 1\n");
	EXPECT_EQ(testing::internal::GetCapturedStderr(), "error 2\n");
}

TEST(LogSink, GlobalAndScopedSinks) {
	std::vector<std::pair<console::level, std::string>> global, scoped, inner;
	console::set_sink([&] (console::level l, const std::string &s) { global.emplace_back(l, s); });
	console::write("a\n");
	{
		const console::sink_t s = [&] (console::level l, const std::string &m) { scoped.emplace_back(l, m); };
		console::scoped_sink guard(s);
		console::write("b {}\n", true, 42);
		{
			const console::sink_t s2 = [&] (console::level l, const std::string &m) { inner.emplace_back(l, m); };
			console::scoped_sink guard2(s2);
			console::emit(console::level::debug, "c\n");
		}
		console::write("d\n");
		// another thread does not see this thread's sink
		std::thread([] { console::write("e\n"); }).join();
	}
	console::write("f\n");
	console::set_sink({});
	ASSERT_EQ(global.size(), 3u);
	EXPECT_EQ(global[0].second, "a\n");
	EXPECT_EQ(global[1].second, "e\n");
	EXPECT_EQ(global[2].second, "f\n");
	ASSERT_EQ(scoped.size(), 2u);
	EXPECT_EQ(scoped[0].first, console::level::error);
	EXPECT_EQ(scoped[0].second, "b 42\n");
	EXPECT_EQ(scoped[1].second, "d\n");
	ASSERT_EQ(inner.size(), 1u);
	EXPECT_EQ(inner[0].first, console::level::debug);
}

// --- contexts ---------------------------------------------------------------

TEST(ContextScope, ScopeOverridesProcessContext) {
	const char *argv[] = { "test", nullptr };
	frontend m1(1, argv), m2(1, argv);
	fatx_context *global = new fatx_context(m1);	// no scope: becomes the process context
	EXPECT_EQ(fatx_context::get(), global);
	{
		fatx_context::scope s;
		EXPECT_EQ(fatx_context::get(), nullptr);
		fatx_context local(m2);						// scope active: becomes the scope's context
		EXPECT_EQ(fatx_context::get(), &local);
		std::thread([global] { EXPECT_EQ(fatx_context::get(), global); }).join();
		{
			fatx_context::scope inner(global);
			EXPECT_EQ(fatx_context::get(), global);
		}
		EXPECT_EQ(fatx_context::get(), &local);
	}
	EXPECT_EQ(fatx_context::get(), global);
	delete global;
	EXPECT_EQ(fatx_context::get(), nullptr);
}

TEST(ContextScope, VolumesOnSeveralThreads) {
	auto a = formatted(32 << 20, "VOLA");
	auto b = formatted(24 << 20, "VOLB");
	auto va = open_rw(a), vb = open_rw(b);
	ASSERT_TRUE(va && vb);
	std::atomic<int> failures = 0;
	auto work = [&failures] (fatx::volume *v, const std::string &tag) {
		for(int i = 0; i < 40; i++) {
			const std::string dir = "/" + tag + std::to_string(i);
			const auto data = random_bytes(5000 + static_cast<size_t>(i) * 300, static_cast<unsigned>(i));
			if(v->mkdir(dir) || v->create(dir + "/f.bin", data.size()) || v->write(dir + "/f.bin", 0, data))
				failures++;
			std::vector<std::byte> back(data.size());
			size_t got = 0;
			if(v->read(dir + "/f.bin", 0, back, &got) || got != data.size() || back != data)
				failures++;
		}
	};
	std::thread ta(work, va.get(), "a"), tb(work, vb.get(), "b");
	ta.join();
	tb.join();
	EXPECT_EQ(failures, 0);
	EXPECT_EQ(va->info().label, "VOLA");
	EXPECT_EQ(vb->info().label, "VOLB");
	std::vector<fatx::entry_info> la, lb;
	ASSERT_EQ(va->list("/", la), 0);
	ASSERT_EQ(vb->list("/", lb), 0);
	EXPECT_EQ(la.size(), 41u);	// 40 folders + name.txt
	EXPECT_EQ(lb.size(), 41u);
	EXPECT_TRUE(std::ranges::none_of(la, [] (const auto &e) noexcept { return e.name.starts_with("b"); }));
}

// --- io_backend -------------------------------------------------------------

TEST(IoBackend, MemoryBounds) {
	fatx::memory_io m(1024);
	std::vector<std::byte> b(16);
	EXPECT_TRUE(m.read_at(1008, b));
	EXPECT_FALSE(m.read_at(1009, b));
	EXPECT_FALSE(m.read_at(UINT64_MAX - 4, b));
	EXPECT_TRUE(m.write_at(0, b));
	m.set_writable(false);
	EXPECT_FALSE(m.write_at(0, b));
}

TEST(IoBackend, FileIo) {
	EXPECT_EQ(fatx::file_io::open("/nonexistent/fatx/image", false), nullptr);
	temp_file f("fatx_io", 4096);
	auto ro = fatx::file_io::open(f.path, false);
	ASSERT_NE(ro, nullptr);
	EXPECT_EQ(ro->size(), 4096u);
	EXPECT_FALSE(ro->writable());
	const auto data = bytes_of("XTAF");
	EXPECT_FALSE(ro->write_at(0, data));
	auto rw = fatx::file_io::open(f.path, true);
	ASSERT_NE(rw, nullptr);
	EXPECT_TRUE(rw->write_at(100, data));
	EXPECT_FALSE(rw->write_at(4094, data));
	EXPECT_TRUE(rw->flush());
	std::vector<std::byte> back(4);
	EXPECT_TRUE(ro->read_at(100, back));
	EXPECT_EQ(back, data);
}

// An io_backend whose reads start failing on demand.
class failing_io final : public fatx::io_backend {
public:
	fatx::memory_io inner;
	bool fail = false;
	explicit failing_io(uint64_t n) : inner(n) { }
	uint64_t size() const override { return inner.size(); }
	bool writable() const override { return true; }
	bool read_at(uint64_t o, std::span<std::byte> b) override { return !fail && inner.read_at(o, b); }
	bool write_at(uint64_t o, std::span<const std::byte> b) override { return !fail && inner.write_at(o, b); }
	bool flush() override { return !fail; }
};

TEST(IoBackend, DeviceErrorsAreReported) {
	auto dev = std::make_shared<failing_io>(16 << 20);
	ASSERT_EQ(fatx::format(dev, {}, "", 0, {}), 0);
	dev->fail = true;
	int err = 0;
	std::vector<std::string> log;
	auto v = fatx::volume::open(dev, {}, false, [&log] (fatx::log_level, const std::string &l) { log.push_back(l); }, &err);
	EXPECT_EQ(v, nullptr);
	EXPECT_NE(err, 0);
	EXPECT_FALSE(log.empty());
	dev->fail = false;
	v = fatx::volume::open(dev, {}, false, {}, &err);
	ASSERT_NE(v, nullptr);
	EXPECT_EQ(err, 0);
}

TEST(IoBackend, WritableVolumeNeedsWritableDevice) {
	auto dev = formatted(16 << 20);
	dev->set_writable(false);
	int err = 0;
	EXPECT_EQ(fatx::volume::open(dev, {}, true, {}, &err), nullptr);
	EXPECT_EQ(err, EROFS);
	auto v = fatx::volume::open(dev, {}, false, {}, &err);
	ASSERT_NE(v, nullptr);
	EXPECT_EQ(v->mkdir("/x"), EROFS);
	EXPECT_EQ(fatx::format(dev, {}, "", 0, {}), EROFS);
}

// --- volume API -------------------------------------------------------------

TEST(Volume, FormatAndInfo) {
	auto dev = formatted(32 << 20, "MYDISK");
	int err = -1;
	auto v = fatx::volume::open(dev, {}, false, {}, &err);
	ASSERT_NE(v, nullptr);
	const auto i = v->info();
	EXPECT_EQ(i.label, "MYDISK");
	EXPECT_EQ(i.partition_offset, 0u);
	EXPECT_EQ(i.partition_size, 32u << 20);
	EXPECT_GT(i.cluster_size, 0u);
	EXPECT_GT(i.cluster_count, 0u);
	EXPECT_GT(i.free_clusters, 0u);
	EXPECT_LT(i.free_clusters, i.cluster_count);
	EXPECT_EQ(i.root_cluster, 1u);
	EXPECT_FALSE(i.writable);
	EXPECT_FALSE(i.inconsistent);
	std::vector<fatx::entry_info> root;
	ASSERT_EQ(v->list("/", root), 0);
	ASSERT_EQ(root.size(), 1u);
	EXPECT_EQ(root[0].name, "name.txt");
	EXPECT_TRUE(root[0].label);
	fatx::entry_info st;
	EXPECT_EQ(v->lookup("/", st), 0);
	EXPECT_TRUE(st.directory);
	EXPECT_EQ(v->lookup("/missing", st), ENOENT);
	EXPECT_EQ(v->list("/name.txt", root), ENOTDIR);
}

TEST(Volume, NotFatx) {
	auto dev = std::make_shared<fatx::memory_io>(8 << 20);
	int err = 0;
	EXPECT_EQ(fatx::volume::open(dev, {}, false, {}, &err), nullptr);
	EXPECT_NE(err, 0);
	EXPECT_EQ(fatx::volume::open(dev, fatx::location{"nope", "x2"}, false, {}, &err), nullptr);
	EXPECT_EQ(err, EINVAL);
	EXPECT_EQ(fatx::volume::open(nullptr, {}, false, {}, &err), nullptr);
	EXPECT_EQ(err, EINVAL);
}

TEST(Volume, WriteReadRenameRemove) {
	auto dev = formatted(32 << 20);
	{
		auto v = open_rw(dev);
		ASSERT_TRUE(v);
		ASSERT_EQ(v->mkdir("/Content"), 0);
		ASSERT_EQ(v->mkdir("/Content/0000000000000000"), 0);
		EXPECT_EQ(v->mkdir("/content"), EEXIST);		// differs only in case
		EXPECT_EQ(v->mkdir("/missing/dir"), ENOENT);
		const auto data = random_bytes(300000, 7);
		ASSERT_EQ(v->create("/Content/game.bin", data.size()), 0);
		ASSERT_EQ(v->write("/Content/game.bin", 0, std::span(data).first(100000)), 0);
		ASSERT_EQ(v->write("/Content/game.bin", 100000, std::span(data).subspan(100000)), 0);
		EXPECT_EQ(read_all(*v, "/Content/game.bin"), data);
		// partial reads at odd offsets
		std::vector<std::byte> part(777);
		size_t got = 0;
		ASSERT_EQ(v->read("/Content/game.bin", 123457, part, &got), 0);
		EXPECT_EQ(got, part.size());
		EXPECT_TRUE(std::equal(part.begin(), part.end(), data.begin() + 123457));
		ASSERT_EQ(v->read("/Content/game.bin", data.size() - 10, part, &got), 0);
		EXPECT_EQ(got, 10u);
		ASSERT_EQ(v->read("/Content/game.bin", data.size() + 10, part, &got), 0);
		EXPECT_EQ(got, 0u);
		EXPECT_EQ(v->read("/Content", 0, part, &got), EISDIR);
		// empty file
		ASSERT_EQ(v->create("/empty", 0), 0);
		EXPECT_EQ(read_all(*v, "/empty").size(), 0u);
		EXPECT_EQ(v->create("/EMPTY", 0), EEXIST);
		// rename in place, case-only rename, move
		ASSERT_EQ(v->rename("/Content/game.bin", "/Content/default.xex"), 0);
		ASSERT_EQ(v->rename("/Content/default.xex", "/Content/Default.xex"), 0);
		ASSERT_EQ(v->rename("/Content/Default.xex", "/Content/0000000000000000/Default.xex"), 0);
		ASSERT_EQ(v->rename("/Content/0000000000000000/Default.xex", "/Default.xex"), 0);
		EXPECT_EQ(v->rename("/Content", "/Content/0000000000000000/x"), EINVAL);	// into itself
		EXPECT_EQ(v->rename("/empty", "/Default.xex"), EEXIST);
		ASSERT_EQ(v->rename("/empty", "/Default.xex", true), 0);	// replaces the file
		fatx::entry_info st;
		ASSERT_EQ(v->lookup("/Default.xex", st), 0);
		EXPECT_EQ(st.size, 0u);
		EXPECT_EQ(v->lookup("/empty", st), ENOENT);
		// remove a file and a folder tree
		ASSERT_EQ(v->create("/Content/0000000000000000/a", 5), 0);
		ASSERT_EQ(v->remove("/Content"), 0);
		EXPECT_EQ(v->lookup("/Content", st), ENOENT);
		EXPECT_EQ(v->remove("/name.txt"), EPERM);
		EXPECT_EQ(v->remove("/"), EINVAL);
		ASSERT_EQ(v->flush(), 0);
	}
	// everything is on the device: reopen, and fsck finds nothing to fix
	int err = -1;
	auto v = fatx::volume::open(dev, {}, false, {}, &err);
	ASSERT_NE(v, nullptr);
	EXPECT_EQ(v->info().label, "XBOX");
	std::vector<fatx::entry_info> root;
	ASSERT_EQ(v->list("/", root), 0);
	EXPECT_EQ(root.size(), 2u);
	v.reset();
	fatx::check_report report;
	ASSERT_EQ(fatx::check(dev, {}, false, report), 0);
	EXPECT_EQ(report.problems, 0u) << (report.messages.empty() ? "" : report.messages.front());
}

TEST(Volume, NameValidation) {
	EXPECT_EQ(fatx::validate_name("default.xex"), 0);
	EXPECT_EQ(fatx::validate_name("Name with spaces (1) [x]"), 0);
	EXPECT_EQ(fatx::validate_name(std::string(42, 'a')), 0);
	EXPECT_EQ(fatx::validate_name(std::string(43, 'a')), ENAMETOOLONG);
	EXPECT_EQ(fatx::validate_name(""), EINVAL);
	EXPECT_EQ(fatx::validate_name("."), EINVAL);
	EXPECT_EQ(fatx::validate_name(".."), EINVAL);
	for(const char *bad: { "a/b", "a\\b", "a:b", "a*b", "a?b", "a\"b", "a<b", "a>b", "a|b", "a+b", "a,b", "a;b", "a=b", "\xc3\xa9" })
		EXPECT_EQ(fatx::validate_name(bad), EINVAL) << bad;
	auto dev = formatted(16 << 20);
	auto v = open_rw(dev);
	EXPECT_EQ(v->mkdir("/a:b"), EINVAL);
	EXPECT_EQ(v->create("/" + std::string(43, 'x'), 1), ENAMETOOLONG);
	EXPECT_EQ(v->create("/big", 0x100000000ULL), EFBIG);
	EXPECT_EQ(v->set_label(std::string(43, 'x')), ENAMETOOLONG);
}

TEST(Volume, CheckFindsAndRepairsProblems) {
	auto dev = formatted(16 << 20);
	{
		auto v = open_rw(dev);
		ASSERT_EQ(v->mkdir("/dir"), 0);
	}
	// an allocated FAT chain that no entry uses (a lost chain)
	int err = 0;
	auto v = fatx::volume::open(dev, {}, false, {}, &err);
	const auto i = v->info();
	v.reset();
	const uint64_t lost = 100;
	const auto eoc = i.fat_entry_size == 2 ? bytes_of("\xFF\xFF") : bytes_of("\xFF\xFF\xFF\xFF");
	ASSERT_TRUE(dev->write_at(i.fat_offset + lost * i.fat_entry_size, eoc));
	fatx::check_report report;
	ASSERT_EQ(fatx::check(dev, {}, false, report), 0);
	EXPECT_EQ(report.problems, 1u);
	EXPECT_FALSE(report.repaired);
	ASSERT_EQ(fatx::check(dev, {}, true, report), 0);
	EXPECT_EQ(report.problems, 1u);
	EXPECT_TRUE(report.repaired);
	ASSERT_EQ(fatx::check(dev, {}, false, report), 0);
	EXPECT_EQ(report.problems, 0u);
}

TEST(Volume, ProbeLayouts) {
	// a bare partition image
	auto plain = formatted(16 << 20);
	auto found = fatx::probe(plain);
	ASSERT_EQ(found.size(), 1u);
	EXPECT_EQ(found[0].where.table, "file");
	EXPECT_EQ(found[0].where.partition, "x2");
	EXPECT_TRUE(fatx::probe(std::make_shared<fatx::memory_io>(1 << 20)).empty());

	// a memory unit: system cache + data
	auto mu = std::make_shared<fatx::memory_io>(64 << 20);
	ASSERT_EQ(fatx::format(mu, fatx::location{"mu", "sc", 0, 0}, "", 0, {}), 0);
	ASSERT_EQ(fatx::format(mu, fatx::location{"mu", "x2", 0, 0}, "MU", 0, {}), 0);
	found = fatx::probe(mu);
	ASSERT_EQ(found.size(), 2u);
	EXPECT_EQ(found[0].where.table, "mu");

	// a retail hard disk (sparse file): data partition at 0x130EB0000
	temp_file disk("fatx_hd", 0x130EB0000ULL + (64ULL << 20));
	auto hd = fatx::file_io::open(disk.path, true);
	ASSERT_NE(hd, nullptr);
	ASSERT_EQ(fatx::format(hd, fatx::location{"hd", "x2", 0, 0}, "DATA", 0, {}), 0);
	ASSERT_EQ(fatx::format(hd, fatx::location{"hd", "x1", 0, 0}, "COMPAT", 0, {}), 0);
	found = fatx::probe(hd);
	ASSERT_EQ(found.size(), 2u);
	for(const auto &p: found) {
		EXPECT_EQ(p.where.table, "hd");
		int err = -1;
		auto v = fatx::volume::open(hd, p.where, false, {}, &err);
		ASSERT_NE(v, nullptr) << p.name;
		EXPECT_EQ(v->info().partition_offset, p.offset);
		EXPECT_EQ(v->info().label, p.where.partition == "x2" ? "DATA" : "COMPAT");
	}
}

// --- bugs found while embedding -----------------------------------------------

TEST(Fixes, ReadEveryOffsetAndLength) {
	// reads that start on the last byte of a cluster area
	auto dev = formatted(16 << 20);
	auto v = open_rw(dev);
	const auto data = random_bytes(3 * 16384 + 1, 3);
	ASSERT_EQ(v->create("/f", data.size()), 0);
	ASSERT_EQ(v->write("/f", 0, data), 0);
	const uint32_t cs = v->info().cluster_size;
	v.reset();
	v = open_rw(dev);		// cluster areas read back from the FAT
	for(uint64_t off: { uint64_t(0), uint64_t(cs - 1), uint64_t(cs), data.size() - 1, data.size() - 2 }) {
		for(size_t len: { size_t(1), size_t(2), size_t(cs), size_t(cs + 1) }) {
			std::vector<std::byte> out(len, std::byte{0xAA});
			size_t got = 0;
			ASSERT_EQ(v->read("/f", off, out, &got), 0);
			ASSERT_EQ(got, std::min<size_t>(len, data.size() - off)) << off << " " << len;
			EXPECT_TRUE(std::equal(out.begin(), out.begin() + static_cast<long>(got), data.begin() + static_cast<long>(off))) << off << " " << len;
		}
	}
}

namespace {
void expect_clean(const std::shared_ptr<fatx::io_backend> &dev) {
	fatx::check_report report;
	ASSERT_EQ(fatx::check(dev, {}, false, report), 0);
	std::string all;
	for(const auto &m: report.messages)
		all += m + "\n";
	EXPECT_EQ(report.problems, 0u) << all;
}
}

TEST(Fixes, GrowWithinACluster) {
	auto dev = formatted(16 << 20);
	auto v = open_rw(dev);
	const auto data = random_bytes(300, 1);
	ASSERT_EQ(v->create("/f", 0), 0);
	ASSERT_EQ(v->write("/f", 0, std::span(data).first(100)), 0);
	ASSERT_EQ(v->write("/f", 100, std::span(data).subspan(100)), 0);	// same cluster count
	EXPECT_EQ(read_all(*v, "/f"), data);
	ASSERT_EQ(v->set_label("NEW LABEL"), 0);						// name.txt grows in its cluster
	ASSERT_EQ(v->set_label("X"), 0);
	v.reset();
	v = open_rw(dev);
	EXPECT_EQ(v->info().label, "X");
	EXPECT_EQ(read_all(*v, "/f"), data);
	v.reset();
	expect_clean(dev);
}

TEST(Fixes, GrowFragmentedFromUnalignedSize) {
	auto dev = formatted(16 << 20);
	auto v = open_rw(dev);
	const uint32_t cs = v->info().cluster_size;
	const auto data = random_bytes(5 * cs + 123, 2);
	ASSERT_EQ(v->create("/a", 100), 0);
	ASSERT_EQ(v->write("/a", 0, std::span(data).first(100)), 0);
	ASSERT_EQ(v->create("/b", cs), 0);			// right after /a: /a can't grow in place
	ASSERT_EQ(v->write("/a", 100, std::span(data).subspan(100, 2 * cs)), 0);
	ASSERT_EQ(v->write("/a", 100 + 2 * cs, std::span(data).subspan(100 + 2 * cs)), 0);
	EXPECT_EQ(read_all(*v, "/a"), data);
	v.reset();
	v = open_rw(dev);
	EXPECT_EQ(read_all(*v, "/a"), data);
	v.reset();
	expect_clean(dev);
}

TEST(Fixes, ShrinkFreesClusters) {
	auto dev = formatted(16 << 20);
	auto v = open_rw(dev);
	const uint32_t cs = v->info().cluster_size;
	const auto data = random_bytes(10 * cs, 3);
	ASSERT_EQ(v->create("/f", data.size()), 0);
	ASSERT_EQ(v->write("/f", 0, data), 0);
	const uint64_t before = v->info().free_clusters;
	ASSERT_EQ(v->truncate("/f", cs + 5), 0);
	EXPECT_EQ(v->info().free_clusters, before + 8);
	EXPECT_EQ(read_all(*v, "/f"), std::vector<std::byte>(data.begin(), data.begin() + cs + 5));
	ASSERT_EQ(v->truncate("/f", 0), 0);
	EXPECT_EQ(v->info().free_clusters, before + 10);
	v.reset();
	expect_clean(dev);
}

TEST(Fixes, AllocateFromALargerMiddleGap) {
	auto dev = formatted(8 << 20);
	auto v = open_rw(dev);
	const uint64_t cs = v->info().cluster_size;
	const uint64_t free = v->info().free_clusters;
	ASSERT_GT(free, 100u);
	auto fill = [&] (const std::string &p, uint64_t clusters, unsigned seed) {
		const auto d = random_bytes(clusters * cs, seed);
		ASSERT_EQ(v->create(p, d.size()), 0) << p;
		ASSERT_EQ(v->write(p, 0, d), 0) << p;
	};
	fill("/a", 10, 1);
	fill("/b", 40, 2);
	fill("/c", free - 10 - 40 - 5, 3);		// leaves a last gap of 5 clusters
	ASSERT_EQ(v->remove("/b"), 0);			// a 40-cluster gap in the middle
	fill("/d", 20, 4);						// no exact fit, last gap too small
	fill("/e", 15, 5);
	fill("/f", 5, 6);
	v.reset();
	v = open_rw(dev);
	EXPECT_EQ(read_all(*v, "/a"), random_bytes(10 * cs, 1));
	EXPECT_EQ(read_all(*v, "/c"), random_bytes((free - 55) * cs, 3));
	EXPECT_EQ(read_all(*v, "/d"), random_bytes(20 * cs, 4));
	EXPECT_EQ(read_all(*v, "/e"), random_bytes(15 * cs, 5));
	EXPECT_EQ(read_all(*v, "/f"), random_bytes(5 * cs, 6));
	EXPECT_EQ(v->create("/g", 6 * cs), ENOSPC);	// 5 clusters left
	EXPECT_EQ(v->create("/g", 5 * cs), 0);
	EXPECT_EQ(v->info().free_clusters, 0u);
	EXPECT_EQ(v->create("/h", 1), ENOSPC);
	v.reset();
	expect_clean(dev);
}

TEST(Fixes, MoveToRoot) {
	auto dev = formatted(16 << 20);
	auto v = open_rw(dev);
	ASSERT_EQ(v->mkdir("/d"), 0);
	ASSERT_EQ(v->mkdir("/d/e"), 0);
	ASSERT_EQ(v->create("/d/e/f", 10), 0);
	ASSERT_EQ(v->rename("/d/e", "/e"), 0);
	fatx::entry_info st;
	EXPECT_EQ(v->lookup("/e/f", st), 0);
	EXPECT_EQ(v->lookup("/d/e", st), ENOENT);
	v.reset();
	expect_clean(dev);
}

// Hostile or damaged images must give errors, not crashes or hangs.
TEST(Fixes, CorruptedImages) {
	auto pristine = formatted(4 << 20);
	{
		auto v = open_rw(pristine);
		ASSERT_EQ(v->mkdir("/dir"), 0);
		ASSERT_EQ(v->mkdir("/dir/sub"), 0);
		for(int i = 0; i < 12; i++) {
			const auto d = random_bytes(static_cast<size_t>(i) * 3000 + 1, static_cast<unsigned>(i));
			const std::string p = (i % 2 ? "/dir/f" : "/dir/sub/g") + std::to_string(i);
			ASSERT_EQ(v->create(p, d.size()), 0);
			ASSERT_EQ(v->write(p, 0, d), 0);
		}
	}
	int err = 0;
	const auto info = fatx::volume::open(pristine, {}, false, {}, &err)->info();
	const uint64_t metadata_end = info.data_offset + 8 * info.cluster_size;
	// FATX_FUZZ_ROUNDS / FATX_FUZZ_SEED run a longer campaign
	const char *rounds_env = std::getenv("FATX_FUZZ_ROUNDS");
	const char *seed_env = std::getenv("FATX_FUZZ_SEED");
	const int rounds = rounds_env ? std::atoi(rounds_env) : 300;
	std::mt19937_64 gen(seed_env ? std::strtoull(seed_env, nullptr, 10) : 1234);
	std::function<void(fatx::volume &, const std::string &, int)> walk = [&walk] (fatx::volume &v, const std::string &dir, int depth) {
		std::vector<fatx::entry_info> l;
		if(depth > 8 || v.list(dir, l))
			return;
		for(const auto &e: l) {
			const std::string p = (dir == "/" ? "" : dir) + "/" + e.name;
			if(e.directory)
				walk(v, p, depth + 1);
			else {
				std::vector<std::byte> b(std::min<uint64_t>(e.size, 1 << 20));
				size_t got = 0;
				void(v.read(p, 0, b, &got));
				if(e.size > 10)
					void(v.read(p, e.size - 1, b, &got));
			}
		}
	};
	for(int round = 0; round < rounds; round++) {
		auto dev = std::make_shared<fatx::memory_io>(0);
		dev->data() = pristine->data();
		if(round % 10 == 9) {
			dev->data().resize(gen() % pristine->size());	// truncated image
		}
		else {
			const int flips = 1 + static_cast<int>(gen() % 16);
			for(int i = 0; i < flips; i++) {
				// mostly in the boot sector, FAT and first directory clusters
				const uint64_t at = (gen() % 4 == 0) ? gen() % 512 : gen() % metadata_end;
				dev->data()[at] = static_cast<std::byte>(gen() & 0xFF);
			}
		}
		auto v = fatx::volume::open(dev, {}, false, {}, &err);
		if(v) {
			void(v->info());
			walk(*v, "/", 0);
		}
		v.reset();
		fatx::check_report report;
		void(fatx::check(dev, {}, false, report));
		v = fatx::volume::open(dev, {}, true, {}, &err);
		if(v) {
			void(v->mkdir("/new"));
			void(v->create("/dir/new", 5000));
			void(v->remove("/dir/f1"));
			void(v->rename("/dir/f3", "/f3"));
		}
	}
}

TEST(Fixes, BracesInNames) {
	// '{' and '}' are valid in names and must not be read as format strings
	auto dev = formatted(16 << 20);
	{
		auto v = open_rw(dev);
		ASSERT_EQ(v->mkdir("/{x}"), 0);
		ASSERT_EQ(v->create("/{x}/a}b{", 3), 0);
	}
	auto v = open_rw(dev);
	fatx::entry_info st;
	EXPECT_EQ(v->lookup("/{x}/a}b{", st), 0);
	ASSERT_EQ(v->rename("/{x}/a}b{", "/{}"), 0);
	v.reset();
	expect_clean(dev);
}

// --- in-place replacement -------------------------------------------------------

namespace {
struct snapshot {
	uint64_t entry_offset = 0;
	uint32_t first_cluster = 0;
	std::vector<uint32_t> chain;
	std::string name;
	std::time_t created = 0;
};
snapshot snap(fatx::volume &v, const std::string &path) {
	snapshot s;
	fatx::entry_info i;
	EXPECT_EQ(v.lookup(path, i), 0);
	s.entry_offset = i.entry_offset;
	s.first_cluster = i.first_cluster;
	s.name = i.name;
	s.created = i.created;
	EXPECT_EQ(v.clusters(path, s.chain), 0);
	return s;
}
// replace() then write the new contents, as an application does in its finish()
int replace_with(fatx::volume &v, const std::string &path, const std::vector<std::byte> &data) {
	if(int r = v.replace(path, data.size()))
		return r;
	return data.empty() ? 0 : v.write(path, 0, data);
}
}

TEST(Replace, KeepsTheEntryAndItsClusters) {
	auto dev = formatted(16 << 20);
	auto v = open_rw(dev);
	const uint32_t cs = v->info().cluster_size;
	// three files, the middle one fragmented so its chain is not one run
	ASSERT_EQ(v->mkdir("/dir"), 0);
	ASSERT_EQ(v->create("/dir/a", cs), 0);
	ASSERT_EQ(v->create("/dir/target", 0), 0);
	ASSERT_EQ(v->write("/dir/target", 0, random_bytes(cs, 1)), 0);
	ASSERT_EQ(v->create("/dir/b", cs), 0);
	ASSERT_EQ(v->write("/dir/target", cs, random_bytes(2 * cs + 100, 2)), 0);	// grows elsewhere
	ASSERT_EQ(v->create("/dir/c", 10), 0);
	const auto before = snap(*v, "/dir/target");
	ASSERT_EQ(before.chain.size(), 4u);
	EXPECT_NE(before.chain[1], before.chain[0] + 1);	// fragmented
	uint64_t room = 0;
	ASSERT_EQ(v->replace_capacity("/dir/target", &room), 0);
	EXPECT_EQ(room, 4u * cs);

	// same size: everything stays, new contents
	const auto same = random_bytes(3 * cs + 100, 3);
	ASSERT_EQ(replace_with(*v, "/dir/target", same), 0);
	auto after = snap(*v, "/dir/target");
	EXPECT_EQ(after.entry_offset, before.entry_offset);
	EXPECT_EQ(after.chain, before.chain);
	EXPECT_EQ(after.created, before.created);
	EXPECT_EQ(read_all(*v, "/dir/target"), same);

	// bigger but within the last cluster: still in place
	const auto fuller = random_bytes(4 * cs, 4);
	ASSERT_EQ(replace_with(*v, "/dir/target", fuller), 0);
	after = snap(*v, "/dir/target");
	EXPECT_EQ(after.entry_offset, before.entry_offset);
	EXPECT_EQ(after.chain, before.chain);
	EXPECT_EQ(read_all(*v, "/dir/target"), fuller);

	// too big: refused, nothing on the device changed
	const auto image = dev->data();
	EXPECT_EQ(v->replace("/dir/target", 4u * cs + 1), EFBIG);
	EXPECT_EQ(dev->data(), image);

	// smaller: the start of the chain stays, only the tail is freed
	const uint64_t free_before = v->info().free_clusters;
	const auto small = random_bytes(cs + 1, 5);
	ASSERT_EQ(replace_with(*v, "/dir/target", small), 0);
	after = snap(*v, "/dir/target");
	EXPECT_EQ(after.entry_offset, before.entry_offset);
	EXPECT_EQ(after.chain, std::vector<uint32_t>(before.chain.begin(), before.chain.begin() + 2));
	EXPECT_EQ(v->info().free_clusters, free_before + 2);
	EXPECT_EQ(read_all(*v, "/dir/target"), small);
	v.reset();
	expect_clean(dev);

	// and after reopening
	v = open_rw(dev);
	after = snap(*v, "/dir/target");
	EXPECT_EQ(after.entry_offset, before.entry_offset);
	EXPECT_EQ(after.first_cluster, before.first_cluster);
	EXPECT_EQ(read_all(*v, "/dir/target"), small);
	std::vector<fatx::entry_info> list;
	ASSERT_EQ(v->list("/dir", list), 0);
	ASSERT_EQ(list.size(), 4u);
	EXPECT_EQ(list[1].name, "target");	// same index in the directory
}

TEST(Replace, EmptyFilesAndErrors) {
	auto dev = formatted(16 << 20);
	auto v = open_rw(dev);
	const uint32_t cs = v->info().cluster_size;
	ASSERT_EQ(v->create("/empty", 0), 0);
	uint64_t room = 1;
	ASSERT_EQ(v->replace_capacity("/empty", &room), 0);
	EXPECT_EQ(room, 0u);
	EXPECT_EQ(v->replace("/empty", 1), EFBIG);	// an empty file has no cluster
	EXPECT_EQ(v->replace("/empty", 0), 0);
	// emptying a file frees all its clusters but keeps the entry
	ASSERT_EQ(v->create("/f", 2 * cs), 0);
	const auto before = snap(*v, "/f");
	const uint64_t free_before = v->info().free_clusters;
	ASSERT_EQ(v->replace("/f", 0), 0);
	const auto after = snap(*v, "/f");
	EXPECT_EQ(after.entry_offset, before.entry_offset);
	EXPECT_TRUE(after.chain.empty());
	EXPECT_EQ(v->info().free_clusters, free_before + 2);
	EXPECT_EQ(v->replace("/missing", 0), ENOENT);
	ASSERT_EQ(v->mkdir("/d"), 0);
	EXPECT_EQ(v->replace("/d", 0), EISDIR);
	EXPECT_EQ(v->replace("/name.txt", 0), EPERM);
	v.reset();
	expect_clean(dev);
	auto ro = fatx::volume::open(dev, {}, false, {}, nullptr);
	ASSERT_NE(ro, nullptr);
	EXPECT_EQ(ro->replace("/f", 0), EROFS);
}

TEST(Replace, DamagedImages) {
	auto pristine = formatted(4 << 20);
	{
		auto v = open_rw(pristine);
		ASSERT_EQ(v->mkdir("/d"), 0);
		for(int i = 0; i < 6; i++) {
			const auto data = random_bytes(static_cast<size_t>(i) * 3000 + 1, static_cast<unsigned>(i));
			ASSERT_EQ(v->create("/d/f" + std::to_string(i), data.size()), 0);
			ASSERT_EQ(v->write("/d/f" + std::to_string(i), 0, data), 0);
		}
	}
	int err = 0;
	const auto info = fatx::volume::open(pristine, {}, false, {}, &err)->info();
	std::mt19937_64 gen(99);
	for(int round = 0; round < 150; round++) {
		auto dev = std::make_shared<fatx::memory_io>(0);
		dev->data() = pristine->data();
		for(int i = 0; i < 8; i++)
			dev->data()[gen() % (info.data_offset + 6 * info.cluster_size)] = static_cast<std::byte>(gen() & 0xFF);
		auto v = fatx::volume::open(dev, {}, true, {}, &err);
		if(!v)
			continue;
		for(int i = 0; i < 6; i++) {
			const std::string p = "/d/f" + std::to_string(i);
			uint64_t room = 0;
			std::vector<uint32_t> chain;
			void(v->replace_capacity(p, &room));
			void(v->clusters(p, chain));
			const auto data = random_bytes(gen() % (std::min<uint64_t>(room, 65536) + 1), static_cast<unsigned>(round));
			void(replace_with(*v, p, data));
		}
	}
}
