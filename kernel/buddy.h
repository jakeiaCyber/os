#include <stddef.h>
#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

#define MAX_ORDER 24  // 最大的阶数
#define START 0x87000000  // 起始虚拟地址
#define END 0x88000000    // 终止虚拟地址
typedef unsigned long uintptr_t; // 指针类型


struct buddy {
    int size;  // 内存块的大小
    struct buddy *next;  // 指向下一个同大小的内存块
    struct spinlock lock; // 用于保护块的访问
};

struct buddy_list {
    struct buddy *free_list[MAX_ORDER + 1];  // 每个阶的空闲块链表
    struct spinlock lock; // 用于保护链表的访问
};

void buddy_system_init();
void buddy_init();
void buddy_free(void* ,int);
void *buddy_alloc(int);
