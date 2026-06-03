#pragma once
#include <iostream>
#include <cmath>
#include <algorithm>
#include <cstring>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
/*based on stable fluids and real-time fluid dynamics for games by Jos Stam*/


// N is the inner sim size of one edge
const int N = 32;

// add 2 cells on the edge per the paper for padding to smooth out the sim
// using a 1d array for storage so need to multiply our dimensions to get the size 
const int GRID_SIZE = (N+2) * (N+2) * (N+2);

// number of fluids that can be in the simulation 
const int NUM_FLUIDS = 1;

float gravity_constant = 800.0f; // without dt *,  10 ~ 20is good

// macro that converts coordents to the index in the 1d array 
// loops need to go k j ifor cache optimization. our 1d array is in the order all i data then all j data then all k data 
// this lets the cpu read the memory linearly instad of having to jump ahead.
//(when its i j k then k changes every loop. its data is also the most spread out so it breaks the caching)
// TODO: try changing the loops back now?
#define IX(i,j,k) ((i)+(j) * (N +2)+(k) * (N + 2) * (N + 2))

// macro that swaps the pointers for two arrays 
#define SWAP(x0,x) {float *tmp=x0;x0=x;x=tmp;}



// fluid properties struct 
struct FluidProperties {
    float viscosity;
    float buoyancy;
    glm::vec3 color;
};



// define fluid sim as a class so we can spawn instances 
class FluidSim{
public:

    float* u; // x
    float* v; // y
    float* w; // z

    float* u_old;
    float* v_old;
    float* w_old;

    // need multipule density feilds for multipule fluids 
    // these are pointers to the pointers to the fluidproperties structs that have the info
    float** densities;
    float** densities_old;

    FluidProperties fluidTypes[NUM_FLUIDS];
    
    // constructor method
    FluidSim() {
        // create velocity feilds 
        u = new float[GRID_SIZE](); 
        v = new float[GRID_SIZE](); 
        w = new float[GRID_SIZE](); 

        u_old = new float[GRID_SIZE]();
        v_old = new float[GRID_SIZE]();
        w_old = new float[GRID_SIZE]();
        
        // create density feilds for each liquid 
        densities = new float*[NUM_FLUIDS];
        densities_old = new float*[NUM_FLUIDS];

        // create 
        for (int f = 0; f < NUM_FLUIDS; f++) {
            densities[f] = new float[GRID_SIZE]();
            densities_old[f] = new float[GRID_SIZE]();
        }
        
        // define liqid properties 
        fluidTypes[0] = {0.001f, -0.1f, glm::vec3(0.0f, 0.5f, 1.0f)}; // water
        fluidTypes[1] = {0.050f, 0.2f, glm::vec3(0.8f, 0.7f, 0.2f)}; // oil

    }

    ~FluidSim(){
        delete[] u; 
        delete[] v; 
        delete[] w; 

        delete[] u_old;
        delete[] v_old;
        delete[] w_old;

        for (int f = 0; f < NUM_FLUIDS; f++) {
            delete[] densities[f];
            delete[] densities_old[f];
        }
        delete[] densities;
        delete[] densities_old;
    }
    
    // the source for each frame is given by the array s
    void add_source ( int N, float * x, float * s, float dt ) {
        // if there is a source at the cell x[i] then is adds a source to it scaled by delta time
        // size needs to be n+3 * 3 (dimension)
        int i, size=(N+2)*(N+2)*(N+2);
        for ( i=0 ; i<size ; i++ ) x[i] += dt*s[i];
    }

    // 
    void diffuse ( int N, int b, float* x, float* x0, float diff, float dt ) {
        int g, i, j, k;
        // a is how fast the fluid diffuses 
        float a = dt * diff * N * N;
        // pre calc the devisor 
        float devisor = 1.0f / (1.0f + 6.0f * a);
        // this loop is replacing solving the deffiusion equation with linear algibra. it can be derived from the equation for a single cell opp in the diagel  matrix opp. 
        //  the matrix would be insally big (as it would be diag square), wich is why this is better
        // so instead of inverting the matrix to solve the system were doing it with a loop
        // g is the relaxation incrementaion for Gauss-Seidel relaxation

        // precalculate pointer offsets to do the calculations inplace 
        int row_stride = N + 2;
        int slice_stride = (N + 2) * (N + 2);
        // TODO could add dynamic stopping based on the error tolerance between x and x0 
        // 
        for ( g=0; g<12; g++){
            for ( k=1; k<=N; k++ ){
                for ( j=1; j<=N; j++ ){
                    for ( i=1; i<=N; i++){
                            // each coords new density value is calculated from the weigted average of the surrounding cells scaled by a
                            // x0 is the starting value from the previouse frame 
                            // a*(...) / (1 +6*a) were scaling the average by a so we must divide it out when we average it to avoid it infinitly growing
                            // the 1 is counting the cell itself in the avg 
                            int current = IX(i, j, k); // the current pointer of the loop
                            /*
                            x[IX(i,j,k)] = (x0[IX(i,j,k)] + a*(
                                            x[IX(i-1,j,k)] + x[IX(i+1,j,k)] +  
                                            x[IX(i,j-1,k)] + x[IX(i,j+1,k)] +  
                                            x[IX(i,j,k-1)] + x[IX(i,j,k+1)]    
                                        ))  *devisor;
                            */
                            x[current] = (x0[current] + a * (
                                    x[current - 1] + x[current + 1] +               // Left / Right (-i / +i)
                                    x[current - row_stride] + x[current + row_stride] + // Bottom / Top (-j / +j)
                                    x[current - slice_stride] + x[current + slice_stride] // Front / Back (-k / +k)
                                 )) * devisor;
                    }
                }
            }
        }

        set_bnd ( N, b, x );

    }

    // u v w are the particals coordenites 
    void advect( int N, int b, float* d, float* d0, float* u, float * v, float * w, float dt){
        int i, j, k, i0, j0, k0, i1, j1, k1;
        float x, y, z; 
        float s0, t0, r0, s1, t1, r1, dt0; // interpolation weights
        
        // this is the scaling factor to turn the particals position into how meny grid cell units it has traveld
        dt0 = dt*N;

        int row_stride = N + 2; // moves up 1 row in the current z slice 
        int slice_stride = (row_stride) * (row_stride); // moves up 1 z slice 


        float fN = (float)N;
        float min_bound = 0.5f;
        float max_bound = fN - 0.5f;
        // k is Z plain J is Y plain
        for (k = 1; k <= N; k++) {
            for (j = 1; j <= N; j++) {
                for (i = 1; i <= N; i++) {
                    // pre calc the starting pointer 
                    int current_idx = IX(i, j, k);

                    // find how meny cells the partical would have traveld to get to the current cell
                    x = (float)i - dt0 * u[current_idx];
                    y = (float)j - dt0 * v[current_idx];
                    z = (float)k - dt0 * w[current_idx];

                    // clamp the look back cells so that a partical cant come from outside the boundery walls
                    // changed minimum bound from 0.5 to 1.5 so that data dosnt get pulld from the boundery wall. as the boundery wall has dens = 0 doing the avg from it bleeds out the fluid
                    if (x < 1.5f) x = 1.5f; if (x > (float)N - 0.5f) x = (float)N - 0.5f; i0 = (int)x; i1 = i0 + 1;
                    if (y < 1.5f) y = 1.5f; if (y > (float)N - 0.5f) y = (float)N - 0.5f; j0 = (int)y; j1 = j0 + 1;
                    if (z < 1.5f) z = 1.5f; if (z > (float)N - 0.5f) z = (float)N - 0.5f; k0 = (int)z; k1 = k0 + 1;
                    // trilinear (3d linear) interpolation weights 
                    s1 = x - (float)i0; s0 = 1.0f - s1; 
                    t1 = y - (float)j0; t0 = 1.0f - t1; 
                    r1 = z - (float)k0; r0 = 1.0f - r1;
                    
                    // pre calculte the the bottom left of the 8x8 cube of cells surounding our cell for  this loop 
                    int bottom_left = i0 + (j0 * row_stride) + (k0 * slice_stride);

                    // z plain is extnding the xy 2d plain out forward 
                    // theses are the first 4 sorounding cells on z=0 plain
                    int c000 = bottom_left;
                    int c010 = bottom_left + row_stride;
                    int c100 = bottom_left + 1;
                    int c110 = bottom_left + 1 + row_stride;

                    // next 4 cells on z=1 plain
                    int c001 = bottom_left + slice_stride;
                    int c011 = bottom_left + row_stride + slice_stride;
                    int c101 = bottom_left + 1 + slice_stride;
                    int c111 = bottom_left + 1 + row_stride + slice_stride;

                    // do the linear interpolation blend inplace in the target cell using the pre calculated pointers
                    d[current_idx] = r0 * (
                                        s0 * (t0 * d0[c000] + t1 * d0[c010]) +
                                        s1 * (t0 * d0[c100] + t1 * d0[c110])
                                    ) +
                                    r1 * (
                                        s0 * (t0 * d0[c001] + t1 * d0[c011]) +
                                        s1 * (t0 * d0[c101] + t1 * d0[c111])
                                    );

                }
            }
        }
        set_bnd ( N, b, d );

    }

    void dens_step ( int N, float * x, float * x0, float * u, float * v, float * w, float diff, float dt ) {
        add_source ( N, x, x0, dt );
        SWAP ( x0, x ); diffuse ( N, 0, x, x0, diff, dt );
        SWAP ( x0, x ); advect ( N, 0, x, x0, u, v, w, dt );
    }

   void project ( int N, float* u, float* v, float* w, float* p, float* div) {

        int g,i,j,k;
        // calculate hight map
        float h = 1.0/N;
        float div_scale = -1.0/3.0f * h;
        for ( i=1; i<=N; i++){
            for ( j=1; j<=N; j++) {
                for ( k=1; k<=N; k++){
 
                    div[IX(i,j,k)] = div_scale * (
                                    (u[IX(i+1,j,k)] - u[IX(i-1,j,k)]) +  
                                    (v[IX(i,j+1,k)] - v[IX(i,j-1,k)]) +  
                                    (w[IX(i,j,k+1)] - w[IX(i,j,k-1)])    
                                );
                    p[IX(i,j,k)] = 0;
                }
            }
        }
        set_bnd(N,0,div); 
        set_bnd(N,0,p);
        // reusing our relaxation solver from diffuise step
        float scale_6 = 1.0f / 6.0f;
        int row_stride = N + 2;
        int slice_stride = (N + 2) * (N + 2);
        // TODO: play with relaxation loop amounts sor 20 is stable
        for ( g=0 ; g<12 ; g++ ) {
            for ( i=1 ; i<=N ; i++ ) {
                for ( j=1 ; j<=N ; j++ ) {
                    for ( k=1; k<=N; k++){
                        int current = IX(i, j, k);

                        p[current] = (div[current] + 
                                        p[current - 1] + p[current + 1] +
                                        p[current - row_stride] + p[current + row_stride] +
                                        p[current - slice_stride] + p[current + slice_stride]
                                        ) * scale_6;
                    }
                }
            }
            set_bnd ( N, 0, p );
        }

        // update our velocity feild as the sum of the incompresable feild and the hight map
        for(k=1; k<=N; k++){
            for (j=1; j<=N;j++){
                for (i=1; i<=N; i++){
                    u[IX(i,j,k)] -= 0.5*(p[IX(i+1,j,k)] - p[IX(i-1,j,k)] ) / h;
                    v[IX(i,j,k)] -= 0.5*(p[IX(i,j+1,k)] - p[IX(i,j-1,k)] ) / h;
                    w[IX(i,j,k)] -= 0.5*(p[IX(i,j,k+1)] - p[IX(i,j,k-1)] ) / h;
                }
            }
        }
        set_bnd ( N, 1, u ); set_bnd ( N, 2, v ); set_bnd ( N, 3, w );

   }

    void vel_step ( int N, float * u, float * v, float * w, float * u0, float * v0, float* w0, float visc, float dt )
    {
        add_source ( N, u, u0, dt ); add_source ( N, v, v0, dt ); add_source ( N, w, w0, dt );
        SWAP ( u0, u ); diffuse ( N, 1, u, u0, visc, dt );
        SWAP ( v0, v ); diffuse ( N, 2, v, v0, visc, dt );
        SWAP ( w0, w ); diffuse ( N, 3, w, w0, visc, dt );

        // u0 and v0 are being reused in this instance as p and div
        project ( N, u, v, w, u0, v0 ); 
        SWAP ( u0, u ); SWAP ( v0, v ); SWAP ( w0, w );
        advect ( N, 1, u, u0, u0, v0, w0, dt ); advect ( N, 2, v, v0, u0, v0, w0, dt ); advect ( N, 3, w, w0, u0, v0, w0, dt );
        project ( N, u, v, w, u0, v0 );
    }


    void set_bnd ( int N, int b, float * x ) {
        int i, j;

        // 6 outer faces of the cube
        for ( j=1 ; j<=N ; j++ ) {
            for ( i=1 ; i<=N ; i++ ) {
                // x bnd
                x[IX(0,   i, j)] = b==1 ? -x[IX(1, i, j)] : x[IX(1, i, j)];
                x[IX(N+1, i, j)] = b==1 ? -x[IX(N, i, j)] : x[IX(N, i, j)];
                
                // y bnd
                x[IX(i, 0,   j)] = b==2 ? -x[IX(i, 1, j)] : x[IX(i, 1, j)];
                x[IX(i, N+1, j)] = b==2 ? -x[IX(i, N, j)] : x[IX(i, N, j)];
                
                // z bnd
                x[IX(i, j, 0)]   = b==3 ? -x[IX(i, j, 1)] : x[IX(i, j, 1)];
                x[IX(i, j, N+1)] = b==3 ? -x[IX(i, j, N)] : x[IX(i, j, N)];
            }
        }

        // edges 
        for ( i=1 ; i<=N ; i++ ) {
            // x edges
            x[IX(i, 0,   0  )] = 0.5f * (x[IX(i, 1, 0  )] + x[IX(i, 0,   1)]);
            x[IX(i, N+1, 0  )] = 0.5f * (x[IX(i, N, 0  )] + x[IX(i, N+1, 1)]);
            x[IX(i, 0,   N+1)] = 0.5f * (x[IX(i, 1, N+1)] + x[IX(i, 0,   N)]);
            x[IX(i, N+1, N+1)] = 0.5f * (x[IX(i, N, N+1)] + x[IX(i, N+1, N)]);

            // y edges
            x[IX(0,   i, 0  )] = 0.5f * (x[IX(1, i, 0  )] + x[IX(0,   i, 1)]);
            x[IX(N+1, i, 0  )] = 0.5f * (x[IX(N, i, 0  )] + x[IX(N+1, i, 1)]);
            x[IX(0,   i, N+1)] = 0.5f * (x[IX(1, i, N+1)] + x[IX(0,   i, N)]);
            x[IX(N+1, i, N+1)] = 0.5f * (x[IX(N, i, N+1)] + x[IX(N+1, i, N)]);

            // z edges
            x[IX(0,   0,   i)] = 0.5f * (x[IX(1, 0,   i)] + x[IX(0,   1, i)]);
            x[IX(N+1, 0,   i)] = 0.5f * (x[IX(N, 0,   i)] + x[IX(N+1, 1, i)]);
            x[IX(0,   N+1, i)] = 0.5f * (x[IX(1, N+1, i)] + x[IX(0,   N, i)]);
            x[IX(N+1, N+1, i)] = 0.5f * (x[IX(N, N+1, i)] + x[IX(N+1, N, i)]);
        }

        // corners 
        x[IX(0,   0,   0  )] = 1.0f/3.0f * (x[IX(1, 0, 0)]   + x[IX(0, 1, 0)]   + x[IX(0, 0, 1)]);
        x[IX(0,   N+1, 0  )] = 1.0f/3.0f * (x[IX(1, N+1, 0)] + x[IX(0, N, 0)]   + x[IX(0, N+1, 1)]);
        x[IX(0,   0,   N+1)] = 1.0f/3.0f * (x[IX(1, 0, N+1)] + x[IX(0, 1, N+1)] + x[IX(0, 0, N)]);
        x[IX(0,   N+1, N+1)] = 1.0f/3.0f * (x[IX(1, N+1, N+1)]+x[IX(0, N, N+1)] + x[IX(0, N+1, N)]);
        
        x[IX(N+1, 0,   0  )] = 1.0f/3.0f * (x[IX(N, 0, 0)]   + x[IX(N+1, 1, 0)] + x[IX(N+1, 0, 1)]);
        x[IX(N+1, N+1, 0  )] = 1.0f/3.0f * (x[IX(N, N+1, 0)] + x[IX(N+1, N, 0)] + x[IX(N+1, N+1, 1)]);
        x[IX(N+1, 0,   N+1)] = 1.0f/3.0f * (x[IX(N, 0, N+1)] + x[IX(N+1, 1, N+1)]+x[IX(N+1, 0, N)]);
        x[IX(N+1, N+1, N+1)] = 1.0f/3.0f * (x[IX(N, N+1, N+1)]+x[IX(N+1, N, N+1)]+x[IX(N+1, N+1, N)]);
    } 
    
    void step(float dt, glm::vec3 mouse_interaction, bool is_interacting) {
        // disapearing problem is that the fluid sim is for gas, and in the avging cells were going to 0 and 'evaperating'
        // the bounding cells were eating the liquid as there values are always 0 and they were avreging with the fluid making it go to 0
        // TODO: disapearing problem still exists 
            // mabey the relxation loops were just too small?
            // try to enforce the density? 

        // add mouse added velocities too : 
        // nozzle effect interaction:
        if (is_interacting) {
            
            // add velocity to just 1 index to give directional nozzel effect 
            int targetI = N / 2;
            int targetJ = 5; 
            int targetK = N / 2;
            int idx = IX(targetI, targetJ, targetK); // get idx of the selected vector 

            u[idx] += mouse_interaction.x * 1.0f;
            v[idx] += mouse_interaction.y * 1.0f;
            w[idx] += mouse_interaction.z * 1.0f;

            // add fluid:
            densities[0][idx] += 1.0f;
        }
        
        // i guess this is just the same as setting gravity to 4700
        float gravity = 4000.0f;   
        // bouency basicly just changes the gravety of indavidual cells 
        float buoyancy = 80.0f;  // bouency - gravity 
        float float_sink_coef = buoyancy + gravity;

        // add grav to every cell
        for (int cell_idx = 0; cell_idx < GRID_SIZE; cell_idx++) {

            // use IX to calculate coordentes to index 
            float cell_density = densities[0][cell_idx];
            
            if (cell_density > 0.01f) {
                v_old[cell_idx] -= float_sink_coef * cell_density * dt;
            }


        }

        // higher is thicker
        //float safe_viscosity = 0.00001f; 
        // higher is gas
        //float safe_diffusion = 0.0000001f;

        // higher is thicker
        float safe_viscosity = 0.0001f; 
        // higher is gas
        // TODO: any diffusion number seams to make the fluid disapear eventualy
        float safe_diffusion = 0.0f;

        // vel_step processes your newly added gravity force along with fluid dynamics
        vel_step(N, u, v, w, u_old, v_old, w_old, safe_viscosity, dt);
        
        // dens_step moves the fluid matter along that resulting downward flow
        dens_step(N, densities[0], densities_old[0], u, v, w, safe_diffusion, dt);

        // reset the source arrays . were using swapping so this is IMPORTANT
        std::fill(u_old, u_old + GRID_SIZE, 0.0f);
        std::fill(v_old, v_old + GRID_SIZE, 0.0f);
        std::fill(w_old, w_old + GRID_SIZE, 0.0f);
        std::fill(densities_old[0], densities_old[0] + GRID_SIZE, 0.0f);


    }
    
    // testing the renderer 
    void fill() {


    std::fill(u, u + GRID_SIZE, 0.0f);
    std::fill(v, v + GRID_SIZE, 0.0f);
    std::fill(w, w + GRID_SIZE, 0.0f);
    std::fill(u_old, u_old + GRID_SIZE, 0.0f);
    std::fill(v_old, v_old + GRID_SIZE, 0.0f);
    std::fill(w_old, w_old + GRID_SIZE, 0.0f);

    for (int f = 0; f < NUM_FLUIDS; f++) {
        std::fill(densities[f], densities[f] + GRID_SIZE, 0.0f);
        std::fill(densities_old[f], densities_old[f] + GRID_SIZE, 0.0f);
    }
    //sphere
    float cx = 16.5f; 
    float cy = 9.0f; 
    float cz = 16.5f;
    float radius = 8.0f;

    for (int i = 1; i <= N; i++) {
        for (int j = 1; j <= N; j++) {
            for (int k = 1; k <= N; k++) {
                
                float rx = (float)i - cx;
                float ry = (float)j - cy;
                float rz = (float)k - cz;
                float distance = std::sqrt(rx*rx + ry*ry + rz*rz);

                if (distance <= radius) {
                    densities[0][IX(i, j, k)] = 0.5f;
                }
            }
        }
    }
    // set the bounds here 
    set_bnd(N, 1, u);
    set_bnd(N, 2, v);
    set_bnd(N, 3, w);
    set_bnd(N, 0, densities[0]);
}
    

    // add fluid at the selected cell
    void add_fluid_at_cell(int i, int j, int k, float amount, int fluidId) {

    }

    float** get_densities(){
        return densities;
    }

    int get_width(){
        return N+2;
    }
    int get_depth(){
        return N+2;
    }    
    int get_height(){
        return N+2;
    }

    int get_num_fluid(){
        return NUM_FLUIDS;
    }
};