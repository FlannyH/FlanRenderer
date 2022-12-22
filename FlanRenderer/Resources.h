#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <cassert>
#include <mutex>
#include <vector>
#include <map>
#include "DynamicAllocator.h"
#include <glm/glm.hpp>
#include <iostream>
#include <fstream>

// A descriptor heap keeps track of descriptor handles, and manages allocation and deallocation. 

namespace Flan {
    using Microsoft::WRL::ComPtr;

    struct Vertex {
        glm::vec3 position = { 0, 0, 0 };
        glm::vec3 colour = { 1, 1, 1 };
        glm::vec3 normal = { 0, 1, 0 };
        glm::vec3 tangent = { 0, 0, 1 };
        glm::vec2 texcoord0 = { 0, 0 };
        glm::vec2 texcoord1 = { 0, 0 };
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

        template <typename T> 
        T* get_resource(ResourceHandle handle) {
            // todo: add checks for this
            return reinterpret_cast<T*>(loaded_resource_data[handle]);
        }

        inline static DynamicAllocator* get_allocator_instance() {
            if (allocator_instance == nullptr) {
                allocator_instance = new DynamicAllocator(512 MB);
            }
            return allocator_instance;
        }
        inline static DynamicAllocator* allocator_instance;
    private:
        std::map<ResourceHandle, void*> loaded_resource_data;
        std::map<ResourceHandle, ResourceType> loaded_resource_type;
    };

    static void read_file(const std::string& path, size_t& size_bytes, char*& data, const bool silent)
    {
        //Open file
        std::ifstream file_stream(path, std::ios::binary);

        //Is it actually open?
        if (file_stream.is_open() == false)
        {
            if (!silent)
                printf("[ERROR] Failed to open file '%s'!\n", path.c_str());
            size_bytes = 0;
            data = nullptr;
            return;
        }

        //See how big the file is so we can allocate the right amount of memory
        const auto begin = file_stream.tellg();
        file_stream.seekg(0, std::ifstream::end);
        const auto end = file_stream.tellg();
        const auto size = end - begin;
        size_bytes = static_cast<size_t>(size);

        //Allocate memory
        data = static_cast<char*>(dynamic_allocate(static_cast<u32>(size)));

        //Load file data into that memory
        file_stream.seekg(0, std::ifstream::beg);
        const std::vector<unsigned char> buffer(std::istreambuf_iterator<char>(file_stream), {});

        //Is it actually open?
        if (buffer.empty())
        {
            if (!silent)
                printf("[ERROR] Failed to open file '%s'!\n", path.c_str());
            free(data);
            size_bytes = 0;
            data = nullptr;
            return;
        }
        memcpy(data, buffer.data(), size_bytes);
    }
}