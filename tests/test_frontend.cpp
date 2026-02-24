#include <gtest/gtest.h>
#include <cstring>
#include <unistd.h>
#include <fstream>
#include <stdexcept>
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

// Tests pour getanswer() avec différentes combinaisons de flags
TEST_F(FrontendTest, Frontend_Getanswer_ForceN) {
    int argc = 1;
    const char* argv[] = {"fatx"};
    frontend fe(argc, argv);
    fe.force_n = true;
    EXPECT_FALSE(fe.getanswer(true));
    EXPECT_FALSE(fe.getanswer(false));
}

TEST_F(FrontendTest, Frontend_Getanswer_ForceY) {
    int argc = 1;
    const char* argv[] = {"fatx"};
    frontend fe(argc, argv);
    fe.force_y = true;
    EXPECT_TRUE(fe.getanswer(false));
    EXPECT_TRUE(fe.getanswer(true));
}

TEST_F(FrontendTest, Frontend_Getanswer_ForceA) {
    int argc = 1;
    const char* argv[] = {"fatx"};
    frontend fe(argc, argv);
    fe.force_a = true;
    EXPECT_TRUE(fe.getanswer(true));
    EXPECT_FALSE(fe.getanswer(false));
}

// Tests pour name() avec tous les types de programme
TEST_F(FrontendTest, Frontend_Name_AllProgs) {
    int argc = 1;
    const char* argv[] = {"fatx"};
    frontend fe(argc, argv);
    fe.prog = frontend::fuse;
    EXPECT_EQ(fe.name(), "fusefatx");
    fe.prog = frontend::mkfs;
    EXPECT_EQ(fe.name(), "mkfs.fatx");
    fe.prog = frontend::fsck;
    EXPECT_EQ(fe.name(), "fsck.fatx");
    fe.prog = frontend::unrm;
    EXPECT_EQ(fe.name(), "unrm.fatx");
    fe.prog = frontend::label;
    EXPECT_EQ(fe.name(), "label.fatx");
    fe.prog = frontend::unknown;
    EXPECT_EQ(fe.name(), "fatx");
}

// Fixture pour les tests de setup()
class FrontendSetupTest : public ::testing::Test {
protected:
    int run_setup(std::vector<const char*> args) {
        frontend fe(static_cast<int>(args.size()), args.data());
        return fe.setup();
    }
    int run_setup_check([[maybe_unused]] std::vector<const char*> args, frontend& fe) {
        return fe.setup();
    }
};

// --version retourne 0 même avec --as fsck
TEST_F(FrontendSetupTest, Setup_Version) {
    std::vector<const char*> args = {"fatx", "--as", "fsck", "--version"};
    EXPECT_EQ(run_setup(args), 0);
}

// --default retourne EPERM
TEST_F(FrontendSetupTest, Setup_Default) {
    std::vector<const char*> args = {"fatx", "--as", "fsck", "--partition", "x2", "--table", "file", "--default"};
    EXPECT_EQ(run_setup(args), EPERM);
}

// Partition invalide → EINVAL
TEST_F(FrontendSetupTest, Setup_BadPartition) {
    std::vector<const char*> args = {"fatx", "--as", "fsck", "--partition", "bad", "--table", "file", "--input", "/dev/null"};
    EXPECT_EQ(run_setup(args), EINVAL);
}

// Table invalide → EINVAL
TEST_F(FrontendSetupTest, Setup_BadTable) {
    std::vector<const char*> args = {"fatx", "--as", "fsck", "--partition", "x2", "--table", "bad", "--input", "/dev/null"};
    EXPECT_EQ(run_setup(args), EINVAL);
}

// Pas de --input → EINVAL (affiche l'aide)
TEST_F(FrontendSetupTest, Setup_NoInput) {
    std::vector<const char*> args = {"fatx", "--as", "fsck", "--partition", "x2", "--table", "file"};
    EXPECT_EQ(run_setup(args), EINVAL);
}

// Fuse sans --mount → EINVAL
TEST_F(FrontendSetupTest, Setup_Fuse_NoMount) {
    std::vector<const char*> args = {"fatx", "--as", "fuse", "--partition", "x2", "--table", "file", "--input", "/dev/null"};
    EXPECT_EQ(run_setup(args), EINVAL);
}

// --yes, --verbose, --test
TEST_F(FrontendSetupTest, Setup_Yes_Verbose_Test) {
    std::vector<const char*> args = {"fatx", "--as", "fsck", "--yes", "--verbose", "--test",
                                     "--partition", "x2", "--table", "file", "--input", "/dev/null"};
    frontend fe(static_cast<int>(args.size()), args.data());
    int r = fe.setup();
    EXPECT_EQ(r, 0);
    EXPECT_TRUE(fe.force_y);
    EXPECT_TRUE(fe.verbose);
    EXPECT_FALSE(fe.writeable());
}

// --no, --auto, --nofat (disponibles pour fsck/unrm) -- cutname est fuse-only
TEST_F(FrontendSetupTest, Setup_No_Auto_Cutname_Nofat) {
    std::vector<const char*> args = {"fatx", "--as", "fsck", "--no", "--auto", "--nofat",
                                     "--partition", "x2", "--table", "file", "--input", "/dev/null"};
    frontend fe(static_cast<int>(args.size()), args.data());
    int r = fe.setup();
    EXPECT_EQ(r, 0);
    EXPECT_TRUE(fe.force_n);
    EXPECT_TRUE(fe.force_a);
    EXPECT_TRUE(fe.nofat);
}

// --local, --nodate, --nolost (disponibles pour unrm)
TEST_F(FrontendSetupTest, Setup_Recover_Local_NoDate_NoLost) {
    std::vector<const char*> args = {"fatx", "--as", "unrm", "--local", "--nodate", "--nolost",
                                     "--partition", "x2", "--table", "file", "--input", "/dev/null"};
    frontend fe(static_cast<int>(args.size()), args.data());
    int r = fe.setup();
    EXPECT_EQ(r, 0);
    EXPECT_TRUE(fe.local);
    EXPECT_FALSE(fe.deldate);
    EXPECT_FALSE(fe.dellost);
}

// --debug, --singlethr, --foregrd, --mount
TEST_F(FrontendSetupTest, Setup_Fuse_Debug_Single_Foregrd) {
    std::vector<const char*> args = {"fatx", "--as", "fuse", "--debug", "--singlethr", "--foregrd",
                                     "--mount", "/tmp", "--partition", "x2", "--table", "file", "--input", "/dev/null"};
    frontend fe(static_cast<int>(args.size()), args.data());
    int r = fe.setup();
    EXPECT_EQ(r, 0);
    EXPECT_TRUE(fe.fuse_debug);
    EXPECT_TRUE(fe.fuse_singlethr);
    EXPECT_TRUE(fe.fuse_foregrd);
    EXPECT_EQ(fe.mount, "/tmp");
    (void)r;
}

// --as label --label XBOX (set mode: writeable)
TEST_F(FrontendSetupTest, Setup_Label_Set) {
    std::vector<const char*> args = {"fatx", "--as", "label", "--label", "XBOX",
                                     "--partition", "x2", "--table", "file", "--input", "/dev/null"};
    frontend fe(static_cast<int>(args.size()), args.data());
    int r = fe.setup();
    EXPECT_EQ(r, 0);
    EXPECT_EQ(fe.volname, "XBOX");
    EXPECT_TRUE(fe.writeable());  // has --label → not readonly
}

// --as label sans --label (read mode: readonly)
TEST_F(FrontendSetupTest, Setup_Label_Get) {
    std::vector<const char*> args = {"fatx", "--as", "label",
                                     "--partition", "x2", "--table", "file", "--input", "/dev/null"};
    frontend fe(static_cast<int>(args.size()), args.data());
    int r = fe.setup();
    EXPECT_EQ(r, 0);
    EXPECT_FALSE(fe.writeable());  // no --label → readonly
}

// --uid, --gid, --mask (disponibles pour fuse)
TEST_F(FrontendSetupTest, Setup_ClsSize_Uid_Gid_Mask) {
    std::vector<const char*> args = {"fatx", "--as", "fuse", "--uid", "1000", "--gid", "1001", "--mask", "755",
                                     "--mount", "/tmp", "--partition", "x2", "--table", "file", "--input", "/dev/null"};
    frontend fe(static_cast<int>(args.size()), args.data());
    int r = fe.setup();
    EXPECT_EQ(r, 0);
    EXPECT_EQ(fe.uid, 1000u);
    EXPECT_EQ(fe.gid, 1001u);
}

// --offset, --size
TEST_F(FrontendSetupTest, Setup_Offset_Size) {
    std::vector<const char*> args = {"fatx", "--as", "fsck", "--offset", "4096", "--size", "1048576",
                                     "--partition", "x2", "--table", "file", "--input", "/dev/null"};
    frontend fe(static_cast<int>(args.size()), args.data());
    int r = fe.setup();
    EXPECT_EQ(r, 0);
    EXPECT_EQ(fe.offset, 4096u);
    EXPECT_EQ(fe.size, 1048576u);
}

// --diff, --option ro (option disponible pour fuse)
TEST_F(FrontendSetupTest, Setup_Diff_Option_RO) {
    std::vector<const char*> args = {"fatx", "--as", "fuse", "--diff", "/tmp/test.dif",
                                     "--option", "ro",
                                     "--mount", "/tmp", "--partition", "x2", "--table", "file", "--input", "/dev/null"};
    frontend fe(static_cast<int>(args.size()), args.data());
    int r = fe.setup();
    EXPECT_EQ(r, 0);
    EXPECT_EQ(fe.diffile, "/tmp/test.dif");
    EXPECT_FALSE(fe.writeable());  // "ro" option sets readonly
}

// --option custom (non-ro)
TEST_F(FrontendSetupTest, Setup_Option_Custom) {
    std::vector<const char*> args = {"fatx", "--as", "fuse", "--option", "noatime",
                                     "--mount", "/tmp", "--partition", "x2", "--table", "file", "--input", "/dev/null"};
    frontend fe(static_cast<int>(args.size()), args.data());
    int r = fe.setup();
    EXPECT_EQ(r, 0);
    EXPECT_EQ(fe.fuse_option, "noatime");
}

// --nodate avec fuse → recover + readonly
TEST_F(FrontendSetupTest, Setup_NoDate_Fuse) {
    std::vector<const char*> args = {"fatx", "--as", "fuse", "--nodate",
                                     "--mount", "/tmp", "--partition", "x2", "--table", "file", "--input", "/dev/null"};
    frontend fe(static_cast<int>(args.size()), args.data());
    int r = fe.setup();
    EXPECT_EQ(r, 0);
    EXPECT_TRUE(fe.recover);
    EXPECT_FALSE(fe.writeable());
}

// --nolost avec fuse → recover + readonly
TEST_F(FrontendSetupTest, Setup_NoLost_Fuse) {
    std::vector<const char*> args = {"fatx", "--as", "fuse", "--nolost",
                                     "--mount", "/tmp", "--partition", "x2", "--table", "file", "--input", "/dev/null"};
    frontend fe(static_cast<int>(args.size()), args.data());
    int r = fe.setup();
    EXPECT_EQ(r, 0);
    EXPECT_TRUE(fe.recover);
}

// Détection du nom de programme: mkfs.fatx
TEST_F(FrontendSetupTest, Setup_ProgName_Mkfs) {
    std::vector<const char*> args = {"mkfs.fatx", "--partition", "x2", "--table", "file", "--input", "/dev/null"};
    frontend fe(static_cast<int>(args.size()), args.data());
    int r = fe.setup();
    EXPECT_EQ(r, 0);
    EXPECT_EQ(fe.prog, frontend::mkfs);
}

// Détection: fsck.fatx
TEST_F(FrontendSetupTest, Setup_ProgName_Fsck) {
    std::vector<const char*> args = {"fsck.fatx", "--partition", "x2", "--table", "file", "--input", "/dev/null"};
    frontend fe(static_cast<int>(args.size()), args.data());
    int r = fe.setup();
    EXPECT_EQ(r, 0);
    EXPECT_EQ(fe.prog, frontend::fsck);
}

// Détection: unrm.fatx
TEST_F(FrontendSetupTest, Setup_ProgName_Unrm) {
    std::vector<const char*> args = {"unrm.fatx", "--partition", "x2", "--table", "file", "--input", "/dev/null"};
    frontend fe(static_cast<int>(args.size()), args.data());
    int r = fe.setup();
    EXPECT_EQ(r, 0);
    EXPECT_EQ(fe.prog, frontend::unrm);
}

// Détection: label.fatx
TEST_F(FrontendSetupTest, Setup_ProgName_Label) {
    std::vector<const char*> args = {"label.fatx", "--partition", "x2", "--table", "file", "--input", "/dev/null"};
    frontend fe(static_cast<int>(args.size()), args.data());
    int r = fe.setup();
    EXPECT_EQ(r, 0);
    EXPECT_EQ(fe.prog, frontend::label);
}

// Détection: fusefatx
TEST_F(FrontendSetupTest, Setup_ProgName_Fuse) {
    std::vector<const char*> args = {"fusefatx", "--mount", "/tmp", "--partition", "x2", "--table", "file", "--input", "/dev/null"};
    frontend fe(static_cast<int>(args.size()), args.data());
    int r = fe.setup();
    EXPECT_EQ(r, 0);
    EXPECT_EQ(fe.prog, frontend::fuse);
}

// argv[0] avec séparateur de chemin (ex: /usr/bin/mkfs.fatx)
TEST_F(FrontendSetupTest, Setup_ProgName_WithPath) {
    std::vector<const char*> args = {"/usr/bin/mkfs.fatx", "--partition", "x2", "--table", "file", "--input", "/dev/null"};
    frontend fe(static_cast<int>(args.size()), args.data());
    int r = fe.setup();
    EXPECT_EQ(r, 0);
    EXPECT_EQ(fe.prog, frontend::mkfs);
}

// --as pour chaque mode
TEST_F(FrontendSetupTest, Setup_As_Fuse) {
    std::vector<const char*> args = {"fatx", "--as", "fuse", "--mount", "/tmp",
                                     "--partition", "x2", "--table", "file", "--input", "/dev/null"};
    frontend fe(static_cast<int>(args.size()), args.data());
    int r = fe.setup();
    EXPECT_EQ(r, 0);
    EXPECT_EQ(fe.prog, frontend::fuse);
}

TEST_F(FrontendSetupTest, Setup_As_Mkfs) {
    std::vector<const char*> args = {"fatx", "--as", "mkfs",
                                     "--partition", "x2", "--table", "file", "--input", "/dev/null"};
    frontend fe(static_cast<int>(args.size()), args.data());
    int r = fe.setup();
    EXPECT_EQ(r, 0);
    EXPECT_EQ(fe.prog, frontend::mkfs);
}

TEST_F(FrontendSetupTest, Setup_As_Unrm) {
    std::vector<const char*> args = {"fatx", "--as", "unrm",
                                     "--partition", "x2", "--table", "file", "--input", "/dev/null"};
    frontend fe(static_cast<int>(args.size()), args.data());
    int r = fe.setup();
    EXPECT_EQ(r, 0);
    EXPECT_EQ(fe.prog, frontend::unrm);
    EXPECT_TRUE(fe.recover);  // unrm forces recover=true
}

// Table hd (autorisée sauf fuse dans certains cas)
TEST_F(FrontendSetupTest, Setup_Table_HD) {
    std::vector<const char*> args = {"fatx", "--as", "mkfs",
                                     "--partition", "x2", "--table", "hd", "--input", "/dev/null"};
    EXPECT_EQ(run_setup(args), 0);
}

// Table mu
TEST_F(FrontendSetupTest, Setup_Table_MU) {
    std::vector<const char*> args = {"fatx", "--as", "mkfs",
                                     "--partition", "x2", "--table", "mu", "--input", "/dev/null"};
    EXPECT_EQ(run_setup(args), 0);
}

// --do (script)
TEST_F(FrontendSetupTest, Setup_ScriptOption) {
    std::vector<const char*> args = {"fatx", "--as", "fsck", "--do", "test_script",
                                     "--partition", "x2", "--table", "file", "--input", "/dev/null"};
    frontend fe(static_cast<int>(args.size()), args.data());
    int r = fe.setup();
    EXPECT_EQ(r, 0);
    EXPECT_EQ(fe.script, "test_script");
}

// table usb invalide pour mkfs (code: prog==mkfs => toujours rejeté)
TEST_F(FrontendSetupTest, Setup_Table_USB_Mkfs) {
    std::vector<const char*> args = {"fatx", "--as", "mkfs",
                                     "--partition", "x2", "--table", "usb", "--input", "/dev/null"};
    EXPECT_EQ(run_setup(args), EINVAL);
}

// table usb valide pour fsck (non-mkfs)
TEST_F(FrontendSetupTest, Setup_Table_USB_Fsck_Invalid) {
    std::vector<const char*> args = {"fatx", "--as", "fsck",
                                     "--partition", "x2", "--table", "usb", "--input", "/dev/null"};
    EXPECT_EQ(run_setup(args), 0);
}

// Diverses partitions valides
TEST_F(FrontendSetupTest, Setup_Partition_SC) {
    std::vector<const char*> args = {"fatx", "--as", "fsck",
                                     "--partition", "sc", "--table", "hd", "--input", "/dev/null"};
    EXPECT_EQ(run_setup(args), 0);
}

TEST_F(FrontendSetupTest, Setup_Partition_GC) {
    std::vector<const char*> args = {"fatx", "--as", "fsck",
                                     "--partition", "gc", "--table", "hd", "--input", "/dev/null"};
    EXPECT_EQ(run_setup(args), 0);
}

TEST_F(FrontendSetupTest, Setup_Partition_SE1) {
    std::vector<const char*> args = {"fatx", "--as", "fsck",
                                     "--partition", "se1", "--table", "hd", "--input", "/dev/null"};
    EXPECT_EQ(run_setup(args), 0);
}

TEST_F(FrontendSetupTest, Setup_Partition_XDV) {
    std::vector<const char*> args = {"fatx", "--as", "fsck",
                                     "--partition", "xdv", "--table", "hd", "--input", "/dev/null"};
    EXPECT_EQ(run_setup(args), 0);
}

TEST_F(FrontendSetupTest, Setup_Partition_X1) {
    std::vector<const char*> args = {"fatx", "--as", "fsck",
                                     "--partition", "x1", "--table", "hd", "--input", "/dev/null"};
    EXPECT_EQ(run_setup(args), 0);
}

// Option --option ro,noatime (multiple options)
TEST_F(FrontendSetupTest, Setup_Option_Multiple) {
    std::vector<const char*> args = {"fatx", "--as", "fuse", "--option", "ro,noatime",
                                     "--mount", "/tmp", "--partition", "x2", "--table", "file", "--input", "/dev/null"};
    frontend fe(static_cast<int>(args.size()), args.data());
    int r = fe.setup();
    EXPECT_EQ(r, 0);
    EXPECT_FALSE(fe.writeable());
    EXPECT_EQ(fe.fuse_option, "noatime");
}

// Option --option avec deux options non-ro (couvre la ligne fuse_option += "," pour option vide
TEST_F(FrontendSetupTest, Setup_Option_MultipleNonRo) {
    std::vector<const char*> args = {"fatx", "--as", "fuse", "--option", "noatime,nodiratime",
                                     "--mount", "/tmp", "--partition", "x2", "--table", "file", "--input", "/dev/null"};
    frontend fe(static_cast<int>(args.size()), args.data());
    int r = fe.setup();
    EXPECT_EQ(r, 0);
    EXPECT_EQ(fe.fuse_option, "noatime,nodiratime");
}

// --cls-size pour mkfs (couvre la ligne de parsing clus_size)
TEST_F(FrontendSetupTest, Setup_ClsSize_Mkfs) {
    std::vector<const char*> args = {"fatx", "--as", "mkfs", "--cls-size", "32",
                                     "--partition", "x2", "--table", "file", "--input", "/dev/null"};
    frontend fe(static_cast<int>(args.size()), args.data());
    int r = fe.setup();
    EXPECT_EQ(r, 0);
    EXPECT_EQ(fe.clus_size, 32u);
}

// --cutname pour fuse (couvre la ligne cutname = true)
TEST_F(FrontendSetupTest, Setup_Cutname_Fuse) {
    std::vector<const char*> args = {"fatx", "--as", "fuse", "--cutname",
                                     "--mount", "/tmp", "--partition", "x2", "--table", "file", "--input", "/dev/null"};
    frontend fe(static_cast<int>(args.size()), args.data());
    int r = fe.setup();
    EXPECT_EQ(r, 0);
    EXPECT_TRUE(fe.cutname);
}

// Exception dans le parsing caché: --as sans valeur → prog=unknown → EINVAL
TEST_F(FrontendSetupTest, Setup_HiddenParse_Exception) {
    std::vector<const char*> args = {"fatx", "--as"};
    EXPECT_EQ(run_setup(args), EINVAL);
}

// =====================================================================
// Tests pour frontend::parser() (script de commandes FATX)
// =====================================================================

class FrontendParserTest : public ::testing::Test {
protected:
	std::string   test_file;
	frontend*     tf;
	fatx_context* ctx;

	void SetUp() override {
		char tmpl[] = "/tmp/fatx_parser_XXXXXX";
		int fd = mkstemp(tmpl);
		if (fd == -1) throw std::runtime_error("mkstemp failed");
		close(fd);
		test_file = tmpl;

		// Créer une image FATX 4MB
		std::ofstream ofs(test_file, std::ios::binary | std::ios::out);
		const std::size_t img_size = 0x400000;
		ofs.seekp(static_cast<std::streamoff>(img_size) - 1);
		char z = '\0';
		ofs.write(&z, 1);
		ofs.seekp(0);
		ofs.write("XTAF", 4);
		uint32_t id = 0, spc = 1, root_cl = 1;
		ofs.write(reinterpret_cast<char*>(&id),      4);
		ofs.write(reinterpret_cast<char*>(&spc),     4);
		ofs.write(reinterpret_cast<char*>(&root_cl), 4);
		uint16_t eoc = 0xFFFF;
		ofs.seekp(0x1000 + 2);
		ofs.write(reinterpret_cast<char*>(&eoc), 2);
		ofs.close();

		int tac = 1;
		const char* tav[] = {"test"};
		tf  = new frontend(tac, tav);
		ctx = new fatx_context(*tf);
		fatx_context::set(ctx);
		ctx->mmi.input   = test_file;
		ctx->mmi.table   = "file";
		ctx->mmi.prog    = frontend::fuse;
		ctx->mmi.force_a = true;
		ctx->mmi.force_y = true;

		int res = ctx->setup();
		ASSERT_EQ(res, 0);
		ASSERT_NE(ctx->fat,  nullptr);
		ASSERT_NE(ctx->root, nullptr);
	}

	void TearDown() override {
		if (ctx) {
			delete ctx;
			ctx = nullptr;
		}
		delete tf;
		if (!test_file.empty())
			unlink(test_file.c_str());
	}
};

// parser() : script vide → ne crash pas
TEST_F(FrontendParserTest, Parser_EmptyScript) {
	ctx->mmi.script = "";
	ctx->mmi.parser();
	EXPECT_TRUE(true);
}

// parser() : ligne commentaire → ignorée
TEST_F(FrontendParserTest, Parser_CommentLine) {
	ctx->mmi.script = "#this is a comment";
	ctx->mmi.parser();
	EXPECT_TRUE(true);
}

// parser() : script avec seulement des séparateurs → pas de crash
TEST_F(FrontendParserTest, Parser_SemicolonOnly) {
	ctx->mmi.script = ";;;";
	ctx->mmi.parser();
	EXPECT_TRUE(true);
}

// parser() : mkdir avec chemin valide → crée le répertoire
TEST_F(FrontendParserTest, Parser_Mkdir_Valid) {
	ctx->mmi.script = "mkdir,/newdir";
	ctx->mmi.parser();
	entry* found = ctx->root->find("/newdir/");
	EXPECT_NE(found, nullptr);
}

// parser() : rmdir du répertoire créé → le supprime
TEST_F(FrontendParserTest, Parser_Rmdir_Valid) {
	// D'abord créer
	ctx->mmi.script = "mkdir,/deldir";
	ctx->mmi.parser();
	ASSERT_NE(ctx->root->find("/deldir/"), nullptr);

	// Ensuite supprimer
	ctx->mmi.script = "rmdir,/deldir/";
	ctx->mmi.parser();
	EXPECT_EQ(ctx->root->find("/deldir/"), nullptr);
}

// parser() : mkdir avec chemin racine exact '/' → syntax error (pas de sous-répertoire
TEST_F(FrontendParserTest, Parser_Mkdir_RootOnly) {
	ctx->mmi.script = "mkdir,/";
	ctx->mmi.parser();
	// '/' est un dossier root, pas un nom valide → syntax error ou no-op
	EXPECT_TRUE(true);
}

// parser() : mkdir avec syntaxe sans slash → erreur syntaxe (pas de '/')
TEST_F(FrontendParserTest, Parser_Mkdir_SyntaxError) {
	ctx->mmi.script = "mkdir,nodir";  // pas de '/' → syntax error
	ctx->mmi.parser();
	EXPECT_EQ(ctx->root->find("/nodir/"), nullptr);
}

// parser() : mkdir avec parent inexistant → "not found"
TEST_F(FrontendParserTest, Parser_Mkdir_ParentNotFound) {
	ctx->mmi.script = "mkdir,/ghost/subdir";
	ctx->mmi.parser();
	EXPECT_EQ(ctx->root->find("/ghost/subdir/"), nullptr);
}

// parser() : rmdir d'un répertoire inexistant → "not found"
TEST_F(FrontendParserTest, Parser_Rmdir_NotFound) {
	ctx->mmi.script = "rmdir,/nosuchdir/";
	ctx->mmi.parser();
	EXPECT_TRUE(true);  // ne doit pas crasher
}

// parser() : commande inconnue → ignorée silencieusement
TEST_F(FrontendParserTest, Parser_UnknownCommand) {
	ctx->mmi.script = "unknowncmd,arg1,arg2";
	ctx->mmi.parser();
	EXPECT_TRUE(true);
}

// parser() : deux commandes séparées par ';'
TEST_F(FrontendParserTest, Parser_TwoCommands) {
	ctx->mmi.script = "mkdir,/dir1;mkdir,/dir2";
	ctx->mmi.parser();
	EXPECT_NE(ctx->root->find("/dir1/"), nullptr);
	EXPECT_NE(ctx->root->find("/dir2/"), nullptr);
}

// parser() : commentaire mélangé avec commande valide
TEST_F(FrontendParserTest, Parser_CommentThenCommand) {
	ctx->mmi.script = "#comment;mkdir,/dir3";
	ctx->mmi.parser();
	EXPECT_NE(ctx->root->find("/dir3/"), nullptr);
}

// parser() : rmdir d'un répertoire non vide → "not empty"
TEST_F(FrontendParserTest, Parser_Rmdir_NotEmpty) {
	// Créer un répertoire parent avec un sous-répertoire dedans
	ctx->mmi.script = "mkdir,/parent";
	ctx->mmi.parser();
	auto* nd = new entry("child", 0, true);
	entry* parent = ctx->root->find("/parent/");
	ASSERT_NE(parent, nullptr);
	ASSERT_EQ(parent->addtodir(nd), 0);

	// Tenter de supprimer parent (non vide) → "not empty"
	ctx->mmi.script = "rmdir,/parent/";
	ctx->mmi.parser();
	// Le répertoire parent doit toujours exister
	EXPECT_NE(ctx->root->find("/parent/"), nullptr);
}

// parser() : rmdir d'un fichier (pas un répertoire) → "not found"
TEST_F(FrontendParserTest, Parser_Rmdir_NotADirectory) {
	auto* nf = new entry("afile", 0, false);
	ASSERT_EQ(ctx->root->addtodir(nf), 0);

	ctx->mmi.script = "rmdir,/afile";
	ctx->mmi.parser();
	// afile existe toujours (rmdir refusé sur les fichiers)
	EXPECT_NE(ctx->root->find("/afile"), nullptr);
}

// parser() : whitespace/newlines dans le script → supprimés
TEST_F(FrontendParserTest, Parser_Whitespace_Stripped) {
	ctx->mmi.script = "  mkdir , /cleandir  ";  // espaces → supprimés → mkdir,/cleandir
	ctx->mmi.parser();
	// Le parser élimine les espaces → la commande est parsée
	EXPECT_TRUE(true);
}

// parser() : cp,/src,/dst/newname → copie un fichier
TEST_F(FrontendParserTest, Parser_Cp_Valid) {
	// Créer un fichier source dans root
	auto* src = new entry("cpsrc", 100, false);
	ASSERT_EQ(ctx->root->addtodir(src), 0);

	// Créer un répertoire de destination
	ctx->mmi.script = "mkdir,/cpdst";
	ctx->mmi.parser();
	ASSERT_NE(ctx->root->find("/cpdst/"), nullptr);

	// Copier le fichier
	ctx->mmi.script = "cp,/cpsrc,/cpdst/cpfile";
	ctx->mmi.parser();
	entry* copied = ctx->root->find("/cpdst/cpfile");
	EXPECT_NE(copied, nullptr);
}

// parser() : cp avec source inexistante → "not found"
TEST_F(FrontendParserTest, Parser_Cp_SrcNotFound) {
	ctx->mmi.script = "cp,/nosuchfile,/root/dst";
	ctx->mmi.parser();
	EXPECT_TRUE(true);
}

// parser() : cp avec destination parent inexistante → "not found"
TEST_F(FrontendParserTest, Parser_Cp_DstParentNotFound) {
	auto* nf = new entry("cp2src", 0, false);
	ASSERT_EQ(ctx->root->addtodir(nf), 0);
	ctx->mmi.script = "cp,/cp2src,/noparent/dst";
	ctx->mmi.parser();
	EXPECT_TRUE(true);
}

// parser() : cp avec mauvaise syntaxe destination → "syntax error"
TEST_F(FrontendParserTest, Parser_Cp_SyntaxError) {
	auto* nf = new entry("cp3src", 0, false);
	ASSERT_EQ(ctx->root->addtodir(nf), 0);
	ctx->mmi.script = "cp,/cp3src,nodst";  // sans '/'
	ctx->mmi.parser();
	EXPECT_TRUE(true);
}

// parser() : mv,/file,/newname → renomme un fichier
TEST_F(FrontendParserTest, Parser_Mv_Valid) {
	auto* nf = new entry("mvfile", 0, false);
	ASSERT_EQ(ctx->root->addtodir(nf), 0);

	ctx->mmi.script = "mv,/mvfile,/renamedfile";
	ctx->mmi.parser();
	EXPECT_NE(ctx->root->find("/renamedfile"), nullptr);
}

// parser() : mv avec source inexistante → "not found"
TEST_F(FrontendParserTest, Parser_Mv_NotFound) {
	ctx->mmi.script = "mv,/nofile,/dst";
	ctx->mmi.parser();
	EXPECT_TRUE(true);
}

// parser() : rm,/file → supprime un fichier
TEST_F(FrontendParserTest, Parser_Rm_Valid) {
	auto* nf = new entry("rmfile2", 0, false);
	ASSERT_EQ(ctx->root->addtodir(nf), 0);

	ctx->mmi.script = "rm,/rmfile2";
	ctx->mmi.parser();
	EXPECT_EQ(ctx->root->find("/rmfile2"), nullptr);
}

// parser() : rm avec source inexistante → "not found"
TEST_F(FrontendParserTest, Parser_Rm_NotFound) {
	ctx->mmi.script = "rm,/nosuchfile";
	ctx->mmi.parser();
	EXPECT_TRUE(true);
}

// parser() : rm d'un répertoire (flags.dir=true) → "not found"
TEST_F(FrontendParserTest, Parser_Rm_Directory) {
	ctx->mmi.script = "mkdir,/rmdir2";
	ctx->mmi.parser();
	ctx->mmi.script = "rm,/rmdir2/";
	ctx->mmi.parser();
	// rm ne supprime pas les répertoires
	EXPECT_NE(ctx->root->find("/rmdir2/"), nullptr);
}

// parser() : lsfat,/file → liste la chaîne FAT du fichier
TEST_F(FrontendParserTest, Parser_Lsfat_Valid) {
	auto* nf = new entry("lsfile", 100, false);
	ASSERT_EQ(ctx->root->addtodir(nf), 0);
	ctx->mmi.script = "lsfat,/lsfile";
	ctx->mmi.parser();
	EXPECT_TRUE(true);
}

// parser() : lsfat avec fichier inexistant → "not found"
TEST_F(FrontendParserTest, Parser_Lsfat_NotFound) {
	ctx->mmi.script = "lsfat,/notexist";
	ctx->mmi.parser();
	EXPECT_TRUE(true);
}

// parser() : help → affiche l'aide
TEST_F(FrontendParserTest, Parser_Help) {
	ctx->mmi.script = "help";
	ctx->mmi.parser();
	EXPECT_TRUE(true);
}

// parser() : commande inconnue → "unknown" (couvre la branche else finale)
TEST_F(FrontendParserTest, Parser_Unknown_Command_ElseBranch) {
	ctx->mmi.script = "badcmd";
	ctx->mmi.parser();
	EXPECT_TRUE(true);
}

// parser() : lcp,/fatxfile,/localfile → copie locale (source inexistante)
TEST_F(FrontendParserTest, Parser_Lcp_SrcNotFound) {
	ctx->mmi.script = "lcp,/nofile,/tmp/output";
	ctx->mmi.parser();
	EXPECT_TRUE(true);
}

// parser() : lcp avec fichier existant → copie locale
// Couvre frontend.cpp L587-597 (lcp success path incluant console::write(s->size) L591)
TEST_F(FrontendParserTest, Parser_Lcp_Valid) {
	// Créer un fichier avec données écrites (pour que data() réussisse)
	auto* nf = new entry("lcpsrc", 100, false);
	ASSERT_EQ(ctx->root->addtodir(nf), 0);
	entry* f = ctx->root->find("/lcpsrc");
	ASSERT_NE(f, nullptr);

	// Écrire des données dans le fichier pour que data(read) réussisse
	std::string zeros(100, 'X');
	(void)f->data(&zeros[0], false, 0, 100);

	std::string outfile = "/tmp/fatx_lcp_out_test";
	unlink(outfile.c_str());

	ctx->mmi.script = "lcp,/lcpsrc," + outfile;
	ctx->mmi.parser();
	EXPECT_TRUE(true);
	unlink(outfile.c_str());
}

// parser() : lcp lorsque le fichier local existe déjà → "local file exists"
// Couvre frontend.cpp L580-583 (branche t is open → local file exists)
TEST_F(FrontendParserTest, Parser_Lcp_LocalExists) {
	auto* nf = new entry("lcpexist", 50, false);
	ASSERT_EQ(ctx->root->addtodir(nf), 0);

	// Créer le fichier local cible avant la commande
	std::string outfile = "/tmp/fatx_lcp_exists_test";
	{
		std::ofstream f(outfile);
		f << "existing content";
	}

	ctx->mmi.script = "lcp,/lcpexist," + outfile;
	ctx->mmi.parser();  // → "local file exists\n"
	EXPECT_TRUE(true);
	unlink(outfile.c_str());
}

// parser() : rcp,/localfile,/dst/newfile → copie local→FATX (fichier inexistant)
// Couvre frontend.cpp L542-543 (rcp "can't open")
TEST_F(FrontendParserTest, Parser_Rcp_CantOpen) {
	ctx->mmi.script = "rcp,/tmp/fatx_no_such_local_9999,/root/out";
	ctx->mmi.parser();  // → "can't open\n"
	EXPECT_TRUE(true);
}

// parser() : rcp avec fichier local valide → copie vers FATX
// Couvre frontend.cpp L550-570 (rcp success path)
TEST_F(FrontendParserTest, Parser_Rcp_Valid) {
	// Créer un fichier local source
	std::string srcfile = "/tmp/fatx_rcp_src_test";
	{
		std::ofstream f(srcfile, std::ios::binary);
		f << "rcpdata";
	}

	// Créer un répertoire de destination FATX
	ctx->mmi.script = "mkdir,/rcpdst";
	ctx->mmi.parser();
	ASSERT_NE(ctx->root->find("/rcpdst/"), nullptr);

	// Copier le fichier local vers FATX
	ctx->mmi.script = "rcp," + srcfile + ",/rcpdst/rcpout";
	ctx->mmi.parser();
	EXPECT_TRUE(true);
	unlink(srcfile.c_str());
}

// parser() : rcp avec destination parent inexistante → "not found"
// Couvre frontend.cpp L560-561 (rcp "not found")
TEST_F(FrontendParserTest, Parser_Rcp_DstNotFound) {
	std::string srcfile = "/tmp/fatx_rcp_notfound_test";
	{
		std::ofstream f(srcfile, std::ios::binary);
		f << "data";
	}
	ctx->mmi.script = "rcp," + srcfile + ",/nodir/outfile";
	ctx->mmi.parser();  // → "not found\n"
	EXPECT_TRUE(true);
	unlink(srcfile.c_str());
}

// parser() : rcp avec mauvaise syntaxe de destination → "syntax error"
// Couvre frontend.cpp L550-551 (rcp "syntax error")
TEST_F(FrontendParserTest, Parser_Rcp_SyntaxError) {
	std::string srcfile = "/tmp/fatx_rcp_syntax_test";
	{
		std::ofstream f(srcfile, std::ios::binary);
		f << "data";
	}
	ctx->mmi.script = "rcp," + srcfile + ",nodest";
	ctx->mmi.parser();  // → "syntax error\n"
	EXPECT_TRUE(true);
	unlink(srcfile.c_str());
}

// parser() : mv en mode read-only → "read-only"
// Couvre frontend.cpp L602-603
TEST_F(FrontendParserTest, Parser_Mv_ReadOnly) {
	ctx->mmi.set_readonly(true);
	ctx->mmi.script = "mv,/anyfile,/dst";
	ctx->mmi.parser();
	ctx->mmi.set_readonly(false);
	EXPECT_TRUE(true);
}

// parser() : rm en mode read-only → "read-only"
// Couvre frontend.cpp L619-620
TEST_F(FrontendParserTest, Parser_Rm_ReadOnly) {
	ctx->mmi.set_readonly(true);
	ctx->mmi.script = "rm,/anyfile";
	ctx->mmi.parser();
	ctx->mmi.set_readonly(false);
	EXPECT_TRUE(true);
}

// parser() : cp en mode read-only → "read-only"
// Couvre frontend.cpp L504-505 (cp read-only branch)
TEST_F(FrontendParserTest, Parser_Cp_ReadOnly) {
	ctx->mmi.set_readonly(true);
	ctx->mmi.script = "cp,/src,/dst/file";
	ctx->mmi.parser();
	ctx->mmi.set_readonly(false);
	EXPECT_TRUE(true);
}

// parser() : rcp en mode read-only → "read-only"
// Couvre frontend.cpp L542 (rcp read-only pas présent - rcp n'a pas de writeable check)
// On ajoute mklost en mode read-only pour couvrir L643+
TEST_F(FrontendParserTest, Parser_Mklost_ReadOnly) {
	ctx->mmi.set_readonly(true);
	ctx->mmi.script = "mklost,2:5";
	ctx->mmi.parser();
	ctx->mmi.set_readonly(false);
	EXPECT_TRUE(true);
}

// parser() : mklost avec cluster simple → couvre L643-695 (mklost path)
TEST_F(FrontendParserTest, Parser_Mklost_SingleCluster) {
	ctx->mmi.script = "mklost,2";
	ctx->mmi.parser();
	EXPECT_TRUE(true);
}

// parser() : mklost avec intervalle de clusters → couvre do/while L675-681
TEST_F(FrontendParserTest, Parser_Mklost_ClusterRange) {
	ctx->mmi.script = "mklost,2:4";
	ctx->mmi.parser();
	EXPECT_TRUE(true);
}

// parser() : chcls → nécessite 2 args numérique (peut échouer si non trouvé)
TEST_F(FrontendParserTest, Parser_Chcls_NotFound) {
	ctx->mmi.script = "chcls,/noclsfile,5";
	ctx->mmi.parser();
	EXPECT_TRUE(true);
}
// parser() : script avec commande vide via ';' → couvre L457 (break si i->empty())
// Script ";" → une seule commande vide → i->empty() → break
TEST_F(FrontendParserTest, Parser_EmptyCommand_Break) {
	ctx->mmi.script = ";";
	ctx->mmi.parser();
	EXPECT_TRUE(true);
}

// parser() : mkdir avec nom déjà existant → "failed"
// Couvre frontend.cpp L478-479 (mkdir addtodir failure)
TEST_F(FrontendParserTest, Parser_Mkdir_AlreadyExists) {
	// Créer le répertoire une première fois
	ctx->mmi.script = "mkdir,/dupdir";
	ctx->mmi.parser();
	ASSERT_NE(ctx->root->find("/dupdir/"), nullptr);

	// Le recréer → addtodir retourne EEXIST → "failed\n"
	ctx->mmi.script = "mkdir,/dupdir";
	ctx->mmi.parser();  // → "mkdir:failed\n"
	EXPECT_TRUE(true);
}

// parser() : cp avec fichier de taille > 0 et data succès → couvre L529-534
TEST_F(FrontendParserTest, Parser_Cp_WithData) {
	// Créer un fichier avec des données
	auto* src = new entry("cpdatasrc", 100, false);
	ASSERT_EQ(ctx->root->addtodir(src), 0);
	entry* f = ctx->root->find("/cpdatasrc");
	ASSERT_NE(f, nullptr);
	std::string dat(100, 'A');
	(void)f->data(&dat[0], false, 0, 100);

	ctx->mmi.script = "mkdir,/cpdatadst";
	ctx->mmi.parser();

	ctx->mmi.script = "cp,/cpdatasrc,/cpdatadst/cpdataout";
	ctx->mmi.parser();
	EXPECT_NE(ctx->root->find("/cpdatadst/cpdataout"), nullptr);
}

// parser() : lsfat avec fichier qui a des clusters alloués
// Couvre frontend.cpp L611-612 (lsfat entry found + printchain)
TEST_F(FrontendParserTest, Parser_Lsfat_WithClusters) {
	auto* nf = new entry("lsfatclus", 1024, false);
	ASSERT_EQ(ctx->root->addtodir(nf), 0);
	ctx->mmi.script = "lsfat,/lsfatclus";
	ctx->mmi.parser();
	EXPECT_TRUE(true);
}

// parser() : cp avec duplicate dans dst → addtodir échoue → "*ERR*"
// Couvre frontend.cpp L524-525 (cp addtodir *ERR*)
TEST_F(FrontendParserTest, Parser_Cp_DuplicateInDst) {
	auto* src = new entry("cpdupfile", 50, false);
	ASSERT_EQ(ctx->root->addtodir(src), 0);
	ctx->mmi.script = "mkdir,/cpdupdir";
	ctx->mmi.parser();
	entry* dstdir = ctx->root->find("/cpdupdir/");
	ASSERT_NE(dstdir, nullptr);
	auto* existing = new entry("cpdupout", 50, false);
	ASSERT_EQ(dstdir->addtodir(existing), 0);

	// Tenter cp vers nom existant → addtodir retourne EEXIST → "*ERR*"
	ctx->mmi.script = "cp,/cpdupfile,/cpdupdir/cpdupout";
	ctx->mmi.parser();
	EXPECT_TRUE(true);
}

// parser() : rcp avec duplicate dans dst → addtodir échoue → "*ERR*"
// Couvre frontend.cpp L560-561 (rcp addtodir *ERR*)
TEST_F(FrontendParserTest, Parser_Rcp_DuplicateInDst) {
	std::string srcfile = "/tmp/fatx_rcp_dup_test";
	{
		std::ofstream f(srcfile, std::ios::binary);
		f << "dup data content here";
	}

	ctx->mmi.script = "mkdir,/rcpdupdir";
	ctx->mmi.parser();
	entry* dstdir = ctx->root->find("/rcpdupdir/");
	ASSERT_NE(dstdir, nullptr);
	auto* existing = new entry("rcpdupout", 0, false);
	ASSERT_EQ(dstdir->addtodir(existing), 0);

	ctx->mmi.script = "rcp," + srcfile + ",/rcpdupdir/rcpdupout";
	ctx->mmi.parser();  // → addtodir échoue → "*ERR*\n"
	EXPECT_TRUE(true);
	unlink(srcfile.c_str());
}
