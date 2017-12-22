/*
 * Placeholder PetaLinux user application.
 *
 * Replace this with your application code
 */
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h> /*for mmmap()*/

#include <sys/types.h> /*for open()*/
#include <sys/stat.h>
#include <fcntl.h>

#include <unistd.h> /*for read()*/
#include <errno.h> /*for errno*/

#include <string.h>

#include "elf.h"
#include "memctl.h"

#define CPU1_START_ADDR        0xFFFFFFF0
#define CPU1_MEM_BASE		   0x00100000

#define SHARED_DRAM_BASE 	   0x1000000
#define SHARED_DRAM_SIZE	   0x100

#define READ_REG(base, offset) *(base + offset)
#define WRITE_REG(base, offset, value) *(base + offset) = value

volatile int *shared_dram_ptr;

int dpd_start(){
    int fd, elf_fd, ret;
    uint64_t offset, base;
    volatile uint8_t *mm;
    struct stat sb;
    char *buf;

    elf_fd = open("/home/root/cpu1_app.elf", O_RDONLY);
    if (elf_fd < 0) {
        printf("Can't open dpd_file for reading, using default dpd\n");
    }else{
        fstat(elf_fd, &sb);
        buf = malloc(sb.st_size);
        read(elf_fd, buf, sb.st_size);
    }

    offset = CPU1_START_ADDR;
    fd = open("/dev/mem", O_RDWR | O_SYNC );
    if (fd < 0) {
        fprintf(stderr, "open(/dev/mem) failed (%d)\n", errno);
        goto mem_err;
    }
    
    if(elf_fd >= 0){
    	printf("Start Write ELF\n");
        rproc_elf_load_segments(buf, sb.st_size, fd);
    }
    
    base = offset & PAGE_MASK;
    offset &= ~PAGE_MASK;

    mm = mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, base);
    if (mm == MAP_FAILED) {
        fprintf(stderr, "mmap64(0x%x@0x%lx) failed (%d)\n",
                PAGE_SIZE, base, errno);
        goto map_err;
    }

    *(volatile uint32_t *)(mm + offset) = CPU1_MEM_BASE;
    printf("CPU1_START_ADDR: %x\n", *(volatile uint32_t *)(mm + offset));

    munmap((void *)mm, PAGE_SIZE);
map_err:
    close(fd);
mem_err:
    if(elf_fd >= 0){    
        free(buf);
        close(elf_fd);
    }

    return 0;
}

void cpu0_wait() {
    while (*shared_dram_ptr == 1) {};
    return;
}

void cpu0_signal() {
    *shared_dram_ptr = 1;
    return;
}

int main(int argc, char *argv[])
{   
    int i = 0;

    /*Maping mem for read/write*/	
	memctl_element_t memelement_dpd_usr = {SHARED_DRAM_BASE, SHARED_DRAM_SIZE};
	// memctl_element_t memelement_cpu1_base = {CPU1_MEM_BASE, SHARED_DRAM_SIZE};

	if ((shared_dram_ptr = memctl_mmap(memelement_dpd_usr)) < 0) {
		printf("[ERROR] Can not map axi_dpd_ctl_ptr\n");
	}
	// if ((cpu1_mem_ptr = memctl_mmap(memelement_cpu1_base)) < 0) {
	// 	printf("[ERROR] Can not map axi_dpd_ctl_ptr\n");
	// }

    /*Writing CPU1 ELF to DRAM*/
	printf("[CPU0] - Write CPU1 ELF to DRAm & Execute\n");
    dpd_start();

    /*Init Semaphore*/
    *shared_dram_ptr = 0;

    /*Init*/
    *(shared_dram_ptr + 1) = i;
    *(shared_dram_ptr + 2) = i + 1;

    /*Finite While Loop*/
    printf("[CPU0] - Start Finite Loop\n");
    while (1) {
        i++;
        cpu0_wait();
        printf("[CPU0] %d + %d = %d\n", *(shared_dram_ptr + 1), *(shared_dram_ptr + 2), *(shared_dram_ptr + 3));
        *(shared_dram_ptr + 1) = i;
        *(shared_dram_ptr + 2) = i + 1;
        sleep(1);
        cpu0_signal();
    }   


	usleep(500);

	memctl_munmap(memelement_dpd_usr, shared_dram_ptr);
	// memctl_munmap(memelement_cpu1_base, cpu1_mem_ptr);

	return 0;
}


