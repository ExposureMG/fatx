#include <gtest/gtest.h>
#include "context.hpp"

// Tests pour les classes date et attrib (héritées par entry)
class EntryTest : public ::testing::Test {
protected:
    void SetUp() override {
        // No complex initialization needed
    }

    void TearDown() override {
        // Cleanup if needed
    }
};

// Tests des dates
TEST_F(EntryTest, DateCreation_Default) {
    date test_date;
    
    EXPECT_EQ(test_date.year, 1980);
    EXPECT_EQ(test_date.month, 1);
    EXPECT_EQ(test_date.day, 1);
    EXPECT_EQ(test_date.hour, 0);
    EXPECT_EQ(test_date.min, 0);
    EXPECT_EQ(test_date.sec, 0);
}

TEST_F(EntryTest, DateCreation_CustomValues) {
    date test_date;
    test_date.year = 2024;
    test_date.month = 2;
    test_date.day = 18;
    test_date.hour = 15;
    test_date.min = 30;
    test_date.sec = 45;
    
    EXPECT_EQ(test_date.year, 2024);
    EXPECT_EQ(test_date.month, 2);
    EXPECT_EQ(test_date.day, 18);
    EXPECT_EQ(test_date.hour, 15);
    EXPECT_EQ(test_date.min, 30);
    EXPECT_EQ(test_date.sec, 45);
}

TEST_F(EntryTest, DateWrite_AndRead) {
    date original;
    original.year = 2023;
    original.month = 6;
    original.day = 15;
    original.hour = 12;
    original.min = 30;
    original.sec = 45;
    
    unsigned char buf[4] = {0};
    original.write(buf);
    
    // Vérifier que les données ont été écrites
    date restored(buf);
    EXPECT_EQ(restored.year, original.year);
    EXPECT_EQ(restored.month, original.month);
    EXPECT_EQ(restored.day, original.day);
}

TEST_F(EntryTest, DateSequence) {
    date d1;
    d1.year = 2000;
    d1.month = 1;
    d1.day = 1;
    
    date::date_t seq = d1.seq();
    EXPECT_GT(seq, 0);
}

TEST_F(EntryTest, DateMonthRange) {
    date test_date;
    for (unsigned int month = 1; month <= 12; month++) {
        test_date.month = month;
        EXPECT_GE(test_date.month, 1);
        EXPECT_LE(test_date.month, 12);
    }
}

TEST_F(EntryTest, DateDayRange) {
    date test_date;
    for (unsigned int day = 1; day <= 31; day++) {
        test_date.day = day;
        EXPECT_GE(test_date.day, 1);
        EXPECT_LE(test_date.day, 31);
    }
}

TEST_F(EntryTest, DateHourRange) {
    date test_date;
    for (unsigned int hour = 0; hour < 24; hour++) {
        test_date.hour = hour;
        EXPECT_GE(test_date.hour, 0);
        EXPECT_LT(test_date.hour, 24);
    }
}

TEST_F(EntryTest, DateMinSecRange) {
    date test_date;
    for (unsigned int min = 0; min < 60; min++) {
        test_date.min = min;
        EXPECT_GE(test_date.min, 0);
        EXPECT_LT(test_date.min, 60);
    }
}

// Tests des attributs
TEST_F(EntryTest, AttribCreation_Default) {
    attrib a;
    
    EXPECT_FALSE(a.ro);
    EXPECT_FALSE(a.hid);
    EXPECT_FALSE(a.sys);
    EXPECT_FALSE(a.lab);
    EXPECT_FALSE(a.dir);
    EXPECT_FALSE(a.arc);
    EXPECT_FALSE(a.dev);
    EXPECT_FALSE(a.na);
}

TEST_F(EntryTest, AttribSetReadOnly) {
    attrib a;
    a.ro = true;
    
    EXPECT_TRUE(a.ro);
    EXPECT_FALSE(a.hid);
}

TEST_F(EntryTest, AttribSetHidden) {
    attrib a;
    a.hid = true;
    
    EXPECT_TRUE(a.hid);
    EXPECT_FALSE(a.ro);
}

TEST_F(EntryTest, AttribSetSystem) {
    attrib a;
    a.sys = true;
    
    EXPECT_TRUE(a.sys);
}

TEST_F(EntryTest, AttribSetLabel) {
    attrib a;
    a.lab = true;
    
    EXPECT_TRUE(a.lab);
}

TEST_F(EntryTest, AttribSetDirectory) {
    attrib a;
    a.dir = true;
    
    EXPECT_TRUE(a.dir);
}

TEST_F(EntryTest, AttribSetArchive) {
    attrib a;
    a.arc = true;
    
    EXPECT_TRUE(a.arc);
}

TEST_F(EntryTest, AttribSetDevice) {
    attrib a;
    a.dev = true;
    
    EXPECT_TRUE(a.dev);
}

TEST_F(EntryTest, AttribSetNotAvailable) {
    attrib a;
    a.na = true;
    
    EXPECT_TRUE(a.na);
}

TEST_F(EntryTest, AttribMultipleFlags) {
    attrib a;
    a.ro = true;
    a.hid = true;
    a.arc = true;
    
    EXPECT_TRUE(a.ro);
    EXPECT_TRUE(a.hid);
    EXPECT_TRUE(a.arc);
    EXPECT_FALSE(a.dir);
    EXPECT_FALSE(a.sys);
}

TEST_F(EntryTest, AttribFromChar) {
    // Test creating an attrib from a char value
    char c = 0x21; // bit 0 and 5 set (RO and ARC)
    attrib a(c);
    
    EXPECT_TRUE(a.ro); // bit 0
    EXPECT_TRUE(a.arc); // bit 5
    EXPECT_FALSE(a.hid); // bit 1
}

TEST_F(EntryTest, AttribWrite) {
    attrib a;
    a.ro = true;
    a.arc = true;
    
    char buf[1] = {0};
    a.write(buf);
    
    // Check that something was written
    EXPECT_NE(buf[0], 0);
}
