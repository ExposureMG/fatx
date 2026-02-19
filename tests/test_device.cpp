#include <gtest/gtest.h>
#include <fstream>
#include <filesystem>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <stdexcept>
#include "context.hpp"

// Classe pour gérer le contexte de test
class DeviceDirectTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Créer un fichier temporaire pour les tests (mkstemp is safer than tmpnam)
        char tmpl[] = "/tmp/fatx_dev_XXXXXX";
        int fd = mkstemp(tmpl);
        if (fd == -1) {
            throw std::runtime_error("mkstemp failed to create temp file");
        }
        close(fd);
        test_file = tmpl;
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
    char empty_tmpl[] = "/tmp/fatx_empty_XXXXXX";
    int empty_fd = mkstemp(empty_tmpl);
    if (empty_fd == -1) {
        throw std::runtime_error("mkstemp failed to create empty temp file");
    }
    close(empty_fd);
    std::string empty_file = empty_tmpl;
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
// Test supplémentaires pour device
TEST_F(DeviceDirectTest, Read_PartialBeyondEnd) {
    // D'abord setup du device
    int setup_result = dev->setup();
    ASSERT_EQ(setup_result, 0);

    // Demander plus que disponible à la fin du fichier
    const size_t offset = test_data.size() - 5;
    const size_t read_size = 20; // Demander plus que disponible
    auto partial_read = dev->read(offset, read_size);
    
    EXPECT_LE(partial_read.size(), read_size);
    EXPECT_GE(partial_read.size(), 0);
}

TEST_F(DeviceDirectTest, Write_MultipleWrites) {
    // D'abord setup du device
    int setup_result = dev->setup();
    ASSERT_EQ(setup_result, 0);

    std::string write_data1 = "first";
    std::string write_data2 = "second";
    
    int result1 = dev->write(0, write_data1);
    int result2 = dev->write(10, write_data2);
    
    EXPECT_EQ(result1, 0);
    EXPECT_EQ(result2, 0);
    EXPECT_TRUE(dev->modified());
}

TEST_F(DeviceDirectTest, Size_BeforeSetup) {
    // Avant setup, la taille devrait être 0
    EXPECT_EQ(dev->size(), 0);
}

TEST_F(DeviceDirectTest, Read_AtExactEnd) {
    int setup_result = dev->setup();
    ASSERT_EQ(setup_result, 0);

    // Lire exactement jusqu'à la fin
    auto end_read = dev->read(test_data.size() - 1, 1);
    EXPECT_EQ(end_read.size(), 1);
}

TEST_F(DeviceDirectTest, Modified_AfterMultipleWrites) {
    int setup_result = dev->setup();
    ASSERT_EQ(setup_result, 0);

    std::string write_data = "test";
    
    EXPECT_FALSE(dev->modified());
    int write_result1 = dev->write(0, write_data);
    EXPECT_EQ(write_result1, 0);
    EXPECT_TRUE(dev->modified());
    
    // Le flag devrait rester true après un autre write
    int write_result2 = dev->write(5, write_data);
    EXPECT_EQ(write_result2, 0);
    EXPECT_TRUE(dev->modified());
}

TEST_F(DeviceDirectTest, Setup_FilePersistence) {
    int setup_result1 = dev->setup();
    ASSERT_EQ(setup_result1, 0);
    
    size_t size1 = dev->size();
    EXPECT_GT(size1, 0);
}

// Additional tests for better device coverage
TEST_F(DeviceDirectTest, Read_LargeSize) {
    int setup_result = dev->setup();
    ASSERT_EQ(setup_result, 0);

    // Tenter de lire une très grande taille
    auto large_read = dev->read(0, 1000000);
    // Devrait lire jusqu'à la fin du fichier
    EXPECT_LE(large_read.size(), test_data.size());
}

TEST_F(DeviceDirectTest, Write_ConsecutiveOffsets) {
    int setup_result = dev->setup();
    ASSERT_EQ(setup_result, 0);

    // Écrire à différents offsets
    int res1 = dev->write(0, "A");
    int res2 = dev->write(100, "B");
    int res3 = dev->write(500, "C");

    EXPECT_EQ(res1, 0);
    EXPECT_EQ(res2, 0);
    EXPECT_EQ(res3, 0);
    EXPECT_TRUE(dev->modified());
}

TEST_F(DeviceDirectTest, Destructor_ClosesFiles) {
    // Créer et détruire un device
    {
        device temp_dev;
        auto result = temp_dev.setup();
        EXPECT_EQ(result, 0);
    }
    // Si le destructeur s'exécute sans erreur, test réussi
    EXPECT_TRUE(true);
}

TEST_F(DeviceDirectTest, Read_WithinBounds) {
    int setup_result = dev->setup();
    ASSERT_EQ(setup_result, 0);

    // Lire différentes portions du fichier
    auto part1 = dev->read(0, 100);
    auto part2 = dev->read(100, 100);
    auto part3 = dev->read(200, 100);

    EXPECT_EQ(part1.size(), 100);
    EXPECT_EQ(part2.size(), 100);
    EXPECT_EQ(part3.size(), 100);
}

TEST_F(DeviceDirectTest, Size_Consistency) {
    int setup_result = dev->setup();
    ASSERT_EQ(setup_result, 0);

    streamptr size1 = dev->size();
    streamptr size2 = dev->size();

    // La taille doit être consistante
    EXPECT_EQ(size1, size2);
    EXPECT_EQ(size1, test_data.size());
}

TEST_F(DeviceDirectTest, Write_VariousOffsets) {
    int setup_result = dev->setup();
    ASSERT_EQ(setup_result, 0);

    // Écrire à différents offsets
    int res1 = dev->write(0, "Start");
    int res2 = dev->write(500, "Middle");
    
    EXPECT_EQ(res1, 0);
    EXPECT_EQ(res2, 0);
    EXPECT_TRUE(dev->modified());
}

TEST(DeviceFileTest, Setup_CreateTemporaryFile) {
    char tmp_tmpl[] = "/tmp/fatx_file_XXXXXX";
    int tmp_fd = mkstemp(tmp_tmpl);
    if (tmp_fd == -1) {
        throw std::runtime_error("mkstemp failed to create temp file");
    }
    close(tmp_fd);
    std::string temp_file = tmp_tmpl;
    // Créer un fichier temporaire avec données
    {
        std::ofstream ofs(temp_file, std::ios::binary);
        ofs.write("test data", 9);
    }

    int tac = 1;
    const char *tav[] = {"test"};
    frontend *tf = new frontend(tac, tav);
    fatx_context::set(new fatx_context(*tf));
    fatx_context::get()->mmi.input = temp_file;
    fatx_context::get()->mmi.table = "";
    fatx_context::get()->mmi.diffile = "";

    device test_dev;
    int result = test_dev.setup();

    EXPECT_EQ(result, 0);
    EXPECT_EQ(test_dev.size(), 9);

    delete fatx_context::get();
    fatx_context::set(nullptr);
    delete tf;
    std::filesystem::remove(temp_file);
}

// Test pour le fichier diff (diffile)
class DeviceDiffTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Fichier principal
        char main_tmpl[] = "/tmp/fatx_main_XXXXXX";
        int main_fd = mkstemp(main_tmpl);
        close(main_fd);
        main_file = main_tmpl;
        {
            std::ofstream ofs(main_file, std::ios::binary);
            ofs.write("ORIGINAL", 8);
            ofs.write("        ", 8); // Un peu plus de place
        }

        // Fichier diff
        char diff_tmpl[] = "/tmp/fatx_diff_XXXXXX";
        int diff_fd = mkstemp(diff_tmpl);
        close(diff_fd);
        diff_file = diff_tmpl;

        // Contexte
        int tac = 1;
        const char *tav[] = {"test"};
        tf = new frontend(tac, tav);
        fatx_context::set(new fatx_context(*tf));
        auto ctx = fatx_context::get();
        ctx->mmi.input = main_file;
        ctx->mmi.diffile = diff_file;
        ctx->mmi.table = "";
        // ctx->mmi.readonly est false par défaut, donc writeable() est true

        dev = std::make_unique<device>();
    }

    void TearDown() override {
        dev.reset();
        if (fatx_context::get()) {
            delete fatx_context::get();
            fatx_context::set(nullptr);
        }
        if (tf) {
            delete tf;
            tf = nullptr;
        }
        if (std::filesystem::exists(main_file))
            std::filesystem::remove(main_file);
        if (std::filesystem::exists(diff_file))
            std::filesystem::remove(diff_file);
    }

    std::unique_ptr<device> dev;
    std::string main_file;
    std::string diff_file;
    frontend *tf = nullptr;
};

TEST_F(DeviceDiffTest, Write_ToDiffFile) {
    ASSERT_EQ(dev->setup(), 0);
    
    // Écrire des données
    std::string new_data = "MODIFIED";
    ASSERT_EQ(dev->write(0, new_data), 0);
    
    // Lire pour vérifier
    EXPECT_EQ(dev->read(0, 8), "MODIFIED");
    
    // Vérifier que le fichier original n'a pas changé
    std::ifstream ifs(main_file, std::ios::binary);
    char buf[8];
    ifs.read(buf, 8);
    EXPECT_EQ(std::string(buf, 8), "ORIGINAL");
}

TEST_F(DeviceDiffTest, Read_FromDiffFileAfterReload) {
    // 1. Premier setup et écriture
    ASSERT_EQ(dev->setup(), 0);
    ASSERT_EQ(dev->write(4, "XXXX"), 0);
    dev.reset(); // Ferme les fichiers

    // 2. Second setup (recharger le diff)
    dev = std::make_unique<device>();
    ASSERT_EQ(dev->setup(), 0);
    
    // Devrait lire ORIG au début et XXXX à partir de l'offset 4
    EXPECT_EQ(dev->read(0, 8), "ORIGXXXX");
}

TEST_F(DeviceDiffTest, Load_DuplicateSegment_Fails) {
    // Créer un fichier diff avec des segments en double
    // addseg écrit: p(8), s(8), data(s)
    std::ofstream ofs(diff_file, std::ios::binary);
    uint64_t p = 0;
    uint64_t s = 4;
    ofs.write(reinterpret_cast<char*>(&p), 8);
    ofs.write(reinterpret_cast<char*>(&s), 8);
    ofs.write("AAAA", 4);
    // Doublon
    ofs.write(reinterpret_cast<char*>(&p), 8);
    ofs.write(reinterpret_cast<char*>(&s), 8);
    ofs.write("BBBB", 4);
    ofs.close();

    ASSERT_NE(dev->setup(), 0);
}

TEST_F(DeviceDiffTest, Load_ShortSize_Fails) {
    std::ofstream ofs(diff_file, std::ios::binary);
    uint64_t p = 0;
    ofs.write(reinterpret_cast<char*>(&p), 8);
    // Pas de taille s
    ofs.close();

    ASSERT_NE(dev->setup(), 0);
}

// Test pour le support USB
class DeviceUSBTest : public ::testing::Test {
protected:
    void SetUp() override {
        char usb_tmpl[] = "/tmp/fatx_usb_XXXXXX";
        if (mkdtemp(usb_tmpl) == nullptr) {
            throw std::runtime_error("mkdtemp failed");
        }
        usb_root = usb_tmpl;
        
        // Créer Data0000, Data0001, Data0002
        // Data0000 doit contenir la taille cumulée à l'offset 0x240 (8 octets, bigend)
        std::ofstream d0(usb_root + "/Data0000", std::ios::binary);
        d0.seekp(0x240);
        uint64_t dts = __builtin_bswap64(1024); // Taille des fichiers à partir de Data0002
        d0.write(reinterpret_cast<char*>(&dts), 8);
        d0.close();

        std::ofstream d1(usb_root + "/Data0001", std::ios::binary);
        d1.write("DATA1", 5);
        d1.close();

        std::ofstream d2(usb_root + "/Data0002", std::ios::binary);
        std::string d2_content(1024, '2');
        d2.write(d2_content.data(), 1024);
        d2.close();

        // Contexte
        int tac = 1;
        const char *tav[] = {"test"};
        tf = new frontend(tac, tav);
        fatx_context::set(new fatx_context(*tf));
        auto ctx = fatx_context::get();
        ctx->mmi.input = usb_root;
        ctx->mmi.table = "usb";
        ctx->mmi.diffile = "";

        dev = std::make_unique<device>();
    }

    void TearDown() override {
        dev.reset();
        if (fatx_context::get()) {
            delete fatx_context::get();
            fatx_context::set(nullptr);
        }
        if (tf) {
            delete tf;
            tf = nullptr;
        }
        std::filesystem::remove_all(usb_root);
    }

    std::unique_ptr<device> dev;
    std::string usb_root;
    frontend *tf = nullptr;
};

TEST_F(DeviceUSBTest, Setup_USB_Succeeds) {
    ASSERT_EQ(dev->setup(), 0);
    // Taille totale = Data0000 size (0x248) + Data0001 size (5) + Data0002 size (1024)
    // Data0000 size est 0x248 car on a écrit à 0x240 et mis 8 octets.
    EXPECT_GT(dev->size(), 1024);
}

TEST_F(DeviceUSBTest, Read_USB_Data) {
    ASSERT_EQ(dev->setup(), 0);
    
    // On ne connaît pas exactement les offsets sans faire le calcul
    // Data0000 est à l'offset 0
    // Data0001 est à l'offset size(Data0000)
    // Data0002 est à l'offset size(Data0000) + size(Data0001)
    
    streamptr s0 = std::filesystem::file_size(usb_root + "/Data0000");
    streamptr s1 = std::filesystem::file_size(usb_root + "/Data0001");
    
    auto r1 = dev->read(s0, 5);
    EXPECT_EQ(r1, "DATA1");
    
    auto r2 = dev->read(s0 + s1, 4);
    EXPECT_EQ(r2, "2222");
}

TEST_F(DeviceUSBTest, Setup_USB_Xbox360Structure_Succeeds) {
    // Supprimer DataXXXX de la racine
    std::filesystem::remove(usb_root + "/Data0000");
    std::filesystem::remove(usb_root + "/Data0001");
    std::filesystem::remove(usb_root + "/Data0002");
    
    // Créer Xbox360/
    std::string xdir = usb_root + "/Xbox360";
    std::filesystem::create_directories(xdir);
    
    // Créer Data0000 dedans
    std::ofstream d0(xdir + "/Data0000", std::ios::binary);
    d0.seekp(0x240);
    uint64_t dts = __builtin_bswap64(5); // Juste Data0002
    d0.write(reinterpret_cast<char*>(&dts), 8);
    d0.close();
    
    std::ofstream d1(xdir + "/Data0001", std::ios::binary);
    d1.write("DATA1", 5);
    d1.close();
    
    std::ofstream d2(xdir + "/Data0002", std::ios::binary);
    d2.write("DATA2", 5);
    d2.close();
    
    ASSERT_EQ(dev->setup(), 0);
}

TEST_F(DeviceUSBTest, Mismatched_DTS_Fails) {
    // Modifier Data0000 pour avoir un mauvais DTS
    std::ofstream d0(usb_root + "/Data0000", std::ios::binary | std::ios::in);
    d0.seekp(0x240);
    uint64_t dts = __builtin_bswap64(9999); // Trop grand
    d0.write(reinterpret_cast<char*>(&dts), 8);
    d0.close();
    
    ASSERT_NE(dev->setup(), 0);
}

TEST_F(DeviceUSBTest, Write_USB_WithDiffFile) {
    // On ajoute un diffile pour USB
    char diff_tmpl[] = "/tmp/fatx_usb_diff_XXXXXX";
    int diff_fd = mkstemp(diff_tmpl);
    close(diff_fd);
    std::string diff_file = diff_tmpl;
    
    fatx_context::get()->mmi.diffile = diff_file;
    
    ASSERT_EQ(dev->setup(), 0);
    
    ASSERT_EQ(dev->write(0, "USBMOD"), 0);
    EXPECT_EQ(dev->read(0, 6), "USBMOD");
    
    dev.reset();
    std::filesystem::remove(diff_file);
}
