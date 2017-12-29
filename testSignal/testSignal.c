/*
 * Placeholder PetaLinux user application.
 *
 * Replace this with your application code
 */
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <semaphore.h>

#define DEV_NAME "/dev/intrDemo"

static sem_t event_sem;
static volatile sig_atomic_t interested_event = 0;

void sig_handler_event1(int sig) {
       interested_event = 1;
       sem_post(&event_sem);
}

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

static void * event_handler_thread_func() {
    while(1){
	    sem_wait(&event_sem);
	    if (interested_event){
            printf("%s,%d, received interested_event signal.\n",__func__, __LINE__);
            interested_event = 0;
	    }
    }
    pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
	int rc, my_pid;

	rc = vip_open_device();

	if (rc) {
		printf("Can not open device. Exit.\n");
		return -1;
	}

	printf("Create Thread to monitor event\n");
	pthread_t event_thread;
	if (pthread_create(&event_thread, NULL, event_handler_thread_func, NULL) != 0){
		printf("Thread create failed%s.\n", strerror(errno));
		return 0;
	}
	sem_init(&event_sem, 0, 0);

	printf("Build up signal struct\n");
	struct sigaction usr_action;
	sigset_t block_mask;
	sigfillset (&block_mask);
	usr_action.sa_handler = sig_handler_event1;
	usr_action.sa_mask = block_mask;//block all signal inside signal handler.
	usr_action.sa_flags = SA_NODEFER;//do not block SIGUSR1 within sig_handler_int.
	sigaction (SIGUSR1, &usr_action, NULL);

	printf("User ioctl to send PID\n");
	my_pid = getpid();
	ioctl(device_fd, 0x111, &my_pid);
	while(1) {}
	vip_close_device();
	return 0;
}


