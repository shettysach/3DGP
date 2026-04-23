#ifndef SHADERS_H
#define SHADERS_H

namespace renderer {

constexpr char kTerrainVertexShader[] = R"glsl(#version 330 core
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec3 aBaseColor;
layout(location = 3) in vec4 aParams0;
layout(location = 4) in vec4 aParams1;

uniform mat4 uViewProj;
uniform mat4 uLightViewProj;

out vec3 vWorldPos;
out vec3 vNormal;
out vec3 vBaseColor;
out vec4 vParams0;
out vec4 vParams1;
out vec4 vShadowCoord;

void main() {
    vec4 worldPos = vec4(aPosition, 1.0);
    vWorldPos = aPosition;
    vNormal = aNormal;
    vBaseColor = aBaseColor;
    vParams0 = aParams0;
    vParams1 = aParams1;
    vShadowCoord = uLightViewProj * worldPos;
    gl_Position = uViewProj * worldPos;
}
)glsl";

constexpr char kTerrainFragmentShader[] = R"glsl(#version 330 core
in vec3 vWorldPos;
in vec3 vNormal;
in vec3 vBaseColor;
in vec4 vParams0;
in vec4 vParams1;
in vec4 vShadowCoord;

uniform sampler2D uGrassTex;
uniform sampler2D uRockTex;
uniform sampler2D uSandTex;
uniform sampler2D uSnowTex;
uniform sampler2D uShadowMap;
uniform vec3 uCameraPos;
uniform vec3 uSunLightDir;
uniform vec3 uSunColor;
uniform vec3 uSkyAmbientColor;
uniform vec3 uGroundAmbientColor;
uniform vec3 uFogHorizonColor;
uniform vec3 uFogZenithColor;
uniform vec3 uFogSunColor;
uniform float uFogDensity;
uniform float uFogHeightFalloff;
uniform float uFogBaseHeight;
uniform float uShadowTexelSize;
uniform int uEnableMaterials;
uniform int uEnableShadows;

out vec4 fragColor;

float saturate(float x) {
    return clamp(x, 0.0, 1.0);
}

vec3 toneMap(vec3 color) {
    color = max(color, vec3(0.0));
    return clamp(
        (color * (2.51 * color + 0.03)) /
            max(color * (2.43 * color + 0.59) + 0.14, vec3(0.14)),
        0.0,
        1.0);
}

vec3 toDisplay(vec3 color) {
    return pow(toneMap(color), vec3(1.0 / 2.2));
}

vec3 triplanarSample(sampler2D tex, vec3 pos, vec3 normal, float scale) {
    vec3 weights = abs(normal);
    weights = pow(weights, vec3(6.0));
    weights /= max(weights.x + weights.y + weights.z, 0.0001);

    vec3 sampleX = texture(tex, pos.yz * scale).rgb;
    vec3 sampleY = texture(tex, pos.xz * scale).rgb;
    vec3 sampleZ = texture(tex, pos.xy * scale).rgb;
    return sampleX * weights.x + sampleY * weights.y + sampleZ * weights.z;
}

float sampleShadow(vec4 shadowCoord, vec3 normal, vec3 lightDir) {
    if (uEnableShadows == 0) {
        return 1.0;
    }

    vec3 proj = shadowCoord.xyz / max(shadowCoord.w, 0.0001);
    proj = proj * 0.5 + 0.5;
    if (proj.z <= 0.0 || proj.z >= 1.0) {
        return 1.0;
    }
    if (proj.x <= 0.0 || proj.x >= 1.0 || proj.y <= 0.0 || proj.y >= 1.0) {
        return 1.0;
    }

    float ndotl = saturate(dot(normal, lightDir));
    float bias = max(0.0013 * (1.0 - ndotl), 0.00018);
    float lit = 0.0;
    for (int y = -1; y <= 1; ++y) {
        for (int x = -1; x <= 1; ++x) {
            vec2 offset = vec2(float(x), float(y)) * uShadowTexelSize;
            float depth = texture(uShadowMap, proj.xy + offset).r;
            lit += proj.z - bias <= depth ? 1.0 : 0.0;
        }
    }
    return lit / 9.0;
}

vec3 fogColor(vec3 viewDir) {
    float horizonMix = saturate(pow(viewDir.y * 0.5 + 0.5, 0.65));
    float sunScatter = pow(max(dot(-viewDir, uSunLightDir), 0.0), 8.0);
    vec3 fog = mix(uFogHorizonColor, uFogZenithColor, horizonMix);
    fog += uFogSunColor * sunScatter * 0.08;
    return fog;
}

void main() {
    vec3 normal = normalize(vNormal);
    vec3 lightDir = normalize(uSunLightDir);
    vec3 viewDir = normalize(vWorldPos - uCameraPos);
    vec3 viewToCamera = normalize(uCameraPos - vWorldPos);

    float slope = vParams0.x;
    float mountain = vParams0.y;
    float river = vParams0.z;
    float moisture = vParams0.w;
    float heightN = vParams1.x;
    float temperature = vParams1.y;
    float precipitation = vParams1.z;

    vec3 albedo = vBaseColor;
    float rock = 0.0;
    float snow = 0.0;
    float sand = 0.0;
    float grass = 1.0;
    float wetness = 0.0;
    if (uEnableMaterials == 1) {
        rock = smoothstep(0.38, 0.92, slope + mountain * 0.34 + heightN * 0.10);
        snow = smoothstep(0.76, 0.99, heightN + (1.0 - temperature) * 0.28 + slope * 0.04) *
               smoothstep(0.0, 0.55, 1.0 - moisture * 0.86);
        sand = smoothstep(0.08, 0.48, river) *
               smoothstep(0.0, 0.62, 1.0 - heightN) *
               smoothstep(0.0, 0.62, 1.0 - moisture);
        grass = clamp(1.0 - max(rock, snow), 0.0, 1.0) * (1.0 - sand * 0.72);
        wetness = saturate(river * 0.65 + moisture * 0.22 - slope * 0.18);

        float weightSum = max(grass + rock + sand + snow, 0.0001);
        grass /= weightSum;
        rock /= weightSum;
        sand /= weightSum;
        snow /= weightSum;

        vec3 grassDetail = texture(uGrassTex, vWorldPos.xz * 0.052).rgb;
        vec3 sandDetail = texture(uSandTex, vWorldPos.xz * 0.067).rgb;
        vec3 rockDetail = triplanarSample(uRockTex, vWorldPos, normal, 0.085);
        vec3 snowDetail = triplanarSample(uSnowTex, vWorldPos, normal, 0.042);

        vec3 fertileTint = mix(vec3(0.66, 0.58, 0.40), vec3(0.82, 0.93, 0.70), saturate(moisture * 0.62 + precipitation * 0.38));
        vec3 grassMaterial = grassDetail * mix(fertileTint, vBaseColor * 1.10, 0.42);
        vec3 rockMaterial = rockDetail * mix(vec3(0.66, 0.67, 0.69), vBaseColor * 1.08, 0.20);
        vec3 sandMaterial = sandDetail * mix(vec3(0.92, 0.84, 0.62), vBaseColor * 1.03, 0.12);
        vec3 snowMaterial = snowDetail * vec3(0.93, 0.97, 1.02);

        albedo = grassMaterial * grass + rockMaterial * rock + sandMaterial * sand + snowMaterial * snow;

        float macroNoise = texture(uGrassTex, vWorldPos.xz * 0.0045).r;
        vec3 macroTint = mix(vec3(0.92, 0.96, 1.02), vec3(1.08, 1.03, 0.96), macroNoise);
        albedo *= mix(vec3(1.0), macroTint, 0.18);
        albedo = mix(albedo, albedo * vec3(0.76, 0.80, 0.84), wetness * 0.22);
    }

    float diffuse = saturate(dot(normal, lightDir));
    float wrappedDiffuse = saturate((dot(normal, lightDir) + 0.24) / 1.24);
    float shadow = sampleShadow(vShadowCoord, normal, lightDir);
    float horizon = saturate(1.0 - normal.y * normal.y);
    vec3 hemiAmbient = mix(uGroundAmbientColor, uSkyAmbientColor, normal.y * 0.5 + 0.5);
    hemiAmbient *= mix(0.92, 1.10, heightN);
    hemiAmbient += uFogHorizonColor * horizon * 0.035;
    vec3 lit = albedo * (hemiAmbient + uSunColor * wrappedDiffuse * shadow);
    float backScatter = pow(saturate(dot(-lightDir, viewToCamera)), 3.0) * (1.0 - diffuse);
    lit += albedo * uFogSunColor * backScatter * (0.03 + moisture * 0.05 + grass * 0.04 + snow * 0.05);

    if (uEnableMaterials == 1) {
        vec3 halfVector = normalize(viewToCamera + lightDir);
        float spec = pow(saturate(dot(normal, halfVector)), 18.0 + heightN * 38.0 + rock * 12.0 + snow * 10.0);
        float specStrength = mix(
            0.015,
            0.09,
            saturate(wetness * 0.55 + river * 0.25 + (1.0 - temperature) * 0.18 + rock * 0.16 + snow * 0.12));
        lit += uSunColor * spec * specStrength * shadow;
    }

    lit *= 0.98 - slope * 0.035;

    float distanceToCamera = length(uCameraPos - vWorldPos);
    float fogDist = 1.0 - exp(-distanceToCamera * uFogDensity);
    float fogHeight = exp(-max(vWorldPos.y - uFogBaseHeight, 0.0) * uFogHeightFalloff);
    float fogAmount = saturate(fogDist * fogHeight);

    vec3 atmosphere = fogColor(viewDir);
    vec3 finalColor = mix(lit, atmosphere, fogAmount);
    fragColor = vec4(toDisplay(finalColor), 1.0);
}
)glsl";

constexpr char kWaterVertexShader[] = R"glsl(#version 330 core
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec4 aParams;

uniform mat4 uViewProj;
uniform mat4 uLightViewProj;

out vec3 vWorldPos;
out vec4 vParams;
out vec4 vShadowCoord;

void main() {
    vec4 worldPos = vec4(aPosition, 1.0);
    vWorldPos = aPosition;
    vParams = aParams;
    vShadowCoord = uLightViewProj * worldPos;
    gl_Position = uViewProj * worldPos;
}
)glsl";

constexpr char kWaterFragmentShader[] = R"glsl(#version 330 core
in vec3 vWorldPos;
in vec4 vParams;
in vec4 vShadowCoord;

uniform sampler2D uWaterTex;
uniform sampler2D uShadowMap;
uniform vec3 uCameraPos;
uniform vec3 uSunLightDir;
uniform vec3 uSunColor;
uniform vec3 uSkyZenithColor;
uniform vec3 uFogHorizonColor;
uniform vec3 uFogZenithColor;
uniform vec3 uFogSunColor;
uniform float uFogDensity;
uniform float uFogHeightFalloff;
uniform float uFogBaseHeight;
uniform float uShadowTexelSize;
uniform float uTime;

out vec4 fragColor;

float saturate(float x) {
    return clamp(x, 0.0, 1.0);
}

vec3 toneMap(vec3 color) {
    color = max(color, vec3(0.0));
    return clamp(
        (color * (2.51 * color + 0.03)) /
            max(color * (2.43 * color + 0.59) + 0.14, vec3(0.14)),
        0.0,
        1.0);
}

vec3 toDisplay(vec3 color) {
    return pow(toneMap(color), vec3(1.0 / 2.2));
}

float sampleShadow(vec4 shadowCoord, vec3 normal, vec3 lightDir) {
    vec3 proj = shadowCoord.xyz / max(shadowCoord.w, 0.0001);
    proj = proj * 0.5 + 0.5;
    if (proj.z <= 0.0 || proj.z >= 1.0) {
        return 1.0;
    }
    if (proj.x <= 0.0 || proj.x >= 1.0 || proj.y <= 0.0 || proj.y >= 1.0) {
        return 1.0;
    }

    float ndotl = saturate(dot(normal, lightDir));
    float bias = max(0.0015 * (1.0 - ndotl), 0.0002);
    float lit = 0.0;
    for (int y = -1; y <= 1; ++y) {
        for (int x = -1; x <= 1; ++x) {
            vec2 offset = vec2(float(x), float(y)) * uShadowTexelSize;
            float depth = texture(uShadowMap, proj.xy + offset).r;
            lit += proj.z - bias <= depth ? 1.0 : 0.0;
        }
    }
    return lit / 9.0;
}

vec3 fogColor(vec3 viewDir) {
    float horizonMix = saturate(pow(viewDir.y * 0.5 + 0.5, 0.65));
    float sunScatter = pow(max(dot(-viewDir, uSunLightDir), 0.0), 8.0);
    vec3 fog = mix(uFogHorizonColor, uFogZenithColor, horizonMix);
    fog += uFogSunColor * sunScatter * 0.08;
    return fog;
}

vec3 waterNormal(vec2 uv) {
    vec3 n1 = texture(uWaterTex, uv + vec2(uTime * 0.038, uTime * 0.021)).rgb * 2.0 - 1.0;
    vec3 n2 = texture(uWaterTex, uv * 1.7 + vec2(-uTime * 0.029, uTime * 0.034)).rgb * 2.0 - 1.0;
    vec2 slope = vec2(n1.r + n2.g, n1.g + n2.b) - 1.0;
    return normalize(vec3(slope.x * 0.38, 1.0, slope.y * 0.38));
}

void main() {
    float river = vParams.x;
    float slope = vParams.y;
    float heightN = vParams.z;
    float foamFactor = vParams.w;

    vec2 uv = vWorldPos.xz * 0.12;
    vec3 normal = waterNormal(uv);
    vec3 lightDir = normalize(uSunLightDir);
    vec3 toCamera = normalize(uCameraPos - vWorldPos);
    vec3 viewDir = normalize(vWorldPos - uCameraPos);

    float fresnel = mix(0.02, 1.0, pow(1.0 - saturate(dot(normal, toCamera)), 5.0));
    float depthLike = saturate(river * 1.18 + heightN * 0.12);
    float shadow = sampleShadow(vShadowCoord, normal, lightDir);

    vec3 shallowColor = vec3(0.12, 0.38, 0.44);
    vec3 deepColor = vec3(0.03, 0.16, 0.25);
    vec3 transmission = mix(shallowColor, deepColor, depthLike);
    vec3 reflection = mix(uFogHorizonColor, uSkyZenithColor, saturate(normal.y * 0.5 + 0.5));
    reflection += uSunColor * pow(saturate(dot(reflect(-lightDir, normal), toCamera)), 64.0) * 0.12;
    vec3 waterColor = mix(transmission, reflection, saturate(fresnel * 0.82 + 0.10));

    float diffuse = saturate(dot(normal, lightDir));
    vec3 halfVector = normalize(toCamera + lightDir);
    float specular = pow(saturate(dot(normal, halfVector)), 80.0) * (0.18 + depthLike * 0.12);
    float foamNoise = texture(uWaterTex, uv * 1.85 + vec2(uTime * 0.08, -uTime * 0.05)).r;
    float foam = saturate(foamFactor * (0.55 + foamNoise * 0.65));

    vec3 lit = waterColor * (0.18 + diffuse * shadow * 0.42);
    lit += uSunColor * specular * shadow;
    lit += vec3(0.96, 0.98, 1.0) * foam * (0.38 + slope * 0.16);

    float distanceToCamera = length(uCameraPos - vWorldPos);
    float fogDist = 1.0 - exp(-distanceToCamera * (uFogDensity * 1.1));
    float fogHeight = exp(-max(vWorldPos.y - uFogBaseHeight, 0.0) * uFogHeightFalloff);
    float fogAmount = saturate(fogDist * fogHeight);

    vec3 finalColor = mix(lit, fogColor(viewDir), fogAmount);
    float alpha = saturate(0.38 + depthLike * 0.26 + fresnel * 0.18);
    fragColor = vec4(toDisplay(finalColor), alpha);
}
)glsl";

constexpr char kShadowVertexShader[] = R"glsl(#version 330 core
layout(location = 0) in vec3 aPosition;

uniform mat4 uLightViewProj;

void main() {
    gl_Position = uLightViewProj * vec4(aPosition, 1.0);
}
)glsl";

constexpr char kShadowFragmentShader[] = R"glsl(#version 330 core
void main() {}
)glsl";

constexpr char kSkyVertexShader[] = R"glsl(#version 330 core
out vec2 vNdc;

void main() {
    vec2 positions[3] = vec2[](
        vec2(-1.0, -1.0),
        vec2(3.0, -1.0),
        vec2(-1.0, 3.0)
    );
    vec2 pos = positions[gl_VertexID];
    vNdc = pos;
    gl_Position = vec4(pos, 0.0, 1.0);
}
)glsl";

constexpr char kSkyFragmentShader[] = R"glsl(#version 330 core
in vec2 vNdc;

uniform vec3 uCameraForward;
uniform vec3 uCameraRight;
uniform vec3 uCameraUp;
uniform vec3 uSunLightDir;
uniform vec3 uSunColor;
uniform vec3 uSkyZenithColor;
uniform vec3 uSkyHorizonColor;
uniform vec3 uFogHorizonColor;
uniform float uAspect;
uniform float uTanHalfFov;

out vec4 fragColor;

vec3 toneMap(vec3 color) {
    color = max(color, vec3(0.0));
    return clamp(
        (color * (2.51 * color + 0.03)) /
            max(color * (2.43 * color + 0.59) + 0.14, vec3(0.14)),
        0.0,
        1.0);
}

vec3 toDisplay(vec3 color) {
    return pow(toneMap(color), vec3(1.0 / 2.2));
}

void main() {
    vec3 rayDir = normalize(
        uCameraForward +
        vNdc.x * uCameraRight * uAspect * uTanHalfFov +
        vNdc.y * uCameraUp * uTanHalfFov);

    float horizonMix = clamp(pow(rayDir.y * 0.5 + 0.5, 0.60), 0.0, 1.0);
    vec3 sky = mix(uSkyHorizonColor, uSkyZenithColor, horizonMix);

    float horizonGlow = pow(1.0 - abs(rayDir.y), 5.5);
    sky += uFogHorizonColor * horizonGlow * 0.08;

    float sunHalo = pow(max(dot(rayDir, uSunLightDir), 0.0), 18.0);
    float sunDisk = pow(max(dot(rayDir, uSunLightDir), 0.0), 900.0);
    float antiSun = pow(max(dot(rayDir, -uSunLightDir), 0.0), 3.0);
    sky += vec3(0.05, 0.09, 0.16) * antiSun * (0.20 + horizonMix * 0.15);
    sky += uSunColor * (sunHalo * 0.34 + sunDisk * 8.5);

    fragColor = vec4(toDisplay(sky), 1.0);
}
)glsl";

} // namespace renderer

#endif // SHADERS_H
