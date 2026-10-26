#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>

#define DEVICE_PATH "/dev/vchar_ipc"
#define VCHAR_MAGIC        'V'
#define VCHAR_IOC_CLEAR    _IO(VCHAR_MAGIC, 1)
#define VCHAR_IOC_GET_SIZE _IOR(VCHAR_MAGIC, 2, int)

int main(void)
{
    const char *msg = "Zero-copy IPC payload from Userspace to Ring 0 Kernel Buffer!";
    char read_buf[256] = {0};
    int stored_bytes = 0;

    int fd = open(DEVICE_PATH, O_RDWR);
    if (fd < 0) {
        perror("Failed to open /dev/vchar_ipc");
        return EXIT_FAILURE;
    }

    printf("[1] Writing payload to %s...\n", DEVICE_PATH);
    if (write(fd, msg, strlen(msg)) < 0) {
        perror("Write failed");
        close(fd);
        return EXIT_FAILURE;
    }

    if (ioctl(fd, VCHAR_IOC_GET_SIZE, &stored_bytes) == 0) {
        printf("[2] ioctl(VCHAR_IOC_GET_SIZE): Kernel buffer currently holds %d bytes\n", stored_bytes);
    }

    if (read(fd, read_buf, sizeof(read_buf) - 1) < 0) {
        perror("Read failed");
        close(fd);
        return EXIT_FAILURE;
    }
    printf("[3] Read back from kernel: \"%s\"\n", read_buf);

    if (ioctl(fd, VCHAR_IOC_CLEAR) == 0) {
        ioctl(fd, VCHAR_IOC_GET_SIZE, &stored_bytes);
        printf("[4] ioctl(VCHAR_IOC_CLEAR): Buffer cleared (size is now %d bytes)\n", stored_bytes);
    }

    close(fd);
    return EXIT_SUCCESS;
}
