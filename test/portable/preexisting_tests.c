/* 
 *
 */

// ---- Test framework ---- //
#include "unity.h"

// ---- Code to be tested ---- //
#include "ramfs/vfs.h"

// ---- Additional test fixtures ---- //

// ---- esp-idf components & system headers ---- //
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <errno.h>
#include <sys/fcntl.h>


#include "esp_err.h"
#include "esp_check.h"
#include "esp_log.h"

#include "esp_system.h"  // Allows measuring currently available memory

// ---- Local files (this component) ---- //

// ---- End of includes ---- //

// Tag prepended to ESP_LOG messages in this component: can also be used to filter messages at runtime
__attribute__((unused))
static const char* TAG = "test_ramfs";

__attribute__((unused))
static const char* SANITY = "sanity check for test-fixture";        //!< Marks tests which are to ensure the test-fixture is working correctly (not TDD)

// ---- private types ---- //

// ---- private Global variables ---- //


// ---- Forward-declared static functions ---- //

// ---- Cleanup functions ---- //

// This will run before each test!
static void LOCAL_TEST_setUp(void)
{
	errno = 0;
}

// This will run after each test!
static void LOCAL_TEST_tearDown(void)
{
	;
}


// ---- Tests ---- //



TEST_CASE("init", "[esp_ramfs]")
{
    LOCAL_TEST_setUp();

	ramfs_fs_t *fs;

	fs = ramfs_init();
	TEST_ASSERT_NOT_NULL(fs);
	
    LOCAL_TEST_tearDown();
}

TEST_CASE("deinit", "[esp_ramfs]")
{
    LOCAL_TEST_setUp();

	ramfs_fs_t *fs;

	fs = ramfs_init();
	TEST_ASSERT_NOT_NULL(fs);

	ramfs_deinit(fs);
	fs = NULL;
	
    LOCAL_TEST_tearDown();
}

TEST_CASE("create", "[esp_ramfs]")
{
    LOCAL_TEST_setUp();

	ramfs_fs_t *fs;
	ramfs_entry_t *file;

	fs = ramfs_init();
	TEST_ASSERT_NOT_NULL(fs);

	file = ramfs_create(fs, "test", 0);
	TEST_ASSERT_NOT_NULL(file);

	ramfs_deinit(fs);
	fs = NULL;
	
    LOCAL_TEST_tearDown();
}



TEST_CASE("unlink", "[esp_ramfs]")
{
    LOCAL_TEST_setUp();

	ramfs_fs_t *fs;
	ramfs_entry_t *file;

	fs = ramfs_init();
	TEST_ASSERT_NOT_NULL(fs);

	file = ramfs_create(fs, "test", 0);
	TEST_ASSERT_NOT_NULL(file);

	TEST_ASSERT_EQUAL(0, ramfs_unlink(file));

	ramfs_deinit(fs);
	fs = NULL;
	
    LOCAL_TEST_tearDown();
}

TEST_CASE("open", "[esp_ramfs]")
{
    LOCAL_TEST_setUp();

	ramfs_fs_t *fs;
	ramfs_entry_t *file;
	ramfs_fh_t *fh;

	fs = ramfs_init();
	TEST_ASSERT_NOT_NULL(fs);

	file = ramfs_create(fs, "test", 0);
	TEST_ASSERT_NOT_NULL(file);

	fh = ramfs_open(fs, file, 0);
	TEST_ASSERT_NOT_NULL(fh);

	ramfs_close(fh);

	ramfs_deinit(fs);
	fs = NULL;
	
    LOCAL_TEST_tearDown();
}

TEST_CASE("read", "[esp_ramfs]")
{
    LOCAL_TEST_setUp();

	ramfs_fs_t *fs;
	ramfs_entry_t *file;
	ramfs_fh_t *fh;
	char buf[12];

	fs = ramfs_init();
	TEST_ASSERT_NOT_NULL(fs);

	file = ramfs_create(fs, "test", 0);
	TEST_ASSERT_NOT_NULL(file);

	fh = ramfs_open(fs, file, O_WRONLY);
	TEST_ASSERT_NOT_NULL(fh);

	TEST_ASSERT_EQUAL(12, ramfs_write(fh, "Hello World!", 12));

	ramfs_close(fh);

	fh = ramfs_open(fs, file, O_RDONLY);
	TEST_ASSERT_NOT_NULL(fh);

	TEST_ASSERT_EQUAL(12, ramfs_read(fh, buf, 12));

	TEST_ASSERT_EQUAL(0, memcmp(buf, "Hello World!", 12));

	ramfs_close(fh);

	ramfs_deinit(fs);
	fs = NULL;	
	
    LOCAL_TEST_tearDown();
}

TEST_CASE("write", "[esp_ramfs]")
{
    LOCAL_TEST_setUp();

	ramfs_fs_t *fs;
	ramfs_entry_t *file;
	ramfs_fh_t *fh;
	char buf[12];

	fs = ramfs_init();
	TEST_ASSERT_NOT_NULL(fs);

	file = ramfs_create(fs, "test", 0);
	TEST_ASSERT_NOT_NULL(file);

	fh = ramfs_open(fs, file, O_WRONLY);
	TEST_ASSERT_NOT_NULL(fh);

	TEST_ASSERT_EQUAL(12, ramfs_write(fh, "Hello World!", 12));

	ramfs_close(fh);

	fh = ramfs_open(fs, file, O_RDONLY);
	TEST_ASSERT_NOT_NULL(fh);

	TEST_ASSERT_EQUAL(12, ramfs_read(fh, buf, 12));

	TEST_ASSERT_EQUAL(0, memcmp(buf, "Hello World!", 12));

	ramfs_close(fh);

	ramfs_deinit(fs);
	fs = NULL;
	
    LOCAL_TEST_tearDown();
}

TEST_CASE("seek", "[esp_ramfs]")
{
    LOCAL_TEST_setUp();

	ramfs_fs_t *fs;
	ramfs_entry_t *file;
	ramfs_fh_t *fh;
	char buf[12];

	fs = ramfs_init();
	TEST_ASSERT_NOT_NULL(fs);

	file = ramfs_create(fs, "test", 0);
	TEST_ASSERT_NOT_NULL(file);

	fh = ramfs_open(fs, file, O_RDWR);
	TEST_ASSERT_NOT_NULL(fh);

	TEST_ASSERT_EQUAL(12, ramfs_write(fh, "Hello World!", 12));

	TEST_ASSERT_EQUAL(6, ramfs_seek(fh, -6, SEEK_CUR));

	TEST_ASSERT_EQUAL(5, ramfs_write(fh, "There", 5));

	TEST_ASSERT_EQUAL(0, ramfs_seek(fh, 0, SEEK_SET));

	TEST_ASSERT_EQUAL(12, ramfs_read(fh, buf, 12));

	TEST_ASSERT_EQUAL(0, memcmp("Hello There!", buf, 12));

	ramfs_close(fh);

	ramfs_deinit(fs);
	fs = NULL;	
	
    LOCAL_TEST_tearDown();
}

TEST_CASE("mkdir", "[esp_ramfs]")
{
    LOCAL_TEST_setUp();

	ramfs_fs_t *fs;
	ramfs_entry_t *dir;

	fs = ramfs_init();
	TEST_ASSERT_NOT_NULL(fs);

	dir = ramfs_mkdir(fs, "test");
	TEST_ASSERT_NOT_NULL(dir);

	ramfs_deinit(fs);
	fs = NULL;
	
    LOCAL_TEST_tearDown();
}

TEST_CASE("rmdir", "[esp_ramfs]")
{
    LOCAL_TEST_setUp();

	ramfs_fs_t *fs;
	ramfs_entry_t *dir;

	fs = ramfs_init();
	TEST_ASSERT_NOT_NULL(fs);

	dir = ramfs_mkdir(fs, "test");
	TEST_ASSERT_NOT_NULL(dir);

	TEST_ASSERT_EQUAL(0, ramfs_rmdir(dir));

	ramfs_deinit(fs);
	fs = NULL;
	
    LOCAL_TEST_tearDown();
}

TEST_CASE("issue 1", "[esp_ramfs]")
{
    LOCAL_TEST_setUp();

	ramfs_fs_t *fs;
	ramfs_entry_t *dir, *file;
	ramfs_fh_t *fh;
	char buf[25];

	fs = ramfs_init();
	TEST_ASSERT_NOT_NULL(fs);

	dir = ramfs_mkdir(fs, "dir");
	TEST_ASSERT_NOT_NULL(dir);

	file = ramfs_create(fs, "dir/test_file.txt", 0);
	TEST_ASSERT_NOT_NULL(file);

	fh = ramfs_open(fs, file, O_WRONLY);
	TEST_ASSERT_NOT_NULL(fh);

	TEST_ASSERT_EQUAL(25, ramfs_write(fh, "This is dir/test_file.txt", 25));

	ramfs_close(fh);

	TEST_ASSERT_EQUAL(0, ramfs_rename(fs, "dir/test_file.txt", "dir/test_file_new.txt"));

	file = ramfs_get_entry(fs, "dir/test_file_new.txt");
	TEST_ASSERT_NOT_NULL(file);

	fh = ramfs_open(fs, file, O_RDONLY);
	TEST_ASSERT_NOT_NULL(fh);

	TEST_ASSERT_EQUAL(25, ramfs_read(fh, buf, 25));

	TEST_ASSERT_EQUAL(0, memcmp(buf, "This is dir/test_file.txt", 25));

	ramfs_close(fh);

	file = ramfs_get_entry(fs, "dir/test_file.txt");
	TEST_ASSERT_NULL(file);

	file = ramfs_get_entry(fs, "dir/test_file_new.txt");
	TEST_ASSERT_NOT_NULL(file);
	TEST_ASSERT_EQUAL(0, ramfs_unlink(file));

	file = ramfs_get_entry(fs, "dir/test_file_new.txt");
	TEST_ASSERT_NULL(file);

	ramfs_deinit(fs);
	fs = NULL;
	
    LOCAL_TEST_tearDown();
}
