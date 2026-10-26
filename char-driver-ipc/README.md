# Linux Kernel Character Device Driver (vchar_ipc)

A loadable Linux kernel module (LKM) implementing a thread-safe virtual character device (`/dev/vchar_ipc`) for low-latency Inter-Process Communication (IPC) between user-space processes across the Ring 3 / Ring 0 boundary.

## Key Architectural Features
- **Dynamic VFS Registration:** Allocates Major/Minor device numbers dynamically via `alloc_chrdev_region` and exposes `/dev/vchar_ipc` automatically through `udev` (`class_create` / `device_create`).
- **Cross-Kernel Compatibility:** Supports Linux Kernel 5.15 through 7.0+ via conditional Kbuild preprocessor guards (`LINUX_VERSION_CODE`).
- **Thread-Safe Concurrency:** Serializes concurrent user-space `read`, `write`, and `ioctl` operations using interruptible kernel mutex locks (`DEFINE_MUTEX` / `mutex_lock_interruptible`).
- **Data Plane & Control Plane Separation:**
  - **Data Plane (`read` / `write`):** Safe user/kernel address-space transfers using `copy_from_user()` and `copy_to_user()`.
  - **Control Plane (`unlocked_ioctl`):** Out-of-band buffer inspection (`VCHAR_IOC_GET_SIZE`) and zeroing (`VCHAR_IOC_CLEAR`).

## Project Structure
~~~text
char-driver-ipc/
 Makefile              # Top-level Kbuild + userspace build orchestration
 src/
    Makefile          # Kbuild object definition
    vchar_driver.c    # Ring-0 character device driver implementation
 user/
     test_app.c        # Userspace POSIX syscall & ioctl verification suite
~~~

## Build & Verification
~~~bash
make
sudo insmod src/vchar_driver.ko
ls -l /dev/vchar_ipc
sudo ./user/test_app
sudo dmesg | tail -n 10
sudo rmmod vchar_driver
~~~

## Sample Output (Linux 6.8.0-147-generic)
~~~text
crw------- 1 root root 240, 0 Oct 26 20:36 /dev/vchar_ipc
[1] Writing payload to /dev/vchar_ipc...
[2] ioctl(VCHAR_IOC_GET_SIZE): Kernel buffer currently holds 61 bytes
[3] Read back from kernel: "Zero-copy IPC payload from Userspace to Ring 0 Kernel Buffer!"
[4] ioctl(VCHAR_IOC_CLEAR): Buffer cleared (size is now 0 bytes)

[  784.716495] vchar_ipc: Registered with Major 240, Minor 0
[  784.730248] vchar_ipc: Device opened by PID 3187
[  784.730306] vchar_ipc: Received 61 bytes from userspace
[  784.730316] vchar_ipc: Sent 61 bytes to userspace
[  784.730318] vchar_ipc: Buffer cleared via ioctl
[  784.730320] vchar_ipc: Device closed by PID 3187
~~~
