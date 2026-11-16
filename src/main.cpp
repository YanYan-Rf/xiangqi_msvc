#include "XiangqiUI.hpp"
#include "raylib.h"

int main() {
    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(XiangqiUI::WINDOW_WIDTH, XiangqiUI::WINDOW_HEIGHT,
               "Chinese Chess - MSVC + vcpkg + SQLite + CMake");
    SetTargetFPS(60);

    XiangqiUI ui;
    while (!WindowShouldClose()) {
        ui.Update();
        ui.Draw();
    }

    CloseWindow();
    return 0;
}