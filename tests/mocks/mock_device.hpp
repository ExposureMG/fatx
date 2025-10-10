#pragma once

#include <gmock/gmock.h>
#include "../../src/fatx.hpp"

class MockDevice : public device {
public:
    MOCK_METHOD(std::string, read, (streamptr p, size_t s), (override));
    MOCK_METHOD(int, write, (streamptr p, const std::string &s), (override));
    MOCK_METHOD(filesize, size, (), (const, override));
    MOCK_METHOD(bool, is_open, (), (const, override));
    MOCK_METHOD(void, close, (), (override));
    
    // Implémentation par défaut pour éviter les appels non attendus
    MockDevice() {
        ON_CALL(*this, read(_, _))
            .WillByDefault(::testing::Return(std::string()));
            
        ON_CALL(*this, write(_, _))
            .WillByDefault(::testing::Return(0));
            
        ON_CALL(*this, size())
            .WillByDefault(::testing::Return(0));
            
        ON_CALL(*this, is_open())
            .WillByDefault(::testing::Return(true));
    }
};
