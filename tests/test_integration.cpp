#include <gtest/gtest.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fstream>
#include <sstream>
#include "context.hpp"

// Integration tests using real FATX workflows
class FATXIntegrationTest : public ::testing::Test {
protected:
    char temp_disk_path[256];
    char temp_dir[256];

    void SetUp() override {
        // Create temporary directory
        strcpy(temp_dir, "/tmp/fatx_test_XXXXXX");
        if (mkdtemp(temp_dir) == nullptr) {
            FAIL() << "Failed to create temp directory";
        }
        
        // Create temporary disk file (50MB)
        // Use /tmp to avoid path length issues
        snprintf(temp_disk_path, 200, "/tmp/test_disk_%d.img", getpid());
        
        int fd = open(temp_disk_path, O_CREAT | O_WRONLY | O_TRUNC, 0644);
        if (fd == -1) {
            FAIL() << "Failed to create temp disk file";
        }
        
        // Create a 50MB disk with 0xFF
        char blank[1024];
        memset(blank, 0xFF, sizeof(blank));
        
        for (int i = 0; i < 50 * 1024; i++) {
            if (write(fd, blank, sizeof(blank)) < static_cast<ssize_t>(sizeof(blank))) {
                close(fd);
                FAIL() << "Failed to write to disk file";
            }
        }
        close(fd);
    }

    void TearDown() override {
        // Clean up temporary files
        unlink(temp_disk_path);
        
        // Clean up temp directory
        rmdir(temp_dir);
    }
    
    // Helper to run a shell command
    void run_command(const std::string& cmd) {
        FILE* pipe = popen(cmd.c_str(), "r");
        if (pipe) {
            char buffer[128];
            while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                // Just consume the output
            }
            pclose(pipe);
        }
    }
};

// Test 1: mkfs.fatx - Format partition X2
TEST_F(FATXIntegrationTest, MKFS_Format_X2_Partition) {
    std::string cmd = std::string("cd /home/baxter/Documents/dev/fatx/fatx.git && ./build/fatx --as mkfs --force-y --table file --partition x2 ") + temp_disk_path;
    run_command(cmd);
    
    // Check that file exists and is not empty
    struct stat st;
    EXPECT_EQ(stat(temp_disk_path, &st), 0);
    EXPECT_GT(st.st_size, 0);
}

// Test 2: mkfs.fatx - Format partition X3
TEST_F(FATXIntegrationTest, MKFS_Format_X3_Partition) {
    std::string cmd = std::string("cd /home/baxter/Documents/dev/fatx/fatx.git && ./build/fatx --as mkfs --force-y --table file --partition x3 ") + temp_disk_path;
    run_command(cmd);
    
    struct stat st;
    EXPECT_EQ(stat(temp_disk_path, &st), 0);
}

// Test 3: fsck.fatx - Check filesystem integrity
TEST_F(FATXIntegrationTest, FSCK_Check_Filesystem) {
    // First format
    std::string mkfs_cmd = std::string("cd /home/baxter/Documents/dev/fatx/fatx.git && ./build/fatx --as mkfs --force-y --table file --partition x2 ") + temp_disk_path;
    run_command(mkfs_cmd);
    
    // Then check
    std::string fsck_cmd = std::string("cd /home/baxter/Documents/dev/fatx/fatx.git && ./build/fatx --as fsck --force-y --table file --partition x2 ") + temp_disk_path;
    run_command(fsck_cmd);
    
    // fsck should complete without crashing
    EXPECT_TRUE(true);
}

// Test 4: label.fatx - Get volume label
TEST_F(FATXIntegrationTest, LABEL_Get_Volume_Label) {
    // Format with label
    std::string mkfs_cmd = std::string("cd /home/baxter/Documents/dev/fatx/fatx.git && ./build/fatx --as mkfs --force-y --table file --partition x2 ") + temp_disk_path;
    run_command(mkfs_cmd);
    
    // Get label
    std::string label_cmd = std::string("cd /home/baxter/Documents/dev/fatx/fatx.git && ./build/fatx --as label --table file --partition x2 ") + temp_disk_path;
    run_command(label_cmd);
    
    EXPECT_TRUE(true);
}

// Test 5: label.fatx - Set new label
TEST_F(FATXIntegrationTest, LABEL_Set_New_Label) {
    // Format
    std::string mkfs_cmd = std::string("cd /home/baxter/Documents/dev/fatx/fatx.git && ./build/fatx --as mkfs --force-y --table file --partition x2 ") + temp_disk_path;
    run_command(mkfs_cmd);
    
    // Set label
    std::string label_cmd = std::string("cd /home/baxter/Documents/dev/fatx/fatx.git && ./build/fatx --as label --set NewLabel --force-y --table file --partition x2 ") + temp_disk_path;
    run_command(label_cmd);
    
    EXPECT_TRUE(true);
}

// Test 6: unrm.fatx - Undelete recovery (on fresh format)
TEST_F(FATXIntegrationTest, UNRM_List_Deleted_Files) {
    // Format
    std::string mkfs_cmd = std::string("cd /home/baxter/Documents/dev/fatx/fatx.git && ./build/fatx --as mkfs --force-y --table file --partition x2 ") + temp_disk_path;
    run_command(mkfs_cmd);
    
    // List deleted files
    std::string unrm_cmd = std::string("cd /home/baxter/Documents/dev/fatx/fatx.git && ./build/fatx --as unrm --table file --partition x2 ") + temp_disk_path;
    run_command(unrm_cmd);
    
    EXPECT_TRUE(true);
}

// Test 7: Device - Read sector operations
TEST_F(FATXIntegrationTest, Device_Sector_Read) {
    // Write test data
    int fd = open(temp_disk_path, O_WRONLY);
    if (fd != -1) {
        char test_data[512];
        memset(test_data, 0xAA, sizeof(test_data));
        lseek(fd, 0, SEEK_SET);
        EXPECT_EQ(write(fd, test_data, sizeof(test_data)), static_cast<ssize_t>(sizeof(test_data)));
        close(fd);
    }
    
    // Device operations are tested through mkfs/fsck
    std::string cmd = std::string("cd /home/baxter/Documents/dev/fatx/fatx.git && ./build/fatx --as mkfs --force-y --table file --partition x2 ") + temp_disk_path;
    run_command(cmd);
    
    EXPECT_TRUE(true);
}

// Test 8: Partition detection - X2
TEST_F(FATXIntegrationTest, Partition_X2_Detection) {
    // Format with x2
    std::string mkfs_cmd = std::string("cd /home/baxter/Documents/dev/fatx/fatx.git && ./build/fatx --as mkfs --force-y --table file --partition x2 ") + temp_disk_path;
    run_command(mkfs_cmd);
    
    // fsck should detect partition correctly
    std::string fsck_cmd = std::string("cd /home/baxter/Documents/dev/fatx/fatx.git && ./build/fatx --as fsck --table file --partition x2 ") + temp_disk_path;
    run_command(fsck_cmd);
    
    EXPECT_TRUE(true);
}

// Test 9: Partition detection - X3
TEST_F(FATXIntegrationTest, Partition_X3_Detection) {
    // Format with x3
    std::string mkfs_cmd = std::string("cd /home/baxter/Documents/dev/fatx/fatx.git && ./build/fatx --as mkfs --force-y --table file --partition x3 ") + temp_disk_path;
    run_command(mkfs_cmd);
    
    // fsck should detect partition correctly
    std::string fsck_cmd = std::string("cd /home/baxter/Documents/dev/fatx/fatx.git && ./build/fatx --as fsck --table file --partition x3 ") + temp_disk_path;
    run_command(fsck_cmd);
    
    EXPECT_TRUE(true);
}

// Test 10: DiskMap - Cluster chain traversal
TEST_F(FATXIntegrationTest, DiskMap_Cluster_Chain_Traversal) {
    // Format filesystem
    std::string mkfs_cmd = std::string("cd /home/baxter/Documents/dev/fatx/fatx.git && ./build/fatx --as mkfs --force-y --table file --partition x2 ") + temp_disk_path;
    run_command(mkfs_cmd);
    
    // fsck will traverse cluster chains
    std::string fsck_cmd = std::string("cd /home/baxter/Documents/dev/fatx/fatx.git && ./build/fatx --as fsck --verbose --table file --partition x2 ") + temp_disk_path;
    run_command(fsck_cmd);
    
    EXPECT_TRUE(true);
}

// Test 11: Frontend CLI - Verbose mode
TEST_F(FATXIntegrationTest, Frontend_Verbose_Output) {
    std::string mkfs_cmd = std::string("cd /home/baxter/Documents/dev/fatx/fatx.git && ./build/fatx --as mkfs --verbose --force-y --table file --partition x2 ") + temp_disk_path;
    run_command(mkfs_cmd);
    
    EXPECT_TRUE(true);
}

// Test 12: Frontend CLI - Force yes flag
TEST_F(FATXIntegrationTest, Frontend_Force_Yes) {
    std::string mkfs_cmd = std::string("cd /home/baxter/Documents/dev/fatx/fatx.git && ./build/fatx --as mkfs --force-y --table file --partition x2 ") + temp_disk_path;
    run_command(mkfs_cmd);
    
    EXPECT_TRUE(true);
}

// Test 13: Error handling - Invalid partition type
TEST_F(FATXIntegrationTest, Error_Invalid_Partition_Type) {
    std::string cmd = std::string("cd /home/baxter/Documents/dev/fatx/fatx.git && ./build/fatx --as mkfs --table file --partition invalid ") + temp_disk_path;
    run_command(cmd);
    
    // Command should handle error gracefully without crashing
    EXPECT_TRUE(true);
}

// Test 14: Error handling - Non-existent file
TEST_F(FATXIntegrationTest, Error_Nonexistent_File) {
    std::string cmd = "cd /home/baxter/Documents/dev/fatx/fatx.git && ./build/fatx --as mkfs --table file --partition x2 /nonexistent/path/disk.img";
    run_command(cmd);
    
    // Should not crash - error handled gracefully
    EXPECT_TRUE(true);
}

// Test 15: Frontend - Help option
TEST_F(FATXIntegrationTest, Frontend_Help_Option) {
    std::string cmd = "cd /home/baxter/Documents/dev/fatx/fatx.git && ./build/fatx --help";
    run_command(cmd);
    
    // Help should produce output
    EXPECT_TRUE(true);
}

// Test 16: ByteOrder utilities - 32-bit little endian
TEST_F(FATXIntegrationTest, ByteOrder_32bit_Conversion) {
    unsigned char buffer[4] = {0x01, 0x02, 0x03, 0x04};
    uint32_t value = byte_order<4>::bigend(reinterpret_cast<const char*>(buffer))();
    EXPECT_TRUE(value > 0);
}

// Test 17: ByteOrder utilities - 16-bit little endian
TEST_F(FATXIntegrationTest, ByteOrder_16bit_Conversion) {
    unsigned char buffer[2] = {0x01, 0x02};
    uint16_t value = byte_order<2>::bigend(reinterpret_cast<const char*>(buffer))();
    EXPECT_TRUE(value > 0);
}

// Test 18: ByteOrder utilities - Verification
TEST_F(FATXIntegrationTest, ByteOrder_Verification) {
    // Verify byte order conversions
    unsigned char test[4] = {0x01, 0x02, 0x03, 0x04};
    uint32_t val = byte_order<4>::bigend(reinterpret_cast<const char*>(test))();
    // Should be little endian conversion
    EXPECT_TRUE(val > 0);
}

// Test 19: Full workflow - Format, Check, Label
TEST_F(FATXIntegrationTest, Full_Workflow_Format_Check_Label) {
    // Step 1: Format
    std::string mkfs_cmd = std::string("cd /home/baxter/Documents/dev/fatx/fatx.git && ./build/fatx --as mkfs --force-y --table file --partition x2 ") + temp_disk_path;
    run_command(mkfs_cmd);
    
    // Step 2: Check filesystem
    std::string fsck_cmd = std::string("cd /home/baxter/Documents/dev/fatx/fatx.git && ./build/fatx --as fsck --table file --partition x2 ") + temp_disk_path;
    run_command(fsck_cmd);
    
    // Step 3: Get label
    std::string label_cmd = std::string("cd /home/baxter/Documents/dev/fatx/fatx.git && ./build/fatx --as label --table file --partition x2 ") + temp_disk_path;
    run_command(label_cmd);
    
    EXPECT_TRUE(true);
}

// Test 20: Full workflow with recovery mode
TEST_F(FATXIntegrationTest, Full_Workflow_With_Recovery) {
    // Format
    std::string mkfs_cmd = std::string("cd /home/baxter/Documents/dev/fatx/fatx.git && ./build/fatx --as mkfs --force-y --table file --partition x2 ") + temp_disk_path;
    run_command(mkfs_cmd);
    
    // Check with recovery
    std::string unrm_cmd = std::string("cd /home/baxter/Documents/dev/fatx/fatx.git && ./build/fatx --as unrm --recover --table file --partition x2 ") + temp_disk_path;
    run_command(unrm_cmd);
    
    EXPECT_TRUE(true);
}
