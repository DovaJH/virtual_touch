# third_party/ffmpeg_linux.BUILD

licenses(["notice"])  # LGPL

exports_files(["LICENSE"])

cc_library(
    name = "libffmpeg",
    # --- 🔽🔽🔽 [수정] 누락된 라이브러리 2개 추가 🔽🔽🔽 ---
    linkopts = [
        "-l:libavcodec.so",
        "-l:libavformat.so",
        "-l:libavutil.so",
        "-l:libswscale.so",
        "-l:libavdevice.so",
    ],
    # --- 🔼🔼🔼 여기까지 수정 🔼🔼🔼 ---
    visibility = ["//visibility:public"],
)