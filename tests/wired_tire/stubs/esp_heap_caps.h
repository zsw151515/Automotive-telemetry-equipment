#pragma once
#include <cstdlib>
constexpr unsigned MALLOC_CAP_DMA=1,MALLOC_CAP_INTERNAL=2,MALLOC_CAP_SPIRAM=4,MALLOC_CAP_8BIT=8;
inline void* heap_caps_aligned_alloc(size_t,size_t n,unsigned){return std::malloc(n);}
inline void* heap_caps_malloc(size_t n,unsigned){return std::malloc(n);}
inline void heap_caps_free(void*p){std::free(p);}
