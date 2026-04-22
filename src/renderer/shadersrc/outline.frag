#version 460

layout (location = 0) out vec4 out_color;

void main()
{
    // TODO: Make outline_colour a field of ObjectData so it can be changed dynamically.
    // E.g. When an object on the ground is hovered over, would be cool to set it's material to OUTLINE and add a white outline.
    out_color = vec4(0.0, 0.0, 0.0, 1.0);
}
