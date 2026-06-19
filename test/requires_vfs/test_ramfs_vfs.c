/* test harness for ramfs.
 *
 * Mostly calls ramfs/vfs.h. Indirectly covers vfs.c, ramfs.c
 *
 * Note: not developed with TDD; rather, this was an existing library, but when using it I found
 * that the mkdir() function wasn't working! So, this test harness was written to help me
 * debug and fix it. That's why almost all the tests are about directories.
 *
 * Note: Assume every single TEST_ASSERT statements has a purpose. All tests* have, at some point,
 * failed and required code to 'fix' (i.e. implement the correct functionality). Exception is tests
 * marked SANITY - these are either copies of stuff already tested in a previous TEST_CASE, or they
 * were added to aid in debugging.
 */

// ---- Test framework ---- //
#include "freertos/projdefs.h"
#include "unity.h"

// ---- Code to be tested ---- //
#include "ramfs/vfs.h"

// ---- Additional test fixtures ---- //

// ---- esp-idf components & system headers ---- //
//#include <sys/param.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/unistd.h>
#include <sys/dirent.h>
#include <sys/syslimits.h>
#include <errno.h>

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

#define MOUNT_POINT "/ramfs"

static ramfs_vfs_conf_t ramfs_vfs_conf = {
    .base_path = MOUNT_POINT,
    .fs = NULL,
    .max_files = 5,
};

// ---- Forward-declared static functions ---- //

// Deliberately avoids using stat()
static bool file_exists(const char* path)
{
	int cache_errno = errno;

    FILE* fd = fopen(path, "r");
    if (NULL == fd) {
		errno = cache_errno;
		return false;
	}
	fclose(fd);

	errno = cache_errno;
	return true;
}

// Deliberately avoids using stat()
static bool dir_exists(const char* path)
{
	int cache_errno = errno;

    DIR* dir = opendir(path);
    if (dir == NULL) {
		errno = cache_errno;
        return false;
    }
	closedir(dir);

	errno = cache_errno;
	return true;
}


// ---- Cleanup functions ---- //

// This will run before each test!
static void LOCAL_TEST_setUp(void)
{
	// Failed tests may leave the filesystem initialised
	if (ramfs_vfs_conf.fs != NULL) {
		ESP_ERROR_CHECK(ramfs_vfs_unregister(&ramfs_vfs_conf));
		ramfs_deinit(ramfs_vfs_conf.fs);
		ramfs_vfs_conf.fs = NULL;
	}

	// Create and mount
	ramfs_vfs_conf.fs = ramfs_init();
	assert(ramfs_vfs_conf.fs != NULL);
    ESP_ERROR_CHECK(ramfs_vfs_register(&ramfs_vfs_conf));
	//

    errno = 0;
}

// This will run after each test!
static void LOCAL_TEST_tearDown(void)
{
	// Unmount and destroy
	ESP_ERROR_CHECK(ramfs_vfs_unregister(&ramfs_vfs_conf));
	ramfs_deinit(ramfs_vfs_conf.fs);
	ramfs_vfs_conf.fs = NULL;
}

// NOTE: These tests were written for an existing libray, not originally created with TDD.
// There were quite a few bugs though, so much of the below is TDD and can be relied upon


TEST_CASE("Register and Unregister file system", "[esp_ramfs]")
{
    LOCAL_TEST_setUp();

	// This test was essentially moved into setUp/tearDown

    LOCAL_TEST_tearDown();
}

TEST_CASE("Create a file in the root directory", "[esp_ramfs]")
{
    LOCAL_TEST_setUp();

	const char* filepath = MOUNT_POINT "/file.txt";

	// Shouldn't be there yet
	TEST_ASSERT_FALSE(file_exists(filepath));
	errno = 0;

	// Open for writing, creates the file
	{
	    FILE* fd = fopen(filepath, "w");
	    TEST_ASSERT_NOT_NULL(fd);
	
	    int ret = fclose(fd);
	    TEST_ASSERT_EQUAL(0, ret);
	}

	TEST_ASSERT_TRUE(file_exists(filepath));

    LOCAL_TEST_tearDown();
}


TEST_CASE("Create a directory", "[esp_ramfs]")
{
    LOCAL_TEST_setUp();

	const char* dirpath = MOUNT_POINT "/dir";

	// Shouldn't be there yet
	TEST_ASSERT_FALSE(dir_exists(dirpath));

	// Create the directory
	{
	    int ret = mkdir(dirpath, 0755);
		TEST_ASSERT_EQUAL_MESSAGE(0, ret, SANITY);
		TEST_ASSERT_EQUAL_MESSAGE(0, errno, SANITY);
	}

	TEST_ASSERT_TRUE(dir_exists(dirpath));

    LOCAL_TEST_tearDown();
}

TEST_CASE("opendir() returns non-NULL on empty directories", "[esp_ramfs]")
{
    LOCAL_TEST_setUp();

	const char* dirpath = MOUNT_POINT "/dir";

	// Create the directory
	{
	    int ret = mkdir(dirpath, 0755);
		TEST_ASSERT_EQUAL_MESSAGE(0, ret, SANITY);
		TEST_ASSERT_EQUAL_MESSAGE(0, errno, SANITY);
	}

	{
	    DIR* dir = opendir(dirpath);
	    TEST_ASSERT_NOT_NULL_MESSAGE(dir, strerror(errno));
	    TEST_ASSERT_EQUAL(0, errno);

		int ret = closedir(dir);
	    TEST_ASSERT_EQUAL_MESSAGE(0, ret, strerror(errno));
	    TEST_ASSERT_EQUAL(0, errno);
	}

    LOCAL_TEST_tearDown();
}

// Note: According to POSIX, a path ending in '/' will be interpreted as if it has a trailing '.'
// i.e. '/dir/subdir/' -> '/dir/subdit/.'	This is significant when '/dir/subdir' is a symbolic link
// esp-idf does not support symbolic links, so we can simply reject any calls where a trailing is slash
// is given but it's not a directory.
TEST_CASE("stat() accepts trailing slash for directories", "[esp_ramfs]")
{
    LOCAL_TEST_setUp();

	const char* dirpath = MOUNT_POINT "/dir";
	const char* dirpath_slash = MOUNT_POINT "/dir/";

	// Create the directory
	{
	    int ret = mkdir(dirpath, 0755);
		TEST_ASSERT_EQUAL_MESSAGE(0, ret, SANITY);
		TEST_ASSERT_EQUAL_MESSAGE(0, errno, SANITY);
	}
	
	// Check it works without the slash
	{
		struct stat st;
		int ret = stat(dirpath, &st);
		TEST_ASSERT_EQUAL_MESSAGE(0, ret, SANITY);
		TEST_ASSERT_EQUAL_MESSAGE(0, errno, SANITY);
	}
	
	{
		struct stat st;
		int ret = stat(dirpath_slash, &st);
		TEST_ASSERT_EQUAL(0, ret);
		TEST_ASSERT_EQUAL(0, errno);
	}

    LOCAL_TEST_tearDown();
}

// Note: According to POSIX, a path ending in '/' will be interpreted as if it has a trailing '.'
// i.e. '/dir/subdir/' -> '/dir/subdit/.'	This is significant when '/dir/subdir' is a symbolic link
// esp-idf does not support symbolic links, so we can simply reject any calls where a trailing is slash
// is given but it's not a directory.
TEST_CASE("stat() should not accept trailing slash for files", "[esp_ramfs]")
{
    LOCAL_TEST_setUp();

	const char* dirpath = MOUNT_POINT "/dir";
	const char* filepath = MOUNT_POINT "/dir" "/file";
	const char* filepath_slash = MOUNT_POINT "/dir" "/file/";

	// Create directory and file
	{
	    int ret = mkdir(dirpath, 0755);
		TEST_ASSERT_EQUAL_MESSAGE(0, ret, SANITY);
		TEST_ASSERT_EQUAL_MESSAGE(0, errno, SANITY);

		FILE* fd = fopen(filepath, "a");
		TEST_ASSERT_NOT_NULL_MESSAGE(fd, SANITY);
		ret = fclose(fd);
		TEST_ASSERT_EQUAL_MESSAGE(0, ret, SANITY);
	}
	
	// Check it works without the slash
	{
		struct stat st;
		int ret = stat(dirpath, &st);
		TEST_ASSERT_EQUAL_MESSAGE(0, ret, SANITY);
		TEST_ASSERT_EQUAL_MESSAGE(0, errno, SANITY);
	}
	
	{
		struct stat st;
		int ret = stat(filepath_slash, &st);
		TEST_ASSERT_EQUAL(-1, ret);
		TEST_ASSERT_EQUAL(ENOTDIR, errno);
	}

    LOCAL_TEST_tearDown();
}

// Note: According to POSIX, a path ending in '/' will be interpreted as if it has a trailing '.'
// i.e. '/dir/subdir/' -> '/dir/subdit/.'	This is significant when '/dir/subdir' is a symbolic link
// esp-idf does not support symbolic links, so we can simply reject any calls where a trailing is slash
// is given but it's not a directory.
TEST_CASE("mkdir(), opendir(), and rmdir() accepts trailing slash", "[esp_ramfs]")
{
    LOCAL_TEST_setUp();

	const char* dirpath = MOUNT_POINT "/dir/";

	// Create
	{
	    int ret = mkdir(dirpath, 0755);
		TEST_ASSERT_EQUAL_MESSAGE(0, ret, SANITY);		// Never failed
		TEST_ASSERT_EQUAL_MESSAGE(0, errno, SANITY);	// Never failed
	}

	// Open
	{
	    DIR* dir = opendir(dirpath);
	    TEST_ASSERT_NOT_NULL_MESSAGE(dir, strerror(errno));
	    TEST_ASSERT_EQUAL(0, errno);

		int ret = closedir(dir);
	    TEST_ASSERT_EQUAL_MESSAGE(0, ret, strerror(errno));
	    TEST_ASSERT_EQUAL(0, errno);
	}

	// Remove
	{
		int ret = rmdir(dirpath);
		TEST_ASSERT_EQUAL_MESSAGE(0, ret, SANITY);		// Never failed
		TEST_ASSERT_EQUAL_MESSAGE(0, errno, SANITY);	// Never failed
	}

	// Check it is gone
	TEST_ASSERT_FALSE_MESSAGE(dir_exists(dirpath), SANITY);		// Never failed

    LOCAL_TEST_tearDown();
}

TEST_CASE("Create a file in a directory", "[esp_ramfs]")
{
    LOCAL_TEST_setUp();

	const char* dirpath = MOUNT_POINT "/dir";
	const char* filepath = MOUNT_POINT "/dir" "/file.txt";

	
    mkdir(dirpath, 0755);
	TEST_ASSERT_TRUE_MESSAGE(dir_exists(dirpath), SANITY);

	FILE* fd = fopen(filepath, "w");
	TEST_ASSERT_NOT_NULL_MESSAGE(fd, SANITY);		// Never failed
	TEST_ASSERT_EQUAL(0, errno);
	int ret = fclose(fd);
	TEST_ASSERT_EQUAL_MESSAGE(0, ret, SANITY);		// Never failed
	TEST_ASSERT_EQUAL_MESSAGE(0, errno, SANITY);	// Never failed

	TEST_ASSERT_TRUE_MESSAGE(file_exists(filepath), SANITY);	// Never failed

    LOCAL_TEST_tearDown();
}

TEST_CASE("Iterate through a directory", "[esp_ramfs]")
{
    LOCAL_TEST_setUp();

	const char* dirpath = MOUNT_POINT "/dir/";
    mkdir(dirpath, 0755);

	const int number_of_files = 3;
    const char* file_path[3] = {
        MOUNT_POINT "/dir/" "file1.txt",
        MOUNT_POINT "/dir/" "submarine.txt",
        MOUNT_POINT "/dir/" "file3.sub",
    };
    for (int i = 0; i < number_of_files; i++) {
	    FILE* fd = fopen(file_path[i], "w");
	    TEST_ASSERT_NOT_NULL(fd);
	
	    int ret = fclose(fd);
	    TEST_ASSERT_EQUAL(0, ret);
    }

	int count = 0;

    DIR* dir = opendir(dirpath);       // uses approximately 596 bytes of heap memory (vfs_fat_dir_t)
    TEST_ASSERT_NOT_NULL_MESSAGE(dir, SANITY);
    struct dirent * entry;
    while ((entry = readdir(dir)) != NULL) {
		ESP_LOGI(TAG, "Found entry: '%s'", entry->d_name);
		count++;
	}
	TEST_ASSERT_EQUAL_MESSAGE(number_of_files, count, SANITY);	// Never failed



    LOCAL_TEST_tearDown();
}

// POSIX specifications for readdir:
//
// 		"If a file is removed from or added to the directory after the most recent call 
//		to opendir() or rewinddir(), whether a subsequent call to readdir() returns an 
//		entry for that file is unspecified."
//
//	Note "that file", meaning that other files are unaffected.

TEST_CASE("Delete files as we iterate a directory", "[esp_ramfs]")
{
    LOCAL_TEST_setUp();

	const char* dirpath = MOUNT_POINT "/dir/";
    mkdir(dirpath, 0755);

	// With enough of them, the pattern of failures becomes more obvious
	const int number_of_files = 10;
    const char* file_path[10] = {
        MOUNT_POINT "/dir/" "0",
        MOUNT_POINT "/dir/" "1",
        MOUNT_POINT "/dir/" "2",
        MOUNT_POINT "/dir/" "3",
        MOUNT_POINT "/dir/" "4",
        MOUNT_POINT "/dir/" "5",
        MOUNT_POINT "/dir/" "6",
        MOUNT_POINT "/dir/" "7",
        MOUNT_POINT "/dir/" "8",
        MOUNT_POINT "/dir/" "9",
    };
    for (int i = 0; i < number_of_files; i++) {
	    FILE* fd = fopen(file_path[i], "w");
	    TEST_ASSERT_NOT_NULL(fd);
	
	    int ret = fclose(fd);
	    TEST_ASSERT_EQUAL(0, ret);
    }

	char scratch_path[100];
    strncpy(scratch_path, dirpath, 100);


    DIR* dir = opendir(dirpath);       // uses approximately 596 bytes of heap memory (vfs_fat_dir_t)
    TEST_ASSERT_NOT_NULL_MESSAGE(dir, SANITY);
    
	struct dirent * entry;
	int count = 0;
    while ((entry = readdir(dir)) != NULL) {
		ESP_LOGI(TAG, "found entry: '%s'", entry->d_name);
        scratch_path[strlen(dirpath)] = '\0';  // No need to write the directory each time
        strlcat(scratch_path, entry->d_name, 100);

//		TEST_ASSERT_EQUAL_STRING(file_path[count], scratch_path);	// Check it's the right one

		ESP_LOGI(TAG, "Deleting: '%s'", scratch_path);
		

		int ret = unlink(scratch_path);
		TEST_ASSERT_EQUAL_MESSAGE(0, ret, scratch_path);
		count++;
	}
	int ret = closedir(dir);
	TEST_ASSERT_EQUAL_MESSAGE(0, ret, strerror(errno));

	TEST_ASSERT_EQUAL(number_of_files, count);



    LOCAL_TEST_tearDown();
}

TEST_CASE("Delete multiple files behind the iterator", "[esp_ramfs]")
{
    LOCAL_TEST_setUp();

	int ret;

	const char* dirpath = MOUNT_POINT "/dir/";
    mkdir(dirpath, 0755);

	// With enough of them, the pattern of failures becomes more obvious
	const int number_of_files = 10;
    const char* file_path[10] = {
        MOUNT_POINT "/dir/" "0",
        MOUNT_POINT "/dir/" "1",
        MOUNT_POINT "/dir/" "2",
        MOUNT_POINT "/dir/" "3",
        MOUNT_POINT "/dir/" "4",
        MOUNT_POINT "/dir/" "5",
        MOUNT_POINT "/dir/" "6",
        MOUNT_POINT "/dir/" "7",
        MOUNT_POINT "/dir/" "8",
        MOUNT_POINT "/dir/" "9",
    };
    for (int i = 0; i < number_of_files; i++) {
	    FILE* fd = fopen(file_path[i], "w");
	    TEST_ASSERT_NOT_NULL(fd);
	
	    ret = fclose(fd);
	    TEST_ASSERT_EQUAL(0, ret);
    }

	char scratch_path[100];
    strncpy(scratch_path, dirpath, 100);

    DIR* dir = opendir(dirpath);       // uses approximately 596 bytes of heap memory (vfs_fat_dir_t)
    TEST_ASSERT_NOT_NULL_MESSAGE(dir, SANITY);
    
	struct dirent * entry;
	int count = 0;
	// Read several entries
	for (int i=0; i < 3; i++) {
		entry = readdir(dir);
		count++;
	}
	// Delete two of the already read ones
	ret = unlink(file_path[0]);
	ret = unlink(file_path[1]);


    while ((entry = readdir(dir)) != NULL) {
		ESP_LOGI(TAG, "found entry: '%s'", entry->d_name);
        scratch_path[strlen(dirpath)] = '\0';  // No need to write the directory each time
        strlcat(scratch_path, entry->d_name, 100);

		TEST_ASSERT_EQUAL_STRING(file_path[count], scratch_path);	// Check it's the right one

		count++;
	}
	ret = closedir(dir);
	TEST_ASSERT_EQUAL_MESSAGE(0, ret, strerror(errno));

	TEST_ASSERT_EQUAL(number_of_files, count);



    LOCAL_TEST_tearDown();
}

// A test case for deleting in front of the iterator is not included, because it is 'unspecified behaviour'
// i.e. the deleted files can be listed anyway, not listed, or a mixture of both!

TEST_CASE("Delete behind and in front of the iterator", "[esp_ramfs]")
{
    LOCAL_TEST_setUp();

	int ret;

	const char* dirpath = MOUNT_POINT "/dir/";
    mkdir(dirpath, 0755);

	// With enough of them, the pattern of failures becomes more obvious
	const int number_of_files = 10;
    const char* file_path[10] = {
        MOUNT_POINT "/dir/" "0",
        MOUNT_POINT "/dir/" "1",
        MOUNT_POINT "/dir/" "2",
        MOUNT_POINT "/dir/" "3",
        MOUNT_POINT "/dir/" "4",
        MOUNT_POINT "/dir/" "5",
        MOUNT_POINT "/dir/" "6",
        MOUNT_POINT "/dir/" "7",
        MOUNT_POINT "/dir/" "8",
        MOUNT_POINT "/dir/" "9",
    };
    for (int i = 0; i < number_of_files; i++) {
	    FILE* fd = fopen(file_path[i], "w");
	    TEST_ASSERT_NOT_NULL(fd);
	
	    ret = fclose(fd);
	    TEST_ASSERT_EQUAL(0, ret);
    }

	char scratch_path[100];
    strncpy(scratch_path, dirpath, 100);

    DIR* dir = opendir(dirpath);       // uses approximately 596 bytes of heap memory (vfs_fat_dir_t)
    TEST_ASSERT_NOT_NULL_MESSAGE(dir, SANITY);
    
	struct dirent * entry;
	int count = 0;
	// Read several entries
	for (int i=0; i < 3; i++) {
		entry = readdir(dir);
		count++;
	}
	// Delete two of the already read ones
	ret = unlink(file_path[1]);
	ret = unlink(file_path[6]);


    while ((entry = readdir(dir)) != NULL) {
		ESP_LOGI(TAG, "entry[%d]: '%s'", count, entry->d_name);
        scratch_path[strlen(dirpath)] = '\0';  // No need to write the directory each time
        strlcat(scratch_path, entry->d_name, 100);

		// Whether a file added/removed after the call to opendir() is returned by subsequent readdir is unspecified.
		// The 'implementation' doesn't even need to Ddocument it! Or consistently do one, and not the other.
		if (count == 6) {
			count++;	// In our case, though, it will consistently skip deleted directories.
		}

		TEST_ASSERT_EQUAL_STRING(file_path[count], scratch_path);

		count++;
	}
	ret = closedir(dir);
	TEST_ASSERT_EQUAL_MESSAGE(0, ret, strerror(errno));

	TEST_ASSERT_EQUAL(number_of_files, count);



    LOCAL_TEST_tearDown();
}

TEST_CASE("Delete many behind and in front of the iterator", "[esp_ramfs]")
{
    LOCAL_TEST_setUp();

	int ret;

	const char* dirpath = MOUNT_POINT "/dir/";
    mkdir(dirpath, 0755);

	// With enough of them, the pattern of failures becomes more obvious
	const int number_of_files = 10;
    const char* file_path[10] = {
        MOUNT_POINT "/dir/" "0",
        MOUNT_POINT "/dir/" "1",
        MOUNT_POINT "/dir/" "2",
        MOUNT_POINT "/dir/" "3",
        MOUNT_POINT "/dir/" "4",
        MOUNT_POINT "/dir/" "5",
        MOUNT_POINT "/dir/" "6",
        MOUNT_POINT "/dir/" "7",
        MOUNT_POINT "/dir/" "8",
        MOUNT_POINT "/dir/" "9",
    };
    for (int i = 0; i < number_of_files; i++) {
	    FILE* fd = fopen(file_path[i], "w");
	    TEST_ASSERT_NOT_NULL(fd);
	
	    ret = fclose(fd);
	    TEST_ASSERT_EQUAL(0, ret);
    }

	char scratch_path[100];
    strncpy(scratch_path, dirpath, 100);

    DIR* dir = opendir(dirpath);       // uses approximately 596 bytes of heap memory (vfs_fat_dir_t)
    TEST_ASSERT_NOT_NULL_MESSAGE(dir, SANITY);
    
	struct dirent * entry;
	int count = 0;
	// Read several entries
	for (int i=0; i < 3; i++) {
		entry = readdir(dir);
		count++;
	}
	// Delete two of the already read ones
	ret = unlink(file_path[1]);
	ret = unlink(file_path[2]);
	ret = unlink(file_path[6]);
	ret = unlink(file_path[8]);
	ret = unlink(file_path[7]);


    while ((entry = readdir(dir)) != NULL) {
		ESP_LOGI(TAG, "entry[%d]: '%s'", count, entry->d_name);
        scratch_path[strlen(dirpath)] = '\0';  // No need to write the directory each time
        strlcat(scratch_path, entry->d_name, 100);

		// Whether a file added/removed after the call to opendir() is returned by subsequent readdir is unspecified.
		// The 'implementation' doesn't even need to Ddocument it! Or consistently do one, and not the other.
		if (count == 6) {
			count += 3;	// In our case, though, it will consistently skip deleted directories.
		}

		TEST_ASSERT_EQUAL_STRING(file_path[count], scratch_path);

		count++;
	}
	ret = closedir(dir);
	TEST_ASSERT_EQUAL_MESSAGE(0, ret, strerror(errno));

	TEST_ASSERT_EQUAL(number_of_files, count);



    LOCAL_TEST_tearDown();
}

