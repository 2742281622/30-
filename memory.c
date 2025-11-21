#include "bootpack.h"

#define EFLAGS_AC_BIT			0x00040000
#define CR0_CACHE_DISABLE	0x60000000

unsigned int memtest(unsigned int start, unsigned int end) 
{
	char flg486 = 0;
	unsigned int eflg, cr0, i;

	/* 确认CPU是386还是486以上的 */
	eflg = io_load_eflags();
	eflg |= EFLAGS_AC_BIT; /* AC-bit = 1 */
	io_store_eflags(eflg);
	eflg = io_load_eflags();
	if ((eflg & EFLAGS_AC_BIT) != 0) {
		/* 如果是386，即使设定AC=1，AC的值还会自动回到0 */
		flg486 = 1;
	}

	eflg &= ~EFLAGS_AC_BIT; /* AC-bit = 0 */
	io_store_eflags(eflg);

	if (flg486 != 0) {
		cr0 = load_cr0();
		cr0 |= CR0_CACHE_DISABLE; /* 禁止缓存 */ 
		store_cr0(cr0);
	}

	i = memtest_sub(start, end);

	if (flg486 != 0) {
		cr0 = load_cr0();
		cr0 &= ~CR0_CACHE_DISABLE; /* 允许缓存 */
		store_cr0(cr0);
	}

	return i;
}

int memory_init(struct MEM_CONTROL *man)
{
    int i;
    unsigned int total_size = 0;
    if (man == 0)
    {
        return -1;
    }                                               // 如果man为空，返回-1
    total_size = memtest(MEMORY_BEGIN, MEMORY_END); // 测试内存大小
    if (total_size == 0)
    {
        return -1;
    } // 如果测试内存大小为0，返回-1
    man->total_size = total_size - MEMORY_BEGIN + 0x0009e000;
    man->free_size = total_size - MEMORY_BEGIN + 0x0009e000;
    man->free = &man->frees[0];
    man->use = &man->uses[0];
    for (i = 0; i < MEMMAN_FREES - 1; i++)
    {
        man->frees[i].next = 0;
        man->frees[i].flag = 0;
    }
    man->frees[MEMMAN_FREES - 1].flag = 0;

    for (i = 0; i < MEMMAN_FREES - 1; i++)
    {
        man->uses[i].next = &man->uses[i + 1];
        man->uses[i].flag = 0;
    }
    man->uses[MEMMAN_FREES - 1].next = 0;
    man->uses[MEMMAN_FREES - 1].flag = 0;

    man->frees[0].addr = 0x00001000;
    man->frees[0].size = 0x0009e000;// 632KB
    man->frees[0].flag = 1;
    man->frees[0].next = &man->frees[1];


    man->frees[1].addr = MEMORY_BEGIN;
    man->frees[1].size = total_size - MEMORY_BEGIN;
    man->frees[1].flag = 1;
    man->frees[1].next = 0;
    return 0;
}

struct MEM_USE_INFO *memory_alloc(struct MEM_CONTROL *man, unsigned int size, unsigned int id)
{
    if (man == 0)
    {
        return 0;
    } // 如果man为空，返回0
    if (man->free_size < size)
    {
        return 0;
    } // 如果空闲内存大小小于请求大小，返回0
    struct MEM_FREE *free = man->free;
    struct MEM_FREE *free_prev = 0;  // 记录free的前一个节点
    struct MEM_FREE *best_free = 0;
    struct MEM_FREE *prev_best = 0;  // 记录best_free的前一个节点
    struct MEM_USE_INFO *use = man->use;
    while(use)// 遍历已使用块表,找到最后一个已使用块
    {
        if (use->flag == 0)
        {
            break;
        }
        use = use->next;
    }

    while (free)
    { // 遍历空闲块链表
        if (free->size >= size && (!best_free || free->size < best_free->size) && free->flag == 1)
        {
            prev_best = free_prev;  // 记录当前最佳块的前一个节点
            best_free = free;
            if (best_free->size == size)
            {
                break;
            }
        }
        free_prev = free;
        free = free->next;
    }

    if (best_free)
    {
        use->addr = best_free->addr;

        best_free->addr += size;
        best_free->size -= size;

        use->size = size;
        use->flag = 1;
        use->id = id;

        man->free_size -= use->size;
        if (best_free->size == 0)
        { // 如果空闲块大小为0，标记为未使用
            best_free->flag = 0;
            if (prev_best) // 如果前一个空闲块存在
            {
                prev_best->next = best_free->next;
            }
            else // 如果前一个空闲块不存在
            {
                man->free = best_free->next;
            }
        }
        return use;
    }

    return 0;
}

int memory_free(struct MEM_CONTROL *man, struct MEM_USE_INFO *use)
{
    if (use->flag == 0)
    {
        return -1;
    } // 如果use未使用，返回-1
    if (man == 0 || use == 0)
    {
        return -1;
    } // 如果man或use为空，返回-1
    int i;
    // 修改已使用块为空闲块
    use->flag = 0;
    use->id = 0;
    man->free_size += use->size;
    // 首先，从use链表中移除当前节点
    struct MEM_USE_INFO *prev_use = 0;
    struct MEM_USE_INFO *current_use = man->use;

    // 查找当前use节点在链表中的位置
    while (current_use && current_use != use) {
        prev_use = current_use;
        current_use = current_use->next;
    }

    // 如果找到了，调整链表连接
    if (current_use) {
        if (prev_use) {
            // 如果不是第一个节点，直接跳过当前节点
            prev_use->next = use->next;
        } else {
            // 如果是第一个节点，更新头指针
            man->use = use->next;
        }
    }
    // 遍历空闲块链表，找到合适的位置插入空闲块
    struct MEM_FREE *free = man->free;
    struct MEM_FREE *prev_free = 0; // 指向前一个空闲块
    while (free)
    {
        if (free->addr > use->addr)
        {
            break;
        }
        prev_free = free;
        free = free->next;
    }
    // 插入空闲块
    if (prev_free) // 如果前一个空闲块存在
    {
        if(free == 0){//如果后一个空闲块不存在
            if(prev_free->addr + prev_free->size == use->addr){//如果前一个空闲块与use相邻
                prev_free->size += use->size;
                return 0;
            }
            else{//如果前一个空闲块与use不相邻
                for (i = 0; i < MEMMAN_FREES; i++)
                {
                    if (man->frees[i].flag == 0)
                    { // 如果空闲块未使用
                        man->frees[i].addr = use->addr;
                        man->frees[i].size = use->size;
                        man->frees[i].flag = 1;
                        break;
                    }
                }
                if (i >= MEMMAN_FREES - 1 && man->frees[i].flag == 1)
                { // 如果没有找到合适的位置
                    return -1;
                }
                prev_free->next = &man->frees[i];
                man->frees[i].next = 0;
                return 0;
            }
        }
        else{//如果后一个空闲块存在
        if (prev_free->addr + prev_free->size == use->addr) // 如果前一个空闲块与use相邻
        {
            prev_free->size += use->size;
            if (prev_free->addr + prev_free->size == free->addr)// 如果与后一个空闲块相邻
            {
                prev_free->size += free->size;
                free->flag = 0; // 标记为未使用
            }
            return 0;
        }
        else // 如果前一个空闲块不相邻
        {
            if (use->addr + use->size == free->addr) // 如果与后一个空闲块相邻
            {
                free->addr = use->addr;
                free->size += use->size;
                return 0;
            }
            else // 如果与后一个空闲块不相邻
            {
                for (i = 0; i < MEMMAN_FREES; i++)
                {
                    if (man->frees[i].flag == 0)
                    { // 如果空闲块未使用
                        man->frees[i].addr = use->addr;
                        man->frees[i].size = use->size;
                        man->frees[i].flag = 1;
                        break;
                    }
                }
                if (i >= MEMMAN_FREES - 1 && man->frees[i].flag == 1)
                { // 如果没有找到合适的位置
                    return -1;
                }
                // 如果插入到中间或最后一个空闲块
                    prev_free->next = &man->frees[i];
                    man->frees[i].next = free;
                    return 0;
            }
        }
        }
    }
    else // 如果前一个空闲块不存在
    {
        if(use->addr + use->size == free->addr){//如果use与后一个空闲块相邻
            free->addr = use->addr;
            free->size += use->size;
            return 0;
        }else{//如果use与后一个空闲块不相邻
                for (i = 0; i < MEMMAN_FREES; i++)
                {
                    if (man->frees[i].flag == 0)
                    { // 如果空闲块未使用
                        man->frees[i].addr = use->addr;
                        man->frees[i].size = use->size;
                        man->frees[i].flag = 1;
                        break;
                    }
                }
                if (i >= MEMMAN_FREES - 1 && man->frees[i].flag == 1)
                { // 如果没有找到合适的位置
                    return -1;
                }
                 // 如果插入到第一个空闲块
                    man->free = &man->frees[i];
                    man->frees[i].next = free;
                    return 0;
        }
    }
    return -1;
}

struct MEM_USE_INFO *memory_alloc_4k(struct MEM_CONTROL *man, unsigned int size, unsigned int id)
{
    struct MEM_USE_INFO *use;
    size =  (size + 0xfff) & 0xfffff000;// 4KB对齐,向上取整
    use = memory_alloc(man, size, id);
    if(use)//如果分配成功
    {
        return use;
    }
    return 0;
}
