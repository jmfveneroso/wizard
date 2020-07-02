cmake_minimum_required (VERSION 3.0)
project (Fbx)
set (CMAKE_CXX_STANDARD 17)

find_package(OpenGL REQUIRED)

set(ALL_LIBS
  ${OPENGL_LIBRARY}
  glfw
  glew
  freetype
  boost_regex-mt
  boost_system 
  boost_filesystem
  fbxsdk
)

set(INCLUDE_DIRS
  ${CMAKE_SOURCE_DIR}/src
  /usr/local/Cellar/glew/2.1.0/include
  /usr/local/Cellar/glfw/3.2.1/include
  /usr/local/Cellar/glm/0.9.9.4/include
  /usr/local/Cellar/boost/1.70.0/include
  /usr/local/Cellar/freetype/2.10.1/include/freetype2
  /Applications/Autodesk/FBX\ SDK/2020.0.1/include
)

set(LINK_DIRS
  /usr/local/Cellar/glew/2.1.0/lib
  /usr/local/Cellar/glfw/3.2.1/lib
  /usr/local/Cellar/glm/0.9.9.4/lib
  /usr/local/Cellar/boost/1.70.0/lib
  /usr/local/Cellar/freetype/2.10.1/lib
  "/Applications/Autodesk/FBX\ SDK/2020.0.1/lib/clang/release/"
)
add_definitions(-DAPPLE)

file(COPY "/Applications/Autodesk/FBX\ SDK/2020.0.1/lib/clang/release/libfbxsdk.dylib"
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

set(
  SRC
  src/shaders.cpp 
  src/renderer.cpp 
  src/fbx_loader.cpp 
)

include_directories(${INCLUDE_DIRS})
link_directories(${LINK_DIRS})

# Create sybil library.
add_library(test ${SRC})
target_link_libraries(test ${ALL_LIBS})

# Create main executable.
add_executable(main main.cpp)
target_link_libraries(main test)
