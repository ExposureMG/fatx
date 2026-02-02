#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "fatx.hpp"
#include "mocks/mock_device.hpp"

using ::testing::_;
using ::testing::Return;
using ::testing::Invoke;
using ::testing::NiceMock;

class DskMapTest : public ::testing::Test {
protected:
    void SetUp() override {
        mock_dev = std::make_shared<NiceMock<MockDevice>>();
        params.cluster_size = 4096;
        params.fat_start = 0x1000;
        params.data_start = 0x10000;
        params.total_clusters = 1000;
        
        // Configurer le mock pour les lectures/écritures
        ON_CALL(*mock_dev, read(_, _))
            .WillByDefault(Invoke([](streamptr p, size_t s) {
                return std::string(s, '\0');
            }));
            
        fat = std::make_unique<dskmap>(params);
    }

    std::shared_ptr<MockDevice> mock_dev;
    fatxpar params;
    std::unique_ptr<dskmap> fat;
};

TEST_F(DskMapTest, AllocFat_ZeroClusters_ReturnsEmpty) {
    auto result = fat->allocfat(0, 0);
    EXPECT_TRUE(result.empty());
}

TEST_F(DskMapTest, AllocFat_SingleCluster_ReturnsValidCluster) {
    auto result = fat->allocfat(1, 0);
    ASSERT_FALSE(result.empty());
    EXPECT_EQ(result.nbcls(), 1);
}

TEST_F(DskMapTest, AllocFat_MultipleClusters_ReturnsContiguousClusters) {
    const int num_clusters = 5;
    auto result = fat->allocfat(num_clusters, 0);
    ASSERT_FALSE(result.empty());
    EXPECT_EQ(result.nbcls(), num_clusters);
    
    // Vérifier la continuité
    clusptr first = result.first();
    for (int i = 0; i < num_clusters - 1; ++i) {
        EXPECT_EQ(fat->read(first + i), first + i + 1);
    }
    EXPECT_EQ(fat->read(first + num_clusters - 1), EOC);
}

TEST_F(DskMapTest, FreeFat_ValidCluster_FreesCluster) {
    auto allocated = fat->allocfat(3, 0);
    ASSERT_FALSE(allocated.empty());
    
    fat->freefat(allocated.first());
    
    // Essayer de réallouer la même quantité
    auto reallocated = fat->allocfat(3, 0);
    EXPECT_FALSE(reallocated.empty());
}

TEST_F(DskMapTest, ResizeFat_IncreaseSize_ExtendsAllocation) {
    auto allocated = fat->allocfat(3, 0);
    ASSERT_FALSE(allocated.empty());
    
    int res = fat->resizefat(allocated, 5);
    EXPECT_EQ(res, 0);
    EXPECT_EQ(allocated.nbcls(), 5);
}

TEST_F(DskMapTest, ResizeFat_DecreaseSize_ShrinksAllocation) {
    auto allocated = fat->allocfat(5, 0);
    ASSERT_FALSE(allocated.empty());
    
    int res = fat->resizefat(allocated, 3);
    EXPECT_EQ(res, 0);
    EXPECT_EQ(allocated.nbcls(), 3);
}

TEST_F(DskMapTest, Integration_AllocFreeRealloc) {
    // Allouer
    auto a1 = fat->allocfat(10, 0);
    ASSERT_FALSE(a1.empty());
    
    // Libérer
    fat->freefat(a1.first());
    
    // Réallouer
    auto a2 = fat->allocfat(10, 0);
    EXPECT_FALSE(a2.empty());
}

TEST_F(DskMapTest, AllocFat_DiskFull_ReturnsEmpty) {
    // Remplir le disque
    auto full = fat->allocfat(params.total_clusters, 0);
    ASSERT_FALSE(full.empty());
    
    // Essayer d'allouer à nouveau
    testing::internal::CaptureStderr();
    auto result = fat->allocfat(1, 0);
    std::string output = testing::internal::GetCapturedStderr();
    
    EXPECT_TRUE(result.empty());
    EXPECT_THAT(output, testing::HasSubstr("No space left"));
}

TEST_F(DskMapTest, AllocFat_MaxClusters_Succeeds) {
    auto result = fat->allocfat(params.total_clusters, 0);
    EXPECT_FALSE(result.empty());
    EXPECT_EQ(result.nbcls(), params.total_clusters);
}

TEST_F(DskMapTest, AllocFat_WithOffset_RespectsOffset) {
    // Allouer un cluster à un offset spécifique
    clusptr offset = 10;
    auto result = fat->allocfat(5, offset);
    ASSERT_FALSE(result.empty());
    EXPECT_GE(result.first(), offset);
}

TEST_F(DskMapTest, Fragmentation_Handling) {
    // Allouer des clusters alternés
    auto a1 = fat->allocfat(5, 0);
    auto a2 = fat->allocfat(5, 0);
    
    // Libérer le premier bloc
    fat->freefat(a1.first());
    
    // Allouer un bloc plus grand qui ne peut pas tenir dans l'espace libéré
    auto a3 = fat->allocfat(10, 0);
    
    // Devrait réussir en utilisant l'espace après a2
    EXPECT_FALSE(a3.empty());
    EXPECT_EQ(a3.nbcls(), 10);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
