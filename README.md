# 6.S081-2020


Finished 6.s081 by the end of March as scheduled,i did learn a lot.By doing labs,I got an overall understanding of operating system,such as virtual address,pagetable,traps,lazy alloction,copy on write,file system and mmap.  
reference: a translation to the videos,very useful. https://www.zhihu.com/column/c_1294282919087964160 

## lab3 pagetable
This is probably the most important part,at first I got confused with it,I don't what the requirements are and why we need to do this,it took me more than three weeks to understand.Finally i got it,anything is clear.Here are some conclusions.
1. Why we need to add a pagetbale into the process that mapped both the kernel and the process address space?
  Because you don't need to switch the pagetable when there's a system call or trap,you can easily copy something from the kernel to user or user to kernel,it's more efficient.
  In the real Linux,we implement the pagetable like this.
2. Now that there is a pagetable mapped both the kernel and the user address space,what's the kernel pagetbale for?
  There are two kinds of region in the kernel pagetable.Linear region and vmalloc area,both are mapped when the pg is established,but the vmalloc region is not added into the user pagetable,it's a lazy allocation.When there's a page fault,the user pg will update it.
  
## lab5 lazy allocation
we often require more space than we actually need,so this may cause waste.we should allocate space only when there is a page fault,it's a way to save space.

## lab6 copy on write
We often do exec after fork,so we don't need to copy the data to the child immediately.If there is a write operation,copy it.
Here are some details:
1. the core of this part is reference count,I use a array to record this.Do not free this page unless the refcount is zero.
2. When fork,use a bit flag to indicate the pages in the father's pagetable are COW pages,erase the writeable flag thus there will be a page fault when write,the child map the same physical address,then add the refcount.
3. check the COW flag and refcount when page fault,if refcount is one,modify the flag.erase the COW and set the writeable flag,use directly.If the refcount is more than 1,copy dat a to a new physical page,decrease the refcount of the old page.

## lab7 buffer cache
This part is about lock contention.
Buffer cache is used for the data transmition between disk and memory.we need to enhance the concurrency.
Here are some details:
1. divide the buffers into several buckets,each bucket with a lock.it's like a hashtable.
2. In each bucket,buffers are organized by a two-way linked list,be careful of the pointer when break and insert the linked list. 

## lab10 mmap
Map the file into the userspace,no need to get into the kernel state unless there is a page fault.
Here are some details:
1. Save the file handler and other infos in the process by using a struct,and reserve several pagse in the user address space.
2. When page fault,get the block number according to the virtual address ,copy data fr0m the buffer cache.
3. When unmap,check whether there is a map because we use lazy allocation,write back the data if the flag asks.
4. modify fork and exit.

