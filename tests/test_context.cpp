#include <gtest/gtest.h>
#include <fstream>
#include <filesystem>
#include <cstdlib>
#include <unistd.h>
#include <stdexcept>
#include "context.hpp"

// Tests pour la classe fatx_context
class ContextTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Créer un fichier temporaire pour les tests using mkstemp (safer than tmpnam)
        char tmpl[] = "/tmp/fatx_test_XXXXXX";
        int fd = mkstemp(tmpl);
        if (fd == -1) {
            throw std::runtime_error("mkstemp failed to create temp file");
        }
        // Close the fd; we'll open via ofstream
        close(fd);
        test_file = tmpl;
        std::ofstream ofs(test_file, std::ios::binary);
        const size_t test_data_size = 1024;
        test_data.assign(test_data_size, 'A');
        ofs.write(test_data.c_str(), test_data_size);
        ofs.close();
    }

    void TearDown() override {
        // Nettoyer les fichiers temporaires
        if (std::filesystem::exists(test_file)) {
            std::filesystem::remove(test_file);
        }
    }

    std::string test_file;
    std::string test_data;
};

// Tests pour la gestion du contexte global
TEST_F(ContextTest, Context_SetAndGet) {
    // Créer un contexte
    int argc = 1;
    const char* argv[] = {"test"};
    frontend mmi(argc, argv);
    mmi.input = test_file;
    mmi.table = "";
    mmi.diffile = "";
    
    fatx_context *ctx = new fatx_context(mmi);
    
    // Tester set/get
    fatx_context::set(ctx);
    EXPECT_NE(fatx_context::get(), nullptr);
    EXPECT_EQ(fatx_context::get(), ctx);
    
    // Cleanup
    fatx_context::set(nullptr);
    delete ctx;
}

TEST_F(ContextTest, Context_Constructor_InitializesMM) {
    // Créer un contexte
    int argc = 1;
    const char* argv[] = {"test"};
    frontend mmi(argc, argv);
    mmi.input = test_file;
    mmi.table = "";
    mmi.diffile = "";
    
    fatx_context ctx(mmi);
    
    // Vérifier que le contexte est créé
    EXPECT_EQ(ctx.ready, false); // Pas setup() appelé
}

TEST_F(ContextTest, Context_MultipleInstances) {
    // Créer deux instances
    int argc = 1;
    const char* argv[] = {"test"};
    frontend mmi1(argc, argv);
    frontend mmi2(argc, argv);
    
    mmi1.input = test_file;
    mmi1.table = "";
    mmi1.diffile = "";
    
    mmi2.input = test_file;
    mmi2.table = "";
    mmi2.diffile = "";
    
    fatx_context *ctx1 = new fatx_context(mmi1);
    
    // Définir le contexte global
    fatx_context::set(ctx1);
    EXPECT_EQ(fatx_context::get(), ctx1);
    
    // Cleanup
    fatx_context::set(nullptr);
    delete ctx1;
}

TEST_F(ContextTest, Context_ClusterArithmetic_ptr2cls) {
    // Créer un contexte pour tester ptr2cls
    int argc = 1;
    const char* argv[] = {"test"};
    frontend mmi(argc, argv);
    mmi.input = test_file;
    mmi.table = "";
    mmi.diffile = "";
    
    auto* ctx = new fatx_context(mmi);
    fatx_context::set(ctx);
    
    // Set partition parameters
    fatx_context::get()->par.clus_pow = 12;
    fatx_context::get()->par.clus_size = 4096;
    fatx_context::get()->par.root_start = 0x10000;
    fatx_context::get()->par.root_clus = 1;
    
    // Test pointer -> cluster conversion
    streamptr ptr = 0x10000; // Première position
    clusptr cls = clsarithm::ptr2cls(ptr);
    
    EXPECT_EQ(cls, 1); // First cluster
    
    // Cleanup
    fatx_context::set(nullptr);
    delete ctx;
}

TEST_F(ContextTest, Context_ClusterArithmetic_cls2ptr) {
    // Créer un contexte pour tester cls2ptr
    int argc = 1;
    const char* argv[] = {"test"};
    frontend mmi(argc, argv);
    mmi.input = test_file;
    mmi.table = "";
    mmi.diffile = "";
    
    auto* ctx = new fatx_context(mmi);
    fatx_context::set(ctx);
    
    // Set partition parameters
    fatx_context::get()->par.clus_pow = 12;
    fatx_context::get()->par.clus_size = 4096;
    fatx_context::get()->par.root_start = 0x10000;
    fatx_context::get()->par.root_clus = 1;
    fatx_context::get()->par.clus_fat = 0x10000;
    
    // Test cluster -> pointer conversion
    clusptr cls = 1;
    streamptr ptr = clsarithm::cls2ptr(cls);
    
    EXPECT_EQ(ptr, 0x10000); // root_start
    
    // Cleanup
    fatx_context::set(nullptr);
    delete ctx;
}

TEST_F(ContextTest, Context_Destroy_Method) {
    // Créer un contexte
    int argc = 1;
    const char* argv[] = {"test"};
    frontend mmi(argc, argv);
    mmi.input = test_file;
    mmi.table = "";
    mmi.diffile = "";
    
    fatx_context *ctx = new fatx_context(mmi);
    fatx_context::set(ctx);
    
    // Tester que destroy() peut être appelé
    ctx->destroy();
    
    // Cleanup
    fatx_context::set(nullptr);
    delete ctx;
}

TEST_F(ContextTest, Context_Frontend_Reference) {
    // Créer un contexte
    int argc = 1;
    const char* argv[] = {"test"};
    frontend mmi(argc, argv);
    mmi.input = test_file;
    mmi.table = "";
    mmi.diffile = "";
    
    fatx_context ctx(mmi);
    
    // Vérifier que la référence frontend est accessible
    EXPECT_EQ(&ctx.mmi, &mmi);
}

// Additional extensive tests for context coverage
TEST_F(ContextTest, Context_ClusterArithmetic_siz2cls_Boundary) {
    int argc = 1;
    const char* argv[] = {"test"};
    frontend mmi(argc, argv);
    mmi.input = test_file;
    mmi.table = "";
    mmi.diffile = "";
    
    auto* ctx = new fatx_context(mmi);
    fatx_context::set(ctx);
    
    fatx_context::get()->par.clus_pow = 12;
    fatx_context::get()->par.clus_size = 4096;
    
    // Test boundary cases
    filesize size_exact = 4096;
    clusptr clusters_exact = clsarithm::siz2cls(size_exact);
    EXPECT_EQ(clusters_exact, 1);
    
    filesize size_plus_one = 4097;
    clusptr clusters_plus = clsarithm::siz2cls(size_plus_one);
    EXPECT_EQ(clusters_plus, 2);
    
    fatx_context::set(nullptr);
    delete ctx;
}

TEST_F(ContextTest, Context_ClusterArithmetic_Zero_Size) {
    int argc = 1;
    const char* argv[] = {"test"};
    frontend mmi(argc, argv);
    mmi.input = test_file;
    mmi.table = "";
    mmi.diffile = "";
    
    auto* ctx = new fatx_context(mmi);
    fatx_context::set(ctx);
    
    fatx_context::get()->par.clus_pow = 12;
    fatx_context::get()->par.clus_size = 4096;
    
    // Test zero size
    filesize zero_size = 0;
    clusptr zero_clusters = clsarithm::siz2cls(zero_size);
    EXPECT_EQ(zero_clusters, 0);
    
    fatx_context::set(nullptr);
    delete ctx;
}

TEST_F(ContextTest, Context_ClusterArithmetic_Large_Size) {
    int argc = 1;
    const char* argv[] = {"test"};
    frontend mmi(argc, argv);
    mmi.input = test_file;
    mmi.table = "";
    mmi.diffile = "";
    
    auto* ctx = new fatx_context(mmi);
    fatx_context::set(ctx);
    
    fatx_context::get()->par.clus_pow = 12;
    fatx_context::get()->par.clus_size = 4096;
    
    // Test large size
    filesize large_size = 0x40000000ULL;
    clusptr large_clusters = clsarithm::siz2cls(large_size);
    EXPECT_GT(large_clusters, 0);
    
    fatx_context::set(nullptr);
    delete ctx;
}

TEST_F(ContextTest, Context_ClusterArithmetic_ptr2cls_Multiple) {
    int argc = 1;
    const char* argv[] = {"test"};
    frontend mmi(argc, argv);
    mmi.input = test_file;
    mmi.table = "";
    mmi.diffile = "";
    
    auto* ctx = new fatx_context(mmi);
    fatx_context::set(ctx);
    
    fatx_context::get()->par.clus_pow = 12;
    fatx_context::get()->par.clus_size = 4096;
    fatx_context::get()->par.root_start = 0x10000;
    fatx_context::get()->par.root_clus = 1;
    
    // Test multiple cluster pointers
    streamptr ptr1 = 0x10000;
    clusptr cls1 = clsarithm::ptr2cls(ptr1);
    EXPECT_EQ(cls1, 1);
    
    streamptr ptr2 = 0x11000;
    clusptr cls2 = clsarithm::ptr2cls(ptr2);
    EXPECT_EQ(cls2, 2);
    
    fatx_context::set(nullptr);
    delete ctx;
}

TEST_F(ContextTest, Context_ClusterArithmetic_cls2ptr_Multiple) {
    int argc = 1;
    const char* argv[] = {"test"};
    frontend mmi(argc, argv);
    mmi.input = test_file;
    mmi.table = "";
    mmi.diffile = "";
    
    auto* ctx = new fatx_context(mmi);
    fatx_context::set(ctx);
    
    fatx_context::get()->par.clus_pow = 12;
    fatx_context::get()->par.clus_size = 4096;
    fatx_context::get()->par.root_start = 0x10000;
    fatx_context::get()->par.root_clus = 1;
    fatx_context::get()->par.clus_fat = 0x100000;
    
    // Test multiple conversions
    clusptr cls1 = 1;
    streamptr ptr1 = clsarithm::cls2ptr(cls1);
    EXPECT_EQ(ptr1, 0x10000);
    
    clusptr cls2 = 2;
    streamptr ptr2 = clsarithm::cls2ptr(cls2);
    EXPECT_EQ(ptr2, 0x11000);
    
    fatx_context::set(nullptr);
    delete ctx;
}

TEST_F(ContextTest, Context_ClusterArithmetic_Out_Of_Bounds) {
    int argc = 1;
    const char* argv[] = {"test"};
    frontend mmi(argc, argv);
    mmi.input = test_file;
    mmi.table = "";
    mmi.diffile = "";
    
    auto* ctx = new fatx_context(mmi);
    fatx_context::set(ctx);
    
    fatx_context::get()->par.clus_pow = 12;
    fatx_context::get()->par.clus_size = 4096;
    fatx_context::get()->par.root_start = 0x10000;
    fatx_context::get()->par.root_clus = 1;
    fatx_context::get()->par.clus_fat = 100;
    
    // Test out of bounds
    clusptr out_of_bounds = 101;
    streamptr ptr = clsarithm::cls2ptr(out_of_bounds);
    EXPECT_EQ(ptr, 0);
    
    fatx_context::set(nullptr);
    delete ctx;
}

TEST_F(ContextTest, Context_Multiple_Set_And_Get) {
    int argc = 1;
    const char* argv[] = {"test"};
    frontend mmi(argc, argv);
    mmi.input = test_file;
    mmi.table = "";
    mmi.diffile = "";
    
    fatx_context *ctx1 = new fatx_context(mmi);
    fatx_context::set(ctx1);
    EXPECT_EQ(fatx_context::get(), ctx1);
    
    fatx_context::set(nullptr);
    EXPECT_EQ(fatx_context::get(), nullptr);
    
    delete ctx1;
}
