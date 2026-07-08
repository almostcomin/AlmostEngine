#include "Interop/RenderResources.h"
#include "BindlessRS.hlsli"
#include "Noise.hlsli"
#include "Common.hlsli"

ConstantBuffer<interop::CloudsConstants> Constants : register(b0);

struct PS_INPUT
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

struct CloudResult
{
    float3 color;
    float transmittance;
    float weightedDistance; // Density-weighted average distance
    float tEntry;
    float tExit;
};

float IntersectCloudSphere(float3 rd, float r, float earthRadius)
{
    float b = earthRadius * rd.y;
    float d = b * b + r * r + 2.0 * earthRadius * r;
    return -b + sqrt(d);
}

float SampleCloudDensity(float3 pos, float norY, Texture3D cloudsTexture, Texture3D cloudsDetailTexture,
    ConstantBuffer <interop::CloudsData> cloudsData)
{    
    // Si norY está fuera del rango válido [0,1], no hay densidad
    if (norY < 0.0 || norY > 1.0)
        return 0.0;

    // --- BASE SHAPE

    float3 uvw1;
    uvw1.xy = pos.xz * cloudsData.cloudsScale;
    uvw1.xy += cloudsData.windOffset;
    uvw1.z = norY;
    float3 uvw2;
    uvw2.xy = pos.xz * cloudsData.cloudsScale * 0.37; // prime number
    uvw2.xy += cloudsData.windOffset * 0.73;
    uvw2.z = frac(norY + 0.5);
        
    float4 noise1 = cloudsTexture.SampleLevel(linearWrapSampler, uvw1, 0.0);
    float4 noise2 = cloudsTexture.SampleLevel(linearWrapSampler, uvw2, 0.0);
    float4 noise = lerp(noise1, noise2, 0.3);

    float perlinWorley = noise.r;
    float3 worley = noise.gba;
    
    // cloud shape modeled after the GPU Pro 7 chapter
    float wfbm = worley.r * 0.625 + worley.g * 0.25 + worley.b * 0.125;
    float coverage = remap(perlinWorley, wfbm - 1.0, 1.0, 0.0, 1.0);
    coverage = remap(coverage, 1.0 - cloudsData.coverage, 1.0, 0.0, 1.0);

    if (coverage <= 0.0)
        return 0.0;

    coverage = saturate(coverage);
    //coverage = pow(coverage, 2.0f);

    // Gradiente de densidad vertical — nubes más densas en el centro de la capa
    //float heightGradient = saturate(remap(uvw.z, 0.0f, 0.1f, 0.0f, 1.0f))   // fade inferior
    //                     * saturate(remap(uvw.z, 0.6f, 1.0f, 1.0f, 0.0f));  // fade superior
    //coverage *= heightGradient;
    
    // -- DETAIL EROSION

    float3 detailUVW;
    detailUVW.xy = pos.xz * cloudsData.cloudsScale * cloudsData.detailScale;
    detailUVW.xy += cloudsData.windOffset * cloudsData.detailScale;
    detailUVW.z  = norY;

    float3 detail = cloudsDetailTexture.SampleLevel(linearWrapSampler, detailUVW, 0.0).rgb;
    float hfbm = detail.r * 0.625 + detail.g * 0.25 + detail.b * 0.125;

    // Truco paper: en la mitad baja (cerca del suelo de la capa),
    // invertir la worley → bordes wispy/transparentes.
    // norY ∈ [0,1]: bajo = wispy, alto = billowy (cumulus).
    hfbm = lerp(1.0 - hfbm, hfbm, smoothstep(0.0, 0.5, norY));

    // Threshold móvil controlado por detail strength.
    // Empuja las densidades por debajo de `hfbm*strength` a 0.
    float threshold = hfbm * cloudsData.detailErosionStrength;
    float eroded = remap(coverage, threshold, 1.0, 0.0, 1.0);

    return saturate(eroded);
}

float VolumetricShadow(float3 from, float shadowJitter, Texture3D cloudsTexture, Texture3D cloudsDetailTexture,
    ConstantBuffer <interop::CloudsData> cloudsData)
{
    // Proportional step size
    float layerThickness = cloudsData.cloudLayerMax - cloudsData.cloudLayerMin;
    float shadowStepSize = layerThickness / (float)cloudsData.lightSteps;

    float shadowD = shadowJitter * shadowStepSize; // [0, shadowStepSize)
    float shadow = 1.0;

    for(int i = 0; i < cloudsData.lightSteps; ++i)
    {
        float3 pos = from + cloudsData.toSunDirection * shadowD;
        float altitude = length(pos) - cloudsData.earthRadius;
        float norY = (altitude - cloudsData.cloudLayerMin) * cloudsData.invCloudLayerThickness;
        if (norY > 1.0 || norY < 0.0)
            break;

        float density = SampleCloudDensity(pos, norY, cloudsTexture, cloudsDetailTexture, cloudsData);
        shadow *= exp(-density * shadowStepSize * cloudsData.absorptionCoeff);
        
        shadowD += shadowStepSize;
    }
    return shadow;
}

bool GetCloudsLayerIntersectionPoints(
    float3 rayOrigin,
    float3 rayDir,
    float earthRadius,          // Solid Earth radius
    float innerSphereRadius,    // Cloud base altitude (earthRadius + cloudMinHeight)
    float outerSphereRadius,    // Cloud top altitude (earthRadius + cloudMaxHeight)
    float tMax,                 // Additional solid occluder (e.g., depth buffer for mountains/buildings)
    out float tEntry,
    out float tExit)
{
    tEntry = 0.0;
    tExit = 0.0;

    // Intersections with cloud layer boundaries
    float2 innerHit = RaySphereIntersection(rayOrigin, rayDir, float3(0, 0, 0), innerSphereRadius);
    float2 outerHit = RaySphereIntersection(rayOrigin, rayDir, float3(0, 0, 0), outerSphereRadius);

    // If the outer sphere is entirely behind the origin or misses it, no clouds ahead
    if (outerHit.y < 0.0) 
        return false;

    // Entry: start at outerHit.x or 0 if inside, or innerHit.y if inside inner sphere
    tEntry = max(max(outerHit.x, 0.0), (innerHit.x < 0.0 && innerHit.y > 0.0) ? innerHit.y : 0.0);

    // Exit: always go to outer boundary, let density be zero inside inner sphere
    tExit = outerHit.y;

    // Solid Earth occlusion
    float2 earthHit = RaySphereIntersection(rayOrigin, rayDir, float3(0, 0, 0), earthRadius);
    if (length(rayOrigin) >= earthRadius)
    {
        if (earthHit.x > 0.0)
            tExit = min(tExit, earthHit.x);
    }
    else
    {
        if (earthHit.y > 0.0)
            tEntry = max(tEntry, earthHit.y);
        else
            return false;
    }

    // Solid geometry occlusion
    tExit = min(tExit, tMax);

    if (tEntry >= tExit)
        return false;

    return true;
}

// rayOriginLocal: camera position in world space
// rayDir:         normalized ray direction in world space
CloudResult GetCloudsColorRayMarch(float3 rayOriginLocal, float3 rayDir, Texture3D cloudsTexture, Texture3D cloudsDetailTexture,
    ConstantBuffer<interop::CloudsData> cloudsData, float sceneDist, float mainJitter, float shadowJitter)
{
    CloudResult result;
    result.color = 0.0;
    result.transmittance = 1.0;
    result.weightedDistance = 0.0;
    result.tEntry = 0.0;
    result.tExit = 0.0;
    
    // Translate ray origin to Earth-centered coordinates.
    float3 rayOrigin = rayOriginLocal - cloudsData.earthCenter;
    
    float tEntry, tExit;
    if (!GetCloudsLayerIntersectionPoints(rayOrigin, rayDir, cloudsData.earthRadius, cloudsData.earthRadius + cloudsData.cloudLayerMin,
        cloudsData.earthRadius + cloudsData.cloudLayerMax, sceneDist, tEntry, tExit))
    {
        return result;
    }
            
    float stepSize = (tExit - tEntry) / cloudsData.maxSteps;
        
    float t = tEntry + mainJitter * stepSize;
    float totalDensity = 0.0;
    
    for (uint step = 0; step < cloudsData.maxSteps; ++step)
    {
        float3 pos = rayOrigin + rayDir * t;
        float altitude = length(pos) - cloudsData.earthRadius;
        float norY = (altitude - cloudsData.cloudLayerMin) * cloudsData.invCloudLayerThickness;

        float density = SampleCloudDensity(pos, norY, cloudsTexture, cloudsDetailTexture, cloudsData);
        
        float fadeFactor = saturate(1.0 - pow(t / cloudsData.cloudFadeDistance, 2.0));
        density *= fadeFactor;
        
        if (density > 0.0)
        {
            float absorption = exp(-density * stepSize * cloudsData.absorptionCoeff);
            float lightEnergy = VolumetricShadow(pos, shadowJitter, cloudsTexture, cloudsDetailTexture, cloudsData);
            lightEnergy += 0.2; // ambient

            float3 S = lightEnergy * density;
            float dTrans = absorption;
            float3 Sint = (S - S * dTrans) * (1.0f / max(density, 0.0001f)); // analytical integral
            
            result.color += result.transmittance * Sint;
            result.transmittance *= dTrans;
            result.weightedDistance += t * density;

            totalDensity += density;
            if (result.transmittance < 0.001)
                break;
        }
        
        t += stepSize;
    }    
    
    result.weightedDistance /= max(totalDensity, 0.0001f);
    result.tEntry = tEntry;
    result.tExit = tExit;

    return result;
}

[RootSignature(BindlessRootSignature)]
float4 main(PS_INPUT input) : SV_Target
{
    ConstantBuffer<interop::CloudsData> cloudsData = ResourceDescriptorHeap[Constants.cloudsDataDI];    
    Texture3D<float4> baseTexture = ResourceDescriptorHeap[cloudsData.cloudsBaseShapeTexture];
    Texture3D<float4> detailTexture = ResourceDescriptorHeap[cloudsData.cloudsDetailTexture];
    Texture2D<float> linearDepthTex = ResourceDescriptorHeap[cloudsData.linearDepthTexDI];
    
    // Reconstruct world-space ray direction from clip-space coordinates.
    // matClipToTranslatedWorld transforms from clip space to world space
    // with the camera at the origin (translated world), so no camera translation
    // is baked into the matrix -- avoids floating point precision issues at large distances.
    float4 clipPos;
    clipPos.x = input.uv.x * 2.0 - 1.0;
    clipPos.y = 1.0 - input.uv.y * 2.0; // flip Y: UV origin is top-left, clip space origin is bottom-left
    clipPos.z = 1.0;
    clipPos.w = 1.0;

    float4 rayDirH = mul(Constants.matClipToTranslatedWorld, clipPos);
    float3 rayDir = normalize(rayDirH.xyz);

    // Read linear depth (distance along the camera forward axis) and convert to scene distance along the ray direction.
    // viewZ: depth along camera forward vector (Z component in view space)
    // sceneDist: actual distance along rayDir to the geometry
    float viewZ = linearDepthTex.SampleLevel(pointClampSampler, input.uv, 0.0);
    float cosAngle = dot(rayDir, cloudsData.cameraForward);
    float sceneDist = viewZ / max(cosAngle, 0.0001);
    
    // Calc jitter
    float2 pixelPos = input.uv * Constants.viewportSize;
    float mainJitter = InterleavedGradientNoise(pixelPos);
    float shadowJitter = InterleavedGradientNoise(pixelPos + float2(0.5, 0.5));
    //float jitter = InterleavedGradientNoise(pixelPos + float2(Constants.frameIndex * 5.588238f, Constants.frameIndex * 3.424234f));
    
    CloudResult clouds = GetCloudsColorRayMarch(
        Constants.cameraPosition, rayDir, baseTexture, detailTexture, cloudsData, sceneDist, mainJitter, shadowJitter);
    
    // Punto estable: centro del segmento atravesado por el rayo dentro de la capa.
    // Es EXACTO para este rayo (no depende del muestreo) y NO SALTA con jitter.
    float hit_t = 0.5f * (clouds.tEntry + clouds.tExit);
    float blendToMass = saturate((1.0f - clouds.transmittance) * (1.0f - clouds.transmittance));
    hit_t = lerp(hit_t, clouds.weightedDistance, blendToMass);
    
    float3 hitPosWorld = Constants.cameraPosition + rayDir * hit_t;
    
    // hit in prev frame
    float4 prevClipPos = mul(cloudsData.matPrevFrameViewProj, float4(hitPosWorld, 1.0));
    prevClipPos.xyz /= prevClipPos.w;
    // Convert NDC to uv
    float2 prevUv = prevClipPos.xy * float2(0.5, -0.5) + 0.5;
    bool prevUvValid = all(prevUv > 0.0) && all(prevUv < 1.0)
                       && prevClipPos.w > 0.0
                       && abs(prevClipPos.z) <= prevClipPos.w;;

    bool hasHit = clouds.transmittance < 0.999;
    
    float4 currentColor = float4(clouds.color, clouds.transmittance);
    float4 finalColor = currentColor;
    
    if (prevUvValid && hasHit)
    {
        Texture2D<float4> prevCloudsTex = ResourceDescriptorHeap[cloudsData.prevCloudsTexDI];
        float4 prevColor = prevCloudsTex.SampleLevel(linearClampSampler, prevUv, 0.0);
        
        bool historyHadCloud = prevColor.a < 0.999;
        if (historyHadCloud)
        {
            finalColor = lerp(prevColor, currentColor, 0.75);
        }
        else
        {
            finalColor = currentColor;
        }
    }
    
    return finalColor;
}