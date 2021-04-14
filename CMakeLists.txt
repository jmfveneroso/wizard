cmake_minimum_required (VERSION 3.0)
set(CMAKE_TRY_COMPILE_TARGET_TYPE "STATIC_LIBRARY")
project (Fbx)
set (CMAKE_CXX_STANDARD 17)
set(CMAKE_BUILD_TYPE Debug)

include(CTest)

find_package(OpenGL REQUIRED)
find_package(PythonInterp REQUIRED)
find_package(PythonLibs REQUIRED)

set(ALL_LIBS
  ${OPENGL_LIBRARY}
  glfw
  glew
  freetype
  boost_system 
  boost_filesystem
  fbxsdk
  png
  pugixml
  python3.8
)

set(INCLUDE_DIRS
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_SOURCE_DIR}/lib/libpng-1.6.37
  /usr/local/Cellar/glew/2.2.0/include
  /usr/local/Cellar/glfw/3.3.2/include
  /usr/local/Cellar/glm/0.9.9.4/include
  /usr/local/Cellar/boost/1.70.0/include
  /usr/local/opt/freetype/include/freetype2
  /Applications/Autodesk/FBX\ SDK/2020.0.1/include
  /Library/Developer/CommandLineTools/Library/Frameworks/Python3.framework/Versions/3.8/include/python3.8
  ${ZLIB_INCLUDE_DIR}
  ${CMAKE_BINARY_DIR}/lib/libpng-1.6.37
)

set(LINK_DIRS
  /usr/local/Cellar/glew/2.2.0/lib
  /usr/local/Cellar/glfw/3.3.2/lib
  /usr/local/Cellar/glm/0.9.9.4/lib
  /usr/local/Cellar/boost/1.70.0/lib
  /usr/local/opt/freetype/lib
  /Library/Developer/CommandLineTools/Library/Frameworks
  /Library/Developer/CommandLineTools/Library/Frameworks/Python3.framework/Versions/3.8/lib
  "/Applications/Autodesk/FBX\ SDK/2020.0.1/lib/clang/release/"
  ${CMAKE_BINARY_DIR}/lib/libpng-1.6.37
)
add_definitions(-DAPPLE)

file(COPY "/Applications/Autodesk/FBX\ SDK/2020.0.1/lib/clang/release/libfbxsdk.dylib"
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

file(COPY /Library/Developer/CommandLineTools/Library/Frameworks/Python3.framework/Versions/3.8/Python3
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

# Include external libs
add_subdirectory(${CMAKE_SOURCE_DIR}/lib/libpng-1.6.37)
add_subdirectory(${CMAKE_SOURCE_DIR}/lib/pugixml-1.10)

set(
  SRC
  src/renderer.cpp 
  src/terrain.cpp 
  src/util.cpp 
  src/collision.cpp 
  src/fbx_loader.cpp 
  src/resources.cpp 
  src/2d.cpp 
  src/text_editor.cpp 
  src/collision_resolver.cpp 
  src/ai.cpp 
  src/physics.cpp 
  src/4d.cpp 
  src/player.cpp 
  src/inventory.cpp 
  src/engine.cpp 
  src/game_asset.cpp 
  src/game_object.cpp 
  src/scripts.cpp 
  src/height_map.cpp 
)

include_directories(${INCLUDE_DIRS})
link_directories(${LINK_DIRS})

# Create sybil library.
add_library(wizard_lib ${SRC})
target_link_libraries(wizard_lib ${ALL_LIBS})

# Create executables.
add_executable(main src/main.cpp)
target_link_libraries(main wizard_lib)

if(BUILD_TESTING)
  add_subdirectory(test)
endif()
