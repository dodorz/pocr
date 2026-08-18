cmake_minimum_required(VERSION 3.14)
project(pocr CXX)

set(DEMO_NAME "pocr")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Unify MSVC runtime to static (/MT) for all targets incl. subprojects (abseil).
# Paddle prebuilt static libs (yaml-cpp/gflags/glog/abseil) are /MT builds.
if (MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
  set(FLAG_OPENMP "/openmp")
  set(CMAKE_C_FLAGS_DEBUG   "${CMAKE_C_FLAGS_DEBUG} /bigobj /MTd ${FLAG_OPENMP}")
  set(CMAKE_C_FLAGS_RELEASE  "${CMAKE_C_FLAGS_RELEASE} /bigobj /MT ${FLAG_OPENMP}")
  set(CMAKE_CXX_FLAGS_DEBUG  "${CMAKE_CXX_FLAGS_DEBUG} /bigobj /MTd ${FLAG_OPENMP}")
  set(CMAKE_CXX_FLAGS_RELEASE   "${CMAKE_CXX_FLAGS_RELEASE} /bigobj /MT ${FLAG_OPENMP}")
endif()

option(WITH_MKL        "Compile demo with MKL/OpenBlas support, default use MKL."       ON)
option(WITH_GPU        "Compile demo with GPU/CPU, default use CPU."                    OFF)
# Paddle official Windows prebuilt static third-party libs (yaml-cpp etc.) are
# compiled with /MT. The whole program must be /MT; paddle_inference.dll brings
# its own /MD CRT. (This matches the official cpp_infer build flags.)
option(WITH_STATIC_LIB "Compile demo with static/shared library, default use static."   ON)

macro(safe_set_static_flag)
    foreach(flag_var
        CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE
        CMAKE_CXX_FLAGS_MINSIZEREL CMAKE_CXX_FLAGS_RELWITHDEBINFO)
      if(${${flag_var}} MATCHES "/MD")
        string(REGEX REPLACE "/MD" "/MT" ${flag_var} "${${flag_var}}")
      endif()
    endforeach(flag_var)
endmacro()

SET(PADDLE_LIB "" CACHE PATH "Location of Paddle Inference libs")
SET(OPENCV_DIR "" CACHE PATH "Location of OpenCV build dir")

# ---------------------------------------------------------------------------
# Official PaddleOCR cpp_infer source tree (vendor/ppocr_upstream)
# ---------------------------------------------------------------------------
set(PPOCR_CPP_INFER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vendor/ppocr_upstream/deploy/cpp_infer")

if(NOT DEFINED PADDLE_LIB OR "${PADDLE_LIB}" STREQUAL "")
  message(FATAL_ERROR "please set PADDLE_LIB with -DPADDLE_LIB=/path/to/paddle_inference")
endif()
if(NOT DEFINED OPENCV_DIR OR "${OPENCV_DIR}" STREQUAL "")
  message(FATAL_ERROR "please set OPENCV_DIR with -DOPENCV_DIR=/path/to/opencv/build")
endif()

if (WITH_MKL)
    ADD_DEFINITIONS(-DUSE_MKL)
endif()

# ---------------------------------------------------------------------------
# Paddle Inference third-party headers/libs (bundled in PADDLE_LIB)
# ---------------------------------------------------------------------------
include_directories("${PADDLE_LIB}/paddle/include")
include_directories("${PADDLE_LIB}/third_party/install/protobuf/include")
include_directories("${PADDLE_LIB}/third_party/install/glog/include")
include_directories("${PADDLE_LIB}/third_party/install/gflags/include")
include_directories("${PADDLE_LIB}/third_party/install/xxhash/include")
include_directories("${PADDLE_LIB}/third_party/install/zlib/include")
include_directories("${PADDLE_LIB}/third_party/install/onnxruntime/include")
include_directories("${PADDLE_LIB}/third_party/install/paddle2onnx/include")
include_directories("${PADDLE_LIB}/third_party/install/yaml-cpp/include")
include_directories("${PADDLE_LIB}/third_party/install/openvino/include")
include_directories("${PADDLE_LIB}/third_party/install/tbb/include")
include_directories("${PADDLE_LIB}/third_party/boost")
include_directories("${PADDLE_LIB}/third_party/eigen3")
include_directories("${PADDLE_LIB}/paddle/include/")
include_directories("${PPOCR_CPP_INFER_DIR}")
# dirent.h for MSVC (POSIX dir API used by official src)
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/vendor")
# "third_party/nlohmann/json.hpp" relative include -> build/third_party/nlohmann
include_directories("${CMAKE_BINARY_DIR}")
# PDFium
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/third_party/pdfium/include")

link_directories("${PADDLE_LIB}/third_party/install/zlib/lib")
link_directories("${PADDLE_LIB}/third_party/install/protobuf/lib")
link_directories("${PADDLE_LIB}/third_party/install/glog/lib")
link_directories("${PADDLE_LIB}/third_party/install/gflags/lib")
link_directories("${PADDLE_LIB}/third_party/install/xxhash/lib")
link_directories("${PADDLE_LIB}/third_party/install/onnxruntime/lib")
link_directories("${PADDLE_LIB}/third_party/install/paddle2onnx/lib")
link_directories("${PADDLE_LIB}/third_party/install/yaml-cpp/lib")
link_directories("${PADDLE_LIB}/third_party/install/openvino/intel64")
link_directories("${PADDLE_LIB}/third_party/install/tbb/lib")
link_directories("${PADDLE_LIB}/paddle/lib")

if(WITH_MKL)
  include_directories("${PADDLE_LIB}/third_party/install/mklml/include")
  set(MATH_LIB ${PADDLE_LIB}/third_party/install/mklml/lib/mklml.lib
               ${PADDLE_LIB}/third_party/install/mklml/lib/libiomp5md.lib)
  set(MKLDNN_PATH "${PADDLE_LIB}/third_party/install/onednn")
  if(EXISTS ${MKLDNN_PATH})
    include_directories("${MKLDNN_PATH}/include")
    set(MKLDNN_LIB ${MKLDNN_PATH}/lib/mkldnn.lib)
  endif()
else()
  set(MATH_LIB ${PADDLE_LIB}/third_party/install/openblas/lib/openblas.lib)
endif()

set(DEPS
    ${PADDLE_LIB}/paddle/lib/paddle_inference.lib
    ${MATH_LIB} ${MKLDNN_LIB}
    glog gflags_static libprotobuf xxhash yaml-cpp
    shlwapi)
if (EXISTS "${PADDLE_LIB}/third_party/install/snappy/lib")
    set(DEPS ${DEPS} snappy)
endif()
if(EXISTS "${PADDLE_LIB}/third_party/install/snappystream/lib")
    set(DEPS ${DEPS} snappystream)
endif()

# ---------------------------------------------------------------------------
# Extra C++ deps: abseil-cpp / clipper / nlohmann (same as official)
# ---------------------------------------------------------------------------
set(THIRD_PARTY_PATH ${CMAKE_BINARY_DIR}/third_party)
function(download_and_decompress url filename decompress_dir)
  if(NOT EXISTS "${filename}" AND NOT EXISTS "${decompress_dir}")
    message(STATUS "Downloading ${url} ...")
    file(DOWNLOAD ${url} "${filename}.tmp" SHOW_PROGRESS)
    file(RENAME "${filename}.tmp" ${filename})
  endif()
  if(NOT EXISTS ${decompress_dir})
    file(MAKE_DIRECTORY ${decompress_dir})
    message(STATUS "Decompress ${filename} ...")
    execute_process(COMMAND ${CMAKE_COMMAND} -E tar -xf ${filename} WORKING_DIRECTORY ${decompress_dir})
  endif()
endfunction()

set(PACKAGE_LIST abseil-cpp clipper_ver6.4.2 nlohmann)
foreach(PKG ${PACKAGE_LIST})
    set(PKG_URL "https://paddle-model-ecology.bj.bcebos.com/paddlex/cpp/libs/${PKG}.tgz")
    set(PKG_TGZ_PATH "${CMAKE_BINARY_DIR}/${PKG}.tgz")
    set(PKG_DST_PATH "${THIRD_PARTY_PATH}/${PKG}")
    download_and_decompress(${PKG_URL} ${PKG_TGZ_PATH} ${PKG_DST_PATH})
endforeach()

# abseil subproject: static libs + global /MT CRT (inherited above)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "build shared libs" FORCE)
add_subdirectory(${THIRD_PARTY_PATH}/abseil-cpp)
add_subdirectory(${THIRD_PARTY_PATH}/clipper_ver6.4.2/cpp)
include_directories(${POLYCLIPPING_INCLUDE_DIR})

# ---------------------------------------------------------------------------
# OpenCV (must be found BEFORE DEPS uses ${OpenCV_LIBS})
# ---------------------------------------------------------------------------
if (WIN32)
  set(OpenCV_DIR "${OPENCV_DIR}/x64/vc16/lib")
endif()
find_package(OpenCV REQUIRED)
include_directories(${OpenCV_INCLUDE_DIRS})

set(DEPS ${DEPS} ${OpenCV_LIBS})
set(DEPS ${DEPS} absl::statusor)
set(DEPS ${DEPS} polyclipping)
set(DEPS ${DEPS} ${CMAKE_CURRENT_SOURCE_DIR}/third_party/pdfium/lib/pdfium.dll.lib)

if (WIN32)
    add_definitions("/DGOOGLE_GLOG_DLL_DECL=")
    if(WITH_MKL)
        set(FLAG_OPENMP "/openmp")
    endif()
    set(CMAKE_C_FLAGS_DEBUG   "${CMAKE_C_FLAGS_DEBUG} /bigobj /MTd ${FLAG_OPENMP}")
    set(CMAKE_C_FLAGS_RELEASE  "${CMAKE_C_FLAGS_RELEASE} /bigobj /MT ${FLAG_OPENMP}")
    set(CMAKE_CXX_FLAGS_DEBUG  "${CMAKE_CXX_FLAGS_DEBUG} /bigobj /MTd ${FLAG_OPENMP}")
    set(CMAKE_CXX_FLAGS_RELEASE   "${CMAKE_CXX_FLAGS_RELEASE} /bigobj /MT ${FLAG_OPENMP}")
    if (WITH_STATIC_LIB)
        safe_set_static_flag()
        add_definitions(-DSTATIC_LIB)
        add_definitions(-DYAML_CPP_STATIC_DEFINE)
    endif()
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O3 ${FLAG_OPENMP} -std=c++17")
    set(CMAKE_STATIC_LIBRARY_PREFIX "")
endif()

# ---------------------------------------------------------------------------
# Sources: our CLI + official src (Apache-2.0)
# ---------------------------------------------------------------------------
file(GLOB_RECURSE OFFICIAL_SRC "${PPOCR_CPP_INFER_DIR}/src/*.cc")
file(GLOB OUR_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
list(REMOVE_ITEM OFFICIAL_SRC "${PPOCR_CPP_INFER_DIR}/cli.cc")

add_executable(${DEMO_NAME} ${OUR_SRC} ${OFFICIAL_SRC})
target_link_libraries(${DEMO_NAME} ${DEPS})

set(PCOR_BIN_DIR "$<TARGET_FILE_DIR:${DEMO_NAME}>")

# The upstream C++ API resolves this from __FILE__, which is invalid after the
# executable is packaged without the source tree. Keep a runtime copy next to
# pocr.exe; src/main.cpp passes this path to PaddleOCR explicitly.
add_custom_command(TARGET ${DEMO_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${PCOR_BIN_DIR}/configs"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${PPOCR_CPP_INFER_DIR}/src/configs/OCR.yaml"
        "${PCOR_BIN_DIR}/configs/OCR.yaml"
)

# ---------------------------------------------------------------------------
# Copy runtime DLLs next to the exe
# ---------------------------------------------------------------------------
if (WIN32)
    add_custom_command(TARGET ${DEMO_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${PADDLE_LIB}/paddle/lib/paddle_inference.dll ${PCOR_BIN_DIR}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${PADDLE_LIB}/paddle/lib/common.dll ${PCOR_BIN_DIR}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${PADDLE_LIB}/paddle/lib/phi.dll ${PCOR_BIN_DIR}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${PADDLE_LIB}/third_party/install/mklml/lib/mklml.dll ${PCOR_BIN_DIR}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${PADDLE_LIB}/third_party/install/mklml/lib/libiomp5md.dll ${PCOR_BIN_DIR}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${PADDLE_LIB}/third_party/install/onednn/lib/mkldnn.dll ${PCOR_BIN_DIR}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${OpenCV_INSTALL_PATH}/x64/vc16/bin/opencv_world4110.dll ${PCOR_BIN_DIR}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_CURRENT_SOURCE_DIR}/third_party/pdfium/bin/pdfium.dll ${PCOR_BIN_DIR}
    )
endif()
