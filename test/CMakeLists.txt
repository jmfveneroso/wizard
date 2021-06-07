find_package(GTest REQUIRED)
enable_testing()

file(GLOB files "${CMAKE_CURRENT_SOURCE_DIR}/*_test.cpp")
foreach(file ${files})
  get_filename_component(test_name ${file} NAME_WE)

  add_executable(${test_name} ${file})
  target_link_libraries(${test_name} GTest::GTest GTest::Main wizard_lib)
  target_link_libraries(${test_name} wizard_lib)
  add_test(${test_name} ${test_name})
endforeach()

add_executable(dungeon_main "${CMAKE_CURRENT_SOURCE_DIR}/dungeon_main.cpp")
target_link_libraries(dungeon_main wizard_lib)
# add_test(${test_name} ${test_name})

file(COPY "/Applications/Autodesk/FBX\ SDK/2020.0.1/lib/clang/release/libfbxsdk.dylib"
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

file(COPY /Library/Developer/CommandLineTools/Library/Frameworks/Python3.framework/Versions/3.8/Python3
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
