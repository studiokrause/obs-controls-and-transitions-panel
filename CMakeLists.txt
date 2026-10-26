cmake_minimum_required(VERSION 3.22...3.31)

# One version for the whole suite; tags are v<version>.
project(obs-horizontal-panels VERSION 1.0.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(libobs REQUIRED)
find_package(obs-frontend-api REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Core Widgets)

# add_obs_panel_plugin(<name> <source dir>)
# Builds one independent plugin DLL with its own data folder.
function(add_obs_panel_plugin name dir)
  set(PLUGIN_NAME ${name})
  set(PLUGIN_VERSION ${PROJECT_VERSION})
  configure_file(${CMAKE_SOURCE_DIR}/common/plugin-version.h.in ${CMAKE_CURRENT_BINARY_DIR}/${name}/plugin-version.h @ONLY)

  add_library(${name} MODULE ${dir}/src/plugin-main.cpp ${CMAKE_SOURCE_DIR}/common/hpanels.hpp)
  target_include_directories(${name} PRIVATE ${CMAKE_SOURCE_DIR}/common ${CMAKE_CURRENT_BINARY_DIR}/${name})
  target_link_libraries(${name} PRIVATE OBS::libobs OBS::obs-frontend-api Qt6::Core Qt6::Widgets)
  set_target_properties(${name} PROPERTIES PREFIX "")

  if(MSVC)
    target_compile_options(${name} PRIVATE /W3 /utf-8 /Zc:__cplusplus)
  endif()

  # Per-plugin folder layout used by OBS 28+:
  #   <prefix>/<name>/bin/64bit/<name>.dll
  #   <prefix>/<name>/data/locale/*.ini
  if(WIN32)
    install(TARGETS ${name} LIBRARY DESTINATION ${name}/bin/64bit)
    install(FILES $<TARGET_PDB_FILE:${name}> DESTINATION ${name}/bin/64bit OPTIONAL)
    install(DIRECTORY ${dir}/data/ DESTINATION ${name}/data)
  else()
    install(TARGETS ${name} LIBRARY DESTINATION lib/obs-plugins)
    install(DIRECTORY ${dir}/data/ DESTINATION share/obs/obs-plugins/${name})
  endif()
endfunction()

add_obs_panel_plugin(horizontal-controls horizontal-controls)
add_obs_panel_plugin(horizontal-scene-transitions horizontal-scene-transitions)
add_obs_panel_plugin(horizontal-studio-transitions horizontal-studio-transitions)
