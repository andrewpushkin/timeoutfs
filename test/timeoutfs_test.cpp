#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <thread>
#include <chrono>
#include <sys/stat.h>
#include <cstdlib>

namespace fs = std::filesystem;

class TimeoutFSTest : public ::testing::Test {
protected:
    const std::string mount_dir = "/tmp/timeoutfs_test";
    const std::string backing_dir = "/tmp/timeoutfs_test_data";
    const int timeout_seconds = 3; // Short timeout for tests

    void SetUp() override {
        // Create mount and backing directories
        fs::create_directories(mount_dir);
        fs::create_directories(backing_dir);

        // Start timeoutfs with short timeout
        std::string cmd = "./timeoutfs " + mount_dir + " -f -o timeout=" + 
                         std::to_string(timeout_seconds) + " &";
        // Run the command in the background
        std::cout << "Starting timeoutfs with command: " << cmd << std::endl;
        // Execute the command and check return code
        int ret = system(cmd.c_str());
        if (ret != 0) {
            std::cerr << "Failed to start timeoutfs: " << ret << std::endl;
            exit(EXIT_FAILURE);
        }

        // Give it a moment to start up
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    void TearDown() override {
        // Unmount timeoutfs
        std::string cmd = "fusermount -u " + mount_dir;
        system(cmd.c_str());

        // Clean up directories
        fs::remove_all(mount_dir);
        fs::remove_all(backing_dir);
    }
    
    bool fileExists(const std::string& path) {
        struct stat buffer;
        return (stat(path.c_str(), &buffer) == 0);
    }
};

TEST_F(TimeoutFSTest, DirectoryCreation) {
    ASSERT_TRUE(fs::exists(mount_dir));
    ASSERT_TRUE(fs::is_directory(mount_dir));
}

TEST_F(TimeoutFSTest, FileCreationAndDeletion) {
    std::string test_file = mount_dir + "/test.txt";
    std::ofstream file(test_file);
    file << "Test content" << std::endl;
    file.close();
    
    ASSERT_TRUE(fileExists(test_file));
    
    // Wait for 2 seconds
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    // File should still exist as timeout is not reached
    ASSERT_TRUE(fileExists(test_file));
}

TEST_F(TimeoutFSTest, FileReadWrite) {
    std::string test_file = mount_dir + "/test_rw.txt";
    std::string test_content = "Hello, TimeoutFS!";
    
    // Write to file
    {
        std::ofstream file(test_file);
        file << test_content;
    }
    
    // Read from file
    std::string read_content;
    {
        std::ifstream file(test_file);
        std::getline(file, read_content);
    }
    
    ASSERT_EQ(read_content, test_content);
}

TEST_F(TimeoutFSTest, FileDeletedAfterTimeout) {
    std::string test_file = mount_dir + "/test_timeout.txt";
    std::ofstream file(test_file);
    file << "Test content" << std::endl;
    file.close();
    
    ASSERT_TRUE(fileExists(test_file));
    
    // Wait for a bit more than the timeout period (using 5 seconds to ensure deletion)
    std::this_thread::sleep_for(std::chrono::seconds(5));
    
    // File should be deleted after timeout
    ASSERT_FALSE(fileExists(test_file));
}