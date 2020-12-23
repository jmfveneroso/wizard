find_package(GTest REQUIRED)
enable_testing()

file(GLOB files "${CMAKE_CURRENT_SOURCE_DIR}/*_test.cpp")
foreach(file ${files})
  get_filename_component(test_name ${file} NAME_WE)

  add_executable(${test_name} ${file})
  target_link_libraries(${test_name} GTest::GTest GTest::Main wizard_lib)
  add_test(${test_name} ${test_name})
endforeach()

file(COPY "/Applications/Autodesk/FBX\ SDK/2020.0.1/lib/clang/release/libfbxsdk.dylib"
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
