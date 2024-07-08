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

void* memory = (void*)START; //指向分配器起始地址的指针
static struct buddy_list *bl[NCPU]; //指向每个CPU的buddy_list结构体的指针数组。NCPU是系统中CPU的数量


// 初始化伙伴系统，为每个CPU分配并初始化一个buddy_list
void
buddy_system_init()
{
    //为每一个CPU分配一个buddy_list
    for (int i = 0; i < NCPU; i++)
    {
        bl[i] = (struct buddy_list *)kalloc();
        // 分配失败 打印错误信息并退出
        if (bl[i] == 0)
        {
            panic("buddy_list alloc failed");
        }

        // 初始化每个阶的空闲块链表
        for (int j = 0; j <= MAX_ORDER; j++)
        {
            bl[i]->free_list[j] = NULL;
        }

        // 初始化锁
        initlock(&bl[i]->lock, "buddy_list_lock");
    }
}
void 
buddy_init()
{
    // 初始化一个最大阶的空闲块链表
    int order = MAX_ORDER;

    // 将内存块添加到空闲列表中，设置内存块的大小和下一个块指针
    bl[cpuid()]->free_list[order] = (struct buddy *)memory;
    bl[cpuid()]->free_list[order]->size = 1 << order;
    bl[cpuid()]->free_list[order]->next = NULL;

    // 初始化其他所有阶的空闲块链表
    for (int i = 0; i < order; i++) {
        bl[cpuid()]->free_list[i] = NULL;
    }
}

void
*buddy_alloc(int size)
{
    int order = 0;
    // 找到最小的阶数，使得2^blockSizeIndex >= size
    while ((1 << order) < size) order++;
    // 获取当前CPU的锁，防止多个CPU同时访问
    acquire(&bl[cpuid()]->lock);

    // 查找第一个非空足够大的空闲块
    for (int i = order; i <= MAX_ORDER; i++)
    {
        if (bl[cpuid()]->free_list[i] != NULL)
        {
            // 从空闲块链表中移除块
            struct buddy *block = bl[cpuid()]->free_list[i];
            bl[cpuid()]->free_list[i] = block->next;

            // 检查这个块是否比所需的大
            while (i > order) 
            {
                i--;
                // 每次将块分成两个更小的块
                struct buddy *new_block = (struct buddy *)((char *)block + (1 << i));
                new_block->size = 1 << i;
                new_block->next = bl[cpuid()]->free_list[i];
                // 将新块添加到空闲块链表中
                bl[cpuid()]->free_list[i] = new_block;
            }
            // 释放锁并返回块
            release(&bl[cpuid()]->lock);
            return (void *)block;
        }
    }

    // 如果在所有的空闲列表中都没有找到足够大的块，则释放锁并返回NULL
    release(&bl[cpuid()]->lock);
    return NULL;  // 没有足够的空闲内存
}

// 回收不再使用的内存块，并尝试合并伙伴块,减少内存碎片
void
buddy_free(void* memory,int size)
{
    int order = 0;
    while ((1 << order) < size) order++;// 循环直到找到一个足够大以容纳size的order
    struct buddy *block = (struct buddy *)memory;
    block->size = 1 << order;
    
    acquire(&bl[cpuid()]->lock);

    for (int i = order; i < MAX_ORDER; i++)
    {
        uintptr_t buddy_addr = (uintptr_t)memory ^ (1 << i);// 计算伙伴块的地址
        struct buddy *prev = NULL, *curr = bl[cpuid()]->free_list[i];//find

        while (curr != NULL) 
        {
            // 如果找到伙伴块，则合并两个块
            if ((uintptr_t)curr == buddy_addr) 
            {
                if (prev) // 说明当前块不是表头
                {
                    prev->next = curr->next;
                } else {
                    bl[cpuid()]->free_list[i] = curr->next;
                }
                // 合并操作还需要检查两个块的地址，保证合并后的块的地址是较小的那个
                if((uintptr_t)memory>buddy_addr)
                {
                    uintptr_t tmp = (uintptr_t)memory;
                    memory = (void *)buddy_addr;
                    buddy_addr = tmp;
                }
                
                order++; // 合并后的块的阶数增加1
                block = (struct buddy *)memory;
                block->size = 1 << order;//hebin
                break;// 找到并处理了伙伴块，退出循环
            }
            prev = curr;
            curr = curr->next;
        }

        if (curr == NULL) break;
    }

    block->next = bl[cpuid()]->free_list[order];
    bl[cpuid()]->free_list[order] = block;
    release(&bl[cpuid()]->lock);
}