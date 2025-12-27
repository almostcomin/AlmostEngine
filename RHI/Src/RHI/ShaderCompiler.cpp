#include "RHI/RHI_PCH.h"
#include "RHI/ShaderCompiler.h"
#include "RHI/dx12/d3d12_headers.h"
#include <dxcapi.h>
#include "Core/ComPtr.h"

#define CHECK(expr) { HRESULT hr = expr; if(FAILED(hr)) { LOG_ERROR("Failed " #expr ", hr = '{}'", hr); return {}; }}

namespace
{
    // Responsible for the actual compilation of shaders.
    st::ComPtr<IDxcCompiler3> Compiler;
    // Used to create include handle and provides interfaces for loading shader to blob, etc.
    st::ComPtr<IDxcUtils> Utils;
    st::ComPtr<IDxcIncludeHandler> IncludeHandler;
} // anonymouse namespace

st::Blob st::rhi::ShaderCompiler::Compile(ShaderType shaderType, const st::WeakBlob& srcData, const std::string& includeFolder, const std::string& entryPoint, 
    bool debugMode)
{
    if (!Utils)
    {
        CHECK(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&Utils)));
        CHECK(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&Compiler)));
        CHECK(Utils->CreateDefaultIncludeHandler(&IncludeHandler));
    }

    // Setup compilation arguments.
    const std::wstring targetProfile = [=]()
    {
        switch (shaderType)
        {
        case ShaderType::Vertex:
            return L"vs_6_6";
        case ShaderType::Pixel:
            return L"ps_6_6";
        case ShaderType::Compute:
            return L"cs_6_6";
        default:
            return L"";
        }
    }();

    std::wstring wsEntryPoint = ToWide(entryPoint.c_str());
    std::wstring wsIncludeFolder = ToWide(includeFolder.c_str());
    std::vector<LPCWSTR> compilationArguments = 
    {
        L"-HV",
        L"2021",
        L"-E",
        wsEntryPoint.data(),
        L"-T",
        targetProfile.c_str(),
        DXC_ARG_PACK_MATRIX_ROW_MAJOR,
        DXC_ARG_WARNINGS_ARE_ERRORS,
        DXC_ARG_ALL_RESOURCES_BOUND,
        L"-enable-16bit-types",
        L"-I",
        wsIncludeFolder.data()
    };

    // Indicate that the shader should be in a debuggable state if in debug mode.
    // Else, set optimization level to 03.
    if(debugMode)
    {
        compilationArguments.push_back(DXC_ARG_DEBUG); // Zi
        compilationArguments.push_back(L"-Qembed_debug");
        compilationArguments.push_back(L"-Od");
        compilationArguments.push_back(L"-DDEBUG");
    }
    else
    {
        compilationArguments.push_back(DXC_ARG_OPTIMIZATION_LEVEL3);
    }

    const DxcBuffer sourceBuffer = {
        .Ptr = srcData.data(),
        .Size = srcData.size(),
        .Encoding = DXC_CP_ACP
    };

    ComPtr<IDxcResult> compulationResult;
    HRESULT hr = Compiler->Compile(
        &sourceBuffer,                          // Source buffer.
        compilationArguments.data(),	        // Array of pointers to arguments.
        (uint32_t)compilationArguments.size(),	// Number of arguments.
        IncludeHandler.Get(),		            // User-provided interface to handle #include directives (optional).
        IID_PPV_ARGS(&compulationResult)	    // Compiler output status, buffer, and errors.
    );
    if (FAILED(hr))
    {
        LOG_ERROR("Failed to compile shader, hr '{}'", hr);
        return {};
    }

    // Get compilation errors (if any).
    ComPtr<IDxcBlobUtf8> errorsBlob;
    hr = compulationResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errorsBlob), nullptr);
    assert(SUCCEEDED(hr));
    if (errorsBlob && errorsBlob->GetStringLength() > 0)
    {
        const LPCSTR errorMessage = errorsBlob->GetStringPointer();
        LOG_ERROR("Shader compilation errors:\n'{}'", errorMessage);
    }

    ComPtr<IDxcBlob> compiledShaderBlob;
    hr = compulationResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&compiledShaderBlob), nullptr);
    if (FAILED(hr))
    {
        LOG_ERROR("Failed to get compiled data");
        return {};
    }
    
    void* data = std::malloc(compiledShaderBlob->GetBufferSize());
    std::memcpy(data, compiledShaderBlob->GetBufferPointer(), compiledShaderBlob->GetBufferSize());

    return st::Blob{ (char*)data, compiledShaderBlob->GetBufferSize() };
}