#include <gtest/gtest.h>
#include "fatx.hpp"

class UtilsTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialisation commune si nécessaire
    }
};

TEST_F(UtilsTest, ClsArithmetic_Conversions) {
    clsarithmetic cls_math;
    
    // Test cls2ptr et ptr2cls
    clusptr cluster = 10;
    streamptr ptr = cls_math.cls2ptr(cluster);
    clusptr converted_back = cls_math.ptr2cls(ptr);
    
    EXPECT_EQ(converted_back, cluster);
    
    // Test siz2cls
    filesize size = 8192; // 8KB
    clusptr clusters = cls_math.siz2cls(size);
    EXPECT_GT(clusters, 0);
    
    // Vérifier que la taille en octets est suffisante
    EXPECT_GE(cls_math.cls2ptr(clusters + 1) - cls_math.cls2ptr(1), size);
}

TEST_F(UtilsTest, Vareas_Operations) {
    vareas areas;
    
    // Tester l'ajout de zones
    areas.add(10);
    areas.add(11);
    areas.add(12);
    
    // Vérifier la taille
    EXPECT_EQ(areas.nbcls(), 3);
    
    // Tester la recherche
    EXPECT_EQ(areas.at(0), 10);
    EXPECT_EQ(areas.at(1), 11);
    EXPECT_EQ(areas.at(2), 12);
    
    // Tester la sous-séquence
    auto sub = areas.sub(1, 2);
    EXPECT_EQ(sub.nbcls(), 2);
    EXPECT_EQ(sub.at(0), 11);
    EXPECT_EQ(sub.at(1), 12);
}

TEST_F(UtilsTest, Buffer_Operations) {
    const streamptr offset = 0;
    const streamptr size = 100;
    buffer buf(offset, size);
    
    // Tester l'écriture et la lecture
    const char* test_data = "Test data";
    size_t data_size = strlen(test_data);
    
    // Écrire des données
    buf.write(offset, test_data, data_size);
    
    // Lire les données
    char read_buffer[256] = {0};
    size_t bytes_read = buf.read(offset, read_buffer, data_size);
    
    EXPECT_EQ(bytes_read, data_size);
    EXPECT_STREQ(read_buffer, test_data);
    
    // Tester l'agrandissement
    buf.enlarge(200);
    EXPECT_GE(buf.size(), 200);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
