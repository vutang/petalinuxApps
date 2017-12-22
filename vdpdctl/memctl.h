#include <sys/mman.h>
#include <fcntl.h>
#include <err.h>
#include <errno.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stddef.h>
#include <stdint.h>

#define calc_offset(mem_paddr) (mem_paddr & (getpagesize() - 1))
#define MAPPED_MEM_SIZE 0x0000FFFF 	/*64KB*/

typedef struct memctl_element_t {
	uint32_t mem_paddr;
	uint32_t mem_size;
} memctl_element_t;

/*Mapping mem_paddr (physical address) to a virtual address*/
volatile uint32_t* memctl_mmap(memctl_element_t mem_element);

/*Um-map*/
int memctl_munmap(memctl_element_t mem_element, volatile uint32_t* vaddr_base);

/*Write "value" to "mem_vaddr_base + offset"*/
void memctl_write(volatile uint32_t* vaddr, uint32_t value);

/*Read*/
uint32_t memctl_read(volatile uint32_t* vaddr);
