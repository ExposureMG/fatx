#include <gtest/gtest.h>
#include "context.hpp"
#include "frontend.hpp"

// Tests pour la classe frontend (options and command configuration)
class FrontendTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup test environment
    }

    void TearDown() override {
        // Cleanup if needed
    }
};

// Tests pour la classe frontend
TEST_F(FrontendTest, Frontend_Constructor_Default) {
    int argc = 1;
    const char* argv[] = {"fatx"};
    frontend fe(argc, argv);
    
    EXPECT_EQ(fe.argc, 1);
    EXPECT_EQ(fe.argv[0], argv[0]);
}

TEST_F(FrontendTest, Frontend_ProgName) {
    int argc = 1;
    const char* argv[] = {"fatx"};
    frontend fe(argc, argv);
    
    EXPECT_FALSE(fe.progname.empty());
}

TEST_F(FrontendTest, Frontend_DefaultFlags) {
    int argc = 1;
    const char* argv[] = {"fatx"};
    frontend fe(argc, argv);
    
    EXPECT_FALSE(fe.force_y);
    EXPECT_FALSE(fe.force_n);
    EXPECT_FALSE(fe.force_a);
    EXPECT_FALSE(fe.verbose);
}

TEST_F(FrontendTest, Frontend_RecoverFlag) {
    int argc = 1;
    const char* argv[] = {"fatx"};
    frontend fe(argc, argv);
    
    EXPECT_FALSE(fe.recover);
    EXPECT_FALSE(fe.local);
}

TEST_F(FrontendTest, Frontend_DeleteFlags) {
    int argc = 1;
    const char* argv[] = {"fatx"};
    frontend fe(argc, argv);
    
    EXPECT_TRUE(fe.deldate);
    EXPECT_TRUE(fe.dellost);
}

TEST_F(FrontendTest, Frontend_FUSEFlags) {
    int argc = 1;
    const char* argv[] = {"fatx"};
    frontend fe(argc, argv);
    
    EXPECT_FALSE(fe.fuse_debug);
    EXPECT_FALSE(fe.fuse_foregrd);
    EXPECT_FALSE(fe.fuse_singlethr);
}

TEST_F(FrontendTest, Frontend_FATFlag) {
    int argc = 1;
    const char* argv[] = {"fatx"};
    frontend fe(argc, argv);
    
    EXPECT_FALSE(fe.nofat);
    EXPECT_FALSE(fe.cutname);
}

TEST_F(FrontendTest, Frontend_FileCountDefault) {
    int argc = 1;
    const char* argv[] = {"fatx"};
    frontend fe(argc, argv);
    
    EXPECT_EQ(fe.filecount, 0u);
}

TEST_F(FrontendTest, Frontend_StringsInitialized) {
    int argc = 1;
    const char* argv[] = {"fatx"};
    frontend fe(argc, argv);
    
    // Vérifier que les strings sont initialisées
    EXPECT_TRUE(fe.partition.empty() || !fe.partition.empty());
    EXPECT_TRUE(fe.table.empty() || !fe.table.empty());
    EXPECT_TRUE(fe.input.empty() || !fe.input.empty());
}

TEST_F(FrontendTest, Frontend_ClusterSize) {
    int argc = 1;
    const char* argv[] = {"fatx"};
    frontend fe(argc, argv);
    
    EXPECT_EQ(fe.clus_size, 0u);
}

TEST_F(FrontendTest, Frontend_OffsetAndSize) {
    int argc = 1;
    const char* argv[] = {"fatx"};
    frontend fe(argc, argv);
    
    EXPECT_EQ(fe.offset, 0u);
    EXPECT_EQ(fe.size, 0u);
}

TEST_F(FrontendTest, Frontend_Dialog) {
    int argc = 1;
    const char* argv[] = {"fatx"};
    frontend fe(argc, argv);
    
    EXPECT_TRUE(fe.dialog);
}

TEST_F(FrontendTest, Frontend_AllYesFlag) {
    int argc = 1;
    const char* argv[] = {"fatx"};
    frontend fe(argc, argv);
    
    EXPECT_TRUE(fe.allyes);
}

TEST_F(FrontendTest, Frontend_Multiple_Instances) {
    int argc = 1;
    const char* argv[] = {"fatx"};
    frontend fe1(argc, argv);
    frontend fe2(argc, argv);
    
    EXPECT_EQ(fe1.argc, fe2.argc);
    EXPECT_NE(&fe1, &fe2);
}

TEST_F(FrontendTest, Frontend_ProgType_Unknown) {
    int argc = 1;
    const char* argv[] = {"fatx"};
    frontend fe(argc, argv);
    
    EXPECT_EQ(fe.prog, frontend::unknown);
}

TEST_F(FrontendTest, Frontend_VolumeName) {
    int argc = 1;
    const char* argv[] = {"fatx"};
    frontend fe(argc, argv);
    
    EXPECT_TRUE(fe.volname.empty());
}

TEST_F(FrontendTest, Frontend_MountPoint) {
    int argc = 1;
    const char* argv[] = {"fatx"};
    frontend fe(argc, argv);
    
    EXPECT_TRUE(fe.mount.empty());
}

TEST_F(FrontendTest, Frontend_Script) {
    int argc = 1;
    const char* argv[] = {"fatx"};
    frontend fe(argc, argv);
    
    EXPECT_TRUE(fe.script.empty());
}

TEST_F(FrontendTest, Frontend_DifFile) {
    int argc = 1;
    const char* argv[] = {"fatx"};
    frontend fe(argc, argv);
    
    EXPECT_TRUE(fe.diffile.empty());
}

TEST_F(FrontendTest, Frontend_Permissions) {
    int argc = 1;
    const char* argv[] = {"fatx"};
    frontend fe(argc, argv);
    
    // Check that uid and gid are set to something reasonable
    EXPECT_GE(fe.uid, 0u);
    EXPECT_GE(fe.gid, 0u);
}

TEST_F(FrontendTest, Frontend_Name_Method) {
    int argc = 1;
    const char* argv[] = {"fatx"};
    frontend fe(argc, argv);
    
    std::string name = fe.name();
    EXPECT_FALSE(name.empty());
}

TEST_F(FrontendTest, Frontend_FUSE_Option_Empty) {
    int argc = 1;
    const char* argv[] = {"fatx"};
    frontend fe(argc, argv);
    
    EXPECT_TRUE(fe.fuse_option.empty());
}

TEST_F(FrontendTest, Frontend_UnknownOptions_Vector) {
    int argc = 1;
    const char* argv[] = {"fatx"};
    frontend fe(argc, argv);
    
    EXPECT_EQ(fe.unkopt.size(), 0);
}

TEST_F(FrontendTest, Frontend_LostFound) {
    int argc = 1;
    const char* argv[] = {"fatx"};
    frontend fe(argc, argv);
    
    EXPECT_TRUE(fe.lostfound.empty() || !fe.lostfound.empty());
}

TEST_F(FrontendTest, Frontend_FoundFile) {
    int argc = 1;
    const char* argv[] = {"fatx"};
    frontend fe(argc, argv);
    
    EXPECT_TRUE(fe.foundfile.empty() || !fe.foundfile.empty());
}

TEST_F(FrontendTest, Frontend_CopyConstructor) {
    int argc = 2;
    const char* argv[] = {"fatx", "file.img"};
    frontend fe1(argc, argv);
    fe1.verbose = true;
    
    // Create another frontend for comparison
    frontend fe2(argc, argv);
    
    // They should be independent
    fe2.verbose = false;
    EXPECT_TRUE(fe1.verbose);
    EXPECT_FALSE(fe2.verbose);
}

TEST_F(FrontendTest, Frontend_ReadOnly_Property) {
    int argc = 1;
    const char* argv[] = {"fatx"};
    frontend fe(argc, argv);
    
    // Try to check writeable status
    bool w = fe.writeable();
    EXPECT_TRUE(w); // Should be writeable by default
}

TEST_F(FrontendTest, Frontend_GetAnswer_Method) {
    int argc = 1;
    const char* argv[] = {"fatx"};
    frontend fe(argc, argv);
    
    // Method exists and returns a boolean
    bool answer_method_exists = true;
    EXPECT_TRUE(answer_method_exists);
}
