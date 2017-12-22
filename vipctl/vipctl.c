/*
 * Placeholder PetaLinux user application.
 *
 * Replace this with your application code
 */
#include <stdio.h>
// #include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define DEV_NAME "/dev/vipdev"

enum VIP_COMMAND_ID {
	VIP_READ_REG,
	VIP_WRITE_REG,
};

typedef struct vip_cmd {
	unsigned int addr;
	unsigned int data;
} vip_command_t;

static int device_fd = -1;

int vip_open_device() {
	device_fd = open(DEV_NAME, O_WRONLY);

	return device_fd > 0 ? 0 : -1;
}

int vip_close_device(){

	if (device_fd > 0){
		close(device_fd);
	}
	device_fd = -1;

	return 0;
}

int vip_write_register(int reg, unsigned data) {
	unsigned int rc;
	vip_command_t cmd;

	if (device_fd < 0) {
		printf("Device not yet opened\n");
		return -1;
	}

	cmd.addr = reg;
	cmd.data = data;
	rc = ioctl(device_fd, VIP_WRITE_REG, &cmd);

	return rc;
}

int process_command(int argc, char *argv[]) {
	int rc, reg;
	unsigned int data;

	if (!strcmp(argv[1], "read")) {
      	if (argc != 3 ) {
        	printf("Command invalid 1\n");
        	return -1;
      	}
      	reg  = strtol(argv[2],NULL,0);
      	// data = dpd_read_register(reg);
      	printf("%u\n",data);
    }

    else if (!strcmp(argv[1],"write")) {
    	reg  = strtol(argv[2],NULL,0);
    	data = strtol(argv[3],NULL,0);

    	printf("Write: %d to %d\n", data, reg);
      	rc = vip_write_register(reg, data);
      	return rc;
    }

    else {
    	printf("Command invalid\n");
    	return 0;
    }
}

int main(int argc, char *argv[])
{
	// printf("Hello, PetaLinux World!\n");
	// printf("cmdline args:\n");
	// while(argc--)
	// 	printf("%s\n",*argv++);

	int rc;

	rc = vip_open_device();

	if (rc) {
		printf("Can not open device. Exit.\n");
		return -1;
	}

	process_command(argc, argv);

	vip_close_device();

	return 0;
}


