#pragma once

#define GLFW_EXPOSE_NATIVE_WIN32

#include <iostream>
#include <d3d12.h>
#include <d3d12sdklayers.h>
#include <dxgi1_6.h>
#include <glfw/glfw3.h>
#include <glfw/glfw3native.h>
#include <vector>
#include "glm/vec3.hpp"
#include <fstream>
#include <chrono>
#include <wrl.h>
#include <string_view>

#include "FlanTypes.h"

namespace Flan {
    using Microsoft::WRL::ComPtr;
    static constexpr int m_backbuffer_count = 3;

    class D3D12_Command {
    public:
        D3D12_Command(ID3D12Device* device, D3D12_COMMAND_LIST_TYPE type);
        ~D3D12_Command();
        void begin_frame();
        void end_frame();
        void release();
        constexpr auto const get_command_queue() const { return command_queue.Get(); }
        constexpr auto const get_command_list() const { return command_list.Get(); }
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
        HANDLE fence_event;
        u32 frame_index;
    };

    class RendererHigh
    {
    public:
        virtual bool create_window(int width, int height, std::string_view name) { return false; }
        virtual bool init() { return false; }

    protected:
        HWND m_hwnd;
    };
    class RendererDX12 : public RendererHigh {
    public:
        bool create_window(int width, int height, std::string_view name) override;
        bool init() override;
    private:
        void create_hwnd(int width, int height, std::string_view name);
        void create_fence();
        void create_swapchain(int width, int height);
        void create_factory();
        void create_device();
        void create_command();
        UINT frame_index;
        D3D12_Command command;
        ComPtr<IDXGIFactory4> m_factory;
        ComPtr<ID3D12Device> device = nullptr;
        [[maybe_unused]] ComPtr<ID3D12Debug1> m_debug_interface;// If we're in release mode, this variable will be unused
    };
}