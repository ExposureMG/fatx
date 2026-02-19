#include <gtest/gtest.h>
#include <fstream>
#include <filesystem>
#include <unistd.h>
#include <stdexcept>
#include "context.hpp"
#include "diskmap.hpp"

static std::string make_temp_file()
{
    char tmpl[] = "/tmp/fatx_ctx_XXXXXX";
    int fd = mkstemp(tmpl);
    if (fd == -1) throw std::runtime_error("mkstemp failed");
    close(fd);
    // create a minimally sized image with FATX signature and basic FAT
    std::ofstream ofs(tmpl, std::ios::binary | std::ios::out);
    const std::size_t size = 0x200000; // 2MB
    ofs.seekp(size - 1);
    char zero = '\0';
    ofs.write(&zero, 1);
    ofs.seekp(0);
    ofs.write("XTAF", 4);
    // Write a basic boot sector
    uint32_t id = 0;
    uint32_t spc = 1;
    uint32_t root = 1;
    ofs.seekp(4);
    ofs.write(reinterpret_cast<char*>(&id), 4);
    ofs.write(reinterpret_cast<char*>(&spc), 4);
    ofs.write(reinterpret_cast<char*>(&root), 4);
    // Initialize FAT: cluster 1 = EOC (0xFFFFFFFF), others = FLK (0x00000000)
    // Since chain_size=2, EOC=0xFFFF, FLK=0x0000
    ofs.seekp(0x1000); // fat_start
    uint16_t fat_entry = 0xFFFF; // EOC for cluster 1
    ofs.write(reinterpret_cast<char*>(&fat_entry), 2);
    // Rest are 0x0000 by default since file is zero-filled
    ofs.close();
    return std::string(tmpl);
}

TEST(ContextSetupExtra, UsesMemmapForFsck)
{
    std::string tf = make_temp_file();
    int argc = 1;
    const char* argv[] = {"test"};
    frontend mmi(argc, argv);
    mmi.input = tf;
    mmi.table = "file";
    mmi.prog = frontend::fsck;
    mmi.force_a = true;  // Automatically answer with default

    fatx_context ctx(mmi);
    int res = ctx.setup();
    EXPECT_EQ(res, 0);
    EXPECT_NE(ctx.fat, nullptr);
    EXPECT_NE(dynamic_cast<memmap*>(ctx.fat), nullptr);

    ctx.destroy();
    std::filesystem::remove(tf);
}

TEST(ContextSetupExtra, UsesDskmapForOther)
{
    std::string tf = make_temp_file();
    int argc = 1;
    const char* argv[] = {"test"};
    frontend mmi(argc, argv);
    mmi.input = tf;
    mmi.table = "file";
    mmi.prog = frontend::label; // neither fsck/unrm nor (fuse+recover)

    fatx_context ctx(mmi);
    int res = ctx.setup();
    EXPECT_EQ(res, 0);
    EXPECT_NE(ctx.fat, nullptr);
    EXPECT_EQ(dynamic_cast<memmap*>(ctx.fat), nullptr);

    ctx.destroy();
    std::filesystem::remove(tf);
}

TEST(ContextSetupExtra, MkfsDoesNotCreateRoot)
{
    std::string tf = make_temp_file();
    int argc = 1;
    const char* argv[] = {"test"};
    frontend mmi(argc, argv);
    mmi.input = tf;
    mmi.table = "file";
    mmi.prog = frontend::mkfs;

    fatx_context ctx(mmi);
    int res = ctx.setup();
    EXPECT_EQ(res, 0);
    EXPECT_EQ(ctx.root, nullptr);

    ctx.destroy();
    std::filesystem::remove(tf);
}

TEST(ContextSetupExtra, FuseRecoverUsesMemmap)
{
    std::string tf = make_temp_file();
    int argc = 1;
    const char* argv[] = {"test"};
    frontend mmi(argc, argv);
    mmi.input = tf;
    mmi.table = "file";
    mmi.prog = frontend::fuse;
    mmi.recover = true;

    fatx_context ctx(mmi);
    int res = ctx.setup();
    EXPECT_EQ(res, 0);
    EXPECT_NE(ctx.fat, nullptr);
    EXPECT_NE(dynamic_cast<memmap*>(ctx.fat), nullptr);

    ctx.destroy();
    std::filesystem::remove(tf);
}

TEST(ContextSetupExtra, FuseReadyReturnsCanceled)
{
    std::string tf = make_temp_file();
    int argc = 1;
    const char* argv[] = {"test"};
    frontend mmi(argc, argv);
    mmi.input = tf;
    mmi.table = "file";
    mmi.prog = frontend::fuse;
    mmi.recover = false;

    fatx_context *ctx = new fatx_context(mmi);
    // simulate already ready
    ctx->ready = true;
    int res = ctx->setup();
    EXPECT_EQ(res, ECANCELED);

    delete ctx;
    std::filesystem::remove(tf);
}
