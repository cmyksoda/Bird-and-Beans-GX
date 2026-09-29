// SPDX-License-Identifier: GPL-3.0-only
#ifndef BB_VM_H
#define BB_VM_H
#include <stdint.h>
#include <stddef.h>
#define RAM_BASE 0x02000000u
#define RAM_SIZE 0x00400000u
typedef struct VM VM;
struct VM {
    uint8_t *mem;
    uint32_t r[16], pc, instructions;
    unsigned n,z,c,v;
    int fault;
    char error[160];
    int (*hook)(VM *, uint32_t);
    void *user;
};
uint32_t vm_read(VM *, uint32_t, unsigned);
void vm_write(VM *, uint32_t, uint32_t, unsigned);
int vm_call(VM *, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
#endif
