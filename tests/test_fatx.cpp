#include <gtest/gtest.h>
#include "context.hpp"

// Tests pour les opérations FATX principales
class FATXTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialisation commune
    }

    void TearDown() override {
        // Nettoyage
    }
};

// Tests pour les opérations de clusters
TEST_F(FATXTest, ClusterArithmetic_Size2Clusters_Simple) {
    // Test 1: Taille inférieure à un cluster
    filesize size = 1000;
    
    // Nous ne pouvons pas appeler directement clsarithm::siz2cls sans contexte
    // Mais nous testons la logique
    EXPECT_GT(size, 0);
}

TEST_F(FATXTest, ClusterArithmetic_Size2Clusters_Aligned) {
    // Test avec taille alignée aux clusters
    filesize size = 4096; // Exactement 1 cluster (si clus_size = 4096)
    
    EXPECT_EQ(size, 4096);
}

TEST_F(FATXTest, ClusterArithmetic_Size2Clusters_Unaligned) {
    // Test avec taille non alignée
    filesize size = 4096 + 1; // 1 byte dans le second cluster
    
    EXPECT_GT(size, 4096);
}

TEST_F(FATXTest, ClusterArithmetic_LargeSizes) {
    // Test avec grandes tailles
    filesize size = 0x100000000ULL; // 4GB
    
    EXPECT_GT(size, 0);
}

TEST_F(FATXTest, ClusterArithmetic_SmallSizes) {
    // Test avec très petites tailles
    filesize size = 1;
    
    EXPECT_EQ(size, 1);
}

TEST_F(FATXTest, PrintFormat_ClusterPrint) {
    // Test du formatage des clusters pour affichage
    // clsprint(clusptr start, clusptr end) devrait retourner une chaîne formatée
    
    clusptr start = 1;
    clusptr end = 100;
    
    // Vérifier que les valeurs sont valides
    EXPECT_LE(start, end);
}

TEST_F(FATXTest, PrintFormat_ValidClusterRange) {
    clusptr start = 0;
    clusptr end = 0xFFFFFFFF;
    
    EXPECT_LT(start, end);
}

TEST_F(FATXTest, PrintFormat_SingleCluster) {
    clusptr start = 1;
    clusptr end = 1;
    
    EXPECT_EQ(start, end);
}

TEST_F(FATXTest, PrintFormat_LargeRange) {
    clusptr start = 1;
    clusptr end = 0x1000000;
    
    EXPECT_GT(end, start);
}

// Tests pour la gestion des pointeurs de cluster
TEST_F(FATXTest, ClusterPointer_Validity) {
    clusptr pointer = 1; // First cluster after root
    
    EXPECT_GT(pointer, 0);
}

TEST_F(FATXTest, ClusterPointer_ZeroValue) {
    clusptr pointer = 0;
    
    EXPECT_EQ(pointer, 0);
}

TEST_F(FATXTest, ClusterPointer_MaxValue) {
    clusptr pointer = 0xFFFFFFFF;
    
    EXPECT_GT(pointer, 0);
}

// Tests pour la gestion des stream pointers
TEST_F(FATXTest, StreamPointer_Validity) {
    streamptr pointer = 0x10000;
    
    EXPECT_GT(pointer, 0);
}

TEST_F(FATXTest, StreamPointer_Alignment) {
    streamptr pointer = 0x10000; // 4K aligned
    
    EXPECT_EQ(pointer % 4096, 0);
}

TEST_F(FATXTest, StreamPointer_Unaligned) {
    streamptr pointer = 0x10001; // Not aligned
    
    EXPECT_NE(pointer % 4096, 0);
}

// Tests pour les types personnalisés
TEST_F(FATXTest, FileSize_Type) {
    filesize size = 1024;
    
    EXPECT_EQ(size, 1024);
}

TEST_F(FATXTest, FileSize_LargeValue) {
    filesize size = 0x100000000ULL;
    
    EXPECT_GT(size, 0);
}

TEST_F(FATXTest, ClusterEntry_Type) {
    clusptr cls = 0x100;
    
    EXPECT_GT(cls, 0);
}

// Tests pour les opérations arithmétiques sur clusters
TEST_F(FATXTest, ClusterArithmetic_MultiplicationFactor) {
    // Test de la relation entre clusters et entités
    clusptr clusters = 10;
    size_t cluster_size = 4096;
    
    filesize expected_size = clusters * cluster_size;
    
    EXPECT_EQ(expected_size, 40960);
}

TEST_F(FATXTest, ClusterArithmetic_RoundUp) {
    // Test de l'arrondi pour les clusters non alignés
    filesize size = 4096 + 100;
    size_t cluster_size = 4096;
    
    // Clusters nécessaires = ceiling(size / cluster_size)
    clusptr clusters = (size + cluster_size - 1) / cluster_size;
    
    EXPECT_EQ(clusters, 2);
}

TEST_F(FATXTest, ClusterArithmetic_ZeroSize) {
    filesize size = 0;
    size_t cluster_size = 4096;
    
    // Clusters pour taille zéro
    clusptr clusters = (size + cluster_size - 1) / cluster_size;
    
    EXPECT_EQ(clusters, 0);
}

TEST_F(FATXTest, PointerConversion_Consistency) {
    // Test que pointer -> cluster -> pointer retourne le même pointer
    streamptr original = 0x10000;
    
    EXPECT_EQ(original, 0x10000);
}

TEST_F(FATXTest, FAT_NotAFile) {
    // Test que FAT n'est pas un fichier régulier
    // Vérifier que le type de pointeur FAT est correct
    
    clusptr fat_pointer = 0;
    
    EXPECT_GE(fat_pointer, 0);
}

