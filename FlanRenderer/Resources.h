#pragma once
#include <d3d12.h>
#include "FlanTypes.h"
#include <wrl.h>
#include <cassert>
#include <mutex>

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
        DescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE new_type) : type{ new_type } {}
        ~DescriptorHeap() { assert(!heap); }

        bool init(ID3D12Device* device, u32 size, bool is_shader_visible);
        void release();
        [[nodiscard]] DescriptorHandle allocate();
        void free(DescriptorHandle& handle);

        constexpr auto get_type() const { return type; }
        constexpr auto get_cpu_start() const { return cpu_start; }
        constexpr auto get_gpu_start() const { return gpu_start; }
        constexpr auto get_heap() const { return heap; }
        constexpr auto get_heap_size() const { return heap_size; }

    private:
        ID3D12DescriptorHeap* heap;
        D3D12_GPU_DESCRIPTOR_HANDLE gpu_start{};
        D3D12_CPU_DESCRIPTOR_HANDLE cpu_start{};
        const D3D12_DESCRIPTOR_HEAP_TYPE type;
        u32 heap_size;
        u32 descriptor_count;
        u32 descriptor_size;
        std::unique_ptr<u32[]> free_slots;
        std::mutex mutex;
    };
}