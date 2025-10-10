#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "fatx.hpp"
#include "mocks/mock_device.hpp"

using ::testing::_;
using ::testing::Return;
using ::testing::Invoke;
using ::testing::NiceMock;

class PartitionTest : public ::testing::Test {
protected:
    void SetUp() override {
        mock_dev = std::make_shared<NiceMock<MockDevice>>();
        fat = std::make_unique<partition>();
    }

    std::shared_ptr<MockDevice> mock_dev;
    std::unique_ptr<fatxpar> fat;
};

TEST_F(PartitionTest, Setup_ValidDevice_ReturnsSuccess) {
    // Configuration du mock pour simuler un périphérique valide
    ON_CALL(*mock_dev, size())
        .WillByDefault(Return(1024 * 1024)); // 1MB de taille
    
    int result = fat->setup();
    
    EXPECT_EQ(result, 0);
}

TEST_F(PartitionTest, Label_GetSet_WorksCorrectly) {
    const unsigned char test_label[] = "TEST_LABEL";
    const size_t label_size = strlen((const char*)test_label);
    
    // Tester la définition du label
    fat->label(test_label, label_size);
    
    // Tester la récupération du label
    unsigned char buffer[256] = {0};
    size_t read_size = fat->label(buffer);
    
    EXPECT_EQ(read_size, label_size);
    EXPECT_EQ(memcmp(buffer, test_label, label_size), 0);
}

TEST_F(PartitionTest, Write_UpdatesDevice) {
    // Configurer le mock pour accepter l'écriture
    EXPECT_CALL(*mock_dev, write(_, _))
        .WillOnce(Return(0));
    
    int result = fat->write();
    
    EXPECT_EQ(result, 0);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
