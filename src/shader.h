#ifndef SHADER_H
#define SHADER_H

#include <glad/glad.h> // include glad to get all the required OpenGL headers
  
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
  

class Shader
{
public:
    // the program ID
    GLuint ID;
 
    // compiles shader from source files
    Shader(const std::string& vert_file_path,const std::string& frag_file_path){
        // -- read file content into string --
        std::string vert_code;
        std::ifstream vert_shader_file;

        std::string frag_code;
        std::ifstream frag_shader_file;
        
        // add errors to ifstream objects 
        vert_shader_file.exceptions (std::ifstream::failbit | std::ifstream::badbit);
        frag_shader_file.exceptions (std::ifstream::failbit | std::ifstream::badbit);    

        try {
            // open shader file and read the string
            vert_shader_file.open(vert_file_path);
            frag_shader_file.open(frag_file_path);

            std::stringstream vert_shader_stream;
            std::stringstream frag_shader_stream;

            // get the actual stream buffer data so we can read the whole file as a string (not line by line)
            vert_shader_stream << vert_shader_file.rdbuf();
            frag_shader_stream << frag_shader_file.rdbuf();
            //close file handeler
            vert_shader_file.close();
            frag_shader_file.close();

            // convert the raw data into a string 
            vert_code = vert_shader_stream.str();
            frag_code = frag_shader_stream.str();

        }
        catch(std::ifstream::failure e) {
            std::cout << "ERROR::SHADER::FILE_NOT_SUCCESFULLY_READ" << std::endl;
        }
        // conver to c string for opengl
        const char* vert_shader_code = vert_code.c_str();
        const char* frag_shader_code = frag_code.c_str();

        // --- compile shaders --
        GLuint vertex_id;
        GLuint fragment_id;
        int success;
        char info_log[512];

        // vertex shader 
        // create shader object 
        vertex_id = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertex_id, 1,&vert_shader_code,nullptr);
        glCompileShader(vertex_id);
        // print compile errors if any
        glGetShaderiv(vertex_id, GL_COMPILE_STATUS, &success);
        if(!success) {
            glGetShaderInfoLog(vertex_id, 512, NULL, info_log);
            std::cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << info_log << std::endl;
        };


        // fragment shader 
        // create shader object 
        fragment_id = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragment_id, 1,&frag_shader_code,nullptr);
        glCompileShader(fragment_id);
        // print compile errors if any
        glGetShaderiv(fragment_id, GL_COMPILE_STATUS, &success);
        if(!success) {
            glGetShaderInfoLog(fragment_id, 512, NULL, info_log);
            std::cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << info_log << std::endl;
        };

        // --- shader program
        ID = glCreateProgram();
        glAttachShader(ID, vertex_id);
        glAttachShader(ID, fragment_id);
        glLinkProgram(ID);
        // print linking errors if any
        glGetProgramiv(ID, GL_LINK_STATUS, &success);
        if(!success)
        {
            glGetProgramInfoLog(ID, 512, NULL, info_log);
            std::cout << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << info_log << std::endl;
        }

        glDeleteShader(vertex_id);
        glDeleteShader(fragment_id);
    }  

    ~Shader(){
        glDeleteProgram(ID);
    }
    
    // use/activate the shader
    void use();
    // utility uniform functions
    void set_bool(const std::string &name, bool value) const;  
    void set_int(const std::string &name, int value) const;   
    void set_float(const std::string &name, float value) const;
};
  

void Shader::use() 
{ 
    glUseProgram(ID);
}
void Shader::set_bool(const std::string &name, bool value) const
{         
    glUniform1i(glGetUniformLocation(ID, name.c_str()), (int)value); 
}
void Shader::set_int(const std::string &name, int value) const
{ 
    glUniform1i(glGetUniformLocation(ID, name.c_str()), value); 
}
void Shader::set_float(const std::string &name, float value) const
{ 
    glUniform1f(glGetUniformLocation(ID, name.c_str()), value); 
}

#endif