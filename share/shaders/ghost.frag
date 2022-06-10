#include "geometry_pipeline_interface.glsl"

layout(location=0) out vec4 out_color;
layout(location=1) out vec3 out_normal;
layout(location=2) out vec3 out_position;

layout(location=0) uniform vec3 u_color;

void main()
{
    out_normal = gpi.hbaoNormal;
    out_position = gpi.vertexPos;
    float y = abs(mod(out_position.y, 16.0)-8.0) / 8.0;
    out_color = vec4(u_color, 1) * 0.8 * y;
}
