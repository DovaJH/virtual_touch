#include "virtual_touch_app.h"
#include <iostream>
#include <memory>

int main(int argc, char** argv) {
    auto app = std::make_unique<VirtualTouchApp>();

    if (!app->setup()) {
        std::cerr << "Application setup failed!" << std::endl;
        return -1;
    }

    app->run();

    return 0;
}