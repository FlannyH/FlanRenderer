#pragma once
#include <d3d12.h>
#include "FlanTypes.h"
#include <cassert>
#include <wrl.h>
#include <vector>
#include "CommonDefines.h"

namespace Flan {
    using Microsoft::WRL::ComPtr;
    class DescriptorHeap;
    struct DescriptorHandle {
        D3D12_CPU_DESCRIPTOR_HANDLE cpu{};
        D3D12_GPU_DESCRIPTOR_HANDLE gpu{};

        constexpr bool is_valid() const { return cpu.ptr != 0; }
        constexpr bool is_shader_visible() const { return gpu.ptr != 0; }
#ifdef _DEBUG
        friend class DescriptorHeap;
        DescriptorHeap* owner = nullptr;
        u32 index = 0;
#endif
    };

    class DescriptorHeap
    {
    public:
        DescriptorHeap() { type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV; }
        DescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE new_type) : type{ new_type } {}
        ~DescriptorHeap() { assert(!heap); }

        bool init(ID3D12Device* device, u32 size, bool is_shader_visible);
        void do_deferred_releases(u32 frame_index);
        [[nodiscard]] DescriptorHandle allocate();
        void free(DescriptorHandle& handle);

        constexpr auto get_type() const { return type; }
        constexpr auto get_cpu_start() const { return cpu_start; }
        constexpr auto get_gpu_start() const { return gpu_start; }
        constexpr auto get_heap_size() const { return capacity; }
        const auto get_heap() const { return heap.Get(); }
        void free_later(const DescriptorHandle& handle);

    private:
        ComPtr<ID3D12DescriptorHeap> heap;
        D3D12_GPU_DESCRIPTOR_HANDLE gpu_start{};
        D3D12_CPU_DESCRIPTOR_HANDLE cpu_start{};
        D3D12_DESCRIPTOR_HEAP_TYPE type;
        u32 capacity;//how many slots are there
        u32 size;//how many slots are allocated
        u32 descriptor_size;//how big is one descriptor
        std::vector<u32> slots_used;
        std::vector<u32> slots_to_be_freed[m_backbuffer_count]{};
        bool shader_visible;
    };
}
