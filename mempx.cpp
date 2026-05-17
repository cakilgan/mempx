#include "mempx.hpp"

#ifdef __linux__
#include <sys/mman.h>
#endif

mpx::heap_allocator mpx::g_heap;
mpx::virtual_allocator mpx::g_virtual;

mpx::memory mpx::heap_allocator::allocate(size bytes){
	return {::malloc(bytes),bytes,true,memory::type::heap_memory};
}
void mpx::heap_allocator::deallocate(memory& mem){
	assert(mem.raw());
	::free(mem.raw());
	mem.reset();
}

#ifdef __linux__
mpx::memory mpx::virtual_allocator::allocate(size bytes){
	assert(bytes != 0);
    void* ptr = ::mmap(nullptr, bytes,
           PROT_READ | PROT_WRITE,
           MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE,
           -1, 0);
    return memory{ptr,bytes,true,memory::type::virtual_memory};
}
void mpx::virtual_allocator::deallocate(memory& mem){
	assert(mem.is_owner() && !mem.is_invalid());
    if (mem.raw()) {
        ::munmap(mem.raw(),mem.bytes());
		mem.reset();
    }
}
#endif

mpx::memory mpx::malloc(size bytes){
	return g_heap.allocate(bytes);
}
void mpx::free(memory& mem){
	g_heap.deallocate(mem);
}

mpx::memory mpx::vmalloc(size bytes){
	return g_virtual.allocate(bytes);
}
void mpx::vfree(memory& mem){
	g_virtual.deallocate(mem);
}

