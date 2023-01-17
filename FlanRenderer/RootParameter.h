#pragma once
#include <d3d12.h>
#include <wrl/client.h>

#include "FlanTypes.h"

/*
 * Huge credits to the YouTube channel Game Engine Series. Most of this code is theirs.
 */

namespace Flan {
    using Microsoft::WRL::ComPtr;
    struct DescriptorRange : public D3D12_DESCRIPTOR_RANGE1 {
        constexpr explicit DescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE range_type, 
                                            u32 descriptor_count, 
                                            u32 shader_register, 
                                            u32 space = 0, 
                                            D3D12_DESCRIPTOR_RANGE_FLAGS flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE, 
                                            u32 offset_from_table_start = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND)
            : D3D12_DESCRIPTOR_RANGE1{ range_type, descriptor_count, shader_register, space, flags, offset_from_table_start }
        {
        }
    };

    struct RootParameter : public D3D12_ROOT_PARAMETER1 {
    public:
        constexpr void as_constants(u32 n_constants, D3D12_SHADER_VISIBILITY visibility, u32 shader_register, u32 space = 0) {
            ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
            ShaderVisibility = visibility;
            Constants.Num32BitValues = n_constants;
            Constants.ShaderRegister = shader_register;
            Constants.RegisterSpace = space;
        }

        constexpr void as_cbv(D3D12_SHADER_VISIBILITY visibility, u32 shader_register, u32 space = 0, D3D12_ROOT_DESCRIPTOR_FLAGS flags = D3D12_ROOT_DESCRIPTOR_FLAG_NONE) {
            as_descriptor(D3D12_ROOT_PARAMETER_TYPE_CBV, visibility, shader_register, space, flags);
        }

        constexpr void as_srv(D3D12_SHADER_VISIBILITY visibility, u32 shader_register, u32 space = 0, D3D12_ROOT_DESCRIPTOR_FLAGS flags = D3D12_ROOT_DESCRIPTOR_FLAG_NONE) {
            as_descriptor(D3D12_ROOT_PARAMETER_TYPE_SRV, visibility, shader_register, space, flags);
        }

        constexpr void as_uav(D3D12_SHADER_VISIBILITY visibility, u32 shader_register, u32 space = 0, D3D12_ROOT_DESCRIPTOR_FLAGS flags = D3D12_ROOT_DESCRIPTOR_FLAG_NONE) {
            as_descriptor(D3D12_ROOT_PARAMETER_TYPE_SRV, visibility, shader_register, space, flags);
        }

        constexpr void as_descriptor_table(D3D12_SHADER_VISIBILITY visibility, const DescriptorRange* ranges, u32 range_count) {
            ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            ShaderVisibility = visibility;
            DescriptorTable.NumDescriptorRanges = range_count;
            DescriptorTable.pDescriptorRanges = ranges;
        }
    private:
        constexpr void as_descriptor(D3D12_ROOT_PARAMETER_TYPE type, D3D12_SHADER_VISIBILITY visibility, u32 shader_register, u32 space, D3D12_ROOT_DESCRIPTOR_FLAGS flags) {
            ParameterType = type;
            ShaderVisibility = visibility;
            Descriptor.ShaderRegister = shader_register;
            Descriptor.RegisterSpace = space;
            Descriptor.Flags = flags;
        }
    };

    struct RootSignatureDesc : public D3D12_ROOT_SIGNATURE_DESC1 {
        constexpr explicit RootSignatureDesc(   const RootParameter* parameters, 
                                                UINT n_parameters, 
                                                const D3D12_STATIC_SAMPLER_DESC* static_samplers = nullptr, 
                                                UINT n_samplers = 0, 
                                                D3D12_ROOT_SIGNATURE_FLAGS flags = D3D12_ROOT_SIGNATURE_FLAG_NONE)
            : D3D12_ROOT_SIGNATURE_DESC1{ n_parameters, parameters, n_samplers, static_samplers, flags } {}
        ID3D12RootSignature* create(ID3D12Device* device);
    };
}