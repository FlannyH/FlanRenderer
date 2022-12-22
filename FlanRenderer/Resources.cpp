#include "Resources.h"
#include "HelperFunctions.h"
#include "ModelResource.h"
#include "Descriptor.h"

namespace Flan {

    ResourceManager::ResourceManager()
    {
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