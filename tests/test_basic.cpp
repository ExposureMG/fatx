#include <gtest/gtest.h>
#include <cstring>
#include "../src/constants.hpp"

// Tests de base pour les constantes et fonctionnalités simples de FATX
class FatxBasicTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Configuration des tests si nécessaire
    }
};

// Test des constantes de base
TEST_F(FatxBasicTest, BlockSize) {
    EXPECT_EQ(blksize, 512);
    EXPECT_EQ(name_size, 0x2A);  // 42 caractères
}

// Test de la validité des constantes FATX
TEST_F(FatxBasicTest, FatxConstants) {
    EXPECT_EQ(EOC, 0xFFFFFFFF);
    EXPECT_EQ(FLK, 0x00000000);
    EXPECT_EQ(EOD, -1);
    EXPECT_EQ(deleted_size, 0xE5);
}

// Test des constantes de chaîne
TEST_F(FatxBasicTest, StringConstants) {
    EXPECT_STREQ(sepdir, "/");
    EXPECT_STREQ(fsid, "XTAF");
    EXPECT_STREQ(flab, "name.txt");
    EXPECT_STREQ(def_landf, "lost+found");
    EXPECT_STREQ(def_fpre, "FILE");
    EXPECT_STREQ(def_label, "XBOX");
}

// Test de la taille du slab (maximum size of label name file)
TEST_F(FatxBasicTest, SlabSize) {
    // slab = name_size * 2 + 2
    size_t expected_slab = name_size * 2 + 2;
    EXPECT_EQ(slab, expected_slab);
}

// Test des constantes de buffer et cache
TEST_F(FatxBasicTest, BufferConstants) {
    EXPECT_GT(max_buf, 0);
    EXPECT_GT(max_cache_div, 0);
    EXPECT_GT(nb_cache_div, 0);
    EXPECT_GT(timeout, 0);
}

// Test des constantes de code d'erreur
TEST_F(FatxBasicTest, ErrorCodes) {
    EXPECT_EQ(code_noerr, 0);
    EXPECT_EQ(code_corrd, 1<<0);
    EXPECT_EQ(code_ncorr, 1<<2);
    EXPECT_EQ(code_operr, 1<<3);
    EXPECT_EQ(code_usage, 1<<4);
}

// Test de la longueur maximale du nom de fichier
TEST_F(FatxBasicTest, MaxFilenameLength) {
    // Le nom de fichier FATX ne peut pas dépasser name_size
    char long_name[name_size + 10];
    memset(long_name, 'A', sizeof(long_name) - 1);
    long_name[sizeof(long_name) - 1] = '\0';

    // Cette longueur devrait être supérieure à name_size
    EXPECT_GT(strlen(long_name), static_cast<size_t>(name_size));
}

// Test simple de logique arithmétique de base (si disponible)
// Note: Ces tests sont basiques et ne nécessitent pas l'exécution complète du programme

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
