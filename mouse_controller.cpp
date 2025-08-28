// 📄 mouse_controller.cpp 파일의 올바른 내용

#include "mouse_controller.h"
#include <X11/extensions/XTest.h>
#include <iostream>

MouseController::MouseController() = default;

MouseController::~MouseController() {
    if (display_) {
        XCloseDisplay(display_);
    }
}

bool MouseController::initialize() {
    display_ = XOpenDisplay(NULL);
    if (!display_) {
        std::cerr << "⛔ X 서버에 연결할 수 없습니다!" << std::endl;
        return false;
    }
    root_window_ = DefaultRootWindow(display_);

    Window dummy_win;
    int dummy_int;
    unsigned int dummy_uint;
    XGetGeometry(display_, root_window_, &dummy_win, &dummy_int, &dummy_int,
                 (unsigned int*)&screen_width_, (unsigned int*)&screen_height_,
                 &dummy_uint, &dummy_uint);
    return true;
}

void MouseController::move(float x, float y) {
    if (!display_) return;
    XWarpPointer(display_, None, root_window_, 0, 0, 0, 0, static_cast<int>(x), static_cast<int>(y));
    XFlush(display_);
}

void MouseController::press(unsigned int button) {
    if (!display_) return;
    XTestFakeButtonEvent(display_, button, True, CurrentTime);
    XFlush(display_);
}

void MouseController::release(unsigned int button) {
    if (!display_) return;
    XTestFakeButtonEvent(display_, button, False, CurrentTime);
    XFlush(display_);
}

void MouseController::click(unsigned int button) {
    if (!display_) return;
    press(button);
    release(button);
}

int MouseController::get_screen_width() const {
    return screen_width_;
}

int MouseController::get_screen_height() const {
    return screen_height_;
}