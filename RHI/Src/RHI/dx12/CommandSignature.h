#pragma once

#include "RHI/CommandSignature.h"

namespace alm::rhi::dx12
{

class CommandSignature : public alm::rhi::ICommandSignature
{
public:

    CommandSignature(ID3D12CommandSignature* commandSig, const CommandSignatureDesc& desc, uint32_t stride, Device* device, const std::string& debugName) :
        alm::rhi::ICommandSignature{ device, debugName },
        m_Desc{ desc },
        m_Stride{ stride },
        m_pCommandSig{ commandSig }
    {}

    ~CommandSignature() override
    {}

    const CommandSignatureDesc& GetDesc() const override { return m_Desc; }
    uint32_t GetStride() const override { return m_Stride; }

    NativeResource GetNativeResource() override { return m_pCommandSig.Get(); }

private:

    void Release(Device* device) override { m_pCommandSig.Reset(); }

private:

    CommandSignatureDesc m_Desc;
    uint32_t m_Stride = 0;
    ComPtr<ID3D12CommandSignature> m_pCommandSig;
};

} // namespace alm::rhi