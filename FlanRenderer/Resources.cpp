#include "Resources.h"
#include "HelperFunctions.h"
#include "ModelResource.h"

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
        memset(slots_used.data(), -1, capacity);

        // Create descriptor heap
        D3D12_DESCRIPTOR_HEAP_DESC heap_desc{};
        heap_desc.NumDescriptors = new_size;
        heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
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
    ResourceManager::ResourceManager()
    {
        cbv_srv_uav_heap = DescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        sample_heap = DescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
    }
    ResourceManager::~ResourceManager()
    {
    }
    ResourceHandle ResourceManager::load_mesh(const std::string& path)
    {
        // Generate a hash for the resource
        ResourceHandle handle = std::hash<std::string>{}(path);

        // Load mesh from gltf
        ModelResource* model = (ModelResource*)dynamic_allocate(sizeof(ModelResource));
        model->load(path, this);

        // Add the resource to the resources map
        loaded_resource_data[handle] = model;
        loaded_resource_type[handle] = ResourceType::Model;

        // Give the handle back to the player
        return handle;
    }
    void ResourceManager::upload_mesh_to_gpu(ResourceHandle handle, ID3D12Device* device )
    {
        // Let's not jump in raw here, stay safe, check the type :D
        assert(loaded_resource_type[handle] == ResourceType::Model);

        // Get the resource
        ModelResource* model = (ModelResource*)loaded_resource_data[handle];

        // Allocate space for GPU mesh data
        model->meshes_gpu = (MeshGPU*)dynamic_allocate(model->n_meshes * sizeof(MeshGPU));

        // Set it to 0 (so we actually get nullptr references
        memset(model->meshes_gpu, 0, model->n_meshes * sizeof(MeshGPU));

        // For each mesh
        for (size_t i = 0; i < model->n_meshes; i++) {
            // Get the resource
            MeshCPU& mesh_cpu = model->meshes_cpu[i];
            MeshGPU& mesh_gpu = model->meshes_gpu[i];

            // Only the GPU needs this data, set the range accordingly
            mesh_gpu.vertex_buffer_range = { 0, 0 };
            mesh_gpu.index_buffer_range = { 0, 0 };

            // Upload the vertex buffer to the GPU
            {
                D3D12_HEAP_PROPERTIES upload_heap_props = {
                    D3D12_HEAP_TYPE_UPLOAD, // The heap will be used to upload data to the GPU
                    D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
                    D3D12_MEMORY_POOL_UNKNOWN, 1, 1 };

                D3D12_RESOURCE_DESC upload_buffer_desc = {
                    D3D12_RESOURCE_DIMENSION_BUFFER, // Can either be texture or buffer, we want a buffer
                    0,
                    sizeof(Vertex) * mesh_cpu.n_verts,
                    1,
                    1,
                    1,
                    DXGI_FORMAT_UNKNOWN, // This is only really useful for textures, so for buffer this is unknown
                    {1, 0}, // Texture sampling quality settings, not important for non-textures, so set it to lowest
                    D3D12_TEXTURE_LAYOUT_ROW_MAJOR, // First left to right, then top to bottom
                    D3D12_RESOURCE_FLAG_NONE,
                };

                throw_if_failed(device->CreateCommittedResource(&upload_heap_props, D3D12_HEAP_FLAG_NONE, &upload_buffer_desc,
                    D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, __uuidof(ID3D12Resource), (void**)&mesh_gpu.vertex_buffer_resource));

                // Bind the vertex buffer, copy the data to it, then unbind the vertex buffer
                throw_if_failed(mesh_gpu.vertex_buffer_resource->Map(0, &mesh_gpu.vertex_buffer_range, reinterpret_cast<void**>(&mesh_gpu.vertex_buffer_data)));
                memcpy_s(mesh_gpu.vertex_buffer_data, sizeof(Vertex) * mesh_cpu.n_verts, mesh_cpu.vertices, sizeof(Vertex) * mesh_cpu.n_verts);
                mesh_gpu.vertex_buffer_resource->Unmap(0, nullptr);

                // Init the buffer view
                mesh_gpu.vertex_buffer_view = D3D12_VERTEX_BUFFER_VIEW{
                    mesh_gpu.vertex_buffer_resource->GetGPUVirtualAddress(),
                    static_cast<u32>(sizeof(Vertex) * mesh_cpu.n_verts),
                    sizeof(Vertex),
                };
            }

            // Upload the index buffer to the GPU
            {
                D3D12_HEAP_PROPERTIES upload_heap_props = {
                    D3D12_HEAP_TYPE_UPLOAD, // The heap will be used to upload data to the GPU
                    D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
                    D3D12_MEMORY_POOL_UNKNOWN, 1, 1 };

                D3D12_RESOURCE_DESC upload_buffer_desc = {
                    D3D12_RESOURCE_DIMENSION_BUFFER, // Can either be texture or buffer, we want a buffer
                    0,
                    sizeof(u32) * mesh_cpu.n_indices,
                    1,
                    1,
                    1,
                    DXGI_FORMAT_UNKNOWN, // This is only really useful for textures, so for buffer this is unknown
                    {1, 0}, // Texture sampling quality settings, not important for non-textures, so set it to lowest
                    D3D12_TEXTURE_LAYOUT_ROW_MAJOR, // First left to right, then top to bottom
                    D3D12_RESOURCE_FLAG_NONE,
                };

                throw_if_failed(device->CreateCommittedResource(&upload_heap_props, D3D12_HEAP_FLAG_NONE, &upload_buffer_desc,
                    D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, __uuidof(ID3D12Resource), (void**)&mesh_gpu.index_buffer_resource));

                // Bind the index buffer, copy the data to it, then unbind the index buffer
                throw_if_failed(mesh_gpu.index_buffer_resource->Map(0, &mesh_gpu.index_buffer_range, reinterpret_cast<void**>(&mesh_gpu.index_buffer_data)));
                memcpy_s(mesh_gpu.index_buffer_data, sizeof(u32) * mesh_cpu.n_indices, mesh_cpu.indices, sizeof(u32) * mesh_cpu.n_indices);
                mesh_gpu.index_buffer_resource->Unmap(0, nullptr);

                // Init the buffer view
                mesh_gpu.index_buffer_view = D3D12_INDEX_BUFFER_VIEW{
                    mesh_gpu.index_buffer_resource->GetGPUVirtualAddress(),
                    static_cast<u32>(sizeof(u32) * mesh_cpu.n_indices),
                    DXGI_FORMAT_R32_UINT,
                };
            }
        }

        printf("je moeder\n");

        return;
    }
}