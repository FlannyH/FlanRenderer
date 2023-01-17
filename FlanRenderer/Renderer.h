#pragma once

#define GLFW_EXPOSE_NATIVE_WIN32

#include <iostream>
#include <d3d12.h>
#include <d3d12sdklayers.h>
#include <dxgi1_6.h>
#include <glfw/glfw3.h>
#include <glfw/glfw3native.h>
#include <vector>
#include "glm/glm.hpp"
#include "glm/gtc/quaternion.hpp"
#include <fstream>
#include <chrono>
#include <wrl.h>
#include <string_view>

#include "CommonDefines.h"
#include "Descriptor.h"
#include "FlanTypes.h"
#include "DynamicAllocator.h"
#include "Resources.h"

namespace Flan {
    using Microsoft::WRL::ComPtr;

    class D3D12_Command {
    public:
        D3D12_Command() {}
        D3D12_Command(ID3D12Device* device, D3D12_COMMAND_LIST_TYPE type);
        ~D3D12_Command();
        void begin_frame();
        void end_frame();
        void release();
        const auto get_command_queue() const { return command_queue.Get(); }
        const auto get_command_list() const { return command_list.Get(); }
        constexpr auto const get_frame_index() const { return frame_index; }
    private:
        struct CommandFrame {
            ComPtr<ID3D12CommandAllocator> command_allocator;
            uint64_t fence_value = 0;

            void wait_fence(HANDLE fence_event, ID3D12Fence1* fence);
            void release();
        };
        CommandFrame command_frames[m_backbuffer_count];
        ComPtr<ID3D12CommandQueue> command_queue;
        ComPtr<ID3D12GraphicsCommandList> command_list;
        ComPtr<ID3D12Fence1> fence;
        uint64_t fence_value = 0;
        HANDLE fence_event = nullptr;
        u32 frame_index = 0;
        bool initialized = false;
    };

    const LPCSTR SemanticNames[] {
        "BINORMAL",
        "BLENDINDICES",
        "BLENDWEIGHT",
        "COLOR",
        "NORMAL",
        "POSITION",
        "POSITION_TRANSFORMED",
        "POINT_SIZE",
        "TANGENT",
        "TEXCOORD",
        "FOG",
        "TESSELATION_FACTOR",
        "DEPTH"
    };

    enum ShaderSemantics {
        BINORMAL,
        BLENDINDICES,
        BLENDWEIGHT,
        COLOR,
        NORMAL,
        POSITION,
        POSITION_TRANSFORMED,
        POINT_SIZE,
        TANGENT,
        TEXCOORD,
        FOG,
        TESSELATION_FACTOR,
        DEPTH
    };

    struct ShaderParameter {
        std::string descriptive_name;
        ShaderSemantics semantic; // todo: make this into an enum maybe? might save errors
        size_t size_bytes; // todo: make big map for this, or defines, that works too
        u32 semantic_index;
        DXGI_FORMAT format;
    };

    struct ShaderDescription {
        std::string binary_path;
        std::vector<ShaderParameter> parameters;
    };

    struct TransformBuffer {
        glm::mat4 view_matrix;
        glm::mat4 projection_matrix;
    };

    struct ModelTransformBuffer {
        glm::mat4 model_matrix;
    };

    struct Shader {
        D3D12_SHADER_BYTECODE vs_bytecode{};
        D3D12_SHADER_BYTECODE ps_bytecode{};
    };

    struct Transform {
        glm::vec3 position;
        glm::quat rotation;
        glm::vec3 scale;
        glm::mat4 get_matrix() const {
            glm::mat4 matrix = glm::mat4(1.0f);
            matrix = glm::translate(matrix, position);
            matrix = matrix * glm::mat4_cast(rotation);
            matrix = glm::scale(matrix, scale);
            return matrix;
        }
    };

    struct ModelDrawInfo {
        ResourceHandle model_to_draw;
        Transform transform;
    };

    struct ConstBuffer {
        void* buffer_data;
        size_t buffer_size;
        DescriptorHandle handle;
        D3D12_CONSTANT_BUFFER_VIEW_DESC view_desc;
        ID3D12Resource* resource;
        void update_data(ID3D12Device* device, void* new_data, size_t size);
    };

    class RendererHigh
    {
    public:
        virtual bool create_window(int width, int height, std::string_view name) { return false; }
        virtual bool init(int w, int h) { return false; }
        virtual void begin_frame() {}
        virtual void end_frame() {}
        virtual void draw_model(ModelDrawInfo model) {}
        virtual bool should_close() { return false; }

    protected:
        GLFWwindow* window;
        HWND m_hwnd;
        ResourceManager* m_resource_manager;
    };
    class RendererDX12 : public RendererHigh {
    public:
        RendererDX12(ResourceManager* resource_manager) {
            assert(resource_manager != nullptr);
            m_resource_manager = resource_manager;
        }
        bool create_window(int width, int height, std::string_view name) override;
        bool init(int w, int h) override;
        void begin_frame() override;
        void end_frame() override;
        void draw_model(ModelDrawInfo model) override;
        bool should_close() override;
        ID3D12Device* get_device() const { return m_device.Get(); }
    private:
        void create_hwnd(int width, int height, std::string_view name);
        void create_fence();
        void create_swapchain(int width, int height);
        void create_factory();
        void create_device();
        void create_command();
        void create_pipeline_state_object();
        void create_descriptor_heaps();
        void create_root_signature();
        void free_later(void* data_pointer);
        [[nodiscard]] ConstBuffer create_const_buffer(size_t buffer_size, bool temporary = false);
        Shader load_shader(const std::string& path);
        UINT m_frame_index;
        D3D12_Command m_command;
        ComPtr<IDXGIFactory4> m_factory;
        ComPtr<ID3D12Device> m_device = nullptr;
        ComPtr<ID3D12PipelineState> m_pso = nullptr;
        ComPtr<ID3D12RootSignature> m_root_signature = nullptr;
        DynamicAllocator m_renderer_allocator = DynamicAllocator(8 MB);
        ID3D12PipelineState* m_pipeline_state_object;

        // Camera
        Transform m_camera_transform{
            {0, 0, 0},
            {1, 0, 0, 0},
            {1, 1, 1}
        };
        ConstBuffer m_camera_matrix; // todo: make this a root constant instead of a descriptor table entry

        // Draw queues
        ModelDrawInfo* m_model_queue = nullptr;
        size_t m_model_queue_length = 0;

        // Descriptors
        //D3D12_DESCRIPTOR_RANGE1 cbv_srv_uav_range;
        //D3D12_DESCRIPTOR_RANGE1 sampler_range;
        D3D12_DESCRIPTOR_RANGE1 m_dsv_range;
        D3D12_DESCRIPTOR_RANGE1 m_rtv_range;
        D3D12_DESCRIPTOR_RANGE1 m_cbv_range;
        //DescriptorHeap cbv_srv_uav_heap;
        //DescriptorHeap sample_heap;
        DescriptorHeap m_dsv_heap;
        DescriptorHeap m_rtv_heap;
        DescriptorHeap m_cbv_heap;

        // Swapchain
        [[deprecated]] ComPtr<ID3D12DescriptorHeap> m_render_target_view_heap;
        ComPtr<IDXGISwapChain3> m_swapchain = nullptr;
        ComPtr<ID3D12Resource> m_render_targets[m_backbuffer_count];
        ComPtr<ID3D12Resource> m_depth_targets[m_backbuffer_count];
        [[deprecated]] UINT m_render_target_view_descriptor_size;
        D3D12_VIEWPORT m_viewport;
        D3D12_RECT m_surface_size;
        DescriptorHandle m_rtv_handles[m_backbuffer_count];
        DescriptorHandle m_dsv_handles[m_backbuffer_count];

        // Debug
        std::vector<std::string> m_init_flags;

        [[maybe_unused]] ComPtr<ID3D12Debug1> m_debug_interface;// If we're in release mode, this variable will be unused
        std::vector<void*> m_to_be_deallocated[m_backbuffer_count]{};
    };
}