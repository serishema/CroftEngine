#include "portal_pipeline_interface.glsl"
#include "camera_interface.glsl"
#include "time_uniform.glsl"

#include "noise.glsl"
#include "constants.glsl"

layout(location=0) out vec3 out_perturb;
layout(location=1) out float out_position;

#define ROT_2D(a) \
    mat2(cos(a), -sin(a), sin(a), cos(a))

float fbm(in vec2 st) {
    return 0.25 * noise(st * 0.0064) + 0.5;
}

const float TimeMult = 0.0002;
const float TexScale = 2048;
const mat2 rot1 = ROT_2D(.9*PI);
const mat2 rot2 = ROT_2D(.06*PI);

float bumpTex(in vec2 uv, in float time) {
    vec2 coords1 = rot1 * uv + time*vec2(0.1, -0.3)*TimeMult;
    vec2 coords2 = rot2 * uv - time*vec2(0.1, 0.2)*TimeMult;

    float wave1 = fbm(coords1*vec2(30.0, 20.0)) * 0.5;
    float wave2 = fbm(coords2*vec2(30.0, 20.0)) * 0.5;
    float x = wave1 + wave2;
    return x*x;
}


float bumpFunc(in vec2 st, in float time){
    return bumpTex(st + vec2(bumpTex(st, time)*0.11, 0), time);
}

vec2 bumpMap(in vec2 st, in float time){
    const float eps = 2./TexScale;
    vec2 ff = vec2(
    bumpFunc(st-vec2(eps, 0), time),
    bumpFunc(st-vec2(0, eps), time)
    );

    return (ff-vec2(bumpFunc(st, time)))/eps*0.13;
}

void main()
{
    vec2 uv = ppi.vertexPosWorld.xz / TexScale;
    // ripples
    vec2 bm1 = bumpMap(uv, u_time);
    vec3 n1 = normalize(vec3(bm1.x, 1, bm1.y));
    // large (slower) waves
    vec2 bm2 = bumpMap(uv*0.1, u_time*0.2);
    vec3 n2 = normalize(vec3(bm2.x, 1, bm2.y));
    // normal in XZ plane (model space)
    vec3 sn = n1 + n2;
    // specular normal with less ripples
    vec3 specN = normalize(n1*0.3 + n2);

    // blinn specular
    vec4 viewVertexPos = camera.view * vec4(ppi.vertexPosWorld, 1);
    vec3 V = -normalize(viewVertexPos.xyz);
    const vec3 L = vec3(0, 1, 0);
    vec3 H = normalize(V+L);
    float cosTheta = clamp(dot(specN, H), 0.0, 1.0);
    float specular = pow(cosTheta, 40) * 0.7;

    vec4 orig = camera.projection * viewVertexPos;
    orig /= orig.w;
    vec4 surface = camera.viewProjection * vec4(vec3(sn.x, 0, sn.z) + ppi.vertexPosWorld, 1);

    surface /= surface.w;

    out_perturb = vec3((surface-orig).xy, specular);
    out_position = ppi.vertexPosView.z;
}
