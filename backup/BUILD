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
        "//mediapipe/tasks/cc/vision/hand_landmarker:hand_landmarker",  # <--- 최종 수정된 부분
    ],
    alwayslink = 1,
)

cc_binary(
    name = "virtual_touch_app",
    srcs = ["main.cpp"],
    copts = tf_copts() + ["-fexceptions"],
    linkopts = [
        "-lEGL",
        "-lGLESv2",
        "-lGL",
        "-lX11",
        "-lXtst",
    ],
    data = ["hand_landmarker.task"],
    deps = [
        ":force_link_protos",
        ":force_link_calculators",
        "//mediapipe/tasks/cc/vision/hand_landmarker:hand_landmarker",
        "//mediapipe/framework:calculator_framework",
        "//mediapipe/framework/formats:image_frame",
        "//mediapipe/framework/formats:image_frame_opencv",
        "//mediapipe/framework/formats:landmark_cc_proto",
        "//mediapipe/gpu:gl_calculator_helper",
        "//mediapipe/gpu:gpu_buffer",
        "//mediapipe/gpu:gpu_shared_data_internal",
        "@com_google_absl//absl/memory",
        "@linux_opencv//:opencv",
        "@linux_ffmpeg//:libffmpeg",
    ],
)