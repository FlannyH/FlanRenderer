#include "Renderer.h"
#include "HelperFunctions.h"

Flan::D3D12_Command::D3D12_Command(ID3D12Device* device, D3D12_COMMAND_LIST_TYPE type) {
    // Create command queue description
    D3D12_COMMAND_QUEUE_DESC queue_desc = {};
    queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queue_desc.Type = type;

    // Create command queue
    throw_if_failed(device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&command_queue)));

    // Create command allocators for each backbuffer
    for (auto& frame : command_frames) {
        throw_if_failed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&frame.command_allocator)));
    }

    // Create command list
    throw_if_failed(device->CreateCommandList(0, type, command_frames[0].command_allocator.Get(), nullptr, IID_PPV_ARGS(&command_list)));
    command_list->Close();

    // Create a fence
    throw_if_failed(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));

    // Create fence event
    fence_event = CreateEventEx(nullptr, nullptr, 0, EVENT_ALL_ACCESS);
}

Flan::D3D12_Command::~D3D12_Command()
{
    assert(!command_queue && !command_list && !fence);
}

void Flan::D3D12_Command::begin_frame() {
    // The previous frame in this object might still be in progress, let's wait
    command_frames[frame_index].wait_fence(fence_event, fence.Get());

    // Reset command allocator
    command_frames[frame_index].command_allocator->Reset();

    // Reset command list
    command_list->Reset(command_frames[frame_index].command_allocator.Get(), nullptr);
}

void Flan::D3D12_Command::end_frame() {
    // Close the command list
    command_list->Close();

    // Execute command list
    ID3D12CommandList* const command_lists[] {
        command_list.Get(),
    };
    command_queue->ExecuteCommandLists(_countof(command_lists), &command_lists[0]);
    ++fence_value;
    command_frames[frame_index].fence_value = fence_value;
    command_queue->Signal(fence.Get(), fence_value);

    // Set frame index to the next buffer, wrapping around to 0 after the last buffer
    frame_index = (frame_index + 1) % m_backbuffer_count;
}

void Flan::RendererDX12::create_hwnd(int width, int height, std::string_view name) {
    // Create window - use GLFW_NO_API, since we're not using OpenGL
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(width, height, name.data(), nullptr, nullptr);
    m_hwnd = glfwGetWin32Window(window);
}

void Flan::RendererDX12::create_fence() {
    // Create fences for each backbuffer
    HANDLE fence_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    ComPtr<ID3D12Fence> fences[m_backbuffer_count];
    UINT64 fence_values[m_backbuffer_count];
    for (unsigned i = 0u; i < m_backbuffer_count; ++i) {
        throw_if_failed(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fences[i])));
        fence_values[i] = 0;
    }
}

void Flan::RendererDX12::create_swapchain(int width, int height) {
    // Declare variables we need for swapchain
    ComPtr<ID3D12DescriptorHeap> render_target_view_heap;
    ComPtr<IDXGISwapChain3> swapchain = nullptr;
    ComPtr<ID3D12Resource> render_targets[m_backbuffer_count];
    UINT render_target_view_descriptor_size;
    D3D12_VIEWPORT viewport;
    D3D12_RECT surface_size;

    // Define surface size
    surface_size.left = 0;
    surface_size.top = 0;
    surface_size.right = width;
    surface_size.bottom = height;

    // Define viewport
    viewport.TopLeftX = 0.0f;
    viewport.TopLeftY = 0.0f;
    viewport.Width = static_cast<float>(width);
    viewport.Height = static_cast<float>(height);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;

    // Create swapchain description
    DXGI_SWAP_CHAIN_DESC1 swapchain_desc{};
    swapchain_desc.BufferCount = m_backbuffer_count;
    swapchain_desc.Width = width;
    swapchain_desc.Height = height;
    swapchain_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapchain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapchain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapchain_desc.SampleDesc.Count = 1;

    // Create swapchain
    IDXGISwapChain1* new_swapchain;
    m_factory->CreateSwapChainForHwnd(command_queue.Get(), m_hwnd, &swapchain_desc, nullptr, nullptr, &new_swapchain);
    HRESULT swapchain_support = new_swapchain->QueryInterface(__uuidof(IDXGISwapChain3), reinterpret_cast<void**>(&new_swapchain));
    if (SUCCEEDED(swapchain_support)) {
        swapchain = static_cast<IDXGISwapChain3*>(new_swapchain);
    }

    if (!swapchain) {
        throw_fatal("Failed to create Direct3D swapchain!");
    }

    frame_index = swapchain->GetCurrentBackBufferIndex();
}

bool Flan::RendererDX12::create_window(int width, int height, std::string_view name) {
    // Create window handle
    create_hwnd(width, height, name);

    // Create render context
    create_swapchain(width, height);
    return true;
}

void Flan::RendererDX12::create_factory() {
    // Create factory
    UINT dxgi_factory_flags = 0;

#if _DEBUG
    // If we're in debug mode, create a debug layer for proper error tracking
    // Note: Errors will be printed in the Visual Studio output tab, and not in the console!
    ComPtr<ID3D12Debug> debug_layer;
    throw_if_failed(D3D12GetDebugInterface(IID_PPV_ARGS(&debug_layer)));
    throw_if_failed(debug_layer->QueryInterface(IID_PPV_ARGS(&m_debug_interface)));
    m_debug_interface->EnableDebugLayer();
    m_debug_interface->SetEnableGPUBasedValidation(true);
    dxgi_factory_flags |= DXGI_CREATE_FACTORY_DEBUG;
    debug_layer->Release();
#endif

    // Result is saved for debugging purposes, so it may be unused
    [[maybe_unused]] HRESULT result = CreateDXGIFactory2(dxgi_factory_flags, IID_PPV_ARGS(&m_factory));
}

void Flan::RendererDX12::create_device() {
    // Create adapter
    ComPtr<IDXGIAdapter1> adapter;
    UINT adapter_index = 0;
    while (m_factory->EnumAdapters1(adapter_index, &adapter) != DXGI_ERROR_NOT_FOUND) {
        DXGI_ADAPTER_DESC1 desc;
        adapter->GetDesc1(&desc);

        // Ignore software renderer - we want a hardware adapter
        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
            continue;
        }

        // We should have a hardware adapter now, but does it support Direct3D 12.0?
        if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&device)))) {
            // Yes it does! We use this one.
            break;
        }

        // It doesn't? Unfortunate, let it go and try another adapter
        device = nullptr;
        adapter->Release();
        adapter_index++;
    }

    if (device == nullptr) {
        throw_fatal("Failed to create Direct3D device!");
    }

#if _DEBUG
    // If we're in debug mode, create the debug device handle
    ComPtr<ID3D12DebugDevice> device_debug;
    throw_if_failed(device->QueryInterface(device_debug.GetAddressOf()));
#endif
}

void Flan::RendererDX12::create_command()
{
    command = D3D12_Command(device.Get(), D3D12_COMMAND_LIST_TYPE_DIRECT);
    if (command.get_command_queue() == nullptr) {
        throw_fatal("Failed to create command queue!");
    }
}

bool Flan::RendererDX12::init() {
    create_factory();
    create_device();
    create_command();
    return true;
}

void Flan::D3D12_Command::CommandFrame::wait_fence(HANDLE fence_event, ID3D12Fence1* fence) {
    assert(fence && fence_event);

    // If the current completed fence value is lower than this frame's fence value,
    // we aren't done with this frame yet. We should wait.
    if (fence->GetCompletedValue() < fence_value) {
        // We create an event to trigger when the fence value reaches this frame's fence value
        fence->SetEventOnCompletion(fence_value, fence_event);

        // And then we wait for that event to trigger
        WaitForSingleObject(fence_event, INFINITE);
    }
}

void Flan::D3D12_Command::CommandFrame::release()
{
    command_allocator.ReleaseAndGetAddressOf();
}

void Flan::D3D12_Command::release()
{
    for (auto i = 0u; i < m_backbuffer_count; ++i) {
        command_frames[i].wait_fence(fence_event, fence.Get());
    }

    if (fence_event) {
        CloseHandle(fence_event);
        fence_event = nullptr;
    }

    command_queue->Release();
    command_list->Release();

    for (auto i = 0u; i < m_backbuffer_count; ++i) {
        command_frames[i].release();
    }
}
