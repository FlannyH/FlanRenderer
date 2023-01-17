#include "RootParameter.h"
#include <cassert>

#include "HelperFunctions.h"

namespace Flan {
    ID3D12RootSignature* RootSignatureDesc::create(ID3D12Device* device) {
        assert(device);

        D3D12_VERSIONED_ROOT_SIGNATURE_DESC versioned_root_signature_desc{};
        versioned_root_signature_desc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
        versioned_root_signature_desc.Desc_1_1 = *this;

        ID3D12RootSignature* root_signature = nullptr;
        ComPtr<ID3DBlob> signature;
        ComPtr<ID3DBlob> error;
        try {
            throw_if_failed(D3D12SerializeVersionedRootSignature(&versioned_root_signature_desc, &signature, &error));
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

        return root_signature;
    }
}
