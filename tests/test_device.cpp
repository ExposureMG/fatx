#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "fatx.hpp"
#include "mocks/mock_device.hpp"

using ::testing::_;
using ::testing::Return;
using ::testing::Invoke;
using ::testing::NiceMock;

class DeviceTest : public ::testing::Test {
protected:
    void SetUp() override {
        dev = std::make_unique<device>();
    }

    std::unique_ptr<device> dev;
};

TEST_F(DeviceTest, Read_ZeroSize_ReturnsEmpty) {
    auto result = dev->read(0, 0);
    EXPECT_TRUE(result.empty());
}

TEST_F(DeviceTest, Read_OutOfBounds_ReturnsEmpty) {
    // Simuler une taille de périphérique de 1000 octets
    ON_CALL(*dynamic_cast<MockDevice*>(dev.get()), size())
        .WillByDefault(Return(1000));
        
    testing::internal::CaptureStderr();
    auto result = dev->read(1000, 100);
    std::string output = testing::internal::GetCapturedStderr();
    
    EXPECT_TRUE(result.empty());
    EXPECT_THAT(output, testing::HasSubstr("out of bounds"));
}

TEST_F(DeviceTest, Read_ValidRange_ReturnsData) {
    const size_t size = 100;
    const streamptr offset = 0;
    
    // Configurer le mock pour retourner des données de test
    std::string test_data(size, 'A');
    ON_CALL(*dynamic_cast<MockDevice*>(dev.get()), read(offset, size))
        .WillByDefault(Return(test_data));
    
    auto result = dev->read(offset, size);
    
    EXPECT_EQ(result.size(), size);
    EXPECT_EQ(result, test_data);
}

TEST_F(DeviceTest, Write_EmptyData_ReturnsZero) {
    int result = dev->write(0, "");
    EXPECT_EQ(result, 0);
}

TEST_F(DeviceTest, Write_OutOfBounds_ReturnsError) {
    // Simuler une taille de périphérique de 1000 octets
    ON_CALL(*dynamic_cast<MockDevice*>(dev.get()), size())
        .WillByDefault(Return(1000));
    
    std::string data(100, 'A');
    int result = dev->write(950, data);
    
    EXPECT_EQ(result, EOVERFLOW);
}

TEST_F(DeviceTest, Write_ValidData_ReturnsSuccess) {
    std::string data = "Test data";
    
    // Configurer le mock pour accepter l'écriture
    ON_CALL(*dynamic_cast<MockDevice*>(dev.get()), write(_, _))
        .WillByDefault(Return(0));
    
    int result = dev->write(0, data);
    
    EXPECT_EQ(result, 0);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
