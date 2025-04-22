# TimeoutFS ⏳

**TimeoutFS** is a FUSE-based virtual filesystem written in C++ that automatically deletes files after a specified timeout. The timeout is set in **seconds** at mount time using a `-o timeout=SECONDS` option.

## 👨‍💻 Author

Developed by Andrei Pushkin

## 📦 Features

- Automatically deletes files after a configurable timeout
- Timeout is set in **seconds**
- C++ implementation using `std::map<time_t, std::string>` to track files
- Background thread removes expired files every second
- Simple and minimal dependencies

## 🔧 Prerequisites & Build Instructions

First, install the required packages:

```bash
sudo apt install libfuse3-dev cmake g++ pkg-config
```

Clone this repository:

```bash
git clone https://github.com/andrewpushkin/timeoutfs.git
cd timeoutfs
```

Create a build directory and compile the project:

```bash
mkdir build && cd build
cmake ..
make
```

This will generate the timeoutfs binary inside the `build/` directory.

## ▶️ Run Instructions

Create the mount point:

```bash
mkdir /tmp/mountpoint
```

Run the filesystem with a timeout value in seconds (example: 30 seconds):

```bash
./timeoutfs -f /tmp/mountpoint -o timeout=30
```

Files created in `/tmp/mountpoint` will be automatically deleted 30 seconds after creation.

If the timeout option is not provided, it defaults to 60 seconds.

In a separate terminal, you can interact with the filesystem:

```bash
echo "test" > /tmp/mountpoint/file1.txt
ls /tmp/mountpoint
sleep 35
ls /tmp/mountpoint  # file1.txt will be gone
```

To stop the daemon, simply unmount the filesystem:

```bash
fusermount -u /tmp/mountpoint
```

This will cleanly shut down the TimeoutFS daemon.

## 🛠 How It Works

Files are stored under a real directory: `/tmp/timeoutfs_data`

When a file is created:
- Its deletion time (now + timeout) is inserted into a `std::map`

A background thread:
- Wakes up every second
- Deletes files whose time has expired

## ⚠️ Limitations

- Only tracks files created via the FUSE mountpoint
- If multiple files have the same deletion time, only the last is stored (map overwrites)
- No persistence across reboots (in-memory only)
- No directory nesting support yet

## 📝 License

MIT License

