#version 460

layout(location = 0) out vec2 in_uv;

void main()
{
    // NOTE: This fullscreen triangle with any vertex buffers
    //       is described in Sascha Willems' article here https://www.saschawillems.de/blog/2016/08/13/vulkan-tutorial-on-rendering-a-fullscreen-quad-without-buffers/
    in_uv = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    gl_Position = vec4(in_uv * 2.0f + -1.0f, 0.0f, 1.0f);
}
