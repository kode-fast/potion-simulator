#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem> // temp for checking file path

#include "fluidsim.h"
#include "shader.h"

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow *window);

// settings
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;


int main()
{
    // glfw: initialize and configure
    // ------------------------------
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // glfw window creation
    // --------------------
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Alch", NULL, NULL);
    if (window == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    // glad: load all OpenGL function pointers
    // ---------------------------------------
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

//--------------------------------------------------------------------------------
  // create our shader object 
  Shader triangle_shader("shaders/camera.vert","shaders/orange.frag");

//--------------------------------------------------------------------------------

  // create wire frame 

   // -------- SHADER CUBE 

    // 8 vertices representing the corners of the bounding box
    float cubeVertices[] = {
        -0.5f, -0.5f, -0.5f, // Index 0: Front-Bottom-Left
        0.5f, -0.5f, -0.5f, // Index 1: Front-Bottom-Right
        0.5f,  0.5f, -0.5f, // Index 2: Front-Top-Right
        -0.5f,  0.5f, -0.5f, // Index 3: Front-Top-Left
        -0.5f, -0.5f,  0.5f, // Index 4: Back-Bottom-Left
        0.5f, -0.5f,  0.5f, // Index 5: Back-Bottom-Right
        0.5f,  0.5f,  0.5f, // Index 6: Back-Top-Right
        -0.5f,  0.5f,  0.5f  // Index 7: Back-Top-Left
    };

    // 36 indices linking the corners into triangles
    unsigned int cubeIndices[] = {
        0, 1, 2,  2, 3, 0, // Front face
        4, 5, 6,  6, 7, 4, // Back face
        4, 0, 3,  3, 7, 4, // Left face
        1, 5, 6,  6, 2, 1, // Right face
        3, 2, 6,  6, 7, 3, // Top face
        4, 5, 1,  1, 0, 4  // Bottom face
    };


    GLuint fluidVBO, fluidVAO, fluidEBO;
    glGenVertexArrays(1, &fluidVAO);
    glGenBuffers(1, &fluidVBO);
    glGenBuffers(1, &fluidEBO);
    // bind the Vertex Array Object first, then bind and set vertex buffer(s), and then configure vertex attributes(s).
    glBindVertexArray(fluidVAO);

    glBindBuffer(GL_ARRAY_BUFFER, fluidVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVertices), cubeVertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, fluidEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(cubeIndices), cubeIndices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // note that this is allowed, the call to glVertexAttribPointer registered VBO as the vertex attribute's bound vertex buffer object so afterwards we can safely unbind
    glBindBuffer(GL_ARRAY_BUFFER, 0); 

    // remember: do NOT unbind the EBO while a VAO is active as the bound element buffer object IS stored in the VAO; keep the EBO bound.
    //glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    // You can unbind the VAO afterwards so other VAO calls won't accidentally modify this VAO, but this rarely happens. Modifying other
    // VAOs requires a call to glBindVertexArray anyways so we generally don't unbind VAOs (nor VBOs) when it's not directly necessary.
    glBindVertexArray(0); 


    //-------------------------------------------
    // liquid sim setup

    FluidSim sim;

    float** densitiesData = sim.get_densities();
    // randomly fill to test 
    sim.random_fill();
    GLuint densityTextureArray;

    glGenTextures(1, &densityTextureArray);

    glBindTexture(GL_TEXTURE_2D_ARRAY, densityTextureArray);

    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // depth and number of fluids are multipied to allocte enough memory for it
    // this lets it pack all the info into one 2d texture array on the gpu
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_R32F, sim.get_width(), sim.get_height(), sim.get_depth() * sim.get_num_fluid(), 0, GL_RED, GL_FLOAT, nullptr);

    // unbind it to safe-keep it until the main loop
    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);


    //--------------------------------------------
    // build the fluid shader 
    Shader fluid_shader("shaders/camera.vert","shaders/fluid.frag");
    // set fluid colors 
    fluid_shader.use();

    glm::vec3 fluid_colors[2] = {
    glm::vec3(0.0f, 0.4f, 0.8f), // blue
    glm::vec3(0.9f, 0.1f, 0.1f)  // red
        };
    // get memory location of fluid_colors uniform
    GLint colorsLoc = glGetUniformLocation(fluid_shader.ID, "fluid_colors");
    // send colors to gpy memory (with pointer)
    glUniform3fv(colorsLoc, 2, (float*)fluid_colors);
    // unbind safely
    glUseProgram(0);
   // ------------------

    float fov = 45.0f; // fov angle 
    float aspectRatio = 800.0f / 600.0f; 
    float nearPlane = 0.1f; // dont draw things closer than this
    float farPlane = 100.0f; // dont draw things farther than this

    
    glm::mat4 projectionMatrix = glm::perspective(glm::radians(fov), aspectRatio, nearPlane, farPlane);

    float delta_time = 0;
    float last_frame_time = 0;
    float current_frame_time = 0;
    int nbFrames = 0;

    const float radius = 10.0f;
    float camX = sin(glfwGetTime()) * radius;
    float camZ = cos(glfwGetTime()) * radius;
    glm::mat4 view = glm::lookAt(glm::vec3(camX, 0.0, camZ), glm::vec3(0.0, 0.0, 0.0), glm::vec3(0.0, 1.0, 0.0));

    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(0.0f, 0.0f, 0.0f)); 
    model = glm::scale(model, glm::vec3(4.0f, 4.0f, 4.0f));
    // render loop
    // -----------
    while (!glfwWindowShouldClose(window))
    {

        // input
        // -----
        processInput(window);
        // ------
        // clear canvas
        glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // generate moving camera
        /*
        const float radius = 10.0f;
        float camX = sin(glfwGetTime()) * radius;
        float camZ = cos(glfwGetTime()) * radius;
        glm::mat4 view = glm::lookAt(glm::vec3(camX, 0.0, camZ), glm::vec3(0.0, 0.0, 0.0), glm::vec3(0.0, 1.0, 0.0));

        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(0.0f, 0.0f, 0.0f)); 
        model = glm::scale(model, glm::vec3(4.0f, 4.0f, 4.0f));
*/
        // render wire frame
        triangle_shader.use();

        glUniformMatrix4fv(glGetUniformLocation(triangle_shader.ID, "model"), 1, GL_FALSE, glm::value_ptr(model));
        glUniformMatrix4fv(glGetUniformLocation(triangle_shader.ID, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(triangle_shader.ID, "projection"), 1, GL_FALSE, glm::value_ptr(projectionMatrix));
        
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glBindVertexArray(fluidVAO); 
        glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

        // ------ step fluid sim

        current_frame_time = glfwGetTime();
        delta_time = current_frame_time - last_frame_time;
        last_frame_time = current_frame_time;

        sim.step(delta_time);

        // ----------------------
        // debug print frame time 
        // if frames drop below 30 fps 
        if ( delta_time >= 0.033 ){ 
            printf("%f ms/frame\n", delta_time);
        }
           // printf("%f ms/frame\n", delta_time);





        // need to active shader before anything gets sent to it
        // remeber the camera needs to send info to the vert shader from this so this needs to be active BEFORE the camera sends the matrices
        fluid_shader.use();
        glUniform1i(glGetUniformLocation(fluid_shader.ID, "fluid_textures"), 0);
        
        //  send matixs to shader safe because shader is bound via .use()
        glUniformMatrix4fv(glGetUniformLocation(fluid_shader.ID, "model"), 1, GL_FALSE, glm::value_ptr(model));
        glUniformMatrix4fv(glGetUniformLocation(fluid_shader.ID, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(fluid_shader.ID, "projection"), 1, GL_FALSE, glm::value_ptr(projectionMatrix));
        
        // send the local camera position too 
        glm::vec3 worldCameraPos = glm::vec3(camX, 0.0, camZ); 
        glm::mat4 inverseModel = glm::inverse(model);
        glm::vec4 localCam4 = inverseModel * glm::vec4(worldCameraPos, 1.0f);
        glm::vec3 cameraPosLocal = glm::vec3(localCam4) + glm::vec3(0.5f);

        GLint camLoc = glGetUniformLocation(fluid_shader.ID, "camera_pos_local");
        glUniform3fv(camLoc, 1, glm::value_ptr(cameraPosLocal));

        // pass densities data to gpu for the shader
        densitiesData = sim.get_densities();

        // set gl_texture to the 0 unit (what we bound it to earler )
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D_ARRAY, densityTextureArray);

        glPixelStorei(GL_UNPACK_ALIGNMENT, 4); // set pixel store to 4bit aligned floats 
        glPixelStorei(GL_UNPACK_ROW_LENGTH, sim.get_width());  // width is N+2
        glPixelStorei(GL_UNPACK_IMAGE_HEIGHT, sim.get_height()); //hight is N+2
        // pass the texture to the gpu for each texter on the gpu texter layer i 
        for (int i = 0; i < sim.get_num_fluid(); ++i) {
            // get pointer to 3d density data of fluid #i
            float* flatVolumePtr = densitiesData[i]; 

            // upload it directly to OpenGL layer i
            // NOTE it still needs to get passed through the .vert shader
            glTexSubImage3D(
                GL_TEXTURE_2D_ARRAY, 
                0,                      
                0, 0,                   //x,y offset
                i,                      // i is the z layer offset 
                sim.get_width(), sim.get_height(), sim.get_depth(), 
                GL_RED,                 
                GL_FLOAT,               
                flatVolumePtr           
            );
           
        }
        // reset pixel storing state back 
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        glPixelStorei(GL_UNPACK_IMAGE_HEIGHT, 0);
        
        // set blending and face behavour
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDisable(GL_CULL_FACE);

        // draw fluid shader cube 
        glBindVertexArray(fluidVAO); 
        glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);

        // reset cull face 
        glEnable(GL_CULL_FACE);
        
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // optional: de-allocate all resources once they've outlived their purpose:
    // ------------------------------------------------------------------------
    glDeleteVertexArrays(1, &fluidVAO);
    glDeleteBuffers(1, &fluidVBO);
    glDeleteBuffers(1, &fluidEBO);
    // glDeleteProgram(shaderProgram); // deconstrutor handels this now

    // glfw: terminate, clearing all previously allocated GLFW resources.
    // ------------------------------------------------------------------
    glfwTerminate();
    return 0;
}

// process all input: query GLFW whether relevant keys are pressed/released this frame and react accordingly
// ---------------------------------------------------------------------------------------------------------
void processInput(GLFWwindow *window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
}

// glfw: whenever the window size changed (by OS or user resize) this callback function executes
// ---------------------------------------------------------------------------------------------
void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    // make sure the viewport matches the new window dimensions; note that width and 
    // height will be significantly larger than specified on retina displays.
    glViewport(0, 0, width, height);
}

