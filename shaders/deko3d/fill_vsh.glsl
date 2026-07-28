#version 460

layout (location = 0) in vec2 vertex;
layout (location = 1) in vec2 tcoord;
layout (location = 2) in vec2 lcoord;
layout (location = 0) out vec2 ftcoord;
layout (location = 1) out vec2 fpos;
layout (location = 2) out vec2 uv;

layout (std140, binding = 0) uniform View
{
    vec2 size;
} view;

void main(void) {
    ftcoord = tcoord;
    uv = 0.5 * lcoord;
    fpos = vertex;
    gl_Position = vec4(2.0*vertex.x/view.size.x - 1.0, 1.0 - 2.0*vertex.y/view.size.y, 0, 1);
};