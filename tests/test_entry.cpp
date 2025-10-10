#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "fatx.hpp"
#include "mocks/mock_device.hpp"

using ::testing::_;
using ::testing::Return;
using ::testing::Invoke;
using ::testing::NiceMock;

class EntryTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Créer un périphérique mock
        mock_dev = std::make_shared<NiceMock<MockDevice>>();
        
        // Configurer les paramètres FATX
        params.cluster_size = 4096;
        params.fat_start = 0x1000;
        params.data_start = 0x10000;
        params.total_clusters = 1000;
        
        // Créer une entrée racine pour les tests
        root = std::make_unique<entry>();
    }
    
    std::shared_ptr<MockDevice> mock_dev;
    fatxpar params;
    std::unique_ptr<entry> root;
};

TEST_F(EntryTest, CreateFile_ValidName_Succeeds) {
    const char* filename = "test.txt";
    auto* file = new entry(filename, 0, false);
    
    int result = root->addtodir(file);
    
    EXPECT_EQ(result, 0);
    auto* found = root->find(filename);
    EXPECT_NE(found, nullptr);
    EXPECT_STREQ(found->name.c_str(), filename);
}

TEST_F(EntryTest, Find_NonExistentFile_ReturnsNull) {
    auto* found = root->find("nonexistent.txt");
    EXPECT_EQ(found, nullptr);
}

TEST_F(EntryTest, Resize_ValidSize_UpdatesSize) {
    const char* filename = "resize_test.txt";
    const filesize new_size = 1024;
    
    auto* file = new entry(filename, 0, false);
    root->addtodir(file);
    
    int result = file->resize(new_size);
    
    EXPECT_EQ(result, 0);
    EXPECT_EQ(file->size, new_size);
}

TEST_F(EntryTest, Write_ValidData_UpdatesContent) {
    const char* filename = "write_test.txt";
    const char* test_data = "Hello, FATX!";
    const size_t data_size = strlen(test_data);
    
    auto* file = new entry(filename, 0, false);
    root->addtodir(file);
    
    // Simuler l'écriture
    size_t bytes_written = file->bufwrite(test_data, 0, data_size);
    
    EXPECT_EQ(bytes_written, data_size);
    
    // Vérifier la lecture
    char buffer[256] = {0};
    size_t bytes_read = file->bufread(buffer, 0, data_size);
    
    EXPECT_EQ(bytes_read, data_size);
    EXPECT_STREQ(buffer, test_data);
}

TEST_F(EntryTest, Rename_ValidName_UpdatesName) {
    const char* old_name = "old_name.txt";
    const char* new_name = "new_name.txt";
    
    auto* file = new entry(old_name, 0, false);
    root->addtodir(file);
    
    int result = file->rename(new_name);
    
    EXPECT_EQ(result, 0);
    EXPECT_STREQ(file->name.c_str(), new_name);
    
    // Vérifier que l'ancien nom n'existe plus
    EXPECT_EQ(root->find(old_name), nullptr);
    
    // Vérifier que le nouveau nom existe
    auto* found = root->find(new_name);
    EXPECT_NE(found, nullptr);
    EXPECT_EQ(found, file);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
