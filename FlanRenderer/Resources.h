#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <cassert>
#include <mutex>
#include <vector>
#include "Renderer.h"
#include <map>
#include "DynamicAllocator.h"
#include <glm/fwd.hpp>

// A descriptor heap keeps track of descriptor handles, and manages allocation and deallocation. 

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

    struct Vertex {
        glm::vec3 position = { 0, 0, 0 };
        glm::vec3 colour = { 1, 1, 1 };
        glm::vec3 normal = { 0, 1, 0 };
        glm::vec3 tangent = { 0, 0, 1 };
        glm::vec2 texcoord0 = { 0, 0 };
        //glm::vec2 texcoord1;
    };

    struct MeshGPU {
        // Resources
        ID3D12Resource* vertex_buffer_resource;
        ID3D12Resource* index_buffer_resource;

        // Buffer Views
        D3D12_VERTEX_BUFFER_VIEW vertex_buffer_view;
        D3D12_INDEX_BUFFER_VIEW index_buffer_view;

        // Buffer Ranges
        D3D12_RANGE vertex_buffer_range;
        D3D12_RANGE index_buffer_range;

        // Buffer Data
        u8* vertex_buffer_data;
        u8* index_buffer_data;
    };

    struct MeshCPU {
        Vertex* vertices;
        u32* indices;
        size_t n_verts;
        size_t n_indices;
    };

    typedef u64 ResourceHandle;

    enum struct ResourceType {
        Model,
        Texture,
    };

    class ResourceManager {
    public:
        ResourceManager();
        ~ResourceManager();
        ResourceHandle load_mesh(const std::string& path);
        void upload_mesh_to_gpu(ResourceHandle handle, ID3D12Device* device);

        inline static DynamicAllocator* get_allocator_instance() {
            if (allocator_instance == nullptr) {
                allocator_instance = new DynamicAllocator(512 MB);
            }
            return allocator_instance;
        }
        inline static DynamicAllocator* allocator_instance;
    private:
        D3D12_DESCRIPTOR_RANGE1 cbv_srv_uav_range;
        D3D12_DESCRIPTOR_RANGE1 sampler_range;
        DescriptorHeap cbv_srv_uav_heap;
        DescriptorHeap sample_heap;
        std::map<ResourceHandle, void*> loaded_resource_data;
        std::map<ResourceHandle, ResourceType> loaded_resource_type;
    };
}