#include <raylib.h>
#include <raygui.h>
#include <dark/style_dark.h>
#include <rlFPCamera.h>
#include <tinyfiledialogs.h>
#include "types.hpp"
#include "Scene.hpp"
#include "logger.hpp"

Color intToColor(int col) {
    return {(unsigned char)(col >> 24), (unsigned char)(col >> 16 & 0xFF), (unsigned char)(col >> 8 & 0xFF),
            (unsigned char)(col & 0xFF)};
}

void rlLogCallback(int logLevel, const char *text, va_list args) {
    char buffer[1024];
    vsnprintf(buffer, 1024, text, args);
    if (logLevel <= LOG_INFO) {
        logD("[RL] {}", buffer);
    } else if (logLevel == LOG_WARNING) {
        logW("[RL] {}", buffer);
    } else if (logLevel == LOG_ERROR || logLevel == LOG_FATAL) {
        logE("[RL] {}", buffer);
    }
}

int main() {
    // SetTraceLogLevel(LOG_INFO);
    SetTraceLogLevel(LOG_WARNING);
    SetTraceLogCallback(rlLogCallback);
    constexpr auto winSize = Vec2u {1280, 720};
    SetConfigFlags(FLAG_WINDOW_HIGHDPI | FLAG_MSAA_4X_HINT);
    // SetConfigFlags(FLAG_WINDOW_HIGHDPI);
    InitWindow(winSize.x, winSize.y, "NuExplorer");

    GuiLoadStyleDark();

    SetTargetFPS(60);

    rlFPCamera cam;
    cam.MoveSpeed = {24, 24, 24};
    cam.NearPlane = 0.005;
    cam.FarPlane = 1000000.0;
    cam.Setup(50.f, {0, 0, 0});
    std::string camSpeed = fmt::format("Cam speed: {}", cam.MoveSpeed.x);

    Scene scene;

    bool sceneLoaded = false;

    auto borderColor = intToColor(GuiGetStyle(DEFAULT, BORDER_COLOR_NORMAL));
    auto mainColor = intToColor(GuiGetStyle(DEFAULT, BASE_COLOR_NORMAL));

    while (!WindowShouldClose()) {
        auto dt = GetMouseWheelMove() * 3.f;
        if (dt != 0) {
            cam.MoveSpeed.x = std::max(0.f, cam.MoveSpeed.x + dt);
            cam.MoveSpeed.y = std::max(0.f, cam.MoveSpeed.y + dt);
            cam.MoveSpeed.z = std::max(0.f, cam.MoveSpeed.z + dt);
            camSpeed = fmt::format("Cam speed: {}", cam.MoveSpeed.x);
        }

        if (IsKeyPressed(KEY_O)) {
            logD("Open key pressed");
            const char* filterPatterns[] = {"*.gsc"};
            auto name = tinyfd_openFileDialog("Select a scene file (tested on LLOTR only!)", nullptr, 1, filterPatterns,
                                              "Game scene files (.gsc)", false);
            if (name) {
                sceneLoaded = true;
                logD("Selected file {}", name);
                scene.load(name);
                cam.SetCameraPosition({0, 0, 0});
            }
        }

        BeginDrawing();

        cam.Update();

        cam.BeginMode3D();

        // ClearBackground({0, 0, 0, 255});
        ClearBackground({32, 32, 32, 255});

        DrawGrid(30, 1);
        DrawRay({{0, 0, 0}, {1, 0, 0}}, RED);   // x
        DrawRay({{0, 0, 0}, {0, 1, 0}}, GREEN); // y
        DrawRay({{0, 0, 0}, {0, 0, 1}}, BLUE);  // z

        if (sceneLoaded)
            scene.render();

        cam.EndMode3D();

        // GuiDrawRectangle({0, 0, 1280,40}, 2, borderColor, mainColor);
        // GuiDrawRectangle({0, 38, 1280, 682}, 2, borderColor, BLANK);

        // DrawFPS(0, 0);
        DrawText(fmt::format("{} FPS", GetFPS()).c_str(), 0, 0, 20, GREEN);
        DrawText(camSpeed.c_str(), 0, 20, 20, GREEN);
        auto camPos = cam.GetCameraPosition();
        DrawText(fmt::format("Cam pos: {} {} {}", camPos.x, camPos.y, camPos.z).c_str(), 0, 40, 20, GREEN);

        EndDrawing();
    }

    CloseWindow();

    return 0;
}