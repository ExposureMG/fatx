#include <gtest/gtest.h>
#include <fstream>
#include <stdexcept>
#include <unistd.h>
#include <sys/statvfs.h>
#include <sys/stat.h>
#include "context.hpp"
#include "fuse_ops.hpp"

// Tests pour les opérations FUSE
class FUSEOpsTest : public ::testing::Test {
protected:
	void SetUp() override { }
	void TearDown() override { }
};

// Test d'initialisation des callbacks FUSE (smoke test)
TEST_F(FUSEOpsTest, FUSE_Ops_Initialization) {
	fatx_ops_init();
	EXPECT_TRUE(true);
}

// Fixture avec contexte FATX complet (fat + root)
class FUSEOpsContextTest : public ::testing::Test {
protected:
	std::string test_file;
	frontend* tf;
	fatx_context* ctx;

	void SetUp() override {
		char tmpl[] = "/tmp/fatx_fuse_XXXXXX";
		int fd = mkstemp(tmpl);
		if (fd == -1) throw std::runtime_error("mkstemp failed");
		close(fd);
		test_file = tmpl;

		// Créer un fichier XTAF 2MB
		std::ofstream ofs(test_file, std::ios::binary | std::ios::out);
		const std::size_t size = 0x200000;
		ofs.seekp(size - 1); char zero = '\0'; ofs.write(&zero, 1);
		ofs.seekp(0); ofs.write("XTAF", 4);
		uint32_t id = 0, spc = 1, root = 1;
		ofs.write(reinterpret_cast<char*>(&id), 4);
		ofs.write(reinterpret_cast<char*>(&spc), 4);
		ofs.write(reinterpret_cast<char*>(&root), 4);
		uint16_t fat_eoc = 0xFFFF;
		ofs.seekp(0x1000 + 2); ofs.write(reinterpret_cast<char*>(&fat_eoc), 2);
		ofs.close();

		int tac = 1;
		const char* tav[] = {"test"};
		tf = new frontend(tac, tav);
		ctx = new fatx_context(*tf);
		fatx_context::set(ctx);
		ctx->mmi.input = test_file;
		ctx->mmi.table = "file";
		ctx->mmi.partition = "x2";
		ctx->mmi.prog = frontend::fsck;
		ctx->mmi.force_a = true;
		ctx->mmi.force_y = true;

		int res = ctx->setup();
		ASSERT_EQ(res, 0);
	}

	void TearDown() override {
		if (ctx) {
			ctx->destroy();
			delete ctx;
			fatx_context::set(nullptr);
		}
		if (tf) delete tf;
		if (!test_file.empty()) unlink(test_file.c_str());
	}
};

// ------ fatx_getattr ------
TEST_F(FUSEOpsContextTest, GetAttr_NotFound) {
	struct stat st = {};
	EXPECT_EQ(fatx_getattr("/notfound", &st, nullptr), -ENOENT);
}

TEST_F(FUSEOpsContextTest, GetAttr_Root) {
	struct stat st = {};
	int res = fatx_getattr("/", &st, nullptr);
	EXPECT_EQ(res, 0);
	EXPECT_NE(st.st_mode, 0u);
	EXPECT_EQ(st.st_uid, ctx->mmi.uid);
	EXPECT_EQ(st.st_gid, ctx->mmi.gid);
}

// ------ fatx_statfs ------
TEST_F(FUSEOpsContextTest, Statfs_Root) {
	struct statvfs sfs = {};
	EXPECT_EQ(fatx_statfs("/", &sfs), 0);
	EXPECT_EQ(sfs.f_bsize, ctx->par.clus_size);
	EXPECT_EQ(sfs.f_namemax, static_cast<unsigned long>(name_size));
}

TEST_F(FUSEOpsContextTest, Statfs_ReadOnly_Flag) {
	ctx->mmi.set_readonly(true);
	struct statvfs sfs = {};
	EXPECT_EQ(fatx_statfs("/", &sfs), 0);
	EXPECT_TRUE(sfs.f_flag & ST_RDONLY);
}

// ------ fatx_open ------
TEST_F(FUSEOpsContextTest, Open_NotFound) {
	fuse_file_info fi = {};
	EXPECT_EQ(fatx_open("/notfound", &fi), -ENOENT);
}

TEST_F(FUSEOpsContextTest, Open_ReadOnly_WriteAttempt) {
	ctx->mmi.set_readonly(true);
	fuse_file_info fi = {};
	fi.flags = O_WRONLY;
	// root "/" won't be found by open (root cluster != 0) but root should return EROFS
	// Actually fatx_open finds root for "/", let's use a non-existent path
	// But "/" WOULD be found...  Let's test another way: if found and writeable=false
	// We need a valid file. Since we can't create files easily, use a dummy non-file path
	// that fails because not writeable:
	// Actually root exists, so for "/" with O_WRONLY and readonly=true → -EROFS
	EXPECT_EQ(fatx_open("/", &fi), -EROFS);
}

// ------ fatx_read ------
TEST_F(FUSEOpsContextTest, Read_NotFound) {
	fuse_file_info fi = {};
	fi.fh = 0;  // mismatch
	char buf[64] = {};
	EXPECT_EQ(fatx_read("/notfound", buf, 64, 0, &fi), -ENOENT);
}

// ------ fatx_write ------
TEST_F(FUSEOpsContextTest, Write_NotFound) {
	fuse_file_info fi = {};
	fi.fh = 0;
	const char buf[64] = {};
	EXPECT_EQ(fatx_write("/notfound", buf, 64, 0, &fi), -ENOENT);
}

TEST_F(FUSEOpsContextTest, Write_ReadOnly) {
	ctx->mmi.set_readonly(true);
	fuse_file_info fi = {};
	fi.fh = reinterpret_cast<uint64_t>(ctx->root);
	const char buf[64] = {};
	// root found but writeable=false → -EROFS
	EXPECT_EQ(fatx_write("/", buf, 64, 0, &fi), -EROFS);
}

// ------ fatx_flush ------
TEST_F(FUSEOpsContextTest, Flush_NotFound) {
	fuse_file_info fi = {};
	fi.fh = 0;
	EXPECT_EQ(fatx_flush("/notfound", &fi), -ENOENT);
}

// ------ fatx_close ------
TEST_F(FUSEOpsContextTest, Close_NotFound) {
	fuse_file_info fi = {};
	fi.fh = 0;
	EXPECT_EQ(fatx_close("/notfound", &fi), -ENOENT);
}

// ------ fatx_readdir ------
TEST_F(FUSEOpsContextTest, ReadDir_NotFound) {
	fuse_file_info fi = {};
	auto ff = [](void*, const char*, const struct stat*, off_t, enum fuse_fill_dir_flags) -> int { return 0; };
	EXPECT_EQ(fatx_readdir("/notfound", nullptr, ff, 0, &fi, FUSE_READDIR_PLUS), -ENOENT);
}

TEST_F(FUSEOpsContextTest, ReadDir_Root) {
	fuse_file_info fi = {};
	auto ff = [](void*, const char*, const struct stat*, off_t, enum fuse_fill_dir_flags) -> int { return 0; };
	int res = fatx_readdir("/", nullptr, ff, 0, &fi, FUSE_READDIR_PLUS);
	EXPECT_EQ(res, 0);
}

// ------ fatx_create (mkdir) ------
TEST_F(FUSEOpsContextTest, Create_ReadOnly) {
	ctx->mmi.set_readonly(true);
	EXPECT_EQ(fatx_create("/testdir/", S_IFDIR | 0755), -EACCES);
}

TEST_F(FUSEOpsContextTest, Create_InvalidName) {
	EXPECT_EQ(fatx_create("/valid/inval id", S_IFREG | 0644), -EINVAL);  // EINVAL: espace dans le nom invalide pour FATX
}

TEST_F(FUSEOpsContextTest, Create_RootSuffix) {
	// Path ending in "/" with no name → ENOENT
	EXPECT_EQ(fatx_create("/", S_IFDIR | 0755), -ENOENT);
}

// ------ fatx_remove ------
TEST_F(FUSEOpsContextTest, Remove_NotFound) {
	EXPECT_EQ(fatx_remove("/notfound"), -ENOENT);
}

TEST_F(FUSEOpsContextTest, Remove_Root_Busy) {
	EXPECT_EQ(fatx_remove("/"), -EBUSY);
}

TEST_F(FUSEOpsContextTest, Remove_ReadOnly) {
	ctx->mmi.set_readonly(true);
	// "/" exists but writeable=false → -EROFS (checked after EBUSY for root)
	// Actually, EBUSY is checked AFTER writeable. Let's check: in fatx_remove:
	// 1. find path
	// 2. nullptr || invalid → ENOENT
	// 3. !writeable → EROFS
	// 4. flags.ro → EACCES
	// 5. == root → EBUSY
	// 6. dir && childs → ENOTEMPTY
	// So "/" with readonly → EROFS (before EBUSY)
	EXPECT_EQ(fatx_remove("/"), -EROFS);
}

// ------ fatx_rename ------
TEST_F(FUSEOpsContextTest, Rename_NotFound) {
	EXPECT_EQ(fatx_rename("/notfound", "/newname", 0), -ENOENT);
}

TEST_F(FUSEOpsContextTest, Rename_ReadOnly) {
	ctx->mmi.set_readonly(true);
	// "/" exists but writeable=false → -EROFS
	EXPECT_EQ(fatx_rename("/", "/newroot", 0), -EROFS);
}

// ------ fatx_chmod ------
TEST_F(FUSEOpsContextTest, Chmod_NotFound) {
	EXPECT_EQ(fatx_chmod("/notfound", 0755, nullptr), -ENOENT);
}

TEST_F(FUSEOpsContextTest, Chmod_ReadOnly) {
	ctx->mmi.set_readonly(true);
	EXPECT_EQ(fatx_chmod("/", 0755, nullptr), -EROFS);
}

// ------ fatx_chown ------
TEST_F(FUSEOpsContextTest, Chown_NotFound) {
	EXPECT_EQ(fatx_chown("/notfound", 0, 0, nullptr), -ENOENT);
}

TEST_F(FUSEOpsContextTest, Chown_ReadOnly) {
	ctx->mmi.set_readonly(true);
	EXPECT_EQ(fatx_chown("/", 0, 0, nullptr), -EROFS);
}

TEST_F(FUSEOpsContextTest, Chown_Root_Ok) {
	// writeable + valid path → 0 (chown doesn't actually do anything)
	EXPECT_EQ(fatx_chown("/", 1000, 1000, nullptr), 0);
}

// ------ fatx_truncate ------
TEST_F(FUSEOpsContextTest, Truncate_NotFound) {
	EXPECT_EQ(fatx_truncate("/notfound", 0, nullptr), -ENOENT);
}

TEST_F(FUSEOpsContextTest, Truncate_ReadOnly) {
	ctx->mmi.set_readonly(true);
	EXPECT_EQ(fatx_truncate("/", 0, nullptr), -EROFS);
}

// ------ fatx_utimens ------
TEST_F(FUSEOpsContextTest, Utimens_NotFound) {
	struct timespec tv[2] = {{0, 0}, {0, 0}};
	EXPECT_EQ(fatx_utimens("/notfound", tv, nullptr), -ENOENT);
}

TEST_F(FUSEOpsContextTest, Utimens_ReadOnly) {
	ctx->mmi.set_readonly(true);
	struct timespec tv[2] = {{0, 0}, {0, 0}};
	EXPECT_EQ(fatx_utimens("/", tv, nullptr), -EROFS);
}

// ------ fatx_init / fatx_ops struct ------
TEST_F(FUSEOpsContextTest, FatxOps_Callbacks_Assigned) {
	fatx_ops_init();
	EXPECT_NE(fatx_ops.getattr, nullptr);
	EXPECT_NE(fatx_ops.read, nullptr);
	EXPECT_NE(fatx_ops.write, nullptr);
	EXPECT_NE(fatx_ops.readdir, nullptr);
	EXPECT_NE(fatx_ops.statfs, nullptr);
	EXPECT_NE(fatx_ops.create, nullptr);
	EXPECT_NE(fatx_ops.unlink, nullptr);
	EXPECT_NE(fatx_ops.rename, nullptr);
	EXPECT_NE(fatx_ops.chmod, nullptr);
	EXPECT_NE(fatx_ops.chown, nullptr);
	EXPECT_NE(fatx_ops.truncate, nullptr);
	EXPECT_NE(fatx_ops.utimens, nullptr);
	EXPECT_NE(fatx_ops.flush, nullptr);
	EXPECT_NE(fatx_ops.open, nullptr);
	EXPECT_NE(fatx_ops.release, nullptr);
	EXPECT_NE(fatx_ops.init, nullptr);
	EXPECT_NE(fatx_ops.destroy, nullptr);
}

// ------ fatx_creope ------
TEST_F(FUSEOpsContextTest, Creope_ReadOnly) {
	ctx->mmi.set_readonly(true);
	fuse_file_info fi = {};
	EXPECT_EQ(fatx_creope("/testfile.txt", S_IFREG | 0644, &fi), -EACCES);
}

// ------ fatx_init (callback) ------
TEST_F(FUSEOpsContextTest, Init_Callback) {
	fuse_conn_info fci = {};
	fci.want = 0;
	EXPECT_NO_THROW(fatx_init(&fci, nullptr));
	EXPECT_TRUE(fci.want & FUSE_CAP_DONT_MASK);
}

// ------ parser() ------
// Script commentaire → skip, aucune exception
TEST_F(FUSEOpsContextTest, Parser_Comment) {
	ctx->mmi.script = "#test comment";
	EXPECT_NO_THROW(ctx->mmi.parser());
}

// help → affiche l'aide
TEST_F(FUSEOpsContextTest, Parser_Help) {
	ctx->mmi.script = "help";
	EXPECT_NO_THROW(ctx->mmi.parser());
}

// Commande inconnue → "unknowncmd:unknown\n"
TEST_F(FUSEOpsContextTest, Parser_UnknownCommand) {
	ctx->mmi.script = "unknowncmd,foo";
	EXPECT_NO_THROW(ctx->mmi.parser());
}

// mkdir read-only → "read-only\n"
TEST_F(FUSEOpsContextTest, Parser_Mkdir_ReadOnly) {
	ctx->mmi.set_readonly(true);
	ctx->mmi.script = "mkdir,/testdir";
	EXPECT_NO_THROW(ctx->mmi.parser());
	ctx->mmi.set_readonly(false);
}

// mkdir avec erreur de syntaxe (slash final) → "syntax error\n"
TEST_F(FUSEOpsContextTest, Parser_Mkdir_SyntaxError) {
	ctx->mmi.script = "mkdir,testfile/";
	EXPECT_NO_THROW(ctx->mmi.parser());
}

// mkdir parent non trouvé → "not found\n"
TEST_F(FUSEOpsContextTest, Parser_Mkdir_NotFound) {
	ctx->mmi.script = "mkdir,/nonexist/subdir";
	EXPECT_NO_THROW(ctx->mmi.parser());
}

// rmdir non trouvé → "not found\n"
TEST_F(FUSEOpsContextTest, Parser_Rmdir_NotFound) {
	ctx->mmi.script = "rmdir,/nonexistent";
	EXPECT_NO_THROW(ctx->mmi.parser());
}

// lsfat non trouvé → "not found\n"
TEST_F(FUSEOpsContextTest, Parser_Lsfat_NotFound) {
	ctx->mmi.script = "lsfat,/nonexistent";
	EXPECT_NO_THROW(ctx->mmi.parser());
}

// rm non trouvé → "not found\n"
TEST_F(FUSEOpsContextTest, Parser_Rm_NotFound) {
	ctx->mmi.script = "rm,/nonexistent";
	EXPECT_NO_THROW(ctx->mmi.parser());
}

// mv non trouvé → "not found\n"
TEST_F(FUSEOpsContextTest, Parser_Mv_NotFound) {
	ctx->mmi.script = "mv,/nonexistent,/newname";
	EXPECT_NO_THROW(ctx->mmi.parser());
}

// cp non trouvé → "not found\n"
TEST_F(FUSEOpsContextTest, Parser_Cp_NotFound) {
	ctx->mmi.script = "cp,/nonexistent,/dest/foo";
	EXPECT_NO_THROW(ctx->mmi.parser());
}

// rmdir read-only → "read-only\n"
TEST_F(FUSEOpsContextTest, Parser_Rmdir_ReadOnly) {
	ctx->mmi.set_readonly(true);
	ctx->mmi.script = "rmdir,/testdir";
	EXPECT_NO_THROW(ctx->mmi.parser());
	ctx->mmi.set_readonly(false);
}

// Scripts multiples séparés par ";" → deux commandes
TEST_F(FUSEOpsContextTest, Parser_MultipleCommands) {
	ctx->mmi.script = "# first comment; # second comment";
	EXPECT_NO_THROW(ctx->mmi.parser());
}

// ------ diskmap edge cases (requires context) ------
// read(FLK) → retourne 0 et affiche erreur
TEST_F(FUSEOpsContextTest, DiskMap_Read_FLK) {
	clusptr r = fatx_context::get()->fat->dskmap::read(FLK);
	EXPECT_EQ(r, 0u);
}

// read(EOC) → retourne 0 et affiche erreur
TEST_F(FUSEOpsContextTest, DiskMap_Read_EOC) {
	clusptr r = fatx_context::get()->fat->dskmap::read(EOC);
	EXPECT_EQ(r, 0u);
}

// read(p > clus_fat) → retourne 0 et affiche erreur out-of-bounds
TEST_F(FUSEOpsContextTest, DiskMap_Read_OutOfBounds) {
	clusptr big = fatx_context::get()->par.clus_fat + 1;
	clusptr r = fatx_context::get()->fat->dskmap::read(big);
	EXPECT_EQ(r, 0u);
}

// write(FLK, 0) → retourne EOVERFLOW
TEST_F(FUSEOpsContextTest, DiskMap_Write_FLK) {
	EXPECT_NE(fatx_context::get()->fat->dskmap::write(FLK, 0), 0);
}

// write(EOC, 0) → retourne EOVERFLOW
TEST_F(FUSEOpsContextTest, DiskMap_Write_EOC) {
	EXPECT_NE(fatx_context::get()->fat->dskmap::write(EOC, 0), 0);
}

// write(p > clus_fat, 0) → retourne EOVERFLOW
TEST_F(FUSEOpsContextTest, DiskMap_Write_POutOfBounds) {
	clusptr big = fatx_context::get()->par.clus_fat + 1;
	EXPECT_NE(fatx_context::get()->fat->dskmap::write(big, 0), 0);
}

// write(1, v > clus_fat) → retourne EOVERFLOW
TEST_F(FUSEOpsContextTest, DiskMap_Write_VOutOfBounds) {
	clusptr big = fatx_context::get()->par.clus_fat + 1;
	EXPECT_NE(fatx_context::get()->fat->dskmap::write(1u, big), 0);
}

// allocfat(0, 0) → retour immédiat (s == 0)
TEST_F(FUSEOpsContextTest, DiskMap_AllocFat_Zero) {
	vareas r = fatx_context::get()->fat->allocfat(0, 0);
	EXPECT_TRUE(r.empty());
}

// ------ Création / suppression de fichiers et répertoires ------
// Ces tests couvrent addtodir(), new entry constructor, allocfat(),
// gapcheck()/forfat(), freefat(), remfrdir(), resize() dans entry.cpp et diskmap.cpp

// Créer un fichier régulier → couvre new entry ctor (size=0, cluster=FLK),
// addtodir() success path, et allocfat(0) early return
TEST_F(FUSEOpsContextTest, Create_File_Root_Success) {
	ctx->mmi.prog = frontend::fuse;
	EXPECT_EQ(fatx_create("/testfile.txt", S_IFREG | 0644), 0);
	struct stat st = {};
	EXPECT_EQ(fatx_getattr("/testfile.txt", &st, nullptr), 0);
	EXPECT_TRUE(st.st_mode & S_IFREG);
}

// Créer un répertoire → couvre allocfat(1), gapcheck(), forfat()
TEST_F(FUSEOpsContextTest, Create_Dir_Root_Success) {
	ctx->mmi.prog = frontend::fuse;
	EXPECT_EQ(fatx_create("/testdir", S_IFDIR | 0755), 0);
	struct stat st = {};
	EXPECT_EQ(fatx_getattr("/testdir", &st, nullptr), 0);
	EXPECT_TRUE(st.st_mode & S_IFDIR);
}

// Créer un fichier puis le supprimer → couvre remfrdir() avec cluster=FLK (delnodata)
TEST_F(FUSEOpsContextTest, Create_Then_Remove_File) {
	ctx->mmi.prog = frontend::fuse;
	ASSERT_EQ(fatx_create("/delme.txt", S_IFREG | 0644), 0);
	EXPECT_EQ(fatx_remove("/delme.txt"), 0);
	struct stat st = {};
	EXPECT_EQ(fatx_getattr("/delme.txt", &st, nullptr), -ENOENT);
}

// Créer un répertoire puis le supprimer → couvre remfrdir() + freefat() gap management
TEST_F(FUSEOpsContextTest, Create_Then_Remove_Dir) {
	ctx->mmi.prog = frontend::fuse;  // fsck early-returns freefat gap management
	ASSERT_EQ(fatx_create("/deldir", S_IFDIR | 0755), 0);
	EXPECT_EQ(fatx_remove("/deldir"), 0);
	struct stat st = {};
	EXPECT_EQ(fatx_getattr("/deldir", &st, nullptr), -ENOENT);
}

// Créer le même fichier S_IFREG deux fois → couvre le chemin "exists + S_IFREG → remove + recreate"
TEST_F(FUSEOpsContextTest, Create_Existing_File_Overwrites) {
	ctx->mmi.prog = frontend::fuse;
	ASSERT_EQ(fatx_create("/rewrite.txt", S_IFREG | 0644), 0);
	EXPECT_EQ(fatx_create("/rewrite.txt", S_IFREG | 0644), 0);
}

// Essayer de créer un répertoire avec un nom déjà existant → EEXIST
TEST_F(FUSEOpsContextTest, Create_Existing_Dir_Returns_EEXIST) {
	ctx->mmi.prog = frontend::fuse;
	ASSERT_EQ(fatx_create("/existdir", S_IFDIR | 0755), 0);
	// Tenter de créer un autre répertoire avec le même nom → EEXIST
	EXPECT_EQ(fatx_create("/existdir", S_IFDIR | 0755), -EEXIST);
}

// Tronquer un fichier 0-byte à une taille > 0 → couvre resize() + allocfat(n) + resizefat()
TEST_F(FUSEOpsContextTest, Truncate_File_Grow) {
	ctx->mmi.prog = frontend::fuse;
	ASSERT_EQ(fatx_create("/growme.txt", S_IFREG | 0644), 0);
	// Grandir le fichier de 0 à 100 octets → resize(100) → allocfat(1 cluster)
	EXPECT_EQ(fatx_truncate("/growme.txt", 100, nullptr), 0);
	struct stat st = {};
	EXPECT_EQ(fatx_getattr("/growme.txt", &st, nullptr), 0);
	EXPECT_EQ(st.st_size, 100);
}

// Tronquer un fichier non-nul à zéro → couvre resize(0) → freefat()
TEST_F(FUSEOpsContextTest, Truncate_File_Shrink_To_Zero) {
	ctx->mmi.prog = frontend::fuse;
	ASSERT_EQ(fatx_create("/shrinkme.txt", S_IFREG | 0644), 0);
	ASSERT_EQ(fatx_truncate("/shrinkme.txt", 100, nullptr), 0);
	EXPECT_EQ(fatx_truncate("/shrinkme.txt", 0, nullptr), 0);
	struct stat st = {};
	EXPECT_EQ(fatx_getattr("/shrinkme.txt", &st, nullptr), 0);
	EXPECT_EQ(st.st_size, 0);
}

// Renommer un fichier → couvre entry::rename()
TEST_F(FUSEOpsContextTest, Rename_File_Success) {
	ctx->mmi.prog = frontend::fuse;
	ASSERT_EQ(fatx_create("/oldname.txt", S_IFREG | 0644), 0);
	EXPECT_EQ(fatx_rename("/oldname.txt", "/newname.txt", 0), 0);
	struct stat st = {};
	EXPECT_EQ(fatx_getattr("/newname.txt", &st, nullptr), 0);
	EXPECT_EQ(fatx_getattr("/oldname.txt", &st, nullptr), -ENOENT);
}

// Utimens sur fichier créé → couvre utimens success path
TEST_F(FUSEOpsContextTest, Utimens_File_Success) {
	ctx->mmi.prog = frontend::fuse;
	ASSERT_EQ(fatx_create("/timefile.txt", S_IFREG | 0644), 0);
	struct timespec tv[2] = {{1700000000, 0}, {1700000000, 0}};
	EXPECT_EQ(fatx_utimens("/timefile.txt", tv, nullptr), 0);
}

// GetAttr sur fichier créé → couvre getattr pour fichier non-répertoire
TEST_F(FUSEOpsContextTest, GetAttr_RegularFile) {
	ctx->mmi.prog = frontend::fuse;
	ASSERT_EQ(fatx_create("/attrfile.txt", S_IFREG | 0644), 0);
	struct stat st = {};
	EXPECT_EQ(fatx_getattr("/attrfile.txt", &st, nullptr), 0);
	EXPECT_EQ(st.st_size, 0);
}

// ReadDir sur répertoire créé → couvre readdir pour répertoire non-root
TEST_F(FUSEOpsContextTest, ReadDir_CreatedDir) {
	ctx->mmi.prog = frontend::fuse;
	ASSERT_EQ(fatx_create("/subdir", S_IFDIR | 0755), 0);
	fuse_file_info fi = {};
	auto ff = [](void*, const char*, const struct stat*, off_t, fuse_fill_dir_flags) -> int { return 0; };
	EXPECT_EQ(fatx_readdir("/subdir", nullptr, ff, 0, &fi, FUSE_READDIR_PLUS), 0);
}

// Open + Write + Read + Close → couvre bufwrite, bufread, flush, resize(grow)
// de entry.cpp (L914-927, L968-1011, L1013-1075, L1077-1100, L1102-1121 open, L1123-1157 close)
TEST_F(FUSEOpsContextTest, Open_Write_Read_Close) {
	ctx->mmi.prog = frontend::fuse;
	ASSERT_EQ(fatx_create("/rwfile.txt", S_IFREG | 0644), 0);

	fuse_file_info fi = {};
	fi.flags = O_RDWR;
	ASSERT_EQ(fatx_open("/rwfile.txt", &fi), 0);

	// Écriture : couvre bufwrite + resize(grow) + data(write path)
	const char wbuf[] = "Hello FATX World!";
	int nw = fatx_write("/rwfile.txt", wbuf, sizeof(wbuf) - 1, 0, &fi);
	EXPECT_EQ(nw, static_cast<int>(sizeof(wbuf) - 1));

	// Flush (close en écriture) : couvre flush(entbuf->touched)
	EXPECT_EQ(fatx_flush("/rwfile.txt", &fi), 0);

	// Lecture : couvre bufread + data(read path)
	char rbuf[32] = {};
	int nr = fatx_read("/rwfile.txt", rbuf, sizeof(rbuf), 0, &fi);
	EXPECT_GT(nr, 0);

	// Fermeture : couvre close(write=true)
	fatx_close("/rwfile.txt", &fi);
}

// Open (read-only) + Read + Close → couvre close(write=false), cptacc, exclusive.release
TEST_F(FUSEOpsContextTest, Open_Read_Close_ReadOnly) {
	ctx->mmi.prog = frontend::fuse;
	ASSERT_EQ(fatx_create("/rofile.txt", S_IFREG | 0644), 0);

	// Ouvrir en écriture pour créer du contenu
	{
		fuse_file_info fiw = {};
		fiw.flags = O_WRONLY;
		ASSERT_EQ(fatx_open("/rofile.txt", &fiw), 0);
		const char wdata[] = "FATXDATA";
		void(fatx_write("/rofile.txt", wdata, sizeof(wdata) - 1, 0, &fiw));
		fatx_close("/rofile.txt", &fiw);
	}

	// Ouvrir en lecture seule
	fuse_file_info fir = {};
	fir.flags = O_RDONLY;
	ASSERT_EQ(fatx_open("/rofile.txt", &fir), 0);
	char rbuf[16] = {};
	int nr = fatx_read("/rofile.txt", rbuf, sizeof(rbuf), 0, &fir);
	EXPECT_GT(nr, 0);
	fatx_close("/rofile.txt", &fir);
}

// Rename vers un sous-répertoire → couvre entry::rename cas dir.size() != 0 + std::move
// (L535-573 dans entry.cpp)
TEST_F(FUSEOpsContextTest, Rename_File_To_Subdir) {
	ctx->mmi.prog = frontend::fuse;
	// Créer sous-répertoire /mydir
	ASSERT_EQ(fatx_create("/mydir", S_IFDIR | 0755), 0);
	// Créer fichier /srcfile.txt
	ASSERT_EQ(fatx_create("/srcfile.txt", S_IFREG | 0644), 0);
	// Renommer /srcfile.txt → /mydir/srcfile.txt (cross-directory move)
	EXPECT_EQ(fatx_rename("/srcfile.txt", "/mydir/srcfile.txt", 0), 0);
	// Vérifier l'ancienne entrée n'existe plus
	struct stat st = {};
	EXPECT_EQ(fatx_getattr("/srcfile.txt", &st, nullptr), -ENOENT);
	// Vérifier la nouvelle entrée existe
	EXPECT_EQ(fatx_getattr("/mydir/srcfile.txt", &st, nullptr), 0);
}

// Rename de fichier existant sur un autre existant → remfrdir(dst) → L532-533
TEST_F(FUSEOpsContextTest, Rename_Overwrites_Existing_File) {
	ctx->mmi.prog = frontend::fuse;
	ASSERT_EQ(fatx_create("/file1.txt", S_IFREG | 0644), 0);
	ASSERT_EQ(fatx_create("/file2.txt", S_IFREG | 0644), 0);
	// Renommer file1 → file2 : dst != nullptr → remfrdir(dst) → L533
	EXPECT_EQ(fatx_rename("/file1.txt", "/file2.txt", 0), 0);
	struct stat st = {};
	EXPECT_EQ(fatx_getattr("/file2.txt", &st, nullptr), 0);
	EXPECT_EQ(fatx_getattr("/file1.txt", &st, nullptr), -ENOENT);
}

// Chmod sur  fichier existant → couvre fatx_chmod success et fatx_chown
TEST_F(FUSEOpsContextTest, Chmod_And_Chown_File) {
	ctx->mmi.prog = frontend::fuse;
	ASSERT_EQ(fatx_create("/chmodfile.txt", S_IFREG | 0644), 0);
	EXPECT_EQ(fatx_chmod("/chmodfile.txt", 0755, nullptr), 0);
	EXPECT_EQ(fatx_chown("/chmodfile.txt", 1000, 1000, nullptr), 0);
}

// fatx_truncate sur fichier read-only → -EACCES (L261 fuse_ops.cpp)
TEST_F(FUSEOpsContextTest, Truncate_ReadOnlyFile) {
	ctx->mmi.prog = frontend::fuse;
	ASSERT_EQ(fatx_create("/rotrunc.txt", S_IFREG | 0444), 0);
	entry* f = ctx->root->find("/rotrunc.txt");
	ASSERT_NE(f, nullptr);
	f->flags.ro = true;  // marquer l'entrée comme read-only
	EXPECT_EQ(fatx_truncate("/rotrunc.txt", 100, nullptr), -EACCES);
}

// fatx_open write-attempt sur filesystem readonly → -EROFS (L35 fuse_ops.cpp)
TEST_F(FUSEOpsContextTest, Open_Write_ReadOnly_Fs) {
	ctx->mmi.prog = frontend::fuse;
	ASSERT_EQ(fatx_create("/openro.txt", S_IFREG | 0644), 0);
	ctx->mmi.set_readonly(true);
	fuse_file_info fi = {};
	fi.flags = O_WRONLY;
	EXPECT_EQ(fatx_open("/openro.txt", &fi), -EROFS);
}

// fatx_remove sur fichier read-only (flags.ro) → -EACCES (L121)
TEST_F(FUSEOpsContextTest, Remove_ReadOnlyFile) {
	ctx->mmi.prog = frontend::fuse;
	ASSERT_EQ(fatx_create("/rodel.txt", S_IFREG | 0444), 0);
	entry* f = ctx->root->find("/rodel.txt");
	ASSERT_NE(f, nullptr);
	f->flags.ro = true;
	EXPECT_EQ(fatx_remove("/rodel.txt"), -EACCES);
}
