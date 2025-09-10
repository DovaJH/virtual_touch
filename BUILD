load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library")
load("@org_tensorflow//tensorflow:tensorflow.bzl", "tf_copts")

# Protobuf를 강제로 링크하기 위한 라이브러리
cc_library(
    name = "force_link_protos",
    deps = [
        "//mediapipe/tasks/cc/vision/hand_landmarker/proto:hand_landmarker_graph_options_cc_proto",
    ],
    alwayslink = 1,
)

# Calculator를 강제로 링크하는 라이브러리
cc_library(
    name = "force_link_calculators",
    deps = [
        "//mediapipe/calculators/core:flow_limiter_calculator",
        "//mediapipe/tasks/cc/vision/hand_landmarker:hand_landmarker_graph",
    ],
    alwayslink = 1,
)

# 각 모듈을 별도의 라이브러리로 정의
cc_library(
    name = "mouse_controller_lib",
    srcs = ["mouse_controller.cpp"],
    hdrs = ["mouse_controller.h"],
    linkopts = ["-lX11", "-lXtst"],
)

cc_library(
    name = "gesture_controller_lib",
    srcs = ["gesture_controller.cpp"],
    hdrs = ["gesture_controller.h"],
    deps = [
        ":mouse_controller_lib",
        "//mediapipe/tasks/cc/components/containers:landmark",
    ],
)

cc_library(
    name = "webcam_manager_lib",
    srcs = ["webcam_manager.cpp"],
    hdrs = ["webcam_manager.h"],
    deps = [
        "@linux_opencv//:opencv",
        "@linux_ffmpeg//:libffmpeg",
    ],
)

cc_library(
    name = "virtual_touch_app_lib",
    srcs = ["virtual_touch_app.cpp"],
    hdrs = ["virtual_touch_app.h"],
    deps = [
        ":gesture_controller_lib",
        ":mouse_controller_lib",
        ":webcam_manager_lib",
        "@com_google_absl//absl/status",
        "//mediapipe/framework/formats:image",
        "//mediapipe/tasks/cc/vision/hand_landmarker:hand_landmarker",
        # --- ➕ GPU 지원을 위해 아래 두 줄을 추가하세요! ---
        "//mediapipe/gpu:gpu_buffer",
        "//mediapipe/gpu:gl_calculator_helper",
    ],
)

# 최종 실행 파일
cc_binary(
    name = "virtual_touch_app",
    srcs = ["main.cpp"],
    copts = tf_copts() + ["-fexceptions"],
    linkopts = [
        "-lEGL",
        "-lGLESv2",
        "-lGL",
    ],
    data = ["hand_landmarker.task"],
    deps = [
        ":force_link_calculators",
        ":force_link_protos",
        ":virtual_touch_app_lib",
        # --- ➕ GPU 컨텍스트 관리를 위해 아래 의존성을 추가하세요! ---
        "//mediapipe/gpu:gl_context",
    ],
)