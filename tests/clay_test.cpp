#define CLAY_IMPLEMENTATION
#include "clay.h"

#include <SDL3/SDL.h>
#include <iostream>
#include <vector>

static Clay_Dimensions MeasureText(Clay_StringSlice text, Clay_TextElementConfig* config, void* userData) {
    (void)userData;
    const float fontSize = config ? static_cast<float>(config->fontSize) : 16.0f;
    const float width = static_cast<float>(text.length) * (fontSize * 0.6f);
    return Clay_Dimensions{ width, fontSize };
}

static void ClayErrorHandler(Clay_ErrorData errorData) {
    std::cerr << "Clay error: ";
    if (errorData.errorText.chars && errorData.errorText.length > 0) {
        std::cerr.write(errorData.errorText.chars, errorData.errorText.length);
    }
    std::cerr << std::endl;
}

int main(int argc, char* argv[]) {
    std::cout << "Clay UI Test - Initializing SDL3..." << std::endl;

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::cerr << "Failed to initialize SDL3: " << SDL_GetError() << std::endl;
        return 1;
    }

    // Create a simple SDL window
    SDL_Window* window = SDL_CreateWindow(
        "Clay UI Test",
        800,
        600,
        SDL_WINDOW_RESIZABLE
    );

    if (!window) {
        std::cerr << "Failed to create SDL window: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return 1;
    }

    std::cout << "SDL3 window created successfully!" << std::endl;

    // Initialize Clay
    uint32_t clay_memory_size = Clay_MinMemorySize();
    std::vector<char> clay_memory(clay_memory_size);
    Clay_Arena clay_arena = Clay_CreateArenaWithCapacityAndMemory(clay_memory_size, clay_memory.data());
    Clay_ErrorHandler error_handler{ ClayErrorHandler, nullptr };
    Clay_Initialize(clay_arena, Clay_Dimensions{ 800.0f, 600.0f }, error_handler);
    Clay_SetMeasureTextFunction(MeasureText, nullptr);

    std::cout << "Clay UI initialized successfully!" << std::endl;

    // Simple test loop
    bool running = true;
    int frame_count = 0;
    const int test_frames = 60; // Run for 60 frames

    while (running && frame_count < test_frames) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_EVENT_QUIT:
                    running = false;
                    break;
                case SDL_EVENT_KEY_DOWN:
                    if (event.key.key == SDLK_ESCAPE) {
                        running = false;
                    }
                    break;
            }
        }

        // Clear Clay memory for this frame
        Clay_SetPointerState(Clay_Vector2{ 0.0f, 0.0f }, false);

        // Begin layout
        Clay_SetLayoutDimensions(Clay_Dimensions{ 800.0f, 600.0f });
        Clay_BeginLayout();

        CLAY(CLAY_ID("Root"),
            {
                .layout = {
                    .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
                    .padding = { 16, 16, 16, 16 },
                    .childGap = 8,
                    .layoutDirection = CLAY_TOP_TO_BOTTOM
                },
                .backgroundColor = { 30, 30, 30, 255 }
            }) {
            CLAY_TEXT(CLAY_STRING("Clay UI Test"), CLAY_TEXT_CONFIG({ .userData = nullptr, .textColor = { 255, 255, 255, 255 }, .fontId = 0, .fontSize = 18 }));
            CLAY_TEXT(CLAY_STRING("Running layout frame..."), CLAY_TEXT_CONFIG({ .userData = nullptr, .textColor = { 200, 200, 200, 255 }, .fontId = 0, .fontSize = 14 }));
        }

        Clay_RenderCommandArray commands = Clay_EndLayout(1.0f / 60.0f);

        // Frame complete
        frame_count++;
        std::cout << "Frame " << frame_count << " rendered, commands: " << commands.length << std::endl;

        // Small delay to avoid spinning
        SDL_Delay(16); // ~60 FPS
    }

    std::cout << "Clay UI Test completed successfully!" << std::endl;

    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
