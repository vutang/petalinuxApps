/*
 * Placeholder PetaLinux user application.
 *
 * Replace this with your application code
 */
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define DEV_NAME "/dev/vxhls_dev"

static int device_fd = -1;

enum VXHLS_COMMAND_ID {
	VXHLS_TEST,
	VXHLS_READ_REG,
	VXHLS_WRITE_REG
};

int vxdma_open_device() {
	device_fd = open(DEV_NAME, O_WRONLY);

	return device_fd > 0 ? 0 : -1;
}

int vxdma_close_device(){

	if (device_fd > 0){
		close(device_fd);
	}
	device_fd = -1;

	return 0;
}

int main(int argc, char *argv[]) {
	int rc, cmd;

	rc = vxdma_open_device();
	if (rc) {
		printf("Can not open device. Exit.\n");
		return -1;
	}

	if (!strcmp(argv[1], "test"))
		rc = ioctl(device_fd, VXHLS_TEST);
	if (rc < 0) {
		printf("ioctl fail\n");
	}

	vxdma_close_device();
	return 0;
}


