#include <SDL.h>
#include <cstdio>

int SDL_main(int argc, char* argv[]) {
    SDL_SetMainReady();
    
    // Initialize SDL video and event subsystems
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
        std::fprintf(stderr, "SDL_Init error: %s\n", SDL_GetError());
        return 1;
    }

    // Create a window; portrait-ish size is convenient for mobile testing
    SDL_Window* window = SDL_CreateWindow(
        "match_three",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        720, 1280,
        SDL_WINDOW_SHOWN
    );
    if (!window) {
        std::fprintf(stderr, "SDL_CreateWindow error: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    // Create a hardware-accelerated renderer with vsync
    SDL_Renderer* renderer = SDL_CreateRenderer(
        window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );
    if (!renderer) {
        std::fprintf(stderr, "SDL_CreateRenderer error: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    bool running = true;
    while (running) {
        // Process OS/window events
        SDL_Event e {};
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                running = false;
            }
            if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) {
                running = false;
            }
        }

        // Clear backbuffer to black
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        // Compute centered rectangle
        const int w = 200;
        const int h = 200;
        int win_w = 0;
        int win_h = 0;
        SDL_GetWindowSize(window, &win_w, &win_h);
        SDL_Rect rect {
            (win_w - w) / 2,
            (win_h - h) / 2,
            w,
            h
        };

        // Fill with solid red
        SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
        SDL_RenderFillRect(renderer, &rect);

        // Present backbuffer to the screen
        SDL_RenderPresent(renderer);
    }

    // Clean up resources
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
