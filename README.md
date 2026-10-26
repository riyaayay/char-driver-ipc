# Linux Kernel Character Device Driver (`vchar_ipc`)

A high-performance loadable Linux kernel module (LKM) implementing a thread-safe virtual character device (`/dev/vchar_ipc`) for low-latency Inter-Process Communication (IPC) across the Ring 3 (User Space) and Ring 0 (Kernel Space) boundary.

---

## 🚀 Motivation & Problem Statement

Modern local microservices, edge telemetry daemons, and multi-agent systems frequently exchange small, high-frequency control payloads on the same host machine. Routing intra-host IPC over **TCP Loopback (`127.0.0.1`)** or **synchronous disk files (`/tmp/...`)** introduces severe overhead: socket buffer allocation (`sk_buff`), netfilter state-machine traversal, and filesystem journaling bottlenecks.

**`vchar_ipc` eliminates these bottlenecks.** By exposing an in-memory kernel buffer directly through the Linux Virtual File System (`/dev/vchar_ipc`), user-space processes exchange state using raw POSIX file descriptors (`read`/`write`) and out-of-band control commands (`unlocked_ioctl`), achieving **sub-5 microsecond round-trip syscall latency** with deterministic mutex synchronization.

---

## 🛠️ Key Architectural Features

- **Dynamic VFS Registration:** Allocates Major/Minor device numbers dynamically via `alloc_chrdev_region` and exposes `/dev/vchar_ipc` automatically through `udev` (`class_create` / `device_create`).
- **Cross-Kernel Compatibility:** Supports Linux Kernel `5.15` through `7.0+` via conditional Kbuild preprocessor guards (`LINUX_VERSION_CODE`).
- **Thread-Safe Concurrency:** Serializes concurrent user-space `read`, `write`, and `ioctl` operations using interruptible kernel mutex locks (`DEFINE_MUTEX` / `mutex_lock_interruptible`).
- **Data Plane & Control Plane Separation:**
  - **Data Plane (`read` / `write`):** Safe user/kernel address-space memory copies using `copy_from_user()` and `copy_to_user()`.
  - **Control Plane (`unlocked_ioctl`):** Out-of-band buffer inspection (`VCHAR_IOC_GET_SIZE`) and zeroing (`VCHAR_IOC_CLEAR`).

---

## 📁 Project Structure

```text
char-driver-ipc/
├── Makefile            # Top-level Kbuild + userspace build orchestration
├── .gitignore          # Excludes kernel build artifacts (*.ko, *.o)
├── README.md           # Project documentation & benchmark telemetry
├── src/
│   ├── Makefile        # Kbuild object definition
│   └── vchar_driver.c  # Ring-0 character device driver implementation
└── user/
    └── test_app.c      # POSIX syscall verification & 10,000-iteration benchmark suite
```

---

## ⚙️ Build & Verification

```bash
# 1. Compile the kernel module (.ko) and userspace test binary
make

# 2. Load module into the kernel and verify udev node creation
sudo insmod src/vchar_driver.ko
ls -l /dev/vchar_ipc

# 3. Execute functional verification and latency benchmark suite
sudo ./user/test_app

# 4. Inspect kernel ring buffer logs and unload module
sudo dmesg | tail -n 10
sudo rmmod vchar_driver
make clean
```

---

## 📊 Empirical Benchmark Results

*Tested on Linux Kernel 6.8.0-147-generic (Ubuntu 24.04 LTS VM over 10,000 continuous iterations with 64-byte payloads).*

### Performance Comparison

| IPC / I/O Mechanism | Avg Round-Trip Latency | Throughput Efficiency |
| :--- | :---: | :--- |
| **Ring-0 Character Device (`/dev/vchar_ipc`)** | **4.46 µs** | Direct RAM / Zero Disk I/O |
| Traditional Synchronous File I/O (`O_SYNC`) | 3211.07 µs | Filesystem Journaling Overhead |
| **Performance Speedup Factor** | **719.9x Faster** | Eliminates Disk & FS Contention |

### Sample Kernel Ring Buffer (`dmesg`) Trace

```text
[  784.716495] vchar_ipc: Registered with Major 240, Minor 0
[  784.730248] vchar_ipc: Device opened by PID 3187
[  784.730306] vchar_ipc: Received 61 bytes from userspace
[  784.730316] vchar_ipc: Sent 61 bytes to userspace
[  784.730318] vchar_ipc: Buffer cleared via ioctl
[  784.730320] vchar_ipc: Device closed by PID 3187
```
