/*  
🔥 이 코드는 Windows 환경에서 DirectShow, FFmpeg, OpenCV를 조합해 웹캠 영상을 실시간으로 캡처하고 표시하는 프로그램입니다.
🌟 주요 기능:
   - 시스템의 첫 번째 웹캠 장치 이름 자동 감지
   - FFmpeg을 이용한 고성능 영상 디코딩
   - OpenCV를 통한 실시간 영상 출력
   - 크로스 플랫폼 라이브러리 연동 (DirectShow + FFmpeg + OpenCV)
*/

// 🔄 멀티미디어 처리 관련 헤더
#include <iostream>
#include <dshow.h>          // DirectShow (카메라 장치 열거용)
#include <vector>
#include <opencv2/opencv.hpp>  // OpenCV (영상 표시용)
#include <string>

// 🔄 FFmpeg 라이브러리 (C 인터페이스 사용)
extern "C" {
    #include <libavdevice/avdevice.h>  // 장치 입력
    #include <libavformat/avformat.h>  // 미디어 포맷 처리
    #include <libavcodec/avcodec.h>    // 코덱 처리
    #include <libswscale/swscale.h>    // 이미지 스케일링
    #include <libavutil/imgutils.h>    // 이미지 유틸리티
}

// ⚙️ Windows COM 라이브러리 링크
#pragma comment(lib, "ole32.lib")      // COM 기본 라이브러리
#pragma comment(lib, "strmiids.lib")   // DirectShow
#pragma comment(lib, "oleaut32.lib")   // 자동화 지원

// ==================================================
// 🎥 웹캠 장치 열거 함수 (DirectShow 사용)
// ==================================================
char* get_first_webcam_name() {
    char* deviceName = nullptr;

    // 1️⃣ COM 라이브러리 초기화 (멀티스레드 환경)
    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(hr)) return nullptr;

    // 2️⃣ 장치 열거기 생성
    ICreateDevEnum* pDevEnum = nullptr;
    IEnumMoniker* pEnum = nullptr;
    hr = CoCreateInstance(
        CLSID_SystemDeviceEnum,
        NULL,
        CLSCTX_INPROC_SERVER,
        IID_ICreateDevEnum,
        (void**)&pDevEnum
    );

    // 3️⃣ 비디오 입력 장치 카테고리 열거
    if (SUCCEEDED(hr)) {
        hr = pDevEnum->CreateClassEnumerator(
            CLSID_VideoInputDeviceCategory, 
            &pEnum, 
            0
        );
    }

    // 4️⃣ 첫 번째 장치 정보 추출
    if (hr == S_OK) {
        IMoniker* pMoniker = nullptr;
        if (pEnum->Next(1, &pMoniker, NULL) == S_OK) {
            IPropertyBag* pPropBag = nullptr;
            hr = pMoniker->BindToStorage(
                0, 0, 
                IID_IPropertyBag, 
                (void**)&pPropBag
            );

            // 5️⃣ 장치 이름(UTF-8) 추출
            if (SUCCEEDED(hr)) {
                VARIANT varName;
                VariantInit(&varName);
                hr = pPropBag->Read(L"FriendlyName", &varName, 0);
                
                // 🔄 WideChar → UTF-8 변환
                if (SUCCEEDED(hr)) {
                    int len = WideCharToMultiByte(
                        CP_UTF8, 0,
                        varName.bstrVal, -1,
                        nullptr, 0,
                        nullptr, nullptr
                    );
                    deviceName = new char[len];
                    WideCharToMultiByte(
                        CP_UTF8, 0,
                        varName.bstrVal, -1,
                        deviceName, len,
                        nullptr, nullptr
                    );
                }
                VariantClear(&varName);
                pPropBag->Release();
            }
            pMoniker->Release();
        }
    }

    // 6️⃣ 자원 정리
    if (pEnum) pEnum->Release();
    if (pDevEnum) pDevEnum->Release();
    CoUninitialize();

    return deviceName;  // ⚠️ 호출 측에서 반드시 delete[] 해야 함
}

// ==================================================
// 📹 메인 함수 (FFmpeg + OpenCV 파이프라인)
// ==================================================
int main() {
    // 1️⃣ 시스템 설정 초기화
    SetConsoleOutputCP(CP_UTF8);  // UTF-8 콘솔 출력 활성화
    avdevice_register_all();      // FFmpeg 장치 등록

    // 2️⃣ 웹캠 이름 획득
    char* webcam_name = get_first_webcam_name();
    std::string dev_name_str = std::string("video=") + webcam_name;
    const char* dev_name = dev_name_str.c_str();

    // 3️⃣ FFmpeg 입력 컨텍스트 설정
    const AVInputFormat* inputFormat = av_find_input_format("dshow");
    AVFormatContext* fmt_ctx = nullptr;
    
    // 4️⃣ 웹캠 장치 열기
    if (avformat_open_input(&fmt_ctx, dev_name, inputFormat, nullptr) != 0) {
        std::cerr << "❌ 웹캠 연결 실패!" << std::endl;
        return -1;
    }

    // 5️⃣ 스트림 정보 분석
    if (avformat_find_stream_info(fmt_ctx, nullptr) < 0) {
        std::cerr << "⚠️ 스트림 정보 읽기 실패!" << std::endl;
        return -1;
    }

    // 6️⃣ 비디오 스트림 인덱스 찾기
    int video_stream_index = -1;
    for (unsigned int i = 0; i < fmt_ctx->nb_streams; i++) {
        if (fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            video_stream_index = i;
            break;
        }
    }

    // 7️⃣ 코덱 설정
    AVCodecParameters* codecpar = fmt_ctx->streams[video_stream_index]->codecpar;
    const AVCodec* codec = avcodec_find_decoder(codecpar->codec_id);
    AVCodecContext* codec_ctx = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(codec_ctx, codecpar);
    
    // 8️⃣ 코덱 오픈
    if (avcodec_open2(codec_ctx, codec, nullptr) < 0) {
        std::cerr << "⛔ 코덱 초기화 실패!" << std::endl;
        return -1;
    }

    // 9️⃣ FFmpeg 리소스 할당
    AVPacket* pkt = av_packet_alloc();
    AVFrame* frame = av_frame_alloc();
    cv::namedWindow("Webcam", cv::WINDOW_AUTOSIZE);  // OpenCV 창 생성

    // 🔄 색공간 변환기 설정 (YUV → BGR)
    SwsContext* sws_ctx = sws_getContext(
        codec_ctx->width, codec_ctx->height, codec_ctx->pix_fmt,
        codec_ctx->width, codec_ctx->height, AV_PIX_FMT_BGR24,
        SWS_BICUBIC, nullptr, nullptr, nullptr
    );

    // 🔄 OpenCV 호환 버퍼 준비
    int num_bytes = av_image_get_buffer_size(
        AV_PIX_FMT_BGR24, 
        codec_ctx->width, 
        codec_ctx->height, 
        1
    );
    std::vector<uint8_t> bgr_buffer(num_bytes);
    
    AVFrame* bgr_frame = av_frame_alloc();
    av_image_fill_arrays(
        bgr_frame->data, bgr_frame->linesize,
        bgr_buffer.data(), AV_PIX_FMT_BGR24,
        codec_ctx->width, codec_ctx->height, 1
    );

    // ▶️ 메인 루프
    std::cout << "🎬 영상 수신 시작..." << std::endl;
    int frame_count = 0;
    while (av_read_frame(fmt_ctx, pkt) >= 0) {
        if (pkt->stream_index == video_stream_index) {
            if (avcodec_send_packet(codec_ctx, pkt) == 0) {
                while (avcodec_receive_frame(codec_ctx, frame) == 0) {
                    // 🌈 색공간 변환
                    sws_scale(
                        sws_ctx,
                        frame->data, frame->linesize,
                        0, codec_ctx->height,
                        bgr_frame->data, bgr_frame->linesize
                    );

                    // 🖼️ OpenCV로 출력
                    cv::Mat img(
                        codec_ctx->height, codec_ctx->width, 
                        CV_8UC3, 
                        bgr_buffer.data(), 
                        bgr_frame->linesize[0]
                    );
                    cv::imshow("Webcam", img);

                    // 🔚 ESC 키 종료 처리
                    if (cv::waitKey(1) == 27) goto end;
                    std::cout << "📸 프레임 #" << ++frame_count << " 처리 완료" << std::endl;
                }
            }
        }
        av_packet_unref(pkt);
    }

end:
    // 🧹 자원 정리
    av_frame_free(&bgr_frame);
    av_frame_free(&frame);
    av_packet_free(&pkt);
    sws_freeContext(sws_ctx);
    avcodec_free_context(&codec_ctx);
    avformat_close_input(&fmt_ctx);
    cv::destroyAllWindows();
    
    std::cout << "🛑 프로그램 종료" << std::endl;
    delete[] webcam_name;  // 웹캠 이름 메모리 해제
    return 0;
}
