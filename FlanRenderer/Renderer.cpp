#include "Renderer.h"
#include "HelperFunctions.h"
#include "ModelResource.h"

namespace Flan {
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

        initialized = true;
    }

    Flan::D3D12_Command::~D3D12_Command()
    {
        //if (initialized)
        //    assert(!command_queue && !command_list && !fence);
    }

    void Flan::D3D12_Command::begin_frame() {
        // Make sure we are initialized
        assert(initialized);

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
        ID3D12CommandList* const command_lists[]{
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
        window = glfwCreateWindow(width, height, name.data(), nullptr, nullptr);
        m_hwnd = glfwGetWin32Window(window);
    }

    void Flan::RendererDX12::create_fence() {
        // Create fences for each backbuffer
        assert(device);
        HANDLE fence_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        ComPtr<ID3D12Fence> fences[m_backbuffer_count];
        UINT64 fence_values[m_backbuffer_count];
        for (unsigned i = 0u; i < m_backbuffer_count; ++i) {
            throw_if_failed(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fences[i])));
            fence_values[i] = 0;
        }
    }

    void Flan::RendererDX12::create_swapchain(int width, int height) {
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
        IDXGISwapChain1* new_swapchain = nullptr;
        m_factory->CreateSwapChainForHwnd(command.get_command_queue(), m_hwnd, &swapchain_desc, nullptr, nullptr, &new_swapchain);
        HRESULT swapchain_support = new_swapchain->QueryInterface(__uuidof(IDXGISwapChain3), reinterpret_cast<void**>(&new_swapchain));
        if (SUCCEEDED(swapchain_support)) {
            swapchain = static_cast<IDXGISwapChain3*>(new_swapchain);
        }

        if (!swapchain) {
            throw_fatal("Failed to create Direct3D swapchain!");
        }

        // Create RTV for each frame
        for (UINT i = 0; i < m_backbuffer_count; i++) {
            rtv_handles[i] = rtv_heap.allocate();
            throw_if_failed(swapchain->GetBuffer(i, IID_PPV_ARGS(&render_targets[i])));
            device->CreateRenderTargetView(render_targets[i].Get(), nullptr, rtv_handles[i].cpu);
        }

        frame_index = swapchain->GetCurrentBackBufferIndex();
    }

    bool Flan::RendererDX12::create_window(int width, int height, std::string_view name) {
        // Create window handle
        create_hwnd(width, height, name);

        // Create render context
        create_swapchain(width, height);
        create_root_signature();
        create_pipeline_state_object();
        return true;
    }

    void Flan::RendererDX12::create_factory() {
        // Create factory
        UINT dxgi_factory_flags = 0;

#if _DEBUG
        // If we're in debug mode, create a debug layer for proper error tracking
        // Note: Errors will be printed in the Visual Studio output tab, and not in the console!
        ID3D12Debug* debug_layer;
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

    void Flan::RendererDX12::create_pipeline_state_object()
    {
        assert(device);

        // todo: remove magic hardcoded shader description
        ShaderDescription shader_description;
        shader_description.binary_path = "Assets/Shaders/test";
        shader_description.parameters.push_back({ "Vertex Position", POSITION, 12, 0, DXGI_FORMAT_R32G32B32_FLOAT });
        shader_description.parameters.push_back({ "Vertex Colour", COLOR, 12, 0,  DXGI_FORMAT_R32G32B32_FLOAT });
        shader_description.parameters.push_back({ "Vertex Normal", NORMAL, 12, 0, DXGI_FORMAT_R32G32B32_FLOAT });
        shader_description.parameters.push_back({ "Vertex Tangent", TANGENT, 12, 0, DXGI_FORMAT_R32G32B32_FLOAT });
        shader_description.parameters.push_back({ "Vertex Texcoord 0", TEXCOORD, 8, 0,  DXGI_FORMAT_R32G32_FLOAT });
        shader_description.parameters.push_back({ "Vertex Texcoord 1", TEXCOORD, 8, 1,  DXGI_FORMAT_R32G32_FLOAT });

        // Create input assembly - this defines what our shader input is
        size_t offset = 0;
        D3D12_INPUT_ELEMENT_DESC input_element_descs[32]{};
        for (size_t i = 0; i < shader_description.parameters.size(); i++) {
            auto& param = shader_description.parameters[i];
            input_element_descs[i].SemanticName = SemanticNames[param.semantic];
            input_element_descs[i].SemanticIndex = param.semantic_index;
            input_element_descs[i].Format = param.format;
            input_element_descs[i].InputSlot = 0;
            input_element_descs[i].AlignedByteOffset = offset; offset += param.size_bytes;
            input_element_descs[i].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
            input_element_descs[i].InstanceDataStepRate = 0;
        }

        // Load shader
        Shader shader = load_shader(shader_description.binary_path);

        // Create pipeline state description
        ID3D12PipelineState* pipeline_state_object;
        D3D12_GRAPHICS_PIPELINE_STATE_DESC pipeline_state_description{};
        pipeline_state_description.InputLayout = { input_element_descs, (u32)shader_description.parameters.size() };
        pipeline_state_description.pRootSignature = root_signature.Get();
        pipeline_state_description.VS = shader.vs_bytecode;
        pipeline_state_description.PS = shader.ps_bytecode;

        // Create rasterizer description
        D3D12_RASTERIZER_DESC raster_desc;
        raster_desc.FillMode = D3D12_FILL_MODE_SOLID;
        raster_desc.CullMode = D3D12_CULL_MODE_NONE;
        raster_desc.FrontCounterClockwise = FALSE;
        raster_desc.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
        raster_desc.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
        raster_desc.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
        raster_desc.DepthClipEnable = TRUE;
        raster_desc.MultisampleEnable = FALSE;
        raster_desc.AntialiasedLineEnable = FALSE;
        raster_desc.ForcedSampleCount = 0;
        raster_desc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
        pipeline_state_description.RasterizerState = raster_desc;
        pipeline_state_description.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

        // Setup color and alpha blend modes
        D3D12_BLEND_DESC blend_desc{};
        blend_desc.AlphaToCoverageEnable = FALSE;
        blend_desc.IndependentBlendEnable = FALSE;
        constexpr D3D12_RENDER_TARGET_BLEND_DESC default_render_target_blend_desc = {
            FALSE,
            FALSE,
            D3D12_BLEND_ONE,
            D3D12_BLEND_ZERO,
            D3D12_BLEND_OP_ADD,
            D3D12_BLEND_ONE,
            D3D12_BLEND_ZERO,
            D3D12_BLEND_OP_ADD,
            D3D12_LOGIC_OP_NOOP,
            D3D12_COLOR_WRITE_ENABLE_ALL,
        };

        for (auto& i : blend_desc.RenderTarget)
            i = default_render_target_blend_desc;

        pipeline_state_description.BlendState = blend_desc;


        // Set up depth/stencil state
        pipeline_state_description.DepthStencilState.DepthEnable = FALSE;
        pipeline_state_description.DepthStencilState.StencilEnable = FALSE;
        pipeline_state_description.SampleMask = UINT_MAX;

        // Setup render target output
        pipeline_state_description.NumRenderTargets = 1;
        pipeline_state_description.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
        pipeline_state_description.SampleDesc.Count = 1;

        // Create graphics pipeline state
        try {
            throw_if_failed(device->CreateGraphicsPipelineState(&pipeline_state_description, IID_PPV_ARGS(&pipeline_state_object)));
        }
        catch ([[maybe_unused]] std::exception& e) {
            puts("Failed to create Graphics Pipeline");
        }
    }

    void RendererDX12::create_descriptor_heaps()
    {
        // Create descriptor heaps
        cbv_srv_uav_heap = DescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        sample_heap = DescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
        rtv_heap = DescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        cbv_srv_uav_heap.init(device.Get(), 3, true);
        sample_heap.init(device.Get(), 1, true);
        rtv_heap.init(device.Get(), m_backbuffer_count, true);
    }

    void Flan::RendererDX12::create_root_signature()
    {
        assert(device);

        // todo: make this more flexible and less magic number-y

        // Create descriptor ranges
        D3D12_DESCRIPTOR_RANGE1 ranges[3];
        ranges[0].BaseShaderRegister = 0;
        ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
        ranges[0].NumDescriptors = 1;
        ranges[0].RegisterSpace = 0;
        ranges[0].OffsetInDescriptorsFromTableStart = 0;
        ranges[0].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE;

        ranges[1].BaseShaderRegister = 0;
        ranges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        ranges[1].NumDescriptors = 1;
        ranges[1].RegisterSpace = 0;
        ranges[1].OffsetInDescriptorsFromTableStart = 1;
        ranges[1].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE;

        ranges[2].BaseShaderRegister = 0;
        ranges[2].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
        ranges[2].NumDescriptors = 1;
        ranges[2].RegisterSpace = 0;
        ranges[2].OffsetInDescriptorsFromTableStart = 2;
        ranges[2].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE;

        // Bind the descriptor ranges to the descriptor table of the root signature
        D3D12_ROOT_PARAMETER1 root_parameters[1];
        root_parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        root_parameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
        root_parameters[0].DescriptorTable.NumDescriptorRanges = 3;
        root_parameters[0].DescriptorTable.pDescriptorRanges = ranges;

        // Describe the root signature 
        D3D12_VERSIONED_ROOT_SIGNATURE_DESC root_signature_desc;
        root_signature_desc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
        root_signature_desc.Desc_1_1.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
        root_signature_desc.Desc_1_1.NumParameters = 1;
        root_signature_desc.Desc_1_1.pParameters = root_parameters;
        root_signature_desc.Desc_1_1.NumStaticSamplers = 0;
        root_signature_desc.Desc_1_1.pStaticSamplers = nullptr;

        // Now let's actually create the root signature
        ComPtr<ID3DBlob> signature;
        ComPtr<ID3DBlob> error;
        try {
            throw_if_failed(D3D12SerializeVersionedRootSignature(&root_signature_desc, &signature, &error));
            throw_if_failed(device->CreateRootSignature(0, signature->GetBufferPointer(),
                signature->GetBufferSize(), IID_PPV_ARGS(&root_signature)));
            root_signature->SetName(L"Hello Triangle Root Signature");
        }
        catch ([[maybe_unused]] std::exception& e) {
            auto err_str = static_cast<const char*>(error->GetBufferPointer());
            std::cout << err_str;
            error->Release();
            error = nullptr;
        }

        if (signature) {
            signature->Release();
            signature = nullptr;
        }
    }

    void Flan::RendererDX12::create_const_buffer()
    {
        assert(device);

        // Allocate memory for the buffer in CPU space
        TransformBuffer* const_buffer_data = (TransformBuffer*)renderer_allocator.allocate(sizeof(TransformBuffer));

        // Get a handle where we can put the constant buffer
        DescriptorHandle const_buffer_handle = cbv_srv_uav_heap.allocate();

        // Define upload properties
        D3D12_HEAP_PROPERTIES upload_heap_props = {
            D3D12_HEAP_TYPE_UPLOAD, // The heap will be used to upload data to the GPU
            D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
            D3D12_MEMORY_POOL_UNKNOWN, 1, 1
        };

        // Define what we want to upload
        D3D12_RESOURCE_DESC upload_buffer_desc = {
            D3D12_RESOURCE_DIMENSION_BUFFER, // Can either be texture or buffer, we want a buffer
            0,
            (sizeof(const_buffer_data) | 0xFF) + 1, // Constant buffers must be 256-byte aligned
            1,
            1,
            1,
            DXGI_FORMAT_UNKNOWN, // This is only really useful for textures, so for buffer this is unknown
            {1, 0}, // Texture sampling quality settings, not important for non-textures, so set it to lowest
            D3D12_TEXTURE_LAYOUT_ROW_MAJOR, // First left to right, then top to bottom
            D3D12_RESOURCE_FLAG_NONE,
        };

        // Create a constant buffer resource
        ComPtr<ID3D12Resource> const_buffer_resource;
        throw_if_failed(device->CreateCommittedResource(
            &upload_heap_props,
            D3D12_HEAP_FLAG_NONE,
            &upload_buffer_desc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            __uuidof(ID3D12Resource),
            (void**)&const_buffer_resource
        ));

        // Create a constant buffer description
        D3D12_CONSTANT_BUFFER_VIEW_DESC const_buffer_view_desc{};
        const_buffer_view_desc.BufferLocation = const_buffer_resource->GetGPUVirtualAddress();
        const_buffer_view_desc.SizeInBytes = (sizeof(const_buffer_data) | 0xFF) + 1;

        // Copy the data over
        D3D12_RANGE const_range{ 0, 0 };
        uint8_t* const_data_begin = nullptr;
        throw_if_failed(const_buffer_resource->Map(0, &const_range, reinterpret_cast<void**>(&const_data_begin)));
        memcpy_s(const_data_begin, sizeof(const_buffer_data), &const_buffer_data, sizeof(const_buffer_data));
        const_buffer_resource->Unmap(0, nullptr);
    }

    Shader Flan::RendererDX12::load_shader(const std::string& path)
    {
        Shader shader{};
        std::string vs_path = path + ".vs.cso";
        std::string ps_path = path + ".ps.cso";
        size_t vs_size = 0;
        size_t ps_size = 0;
        char* vs_data = nullptr;
        char* ps_data = nullptr;
        read_file(vs_path, vs_size, vs_data, false);
        read_file(ps_path, ps_size, ps_data, false);
        shader.vs_bytecode.BytecodeLength = vs_size;
        shader.ps_bytecode.BytecodeLength = ps_size;
        shader.vs_bytecode.pShaderBytecode = vs_data;
        shader.ps_bytecode.pShaderBytecode = ps_data;
        return shader;
    }

    bool Flan::RendererDX12::init(int w, int h) {
        create_factory();
        create_device();
        create_command();
        create_descriptor_heaps();
        // Create a window
        if (!create_window(w, h, "FlanRenderer (DirectX 12)")) {
            throw std::exception("Could not create window!");
        }
        create_const_buffer();
        return true;
    }

    void RendererDX12::begin_frame()
    {
        command.begin_frame();
        
        // Update constant buffer

        // Bind root signature
        auto* command_list = command.get_command_list();
        command_list->SetGraphicsRootSignature(root_signature.Get());

        // Bind descriptor heap
        ID3D12DescriptorHeap* descriptor_heaps[] = { cbv_srv_uav_heap.get_heap()};
        command_list->SetDescriptorHeaps(_countof(descriptor_heaps), descriptor_heaps);
        //DescriptorHandle const_buffer_view_handle = cbv_srv_uav_heap.get_cpu_start();

        // Set root descriptor table
        command_list->SetGraphicsRootDescriptorTable(0, cbv_srv_uav_heap.get_gpu_start());
        // Set render target
        command_list->OMSetRenderTargets(1, &rtv_handles[frame_index].cpu, FALSE, nullptr);
    }

    void RendererDX12::end_frame()
    {
        // Record raster commands
        constexpr float clear_color[] = { 0.1f, 0.1f, 0.2f, 1.0f };
        auto* command_list = command.get_command_list();

        // Set up viewport
        command_list->RSSetViewports(1, &viewport); // Set viewport
        command_list->RSSetScissorRects(1, &surface_size); // todo: comment
        command_list->ClearRenderTargetView(rtv_handles[frame_index].cpu, clear_color, 0, nullptr); // Clear the screen
        command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST); // We draw triangles

        // Loop over each model
        while (model_queue && model_queue_length > 0) {
            for (size_t i = 0; i < model_queue_length; ++i) {
                // Get the model draw info for this entry
                ModelDrawInfo& curr_model_info = model_queue[i];

                // Get the mesh from the resource manager
                ModelResource* model_resource = m_resource_manager->get_resource<ModelResource>(curr_model_info.model_to_draw);
                auto vertex_buffer_view = model_resource->meshes_gpu->vertex_buffer_view;
                auto index_buffer_view = model_resource->meshes_gpu->index_buffer_view;
                auto n_triangles = model_resource->meshes_cpu->n_indices;

                // Bind the vertex buffer
                command_list->IASetVertexBuffers(0, 1, &vertex_buffer_view); // Bind vertex buffer
                command_list->IASetIndexBuffer(&index_buffer_view); // Bind index buffer

                // Submit draw call
                command_list->DrawIndexedInstanced(n_triangles, 1, 0, 0, 0);
            }
        }
        command.end_frame();

        // Update window
        swapchain->Present(1, 0);

        // Update GLFW window
        glfwPollEvents();
        glfwSwapBuffers(window);
    }

    void RendererDX12::draw_model(ModelDrawInfo model)
    {
        if (model_queue == nullptr) {
            model_queue = (ModelDrawInfo*)renderer_allocator.allocate(sizeof(ModelDrawInfo) * 1024);
        }
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
}