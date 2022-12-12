#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include "FlanTypes.h"

//#define NORMAL_ALLOC
#define DEBUG

#define dynamic_allocate ResourceManager::get_allocator_instance()->allocate
#define dynamic_reallocate ResourceManager::get_allocator_instance()->reallocate
#define dynamic_free ResourceManager::get_allocator_instance()->release

namespace Flan {


    struct MemoryChunk
    {
        std::string name;
        void* pointer;
        u32 size;
        bool is_free;
    };

    class DynamicAllocator
    {
    public:
        DynamicAllocator(const u32 size) { init(size); }
        void init(u32 size);
        void* allocate(u32 size, u32 align = 8);
        void release(void* pointer);
        void* reallocate(void* pointer, u32 size, u32 align = 8);
        void debug_memory();
        std::vector<MemoryChunk> get_memory_chunk_list();

        std::string curr_memory_chunk_label = "unknown";
        std::unordered_map<void*, std::string> memory_labels;

    private:
        void* block_start = nullptr;
        u32 block_size = 0;
        std::unordered_map<void*, std::string> chunk_names;
    };
}