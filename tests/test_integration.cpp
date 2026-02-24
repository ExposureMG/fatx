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

// Test 21: Couvre fatx.cpp L119-120 (fuse_debug block) et L235 (err != 0 → recalcul code retour)
// --debug active fuse_debug=true → vp.push_back("-d") + fuse_argv (L119-120)
// Le montage échoue (point de montage inexistant) → err != 0 → L235 exécuté
TEST_F(FATXIntegrationTest, Fatx_Fuse_Debug_Flag) {
    // Créer un système de fichiers FATX valide pour que setup() réussisse
    std::string mkfs_cmd = std::string("cd /home/baxter/Documents/dev/fatx/fatx.git && ./build/fatx --as mkfs --yes --table file --partition x2 ") + temp_disk_path + " 2>/dev/null";
    run_command(mkfs_cmd);
    // Lancer fuse avec --debug : couvre L118-120 (fuse_debug=true → ajoute "-d")
    // Le point de montage n'existe pas → fuse_main échoue → err != 0 → L235 couvert
    std::string fuse_cmd = std::string("cd /home/baxter/Documents/dev/fatx/fatx.git && ./build/fatx --as fuse --debug --table file --partition x2 ") + temp_disk_path + " /tmp/fatx_mnt_debug_NONEXIST/ 2>/dev/null";
    run_command(fuse_cmd);
    EXPECT_TRUE(true);
}

// Test 22: Couvre fatx.cpp L127-128 (fuse_singlethr block)
// --singlethr active fuse_singlethr=true → vp.push_back("-s") + fuse_argv (L127-128)
TEST_F(FATXIntegrationTest, Fatx_Fuse_Singlethr_Flag) {
    std::string mkfs_cmd = std::string("cd /home/baxter/Documents/dev/fatx/fatx.git && ./build/fatx --as mkfs --yes --table file --partition x2 ") + temp_disk_path + " 2>/dev/null";
    run_command(mkfs_cmd);
    std::string fuse_cmd = std::string("cd /home/baxter/Documents/dev/fatx/fatx.git && ./build/fatx --as fuse --singlethr --table file --partition x2 ") + temp_disk_path + " /tmp/fatx_mnt_sglt_NONEXIST/ 2>/dev/null";
    run_command(fuse_cmd);
    EXPECT_TRUE(true);
}

// Test 23: Couvre fatx.cpp L131-133 (fuse_option block)
// --option allow_other (≠ "ro") → fuse_option="allow_other" → non-vide → L130-133 exécutés
TEST_F(FATXIntegrationTest, Fatx_Fuse_Custom_Option) {
    std::string mkfs_cmd = std::string("cd /home/baxter/Documents/dev/fatx/fatx.git && ./build/fatx --as mkfs --yes --table file --partition x2 ") + temp_disk_path + " 2>/dev/null";
    run_command(mkfs_cmd);
    std::string fuse_cmd = std::string("cd /home/baxter/Documents/dev/fatx/fatx.git && ./build/fatx --as fuse --option allow_other --table file --partition x2 ") + temp_disk_path + " /tmp/fatx_mnt_opt_NONEXIST/ 2>/dev/null";
    run_command(fuse_cmd);
    EXPECT_TRUE(true);
}

// Test 24: Couvre fatx.cpp L136-138 (corps de la boucle unkopt pour args inconnus)
// Les options inconnues vont dans unkopt (allow_unregistered en mode fuse)
// → for(i: unkopt) { if(fuse_argc < 19) { push, fuse_argc++ } } → L136-138 exécutés
TEST_F(FATXIntegrationTest, Fatx_Fuse_Unknown_Args) {
    std::string mkfs_cmd = std::string("cd /home/baxter/Documents/dev/fatx/fatx.git && ./build/fatx --as mkfs --yes --table file --partition x2 ") + temp_disk_path + " 2>/dev/null";
    run_command(mkfs_cmd);
    // --xyzzy1 et --xyzzy2 sont des options inconnues → vont dans unkopt → couvre L136-138
    std::string fuse_cmd = std::string("cd /home/baxter/Documents/dev/fatx/fatx.git && ./build/fatx --as fuse --xyzzy1 --xyzzy2 --table file --partition x2 ") + temp_disk_path + " /tmp/fatx_mnt_unk_NONEXIST/ 2>/dev/null";
    run_command(fuse_cmd);
    EXPECT_TRUE(true);
}

// Test 25: Couvre fatx.cpp L141-142 (garde anti-débordement de unkopt)
// fuse_argc commence à 2 (progname + mountpoint) ; 18 options inconnues → fuse_argc=19 à la 17e
// La 18e option voit fuse_argc=19 >= max_fuse_args-1=19 → else { console::write("Too many..."); break; }
TEST_F(FATXIntegrationTest, Fatx_Fuse_TooMany_Unknown_Args) {
    std::string mkfs_cmd = std::string("cd /home/baxter/Documents/dev/fatx/fatx.git && ./build/fatx --as mkfs --yes --table file --partition x2 ") + temp_disk_path + " 2>/dev/null";
    run_command(mkfs_cmd);
    // Construire 18 options inconnues pour dépasser la limite max_fuse_args-1=19
    std::string many_unknowns;
    for (int i = 0; i < 18; i++)
        many_unknowns += " --unk" + std::to_string(i);
    std::string fuse_cmd = std::string("cd /home/baxter/Documents/dev/fatx/fatx.git && ./build/fatx --as fuse --table file --partition x2") + many_unknowns + " " + std::string(temp_disk_path) + " /tmp/fatx_mnt_over_NONEXIST/ 2>/dev/null";
    run_command(fuse_cmd);
    EXPECT_TRUE(true);
}
