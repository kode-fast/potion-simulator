#version 430 core
layout (location = 0) in vec3 aPos;


// need the tex_coords as an out var and calculate object space to texture space   
out vec3 tex_coords;



uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    // this is converting from the object space (-0.5 to 0.5) to texture space (0.0 to 1.0)
    tex_coords = aPos + vec3(0.5);
    gl_Position = projection * view * model * vec4(aPos, 1.0);
}