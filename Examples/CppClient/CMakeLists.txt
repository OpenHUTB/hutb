cmake_minimum_required(VERSION 3.5.1)
project(example)

# 当链接器需要查找库文件时，它会在这些目录中搜索
link_directories(
    ${RPCLIB_LIB_PATH})

file(GLOB example_sources "main.cpp" "*.h")

file(GLOB example_client_sources "")

set(carla_config client)
list(APPEND build_targets example_${carla_config}_debug)

# Create targets for debug and release in the same build type.
foreach(target ${build_targets})

  add_executable(${target} ${example_sources})

  target_compile_definitions(${target} PUBLIC
      -DLIBCARLA_ENABLE_PROFILER)

  target_include_directories(${target} SYSTEM PRIVATE
      "../../LibCarla/source"
      "../../Build/debug/boost-1.86.0-install/include"
      "../../Build/debug/rpclib-install/include/"
      "../../Build/debug/recast-install/include/"
      "../../Build/zlib-install/include/"
      "../../Build/libpng-1.2.37-install/include/"
      "../../LibCarla/source/third-party/")

  # 为特定的目标（可执行文件或库）指定链接器搜索库文件的目录
  target_link_directories(${target} PRIVATE
      "../../Build/debug/boost-1.86.0-install/lib"
      "../../Build/debug/rpclib-install/lib/"
      "../../Build/debug/recast-install/lib/"
      "../../PythonAPI/carla/dependencies/lib/"  # 为了导入 carla_client_debug.lib
      "../../Build/zlib-install/lib/"
      "../../Build/libcarla-visualstudio/LibCarla/cmake/client/Debug/"
      "../../Build/libpng-1.2.37-install/lib/")

  # 将子目录的头文件包含到目标中
  target_include_directories(${target} PRIVATE
      "${libcarla_source_path}/test")

  if (WIN32)
      # 添加链接库
      target_link_libraries(${target} "rpc.lib")  # 需要使用调试版本的库
      target_link_libraries(${target} "carla_client_debug.lib") # carla_client_debug.lib或carla_client.lib
      target_link_libraries(${target} "DebugUtils-d.lib")
      target_link_libraries(${target} "Detour-d.lib")
      target_link_libraries(${target} "DetourCrowd-d.lib")
      target_link_libraries(${target} "DetourTileCache-d.lib")
      target_link_libraries(${target} "Recast-d.lib")
      target_link_libraries(${target} "Shlwapi.lib")
  else()
      target_link_libraries(${target} "-lrpc")
  endif()

  install(TARGETS ${target} DESTINATION test OPTIONAL)
endforeach(target)

if (LIBCARLA_BUILD_DEBUG)
  # Specific options for debug.
  set_target_properties(example_${carla_config}_debug PROPERTIES COMPILE_FLAGS "${CMAKE_CXX_FLAGS_DEBUG}")
  target_link_libraries(example_${carla_config}_debug "carla_${carla_config}${carla_target_postfix}_debug")
  target_compile_definitions(example_${carla_config}_debug PUBLIC -DBOOST_ASIO_ENABLE_BUFFER_DEBUGGING)
  if (CMAKE_BUILD_TYPE STREQUAL "Client")
      target_link_libraries(example_${carla_config}_debug "${BOOST_LIB_PATH}/libboost_filesystem.a")
  endif()
endif()