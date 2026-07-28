#version 450

layout (push_constant) uniform VertexUniform {
    vec2 viewSize;
    int uniformOffset;
};

layout (location = 0) in vec2 vertex;
layout (location = 1) in vec2 tcoord;
layout (location = 2) in vec2 lcoord;
layout (location = 0) out vec2 ftcoord;
layout (location = 1) out vec2 fpos;
layout (location = 2) out int fid;
layout (location = 3) out vec2 uv;
void main(void) {
    ftcoord = tcoord;
    fpos = vertex;
    fid = uniformOffset;
    uv = 0.5 * lcoord;
    gl_Position = vec4(2.0*vertex.x/viewSize.x - 1.0, 2.0*vertex.y/viewSize.y - 1.0, 0, 1);
}
