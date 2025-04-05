cmake_minimum_required(VERSION 3.14)
project(VulkanTriangle C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set build type to Debug if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# Set all outputs to the build directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# Download CPM.cmake v0.40.7
file(DOWNLOAD
    "https://github.com/cpm-cmake/CPM.cmake/releases/download/v0.40.7/CPM.cmake"
    "${CMAKE_BINARY_DIR}/CPM_0.40.7.cmake"
)
include(${CMAKE_BINARY_DIR}/CPM_0.40.7.cmake)

# Add dependencies with CPM
CPMAddPackage(
    NAME SDL3
    GITHUB_REPOSITORY libsdl-org/SDL
    GIT_TAG release-3.2.8
)

CPMAddPackage(
    NAME VMA
    GITHUB_REPOSITORY GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator
    GIT_TAG v3.2.1
)

# CPMAddPackage(
#     NAME volk
#     GITHUB_REPOSITORY zeux/volk
#     GIT_TAG 1.4.304
# )

CPMAddPackage(
    NAME VulkanHeaders
    GITHUB_REPOSITORY KhronosGroup/Vulkan-Headers
    GIT_TAG vulkan-sdk-1.4.304.1
)

CPMAddPackage(
    NAME freetype
    GITHUB_REPOSITORY freetype/freetype
    GIT_TAG VER-2-13-3
    OPTIONS
      "FT_DISABLE_ZLIB TRUE"
      "FT_DISABLE_BZIP2 TRUE"
      "FT_DISABLE_PNG TRUE"
      "FT_DISABLE_HARFBUZZ TRUE"
      "FT_DISABLE_BROTLI TRUE"
)

CPMAddPackage(
    NAME cimgui
    GITHUB_REPOSITORY cimgui/cimgui
    GIT_TAG 1.91.9
    DOWNLOAD_ONLY TRUE
)

# Find Vulkan from the system
find_package(Vulkan REQUIRED)

# cimgui library
set(CIMGUI_SRC
    ${cimgui_SOURCE_DIR}/cimgui.cpp
    ${cimgui_SOURCE_DIR}/imgui/imgui.cpp
    ${cimgui_SOURCE_DIR}/imgui/imgui_draw.cpp
    ${cimgui_SOURCE_DIR}/imgui/imgui_demo.cpp
    ${cimgui_SOURCE_DIR}/imgui/imgui_widgets.cpp
    ${cimgui_SOURCE_DIR}/imgui/imgui_tables.cpp
    ${cimgui_SOURCE_DIR}/imgui/backends/imgui_impl_sdl3.cpp
    ${cimgui_SOURCE_DIR}/imgui/backends/imgui_impl_vulkan.cpp
    ${cimgui_SOURCE_DIR}/imgui/misc/freetype/imgui_freetype.cpp
)

add_library(cimgui_lib STATIC ${CIMGUI_SRC})
target_compile_definitions(cimgui_lib PUBLIC
    -DIMGUI_DISABLE_OBSOLETE_FUNCTIONS=1
    -DCIMGUI_USE_VULKAN
    -DCIMGUI_USE_SDL3
    -DCIMGUI_FREETYPE=1
    "-DIMGUI_USER_CONFIG=\"${cimgui_SOURCE_DIR}/cimconfig.h\""
)
target_include_directories(cimgui_lib PUBLIC
    ${cimgui_SOURCE_DIR}
    ${cimgui_SOURCE_DIR}/imgui
    ${cimgui_SOURCE_DIR}/imgui/backends
    ${SDL3_SOURCE_DIR}/include
    ${VulkanHeaders_SOURCE_DIR}/include
)
target_link_libraries(cimgui_lib PUBLIC
    SDL3::SDL3
    Vulkan::Vulkan
    # volk
    freetype
)

if (WIN32)
    target_compile_definitions(cimgui_lib PUBLIC "-DIMGUI_IMPL_API=extern \"C\" __declspec\(dllexport\)")
else(WIN32)
    target_compile_definitions(cimgui_lib PUBLIC "-DIMGUI_IMPL_API=extern \"C\" ")
endif(WIN32)



# Define source and include directories
set(SOURCE_DIR ${CMAKE_SOURCE_DIR}/src)
set(INCLUDE_DIR ${CMAKE_SOURCE_DIR}/include)

set(SRC_FILES
  ${SOURCE_DIR}/vsdl_init.c
  ${SOURCE_DIR}/vsdl_renderer.c
  ${SOURCE_DIR}/vsdl_mesh.c
  ${SOURCE_DIR}/vsdl_pipeline.c
  ${SOURCE_DIR}/vsdl_cleanup.c
  ${SOURCE_DIR}/vsdl_cimgui.c
  ${SOURCE_DIR}/vsdl_text.c
  ${SOURCE_DIR}/vsdl_utils.c
  ${SOURCE_DIR}/vma_impl.cpp
)

# Define the executable with all source files
add_executable(${PROJECT_NAME}
  ${SOURCE_DIR}/main.c
  ${SRC_FILES}
)

target_compile_definitions(${PROJECT_NAME} PUBLIC
    -DCIMGUI_DEFINE_ENUMS_AND_STRUCTS
    -DIMGUI_DISABLE_OBSOLETE_FUNCTIONS=1
)

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    SDL3::SDL3
    Vulkan::Vulkan
    # volk
    freetype
    # cimgui
    cimgui_lib
)

# Include directories with proper path validation
target_include_directories(${PROJECT_NAME} PRIVATE
    ${INCLUDE_DIR}
    ${SDL3_SOURCE_DIR}/include
    ${VMA_SOURCE_DIR}/include
    # ${volk_SOURCE_DIR}
    ${VulkanHeaders_SOURCE_DIR}/include
    ${Vulkan_INCLUDE_DIRS}
)

# Shader handling
set(SHADER_SRC_DIR ${CMAKE_SOURCE_DIR}/shaders)
set(SHADER_DEST_DIR ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>/shaders)

if(NOT EXISTS ${SHADER_SRC_DIR})
    message(FATAL_ERROR "Shaders directory '${SHADER_SRC_DIR}' does not exist. Please create it and add 'tri.vert' and 'tri.frag' files.")
endif()

# Find glslc shader compiler
find_program(GLSLC_EXECUTABLE glslc HINTS "C:/VulkanSDK/1.4.304.1/Bin" "$ENV{VULKAN_SDK}/Bin")
if(NOT GLSLC_EXECUTABLE)
    message(FATAL_ERROR "glslc not found. Ensure Vulkan SDK is installed at C:/VulkanSDK/1.4.304.1/")
endif()

# Define shader files
set(SHADER_FILES
    ${SHADER_SRC_DIR}/shader2d.vert
    ${SHADER_SRC_DIR}/shader2d.frag
    ${SHADER_SRC_DIR}/text.vert
    ${SHADER_SRC_DIR}/text.frag
)

# Compile shaders
foreach(SHADER ${SHADER_FILES})
    get_filename_component(SHADER_NAME ${SHADER} NAME)
    set(SHADER_OUTPUT ${SHADER_DEST_DIR}/${SHADER_NAME}.spv)
    add_custom_command(
        OUTPUT ${SHADER_OUTPUT}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADER_DEST_DIR}
        COMMAND ${GLSLC_EXECUTABLE} ${SHADER} -o ${SHADER_OUTPUT}
        DEPENDS ${SHADER}
        COMMENT "Compiling ${SHADER_NAME} to SPIR-V in ${SHADER_DEST_DIR}"
    )
    list(APPEND SHADER_OUTPUTS ${SHADER_OUTPUT})
endforeach()

add_custom_target(Shaders ALL DEPENDS ${SHADER_OUTPUTS})
add_dependencies(${PROJECT_NAME} Shaders)

# Copy SDL3 DLL only if it doesn't exist
if(WIN32 AND TARGET SDL3::SDL3-shared)
    set(SDL3_DLL_DEST "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>/SDL3.dll")
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL3::SDL3-shared>
        ${SDL3_DLL_DEST}
        COMMENT "Copying SDL3.dll to ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG> if needed"
    )
endif()

# Copy fonts folder to build output
set(FONTS_SRC_DIR ${CMAKE_SOURCE_DIR}/fonts)
set(FONTS_DEST_DIR ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>/fonts)

# Check if fonts directory exists
if(NOT EXISTS ${FONTS_SRC_DIR})
    message(WARNING "Fonts directory '${FONTS_SRC_DIR}' does not exist. Creating empty directory in build output.")
endif()

# Add custom command to copy fonts directory
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${FONTS_DEST_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${FONTS_SRC_DIR} ${FONTS_DEST_DIR}
    COMMENT "Copying fonts directory to ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>/fonts"
)