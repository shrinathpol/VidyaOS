#ifndef GRAPHICS_H
#define GRAPHICS_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdint.h>

// Initialise TrueType font system — call once after SDL_Init succeeds
bool init_fonts(SDL_Renderer *renderer);
void close_fonts();

// Main render entry point — draws the complete desktop frame directly to the renderer
void draw_desktop(SDL_Renderer *renderer, int screen_w, int screen_h);

// Accent color helper (also used by state.cpp)
uint32_t get_accent_color();

#endif // GRAPHICS_H
