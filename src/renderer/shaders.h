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

out vec3 vWorldPos;
out vec3 vNormal;
out vec3 vBaseColor;
out vec4 vParams0;
out vec4 vParams1;

void main() {
    vWorldPos = aPosition;
    vNormal = aNormal;
    vBaseColor = aBaseColor;
    vParams0 = aParams0;
    vParams1 = aParams1;
    gl_Position = uViewProj * vec4(aPosition, 1.0);
}
)glsl";

constexpr char kTerrainFragmentShader[] = R"glsl(#version 330 core
in vec3 vWorldPos;
in vec3 vNormal;
in vec3 vBaseColor;
in vec4 vParams0;
in vec4 vParams1;

uniform sampler2D uGrassTex;
uniform sampler2D uRockTex;
uniform sampler2D uSandTex;
uniform sampler2D uSnowTex;
uniform vec3 uSunLightDir;
uniform vec3 uSunColor;
uniform vec3 uAmbientColor;

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

void main() {
    vec3 normal = normalize(vNormal);
    vec3 lightDir = normalize(uSunLightDir);

    float slope = vParams0.x;
    float mountain = vParams0.y;
    float river = vParams0.z;
    float moisture = vParams0.w;
    float heightN = vParams1.x;
    float temperature = vParams1.y;
    float precipitation = vParams1.z;

    float rockSignal = slope * 0.42 + mountain * 0.92 + max(heightN - 0.58, 0.0) * 0.16;
    float rock = smoothstep(0.48, 0.88, rockSignal);
    float snow = smoothstep(0.76, 0.99, heightN + (1.0 - temperature) * 0.28 + slope * 0.04) *
                 smoothstep(0.0, 0.55, 1.0 - moisture * 0.86);
    float sand = smoothstep(0.08, 0.48, river) *
                 smoothstep(0.0, 0.62, 1.0 - heightN) *
                 smoothstep(0.0, 0.62, 1.0 - moisture);
    float grass = clamp(1.0 - max(rock, snow), 0.0, 1.0) * (1.0 - sand * 0.72);

    float weightSum = max(grass + rock + sand + snow, 0.0001);
    grass /= weightSum;
    rock /= weightSum;
    sand /= weightSum;
    snow /= weightSum;

    vec3 grassDetail = texture(uGrassTex, vWorldPos.xz * 0.052).rgb;
    vec3 sandDetail = texture(uSandTex, vWorldPos.xz * 0.067).rgb;
    vec3 rockDetail = triplanarSample(uRockTex, vWorldPos, normal, 0.085);
    vec3 snowDetail = triplanarSample(uSnowTex, vWorldPos, normal, 0.042);

    vec3 fertileTint = mix(vec3(0.78, 0.68, 0.46), vec3(0.86, 0.95, 0.76), saturate(moisture * 0.62 + precipitation * 0.38));
    vec3 grassMaterial = grassDetail * mix(vBaseColor * 1.16, fertileTint, 0.22);
    vec3 rockMaterial = rockDetail * mix(vec3(0.66, 0.67, 0.69), vBaseColor * 1.08, 0.20);
    vec3 sandMaterial = sandDetail * mix(vec3(0.92, 0.84, 0.62), vBaseColor * 1.03, 0.12);
    vec3 snowMaterial = snowDetail * vec3(0.93, 0.97, 1.02);

    vec3 albedo = grassMaterial * grass + rockMaterial * rock + sandMaterial * sand + snowMaterial * snow;

    float diffuse = saturate(dot(normal, lightDir));
    float wrappedDiffuse = saturate((dot(normal, lightDir) + 0.24) / 1.24);
    vec3 lit = albedo * (uAmbientColor + uSunColor * wrappedDiffuse);

    fragColor = vec4(toDisplay(lit), 1.0);
}
)glsl";

} // namespace renderer

#endif // SHADERS_H
