cmake_minimum_required(VERSION 3.10)
project(AlchemySim CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# treat glad as c++ to fix linking
set_source_files_properties(src/glad.c PROPERTIES LANGUAGE CXX)

add_executable(AlchemySim 
    src/main.cpp 
    src/glad.c
    src/fluidsim.h
    src/shader.h
)

# add math optimization options 
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    target_compile_options(AlchemySim PRIVATE -mavx2 -ffast-math)
elseif(MSVC)
    target_compile_options(AlchemySim PRIVATE /arch:AVX2 /fp:fast)
endif()

target_include_directories(AlchemySim PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_directories(AlchemySim PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/lib)

target_link_libraries(AlchemySim PRIVATE 
    glfw3 
    opengl32 
    gdi32
)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/glfw3.dll 
    ${CMAKE_CURRENT_BINARY_DIR}/glfw3.dll 
    COPYONLY
)
