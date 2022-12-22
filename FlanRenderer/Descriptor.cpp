#include "Descriptor.h"
#include "HelperFunctions.h"

namespace Flan {
    bool DescriptorHeap::init(ID3D12Device* device, u32 new_size, bool is_shader_visible)
    {

        // We need a valid device
        assert(device);

        // Depth Stencil Views and Renter Target Views shouldn't be shader visible
        if (type == D3D12_DESCRIPTOR_HEAP_TYPE_DSV || type == D3D12_DESCRIPTOR_HEAP_TYPE_RTV) {
            is_shader_visible = false;
        }

        // Populate used slots array with -1 (available)
        capacity = new_size;
        size = 0;
        slots_used.resize(capacity);
        for (size_t i = 0; i < capacity; i++)
            slots_used[i] = -1;

        // Create descriptor heap
        D3D12_DESCRIPTOR_HEAP_DESC heap_desc{};
        heap_desc.NumDescriptors = new_size;
        heap_desc.Type = type;
        heap_desc.Flags = is_shader_visible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        throw_if_failed(device->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&heap)));

        // Get the memory locations on CPU (and GPU, if it's shader visible)
        descriptor_size = device->GetDescriptorHandleIncrementSize(type);
        cpu_start = heap->GetCPUDescriptorHandleForHeapStart();
        gpu_start = is_shader_visible ? heap->GetGPUDescriptorHandleForHeapStart() : D3D12_GPU_DESCRIPTOR_HANDLE{ 0 };

        shader_visible = is_shader_visible;

        return true;
    }

    void DescriptorHeap::do_deferred_releases(u32 frame_index)
    {
        // Release resources
        for (auto& index : slots_to_be_freed[frame_index]) {

            slots_used[index] = -1;
        }

        // Clear the array
        slots_to_be_freed[frame_index].clear();
    }

    DescriptorHandle DescriptorHeap::allocate()
    {
        // Sanity check? might have to send my code to therapy
        assert(heap);
        assert(size < capacity);

        // Find the next offset so we can allocate that slot
        // TODO: look into if this can be optimized by just using an index that increases and wraps around the capacity
        u32 found_slot = UINT32_MAX;
        for (u32 index = 0; index < capacity; index++) {
            // If we find an available spot, break and use that one
            if (slots_used[index] == -1) {
                found_slot = index;
                break;
            }
        }

        // Allocate the slot
        assert(found_slot != UINT32_MAX);
        slots_used[found_slot] = size;

        // Get the offset
        const u32 offset{ found_slot * descriptor_size };
        ++size;

        // Create the descriptor handle object
        DescriptorHandle handle;
        handle.cpu.ptr = cpu_start.ptr + offset;
        if (shader_visible) {
            handle.gpu.ptr = gpu_start.ptr + offset;
        }

        return handle;
    }

    void DescriptorHeap::free(DescriptorHandle& handle)
    {
        // Ignore if invalid
        if (!handle.is_valid()) return;

        // Sanity checks!
        assert(heap && size); // Ensure heap exists, and there are allocations in this heap
        assert(handle.owner == this); // Ensure the handle belongs to this heap
        assert(handle.cpu.ptr >= cpu_start.ptr); // Ensure the pointer is inside the cpu memory
        assert((handle.cpu.ptr - cpu_start.ptr) % descriptor_size == 0); // Ensure the pointer is not misaligned
        assert(handle.index < capacity); // Ensure the index is not outside the capacity
        const u32 index = (u32)(handle.cpu.ptr - cpu_start.ptr) / descriptor_size; // Calculate the index
        assert(handle.index == index); // Make sure it matches

        // Defer the deallocations to the next time this heap is used
        slots_to_be_freed->push_back(index);

        handle = {};
    }
}