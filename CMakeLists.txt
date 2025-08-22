cmake_minimum_required(VERSION 3.10)
project(VirtualTouchCpp)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ----------------------------------------------------------------------
# 1. Conda 환경 연동 설정 제거
# ----------------------------------------------------------------------
# 시스템에 직접 설치된 라이브러리를 사용하기 위해 Conda 경로 설정을 주석 처리합니다.
# CMake는 이제 /usr/lib, /usr/include 등 표준 시스템 경로에서 패키지를 찾게 됩니다.
# set(CMAKE_PREFIX_PATH $ENV{CONDA_PREFIX})
# list(APPEND CMAKE_FIND_ROOT_PATH $ENV{CONDA_PREFIX})

# ----------------------------------------------------------------------
# 2. 패키지 찾기 (시스템에서 자동으로)
# ----------------------------------------------------------------------
find_package(Protobuf REQUIRED)
find_package(OpenCV REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_search_module(FFMPEG REQUIRED libavformat libavcodec libavutil libswscale libavdevice)
find_package(glog REQUIRED)
find_package(absl REQUIRED)
find_package(Threads REQUIRED)
pkg_search_module(EGL REQUIRED egl)
pkg_search_module(GLES2 REQUIRED glesv2)

# libxdo 라이브러리 찾기 (시스템에 설치된 것을 사용)
find_path(XDO_INCLUDE_DIRS NAMES xdo.h)
find_library(XDO_LIBRARIES NAMES xdo)
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(XDO DEFAULT_MSG XDO_LIBRARIES XDO_INCLUDE_DIRS)
if(XDO_FOUND AND NOT TARGET XDO::xdo)
    add_library(XDO::xdo UNKNOWN IMPORTED)
    set_target_properties(XDO::xdo PROPERTIES
      IMPORTED_LOCATION "${XDO_LIBRARIES}"
      INTERFACE_INCLUDE_DIRECTORIES "${XDO_INCLUDE_DIRS}")
endif()

# ----------------------------------------------------------------------
# 3. Protobuf 소스 생성 규칙
# ----------------------------------------------------------------------
# ⚠️ 주의: 이 경로는 사용자의 환경에 맞게 수정해야 할 수 있습니다.
set(MEDIAPIPE_ROOT "/home/seo/virtual_touch/mediapipe")

set(PROTO_FILES
    "${MEDIAPIPE_ROOT}/mediapipe/framework/calculator.proto"
    "${MEDIAPIPE_ROOT}/mediapipe/framework/calculator_options.proto"
    "${MEDIAPIPE_ROOT}/mediapipe/framework/deps/proto_descriptor.proto"
    "${MEDIAPIPE_ROOT}/mediapipe/framework/formats/image_format.proto"
    "${MEDIAPIPE_ROOT}/mediapipe/framework/formats/landmark.proto"
    "${MEDIAPIPE_ROOT}/mediapipe/framework/formats/rect.proto"
    "${MEDIAPIPE_ROOT}/mediapipe/framework/formats/detection.proto"
    "${MEDIAPIPE_ROOT}/mediapipe/framework/formats/location_data.proto"
     "${MEDIAPIPE_ROOT}/mediapipe/framework/formats/annotation/rasterization.proto"
    "${MEDIAPIPE_ROOT}/mediapipe/framework/graph_runtime_info.proto"
    "${MEDIAPIPE_ROOT}/mediapipe/framework/mediapipe_options.proto"
    "${MEDIAPIPE_ROOT}/mediapipe/framework/packet_factory.proto"
    "${MEDIAPIPE_ROOT}/mediapipe/framework/packet_generator.proto"
    "${MEDIAPIPE_ROOT}/mediapipe/framework/status_handler.proto"
    "${MEDIAPIPE_ROOT}/mediapipe/framework/stream_handler.proto"
    "${MEDIAPIPE_ROOT}/mediapipe/framework/thread_pool_executor.proto"
    "${MEDIAPIPE_ROOT}/mediapipe/framework/tool/calculator_graph_template.proto"
    "${MEDIAPIPE_ROOT}/mediapipe/calculators/core/flow_limiter_calculator.proto"
    "${MEDIAPIPE_ROOT}/mediapipe/gpu/copy_calculator.proto"
    "${MEDIAPIPE_ROOT}/mediapipe/gpu/gpu_origin.proto"
)
set(PROTO_SRCS "")
set(PROTO_HDRS "")
foreach(PROTO_FILE ${PROTO_FILES})
    file(RELATIVE_PATH PROTO_FILE_REL ${MEDIAPIPE_ROOT} ${PROTO_FILE})
    string(REPLACE ".proto" ".pb.cc" PROTO_CC ${PROTO_FILE_REL})
    string(REPLACE ".proto" ".pb.h" PROTO_H ${PROTO_FILE_REL})
    set(PROTO_CC_OUT "${CMAKE_CURRENT_BINARY_DIR}/${PROTO_CC}")
    set(PROTO_H_OUT "${CMAKE_CURRENT_BINARY_DIR}/${PROTO_H}")
    add_custom_command(
        OUTPUT ${PROTO_CC_OUT} ${PROTO_H_OUT} 
        COMMAND ${Protobuf_PROTOC_EXECUTABLE}
            --cpp_out=${CMAKE_CURRENT_BINARY_DIR}
            -I=${MEDIAPIPE_ROOT}
            ${PROTO_FILE}
        DEPENDS ${PROTO_FILE}
        COMMENT "Generating C++ sources from ${PROTO_FILE_REL}"
    )
    list(APPEND PROTO_SRCS ${PROTO_CC_OUT})
    list(APPEND PROTO_HDRS ${PROTO_H_OUT})
endforeach()
add_custom_target(generate_proto_files DEPENDS ${PROTO_SRCS} ${PROTO_HDRS})

# ----------------------------------------------------------------------
# 4. 최종 실행 파일 설정
# ----------------------------------------------------------------------

# bazel-bin 경로에서 'libcalculator_framework.a' 파일을 찾습니다.
find_library(MEDIAPIPE_FRAMEWORK_LIB
    NAMES calculator_framework
    PATHS "/home/seo/.cache/bazel/_bazel_seo/073f4c24b4b82f694ceb0db2d3f6eace/execroot/mediapipe/bazel-out/k8-opt/bin/mediapipe/framework" # <--- fastbuild를 opt로 수정
    NO_DEFAULT_PATH
)


if(NOT MEDIAPIPE_FRAMEWORK_LIB)
    message(FATAL_ERROR "MediaPipe framework 라이브러리(libcalculator_framework.a)를 찾을 수 없습니다.")
endif()


add_executable(virtual_touch_app main.cpp ${PROTO_SRCS})
add_dependencies(virtual_touch_app generate_proto_files)

target_include_directories(virtual_touch_app
    PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}
    ${Protobuf_INCLUDE_DIRS}
    ${OpenCV_INCLUDE_DIRS}
    ${FFMPEG_INCLUDE_DIRS}
    # absl 헤더 경로를 명시적으로 추가합니다.
    /usr/include
    "${MEDIAPIPE_ROOT}/bazel-mediapipe/external/com_google_absl"
    ${MEDIAPIPE_ROOT}
    ${XDO_INCLUDE_DIRS}
    ${EGL_INCLUDE_DIRS}
    ${GLES2_INCLUDE_DIRS}
)

target_link_libraries(virtual_touch_app
    PRIVATE
    # 1. MediaPipe 프레임워크 라이브러리 연결
    ${MEDIAPIPE_FRAMEWORK_LIB}

    # 2. MediaPipe가 의존하는 핵심 라이브러리들 연결
    glog::glog
    absl_base absl_strings absl_status absl_statusor absl_log

    # 3. Protobuf와 OpenCV
    ${Protobuf_LIBRARIES}
    ${OpenCV_LIBS}

    # 4. FFmpeg 라이브러리
    avformat avcodec swscale avutil avdevice
    
    # 5. XDO 라이브러리
    xdo

    # 6. 기타 시스템 라이브러리
    Threads::Threads
    ${EGL_LIBRARIES}
    ${GLES2_LIBRARIES}
    GL
)

