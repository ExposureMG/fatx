// Tests of the embedding seams of fatx_core: log sink, per-thread contexts,
// io_backend and the volume API. They link fatx_core only (no FUSE, no
// Boost.Program_options).

#include <gtest/gtest.h>

#include <atomic>
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
	EXPECT_EQ(v.stat(path, info), 0);
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
	EXPECT_EQ(v->stat("/", st), 0);
	EXPECT_TRUE(st.directory);
	EXPECT_EQ(v->stat("/missing", st), ENOENT);
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
		ASSERT_EQ(v->stat("/Default.xex", st), 0);
		EXPECT_EQ(st.size, 0u);
		EXPECT_EQ(v->stat("/empty", st), ENOENT);
		// remove a file and a folder tree
		ASSERT_EQ(v->create("/Content/0000000000000000/a", 5), 0);
		ASSERT_EQ(v->remove("/Content"), 0);
		EXPECT_EQ(v->stat("/Content", st), ENOENT);
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
