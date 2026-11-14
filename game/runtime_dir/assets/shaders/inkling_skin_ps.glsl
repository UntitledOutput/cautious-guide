#version 430
#extension GL_ARB_texture_gather : enable
#extension GL_ARB_separate_shader_objects : enable
// shader 61dabf4888ca7066 - inkling skin (pixel), hand cleaned
#ifdef VULKAN
#define ATTR_LAYOUT(__vkSet, __location) layout(set = __vkSet, location = __location)
#define UNIFORM_BUFFER_LAYOUT(__glLocation, __vkSet, __vkLocation) layout(set = __vkSet, binding = __vkLocation, std140)
#define TEXTURE_LAYOUT(__glLocation, __vkSet, __vkLocation) layout(set = __vkSet, binding = __vkLocation)
#define GET_FRAGCOORD() vec4(gl_FragCoord.xy*uf_fragCoordScale.xy,gl_FragCoord.z, 1.0/gl_FragCoord.w)
#extension GL_EXT_spirv_intrinsics: enable
spirv_execution_mode(4462, 16);
spirv_execution_mode(4462, 32);
spirv_execution_mode(4462, 64);
#else
#define ATTR_LAYOUT(__vkSet, __location) layout(location = __location)
#define UNIFORM_BUFFER_LAYOUT(__glLocation, __vkSet, __vkLocation) layout(binding = __glLocation, std140)
#define TEXTURE_LAYOUT(__glLocation, __vkSet, __vkLocation) layout(binding = __glLocation)
#define GET_FRAGCOORD() vec4(gl_FragCoord.xy*uf_fragCoordScale,gl_FragCoord.zw)
#endif

#ifdef VULKAN
layout(set = 1, binding = 11) uniform ufBlock
{
uniform ivec4 uf_remappedPS[18];
uniform vec4 uf_fragCoordScale;
};
#else
uniform ivec4 uf_remappedPS[18];
uniform vec2 uf_fragCoordScale;
#endif

TEXTURE_LAYOUT(0, 1, 0) uniform samplerCubeArray textureUnitPS0;
TEXTURE_LAYOUT(1, 1, 1) uniform samplerCubeArray textureUnitPS1;
TEXTURE_LAYOUT(2, 1, 2) uniform sampler2DArray textureUnitPS2;
TEXTURE_LAYOUT(3, 1, 3) uniform sampler2D textureUnitPS3;
TEXTURE_LAYOUT(4, 1, 4) uniform sampler2D textureUnitPS4;
TEXTURE_LAYOUT(5, 1, 5) uniform sampler2D textureUnitPS5;
TEXTURE_LAYOUT(6, 1, 6) uniform sampler2D textureUnitPS6;
TEXTURE_LAYOUT(7, 1, 7) uniform sampler2D textureUnitPS7;
TEXTURE_LAYOUT(10, 1, 8) uniform sampler2D textureUnitPS10;
TEXTURE_LAYOUT(11, 1, 9) uniform sampler2D textureUnitPS11;
TEXTURE_LAYOUT(12, 1, 10) uniform sampler2D textureUnitPS12;

#define envSpecMap       textureUnitPS0   // prefiltered reflection cubes
#define envDiffuseMap    textureUnitPS1   // ambient light map cube
#define lightBuffer      textureUnitPS2   // screen-space light buffer, layer 0 diffuse, layer 1 specular
#define shadowMap        textureUnitPS3
#define screenShadowMap  textureUnitPS4
#define translucencyMap  textureUnitPS5
#define inkMaskMap       textureUnitPS6
#define albedoMap2       textureUnitPS7
#define albedoMap        textureUnitPS10
#define normalMap        textureUnitPS11
#define specMaskMap      textureUnitPS12

layout(location = 0) in vec4 passParameterSem0;
layout(location = 1) in vec4 passParameterSem1;
layout(location = 2) in vec4 passParameterSem2;
layout(location = 3) in vec4 passParameterSem3;
layout(location = 4) in vec4 passParameterSem4;
layout(location = 5) in vec4 passParameterSem6;
layout(location = 6) in vec4 passParameterSem7;
layout(location = 7) in vec4 passParameterSem8;
layout(location = 8) in vec4 passParameterSem254; // unused
layout(location = 0) out vec4 passPixelColor0;

// uniform names are guesses based on how each value gets used
#define CB(i) intBitsToFloat(uf_remappedPS[i])

#define vtxOcclusionBias    CB(0).z
#define inkColor            CB(1).rgb
#define inkMaskOffset       CB(1).w
#define inkMaskMin          CB(2).x
#define inkMaskBias         CB(2).y
#define inkRoughness        CB(3).x
#define inkRimIntensity     CB(3).y
#define inkEmission         CB(3).z
#define inkNormalFlatten    CB(3).w
#define skinRoughness       CB(4).x
#define skinRimPower        CB(5).x
#define translucencyColor   (CB(6).rgb * CB(6).w)
#define inkRimPower         CB(7).x
#define specColor           CB(8).rgb
#define skinRimIntensity    CB(8).w
#define backRimPower        CB(9).x
#define backRimIntensity    CB(9).y
#define shadowMapWeight     CB(10).x
#define albedoTint          CB(11).rgb
#define emissionColor       (CB(12).rgb * CB(12).w)
#define shadowStrength      CB(13).w
#define backAmbientLodScale CB(14).x
#define emissionPower       CB(14).z
#define lightBufferShadow   CB(15).z
#define envScale            CB(16).x
#define envExponent         CB(16).y
#define rimShadow           CB(17).y

// same as the log2/exp2 pow on Latte: pow(0, p) = 0, pow(x, 0) = 1
float lattePow(float x, float p)
{
    float l = log2(max(x, 0.0));
    if (isinf(l)) l = -3.40282347e38;
    return exp2(p == 0.0 ? 0.0 : p * l);
}

vec3 lattePow(vec3 x, float p)
{
    return vec3(lattePow(x.r, p), lattePow(x.g, p), lattePow(x.b, p));
}

// the game's cubemaps have z flipped
vec3 cubeDir(vec3 v)
{
    return vec3(v.x, v.y, -v.z);
}

void main()
{
    vec2  uv0          = passParameterSem0.xy;
    float vtxOcclusion = passParameterSem0.z;
    float vtxAlpha     = passParameterSem0.w;
    vec4  fog          = passParameterSem1;      // rgb color, w amount
    vec4  clipPos      = passParameterSem2;
    vec4  tangent      = passParameterSem3;      // w = bitangent sign
    vec3  normal       = passParameterSem4.xyz;
    vec3  viewDir      = passParameterSem6.xyz;  // eye -> surface
    vec2  uv1          = passParameterSem7.xy;
    vec3  shadowCoord  = passParameterSem8.xyz;

    vec2 screenUV = clipPos.xy / clipPos.w;
    vec2 shadowUV = shadowCoord.xy / shadowCoord.z;

    // normal map (two channel, z rebuilt)
    vec2 nxy = texture(normalMap, uv0).xy;
    float nz = sqrt(clamp(1.0 - dot(nxy, nxy), 0.0, 1.0));
    vec3 bitangent = cross(normal, tangent.xyz) * tangent.w;
    vec3 n = normalize(tangent.xyz * nxy.x + bitangent * nxy.y + normal * nz);
    n *= gl_FrontFacing ? 1.0 : -1.0;

    // ink coverage, basically a hard step
    float inkTex = texture(inkMaskMap, uv1).x;
    float inkMask = clamp((inkTex + inkMaskOffset + inkMaskBias - inkMaskMin) * 1000.0, 0.0, 1.0);

    // inked areas fall back toward the smooth vertex normal
    vec3 N = mix(n, normalize(normal), inkNormalFlatten * inkMask);
    vec3 V = normalize(viewDir);
    vec3 R = reflect(V, N);
    float NdotV = dot(V, N);

    float roughness = mix(skinRoughness, inkRoughness, inkMask);

    // material
    vec4 albedoSample = texture(albedoMap, uv0);
    vec3 albedo = albedoSample.rgb * texture(albedoMap2, uv0).rgb * albedoTint;
    vec3 baseColor = mix(albedo, inkColor, inkMask);
    float alpha = clamp(vtxAlpha * albedoSample.a, 0.0, 1.0);

    vec3 specTint = specColor * mix(texture(specMaskMap, uv0).rgb, vec3(1.0), inkMask);
    vec3 translucency = texture(translucencyMap, uv0).rgb * translucencyColor * (1.0 - inkMask);

    // shadow
    vec2 screenShadow = texture(screenShadowMap, screenUV).xy;
    float shadowVis = texture(shadowMap, shadowUV).x;
    float occ = clamp(vtxOcclusion + vtxOcclusionBias, 0.0, 1.0);
    float shadow = clamp(((1.0 - screenShadow.x) + (1.0 - screenShadow.y)) * occ
                         + (1.0 - shadowVis) * shadowMapWeight, 0.0, 1.0);
    shadow *= shadowStrength;

    // rim / fresnel
    float rimBase = clamp(NdotV + 1.0, 0.0, 1.0);
    float rim = mix(skinRimIntensity * lattePow(rimBase, skinRimPower),
                    inkRimIntensity * lattePow(rimBase, inkRimPower), inkMask);
    rim *= clamp(1.0 - roughness, 0.0, 1.0);
    rim *= 1.0 - shadow * rimShadow;

    // incoming light
    float lightAtten = 1.0 - shadow * lightBufferShadow;
    vec3 lightDiffuse  = texture(lightBuffer, vec3(screenUV, 0.0)).rgb;
    vec3 lightSpecular = texture(lightBuffer, vec3(screenUV, 1.0)).rgb;

    vec3 ambientFront = textureLod(envDiffuseMap, vec4(cubeDir(N), 0.0), shadow).rgb;
    vec3 ambientBack  = textureLod(envDiffuseMap, vec4(cubeDir(-N), 0.0), shadow * backAmbientLodScale).rgb;

    // Cemu passes the ink mask as the array layer here, worth double checking
    vec4 env = textureLod(envSpecMap, vec4(cubeDir(R), inkMask), roughness * 6.0);
    vec3 envColor = env.rgb * (envScale * lattePow(env.a, envExponent));

    // combine
    vec3 diffuse  = baseColor * (lightDiffuse * lightAtten + ambientFront);
    vec3 specular = (envColor + lightSpecular * lightAtten) * (specTint * rim);
    vec3 backLight = translucency * (ambientBack + lattePow(rimBase, backRimPower) * backRimIntensity);
    vec3 glow = emissionColor * lattePow(max(baseColor, 0.0), emissionPower)
              + inkColor * inkEmission * inkMask;

    vec3 color = diffuse + specular + backLight + glow;
    color = mix(color, fog.rgb, fog.w);

    passPixelColor0 = vec4(color, alpha);
}
