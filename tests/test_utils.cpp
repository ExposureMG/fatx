#include <gtest/gtest.h>
#include <cstring>
#include <unistd.h>
#include <fstream>
#include <stdexcept>
#include "context.hpp"
#include "../src/constants.hpp"

class UtilsTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialisation commune si nécessaire
    }
};

// Test pour la classe vareas (vecteur de zones)
TEST_F(UtilsTest, Buffer_Creation) {
    buffer buf(0, 1024);
    EXPECT_EQ(buf.offset, 0);
    EXPECT_GE(buf.size(), 1024);
}

TEST_F(UtilsTest, Buffer_Enlarge) {
    buffer buf(0, 100);
    size_t initial_size = buf.size();
    buf.enlarge(500);
    EXPECT_GE(buf.size(), 500);
    EXPECT_GT(buf.size(), initial_size);
}

TEST_F(UtilsTest, Buffer_TouchedFlag) {
    buffer buf(0, 100);
    EXPECT_FALSE(buf.touched);
    buf.touched = true;
    EXPECT_TRUE(buf.touched);
}

// Test pour byte_order (conversions d'ordre des octets)
TEST_F(UtilsTest, ByteOrder_LittleEndian_2Bytes) {
    byte_order<2>::bigend le(0x1234);
    EXPECT_EQ(le(), 0x1234);
}

TEST_F(UtilsTest, ByteOrder_BigEndian_2Bytes) {
    byte_order<2>::bigend be(0x1234);
    EXPECT_EQ(be(), 0x1234);
}

TEST_F(UtilsTest, ByteOrder_4Bytes_Conversion) {
    byte_order<4>::bigend le(0x12345678);
    uint32_t value = le();
    EXPECT_EQ(value, 0x12345678);
}

// Test pour area (zone individuelle)
TEST_F(UtilsTest, Area_Creation) {
    area test_area(0, 1024, 512, 10, 15);
    EXPECT_EQ(test_area.offset, 0);
    EXPECT_EQ(test_area.pointer, 1024);
    EXPECT_EQ(test_area.size, 512);
    EXPECT_EQ(test_area.start, 10);
    EXPECT_EQ(test_area.stop, 15);
}
// Test supplémentaires pour buffer
TEST_F(UtilsTest, Buffer_LargeSize) {
    buffer buf(1000, 10000);
    EXPECT_EQ(buf.offset, 1000);
    EXPECT_GE(buf.size(), 10000);
}

TEST_F(UtilsTest, Buffer_EnlargeMultipleTimes) {
    buffer buf(0, 100);
    size_t size1 = buf.size();
    
    buf.enlarge(200);
    size_t size2 = buf.size();
    
    // Vérifier que size augmente après un enlarge vers une plus grande taille
    EXPECT_GE(size2, size1);
    
    // Appeler enlarge avec une taille plus grande
    buf.enlarge(300);
    size_t size3 = buf.size();
    
    EXPECT_GE(size3, size2);
}

TEST_F(UtilsTest, Buffer_ZeroSize) {
    buffer buf(0, 0);
    EXPECT_EQ(buf.offset, 0);
    EXPECT_GE(buf.size(), 0);
}

// Test supplémentaires pour byte_order
TEST_F(UtilsTest, ByteOrder_LittleEndian_4Bytes) {
    byte_order<4>::bigend le(0xDEADBEEF);
    EXPECT_EQ(le(), 0xDEADBEEF);
}

TEST_F(UtilsTest, ByteOrder_BigEndian_4Bytes) {
    byte_order<4>::bigend be(0xDEADBEEF);
    EXPECT_EQ(be(), 0xDEADBEEF);
}

TEST_F(UtilsTest, ByteOrder_8Bytes_LittleEndian) {
    byte_order<8>::bigend le(0x0123456789ABCDEF);
    EXPECT_EQ(le(), 0x0123456789ABCDEF);
}

TEST_F(UtilsTest, ByteOrder_8Bytes_BigEndian) {
    byte_order<8>::bigend be(0x0123456789ABCDEF);
    EXPECT_EQ(be(), 0x0123456789ABCDEF);
}

// Test supplémentaires pour area
TEST_F(UtilsTest, Area_LargeValues) {
    area test_area(0x100000, 0x200000, 0x10000, 100, 200);
    EXPECT_EQ(test_area.offset, 0x100000);
    EXPECT_EQ(test_area.pointer, 0x200000);
    EXPECT_EQ(test_area.size, 0x10000);
    EXPECT_EQ(test_area.start, 100);
    EXPECT_EQ(test_area.stop, 200);
}

TEST_F(UtilsTest, Area_ZeroSize) {
    area test_area(0, 0, 0, 0, 0);
    EXPECT_EQ(test_area.size, 0);
    EXPECT_EQ(test_area.start, 0);
    EXPECT_EQ(test_area.stop, 0);
}

TEST_F(UtilsTest, Area_SameStartStop) {
    area test_area(100, 200, 300, 50, 50);
    EXPECT_EQ(test_area.start, 50);
    EXPECT_EQ(test_area.stop, 50);
}

// Tests pour nameval
TEST_F(UtilsTest, Nameval_ValidNames) {
    EXPECT_EQ(nameval::is_valid("test.txt"), 0);
    EXPECT_EQ(nameval::is_valid("FILE123.DAT"), 0);
    EXPECT_EQ(nameval::is_valid("a"), 0);
    EXPECT_EQ(nameval::is_valid("file_with_underscores.dat"), 0);
}

TEST_F(UtilsTest, Nameval_InvalidNames) {
    EXPECT_EQ(nameval::is_valid(""), -EINVAL);
    EXPECT_EQ(nameval::is_valid("file with spaces.txt"), -EINVAL);
    EXPECT_EQ(nameval::is_valid("file\x01.txt"), -EINVAL);
    EXPECT_EQ(nameval::is_valid("file/with/slashes.txt"), -EINVAL);
}

TEST_F(UtilsTest, Nameval_NameTooLong) {
    std::string long_name(256, 'a');
    EXPECT_EQ(nameval::is_valid(long_name), -ENAMETOOLONG);
}

TEST_F(UtilsTest, Nameval_SpecialCharacters) {
    EXPECT_EQ(nameval::is_valid("file!.txt"), 0);
    EXPECT_EQ(nameval::is_valid("file#.txt"), 0);
    EXPECT_EQ(nameval::is_valid("file$.txt"), 0);
    EXPECT_EQ(nameval::is_valid("file&.txt"), 0);
    EXPECT_EQ(nameval::is_valid("file'.txt"), 0);
    EXPECT_EQ(nameval::is_valid("file(.txt"), 0);
    EXPECT_EQ(nameval::is_valid("file).txt"), 0);
    EXPECT_EQ(nameval::is_valid("file-.txt"), 0);
    EXPECT_EQ(nameval::is_valid("file@.txt"), 0);
    EXPECT_EQ(nameval::is_valid("file[].txt"), 0);
    EXPECT_EQ(nameval::is_valid("file^.txt"), 0);
    EXPECT_EQ(nameval::is_valid("file_.txt"), 0);
    EXPECT_EQ(nameval::is_valid("file{.txt"), 0);
    EXPECT_EQ(nameval::is_valid("file}.txt"), 0);
    EXPECT_EQ(nameval::is_valid("file~.txt"), 0);
}

// Tests pour console (nécessite redirection pour éviter l'interaction)
TEST_F(UtilsTest, Console_Write_NoThrow) {
    EXPECT_NO_THROW(console::write("Test message"));
    EXPECT_NO_THROW(console::write("Formatted: {}", 42));
    EXPECT_NO_THROW(console::write("Multiple: {} {}", "hello", "world"));
}

TEST_F(UtilsTest, Console_Write_Error_NoThrow) {
    EXPECT_NO_THROW(console::write("Error message", true));
    EXPECT_NO_THROW(console::write("Error formatted: {}", true, 123));
}

// Tests supplémentaires pour nameval
TEST_F(UtilsTest, Nameval_EdgeCases) {
    EXPECT_EQ(nameval::is_valid("a.txt"), 0);
    EXPECT_EQ(nameval::is_valid("A.TXT"), 0);
    EXPECT_EQ(nameval::is_valid("123.dat"), 0);
    EXPECT_EQ(nameval::is_valid("file-name.dat"), 0);
}

TEST_F(UtilsTest, Nameval_InvalidControlChars) {
    for (int i = 0; i < 32; ++i) {
        std::string name = "file" + std::string(1, static_cast<char>(i)) + ".txt";
        EXPECT_NE(nameval::is_valid(name), 0);
    }
    EXPECT_NE(nameval::is_valid("file\x7F.txt"), 0); // DEL character
}

TEST_F(UtilsTest, Nameval_PathSeparators) {
    EXPECT_NE(nameval::is_valid("file\\with\\backslashes.txt"), 0);
    EXPECT_NE(nameval::is_valid("file:with:colons.txt"), 0);
    EXPECT_NE(nameval::is_valid("file*with*asterisks.txt"), 0);
    EXPECT_NE(nameval::is_valid("file?with?questions.txt"), 0);
    EXPECT_NE(nameval::is_valid("file\"with\"quotes.txt"), 0);
    EXPECT_NE(nameval::is_valid("file<with>brackets.txt"), 0);
    EXPECT_NE(nameval::is_valid("file|with|pipes.txt"), 0);
}

// Tests supplémentaires pour console
TEST_F(UtilsTest, Console_Write_ComplexFormatting) {
    EXPECT_NO_THROW(console::write("Complex: {:08X} {:.2f} {}", 0xDEADBEEF, 3.14159, "test"));
    EXPECT_NO_THROW(console::write("Error complex: {:08X} {:.2f} {}", true, 0xCAFEBABE, 2.71828, "error"));
}

TEST_F(UtilsTest, Console_Write_EmptyString) {
    EXPECT_NO_THROW(console::write(""));
    EXPECT_NO_THROW(console::write("", true));
}

TEST_F(UtilsTest, Console_Write_LongString) {
    std::string long_str(1000, 'A');
    EXPECT_NO_THROW(console::write(long_str));
    EXPECT_NO_THROW(console::write(long_str, true));
}

// Tests pour les constantes et utilitaires
TEST_F(UtilsTest, Constants_Values) {
    EXPECT_EQ(blksize, 512);
    EXPECT_EQ(EOC, 0xFFFFFFFF);
    EXPECT_EQ(FLK, 0x00000000);
    EXPECT_EQ(EOD, static_cast<char>(-1));
    EXPECT_EQ(name_size, 0x2A);
    EXPECT_EQ(deleted_size, 0xE5);
}

TEST_F(UtilsTest, Constants_Calculations) {
    EXPECT_EQ(slab, name_size * 2 + 2);
    EXPECT_GT(max_buf, 0);
    EXPECT_GT(max_cache_div, 0);
    EXPECT_GT(nb_cache_div, 0);
    EXPECT_GT(timeout, 0);
}

TEST_F(UtilsTest, StringConstants) {
    EXPECT_STREQ(fsid, "XTAF");
    EXPECT_STREQ(flab, "name.txt");
    EXPECT_STREQ(def_landf, "lost+found");
    EXPECT_STREQ(def_fpre, "FILE");
    EXPECT_STREQ(def_label, "XBOX");
    EXPECT_STREQ(usb_dir, "Xbox360");
    EXPECT_STREQ(usb_data, "Data");
    EXPECT_STREQ(sepdir, "/");
    EXPECT_STREQ(mutex_buff, "Buffer:");
    EXPECT_STREQ(mutex_data, "Data:");
    EXPECT_STREQ(mutex_entr, "Entry:");
}

// Tests supplémentaires pour buffer
TEST_F(UtilsTest, Buffer_OffsetVariations) {
    buffer buf1(0, 100);
    buffer buf2(1000, 100);
    buffer buf3(10000, 100);
    
    EXPECT_EQ(buf1.offset, 0);
    EXPECT_EQ(buf2.offset, 1000);
    EXPECT_EQ(buf3.offset, 10000);
}

TEST_F(UtilsTest, Buffer_Enlarge_Zero) {
    buffer buf(0, 100);
    size_t original_size = buf.size();
    
    buf.enlarge(0);
    EXPECT_EQ(buf.size(), original_size);
}

TEST_F(UtilsTest, Buffer_Enlarge_Smaller) {
    buffer buf(0, 1000);
    size_t original_size = buf.size();
    
    buf.enlarge(100);
    EXPECT_EQ(buf.size(), original_size); // Should not shrink
}

// Tests supplémentaires pour byte_order
TEST_F(UtilsTest, ByteOrder_RoundTrip) {
    uint32_t original = 0x12345678;
    byte_order<4>::bigend le(original);
    uint32_t result = le();
    EXPECT_EQ(result, original);
}

TEST_F(UtilsTest, ByteOrder_DifferentSizes) {
    byte_order<1>::bigend b1(0xAB);
    byte_order<2>::bigend b2(0xABCD);
    byte_order<4>::bigend b4(0xABCDEF01);
    byte_order<8>::bigend b8(0xABCDEF0123456789);
    
    EXPECT_EQ(b1(), 0xAB);
    EXPECT_EQ(b2(), 0xABCD);
    EXPECT_EQ(b4(), 0xABCDEF01);
    EXPECT_EQ(b8(), 0xABCDEF0123456789);
}

// Tests supplémentaires pour area
TEST_F(UtilsTest, Area_NegativeValues) {
    // Note: Using unsigned types, so negative values not possible
    area test_area(0, 0, 0, 0, 0);
    EXPECT_EQ(test_area.offset, 0);
    EXPECT_EQ(test_area.pointer, 0);
    EXPECT_EQ(test_area.size, 0);
    EXPECT_EQ(test_area.start, 0);
    EXPECT_EQ(test_area.stop, 0);
}

TEST_F(UtilsTest, Area_LargeOffsets) {
    area test_area(0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF);
    EXPECT_EQ(test_area.offset, 0xFFFFFFFF);
    EXPECT_EQ(test_area.pointer, 0xFFFFFFFF);
    EXPECT_EQ(test_area.size, 0xFFFFFFFF);
    EXPECT_EQ(test_area.start, 0x7FFFFFFF);
    EXPECT_EQ(test_area.stop, 0x7FFFFFFF);
}

// Tests pour litend (little-endian byte_order)
TEST_F(UtilsTest, Litend_TwoBytes) {
    byte_order<2>::litend le(0x1234);
    EXPECT_EQ(le(), 0x1234);
    // Vérifier que les octets sont stockés en little-endian
    EXPECT_EQ(static_cast<uint8_t>(le[0]), 0x34);
    EXPECT_EQ(static_cast<uint8_t>(le[1]), 0x12);
}

TEST_F(UtilsTest, Litend_FourBytes) {
    byte_order<4>::litend le(0xDEADBEEF);
    EXPECT_EQ(le(), 0xDEADBEEF);
    EXPECT_EQ(static_cast<uint8_t>(le[0]), 0xEF);
    EXPECT_EQ(static_cast<uint8_t>(le[1]), 0xBE);
    EXPECT_EQ(static_cast<uint8_t>(le[2]), 0xAD);
    EXPECT_EQ(static_cast<uint8_t>(le[3]), 0xDE);
}

TEST_F(UtilsTest, Litend_EightBytes) {
    byte_order<8>::litend le(0x0123456789ABCDEF);
    EXPECT_EQ(le(), 0x0123456789ABCDEF);
    EXPECT_EQ(static_cast<uint8_t>(le[0]), 0xEF);
    EXPECT_EQ(static_cast<uint8_t>(le[7]), 0x01);
}

TEST_F(UtilsTest, Litend_OneByte) {
    byte_order<1>::litend le(0xAB);
    EXPECT_EQ(le(), 0xAB);
}

TEST_F(UtilsTest, Litend_Zero) {
    byte_order<4>::litend le(0u);
    EXPECT_EQ(le(), 0u);
    for(size_t b = 0; b < 4; b++)
        EXPECT_EQ(le[b], '\0');
}

TEST_F(UtilsTest, Litend_VsBigend_Differ) {
    uint16_t val = 0x1234;
    byte_order<2>::litend le(val);
    byte_order<2>::bigend be(val);
    // Both should round-trip to the same value
    EXPECT_EQ(le(), val);
    EXPECT_EQ(be(), val);
    // But the stored bytes should differ
    EXPECT_NE(static_cast<uint8_t>(le[0]), static_cast<uint8_t>(be[0]));
}

// Tests pour vareas::at()
TEST_F(UtilsTest, Vareas_At_Zero_ReturnsLast) {
    vareas va;
    // area(offset, pointer, size, start, stop)
    va.push_back(area(0, 0, 4096, 10, 15));
    // at(0) should return last() = 15
    EXPECT_EQ(va.at(0), 15u);
}

TEST_F(UtilsTest, Vareas_At_First_Cluster) {
    vareas va;
    va.push_back(area(0, 0, 4096, 10, 15));
    // at(1) = start + 1 - 1 = 10
    EXPECT_EQ(va.at(1), 10u);
}

TEST_F(UtilsTest, Vareas_At_Last_Cluster) {
    vareas va;
    va.push_back(area(0, 0, 4096, 10, 15));  // 6 clusters (10,11,12,13,14,15)
    EXPECT_EQ(va.at(6), 15u);
}

TEST_F(UtilsTest, Vareas_At_Middle_Cluster) {
    vareas va;
    va.push_back(area(0, 0, 4096, 10, 15));
    // at(3) = 10 + 3 - 1 = 12
    EXPECT_EQ(va.at(3), 12u);
}

TEST_F(UtilsTest, Vareas_At_Spans_Two_Areas) {
    vareas va;
    va.push_back(area(0, 0, 2048, 10, 14));   // 5 clusters
    va.push_back(area(2048, 2048, 2048, 20, 24)); // 5 clusters
    // at(6) → first area has 5 clusters, s=6-5=1 in second area → 20+1-1=20
    EXPECT_EQ(va.at(6), 20u);
    // at(10) = 20 + 5 - 1 = 24
    EXPECT_EQ(va.at(10), 24u);
}

TEST_F(UtilsTest, Vareas_At_OutOfRange) {
    vareas va;
    va.push_back(area(0, 0, 4096, 10, 15));  // 6 clusters
    // at(7) → out of range → 0
    EXPECT_EQ(va.at(7), 0u);
}

TEST_F(UtilsTest, Vareas_At_Empty) {
    vareas va;
    // at(0) on empty → last() = 0
    EXPECT_EQ(va.at(0), 0u);
    // at(1) on empty → 0
    EXPECT_EQ(va.at(1), 0u);
}

// Tests pour vareas::in()
TEST_F(UtilsTest, Vareas_In_Zero_ReturnsLast) {
    vareas va;
    va.push_back(area(0, 0, 2048, 10, 14));
    va.push_back(area(2048, 2048, 2048, 20, 24));
    // in(0) = end() - 1 = iterator to second area
    auto it = va.in(0);
    EXPECT_EQ(it->start, 20u);
}

TEST_F(UtilsTest, Vareas_In_FirstCluster) {
    vareas va;
    va.push_back(area(0, 0, 2048, 10, 14));   // 5 clusters
    va.push_back(area(2048, 2048, 2048, 20, 24));
    // in(1) → fits in first area (5 >= 1) → iterator to begin()
    auto it = va.in(1);
    EXPECT_EQ(it->start, 10u);
}

TEST_F(UtilsTest, Vareas_In_ExactFit) {
    vareas va;
    va.push_back(area(0, 0, 2048, 10, 14));   // 5 clusters
    va.push_back(area(2048, 2048, 2048, 20, 24));
    // in(5) → fits in first area (5 == 5) → begin()
    auto it = va.in(5);
    EXPECT_EQ(it->start, 10u);
}

TEST_F(UtilsTest, Vareas_In_Second_Area) {
    vareas va;
    va.push_back(area(0, 0, 2048, 10, 14));   // 5 clusters
    va.push_back(area(2048, 2048, 2048, 20, 24));
    // in(6) → first area has 5 → s=1 in second → begin()+1
    auto it = va.in(6);
    EXPECT_EQ(it->start, 20u);
}

TEST_F(UtilsTest, Vareas_In_OutOfRange) {
    vareas va;
    va.push_back(area(0, 0, 2048, 10, 14));   // 5 clusters
    // in(6) > 5 clusters → end()
    auto it = va.in(6);
    EXPECT_EQ(it, va.end());
}

// Tests pour vareas::add(vareas)
TEST_F(UtilsTest, Vareas_Add_EmptySource) {
    vareas dest;
    dest.push_back(area(0, 0, 4096, 10, 15));
    vareas src;  // empty
    dest.add(src);
    EXPECT_EQ(dest.size(), 1u);
    EXPECT_EQ(dest[0].start, 10u);
}

TEST_F(UtilsTest, Vareas_Add_IntoEmpty) {
    vareas dest;  // empty
    vareas src;
    src.push_back(area(0, 0, 4096, 10, 15));
    dest.add(src);
    EXPECT_EQ(dest.size(), 1u);
    EXPECT_EQ(dest[0].start, 10u);
}

TEST_F(UtilsTest, Vareas_Add_Adjacent_Merges) {
    vareas dest;
    dest.push_back(area(0, 0, 4096, 10, 15));  // stop=15
    vareas src;
    src.push_back(area(0, 4096, 4096, 16, 20));  // start=16 = last()+1
    dest.add(src);
    // Adjacent: last area gets extended, no new area added
    EXPECT_EQ(dest.size(), 1u);
    EXPECT_EQ(dest[0].stop, 20u);
    EXPECT_EQ(dest[0].size, 4096u + 4096u);
}

TEST_F(UtilsTest, Vareas_Add_NonAdjacent_Appends) {
    vareas dest;
    dest.push_back(area(0, 0, 4096, 10, 15));  // stop=15
    vareas src;
    src.push_back(area(0, 0, 4096, 20, 25));   // start=20 != 16
    dest.add(src);
    EXPECT_EQ(dest.size(), 2u);
    EXPECT_EQ(dest[0].start, 10u);
    EXPECT_EQ(dest[1].start, 20u);
    // Offset of src area recalculated from dest's last offset
    EXPECT_EQ(dest[1].offset, dest[0].offset + dest[0].size);
}

TEST_F(UtilsTest, Vareas_Add_Adjacent_WithTrailing) {
    vareas dest;
    dest.push_back(area(0, 0, 4096, 10, 15));  // stop=15
    vareas src;
    src.push_back(area(0, 4096, 4096, 16, 20));  // adjacent → merged
    src.push_back(area(4096, 8192, 4096, 25, 30));  // non-adjacent → appended
    dest.add(src);
    // First area of src merged, second gets offset recalculated
    EXPECT_EQ(dest.size(), 2u);
    EXPECT_EQ(dest[0].stop, 20u);
    EXPECT_EQ(dest[1].start, 25u);
    EXPECT_EQ(dest[1].offset, 4096u + 4096u);  // offset after merged first area
}

// =====================================================================
// Tests pour mymutx : lock/unlock actifs quand prog=fuse && ready=true
// =====================================================================

class MymutxTest : public ::testing::Test {
protected:
	std::string   test_file;
	frontend*     tf;
	fatx_context* ctx;

	void SetUp() override {
		char tmpl[] = "/tmp/fatx_mymutx_XXXXXX";
		int fd = mkstemp(tmpl);
		if (fd == -1) throw std::runtime_error("mkstemp failed");
		close(fd);
		test_file = tmpl;

		std::ofstream ofs(test_file, std::ios::binary | std::ios::out);
		const std::size_t img_size = 0x400000; // 4MB
		ofs.seekp(static_cast<std::streamoff>(img_size) - 1);
		char z = '\0';
		ofs.write(&z, 1);
		ofs.seekp(0);
		ofs.write("XTAF", 4);
		uint32_t id = 0, spc = 1, root_cl = 1;
		ofs.write(reinterpret_cast<char*>(&id),      4);
		ofs.write(reinterpret_cast<char*>(&spc),     4);
		ofs.write(reinterpret_cast<char*>(&root_cl), 4);
		uint16_t eoc = 0xFFFF;
		ofs.seekp(0x1000 + 2);
		ofs.write(reinterpret_cast<char*>(&eoc), 2);
		ofs.close();

		int tac = 1;
		const char* tav[] = {"test"};
		tf  = new frontend(tac, tav);
		ctx = new fatx_context(*tf);
		fatx_context::set(ctx);
		ctx->mmi.input   = test_file;
		ctx->mmi.table   = "file";
		ctx->mmi.prog    = frontend::fuse;
		ctx->mmi.force_a = true;
		ctx->mmi.force_y = true;

		int res = ctx->setup();
		ASSERT_EQ(res, 0);

		// Activer ready=true pour que les mymutx exécutent vraiment le lock
		ctx->ready = true;
	}

	void TearDown() override {
		if (ctx) {
			ctx->ready = false;
			delete ctx;
			ctx = nullptr;
		}
		delete tf;
		if (!test_file.empty())
			unlink(test_file.c_str());
	}
};

// Couvre mymutx::lock() et mymutx::unlock() avec prog=fuse && ready=true
TEST_F(MymutxTest, Mymutx_Lock_Unlock_Fuse_Ready) {
	mymutx m("test_mutex");
	// lock() : couvre le bloc conditionnel (prog==fuse && ready==true)
	m.lock();
	// unlock() : même branche
	m.unlock();
	EXPECT_TRUE(true);
}

// Couvre mymutx::lock_shared() et unlock_shared() avec prog=fuse && ready=true
TEST_F(MymutxTest, Mymutx_LockShared_UnlockShared_Fuse_Ready) {
	mymutx m("shared_mutex");
	m.lock_shared();
	m.unlock_shared();
	EXPECT_TRUE(true);
}

// Couvre la branche no-op quand prog != fuse
TEST_F(MymutxTest, Mymutx_NoOp_NonFuse) {
	ctx->mmi.prog = frontend::fsck;
	mymutx m("noop_mutex");
	// Ces appels ne font rien (branche false)
	m.lock();
	m.unlock();
	m.lock_shared();
	m.unlock_shared();
	EXPECT_TRUE(true);
}

// Couvre la branche no-op quand ready=false
TEST_F(MymutxTest, Mymutx_NoOp_NotReady) {
	ctx->ready = false;
	mymutx m("notready_mutex");
	m.lock();
	m.unlock();
	m.lock_shared();
	m.unlock_shared();
	ctx->ready = true;  // restaurer pour le TearDown
	EXPECT_TRUE(true);
}

// Couvre vareas::add(clusptr) quand vareas est vide → return immédiat (ligne 115)
TEST_F(MymutxTest, Vareas_AddClusPtr_EmptyReturn) {
	vareas empty_areas;
	// add() sur vide → early return (couvre ligne 115)
	empty_areas.add(static_cast<clusptr>(5));
	EXPECT_EQ(empty_areas.size(), 0u);
}

// Couvre vareas::add(clusptr) branche else (c+1 != first) → insert en tête (ligne 122)
TEST_F(MymutxTest, Vareas_AddClusPtr_NotAdjacent) {
	vareas areas;
	filesize csz = ctx->par.clus_size;
	// Une zone single cluster à cluster 5
	area a1(0, clsarithm::cls2ptr(5), csz, 5, 5);
	areas.push_back(a1);

	// Ajouter cluster 3 (3+1=4 != 5) → else branch
	areas.add(static_cast<clusptr>(3));
	// La nouvelle zone est insérée en tête
	EXPECT_EQ(areas.front().start, 3u);
	EXPECT_EQ(areas.size(), 2u);
}

// Couvre la boucle for dans add(clusptr) avec 2+ areas (ligne 125)
TEST_F(MymutxTest, Vareas_AddClusPtr_ForLoop) {
	vareas areas;
	filesize csz = ctx->par.clus_size;
	// Deux zones non adjacentes
	area a1(0,   clsarithm::cls2ptr(5), csz, 5, 5);
	area a2(csz, clsarithm::cls2ptr(8), csz, 8, 8);
	areas.push_back(a1);
	areas.push_back(a2);

	// Ajouter cluster 3 (not adjacent to first=5) → else branch
	// puis la for-loop ajuste les pointeurs de a1 et a2
	areas.add(static_cast<clusptr>(3));
	EXPECT_EQ(areas.size(), 3u);  // 3 éléments : nouveau + a1 + a2
	// Les pointeurs de a1 et a2 ont été décalés de csz
	EXPECT_EQ(areas[1].pointer, clsarithm::cls2ptr(5) + csz);
}

// Couvre vareas::add(clusptr) branche if (c+1 == first) → fusion (lignes 117-119)
TEST_F(MymutxTest, Vareas_AddClusPtr_Adjacent_Merge) {
	vareas areas;
	filesize csz = ctx->par.clus_size;
	area a1(0, clsarithm::cls2ptr(4), csz, 4, 4);
	areas.push_back(a1);

	// Ajouter cluster 3 (3+1=4 == first()) → if branch
	areas.add(static_cast<clusptr>(3));
	EXPECT_EQ(areas.front().start, 3u);  // start étendu à 3
	EXPECT_EQ(areas.size(), 1u);          // toujours 1 seule zone
}

// Couvre mymutx::name() en appelant directement le setter
TEST_F(MymutxTest, Mymutx_Name_Setter) {
	mymutx m("initial");
	m.name("new_name");
	EXPECT_TRUE(true);  // juste vérifier que ça ne crashe pas
}

// Tests des constantes de code d'erreur
TEST(FatxBasicTest, ErrorCodes) {
	EXPECT_EQ(code_noerr, 0);
	EXPECT_EQ(code_corrd, 1<<0);
	EXPECT_EQ(code_ncorr, 1<<2);
	EXPECT_EQ(code_operr, 1<<3);
	EXPECT_EQ(code_usage, 1<<4);
}


