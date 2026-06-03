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

# force static linking of compiler libraries for MinGW on Windows
if(WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(AlchemySim PRIVATE "-static-libgcc" "-static-libstdc++" "-static")
endif()

# ------ handel shader assets 
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/shaders" 
     DESTINATION "${CMAKE_CURRENT_BINARY_DIR}"
)

install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/shaders/"
        DESTINATION bin/shaders
)
#-----------------

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

#-----------------------------
# make install exe package 
#----------------------------

# install your main executable target
install(TARGETS AlchemySim
    RUNTIME DESTINATION bin
)

# set installer metadata
set(CPACK_PACKAGE_NAME "MyFluidSim")
set(CPACK_PACKAGE_VENDOR "TheoryOfMajic")
set(CPACK_PACKAGE_VERSION "1.0.0")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "fluid Sim")

# define target generators (NSIS creates a standard Windows .exe setup wizard)
set(CPACK_GENERATOR "NSIS;ZIP")

# include the CPack module (this must be at the very end)
include(CPack)



