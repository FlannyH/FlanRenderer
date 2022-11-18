#include "Resources.h"
#include "HelperFunctions.h"

namespace Flan {
    bool DescriptorHeap::init(ID3D12Device* device, u32 new_size, bool is_shader_visible)
    {
        // Lock the mutex, we want only one thread to access this at at time
        std::lock_guard lock{ mutex };

        // We need a valid device
        assert(device);
        
        // Depth Stencil Views and Renter Target Views shouldn't be shader visible
        if (type == D3D12_DESCRIPTOR_HEAP_TYPE_DSV || type == D3D12_DESCRIPTOR_HEAP_TYPE_RTV) {
            is_shader_visible = false;
        }

        release();

        // Create descriptor heap
        D3D12_DESCRIPTOR_HEAP_DESC heap_desc{};
        heap_desc.NumDescriptors = size;
        heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        heap_desc.Flags = is_shader_visible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        throw_if_failed(device->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&heap)));

        // Create free slots array
        free_slots = std::move(std::make_unique<u32[]>(new_size));
        capacity = new_size;
        size = 0;
        for (u32 i = 0; i < size; ++i) {
            free_slots[i] = i;
        }

        descriptor_size = device->GetDescriptorHandleIncrementSize(type);
        cpu_start = heap->GetCPUDescriptorHandleForHeapStart();
        gpu_start = is_shader_visible ? heap->GetGPUDescriptorHandleForHeapStart() : D3D12_GPU_DESCRIPTOR_HANDLE{ 0 };

        return true;
    }

    void DescriptorHeap::release()
    {
    }

    DescriptorHandle DescriptorHeap::allocate()
    {
        // Lock the mutex
        std::lock_guard lock{ mutex };

        // Sanity check? might have to send my code to therapy
        assert(heap);
        assert(size < capacity);

        // Find the next offset so we can allocate that slot
        const u32 index{ free_handles[size] };
        const u32 offset{ index * descriptor_size };
        ++size;

        // Create the descriptor handle object
        DescriptorHandle handle;
        handle.cpu.ptr = cpu_start.ptr + offset;
        if (is_shader_visible()) {
            handle.gpu.ptr = gpu_start.ptr + offset;
        }
    }

    void DescriptorHeap::free(DescriptorHandle& handle)
    {
        // Ignore if invalid
        if (!handle.is_valid()) return;

        // Lock the mutex
        std::lock_guard lock{ mutex };

        // Sanity checks!
        assert(heap && size); // Ensure heap exists, and there are allocations in this heap
        assert(handle.owner == this); // Ensure the handle belongs to this heap
        assert(handle.cpu.ptr >= cpu_start.ptr); // Ensure the pointer is inside the cpu memory
        assert((handle.cpu.ptr - cpu_start.ptr) % descriptor_size == 0); // Ensure the pointer is not misaligned
        assert(handle.index < capacity); // Ensure the index is not outside the capacity
        const u32 index = (u32)(handle.cpu.ptr - cpu_start.ptr) / descriptor_size; // Calculate the index
        assert(handle.index == index); // Make sure it matches

        handle = {};
    }
}