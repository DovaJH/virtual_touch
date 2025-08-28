// 📄 mouse_controller.h 파일의 올바른 내용

#pragma once
#include <X11/Xlib.h>

class MouseController {
public:
    MouseController();
    ~MouseController();

    bool initialize();
    void move(float x, float y);
    void press(unsigned int button);
    void release(unsigned int button);
    void click(unsigned int button);

    int get_screen_width() const;
    int get_screen_height() const;

private:
    Display* display_ = nullptr;
    Window root_window_;
    int screen_width_ = 0;
    int screen_height_ = 0;
};