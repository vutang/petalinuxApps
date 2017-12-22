/* DMA Proxy Test Application
 *
 * This application is intended to be used with the DMA Proxy device driver. It provides
 * an example application showing how to use the device driver to do user space DMA
 * operations.
 *
 * It has been tested with an AXI DMA system with transmit looped back to receive.
 * The device driver implements a blocking ioctl() function such that a thread is
 * needed for the 2nd channel. Since the AXI DMA transmit is a stream without any
 * buffering it is throttled until the receive channel is running.
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <pthread.h>

#define TEST_SIZE 4096*256

struct dma_proxy_channel_interface {
	unsigned char buffer[TEST_SIZE];
	enum proxy_status { PROXY_NO_ERROR = 0, PROXY_BUSY = 1, PROXY_TIMEOUT = 2, PROXY_ERROR = 3 } status;
	unsigned int length;
};

static struct dma_proxy_channel_interface *tx_proxy_interface_p;
static int tx_proxy_fd;

/* The following function is the transmit thread to allow the transmit and the
 * receive channels to be operating simultaneously. The ioctl calls are blocking
 * such that a thread is needed.
 */
void *tx_thread(void *arg)
{
	int dummy, i;
	int *testsz = (int *) arg;

	/* Set up the length for the DMA transfer and initialize the transmit
 	 * buffer to a known pattern.
 	 */
	tx_proxy_interface_p->length = *testsz;

    	for (i = 0; i < *testsz; i++)
       		tx_proxy_interface_p->buffer[i] = i;

	/* Perform the DMA transfer and the check the status after it completes
 	 * as the call blocks til the transfer is done.
 	 */
	ioctl(tx_proxy_fd, 0, &dummy);
	printf("%s.Call tx_proxy_fd ioctl\n", __func__);
	if (tx_proxy_interface_p->status != PROXY_NO_ERROR)
		printf("Proxy tx transfer error\n");
}

/* The following function uses the dma proxy device driver to perform DMA transfers
 * from user space. This app and the driver are tested with a system containing an
 * AXI DMA without scatter gather and with transmit looped back to receive.
 */
int main(int argc, char *argv[])
{
	struct dma_proxy_channel_interface *rx_proxy_interface_p;
	int rx_proxy_fd, i, error = 0, testsz;
	int dummy;
	pthread_t tid;

	printf("DMA proxy test\n");
	if (argc > 1) {
		testsz = strtod(argv[1], NULL);
		if (testsz > TEST_SIZE) {
			printf("Test size has to be lower than %d\n", TEST_SIZE);
			return 0;
		}
	}
	else 
		testsz = TEST_SIZE;

	/* Step 1, open the DMA proxy device for the transmit and receive channels with
 	 * read/write permissions
 	 */

	tx_proxy_fd = open("/dev/dma_proxy_tx", O_RDWR);

	if (tx_proxy_fd < 1) {
		printf("Unable to open DMA proxy device file");
		return -1;
	}

	rx_proxy_fd = open("/dev/dma_proxy_rx", O_RDWR);
	if (tx_proxy_fd < 1) {
		printf("Unable to open DMA proxy device file");
		return -1;
	}
	printf("Open dma_proxy_tx/rx success\n");
	/* Step 2, map the transmit and receive channels memory into user space so it's accessible
 	 */
	tx_proxy_interface_p = (struct dma_proxy_channel_interface *)mmap(NULL, sizeof(struct dma_proxy_channel_interface),
									PROT_READ | PROT_WRITE, MAP_SHARED, tx_proxy_fd, 0);

	rx_proxy_interface_p = (struct dma_proxy_channel_interface *)mmap(NULL, sizeof(struct dma_proxy_channel_interface),
									PROT_READ | PROT_WRITE, MAP_SHARED, rx_proxy_fd, 0);

	if ((rx_proxy_interface_p == MAP_FAILED) || (tx_proxy_interface_p == MAP_FAILED)) {
    	printf("Failed to mmap\n");
    	return -1;
	}
	printf("mmap success\n");
	/* Create the thread for the transmit processing and then wait a second so the printf output is not
 	 * intermingled with the receive processing
	 */
	pthread_create(&tid, NULL, tx_thread, (void *) &testsz);
	// sleep(1);
	printf("tx success\n");

	/* Initialize the receive buffer so that it can be verified after the transfer is done
	 * and setup the size of the transfer for the receive channel
 	 */
	for (i = 0; i < testsz; i++)
		rx_proxy_interface_p->buffer[i] = 0;

    	rx_proxy_interface_p->length = testsz;

	/* Step 3, Perform the DMA transfer and after it finishes check the status
	 */
    for (i = 0; i < 128; i++) {
    	printf("%d, ", (int) rx_proxy_interface_p->buffer[i]);
    }
    printf("\n");
	ioctl(rx_proxy_fd, 0, &dummy);
	printf("%s.Call rx_proxy_fd ioctl\n", __func__);
	if (rx_proxy_interface_p->status != PROXY_NO_ERROR)
		printf("%s.Proxy rx transfer error\n", __func__);

	/* Verify the data recieved matchs what was sent (tx is looped back to tx)
 	 */
	for (i = 0; i < testsz; i++) {
        	if (tx_proxy_interface_p->buffer[i] !=
            		rx_proxy_interface_p->buffer[i]) {
            		printf("buffer not equal, index = %d\n", i);
            	error = 1;
            	break;
            }
    	}
    if (error == 0) {	
	    for (i = 0; i < 128; i++) {
	    	printf("%d, ", (int) rx_proxy_interface_p->buffer[i]);
	    }
	    printf("\n");
	    printf("Test success\n");
	}
	/* Unmap the proxy channel interface memory and close the device files before leaving
	 */
	munmap(tx_proxy_interface_p, sizeof(struct dma_proxy_channel_interface));
	munmap(rx_proxy_interface_p, sizeof(struct dma_proxy_channel_interface));

	close(tx_proxy_fd);
	close(rx_proxy_fd);
	return 0;
}
