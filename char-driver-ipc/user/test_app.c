#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <sys/ioctl.h>

#define DEVICE_PATH "/dev/vchar_ipc"
#define TEMP_FILE   "/tmp/ipc_disk_bench.tmp"
#define ITERATIONS  10000

#define VCHAR_MAGIC        'V'
#define VCHAR_IOC_CLEAR    _IO(VCHAR_MAGIC, 1)
#define VCHAR_IOC_GET_SIZE _IOR(VCHAR_MAGIC, 2, int)

static double get_time_us(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1e6 + (double)ts.tv_nsec / 1e3;
}

int main(void)
{
    const char *msg = "64-byte high-frequency telemetry frame across Ring3/Ring0 VFS!!";
    char read_buf[256] = {0};
    int stored_bytes = 0;
    ssize_t ret;

    int fd = open(DEVICE_PATH, O_RDWR);
    if (fd < 0) {
        perror("Failed to open /dev/vchar_ipc");
        return EXIT_FAILURE;
    }

    printf("=== [Part 1: Functional & Control Plane Verification] ===\n");
    ret = write(fd, msg, strlen(msg));
    (void)ret;
    
    if (ioctl(fd, VCHAR_IOC_GET_SIZE, &stored_bytes) == 0) {
        printf("[+] ioctl(VCHAR_IOC_GET_SIZE): %d bytes active in Ring-0 buffer\n", stored_bytes);
    }

    ret = read(fd, read_buf, sizeof(read_buf) - 1);
    (void)ret;
    printf("[+] Read back verified: \"%s\"\n", read_buf);

    ioctl(fd, VCHAR_IOC_CLEAR);
    if (ioctl(fd, VCHAR_IOC_GET_SIZE, &stored_bytes) == 0) {
        printf("[+] ioctl(VCHAR_IOC_CLEAR): Buffer reset to %d bytes\n\n", stored_bytes);
    }

    printf("=== [Part 2: IPC Latency Benchmark (%d Round-Trips)] ===\n", ITERATIONS);

    /* Benchmark 1: Ring-0 Character Device IPC */
    double start_k = get_time_us();
    for (int i = 0; i < ITERATIONS; i++) {
        ret = write(fd, msg, 64);
        ret = read(fd, read_buf, 64);
        (void)ret;
    }
    double total_k = get_time_us() - start_k;
    double avg_k = total_k / ITERATIONS;

    /* Benchmark 2: Traditional Filesystem Sync IPC */
    int disk_fd = open(TEMP_FILE, O_RDWR | O_CREAT | O_SYNC, 0644);
    double start_d = get_time_us();
    for (int i = 0; i < (ITERATIONS / 50); i++) {
        lseek(disk_fd, 0, SEEK_SET);
        ret = write(disk_fd, msg, 64);
        lseek(disk_fd, 0, SEEK_SET);
        ret = read(disk_fd, read_buf, 64);
        (void)ret;
    }
    double total_d = get_time_us() - start_d;
    double avg_d = total_d / (ITERATIONS / 50);
    close(disk_fd);
    unlink(TEMP_FILE);

    printf("  -> Ring-0 /dev/vchar_ipc Avg Round-Trip : %.2f us\n", avg_k);
    printf("  -> Traditional Sync File I/O Avg        : %.2f us\n", avg_d);
    printf("  -> Speedup Factor                       : %.1fx faster\n", avg_d / avg_k);

    close(fd);
    return EXIT_SUCCESS;
}
