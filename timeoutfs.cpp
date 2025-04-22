#define FUSE_USE_VERSION 31

#include <fuse3/fuse.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <cstring>
#include <iostream>
#include <string>
#include <map>
#include <mutex>
#include <thread>
#include <chrono>
#include <filesystem>
#include <syslog.h>
#include <signal.h>
#include <errno.h>

namespace fs = std::filesystem;

static std::string backing_dir = "/tmp/timeoutfs_data";
static std::map<time_t, std::string> expiry_map;
static std::mutex expiry_mutex;
static int timeout_seconds = 60;
static bool running = true;

// Logger function
void log_message(int priority, const char* format, ...) {
    va_list args;
    va_start(args, format);
    vsyslog(priority, format, args);
    va_end(args);
}

void signal_handler(int sig) {
    if (sig == SIGTERM || sig == SIGINT) {
        log_message(LOG_INFO, "Received signal %d, shutting down...", sig);
        running = false;
    }
}

void daemonize() {
    if (daemon(0, 0) < 0) {
        std::cerr << "Failed to daemonize: " << strerror(errno) << std::endl;
        exit(EXIT_FAILURE);
    }

    // Initialize syslog
    openlog("timeoutfs", LOG_PID | LOG_NDELAY, LOG_DAEMON);
    log_message(LOG_INFO, "TimeoutFS daemon started");
}

#define TIMEOUTFS_OPT(t, p) { t, offsetof(struct timeoutfs_config, p), 1 }

struct timeoutfs_config {
    int timeout;
};

static struct timeoutfs_config config;

static struct fuse_opt timeoutfs_opts[] = {
    TIMEOUTFS_OPT("timeout=%d", timeout),
    FUSE_OPT_END
};

static int timeoutfs_opt_proc(void *data, const char *arg, int key, struct fuse_args *outargs) {
    (void)data;
    (void)outargs;
    return 1;
}

void delete_expired_files() {
    while (running) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        time_t now = time(nullptr);

        std::lock_guard<std::mutex> lock(expiry_mutex);
        auto it = expiry_map.begin();
        while (it != expiry_map.end() && it->first <= now) {
            std::string full_path = backing_dir + "/" + it->second;
            log_message(LOG_INFO, "Deleting expired file: %s", full_path.c_str());
            unlink(full_path.c_str());
            it = expiry_map.erase(it);
        }
    }
}

static std::string full_path(const char *path) {
    return backing_dir + std::string(path);
}

static int timeoutfs_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *) {
    memset(stbuf, 0, sizeof(struct stat));
    std::string fpath = full_path(path);

    if (fs::is_directory(fpath)) {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
        return 0;
    }

    if (fs::exists(fpath)) {
        return lstat(fpath.c_str(), stbuf);
    }

    return -ENOENT;
}

static int timeoutfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                             off_t, struct fuse_file_info *, enum fuse_readdir_flags) {
    if (std::string(path) != "/") return -ENOENT;

    filler(buf, ".", nullptr, 0, FUSE_FILL_DIR_PLUS);
    filler(buf, "..", nullptr, 0, FUSE_FILL_DIR_PLUS);

    for (const auto &entry : fs::directory_iterator(backing_dir)) {
        std::string name = entry.path().filename();
        filler(buf, name.c_str(), nullptr, 0, FUSE_FILL_DIR_PLUS);
    }

    return 0;
}

static int timeoutfs_open(const char *path, struct fuse_file_info *fi) {
    std::string fpath = full_path(path);
    int fd = open(fpath.c_str(), fi->flags);
    if (fd == -1) return -errno;
    close(fd);
    return 0;
}

static int timeoutfs_read(const char *path, char *buf, size_t size, off_t offset,
                          struct fuse_file_info *fi) {
    std::string fpath = full_path(path);
    int fd = open(fpath.c_str(), O_RDONLY);
    if (fd == -1) return -errno;

    int res = pread(fd, buf, size, offset);
    close(fd);
    return (res == -1) ? -errno : res;
}

static int timeoutfs_write(const char *path, const char *buf, size_t size, off_t offset,
                           struct fuse_file_info *fi) {
    std::string fpath = full_path(path);
    int fd = open(fpath.c_str(), O_WRONLY | O_CREAT, 0644);
    if (fd == -1) return -errno;

    int res = pwrite(fd, buf, size, offset);
    close(fd);
    return (res == -1) ? -errno : res;
}

static int timeoutfs_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    std::string fpath = full_path(path);
    int fd = open(fpath.c_str(), fi->flags | O_CREAT, mode);
    if (fd == -1) return -errno;
    close(fd);

    std::lock_guard<std::mutex> lock(expiry_mutex);
    time_t expiry = time(nullptr) + timeout_seconds;
    expiry_map[expiry] = path + 1; // skip leading '/'

    log_message(LOG_INFO, "Created file: %s with expiry in %d seconds", path, timeout_seconds);

    return 0;
}

static struct fuse_operations timeoutfs_oper = {};

int main(int argc, char *argv[]) {
    struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
    
    // Set default timeout
    config.timeout = 60;
    
    // Parse options
    if (fuse_opt_parse(&args, &config, timeoutfs_opts, &timeoutfs_opt_proc) == -1) {
        return 1;
    }
    
    timeout_seconds = config.timeout;
    std::cout << "[INFO] Timeout set to " << timeout_seconds << " seconds\n";

    fs::create_directories(backing_dir);

    // Daemonize the process
    daemonize();

    // Set up signal handlers
    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);

    std::thread(delete_expired_files).detach();

    timeoutfs_oper.getattr = timeoutfs_getattr;
    timeoutfs_oper.readdir = timeoutfs_readdir;
    timeoutfs_oper.open    = timeoutfs_open;
    timeoutfs_oper.read    = timeoutfs_read;
    timeoutfs_oper.write   = timeoutfs_write;
    timeoutfs_oper.create  = timeoutfs_create;

    int ret = fuse_main(args.argc, args.argv, &timeoutfs_oper, nullptr);
    fuse_opt_free_args(&args);

    log_message(LOG_INFO, "TimeoutFS daemon shutting down");
    closelog();

    return ret;
}
