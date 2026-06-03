#version 430 core 

out vec4 FragColor;

// tex_coords come from the vert shader
in vec3 tex_coords; // the coordenits of the pixel on the texture were rendering (from 0.0 to 1.0)

// this is where the shader accesses the textures we passed into the gpu
uniform sampler2DArray fluid_textures;
// 32 + 2 for outer sim part 
uniform int grid_depth = 32 + 2; // N+2 // dont render the buffer cells ?
uniform int num_fluids = 1;

uniform vec3 camera_pos_local;
uniform int max_steps = 256; // 128
uniform float step_size = 0.01; // 0.01

// TODO: need to update this when doing multi colors 
uniform vec3 fluid_colors;


void main(){
    vec3 current_pos = tex_coords;
    // if camera is in the object stop it from mising the break statment 
    if (camera_pos_local.x >= 0.0 && camera_pos_local.x <= 1.0 &&
        camera_pos_local.y >= 0.0 && camera_pos_local.y <= 1.0 &&
        camera_pos_local.z >= 0.0 && camera_pos_local.z <= 1.0) 
    {
        current_pos = camera_pos_local;
    }

    // ray direction is from the local camera position (relitive to the local space of the object the texter is on) to the texture coordanite we are calculating 
    vec3 ray_dir = normalize(tex_coords - camera_pos_local);

    float accumulated_density = 0.0;
    vec3 mixed_color = vec3(0.0);
    float spec = 0.0;
    float fresnel = 0.0;
    // ray marching loop
    for(int step = 0; step < max_steps; step++) {
        // stop if ray leaves the cube bounds 
        if( current_pos.x < 0.0 || current_pos.x > 1.0 ||
            current_pos.y < 0.0 || current_pos.y > 1.0 ||
            current_pos.z < 0.0 || current_pos.z > 1.0) {
            break;
        }        
        
        
        // this is where the ray is currently in the z value of the texture stack. ie this is the index of the texture we want if there was 1 texture in the stack
        float local_z_slice = current_pos.z * float(grid_depth);
        // for each fluid 
        for(int i = 0; i < num_fluids; i++) {
           
            // this finds the z texture in the gloabal texture stack for the z value of the ray and the texture we are looking for 
            float global_slice_index = (float(i) * float(grid_depth)) + local_z_slice;
            
            // uv_slice gets the x and y texture index from our texture stack, and puts the z index on at the end 
            vec3 uvz_slice = vec3(current_pos.xy, global_slice_index);
            // the final value from the texture array at the uv, only gets red chanell as thats where were storing the values 
            float density = texture(fluid_textures, uvz_slice).r;
            
            // control what density gets rendered
            if (density > 0.01) {
                // debug:
                //FragColor = vec4(1.0, 0.0, 1.0, 1.0); 
                //return;
                // ----------  
                
                // if its the fist colour on the line calculate the specular reflection from the vectors in that cell
                if (accumulated_density < 0.01) {
                    
                    // instead of using the vel sample the nabouring cells densities and get the normal from where the low / high densities are 
                
                    // cell distance in texture space
                    float tex_cell_dist = 1.0 / float(grid_depth); 
                    // sample the 
                    // right and left cells 
                    float x_right = texture(fluid_textures, vec3(current_pos.x + tex_cell_dist, current_pos.y, global_slice_index)).r;
                    float x_left = texture(fluid_textures, vec3(current_pos.x - tex_cell_dist, current_pos.y, global_slice_index)).r;
                    // up and down cells 
                    float y_up = texture(fluid_textures, vec3(current_pos.x, current_pos.y + tex_cell_dist, global_slice_index)).r;
                    float y_down = texture(fluid_textures, vec3(current_pos.x, current_pos.y - tex_cell_dist, global_slice_index)).r;
                    // front and back cells // these need to be done diffrently // the problem was it was looking at the wrong index 
                    float z_front = texture(fluid_textures, vec3(current_pos.xy, global_slice_index + 1.0)).r;
                    float z_back  = texture(fluid_textures, vec3(current_pos.xy, global_slice_index - 1.0)).r;

                    // the direction on the density (ie if x_right is denser the x_left then x value will be positive ect)
                    vec3 density_gradient = vec3(x_right - x_left, y_up - y_down, z_front - z_back);

                    
                    if (length(density_gradient) > 0.0001) {

                        vec3 norm = normalize(-density_gradient);

                        //vec3 light_vec = normalize(current_pos - vec3(0.0, 1.0, 0.0));
                        vec3 light_vec = - ray_dir;
                        vec3 half_vec = normalize(light_vec - ray_dir);

                        fresnel = pow(max(0.0, 1.0 - dot(norm, - ray_dir)),4.0);
                        spec = max(dot(half_vec,norm), 0.0);
                        spec = pow(spec, 64.0); 
                    }

                }

                float sample_absorb = density * step_size * 500.0; // * 500 looks good
                // add fluid color scaled by its density 
                mixed_color += fluid_colors[i] * sample_absorb * (1.0 - accumulated_density); // vec3(0.0f, 0.4f, 0.8f)
                accumulated_density += sample_absorb;
            } 


        }
        // if density is 1 then we cant see through it anymore and can exit early
        if(accumulated_density >= 0.9999) {
            accumulated_density = 1.0;
            break;
        }

        // advance ray
        current_pos += ray_dir * step_size;
    }
    // these need to be outside the for loop duh
    // if no density can discard this shader pixel
    if(accumulated_density <= 0.01){
        discard;
    }

    // scale white highlights from spec 
    vec3 spec_colour = vec3(1.0,1.0,1.0) * spec * 0.4 ;
    // mix function mixes based on the frensel value 
    vec3 sky_colour = vec3(0.4, 0.5, 0.7);
    vec3 final_colour = mix(mixed_color, sky_colour, fresnel * 0.5) + spec_colour;

    // frag color is the mixed color RGB and density as A
    FragColor = vec4(final_colour, accumulated_density);


}