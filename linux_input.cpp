#include <iostream>
#include <vector>
#include <opencv2/opencv.hpp>

extern "C" {
    #include <libavdevice/avdevice.h>
    #include <libavformat/avformat.h>
    #include <libavcodec/avcodec.h>
    #include <libswscale/swscale.h>
    #include <libavutil/imgutils.h>
}

int main() {
    avdevice_register_all();  // FFmpeg 장치 등록

    // 📷 v4l2 입력 장치 설정 (/dev/video0)
    const char* dev_name = "/dev/video0";


    const AVInputFormat* inputFormat = av_find_input_format("v4l2");
    AVFormatContext* fmt_ctx = nullptr;

    AVDictionary* options = nullptr;
    av_dict_set(&options, "input_format", "mjpeg", 0);  // 또는 mjpeg
    av_dict_set(&options, "video_size", "640x480", 0);
    av_dict_set(&options, "framerate", "30", 0);

    std::cout << "1" << std::endl;
    if (avformat_open_input(&fmt_ctx, dev_name, inputFormat, &options) != 0) {
        std::cerr << "❌ 웹캠 연결 실패!" << std::endl;
        return -1;
    }

    std::cout << "2" << std::endl;
    if (avformat_find_stream_info(fmt_ctx, nullptr) < 0) {
        std::cerr << "⚠️ 스트림 정보 읽기 실패!" << std::endl;
        return -1;
    }

    std::cout << "3" << std::endl;
    int video_stream_index = -1;
    for (unsigned int i = 0; i < fmt_ctx->nb_streams; i++) {
        if (fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            video_stream_index = i;
            break;
        }
    }

    AVCodecParameters* codecpar = fmt_ctx->streams[video_stream_index]->codecpar;
    const AVCodec* codec = avcodec_find_decoder(codecpar->codec_id);
    AVCodecContext* codec_ctx = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(codec_ctx, codecpar);

    if (avcodec_open2(codec_ctx, codec, nullptr) < 0) {
        std::cerr << "⛔ 코덱 초기화 실패!" << std::endl;
        return -1;
    }

    AVPacket* pkt = av_packet_alloc();
    AVFrame* frame = av_frame_alloc();
    SwsContext* sws_ctx = sws_getContext(
        codec_ctx->width, codec_ctx->height, codec_ctx->pix_fmt,
        codec_ctx->width, codec_ctx->height, AV_PIX_FMT_BGR24,
        SWS_BICUBIC, nullptr, nullptr, nullptr
    );

    int num_bytes = av_image_get_buffer_size(AV_PIX_FMT_BGR24, codec_ctx->width, codec_ctx->height, 1);
    std::vector<uint8_t> bgr_buffer(num_bytes);
    AVFrame* bgr_frame = av_frame_alloc();
    av_image_fill_arrays(
        bgr_frame->data, bgr_frame->linesize,
        bgr_buffer.data(), AV_PIX_FMT_BGR24,
        codec_ctx->width, codec_ctx->height, 1
    );

    cv::namedWindow("Webcam", cv::WINDOW_AUTOSIZE);
    int frame_count = 0;

    std::cout << "🎬 영상 수신 시작..." << std::endl;

    while (av_read_frame(fmt_ctx, pkt) >= 0) {
        if (pkt->stream_index == video_stream_index) {
            if (avcodec_send_packet(codec_ctx, pkt) == 0) {
                /*
                while (avcodec_receive_frame(codec_ctx, frame) == 0) {
                    sws_scale(
                        sws_ctx,
                        frame->data, frame->linesize,
                        0, codec_ctx->height,
                        bgr_frame->data, bgr_frame->linesize
                    );

                    cv::Mat img(
                        codec_ctx->height, codec_ctx->width,
                        CV_8UC3, bgr_buffer.data(), bgr_frame->linesize[0]
                    );
                    cv::imshow("Webcam", img);
                    if (cv::waitKey(1) == 27) goto end;
                    std::cout << "📸 프레임 #" << ++frame_count << " 처리 완료" << std::endl;
                }
                */
                while (avcodec_receive_frame(codec_ctx, frame) == 0) {
                    AVPixelFormat src_pix_fmt = static_cast<AVPixelFormat>(frame->format);
                    if (src_pix_fmt == AV_PIX_FMT_YUVJ420P) src_pix_fmt = AV_PIX_FMT_YUV420P;
                    else if (src_pix_fmt == AV_PIX_FMT_YUVJ422P) src_pix_fmt = AV_PIX_FMT_YUV422P;
                    else if (src_pix_fmt == AV_PIX_FMT_YUVJ444P) src_pix_fmt = AV_PIX_FMT_YUV444P;

                    if (frame->color_range == AVCOL_RANGE_JPEG)
                        frame->color_range = AVCOL_RANGE_MPEG;

                    // sws_ctx를 재생성하거나 미리 만들어둔 sws_ctx를 해제 후 다시 생성
                    if (sws_ctx) {
                        sws_freeContext(sws_ctx);
                    }
                    sws_ctx = sws_getContext(
                        codec_ctx->width, codec_ctx->height, src_pix_fmt,
                        codec_ctx->width, codec_ctx->height, AV_PIX_FMT_BGR24,
                        SWS_BICUBIC, nullptr, nullptr, nullptr
                    );

                    sws_scale(
                        sws_ctx,
                        frame->data, frame->linesize,
                        0, codec_ctx->height,
                        bgr_frame->data, bgr_frame->linesize
                    );

                    // OpenCV Mat 생성 및 화면 출력 (height, width 순서 주의)
                    cv::Mat img(codec_ctx->height, codec_ctx->width, CV_8UC3, bgr_buffer.data(), bgr_frame->linesize[0]);
                    cv::imshow("Webcam", img);

                    if (cv::waitKey(1) == 27) break;
                }
            }
        }
        av_packet_unref(pkt);
    }

end:
    av_frame_free(&bgr_frame);
    av_frame_free(&frame);
    av_packet_free(&pkt);
    sws_freeContext(sws_ctx);
    avcodec_free_context(&codec_ctx);
    avformat_close_input(&fmt_ctx);
    cv::destroyAllWindows();
    std::cout << "🛑 프로그램 종료" << std::endl;
    return 0;
}