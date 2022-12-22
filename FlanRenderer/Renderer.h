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
#include "FlanTypes.h"
#include "DynamicAllocator.h"
#include "Resources.h"
#include "Descriptor.h"

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
        glm::mat4 model_view_matrix;
        glm::mat4 projection_matrix;
    };

    struct Shader {
        D3D12_SHADER_BYTECODE vs_bytecode{};
        D3D12_SHADER_BYTECODE ps_bytecode{};
    };

    struct Transform {
        glm::vec3 position;
        glm::quat rotation;
        glm::vec3 scale;
    };

    struct ModelDrawInfo {
        ResourceHandle model_to_draw;
        Transform transform;
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
        ID3D12Device* get_device() const { return device.Get(); }
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
        void create_const_buffer();
        Shader load_shader(const std::string& path);
        UINT frame_index;
        D3D12_Command command;
        ComPtr<IDXGIFactory4> m_factory;
        ComPtr<ID3D12Device> device = nullptr;
        ComPtr<ID3D12PipelineState> pso = nullptr;
        ComPtr<ID3D12RootSignature> root_signature = nullptr;
        DynamicAllocator renderer_allocator = DynamicAllocator(8 MB);
        ID3D12PipelineState* pipeline_state_object;

        // Draw queues
        ModelDrawInfo* model_queue = nullptr;
        size_t model_queue_length = 0;

        // Descriptors
        D3D12_DESCRIPTOR_RANGE1 cbv_srv_uav_range;
        D3D12_DESCRIPTOR_RANGE1 sampler_range;
        D3D12_DESCRIPTOR_RANGE1 rtv_range;
        DescriptorHeap cbv_srv_uav_heap;
        DescriptorHeap sample_heap;
        DescriptorHeap rtv_heap;

        // Swapchain
        [[deprecated]] ComPtr<ID3D12DescriptorHeap> render_target_view_heap;
        ComPtr<IDXGISwapChain3> swapchain = nullptr;
        ComPtr<ID3D12Resource> render_targets[m_backbuffer_count];
        [[deprecated]] UINT render_target_view_descriptor_size;
        D3D12_VIEWPORT viewport;
        D3D12_RECT surface_size;
        DescriptorHandle rtv_handles[m_backbuffer_count];

        // Debug
        std::vector<std::string> init_flags;

        [[maybe_unused]] ComPtr<ID3D12Debug1> m_debug_interface;// If we're in release mode, this variable will be unused
    };
}