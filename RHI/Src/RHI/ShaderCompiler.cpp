#include "RHI/RHI_PCH.h"
#include "RHI/ShaderCompiler.h"
#include "RHI/dx12/d3d12_headers.h"
#include <dxcapi.h>
#include "Core/ComPtr.h"

#define CHECK(expr) { HRESULT hr = expr; if(FAILED(hr)) { LOG_ERROR("Failed " #expr ", hr = '{}'", hr); return {}; }}

namespace
{
    // Responsible for the actual compilation of shaders.
    alm::ComPtr<IDxcCompiler3> Compiler;
    // Used to create include handle and provides interfaces for loading shader to blob, etc.
    alm::ComPtr<IDxcUtils> Utils;

    struct RecordingIncludeHandler : public IDxcIncludeHandler
    {
        RecordingIncludeHandler(std::wstring includeFolder) : IncludeFolder{ std::move(includeFolder) }
        {}

        ULONG STDMETHODCALLTYPE AddRef() override
        {
            return ++RefCount;
        }
        ULONG STDMETHODCALLTYPE Release() override
        {
            if (--RefCount == 0)
            {
                delete this;
                return 0;
            }
            return RefCount;
        }
        HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** ppv) override
        {
            if (!ppv) return E_POINTER;

            if (iid == __uuidof(IUnknown) || iid == __uuidof(IDxcIncludeHandler))
            {
                *ppv = static_cast<IDxcIncludeHandler*>(this);
                AddRef();
                return S_OK;
            }

            *ppv = nullptr;
            return E_NOINTERFACE;
        }

        HRESULT STDMETHODCALLTYPE LoadSource(LPCWSTR pFilename, IDxcBlob** ppBlob) override
        {
            if (!pFilename || !ppBlob)
                return E_POINTER;

            std::filesystem::path resolved = std::filesystem::path(IncludeFolder) / pFilename;

            std::error_code ec;
            std::filesystem::path canonical = std::filesystem::weakly_canonical(resolved, ec);
            Dependencies.push_back((ec ? resolved : canonical).string());

            // Si el archivo no existe, devolvemos el error para que DXC lo reporte
            if (!std::filesystem::exists(resolved, ec))
                return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);

            alm::ComPtr<IDxcBlobEncoding> blobEncoding;
            HRESULT hr = Utils->LoadFile(resolved.c_str(), nullptr, &blobEncoding);
            if (FAILED(hr))
                return hr;

            *ppBlob = blobEncoding.Detach();
            return S_OK;
        }

        std::wstring IncludeFolder;
        std::vector<std::string> Dependencies;
        std::atomic<ULONG> RefCount{ 0 };
    };

    static std::vector<std::wstring> GetCompilationArguments(alm::rhi::ShaderType shaderType, alm::rhi::ShaderModel model,
        const std::wstring& wsEntryPoint, bool debugMode)
    {
        const wchar_t* sm = nullptr;
        switch (model)
        {
        case alm::rhi::ShaderModel::SM_6_8: 
            sm = L"6_8";
            break;
        case alm::rhi::ShaderModel::SM_6_6:
        default:
            sm = L"6_6";
            break;
        }

        std::wstring profile;
        switch (shaderType)
        {
        case alm::rhi::ShaderType::Vertex:  profile = std::wstring(L"vs_") + sm; break;
        case alm::rhi::ShaderType::Pixel:   profile = std::wstring(L"ps_") + sm; break;
        case alm::rhi::ShaderType::Compute: profile = std::wstring(L"cs_") + sm; break;
        default: break;
        }

        std::vector<std::wstring> args =
        {
            L"-HV", L"2021",
            L"-E", wsEntryPoint,
            L"-T", profile,
            DXC_ARG_WARNINGS_ARE_ERRORS,       // L"-WX"
            DXC_ARG_ALL_RESOURCES_BOUND,       // L"-all_resources_bound"
            L"-enable-16bit-types"
        };

        if (debugMode)
        {
            args.push_back(DXC_ARG_DEBUG);     // L"-Zi"
            args.push_back(L"-Qembed_debug");
            args.push_back(L"-Od");
            args.push_back(L"-DDEBUG");
        }
        else
        {
            args.push_back(DXC_ARG_OPTIMIZATION_LEVEL3); // L"-O3"
        }

        return args;
    }

} // anonymouse namespace

alm::rhi::ShaderCompiler::CompilationResult alm::rhi::ShaderCompiler::Compile(
    const std::string& shaderName, ShaderType shaderType, const alm::WeakBlob& srcData, const std::wstring& includeFolder,
    const std::string& entryPoint, bool debugMode, alm::rhi::ShaderModel model)
{
    if (!Utils)
    {
        CHECK(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&Utils)));
        CHECK(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&Compiler)));
    }

    alm::ComPtr<RecordingIncludeHandler> includeHandler = new RecordingIncludeHandler(includeFolder);
    const std::vector<std::wstring> args = GetCompilationArguments(shaderType, model, ToWide(entryPoint.c_str()), debugMode);
    std::wstring wsName = ToWide(shaderName.c_str());

    std::vector<LPCWSTR> compilationArguments;
    compilationArguments.reserve(args.size() + 1);
    compilationArguments.push_back(wsName.c_str());
    for (const std::wstring& arg : args)
        compilationArguments.push_back(arg.c_str());

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
        includeHandler.Get(),		            // User-provided interface to handle #include directives (optional).
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

    HRESULT compileStatus = S_OK;
    compulationResult->GetStatus(&compileStatus);
    if (FAILED(compileStatus))
    {
        LOG_ERROR("Shader compilation failed, status '{}'", compileStatus);
        return {};
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

    // Remove duplciateds dependencies
    std::ranges::sort(includeHandler->Dependencies);
    auto [first, last] = std::ranges::unique(includeHandler->Dependencies);
    includeHandler->Dependencies.erase(first, last);

    return CompilationResult{
        .CompiledBlob = alm::Blob{ (uint8_t*)data, compiledShaderBlob->GetBufferSize() },
        .Dependencies = std::move(includeHandler->Dependencies) };
}

std::string alm::rhi::ShaderCompiler::GetToolchainString(alm::rhi::ShaderType shaderType, alm::rhi::ShaderModel model, 
    const std::string& entryPoint, bool debugMode)
{
    const std::vector<std::wstring> args = GetCompilationArguments(shaderType, model, ToWide(entryPoint.c_str()), debugMode);

    std::string tag = "dxc=" RHI_DXC_TAG;
    tag += " args=";
    for (const std::wstring& arg : args)
    {
        tag += ToUtf8(arg.c_str());
        if(arg != args.back())
            tag += ',';
    }
    return tag;
}