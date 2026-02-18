#include <gtest/gtest.h>
#include <fstream>
#include <filesystem>
#include <cstdlib>
#include <cstring>
#include "context.hpp"

// Classe pour gérer le contexte de test
class DeviceDirectTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Créer un fichier temporaire pour les tests
        test_file = std::tmpnam(nullptr);
        std::ofstream ofs(test_file, std::ios::binary);
        const size_t test_data_size = 1024;
        test_data.assign(test_data_size, 'A');
        ofs.write(test_data.c_str(), test_data_size);
        ofs.close();

        // Créer un contexte minimal pour les tests
        int tac = 1;
        const char *tav[] = {"test"};
        tf = new frontend(tac, tav);
        fatx_context::set(new fatx_context(*tf));
        auto ctx = fatx_context::get();
        ctx->mmi.input = test_file;
        ctx->mmi.table = ""; // Pas de table USB
        ctx->mmi.diffile = ""; // Pas de fichier diff

        // Créer l'instance device
        dev = std::make_unique<device>();
    }

    void TearDown() override {
        // Nettoyer le contexte
        delete fatx_context::get();
        fatx_context::set(nullptr);
        delete tf;

        // Nettoyer le fichier temporaire
        if (std::filesystem::exists(test_file)) {
            std::filesystem::remove(test_file);
        }
    }

    std::unique_ptr<device> dev;
    std::string test_file;
    std::string test_data;
    frontend *tf = nullptr;
};

// Test du constructeur et destructeur
TEST_F(DeviceDirectTest, Constructor_InitializesCorrectly) {
    EXPECT_TRUE(dev != nullptr);
    // Le device devrait être initialisé avec tot_size = 0
    EXPECT_EQ(dev->size(), 0);
    EXPECT_FALSE(dev->modified());
}

// Test de la méthode setup avec un fichier valide
TEST_F(DeviceDirectTest, Setup_WithValidFile_Succeeds) {
    int result = dev->setup();
    EXPECT_EQ(result, 0);
    // Après setup, la taille devrait être la taille du fichier
    EXPECT_EQ(dev->size(), test_data.size());
    EXPECT_FALSE(dev->modified());
}

// Test de la méthode read avec différentes tailles
TEST_F(DeviceDirectTest, Read_VariousSizes_ReturnsCorrectData) {
    // D'abord setup du device
    int setup_result = dev->setup();
    ASSERT_EQ(setup_result, 0);

    // Test lecture de 0 octets
    auto zero_read = dev->read(0, 0);
    EXPECT_TRUE(zero_read.empty());

    // Test lecture de quelques octets
    const size_t read_size = 10;
    auto small_read = dev->read(0, read_size);
    EXPECT_EQ(small_read.size(), read_size);
    EXPECT_EQ(small_read, test_data.substr(0, read_size));

    // Test lecture de toute la taille disponible
    auto full_read = dev->read(0, test_data.size());
    EXPECT_EQ(full_read.size(), test_data.size());
    EXPECT_EQ(full_read, test_data);
}

// Test de la méthode read avec offset
TEST_F(DeviceDirectTest, Read_WithOffset_ReturnsCorrectData) {
    // D'abord setup du device
    int setup_result = dev->setup();
    ASSERT_EQ(setup_result, 0);

    const size_t offset = 100;
    const size_t read_size = 50;
    auto offset_read = dev->read(offset, read_size);
    EXPECT_EQ(offset_read.size(), read_size);
    EXPECT_EQ(offset_read, test_data.substr(offset, read_size));
}

// Test de la méthode read hors limites
TEST_F(DeviceDirectTest, Read_OutOfBounds_ReturnsEmpty) {
    // D'abord setup du device
    int setup_result = dev->setup();
    ASSERT_EQ(setup_result, 0);

    // Essayer de lire au-delà de la taille du fichier
    auto out_of_bounds = dev->read(test_data.size(), 10);
    EXPECT_TRUE(out_of_bounds.empty());
}

// Test de la méthode write en mode lecture/écriture
TEST_F(DeviceDirectTest, Write_ReadOnlyMode_ReturnsZero) {
    // D'abord setup du device
    int setup_result = dev->setup();
    ASSERT_EQ(setup_result, 0);

    // Le device est en mode lecture/écriture (writeable() retourne true par défaut)
    // Donc le write() devrait réussir et marquer modified()
    std::string write_data = "test write";
    int result = dev->write(0, write_data);
    EXPECT_EQ(result, 0); // Devrait réussir
    EXPECT_TRUE(dev->modified()); // Devrait être marqué comme modifié
}

// Test de la méthode write avec données vides
TEST_F(DeviceDirectTest, Write_EmptyData_ReturnsZero) {
    // D'abord setup du device
    int setup_result = dev->setup();
    ASSERT_EQ(setup_result, 0);

    std::string empty_data = "";
    int result = dev->write(0, empty_data);
    EXPECT_EQ(result, 0);
}

// Test de la méthode size
TEST_F(DeviceDirectTest, Size_AfterSetup_ReturnsFileSize) {
    int setup_result = dev->setup();
    ASSERT_EQ(setup_result, 0);

    EXPECT_EQ(dev->size(), test_data.size());
}

// Test de la méthode modified
TEST_F(DeviceDirectTest, Modified_InitiallyFalse) {
    EXPECT_FALSE(dev->modified());
}

#ifndef NDEBUG
// Tests pour les méthodes de débogage (seulement si NDEBUG n'est pas défini)

// Test de la méthode address
TEST_F(DeviceDirectTest, Address_ReturnsFormattedString) {
    streamptr test_addr = 0x123456789ABCDEF0;
    std::string addr_str = device::address(test_addr);
    EXPECT_FALSE(addr_str.empty());
    EXPECT_TRUE(addr_str.find("0x") != std::string::npos);
}

// Test de la méthode print
TEST_F(DeviceDirectTest, Print_ReturnsFormattedOutput) {
    // D'abord setup du device
    int setup_result = dev->setup();
    ASSERT_EQ(setup_result, 0);

    const size_t print_size = 16;
    std::string print_output = dev->print(0, print_size, 8);
    EXPECT_FALSE(print_output.empty());
    // La sortie devrait contenir des adresses hexadécimales et des caractères ASCII
    EXPECT_TRUE(print_output.find("41") != std::string::npos); // 'A' en hex
}

// Test de la méthode devlog
TEST_F(DeviceDirectTest, Devlog_DoesNotThrow) {
    EXPECT_NO_THROW(dev->devlog(true, 0, "test message"));
    EXPECT_NO_THROW(dev->devlog(false, 100, "write test"));
}

#endif // NDEBUG

// Test de gestion d'erreur avec fichier inexistant
TEST(DeviceErrorTest, Setup_WithNonExistentFile_Fails) {
    // Créer un contexte avec un fichier inexistant
    int tac = 1;
    const char *tav[] = {"test"};
    frontend *tf = new frontend(tac, tav);
    fatx_context::set(new fatx_context(*tf));
    fatx_context::get()->mmi.input = "/path/to/nonexistent/file";
    fatx_context::get()->mmi.table = "";
    fatx_context::get()->mmi.diffile = "";

    device test_dev;
    int result = test_dev.setup();

    // Devrait échouer
    EXPECT_NE(result, 0);
    EXPECT_EQ(test_dev.size(), 0);

    delete fatx_context::get();
    fatx_context::set(nullptr);
    delete tf;
}

// Test avec fichier vide
TEST(DeviceEmptyFileTest, Setup_WithEmptyFile_Succeeds) {
    // Créer un fichier temporaire vide
    std::string empty_file = std::tmpnam(nullptr);
    std::ofstream ofs(empty_file, std::ios::binary);
    ofs.close();

    // Créer un contexte avec le fichier vide
    int tac = 1;
    const char *tav[] = {"test"};
    frontend *tf = new frontend(tac, tav);
    fatx_context::set(new fatx_context(*tf));
    fatx_context::get()->mmi.input = empty_file;
    fatx_context::get()->mmi.table = "";
    fatx_context::get()->mmi.diffile = "";

    device test_dev;
    int result = test_dev.setup();

    EXPECT_EQ(result, 0);
    EXPECT_EQ(test_dev.size(), 0);

    delete fatx_context::get();
    fatx_context::set(nullptr);
    delete tf;

    // Nettoyer
    std::filesystem::remove(empty_file);
}
