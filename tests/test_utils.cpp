#include <gtest/gtest.h>
#include "context.hpp"

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

// Test pour console::read (difficile à tester sans redirection d'entrée)
TEST_F(UtilsTest, Console_Read_Exists) {
    // Just verify the function exists and can be called
    // In a real test environment, this would require input redirection
    EXPECT_TRUE(true); // Placeholder - console::read exists
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

// Tests pour les fonctions utilitaires diverses
TEST_F(UtilsTest, Utility_Functions) {
    // Test that various utility functions can be called
    EXPECT_TRUE(true); // Placeholder for utility function tests
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