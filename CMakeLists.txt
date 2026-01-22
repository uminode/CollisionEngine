cmake_minimum_required(VERSION 3.20)
project(CollisionEngine LANGUAGES CXX)

# ---------- SETTINGS ----------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
include(FetchContent) 
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

set(D3D12_VERSION "1.618.5")
set(VULKAN_MAJOR_VERSION "1.4.328")
set(VULKAN_MINOR_VERSION ".1")
set(VULKAN_VERSION "${VULKAN_MAJOR_VERSION}${VULKAN_MINOR_VERSION}")

#include(spirv-cross)
# =============
# Slang Library 
# =============
#set(SLANG_VERSION "2025.19.1")
set(SLANG_VERSION "2026.1")
if(WIN32)
    set(SLANG_OS "windows-x86_64")
elseif(APPLE)
    set(SLANG_OS "macos-aarch64")
elseif(UNIX)
    set(SLANG_OS "linux-x86_64")
endif()

message(STATUS "Downloading Slang ${SLANG_VERSION} from ${SLANG_URL}...")

FetchContent_Declare(slang_zip 
					URL https://github.com/shader-slang/slang/releases/download/v${SLANG_VERSION}/slang-${SLANG_VERSION}-${SLANG_OS}.zip
					DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(slang_zip)

#message(STATUS "Slang dir: ${slang_zip_SOURCE_DIR}") #DEBUG
add_library(slang SHARED IMPORTED GLOBAL)
if(WIN32)
    set_target_properties(slang PROPERTIES
        IMPORTED_LOCATION ${slang_zip_SOURCE_DIR}/bin/slang.dll
        IMPORTED_IMPLIB ${slang_zip_SOURCE_DIR}/lib/slang.lib
        INTERFACE_INCLUDE_DIRECTORIES ${slang_zip_SOURCE_DIR}/include
    )
elseif(APPLE)
    set_target_properties(slang PROPERTIES
        IMPORTED_LOCATION ${slang_zip_SOURCE_DIR}/lib/libslang.dylib
        INTERFACE_INCLUDE_DIRECTORIES ${slang_zip_SOURCE_DIR}/include
    )
elseif(UNIX)
    set_target_properties(slang PROPERTIES
        IMPORTED_LOCATION ${slang_zip_SOURCE_DIR}/lib/libslang.so
        INTERFACE_INCLUDE_DIRECTORIES ${slang_zip_SOURCE_DIR}/include
    )
endif()
add_library(slang::slang ALIAS slang) # optional: for slang::slang

# ==========
# D3D12 SDK
# ==========

set(D3D12_URL "https://www.nuget.org/api/v2/package/Microsoft.Direct3D.D3D12/${D3D12_VERSION}")

message(STATUS "Downloading D3D12 ${D3D12_VERSION} SDK from ${D3D12_URL}...")

FetchContent_Declare(d3d12_nuget 
                    URL ${D3D12_URL}
                    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)

#FetchContent_Populate(d3d12_nuget)
FetchContent_MakeAvailable(d3d12_nuget)

set(D3D12_ROOT "${d3d12_nuget_SOURCE_DIR}")
set(D3D12_INCLUDE "${d3d12_nuget_SOURCE_DIR}/build/native/include")
set(D3D12_BIN_DIR "${d3d12_nuget_SOURCE_DIR}/build/native/bin/x64")

# =========
# Vulkan SDK detection
# =========
if(NOT DEFINED VULKAN_SDK_ROOT)
    # TODO: update with your actual version string once known
    set(_VULKAN_CANDIDATE "D:/VulkanSDK/${VULKAN_VERSION}")
    if(EXISTS "${_VULKAN_CANDIDATE}")
        set(VULKAN_SDK_ROOT "${_VULKAN_CANDIDATE}")
    endif()
endif()

if(VULKAN_SDK_ROOT)
    message(STATUS "Using Vulkan SDK from ${VULKAN_SDK_ROOT}")

    set(VULKAN_INCLUDE_DIR "${VULKAN_SDK_ROOT}/Include")
    set(VULKAN_LIB_DIR     "${VULKAN_SDK_ROOT}/Lib")

else()
    message(STATUS "Vulkan SDK not found; Fetching Vulkan / SPIRV / SPIRV-Cross from the internet.")
	# Vulkan-Headers
	FetchContent_Declare(
		vulkan_headers
		GIT_REPOSITORY https://github.com/KhronosGroup/Vulkan-Headers.git
		GIT_TAG vulkan-sdk-${VULKAN_MAJOR_VERSION}
	)
	FetchContent_MakeAvailable(vulkan_headers)
	set(VULKAN_INCLUDE_DIR "${vulkan_headers_SOURCE_DIR}/include")

	# Vulkan-Loader (core API)
	FetchContent_Declare(
		vulkan_loader
		GIT_REPOSITORY https://github.com/KhronosGroup/Vulkan-Loader.git
		GIT_TAG vulkan-sdk-${VULKAN_MAJOR_VERSION}
	)
    set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
	set(BUILD_WSI_SAMPLES OFF CACHE BOOL "" FORCE)
	FetchContent_MakeAvailable(vulkan_loader)

	# Vulkan-ValidationLayers 
	FetchContent_Declare(
		vulkan_validation
		GIT_REPOSITORY https://github.com/KhronosGroup/Vulkan-ValidationLayers.git
		GIT_TAG vulkan-sdk-${VULKAN_MAJOR_VERSION}
	)
    set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
	set(BUILD_WSI_SAMPLES OFF CACHE BOOL "" FORCE)
	FetchContent_MakeAvailable(vulkan_validation)

    if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
		if(UNIX)
			set_property(TARGET CollisionEngine APPEND PROPERTY
				ENVIRONMENT "VK_LAYER_PATH=${vulkan_validation_BINARY_DIR}/layers"
			)
		endif()
	endif()
endif()

# ========= GLFW =========
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

#========== GLAD ==========
add_library(glad STATIC "${CMAKE_SOURCE_DIR}/src/glad/glad.c")
target_include_directories(glad PUBLIC "${CMAKE_SOURCE_DIR}/include")


# ========= GLM =========
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.3
)
FetchContent_MakeAvailable(glm)

# ===== SPIRV-CROSS ===== HOLY COW THIS REPO IS A MESS TO ADD VIA CMAKE
FetchContent_Declare(
    spirv_cross
    GIT_REPOSITORY https://github.com/KhronosGroup/SPIRV-Cross.git
	GIT_TAG vulkan-sdk-${VULKAN_MAJOR_VERSION}
)
FetchContent_Populate(spirv_cross)

# ---------- SOURCE COLLECTION ----------
file(GLOB_RECURSE SRC_FILES
    "${CMAKE_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_SOURCE_DIR}/src/*.h"
    "${CMAKE_SOURCE_DIR}/src/shaders/*.slang" # so they show up in final project
)

# ------ GENERAL DEPENDENCIES (should be eliminated and fetched from the internet if not there) ------
#set(INCLUDE_DIR "${CMAKE_SOURCE_DIR}/Dependencies/Code")
#set(LIBRARIES_DIR "${CMAKE_SOURCE_DIR}/Dependencies/Libraries")

# ---------- EXECUTABLE ----------
add_executable(CollisionEngine ${SRC_FILES})

source_group("External/spirv-cross" FILES
	${spirv_cross_SOURCE_DIR}/spirv_cfg.cpp
	${spirv_cross_SOURCE_DIR}/spirv_cross.cpp
	${spirv_cross_SOURCE_DIR}/spirv_cross_parsed_ir.cpp
	${spirv_cross_SOURCE_DIR}/spirv_parser.cpp
	${spirv_cross_SOURCE_DIR}/spirv_glsl.cpp
)

target_sources(CollisionEngine PRIVATE
	${spirv_cross_SOURCE_DIR}/spirv_cfg.cpp
	${spirv_cross_SOURCE_DIR}/spirv_cross.cpp
	${spirv_cross_SOURCE_DIR}/spirv_cross_parsed_ir.cpp
	${spirv_cross_SOURCE_DIR}/spirv_parser.cpp
	${spirv_cross_SOURCE_DIR}/spirv_glsl.cpp
)

# ---------- INCLUDE PATHS ----------
target_include_directories(CollisionEngine PRIVATE
	"${VULKAN_INCLUDE_DIR}"    # vulkan.h, glm, spirv headers
	"${D3D12_INCLUDE}"       # d3dx12.h, dxgi.h
    "${slang_zip_SOURCE_DIR}"  # Slang headers from its own directory
	"${spirv_cross_SOURCE_DIR}"
)

# ---------- PREPROCESSOR DEFINES ----------
target_compile_definitions(CollisionEngine PRIVATE
    SLANG_STATIC
    GLM_ENABLE_EXPERIMENTAL
)

# ---------- LINK LIBRARIES ----------
target_link_directories(CollisionEngine PRIVATE
    "${VULKAN_LIB_DIR}"
)
#target_link_directories(CollisionEngine "${LIBRARIES_DIR}")

if(WIN32)
	target_link_libraries(CollisionEngine PRIVATE
		slang
		glfw
        glad
		opengl32
        vulkan-1
		user32
		gdi32
		shell32
		winmm
		d3d12
		dxgi
        dxcompiler
	)
else()
	target_link_libraries(CollisionEngine PRIVATE
		slang
		glfw
		glew_s
		opengl32
        vulkan-1
		user32
		gdi32
		shell32
		glew32s
	)
endif()

# ---------- COMPILER WARNINGS ----------
if (MSVC)
    target_compile_options(CollisionEngine PRIVATE /W4 /permissive-)
endif()

# ---------- POST-BUILD: Copy Slang DLL ----------
#if(MSVC AND CMAKE_GENERATOR MATCHES "Visual Studio")
## WHAT DOES A PERSON HAVE TO DO TO WORK WITH Visual Studio....
#	set_target_properties(CollisionEngine PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${OutDir}")
#else()
#	set_target_properties(CollisionEngine PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:CollisionEngine>")
#endif()
if(WIN32)
    add_custom_command(TARGET CollisionEngine POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:slang>
        $<TARGET_FILE_DIR:CollisionEngine>
        COMMENT "Copying Slang DLL to output directory"
    )
    add_custom_command(TARGET CollisionEngine POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${slang_zip_SOURCE_DIR}/bin/slang-compiler.dll"
        $<TARGET_FILE_DIR:CollisionEngine>
        COMMENT "Copying Slang DLL to output directory"
    )
    add_custom_command(TARGET CollisionEngine POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            $<TARGET_FILE_DIR:CollisionEngine>/D3D12
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${D3D12_BIN_DIR}/D3D12Core.dll"
            $<TARGET_FILE_DIR:CollisionEngine>/D3D12
        COMMENT "Copying D3D12Core.dll (Agility) to output directory..."
    )
endif()
# ---------- POST-BUILD: Copy Shaders ----------
add_custom_command(TARGET CollisionEngine POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/src/shaders
    $<TARGET_FILE_DIR:CollisionEngine>/shaders
    COMMENT "Copying slang shaders to output directory"
)
add_custom_command(TARGET CollisionEngine POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/Textures
    $<TARGET_FILE_DIR:CollisionEngine>/textures
    COMMENT "Copying textures to output directory"
)
# ---------- INSTALL (optional) ----------
#install(TARGETS CollisionEngine DESTINATION bin)