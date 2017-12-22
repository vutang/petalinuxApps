#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <errno.h>
#include <sys/mman.h>
#include <fcntl.h>
#include "elf.h"

int rproc_elf_load_segments(char *fw_data, int fw_size, int fd)
{
    struct elf32_hdr *ehdr;
    struct elf32_phdr *phdr;
    int i, ret = 0, virtual_size;
    char *elf_data = fw_data;

    ehdr = (struct elf32_hdr *)elf_data;
    phdr = (struct elf32_phdr *)(elf_data + ehdr->e_phoff);

    /* go through the available ELF segments */
    for (i = 0; i < ehdr->e_phnum; i++, phdr++) {
        u32 da = phdr->p_paddr;
        u32 memsz = phdr->p_memsz;
        u32 filesz = phdr->p_filesz;
        u32 offset = phdr->p_offset;
        void *ptr;

        if (phdr->p_type != PT_LOAD)
            continue;

        printf("phdr: type %d da 0x%x memsz 0x%x filesz 0x%x\n",
                   phdr->p_type, da, memsz, filesz);

        if (filesz > memsz) {
            printf("bad phdr filesz 0x%x memsz 0x%x\n",
                            filesz, memsz);
            ret = -22;
            break;
        }

        if (offset + filesz > fw_size) {
            printf("truncated fw: need 0x%x avail 0x%zx\n",
                    offset + filesz, fw_size);
            ret = -22;
            break;
        }

        /* grab the virtual address for this device address */
        virtual_size = ((memsz / PAGE_SIZE) + 1) * PAGE_SIZE;
        ptr = mmap(NULL, virtual_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, da );
        if (ptr == MAP_FAILED) {
            fprintf(stderr, "mmap64(0x%x@0x%lx) failed (%d)\n",
                    PAGE_SIZE, da, errno);
            ret = -1;
            break;
        }
        if (!ptr) {
            printf("bad phdr da 0x%x mem 0x%x\n", da, memsz);
            ret = -22;
            break;
        }

        /* put the segment where the remote processor expects it */
        if (phdr->p_filesz)
            memcpy(ptr, elf_data + phdr->p_offset, filesz);

        /*
         * Zero out remaining memory for this segment.
         *
         * This isn't strictly required since dma_alloc_coherent already
         * did this for us. albeit harmless, we may consider removing
         * this.
         */
        if (memsz > filesz)
            memset(ptr + filesz, 0, memsz - filesz);

        munmap((void *)ptr, virtual_size);
    }

    return ret;
}