/*
 * Placeholder PetaLinux user application.
 *
 * Replace this with your application code
 */
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <pthread.h>
#include <libplIpcore.h>
#include <plIpcore_def.h>

#define TEST_SIZE 4096*64

static int rx_proxy_fd;
static struct dma_proxy_channel_interface *rx_proxy_interface_p;

struct dma_proxy_channel_interface {
	int buffer[TEST_SIZE];
	enum proxy_status { PROXY_NO_ERROR = 0, PROXY_BUSY = 1, PROXY_TIMEOUT = 2, PROXY_ERROR = 3 } status;
	unsigned int length;
};

int capctl_trig(int length) {
	rru_write_reg(IPCORE_CAPCTL, 1, length);
	rru_write_reg(IPCORE_CAPCTL, 0, 1);
	rru_write_reg(IPCORE_CAPCTL, 0, 0);
}

void *rx_thread(void *arg)
{
	int dummy, i;
	int *testsz = (int *) arg;

	/* Set up the length for the DMA transfer and initialize the transmit
 	 * buffer to a known pattern.
 	 */
	rx_proxy_interface_p->length = *testsz * 4;

	for (i = 0; i < *testsz; i++)
   		rx_proxy_interface_p->buffer[i] = i;

	/* Perform the DMA transfer and the check the status after it completes
 	 * as the call blocks til the transfer is done.
 	 */
	ioctl(rx_proxy_fd, 0, &dummy);
	printf("%s.Call tx_proxy_fd ioctl\n", __func__);
	if (rx_proxy_interface_p->status != PROXY_NO_ERROR)
		printf("Proxy tx transfer error\n");
}

int main(int argc, char *argv[]) {
	int i, error = 0, testsz = 4096, dummy, tmp;
	pthread_t tid;
	FILE *fp1, *fp2;
	fp1 = fopen("/home/root/output_i.csv", "w");
	fp2 = fopen("/home/root/output_q.csv", "w");
	if ((fp1 == NULL) || (fp2 == NULL)) {
		printf("Can not open file for log\n");
		return 0;
	}

	rx_proxy_fd = open("/dev/dma_proxy_rx", O_RDWR);
	if (rx_proxy_fd < 1) {
		printf("Unable to open DMA proxy device file");
		return -1;
	}
	printf("Open dma_proxy_rx success\n");
	rru_open_device();

	rx_proxy_interface_p = (struct dma_proxy_channel_interface *)mmap(NULL, sizeof(struct dma_proxy_channel_interface),
									PROT_READ | PROT_WRITE, MAP_SHARED, rx_proxy_fd, 0);

	if ((rx_proxy_interface_p == MAP_FAILED)) {
    	printf("Failed to mmap\n");
    	return -1;
	}

	pthread_create(&tid, NULL, rx_thread, (void *) &testsz);
	/*Trigger Capture Controller*/
	sleep(1);
	capctl_trig(testsz);

	if (rx_proxy_interface_p->status != PROXY_NO_ERROR)
		printf("%s.Proxy rx transfer error\n", __func__);

	for (i = 0; i < testsz; i++) {
		tmp = rx_proxy_interface_p->buffer[i] & 0x0000FFFF;
		if (tmp >= 32768)
			tmp = tmp - 65536;
		fprintf(fp1, "%d\n", tmp);
		tmp = (rx_proxy_interface_p->buffer[i] >> 16) & 0x0000FFFF;
		if (tmp >= 32768)
			tmp = tmp - 65536;
		fprintf(fp2, "%d\n", tmp);
	}

	munmap(rx_proxy_interface_p, sizeof(struct dma_proxy_channel_interface));
	close(rx_proxy_fd);
	fclose(fp1); fclose(fp2); 
	rru_close_device();
}


