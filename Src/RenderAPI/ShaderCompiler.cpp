#include "RenderAPI/ShaderCompiler.h"
#include <dxcapi.h> // TODO
#include <filesystem>
#include "Core/Log.h"
#include <string>

using namespace Microsoft::WRL;

#define CHECK(expr) { HRESULT hr = expr; if(FAILED(hr)) { LOG_ERROR("Failed " #expr ", hr = '{}'", hr); return {}; }}

namespace
{
    // Responsible for the actual compilation of shaders.
    ComPtr<IDxcCompiler3> Compiler;
    // Used to create include handle and provides interfaces for loading shader to blob, etc.
    ComPtr<IDxcUtils> Utils;
    ComPtr<IDxcIncludeHandler> IncludeHandler;

    std::wstring ShaderDirectory;

} // anonymouse namespace

st::Blob st::rapi::ShaderCompiler::Compile(ShaderType shaderType, const std::string& path, const std::string& entryPoint, bool debugMode, st::Blob* opt_rootSignature)
{
    HRESULT hr = S_OK;

    if (!Utils)
    {
        CHECK(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&Utils)));
        CHECK(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&Compiler)));
        CHECK(Utils->CreateDefaultIncludeHandler(&IncludeHandler));

        auto currentDirectory = std::filesystem::current_path();
        ShaderDirectory = (currentDirectory / "Data" / "Shaders").wstring();
        LOG_INFO("Shader base directory: '{}'", ToUtf8(ShaderDirectory.c_str()));
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
    std::vector<LPCWSTR> compilationArguments = 
    {
        L"-HV"
        L"2021",
        L"-E",
        wsEntryPoint.data(),
        L"-T",
        targetProfile.c_str(),
        DXC_ARG_PACK_MATRIX_ROW_MAJOR,
        DXC_ARG_WARNINGS_ARE_ERRORS,
        DXC_ARG_ALL_RESOURCES_BOUND,
        L"-I",
        ShaderDirectory.c_str()
    };

    // Indicate that the shader should be in a debuggable state if in debug mode.
    // Else, set optimization level to 03.
    if(debugMode)
    {
        compilationArguments.push_back(DXC_ARG_DEBUG);
    }
    else
    {
        compilationArguments.push_back(DXC_ARG_OPTIMIZATION_LEVEL3);
    }

    // Load the shader source file to a blob.
    ComPtr<IDxcBlobEncoding> sourceBlob{ nullptr };
    Utils->LoadFile(ToWide(path.c_str()).c_str(), nullptr, &sourceBlob);

    const DxcBuffer sourceBuffer = {
        .Ptr = sourceBlob->GetBufferPointer(),
        .Size = sourceBlob->GetBufferSize(),
        .Encoding = DXC_CP_ACP
    };

    ComPtr<IDxcResult> compulationResult;
    HRESULT hr = Compiler->Compile(
        &sourceBuffer,                          // Source buffer.
        compilationArguments.data(),	        // Array of pointers to arguments.
        (uint32_t)compilationArguments.size(),	// Number of arguments.
        IncludeHandler.Get(),		            // User-provided interface to handle #include directives (optional).
        IID_PPV_ARGS(&compulationResult)	                // Compiler output status, buffer, and errors.
    );
    if (FAILED(hr))
    {
        LOG_ERROR("Failed to compile shader '{}'", path);
        return {};
    }

    // Get compilation errors (if any).
    ComPtr<IDxcBlobUtf8> errors;
    hr = compulationResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errors), nullptr);
    assert(SUCCEEDED(hr));
    if (errors && errors->GetStringLength() > 0)
    {
        const LPCSTR errorMessage = errors->GetStringPointer();
        LOG_ERROR("Shader '{}' error '{}'", path, errorMessage);
    }

    ComPtr<IDxcBlob> compiledShaderBlob;
    hr = compulationResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&compiledShaderBlob), nullptr);
    if (FAILED(hr))
    {
        LOG_ERROR("Failed to get compiled data, shader '{}'", path);
        return {};
    }
    
    void* data = std::malloc(compiledShaderBlob->GetBufferSize());
    std::memcpy(data, compiledShaderBlob->GetBufferPointer(), compiledShaderBlob->GetBufferSize());

    return st::Blob{ (char*)data, compiledShaderBlob->GetBufferSize() };
}