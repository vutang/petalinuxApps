/*
 * memctl.c
 *
 *  Created on: Jun 3, 2017
 *      Author: vutt6
 */

#include "memctl.h"
// #include "vdpdctl_utils.h"
//#include "dpd_sys.hpp"

#define MODULE_NAME "memctl()"

volatile uint32_t* memctl_mmap(memctl_element_t mem_element) {
    int fd;
    uint32_t paddr = mem_element.mem_paddr;
    paddr &= ~(getpagesize() - 1);

    fd = open("/dev/mem", O_RDWR);
    if (fd < 0) {
        // logger(ERROR, "could not open /dev/mem!", MODULE_NAME);
        return (volatile uint32_t *) -1;
    }

    /*Virtual address*/
    volatile uint32_t* vaddr_base = (uint32_t*) mmap(NULL, mem_element.mem_size,
                                            PROT_READ | PROT_WRITE, MAP_SHARED, fd, paddr);

    close(fd);

    if (vaddr_base == MAP_FAILED) {
        // logger(ERROR, "could not map memory for axilite bus", MODULE_NAME);
        return (volatile uint32_t*) -1;
    }

    return vaddr_base;
}

int memctl_munmap(memctl_element_t mem_element, volatile uint32_t* vaddr_base) {
	int retval = munmap((void*) vaddr_base, mem_element.mem_size);

	if (retval < 0) {
        // logger(ERROR, "could not unmap memory region for axilite bus", MODULE_NAME);
		return -1;
	}

	return 1;
}

void memctl_write(volatile uint32_t* vaddr, uint32_t value) {
    *(volatile uint32_t*) vaddr = value;
}

uint32_t memctl_read(volatile uint32_t* vaddr) {
    return *(volatile uint32_t*) vaddr;
}
