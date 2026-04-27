#include "Gfx/GfxPCH.h"
#include "Gfx/GltfImporter.h"
#include "Core/Log.h"
#include "Gfx/Material.h"
#include "Gfx/Mesh.h"
#include "Core/Math/aabox.h"
#include "Gfx/Math/Util.h"
#include "Gfx/DataUploader.h"
#include "Gfx/TextureCache.h"
#include "Gfx/SceneGraph.h"
#include "Gfx/SceneGraphNode.h"
#include "Gfx/MeshInstance.h"
#include "Gfx/SceneCamera.h"
#include "Gfx/SceneLights.h"
#include <filesystem>
#include "Core/File.h"
#include "Gfx/DeviceManager.h"
#include "RHI/Device.h"
#include "Gfx/LoadedTexture.h"

#define CGLTF_IMPLEMENTATION
#include <cgltf.h>

namespace
{

struct FleContext
{
    alm::Blob data;
};

struct TextureSwizzle
{
    cgltf_image* source = nullptr;
    cgltf_int numChannels = 0;
    cgltf_int channels[4]{};
};

struct TextureExtensions
{
    cgltf_image* ddsImage = nullptr;
    std::vector<TextureSwizzle> swizzleOptions;
};

struct GltfInlineData
{
    alm::Blob buffer;

    // Object name from glTF, if specified.
    // Otherwise, generated as "AssetName.gltf[index]"
    std::string name;

    std::string mimeType;
};

struct LoadTexCache
{
    std::unordered_map<const cgltf_texture*, std::shared_ptr<alm::gfx::LoadedTexture>> texCache;
    std::unordered_map<const cgltf_image*, std::shared_ptr<alm::gfx::LoadedTexture>> imageCache;
    std::unordered_map<const cgltf_image*, std::shared_ptr<GltfInlineData>> inlineDataCache;
    std::filesystem::path path;
    alm::gfx::TextureCache* textureCache;
};

// Contains either a file path for a resource referenced in a glTF asset,
// or an inline data buffer from the same asset.
struct FilePathOrInlineData
{
    // Absolute path for an image file
    std::string path;

    // Data for the image provided in the glTF container
    std::shared_ptr<GltfInlineData> data;

    // Implicit conversion to bool, returns true if there is either a path or a data buffer
    operator bool() const
    {
        return !path.empty() || data != nullptr;
    }

    bool operator==(FilePathOrInlineData const& other) const
    {
        return path == other.path && data == other.data;
    }

    bool operator!=(FilePathOrInlineData const& other) const
    {
        return !(*this == other);
    }

    std::string const& ToString() const
    {
        return data ? data->name : path;
    }
};

cgltf_result ReadFileCB(const struct cgltf_memory_options* memory_options,
    const struct cgltf_file_options* file_options, const char* path, cgltf_size* size, void** data)
{
    FleContext* context = (FleContext*)file_options->user_data;
    if (!context)
    {
        LOG_WARNING("cgltf read: no context for file {} provided.", path);
        return cgltf_result_invalid_options;
    }

    alm::fs::File file(path);
    if (!file.IsOpen())
    {
        LOG_WARNING("cgltf read: file {} not found.", path);
        return cgltf_result_file_not_found;
    }
    auto readResult = file.Read();
    if (readResult)
    {
        context->data = std::move(*readResult);
        if (size) *size = context->data.size();
        if (data) *data = context->data.data();
    }
    else
    {
        LOG_WARNING("cgltf read: file error: '{}'.", readResult.error());
    }

    return cgltf_result_success;
}

void ReleaseFileCB(const struct cgltf_memory_options*, const struct cgltf_file_options*, void*)
{
    // no-op
}

const char* ErrorToString(cgltf_result res)
{
    switch (res)
    {
    case cgltf_result_success:
        return "Success";
    case cgltf_result_data_too_short:
        return "Data is too short";
    case cgltf_result_unknown_format:
        return "Unknown format";
    case cgltf_result_invalid_json:
        return "Invalid JSON";
    case cgltf_result_invalid_gltf:
        return "Invalid glTF";
    case cgltf_result_invalid_options:
        return "Invalid options";
    case cgltf_result_file_not_found:
        return "File not found";
    case cgltf_result_io_error:
        return "I/O error";
    case cgltf_result_out_of_memory:
        return "Out of memory";
    case cgltf_result_legacy_gltf:
        return "Legacy glTF";
    default:
        return "Unknown error";
    }
}

std::pair<const uint8_t*, size_t> BufferIterator(const cgltf_accessor& accessor, size_t defaultStride)
{
    // TODO: sparse accessor support
    const cgltf_buffer_view* view = accessor.buffer_view;
    const uint8_t* data = (uint8_t*)view->buffer->data + view->offset + accessor.offset;
    const size_t stride = view->stride ? view->stride : defaultStride;
    return std::make_pair(data, stride);
}

// Parses the image reference from an MSFT_texture_dds extension.
// See https://github.com/KhronosGroup/glTF/tree/master/extensions/2.0/Vendor/MSFT_texture_dds 
int ParseTextureDDS(jsmntok_t* tokens, int i, const uint8_t* json_chunk,
    cgltf_image** out_image, const cgltf_data* objects)
{
    CGLTF_CHECK_TOKTYPE(tokens[i], JSMN_OBJECT);

    int size = tokens[i].size;
    ++i;

    for (int j = 0; j < size; ++j)
    {
        CGLTF_CHECK_KEY(tokens[i]);

        if (cgltf_json_strcmp(tokens + i, json_chunk, "source") == 0)
        {
            ++i;
            int index = cgltf_json_to_int(tokens + i, json_chunk);
            ++i;

            if (index >= 0 && index < objects->images_count)
                *out_image = objects->images + index;
            else
                return CGLTF_ERROR_JSON;
        }
        else
        {
            i = cgltf_skip_json(tokens, i + 1);
        }

        if (i < 0)
        {
            return i;
        }
    }

    return i;
}

// Parses a single texture swizzle option into a TextureSwizzle object.
// See the comment to cgltf_parse_texture_swizzle_options for an example input.
int ParseTextureSwizzle(jsmntok_t* tokens, int i, const uint8_t* json_chunk,
    TextureSwizzle* out_swizzle, const cgltf_data* objects)
{
    CGLTF_CHECK_TOKTYPE(tokens[i], JSMN_OBJECT);

    int size = tokens[i].size;
    ++i;

    for (int j = 0; j < size; ++j)
    {
        CGLTF_CHECK_KEY(tokens[i]);

        if (cgltf_json_strcmp(tokens + i, json_chunk, "source") == 0)
        {
            ++i;
            int index = cgltf_json_to_int(tokens + i, json_chunk);
            ++i;

            if (index >= 0 && index < objects->images_count)
                out_swizzle->source = objects->images + index;
            else
                return CGLTF_ERROR_JSON;
        }
        else if (cgltf_json_strcmp(tokens + i, json_chunk, "channels") == 0)
        {
            ++i;
            CGLTF_CHECK_TOKTYPE(tokens[i], JSMN_ARRAY);

            int size = tokens[i].size;
            if (size > 4)
                return CGLTF_ERROR_JSON;
            ++i;

            for (int channelIdx = 0; channelIdx < size; ++channelIdx)
            {
                CGLTF_CHECK_TOKTYPE(tokens[i], JSMN_PRIMITIVE);
                int ch = cgltf_json_to_int(tokens + i, json_chunk);
                out_swizzle->channels[channelIdx] = ch;
                ++i;
            }

            out_swizzle->numChannels = size;
        }
        else
        {
            i = cgltf_skip_json(tokens, i + 1);
        }

        if (i < 0)
        {
            return i;
        }
    }

    return i;
}

// Parses an array of texture swizzle options from "NV_texture_swizzle" extension into a cgltf_texture_extensions object.
// There is no public spec for NV_texture_swizzle at this time.
// Example extensions for a glTF texture object:
//
// "extensions": {
//     "NV_texture_swizzle": {
//         "options": [
//             {
//                 "source": <gltf-image-index>,
//                 "channels": [1, 2, ...]
//             },
//             { ... }
//         ]
//     }
// }
static int ParseTextureSwizzleOptions(jsmntok_t* tokens, int i, const uint8_t* json_chunk,
    TextureExtensions* out_extensions, const cgltf_data* objects)
{
    CGLTF_CHECK_TOKTYPE(tokens[i], JSMN_OBJECT);

    int size = tokens[i].size;
    ++i;

    for (int j = 0; j < size; ++j)
    {
        CGLTF_CHECK_KEY(tokens[i]);

        if (cgltf_json_strcmp(tokens + i, json_chunk, "options") == 0)
        {
            ++i;
            CGLTF_CHECK_TOKTYPE(tokens[i], JSMN_ARRAY);

            int numOptions = tokens[i].size;
            ++i;

            for (int swizzleIdx = 0; swizzleIdx < numOptions; ++swizzleIdx)
            {
                TextureSwizzle swizzle;
                i = ParseTextureSwizzle(tokens, i, json_chunk, &swizzle, objects);
                if (i < 0)
                    return i;
                out_extensions->swizzleOptions.push_back(swizzle);
            }
        }
        else
        {
            i = cgltf_skip_json(tokens, i + 1);
        }

        if (i < 0)
        {
            return i;
        }
    }

    return i;
}

// Processes all supported extensions for a glTF texture object.
TextureExtensions ParseTextureExtensions(const cgltf_texture* texture, const cgltf_data* objects)
{
    TextureExtensions result = {};

    for (size_t i = 0; i < texture->extensions_count; i++)
    {
        const cgltf_extension& ext = texture->extensions[i];

        if (!ext.name || !ext.data)
            continue;

        bool isDDS = strcmp(ext.name, "MSFT_texture_dds") == 0;
        bool isSwizzle = strcmp(ext.name, "NV_texture_swizzle") == 0;
        if (!isDDS && !isSwizzle)
            continue;

        size_t extensionLength = strlen(ext.data);
        if (extensionLength > 2048)
            return result; // safeguard against weird inputs

        jsmn_parser parser;
        jsmn_init(&parser);

        constexpr int c_MaxTokens = 64;
        jsmntok_t tokens[c_MaxTokens];

        int k = 0;

        // parse the string into tokens
        int numTokens = jsmn_parse(&parser, ext.data, extensionLength, tokens, c_MaxTokens);
        if (numTokens < 0)
            goto fail;

        if (isDDS)
        {
            if (ParseTextureDDS(tokens, k, (const uint8_t*)ext.data, &result.ddsImage, objects) < 0)
                goto fail;
        }
        else if (isSwizzle)
        {
            if (ParseTextureSwizzleOptions(tokens, k, (const uint8_t*)ext.data, &result, objects) < 0)
                goto fail;
        }

        continue;

    fail:
        LOG_WARNING("Failed to parse glTF extension {}: {}", ext.name, ext.data);
    }

    return result;
}

FilePathOrInlineData LoadImageData(const cgltf_image* image, bool searchForDDS, LoadTexCache& loadCache, int imageIndex, const cgltf_options& options)
{
    FilePathOrInlineData result;
    auto it = loadCache.inlineDataCache.find(image);
    if (it != loadCache.inlineDataCache.end())
    {
        result.data = it->second;
        return result;
    }

    if (image->buffer_view)
    {
        // If the image has inline data, like coming from a GLB container, use that.

        const uint8_t* dataPtr = static_cast<const uint8_t*>(image->buffer_view->buffer->data) + image->buffer_view->offset;
        const size_t dataSize = image->buffer_view->size;

        // We need to have a managed pointer to the texture data for async decoding.
        alm::Blob textureData{ (char*)malloc(dataSize), dataSize };

        result.data = std::make_shared<GltfInlineData>();
        result.data->name = image->name
            ? image->name
            : loadCache.path.stem().string() + "[" + std::to_string(imageIndex) + "]";
        result.data->mimeType = image->mime_type ? image->mime_type : "";
        result.data->buffer = std::move(textureData);

        loadCache.inlineDataCache[image] = result.data;
    }
    else if (image->uri && strncmp(image->uri, "data:", 5) == 0)
    {
        // Decode a Data URI
        char* comma = strchr(image->uri, ',');

        if (comma && comma - image->uri >= 7 && strncmp(comma - 7, ";base64", 7) == 0)
        {
            char* base64data = comma + 1;

            // cgltf doesn't understand Base64 padding with = characters, replace them with A (0)
            size_t len = strlen(base64data);
            while (len > 0 && base64data[len - 1] == '=')
                base64data[--len] = 'A';

            // Derive the size of the decoded buffer
            size_t size = (len * 6 + 7) / 8;

            // Decode the Base64 data
            void* data = nullptr;
            cgltf_result res = cgltf_load_buffer_base64(&options, size, base64data, &data);

            if (res == cgltf_result_success)
            {
                result.data = std::make_shared<GltfInlineData>();
                result.data->name = image->name
                    ? image->name
                    : loadCache.path.stem().string() + "[" + std::to_string(imageIndex) + "]";
                result.data->mimeType = image->mime_type ? image->mime_type : "";
                result.data->buffer = alm::Blob{ (char*)data, size };

                loadCache.inlineDataCache[image] = result.data;
            }
            else
            {
                LOG_WARNING("Failed to decode Base64 data for image {}, ignoring.", int(imageIndex));
            }
        }
        else
        {
            LOG_WARNING("Couldn't find a Base64 marker in Data URI for image {}, ignoring.", int(imageIndex));
        }
    }
    else
    {
        // Decode %-encoded characters in the URI, because cgltf doesn't do that for some reason.
        std::string uri = image->uri;
        cgltf_decode_uri(uri.data());
        uri.resize(strlen(uri.data()));

        // No inline data - read a file.
        std::filesystem::path filePath = loadCache.path.parent_path() / uri;

        // Try to replace the texture with DDS, if enabled.
        if (searchForDDS)
        {
            std::filesystem::path filePathDDS = filePath;

            filePathDDS.replace_extension(".dds");

            if (alm::fs::File::Exists(filePathDDS))
                filePath = filePathDDS;
        }

        result.path = filePath.generic_string();
    }

    return result;
};

std::shared_ptr<alm::gfx::LoadedTexture> LoadImage(const cgltf_image* image, bool sRGB, bool normalMap, bool searchForDDS, LoadTexCache& loadCache,
    int imageIndex, const cgltf_options& options, std::vector<alm::SignalListener>& out_handlesToWait)
{
    auto it = loadCache.imageCache.find(image);
    if (it != loadCache.imageCache.end())
        return it->second;

    FilePathOrInlineData textureSource = LoadImageData(image, searchForDDS, loadCache, imageIndex, options);

    auto flags = alm::gfx::TextureCache::Flags::GenerateMips;
    if (sRGB)
        flags |= alm::gfx::TextureCache::Flags::ForceSRGB;
    if (normalMap)
        flags |= alm::gfx::TextureCache::Flags::IsNormalMap;

    std::shared_ptr<alm::gfx::LoadedTexture> texture;
    if (textureSource.data)
    {
        auto loadResult = loadCache.textureCache->Load(
            alm::WeakBlob{ textureSource.data->buffer }, flags, false, textureSource.data->name);
        if (loadResult)
        {
            texture = loadResult->first;
            out_handlesToWait.push_back(loadResult->second);
            // TODO: save handle to wait
        }
        else
        {
            LOG_ERROR("Failed to load texture from data, cgltf file '{}'", loadCache.path.string());
        }
    }
    else if (!textureSource.path.empty())
    {
        auto loadResult = loadCache.textureCache->Load(textureSource.path, flags);
        if (loadResult)
        {
            texture = loadResult->first;
            out_handlesToWait.push_back(loadResult->second);
        }
        else
        {
            LOG_ERROR("Failed to load texture from file '{}', cgltf file '{}'", textureSource.path, loadCache.path.string());
        }
    }

    loadCache.imageCache[image] = texture;
    return texture;
};

std::shared_ptr<alm::gfx::LoadedTexture> LoadTexture(cgltf_texture* texture, const cgltf_data* objects, bool sRGB, bool normalMap,
    LoadTexCache& loadCache, const cgltf_options& options, std::vector<alm::SignalListener>& out_handlesToWait)
{
    auto it = loadCache.texCache.find(texture);
    if (it != loadCache.texCache.end())
        return it->second;

    const TextureExtensions extensions = ParseTextureExtensions(texture, objects);
    std::shared_ptr<alm::gfx::LoadedTexture> loadedTexture;

    // See if the extensions include a DDS image.
    // Try loading the DDS first if it's specified, fall back to the regular image.
    cgltf_image const* ddsImage = extensions.ddsImage;
    if (ddsImage)
    {
        loadedTexture = LoadImage(ddsImage, sRGB, normalMap, false, loadCache, objects->images - ddsImage, options, out_handlesToWait);
    }
    if (!loadedTexture && texture->image)
    {
        loadedTexture = LoadImage(texture->image, sRGB, normalMap, true, loadCache, objects->images - texture->image, options, out_handlesToWait);
    }

    // If the texture swizzle extension is present, load the source images and transfer the swizzle data
    if (!extensions.swizzleOptions.empty())
    {
        assert(0);
    }

    return loadedTexture;
}

std::unordered_map<const cgltf_material*, std::shared_ptr<alm::gfx::Material>> 
GetMaterialsMap(const cgltf_data* objects, LoadTexCache& loadCache, const cgltf_options& options, std::vector<alm::SignalListener>& out_handlesToWait, 
    alm::rhi::Device* device)
{
    std::unordered_map<const cgltf_material*, std::shared_ptr<alm::gfx::Material>> matMap;
    auto loadTex = [objects, &loadCache, &options, &out_handlesToWait](cgltf_texture* texture, bool sRGB, bool normalMap) 
        -> std::shared_ptr<alm::gfx::LoadedTexture>
    {
        if (!texture)
            return {};
        auto loadedTexture = LoadTexture(texture, objects, sRGB, normalMap, loadCache, options, out_handlesToWait);
        return loadedTexture;
    };

    for (size_t mat_idx = 0; mat_idx < objects->materials_count; mat_idx++)
    {
        const cgltf_material& srcMat = objects->materials[mat_idx];
        std::string path = loadCache.path.generic_string();
        std::shared_ptr<alm::gfx::Material> mat =
            std::make_shared<alm::gfx::Material>(device, (const char*)srcMat.name, path.c_str());

        if (srcMat.has_pbr_specular_glossiness)
        {
            LOG_ERROR("Material {} unsupported mode: Specular-Glossiness", srcMat.name);
            continue;
        }
        assert(srcMat.has_pbr_metallic_roughness);

        mat->SetBaseColorTexture(loadTex(srcMat.pbr_metallic_roughness.base_color_texture.texture, true, false));
        mat->SetMetalRoughTexture(loadTex(srcMat.pbr_metallic_roughness.metallic_roughness_texture.texture, false, false));
        mat->SetNormalTexture(loadTex(srcMat.normal_texture.texture, false, true));
        mat->SetEmissiveTexture(loadTex(srcMat.emissive_texture.texture, true, false));
        mat->SetOcclusionTexture(loadTex(srcMat.occlusion_texture.texture, false, false));

        mat->SetBaseColor(*(float3*)srcMat.pbr_metallic_roughness.base_color_factor);
        mat->SetOpacity(srcMat.pbr_metallic_roughness.base_color_factor[3]);
        mat->SetEmissiveColor(*(float3*)srcMat.emissive_factor);
        mat->SetMetallicFactor(srcMat.pbr_metallic_roughness.metallic_factor);
        mat->SetRoughnessFactor(srcMat.pbr_metallic_roughness.roughness_factor);
        mat->SetOcclusionStrengh(srcMat.occlusion_texture.scale);

        mat->SetNormalTextureScale(srcMat.normal_texture.has_transform ?
            float2{ srcMat.normal_texture.transform.scale[0], srcMat.normal_texture.transform.scale[1] } : float2{ 1.f });

        mat->SetCullMode(srcMat.double_sided ? alm::rhi::CullMode::None : alm::rhi::CullMode::Back);

        mat->SetAlphaCutoff(srcMat.alpha_cutoff);

        // Log warnings for all unsupported texture coordinate transformations
        if (srcMat.pbr_metallic_roughness.base_color_texture.has_transform ||
            srcMat.pbr_metallic_roughness.metallic_roughness_texture.has_transform ||
            srcMat.pbr_specular_glossiness.diffuse_texture.has_transform ||
            srcMat.pbr_specular_glossiness.specular_glossiness_texture.has_transform ||
            srcMat.occlusion_texture.has_transform ||
            srcMat.emissive_texture.has_transform ||
            (srcMat.normal_texture.has_transform &&
                (srcMat.normal_texture.transform.rotation != 0.0f ||
                    srcMat.normal_texture.transform.offset[0] != 0.0f ||
                    srcMat.normal_texture.transform.offset[1] != 0.0f)))
        {
            LOG_WARNING("Material '{}' uses the KHR_texture_transform extension, which is not supported on non-scale transformations and all textures except normal",
                srcMat.name ? srcMat.name : "<Unnamed>");
        }

        switch (srcMat.alpha_mode)
        {
        case cgltf_alpha_mode_opaque:
            mat->SetDomain(alm::gfx::MaterialDomain::Opaque);
            break;
        case cgltf_alpha_mode_mask:
            mat->SetDomain(alm::gfx::MaterialDomain::AlphaTested);
            break;
        case cgltf_alpha_mode_blend:
            mat->SetDomain(alm::gfx::MaterialDomain::AlphaBlended);
            break;
        }

        // Avoid duplicated materials
        for (auto& it : matMap)
        {
            if (*(it.second) == *mat)
            {
                mat = it.second;
            }
        }

        matMap[&srcMat] = mat;
    }

    return matMap;
}

void GetIndexVertexCount(const cgltf_data* objects, size_t& out_totalIndices, size_t& out_totalVertices, size_t& out_morphTargetTotalVertices, bool& out_hasJoints)
{
    out_totalIndices = 0;
    out_totalVertices = 0;
    out_morphTargetTotalVertices = 0;
    out_hasJoints = false;

    for (size_t mesh_idx = 0; mesh_idx < objects->meshes_count; mesh_idx++)
    {
        const cgltf_mesh& mesh = objects->meshes[mesh_idx];

        for (size_t prim_idx = 0; prim_idx < mesh.primitives_count; prim_idx++)
        {
            const cgltf_primitive& prim = mesh.primitives[prim_idx];

            if ((prim.type != cgltf_primitive_type_triangles &&
                prim.type != cgltf_primitive_type_line_strip &&
                prim.type != cgltf_primitive_type_lines) ||
                prim.attributes_count == 0)
            {
                continue;
            }

            if (prim.indices)
                out_totalIndices += prim.indices->count;
            else
                out_totalIndices += prim.attributes->data->count;
            out_totalVertices += prim.attributes->data->count;

            if (!out_hasJoints)
            {
                // Detect if the primitive has joints or weights attributes.
                for (size_t attr_idx = 0; attr_idx < prim.attributes_count; attr_idx++)
                {
                    const cgltf_attribute& attr = prim.attributes[attr_idx];
                    if (attr.type == cgltf_attribute_type_joints || attr.type == cgltf_attribute_type_weights)
                    {
                        out_hasJoints = true;
                        break;
                    }
                }
            }
        }
    }
}

template<typename T>
void CollectPrimitiveIndices(const cgltf_primitive& prim, const cgltf_accessor& positions, alm::Blob& out_indexData)
{
    static_assert(std::is_same_v<T, uint16_t> || std::is_same_v<T, uint32_t>, "Only 16 or 32 bit indices are allowed");

    size_t indexCount = 0;
    if (prim.indices)
    {
        out_indexData = alm::Blob{ (char*)malloc(prim.indices->count * sizeof(T)), prim.indices->count * sizeof(T) };

        // copy the indices
        auto [indexSrc, indexStride] = BufferIterator(*prim.indices, 0);
        T* indexDst = (T*)out_indexData.data();

        switch (prim.indices->component_type)
        {
        case cgltf_component_type_r_8u:
            if (!indexStride) indexStride = sizeof(uint8_t);
            for (size_t i_idx = 0; i_idx < prim.indices->count; i_idx++)
            {
                *indexDst = T(*(const uint8_t*)indexSrc);
                indexSrc += indexStride;
                indexDst++;
            }
            break;

        case cgltf_component_type_r_16u:
            if (!indexStride) indexStride = sizeof(uint16_t);
            for (size_t i_idx = 0; i_idx < prim.indices->count; i_idx++)
            {
                *indexDst = T(*(const uint16_t*)indexSrc);
                indexSrc += indexStride;
                indexDst++;
            }
            break;

        case cgltf_component_type_r_32u:
            if (!indexStride) indexStride = sizeof(uint32_t);
            for (size_t i_idx = 0; i_idx < prim.indices->count; i_idx++)
            {
                *indexDst = T(*(const uint32_t*)indexSrc);
                indexSrc += indexStride;
                indexDst++;
            }
            break;
        default:
            assert(false);
        }
    }
    else
    {
        // generate the indices
        const uint32_t indexCount = positions.count;
        out_indexData = alm::Blob{ (char*)malloc(indexCount * sizeof(T)), indexCount * sizeof(T) };
        T* indexDst = (T*)out_indexData.data();

        for (size_t i_idx = 0; i_idx < indexCount; i_idx++)
        {
            *indexDst = (T)i_idx;
            indexDst++;
        }
    }
}

void CollectPrimitivePositions(const cgltf_accessor& positions, std::vector<glm::vec3>& out_vertexPosData, alm::math::aabox3f& out_bounds)
{
    out_bounds.reset();
    out_vertexPosData.resize(positions.count);

    auto [posSrc, posStride] = BufferIterator(positions, sizeof(float) * 3);
    glm::vec3* posDst = out_vertexPosData.data();

    for (size_t v_idx = 0; v_idx < positions.count; v_idx++)
    {
        *posDst = *(const glm::vec3*)posSrc;
        out_bounds.merge(*posDst);

        posSrc += posStride;
        ++posDst;
    }
}

void CollectPrimitiveNormals(const cgltf_accessor& normals, std::vector<uint32_t>& out_vertexNormalData)
{
    out_vertexNormalData.resize(normals.count);

    auto [normalSrc, normalStride] = BufferIterator(normals, sizeof(float) * 3);
    uint32_t* normalDst = out_vertexNormalData.data();

    for (size_t v_idx = 0; v_idx < normals.count; v_idx++)
    {
        const glm::vec3* normal = (const glm::vec3*)normalSrc;
        *normalDst = alm::math::VectorToSnorm8(*normal);

        normalSrc += normalStride;
        ++normalDst;
    }
}

void CollectPrimitiveTangents(const cgltf_accessor& tangents, std::vector<uint32_t>& out_tangentNormalData)
{
    assert(tangents.type == cgltf_type_vec4);
    assert(tangents.component_type == cgltf_component_type_r_32f);

    out_tangentNormalData.resize(tangents.count);

    auto [tangentSrc, tangentStride] = BufferIterator(tangents, sizeof(float) * 4);
    uint32_t* tangentDst = out_tangentNormalData.data();

    for (size_t v_idx = 0; v_idx < tangents.count; v_idx++)
    {
        const glm::vec4* tangent = (const glm::vec4*)tangentSrc;
        *tangentDst = alm::math::VectorToSnorm8(*tangent);

        tangentSrc += tangentStride;
        ++tangentDst;
    }
}

void CollectPrimitiveTexcoords(const cgltf_accessor& texcoords, std::vector<glm::vec2>& out_vertexTexCoordData)
{
    out_vertexTexCoordData.resize(texcoords.count);

    auto [texcoordSrc, texcoordStride] = BufferIterator(texcoords, sizeof(float) * 2);
    glm::vec2* texcoordDst = out_vertexTexCoordData.data();

    for (size_t v_idx = 0; v_idx < texcoords.count; v_idx++)
    {
        *texcoordDst = *(const glm::vec2*)texcoordSrc;

        texcoordSrc += texcoordStride;
        ++texcoordDst;
    }
}

template<typename index_t>
void ComputeTangents(const cgltf_accessor& indices, const cgltf_accessor& positions, const cgltf_accessor& normals, const cgltf_accessor& texcoords, 
    std::vector<uint32_t>& out_tangentNormalData)
{
    auto [posSrc, posStride] = BufferIterator(positions, sizeof(float) * 3);
    auto [normalSrc, normalStride] = BufferIterator(normals, sizeof(float) * 3);
    auto [texcoordSrc, texcoordStride] = BufferIterator(texcoords, sizeof(float) * 2);
    auto [indexSrc, indexStride] = BufferIterator(indices, sizeof(index_t));

    std::vector<float3> computedTangents{ positions.count, float3{0.f} };
    std::vector<float3> computedBitangents{ positions.count, float3{0.f} };

    for (size_t idx_i = 0; idx_i < indices.count; idx_i += 3)
    {
        index_t idx0 = *(index_t*)(indexSrc + (indexStride * idx_i));
        index_t idx1 = *(index_t*)(indexSrc + (indexStride * (idx_i + 1)));
        index_t idx2 = *(index_t*)(indexSrc + (indexStride * (idx_i + 2)));

        const float3& p0 = *(const float3*)(posSrc + posStride * idx0);
        const float3& p1 = *(const float3*)(posSrc + posStride * idx1);
        const float3& p2 = *(const float3*)(posSrc + posStride * idx2);

        const float2& t0 = *(const float2*)(texcoordSrc + texcoordStride * idx0);
        const float2& t1 = *(const float2*)(texcoordSrc + texcoordStride * idx1);
        const float2& t2 = *(const float2*)(texcoordSrc + texcoordStride * idx2);

        float3 dPds = p1 - p0;
        float3 dPdt = p2 - p0;

        float2 dTds = t1 - t0;
        float2 dTdt = t2 - t0;

        float r = 1.0f / (dTds.x * dTdt.y - dTds.y * dTdt.x);
        float3 tangent = r * (dPds * dTdt.y - dPdt * dTds.y);
        float3 bitangent = r * (dPdt * dTds.x - dPds * dTdt.x);

        float tangentLength = length(tangent);
        float bitangentLength = length(bitangent);
        if (tangentLength > 0 && bitangentLength > 0)
        {
            tangent /= tangentLength;
            bitangent /= bitangentLength;

            computedTangents[idx0] += tangent;
            computedTangents[idx1] += tangent;
            computedTangents[idx2] += tangent;
            computedBitangents[idx0] += bitangent;
            computedBitangents[idx1] += bitangent;
            computedBitangents[idx2] += bitangent;
        }
    }

    out_tangentNormalData.resize(positions.count);
    uint32_t* tangentDst = out_tangentNormalData.data();

    for (size_t v_idx = 0; v_idx < positions.count; v_idx++)
    {
        const float3& normal = *(const float3*)(normalSrc + (v_idx * normalStride));
        float3 tangent = computedTangents[v_idx];
        float3 bitangent = computedBitangents[v_idx];

        float sign = 0;
        float tangentLength = length(tangent);
        float bitangentLength = length(bitangent);
        if (tangentLength > 0 && bitangentLength > 0)
        {
            tangent /= tangentLength;
            bitangent /= bitangentLength;
            float3 cross_b = cross(normal, tangent);
            sign = (dot(cross_b, bitangent) > 0) ? -1.f : 1.f;
        }

        *tangentDst = alm::math::VectorToSnorm8(float4(tangent, sign));
        ++tangentDst;
    }
}

std::expected<std::pair<alm::rhi::BufferOwner, alm::SignalListener>, std::string>
CreateIndexBuffer(alm::Blob&& indexData, bool idx32bits, const char* debugName, alm::gfx::DataUploader* dataUploader, alm::rhi::Device* device)
{
    alm::rhi::BufferDesc bufferDesc;
    bufferDesc.memoryAccess = alm::rhi::MemoryAccess::Default;
    bufferDesc.shaderUsage = alm::rhi::BufferShaderUsage::ReadOnly;
    bufferDesc.sizeBytes = indexData.size();
    bufferDesc.stride = idx32bits ? sizeof(int32_t) : sizeof(int16_t);

    std::string name = debugName ? debugName : "{null}";
    name.append(" - IndexBuffer");

    alm::rhi::BufferOwner indexBuffer = device->CreateBuffer(bufferDesc, alm::rhi::ResourceState::COPY_DST, name);
    auto uploadResult = dataUploader->UploadBufferData(
        alm::WeakBlob{ indexData },
        indexBuffer.get_weak(),
        alm::rhi::ResourceState::COPY_DST,
        alm::rhi::ResourceState::INDEX_BUFFER | alm::rhi::ResourceState::SHADER_RESOURCE);

    if (uploadResult)
    {
        return std::make_pair(std::move(indexBuffer), *uploadResult);
    }
    else
    {
        return std::unexpected("Failed upload data to vertex buffer");
    }
}

std::expected<std::pair<alm::rhi::BufferOwner, alm::SignalListener>, std::string>
CreateVertexBuffer(alm::Blob&& vertexData, int vertexStride, const char* debugName, alm::gfx::DataUploader* dataUploader, alm::rhi::Device* device)
{
    alm::rhi::BufferDesc bufferDesc;
    bufferDesc.memoryAccess = alm::rhi::MemoryAccess::Default;
    bufferDesc.shaderUsage = alm::rhi::BufferShaderUsage::ReadOnly;
    bufferDesc.sizeBytes = vertexData.size();
    bufferDesc.stride = vertexStride;

    std::string name = debugName ? debugName : "{null}";
    name.append(" - VertexBuffer");

    auto vertexBuffer = device->CreateBuffer(bufferDesc, alm::rhi::ResourceState::COPY_DST, name);
    auto uploadResult = dataUploader->UploadBufferData(
        alm::WeakBlob{ vertexData },
        vertexBuffer.get_weak(),
        alm::rhi::ResourceState::COPY_DST,
        alm::rhi::ResourceState::SHADER_RESOURCE);

    if (uploadResult)
    {
        return std::make_pair(std::move(vertexBuffer), *uploadResult);
    }
    else
    {
        return std::unexpected("Failed upload data to vertex buffer");
    }
}

std::expected<std::unordered_map<const cgltf_mesh*, std::vector<std::shared_ptr<alm::gfx::Mesh>>>, std::string>
LoadMeshes(const cgltf_data* objects, std::unordered_map<const cgltf_material*, std::shared_ptr<alm::gfx::Material>>& matMap, 
    const char* filename, alm::gfx::DataUploader* dataUploader, alm::rhi::Device* device, std::vector<alm::SignalListener>& out_handlesToWait)
{
    std::unordered_map<const cgltf_mesh*, std::vector<std::shared_ptr<alm::gfx::Mesh>>> meshMap;

    for (size_t mesh_idx = 0; mesh_idx < objects->meshes_count; mesh_idx++)
    {
        const cgltf_mesh& srcMesh = objects->meshes[mesh_idx];
        std::string debugName;
        if (srcMesh.name)
        {
            debugName = srcMesh.name;
        }
        else
        {
            std::filesystem::path path = filename;
            debugName = path.stem().string();
        }
        debugName.append(".Mesh");

        for (size_t prim_idx = 0; prim_idx < srcMesh.primitives_count; prim_idx++)
        {
            const cgltf_primitive& prim = srcMesh.primitives[prim_idx];

            // Only triangle list for the moment
            if (prim.type != cgltf_primitive_type_triangles ||
                prim.attributes_count == 0)
            {
                continue;
            }

            if (prim.indices)
            {
                assert(prim.indices->component_type == cgltf_component_type_r_32u ||
                    prim.indices->component_type == cgltf_component_type_r_16u ||
                    prim.indices->component_type == cgltf_component_type_r_8u);
                assert(prim.indices->type == cgltf_type_scalar);
            }

            const cgltf_accessor* positions = nullptr;
            const cgltf_accessor* normals = nullptr;
            const cgltf_accessor* tangents = nullptr;
            const cgltf_accessor* texcoords = nullptr;
            const cgltf_accessor* joint_weights = nullptr;
            const cgltf_accessor* joint_indices = nullptr;
            const cgltf_accessor* radius = nullptr;

            for (size_t attr_idx = 0; attr_idx < prim.attributes_count; attr_idx++)
            {
                const cgltf_attribute& attr = prim.attributes[attr_idx];

                switch (attr.type)
                {
                case cgltf_attribute_type_position:
                    assert(attr.data->type == cgltf_type_vec3);
                    assert(attr.data->component_type == cgltf_component_type_r_32f);
                    positions = attr.data;
                    break;
                case cgltf_attribute_type_normal:
                    assert(attr.data->type == cgltf_type_vec3);
                    assert(attr.data->component_type == cgltf_component_type_r_32f);
                    normals = attr.data;
                    break;
                case cgltf_attribute_type_tangent:
                    assert(attr.data->type == cgltf_type_vec4);
                    assert(attr.data->component_type == cgltf_component_type_r_32f);
                    tangents = attr.data;
                    break;
                case cgltf_attribute_type_texcoord:
                    assert(attr.data->type == cgltf_type_vec2);
                    assert(attr.data->component_type == cgltf_component_type_r_32f);
                    if (attr.index == 0)
                        texcoords = attr.data;
                    break;
                case cgltf_attribute_type_joints:
                    assert(attr.data->type == cgltf_type_vec4);
                    assert(attr.data->component_type == cgltf_component_type_r_8u || attr.data->component_type == cgltf_component_type_r_16u);
                    joint_indices = attr.data;
                    break;
                case cgltf_attribute_type_weights:
                    assert(attr.data->type == cgltf_type_vec4);
                    assert(attr.data->component_type == cgltf_component_type_r_8u || attr.data->component_type == cgltf_component_type_r_16u || attr.data->component_type == cgltf_component_type_r_32f);
                    joint_weights = attr.data;
                    break;
                case cgltf_attribute_type_custom:
                    if (strncmp(attr.name, "_RADIUS", 7) == 0)
                    {
                        assert(attr.data->type == cgltf_type_scalar);
                        assert(attr.data->component_type == cgltf_component_type_r_32f);
                        radius = attr.data;
                    }
                    break;
                default:
                    break;
                }
            }
            // Positions at least
            assert(positions);

            // Indices
            bool idx32bits = false;
            if (prim.indices)
            {
                idx32bits = (prim.indices->component_type == cgltf_component_type_r_32u);
            }
            else
            {
                idx32bits = positions->count > std::numeric_limits<uint16_t>::max();
            }
            alm::Blob indexData;
            if (idx32bits)
            {
                CollectPrimitiveIndices<uint32_t>(prim, *positions, indexData);
            }
            else
            {
                CollectPrimitiveIndices<uint16_t>(prim, *positions, indexData);
            }

            // Positions and bounds
            std::vector<glm::vec3> vertexPosData;
            alm::math::aabox3f bounds;
            CollectPrimitivePositions(*positions, vertexPosData, bounds);

            std::vector<uint32_t> vertexNormalData;
            if (normals)
            {
                assert(normals->count == positions->count);
                CollectPrimitiveNormals(*normals, vertexNormalData);
            }

            const bool forceRebuildTangets = false;
            std::vector<uint32_t> vertexTangentData;
            if (tangents && !forceRebuildTangets)
            {
                assert(tangents->count == positions->count);
                CollectPrimitiveTangents(*tangents, vertexTangentData);
            }
            else if (normals && texcoords)
            {
                if (idx32bits)
                {
                    ComputeTangents<uint32_t>(*prim.indices, *positions, *normals, *texcoords, vertexTangentData);
                }
                else
                {
                    ComputeTangents<uint16_t>(*prim.indices, *positions, *normals, *texcoords, vertexTangentData);
                }
            }

            std::vector<glm::vec2> vertexTexCoordData;
            if (texcoords)
            {
                assert(texcoords->count == positions->count);
                CollectPrimitiveTexcoords(*texcoords, vertexTexCoordData);
            }

            // TODO: joint_indices
            // TODO: joint_weights

            // Create mesh
            std::string meshName = debugName;
            if (srcMesh.primitives_count > 1)
            {
                std::stringstream ss;
                ss << debugName << "[" << prim_idx << "]";
                meshName = ss.str();
            }
            auto mesh = std::make_shared<alm::gfx::Mesh>(device, meshName.c_str(), filename);
            mesh->SetBounds(bounds);

            // Assign material
            if (prim.material)
            {
                mesh->SetMaterial(matMap[prim.material]);
            }
            else
            {
                LOG_WARNING("Geometry {} for mesh {} doesn't have a material.", prim_idx, debugName.c_str());
                
                static std::shared_ptr<alm::gfx::Material> emptyMaterial;
                if (!emptyMaterial)
                {
                    emptyMaterial = std::make_shared<alm::gfx::Material>(device, "(empty)", filename);
                }
                mesh->SetMaterial(emptyMaterial);
            }

            // Index buffer
            auto indexBufferResult = CreateIndexBuffer(std::move(indexData), idx32bits, debugName.c_str(), dataUploader, device);
            if (indexBufferResult)
            {
                mesh->SetIndexBuffer(std::move(indexBufferResult->first), alm::rhi::PrimitiveTopology::TriangleList, idx32bits ? sizeof(int32_t) : sizeof(int16_t));
                out_handlesToWait.push_back(indexBufferResult->second);
            }

            // Vertex buffer
            int vertexStride = 0;
            constexpr int posElemSize = sizeof(decltype(vertexPosData)::value_type);
            if (!vertexPosData.empty())
                vertexStride += posElemSize;
            constexpr int normalElemSize = sizeof(decltype(vertexNormalData)::value_type);
            if (!vertexNormalData.empty())
                vertexStride += normalElemSize;
            constexpr int tangentElemSize = sizeof(decltype(vertexTangentData)::value_type);
            if (!vertexTangentData.empty())
                vertexStride += tangentElemSize;
            constexpr int texCoordElemSize = sizeof(decltype(vertexTexCoordData)::value_type);
            if (!vertexTexCoordData.empty())
                vertexStride += texCoordElemSize;

            alm::Blob vertexData{ (char*)malloc(vertexStride * vertexPosData.size()), vertexStride * vertexPosData.size() };

            // Interleave
            uint32_t vertexOffset = 0;
            alm::gfx::Mesh::VertexFormat vertexFormat;
            // Positions
            if (!vertexPosData.empty())
            {
                char* dstData = vertexData.data() + vertexOffset;
                for (size_t v_idx = 0; v_idx < vertexPosData.size(); v_idx++)
                {
                    *(decltype(vertexPosData)::value_type*)dstData = vertexPosData[v_idx];
                    dstData += vertexStride;
                }
                vertexFormat.PositionOffset = vertexOffset;
                vertexOffset += posElemSize;
            }
            // Normals
            if (!vertexNormalData.empty())
            {
                char* dstData = vertexData.data() + vertexOffset;
                for (size_t v_idx = 0; v_idx < vertexNormalData.size(); v_idx++)
                {
                    *(decltype(vertexNormalData)::value_type*)dstData = vertexNormalData[v_idx];
                    dstData += vertexStride;
                }
                vertexFormat.NormalOffset = vertexOffset;
                vertexOffset += normalElemSize;
            }
            // Tangents
            if (!vertexTangentData.empty())
            {
                char* dstData = vertexData.data() + vertexOffset;
                for (size_t v_idx = 0; v_idx < vertexTangentData.size(); v_idx++)
                {
                    *(decltype(vertexTangentData)::value_type*)dstData = vertexTangentData[v_idx];
                    dstData += vertexStride;
                }
                vertexFormat.TangentOffset = vertexOffset;
                vertexOffset += tangentElemSize;
            }
            // TexCoords
            if (!vertexTexCoordData.empty())
            {
                char* dstData = vertexData.data() + vertexOffset;
                for (size_t v_idx = 0; v_idx < vertexTexCoordData.size(); v_idx++)
                {
                    *(decltype(vertexTexCoordData)::value_type*)dstData = vertexTexCoordData[v_idx];
                    dstData += vertexStride;
                }
                vertexFormat.TexCoord0Offset = vertexOffset;
                vertexOffset += texCoordElemSize;
            }
            vertexFormat.VertexStride = vertexOffset;

            // Create vertex buffer
            auto vertexBufferResult = CreateVertexBuffer(std::move(vertexData), vertexStride, debugName.c_str(), dataUploader, device);
            if (vertexBufferResult)
            {
                mesh->SetVertexBuffer(std::move(vertexBufferResult->first), vertexFormat);
                out_handlesToWait.push_back(vertexBufferResult->second);
            }

            meshMap[&srcMesh].push_back(mesh);
        }
    }

    return meshMap;
}

int ChildIndex(const cgltf_node* node, const cgltf_node* parent, const cgltf_scene* scene)
{
    cgltf_node** childp;
    if (parent)
    {
        childp = std::find(parent->children, parent->children + parent->children_count, node);
        assert(childp != parent->children + parent->children_count);
        return childp - parent->children;
    }
    else
    {
        childp = std::find(scene->nodes, scene->nodes + scene->nodes_count, node);
        assert(childp != scene->nodes + scene->nodes_count);
        return childp - scene->nodes;
    }
};

bool LastChild(const cgltf_node* node, const cgltf_node* parent, const cgltf_scene* scene)
{
    int i = ChildIndex(node, parent, scene);
    return parent ? (i == parent->children_count - 1) : (i == scene->nodes_count - 1);
};

const cgltf_node* NextSibling(const cgltf_node* node, const cgltf_node* parent, const cgltf_scene* scene)
{
    cgltf_node** childp;
    if (parent)
    {
        childp = std::find(parent->children, parent->children + parent->children_count, node);
        assert(childp != parent->children + parent->children_count);
        int idx = childp - parent->children;
        return (idx < parent->children_count - 1) ? parent->children[idx + 1] : nullptr;
    }
    else
    {
        childp = std::find(scene->nodes, scene->nodes + scene->nodes_count, node);
        assert(childp != scene->nodes + scene->nodes_count);
        int idx = childp - scene->nodes;
        return (idx < scene->nodes_count - 1) ? scene->nodes[idx + 1] : nullptr;
    }
}

} // anonymous namespace

// ---------------------------------------------------------------------------------------------------------------------------------------

namespace alm::gfx
{

std::expected<alm::unique<alm::gfx::SceneGraphNode>, std::string>
ImportGlTF(const char* path, alm::gfx::DeviceManager* device)
{
    std::string filename = std::filesystem::path(path).filename().string();

    LoadTexCache loadTexCache{};
    loadTexCache.path = path;
    loadTexCache.textureCache = device->GetTextureCache();

    FleContext fileContext;

    cgltf_options options{};
    options.file.read = &ReadFileCB;
    options.file.release = &ReleaseFileCB;
    options.file.user_data = &fileContext;

    cgltf_data* objects = nullptr;
    cgltf_result res = cgltf_parse_file(&options, path, &objects);
    if (res != cgltf_result_success)
    {
        LOG_ERROR("Couldn't load glTF file '{}': {}", path, ErrorToString(res));
        return std::unexpected(ErrorToString(res));
    }

    res = cgltf_load_buffers(&options, objects, path);
    if (res != cgltf_result_success)
    {
        LOG_ERROR("Failed to load buffers for glTF file '%s': ", path, ErrorToString(res));
        return std::unexpected(ErrorToString(res));
    }

    std::vector<alm::SignalListener> handlesToWait;

    // Materials
    auto matMap = GetMaterialsMap(objects, loadTexCache, options, handlesToWait, device->GetDevice());

    // Meshes
    auto loadMeshesResult = LoadMeshes(objects, matMap, path, device->GetDataUploader(), device->GetDevice(), handlesToWait);
    auto meshMap = *loadMeshesResult;

    // Cameras
    std::unordered_map<const cgltf_camera*, std::shared_ptr<SceneCamera>> cameraMap;
    for (size_t camera_idx = 0; camera_idx < objects->cameras_count; camera_idx++)
    {
        const cgltf_camera* src = &objects->cameras[camera_idx];
        std::shared_ptr<SceneCamera> camera;

        if (src->type == cgltf_camera_type_perspective)
        {
            std::shared_ptr<PerspectiveCamera> perspectiveCamera = std::make_shared<PerspectiveCamera>();
            perspectiveCamera->SetNear(src->data.perspective.znear);
            perspectiveCamera->SetVerticalFOV(src->data.perspective.yfov);
            if (src->data.perspective.has_zfar)
                perspectiveCamera->SetFar(src->data.perspective.zfar);
            if (src->data.perspective.has_aspect_ratio)
                perspectiveCamera->SetAspect(src->data.perspective.aspect_ratio);
            
            camera = perspectiveCamera;
        }
        else
        {
            std::shared_ptr<OrthographicCamera> orthoCamera = std::make_shared<OrthographicCamera>();
            orthoCamera->SetNear(src->data.orthographic.znear)
                .SetFar(src->data.orthographic.zfar)
                .SetXMag(src->data.orthographic.xmag)
                .SetYMag(src->data.orthographic.ymag);

            camera = orthoCamera;
        }
        cameraMap[src] = camera;
    }
    
    // Build scene
    assert(objects->scenes_count == 1); // only 1 scene allowed

    //auto sceneGraph = alm::make_unique_with_weak<alm::gfx::SceneGraph>();
    auto rootNode = alm::make_unique_with_weak<SceneGraphNode>();
    rootNode->SetName(filename.c_str());

    std::vector<std::pair<const cgltf_node*, alm::weak<alm::gfx::SceneGraphNode>>> stack;
    stack.emplace_back(nullptr, rootNode.get_weak());

    if (!objects->scene)
    {
        objects->scene = objects->scenes;
    }
    const cgltf_node* srcNode = *objects->scene->nodes;
    while (srcNode)
    {
        auto dstNode = alm::make_unique_with_weak<SceneGraphNode>();

        if (srcNode->has_matrix)
        {
            dstNode->SetLocalTransform(alm::gfx::Transform{ glm::make_mat4(srcNode->matrix) });
        }
        else
        {
            alm::gfx::Transform t;
            if (srcNode->has_scale)
                t.SetScale(glm::vec3{ srcNode->scale[0], srcNode->scale[1], srcNode->scale[2] });
            if (srcNode->has_rotation)
                t.SetRotation(glm::quat{ srcNode->rotation[3], srcNode->rotation[0], srcNode->rotation[1], srcNode->rotation[2] });
            if (srcNode->translation)
                t.SetTranslation(glm::vec3{ srcNode->translation[0], srcNode->translation[1], srcNode->translation[2] });
            dstNode->SetLocalTransform(t);
        }

        if (srcNode->name)
            dstNode->SetName(srcNode->name);

        if (srcNode->mesh)
        {
            assert(!srcNode->camera && !srcNode->light);

            auto found = meshMap.find(srcNode->mesh);
            if (found != meshMap.end() && !found->second.empty())
            {
                if (found->second.size() == 1)
                {
                    auto leaf = alm::make_unique_with_weak<alm::gfx::MeshInstance>(found->second[0]);
                    dstNode->SetLeaf(std::move(leaf));
                }
                else
                {
                    for (int meshIdx = 0; meshIdx < found->second.size(); ++meshIdx)
                    {
                        auto meshNode = alm::make_unique_with_weak<SceneGraphNode>();
                        std::stringstream ss;
                        ss << dstNode->GetName() << "[" << meshIdx << "]";
                        meshNode->SetName(ss.str().c_str());
                        
                        auto leaf = alm::make_unique_with_weak<alm::gfx::MeshInstance>(found->second[meshIdx]);
                        meshNode->SetLeaf(std::move(leaf));

                        dstNode->AddChild(std::move(meshNode));
                    }
                }
            }
            else
            {
                LOG_ERROR("Could not find mesh for node '{}', from mesh '{}', from file '{}'",
                    srcNode->name ? srcNode->name : "<noname>",
                    srcNode->mesh->name ? srcNode->mesh->name : "<noname>",
                    path);
            }
        }

        if (srcNode->camera)
        {
            assert(!srcNode->mesh && !srcNode->light);

            auto found = cameraMap.find(srcNode->camera);
            if (found != cameraMap.end())
            {
                auto camera = found->second;
            }
        }

        if (srcNode->light)
        {
            assert(!srcNode->mesh && !srcNode->camera);
            switch (srcNode->light->type)
            {
            case cgltf_light_type_directional:
            {
                auto leaf = alm::make_unique_with_weak<alm::gfx::SceneDirectionalLight>();
                leaf->SetColor(*(float3*)&srcNode->light->color);
                leaf->SetIrradiance(srcNode->light->intensity);
                leaf->SetAngularSize(0.075f);
                dstNode->SetLeaf(std::move(leaf));
            } break;

            case cgltf_light_type_point:
            {
                auto leaf = alm::make_unique_with_weak<alm::gfx::ScenePointLight>();
                if(srcNode->light->name)
                    leaf->SetName(srcNode->light->name);
                leaf->SetColor(*(float3*)&srcNode->light->color);
                leaf->SetIntensity(srcNode->light->intensity);
                if (srcNode->light->range > 1.0e-05f)
                {
                    leaf->SetRange(srcNode->light->range);
                }
                else
                {
                    const float threshold = 1.0f / 256.0f;
                    leaf->SetRange(sqrtf(srcNode->light->intensity / threshold));
                }
                dstNode->SetLeaf(std::move(leaf));                
            } break;

            case cgltf_light_type_spot:
            {
                auto leaf = alm::make_unique_with_weak<alm::gfx::SceneSpotLight>();
                if (srcNode->light->name)
                    leaf->SetName(srcNode->light->name);
                leaf->SetColor(*(float3*)&srcNode->light->color);
                leaf->SetIntensity(srcNode->light->intensity);
                if (srcNode->light->range > 1.0e-05f)
                {
                    leaf->SetRange(srcNode->light->range);
                }
                else
                {
                    const float threshold = 1.0f / 256.0f;
                    leaf->SetRange(sqrtf(srcNode->light->intensity / threshold));
                }
                leaf->SetInnerConeAngle(srcNode->light->spot_inner_cone_angle);
                leaf->SetOuterConeAngle(srcNode->light->spot_outer_cone_angle);
                dstNode->SetLeaf(std::move(leaf));
            } break;

            default:
                assert(0);
            }
        }

        // Do we have parent? Then attach to parent.
        auto attachedNode = dstNode.get_weak();
        if (!stack.empty())
        {
            stack.back().second->AddChild(std::move(dstNode));
        }
        // Else, we are the root
        else
        {
            rootNode = std::move(dstNode);
        }

        // Do we have children? Then push ourshelve to the stack
        // and the first child is the next node to process
        if (srcNode->children_count)
        {
            stack.emplace_back(srcNode, attachedNode);
            srcNode = srcNode->children[0];
        }
        else
        {
            const cgltf_node* parentNode = stack.empty() ? nullptr : stack.back().first;
            // If we are the last child, go up the stack finding any node still to process
            while (parentNode && LastChild(srcNode, parentNode, objects->scene))
            {
                srcNode = parentNode;
                stack.pop_back();
                parentNode = stack.empty() ? nullptr : stack.back().first;
            }
            srcNode = NextSibling(srcNode, parentNode, objects->scene);
        }        
    }

    // Wait uploads
    for (const auto& signal : handlesToWait)
    {
        signal.Wait();
    }

    //sceneGraph->SetRoot(std::move(rootNode));
    //return sceneGraph;
    return rootNode;
}

} // namespace st::gfx