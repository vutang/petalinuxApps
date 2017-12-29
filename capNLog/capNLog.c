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
#include <unistd.h>

#define TEST_SIZE 4096*64
#define BIT(x)				(1 << x)

#define IPCORE_CAPCTL_REG_CTRL 	0
#define IPCORE_CAPCTL_REG_STS	1
#define IPCORE_CAPCTL_REG_REMAIN_IN_BUFFER	2

#define IPCORE_CAPCTL_CTRL_CAPTURE_REQ_MASK BIT(0)
#define IPCORE_CAPCTL_CTRL_TRANSFER_REQ_MASK BIT(1)
#define IPCORE_CAPCTL_CTRL_TRANSAC_TRIG_MASK BIT(2)

#define IPCORE_CAPCTL_STS_DATA_VALID_MASK BIT(0)
#define IPCORE_CAPCTL_STS_BUFFER_IS_ALMOST_EMPTY_MASK BIT(1)

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

int send_capture_req() {
	int ret;
	ret = rru_write_reg(IPCORE_CAPCTL, IPCORE_CAPCTL_REG_CTRL, 
		rru_read_reg(IPCORE_CAPCTL, IPCORE_CAPCTL_REG_CTRL) & ~IPCORE_CAPCTL_CTRL_CAPTURE_REQ_MASK);

	ret = rru_write_reg(IPCORE_CAPCTL, IPCORE_CAPCTL_REG_CTRL, 
		rru_read_reg(IPCORE_CAPCTL, IPCORE_CAPCTL_REG_CTRL) | IPCORE_CAPCTL_CTRL_CAPTURE_REQ_MASK);

	ret = rru_write_reg(IPCORE_CAPCTL, IPCORE_CAPCTL_REG_CTRL, 
		rru_read_reg(IPCORE_CAPCTL, IPCORE_CAPCTL_REG_CTRL) & ~IPCORE_CAPCTL_CTRL_CAPTURE_REQ_MASK);
	return ret;
}

int is_ready_for_transfer() {
	return ((rru_read_reg(IPCORE_CAPCTL, IPCORE_CAPCTL_REG_STS) & IPCORE_CAPCTL_STS_DATA_VALID_MASK) != 0);
}

int buffer_is_almost_empty() {
	return ((rru_read_reg(IPCORE_CAPCTL, IPCORE_CAPCTL_REG_STS) & IPCORE_CAPCTL_STS_BUFFER_IS_ALMOST_EMPTY_MASK) != 0);	
}

int check_remain_in_buffer() {
	return (rru_read_reg(IPCORE_CAPCTL, IPCORE_CAPCTL_REG_REMAIN_IN_BUFFER));
}

int send_transfer_req() {
	int ret;
	ret = rru_write_reg(IPCORE_CAPCTL, IPCORE_CAPCTL_REG_CTRL, 
		rru_read_reg(IPCORE_CAPCTL, IPCORE_CAPCTL_REG_CTRL) & ~IPCORE_CAPCTL_CTRL_TRANSFER_REQ_MASK);

	ret = rru_write_reg(IPCORE_CAPCTL, IPCORE_CAPCTL_REG_CTRL, 
		rru_read_reg(IPCORE_CAPCTL, IPCORE_CAPCTL_REG_CTRL) | IPCORE_CAPCTL_CTRL_TRANSFER_REQ_MASK);

	ret = rru_write_reg(IPCORE_CAPCTL, IPCORE_CAPCTL_REG_CTRL, 
		rru_read_reg(IPCORE_CAPCTL, IPCORE_CAPCTL_REG_CTRL) & ~IPCORE_CAPCTL_CTRL_TRANSFER_REQ_MASK);
	return ret;
}

int send_transac_trig() {
	int ret;
	ret = rru_write_reg(IPCORE_CAPCTL, IPCORE_CAPCTL_REG_CTRL, 
		rru_read_reg(IPCORE_CAPCTL, IPCORE_CAPCTL_REG_CTRL) & ~IPCORE_CAPCTL_CTRL_TRANSAC_TRIG_MASK);

	ret = rru_write_reg(IPCORE_CAPCTL, IPCORE_CAPCTL_REG_CTRL, 
		rru_read_reg(IPCORE_CAPCTL, IPCORE_CAPCTL_REG_CTRL) | IPCORE_CAPCTL_CTRL_TRANSAC_TRIG_MASK);

	ret = rru_write_reg(IPCORE_CAPCTL, IPCORE_CAPCTL_REG_CTRL, 
		rru_read_reg(IPCORE_CAPCTL, IPCORE_CAPCTL_REG_CTRL) & ~IPCORE_CAPCTL_CTRL_TRANSAC_TRIG_MASK);
	return ret;
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
	// printf("%s.Call rx_proxy_fd ioctl\n", __func__);
	if (rx_proxy_interface_p->status != PROXY_NO_ERROR)
		printf("Proxy rx transfer error\n");
}

int main(int argc, char *argv[]) {
	int i, error = 0, testsz = 16384, dummy, tmp, remain_in_buffer = 0;
	pthread_t tid;
	FILE *fp1;
	fp1 = fopen("/home/root/output_i.csv", "w");
	// fp2 = fopen("/home/root/output_q.csv", "w");
	if (fp1 == NULL) {
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

	// for (i = 0; i < 1; i++) {
	// 	pthread_create(&tid, NULL, rx_thread, (void *) &testsz);
	// 	usleep(20000);
	// 	pthread_create(&tid, NULL, rx_thread, (void *) &testsz);

	// 	usleep(20000);
	// 	capctl_trig(testsz);
	// 	/*Trigger Capture Controller*/
	// 	usleep(20000);
	// 	capctl_trig(testsz);
	// }

	printf("Send capture req\n");
	send_capture_req();
	printf("Wait for transfer\n");
	printf("%d\n", (rru_read_reg(IPCORE_CAPCTL, IPCORE_CAPCTL_REG_STS)));
	while (is_ready_for_transfer() == 0) {
		usleep(10000);
	}
	printf("Send transfer req\n");
	send_transfer_req();
	usleep(10000);

	while(!buffer_is_almost_empty()) {
		printf("------------------------------------------------------\n");
		pthread_create(&tid, NULL, rx_thread, (void *) &testsz);
		usleep(80000);
		printf("Send transac trig\n");
		send_transac_trig();
		usleep(1000);
		pthread_join(tid, NULL);

		if (rx_proxy_interface_p->status != PROXY_NO_ERROR) {
			printf("%s.Proxy rx transfer error: errorno %d\n", 
				__func__, rx_proxy_interface_p->status);
			// break;
		}

		/*Convert int32_t to int16_t*/
		// for (i = 0; i < testsz; i++) {
		// 	tmp = rx_proxy_interface_p->buffer[i] & 0x0000FFFF;
		// 	if (tmp >= 32768)
		// 		tmp = tmp - 65536;
		// 	fprintf(fp1, "%d\n", tmp);
		// 	tmp = (rx_proxy_interface_p->buffer[i] >> 16) & 0x0000FFFF;
		// 	if (tmp >= 32768)
		// 		tmp = tmp - 65536;
		// 	fprintf(fp2, "%d\n", tmp);
		// }
		for (i = 0; i < testsz; i++)
			fprintf(fp1, "%d\n", rx_proxy_interface_p->buffer[i]);
	}

	remain_in_buffer = check_remain_in_buffer();
	printf("remain_in_buffer: %d\n", remain_in_buffer);

	printf("Last transaction\n");
	pthread_create(&tid, NULL, rx_thread, (void *) &remain_in_buffer);
	usleep(300000);
	printf("Send transac trig\n");
	send_transac_trig();
	pthread_join(tid, NULL);
	munmap(rx_proxy_interface_p, sizeof(struct dma_proxy_channel_interface));
	close(rx_proxy_fd);
	fclose(fp1); 
	// fclose(fp2); 
	rru_close_device();
}


