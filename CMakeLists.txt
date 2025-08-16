cmake_minimum_required(VERSION 3.10)
project(VirtualTouchCpp)

# C++17 표준 사용
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Protobuf 패키지 찾기
find_package(Protobuf REQUIRED)

# 필요한 패키지 찾기
find_package(OpenCV REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_search_module(FFMPEG REQUIRED libavformat libavcodec libavutil libswscale libavdevice)

# libxdo 라이브러리 찾기
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

# C++ 코드로 변환할 .proto 파일 목록 정의
set(MEDIAPIPE_ROOT "/home/seo/virtual_touch/mediapipe")
set(PROTO_FILES
    "${MEDIAPIPE_ROOT}/mediapipe/framework/calculator.proto"
    "${MEDIAPIPE_ROOT}/mediapipe/framework/formats/landmark.proto"
    "${MEDIAPIPE_ROOT}/mediapipe/framework/calculator_options.proto"
    "${MEDIAPIPE_ROOT}/mediapipe/framework/mediapipe_options.proto"
    "${MEDIAPIPE_ROOT}/mediapipe/framework/packet_factory.proto"
    "${MEDIAPIPE_ROOT}/mediapipe/framework/packet_generator.proto"
    "${MEDIAPIPE_ROOT}/mediapipe/framework/status_handler.proto"
    "${MEDIAPIPE_ROOT}/mediapipe/framework/stream_handler.proto"
)

# 실행 파일 생성
add_executable(virtual_touch_app main.cpp)

# IMPORT_DIRS를 사용하여 protoc에 정확한 import 경로를 전달합니다. (핵심 수정 부분)
protobuf_generate(
    TARGET virtual_touch_app
    PROTOS ${PROTO_FILES}
    IMPORT_DIRS ${MEDIAPIPE_ROOT}
)

# 헤더 파일 경로 포함
target_include_directories(virtual_touch_app
    PRIVATE
    ${OpenCV_INCLUDE_DIRS}
    ${FFMPEG_INCLUDE_DIRS}
    ${CMAKE_CURRENT_BINARY_DIR}
    ${MEDIAPIPE_ROOT}
    ${XDO_INCLUDE_DIRS}
)

# 라이브러리 링크
target_link_libraries(virtual_touch_app
    PRIVATE
    ${OpenCV_LIBS}
    ${FFMPEG_LIBRARIES}
    XDO::xdo
    ${Protobuf_LIBRARIES}
    pthread
    GL
)