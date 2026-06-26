// =============================================================================
//  graphics.cpp — VidyaOS Modern Desktop UI
//  Renders a macOS / Windows-11-inspired desktop using SDL2 + SDL_TTF
//  All coordinates are in native screen pixels (1:1 mapping, no virtual canvas).
// =============================================================================
#include "graphics.h"
#include "state.h"
#include "footprint.h"
#include "font.h"
#include "browser.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <vterm.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <map>
#include <sstream>
#include <string>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
//  A. COLOR PALETTE  (0xRRGGBBAA)
// ─────────────────────────────────────────────────────────────────────────────
#define C_BG_TOP      0x0F1117FF   // Desktop wallpaper top: #0F1117
#define C_BG_BOT      0x1A1F2EFF   // Desktop wallpaper bottom: #1A1F2E
#define C_GLASS       0x1A1F2CD8   // frosted glass panel
#define C_TITLEBAR    0x141828F2   // Titlebar: rgba(20,24,40, 0.95)
#define C_TITLEBAR2   0x141828F2   // Titlebar: rgba(20,24,40, 0.95)
#define C_TASKBAR     0x0F121CF5   // Taskbar: rgba(15,18,28, 0.96)
#define C_WIN_BG      0x161820FF   // window content background
#define C_SIDEBAR     0x1C1F28FF   // sidebar background
#define C_INPUT_BG    0x0D1015FF   // input field background

#define C_ACCENT      0x0A84FFFF   // primary accent — macOS blue: #0A84FF
#define C_ACCENT_DIM  0x0A84FF40   // accent at low opacity
#define C_GREEN       0x30D158FF   // success / grow button
#define C_RED_ERR     0xFF453AFF   // error
#define C_AMBER       0xFFD60AFF   // warning

#define C_BTN_CLOSE   0xFF5F57FF   // macOS close red
#define C_BTN_MIN     0xFEBC2EFF   // macOS minimize amber
#define C_BTN_MAX     0x28C840FF   // macOS maximize green
#define C_BTN_IDLE    0x3A3A3CFF   // unfocused button

#define C_TEXT_PRI    0xE8EAEDFF   // Primary text: #E8EAED
#define C_TEXT_SEC    0x9BA3B8FF   // Secondary text: #9BA3B8
#define C_TEXT_TER    0x636366FF   // tertiary gray
#define C_TEXT_LINK   0x0A84FFFF   // link blue
#define C_BORDER      0x3A3A3EFF   // subtle border
#define C_BORDER_LT   0x54545870   // lighter translucent border
#define C_SELECTED    0x0A84FF30   // selection highlight
#define C_HOVER       0xFFFFFF10   // hover overlay

// Traffic-light button geometry (macOS left-side style)
#define BTN_CLOSE_CX  18           // close button X centre (relative to wx)
#define BTN_MIN_CX    42           // min button X centre
#define BTN_MAX_CX    66           // max button X centre
#define BTN_CY_OFF    (TITLEBAR_H/2) // button Y centre offset from wy
#define BTN_R         7            // button radius px

#define WIN_RAD       12           // window corner radius

// ─────────────────────────────────────────────────────────────────────────────
//  B. FONT SYSTEM
// ─────────────────────────────────────────────────────────────────────────────
static TTF_Font *fXS   = nullptr;  // 11 px — tiny status text
static TTF_Font *fSM   = nullptr;  // 13 px — secondary labels
static TTF_Font *fMD   = nullptr;  // 15 px — body / menu text
static TTF_Font *fLG   = nullptr;  // 18 px — window titles / headings
static TTF_Font *fXL   = nullptr;  // 24 px — section headers
static TTF_Font *fXXL  = nullptr;  // 36 px — display (login)
static TTF_Font *fMono = nullptr;  // 13 px monospace — terminal
static SDL_Texture *g_font_textures[5][96];
static bool g_fonts_ok = false;

static const char *SANS[] = {
    "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
    "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
    "/usr/share/fonts/truetype/ubuntu/Ubuntu-R.ttf",
    "/usr/share/fonts/truetype/freefont/FreeSans.ttf",
    nullptr
};
static const char *MONO[] = {
    "/usr/share/fonts/truetype/liberation/LiberationMono-Regular.ttf",
    "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
    "/usr/share/fonts/truetype/ubuntu/UbuntuMono-R.ttf",
    nullptr
};

static SDL_Texture* create_font_char_texture(SDL_Renderer* r, char c, int size) {
    int s = size;
    SDL_Surface* surf = SDL_CreateRGBSurfaceWithFormat(0, 8 * s, 8 * s, 32, SDL_PIXELFORMAT_RGBA8888);
    if (!surf) return nullptr;
    
    SDL_FillRect(surf, nullptr, 0);
    
    uint32_t* pixels = (uint32_t*)surf->pixels;
    int pitch = surf->pitch / 4;
    
    int idx = (int)c - 32;
    if (idx < 0 || idx >= 96) idx = 0;
    
    float R = s * 0.72f;
    
    for (int py = 0; py < 8 * s; ++py) {
        for (int px = 0; px < 8 * s; ++px) {
            float max_alpha = 0.0f;
            for (int by = 0; by < 8; ++by) {
                uint8_t row_byte = font8x8[idx][by];
                for (int bx = 0; bx < 8; ++bx) {
                    bool active = (row_byte >> bx) & 1;
                    if (active) {
                        float cx = bx * s + s / 2.0f;
                        float cy = by * s + s / 2.0f;
                        float dx = px + 0.5f - cx;
                        float dy = py + 0.5f - cy;
                        float dist = sqrtf(dx*dx + dy*dy);
                        
                        float alpha = 0.0f;
                        if (dist <= R) {
                            alpha = 1.0f;
                        } else if (dist < R + 1.0f) {
                            alpha = 1.0f - (dist - R);
                        }
                        if (alpha > max_alpha) {
                            max_alpha = alpha;
                        }
                    }
                }
            }
            
            if (max_alpha > 0.01f) {
                uint8_t a = (uint8_t)(max_alpha * 255);
                pixels[py * pitch + px] = (0xFFFFFF00) | a;
            }
        }
    }
    
    SDL_Texture* tex = SDL_CreateTextureFromSurface(r, surf);
    SDL_FreeSurface(surf);
    return tex;
}

bool init_fonts(SDL_Renderer *renderer) {
    if (TTF_Init() < 0) { fprintf(stderr, "[TTF] %s\n", TTF_GetError()); return false; }
    const char *sp = nullptr;
    for (int i = 0; SANS[i]; ++i) {
        if (FILE *f = fopen(SANS[i], "r")) { fclose(f); sp = SANS[i]; break; }
    }
    if (!sp) { fprintf(stderr, "[TTF] No sans-serif font found on this system.\n"); return false; }

    fXS  = TTF_OpenFont(sp, 11);
    fSM  = TTF_OpenFont(sp, 13);
    fMD  = TTF_OpenFont(sp, 15);
    fLG  = TTF_OpenFont(sp, 18);
    fXL  = TTF_OpenFont(sp, 24);
    fXXL = TTF_OpenFont(sp, 36);

    const char *mp = nullptr;
    for (int i = 0; MONO[i]; ++i) {
        if (FILE *f = fopen(MONO[i], "r")) { fclose(f); mp = MONO[i]; break; }
    }
    fMono = mp ? TTF_OpenFont(mp, 13) : fSM;
    g_fonts_ok = (fMD != nullptr);
    printf("[VidyaOS] UI font: %s\n[VidyaOS] Mono  : %s\n", sp, mp ? mp : "(fallback)");

    // Initialize custom font textures
    if (renderer) {
        for (int size = 1; size <= 4; ++size) {
            for (int c = 32; c <= 126; ++c) {
                g_font_textures[size][c - 32] = create_font_char_texture(renderer, (char)c, size);
            }
        }
    }

    return g_fonts_ok;
}

void close_fonts() {
    if (fXS)  TTF_CloseFont(fXS);
    if (fSM)  TTF_CloseFont(fSM);
    if (fMD)  TTF_CloseFont(fMD);
    if (fLG)  TTF_CloseFont(fLG);
    if (fXL)  TTF_CloseFont(fXL);
    if (fXXL) TTF_CloseFont(fXXL);
    if (fMono && fMono != fSM) TTF_CloseFont(fMono);
    fXS = fSM = fMD = fLG = fXL = fXXL = fMono = nullptr;
    TTF_Quit();

    // Clean up custom font textures
    for (int size = 1; size <= 4; ++size) {
        for (int c = 32; c <= 126; ++c) {
            if (g_font_textures[size][c - 32]) {
                SDL_DestroyTexture(g_font_textures[size][c - 32]);
                g_font_textures[size][c - 32] = nullptr;
            }
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  C. LOW-LEVEL PRIMITIVES
// ─────────────────────────────────────────────────────────────────────────────

static void set_col(SDL_Renderer *r, uint32_t c) {
    uint8_t a = c & 0xFF;
    SDL_SetRenderDrawBlendMode(r, (a < 255) ? SDL_BLENDMODE_BLEND : SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(r, (c>>24)&0xFF, (c>>16)&0xFF, (c>>8)&0xFF, a);
}

static void ui_rect(SDL_Renderer *r, int x, int y, int w, int h, uint32_t c) {
    if (w <= 0 || h <= 0) return;
    set_col(r, c);
    SDL_Rect rc = {x, y, w, h};
    SDL_RenderFillRect(r, &rc);
}

static void ui_border(SDL_Renderer *r, int x, int y, int w, int h, uint32_t c, int t = 1) {
    if (w <= 0 || h <= 0) return;
    set_col(r, c);
    SDL_Rect rects[4] = { {x,y,w,t}, {x,y+h-t,w,t}, {x,y,t,h}, {x+w-t,y,t,h} };
    SDL_RenderFillRects(r, rects, 4);
}

static void ui_line(SDL_Renderer *r, int x1, int y1, int x2, int y2, uint32_t c) {
    set_col(r, c);
    SDL_RenderDrawLine(r, x1, y1, x2, y2);
}

static void ui_circle(SDL_Renderer *r, int cx, int cy, int rad, uint32_t c) {
    set_col(r, c);
    for (int dy = -rad; dy <= rad; ++dy) {
        int dx = (int)sqrtf((float)std::max(0, rad*rad - dy*dy));
        SDL_RenderDrawLine(r, cx-dx, cy+dy, cx+dx, cy+dy);
    }
}

static void ui_gradient(SDL_Renderer *r, int x, int y, int w, int h,
                        uint32_t top, uint32_t bot) {
    if (w <= 0 || h <= 0) return;
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    for (int dy = 0; dy < h; ++dy) {
        float t = (float)dy / std::max(1, h-1);
        auto ch = [&](uint32_t col, int sh){ return (uint8_t)((col >> sh) & 0xFF); };
        SDL_SetRenderDrawColor(r,
            (uint8_t)((1-t)*ch(top,24)+t*ch(bot,24)),
            (uint8_t)((1-t)*ch(top,16)+t*ch(bot,16)),
            (uint8_t)((1-t)*ch(top,8 )+t*ch(bot,8 )),
            (uint8_t)((1-t)*ch(top,0 )+t*ch(bot,0 )));
        SDL_RenderDrawLine(r, x, y+dy, x+w-1, y+dy);
    }
}

// Rounded filled rect
static void ui_rounded(SDL_Renderer *r, int x, int y, int w, int h, int rad, uint32_t c) {
    rad = std::min(rad, std::min(w/2, h/2));
    if (rad <= 0) { ui_rect(r, x, y, w, h, c); return; }
    set_col(r, c);
    SDL_Rect rc1 = {x+rad, y, w-2*rad, h};
    SDL_Rect rc2 = {x, y+rad, rad, h-2*rad};
    SDL_Rect rc3 = {x+w-rad, y+rad, rad, h-2*rad};
    SDL_RenderFillRect(r, &rc1);
    SDL_RenderFillRect(r, &rc2);
    SDL_RenderFillRect(r, &rc3);
    ui_circle(r, x+rad,   y+rad,   rad, c);
    ui_circle(r, x+w-rad, y+rad,   rad, c);
    ui_circle(r, x+rad,   y+h-rad, rad, c);
    ui_circle(r, x+w-rad, y+h-rad, rad, c);
}

// Gradient rounded rect (titlebar)
static void ui_rounded_gradient(SDL_Renderer *r, int x, int y, int w, int h, int rad,
                                 uint32_t top_c, uint32_t bot_c) {
    // Draw gradient into a software surface, then composite as rounded rect
    // Simplified: gradient across full rect, then erase corners with background
    ui_gradient(r, x+rad, y, w-2*rad, h, top_c, bot_c);
    for (int dy = 0; dy < rad; ++dy) {
        float t = (float)dy / std::max(1, h-1);
        auto ch = [&](uint32_t col, int sh){ return (uint8_t)((col >> sh) & 0xFF); };
        uint32_t row_c = (
            ((uint32_t)((uint8_t)((1-t)*ch(top_c,24)+t*ch(bot_c,24))) << 24) |
            ((uint32_t)((uint8_t)((1-t)*ch(top_c,16)+t*ch(bot_c,16))) << 16) |
            ((uint32_t)((uint8_t)((1-t)*ch(top_c, 8)+t*ch(bot_c, 8))) <<  8) |
            0xFF
        );
        int dx = (int)sqrtf((float)std::max(0, rad*rad - (rad-dy)*(rad-dy)));
        // left side
        set_col(r, row_c);
        SDL_RenderDrawLine(r, x+rad-dx, y+dy, x+rad, y+dy);
        // right side
        SDL_RenderDrawLine(r, x+w-rad, y+dy, x+w-rad+dx, y+dy);
        // bottom corners
        t = (float)(h-1-dy) / std::max(1, h-1);
        row_c = (
            ((uint32_t)((uint8_t)((1-t)*ch(top_c,24)+t*ch(bot_c,24))) << 24) |
            ((uint32_t)((uint8_t)((1-t)*ch(top_c,16)+t*ch(bot_c,16))) << 16) |
            ((uint32_t)((uint8_t)((1-t)*ch(top_c, 8)+t*ch(bot_c, 8))) <<  8) |
            0xFF
        );
        set_col(r, row_c);
        SDL_RenderDrawLine(r, x+rad-dx, y+h-1-dy, x+rad, y+h-1-dy);
        SDL_RenderDrawLine(r, x+w-rad, y+h-1-dy, x+w-rad+dx, y+h-1-dy);
    }
    // Left/right side strips
    for (int dy = rad; dy < h-rad; ++dy) {
        float t = (float)dy / std::max(1, h-1);
        auto ch = [&](uint32_t col, int sh){ return (uint8_t)((col >> sh) & 0xFF); };
        set_col(r, (uint32_t)(
            ((uint32_t)((uint8_t)((1-t)*ch(top_c,24)+t*ch(bot_c,24))) << 24) |
            ((uint32_t)((uint8_t)((1-t)*ch(top_c,16)+t*ch(bot_c,16))) << 16) |
            ((uint32_t)((uint8_t)((1-t)*ch(top_c, 8)+t*ch(bot_c, 8))) <<  8) | 0xFF));
        SDL_RenderDrawLine(r, x, y+dy, x+rad, y+dy);
        SDL_RenderDrawLine(r, x+w-rad, y+dy, x+w, y+dy);
    }
}

// Soft drop shadow
static void ui_shadow(SDL_Renderer *r, int x, int y, int w, int h) {
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    for (int step = 0; step < 16; ++step) {
        int sx = x - step;
        int sy = y - step + 4;
        int sw = w + step * 2;
        int sh = h + step * 2;
        int alpha = (16 - step) * (16 - step) * 20 / 256;
        if (alpha > 0) {
            ui_rounded(r, sx, sy, sw, sh, WIN_RAD + step, (0x00000000) | alpha);
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  D. TEXT
// ─────────────────────────────────────────────────────────────────────────────
static void draw_ttf_text(SDL_Renderer *r, int x, int y, const char *str, TTF_Font *font, uint32_t color, bool center = false, bool right = false) {
    if (!str || !str[0] || !font) return;
    uint8_t cr = (color >> 24) & 0xFF;
    uint8_t cg = (color >> 16) & 0xFF;
    uint8_t cb = (color >> 8) & 0xFF;
    uint8_t ca = color & 0xFF;
    SDL_Color sdl_col = {cr, cg, cb, ca};
    
    SDL_Surface* surf = TTF_RenderUTF8_Blended(font, str, sdl_col);
    if (!surf) return;
    
    SDL_Texture* tex = SDL_CreateTextureFromSurface(r, surf);
    if (tex) {
        int tx = x;
        if (center) tx -= surf->w / 2;
        else if (right) tx -= surf->w;
        SDL_Rect dst = { tx, y, surf->w, surf->h };
        SDL_RenderCopy(r, tex, nullptr, &dst);
        SDL_DestroyTexture(tex);
    }
    SDL_FreeSurface(surf);
}

static int get_text_width(const char* str, int size) {
    TTF_Font* fnt = fSM;
    if (size == 1) fnt = fXS;
    else if (size == 3) fnt = fLG;
    else if (size == 4) fnt = fXXL;
    int w = 0, h = 0;
    if (fnt) TTF_SizeUTF8(fnt, str, &w, &h);
    return w;
}

static void draw_text(SDL_Renderer* r, int x, int y, const char* str, int size, uint32_t color) {
    TTF_Font* fnt = fSM;
    if (size == 1) fnt = fXS;
    else if (size == 3) fnt = fLG;
    else if (size == 4) fnt = fXXL;
    draw_ttf_text(r, x, y, str, fnt, color, false, false);
}

static void ui_text(SDL_Renderer *r, int x, int y, const char *str, uint32_t c, TTF_Font *fnt) {
    if (!fnt) fnt = fSM;
    draw_ttf_text(r, x, y, str, fnt, c, false, false);
}

static void ui_textC(SDL_Renderer *r, int cx, int y, const char *str, uint32_t c, TTF_Font *fnt) {
    if (!fnt) fnt = fSM;
    draw_ttf_text(r, cx, y, str, fnt, c, true, false);
}

static void ui_textR(SDL_Renderer *r, int rx, int y, const char *str, uint32_t c, TTF_Font *fnt) {
    if (!fnt) fnt = fSM;
    draw_ttf_text(r, rx, y, str, fnt, c, false, true);
}

static int fH(TTF_Font *f) {
    if (!f) f = fSM;
    int h = 16;
    TTF_SizeUTF8(f, "Ag", nullptr, &h);
    return h;
}


// ─────────────────────────────────────────────────────────────────────────────
//  E. COMPOUND COMPONENTS
// ─────────────────────────────────────────────────────────────────────────────

uint32_t get_accent_color() {
    switch (settings_accent_idx) {
        case 0: return 0x0A84FFFF;  // Blue
        case 1: return 0x30D158FF;  // Green
        case 2: return 0xFF453AFF;  // Red
        case 3: return 0xFFD60AFF;  // Amber
        case 4: return 0xBF5AF2FF;  // Purple
        case 5: return 0x40C8E0FF;  // Teal
        default: return 0x0A84FFFF;
    }
}

// Modern pill button
static void ui_btn(SDL_Renderer *r, int x, int y, int w, int h,
                   const char *label, uint32_t bg, uint32_t tc, TTF_Font *f) {
    ui_rounded(r, x, y, w, h, h/2, bg);
    ui_textC(r, x+w/2, y+(h-fH(f))/2, label, tc, f);
}

// macOS traffic-light window control buttons
static void ui_traffic_lights(SDL_Renderer *r, int wx, int wy, bool focused) {
    int cy = wy + BTN_CY_OFF;
    bool hover = (mouse_x >= wx + BTN_CLOSE_CX - BTN_R - 6 &&
                  mouse_x <= wx + BTN_MAX_CX + BTN_R + 6 &&
                  mouse_y >= cy - BTN_R - 6 &&
                  mouse_y <= cy + BTN_R + 6);

    uint32_t close_c = focused ? C_BTN_CLOSE : C_BTN_IDLE;
    uint32_t min_c = focused ? C_BTN_MIN : C_BTN_IDLE;
    uint32_t max_c = focused ? C_BTN_MAX : C_BTN_IDLE;

    ui_circle(r, wx+BTN_CLOSE_CX, cy, BTN_R, close_c);
    ui_circle(r, wx+BTN_MIN_CX,   cy, BTN_R, min_c);
    ui_circle(r, wx+BTN_MAX_CX,   cy, BTN_R, max_c);

    if (hover) {
        uint32_t sym_c = 0x303030FF;
        
        // Close (x)
        ui_line(r, wx+BTN_CLOSE_CX-3, cy-3, wx+BTN_CLOSE_CX+3, cy+3, sym_c);
        ui_line(r, wx+BTN_CLOSE_CX+3, cy-3, wx+BTN_CLOSE_CX-3, cy+3, sym_c);

        // Minimize (-)
        ui_line(r, wx+BTN_MIN_CX-3, cy, wx+BTN_MIN_CX+3, cy, sym_c);

        // Maximize (+)
        ui_line(r, wx+BTN_MAX_CX-3, cy, wx+BTN_MAX_CX+3, cy, sym_c);
        ui_line(r, wx+BTN_MAX_CX, cy-3, wx+BTN_MAX_CX, cy+3, sym_c);
    }
}

// iOS-style toggle switch
static void ui_toggle(SDL_Renderer *r, int x, int y, bool on) {
    int w=48, h=26;
    ui_rounded(r, x, y, w, h, h/2, on ? get_accent_color() : 0x48484AFF);
    // Thumb
    int tx = on ? x+w-h+2 : x+2;
    ui_circle(r, tx+11, y+13, 11, 0xFFFFFFFF);
}

// Progress bar
static void ui_progress(SDL_Renderer *r, int x, int y, int w, int h, float pct, uint32_t fill) {
    ui_rounded(r, x, y, w, h, h/2, 0x3A3A3CFF);
    int fw = (int)(w * std::min(1.0f, std::max(0.0f, pct)));
    if (fw > h) ui_rounded(r, x, y, fw, h, h/2, fill);
}

// Sidebar tab item
static bool ui_tab_item(SDL_Renderer *r, int x, int y, int w, int h,
                         const char *label, bool selected, TTF_Font *f) {
    if (selected) {
        ui_rounded(r, x+4, y+2, w-8, h-4, 6, C_ACCENT_DIM);
        ui_border(r, x+4, y+2, w-8, h-4, C_ACCENT, 1);
    } else if (mouse_x >= x && mouse_x < x+w && mouse_y >= y && mouse_y < y+h) {
        ui_rect(r, x+4, y+2, w-8, h-4, C_HOVER);
    }
    ui_text(r, x+14, y+(h-fH(f))/2, label, selected ? C_TEXT_PRI : C_TEXT_SEC, f);
    return false;
}

// Settings row label + control layout helper
static void ui_row_label(SDL_Renderer *r, int x, int y, const char *label, TTF_Font *f) {
    ui_text(r, x, y, label, C_TEXT_SEC, f);
}

// ─────────────────────────────────────────────────────────────────────────────
//  F. DESKTOP ICON RENDERER  (Scalable vector-art icons via SDL_Renderer)
// ─────────────────────────────────────────────────────────────────────────────
static void draw_icon(SDL_Renderer *r, int ix, int iy, int S, int app_id) {
    int R = S * 7 / 32;

    // Icon shadow
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    for (int i = 0; i < 8 * S / 64; i++) {
        SDL_SetRenderDrawColor(r, 0, 0, 0, (uint8_t)(30 - i * 3));
        SDL_Rect sh = {ix + i, iy + i + 4 * S / 64, S, S};
        SDL_RenderFillRect(r, &sh);
    }

    // Hover highlight
    if (mouse_x >= ix && mouse_x < ix+S && mouse_y >= iy && mouse_y < iy+S) {
        ui_rounded(r, ix - 4 * S / 64, iy - 4 * S / 64, S + 8 * S / 64, S + 8 * S / 64, R + 4 * S / 64, 0xFFFFFF18);
    }

    switch (app_id) {
    case 0: { // Terminal
        ui_rounded(r, ix, iy, S, S, R, 0x0D1117FF);
        ui_rounded(r, ix, iy, S, 12 * S / 64, 4 * S / 64, 0x21252AFF);
        ui_circle(r, ix + 10 * S / 64, iy + 6 * S / 64, 4 * S / 64, 0xFF5F57FF);
        ui_circle(r, ix + 22 * S / 64, iy + 6 * S / 64, 4 * S / 64, 0xFEBC2EFF);
        ui_circle(r, ix + 34 * S / 64, iy + 6 * S / 64, 4 * S / 64, 0x28C840FF);
        draw_text(r, ix + 6 * S / 64, iy + 18 * S / 64, ">", (S < 48) ? 1 : 3, 0x30D158FF);
        draw_text(r, ix + 24 * S / 64, iy + 18 * S / 64, "_", (S < 48) ? 1 : 3, 0xE8EAEDFF);
        if (S >= 48) {
            ui_text(r, ix + 6, iy + 38, "~$", 0x636366FF, fSM);
        }
        break; }
    case 1: { // Files
        ui_rounded(r, ix + 4 * S / 64, iy + 16 * S / 64, 28 * S / 64, 8 * S / 64, 4 * S / 64, 0x2C6EE0FF);
        ui_gradient(r, ix + 2 * S / 64, iy + 22 * S / 64, S - 4 * S / 64, S - 26 * S / 64, 0x3D8EF8FF, 0x1A5BCFFF);
        ui_rounded(r, ix + 2 * S / 64, iy + 22 * S / 64, S - 4 * S / 64, S - 26 * S / 64, 6 * S / 64, 0x3278DBFF);
        ui_rect(r, ix + 10 * S / 64, iy + 32 * S / 64, 44 * S / 64, std::max(1, 3 * S / 64), 0xFFFFFF50);
        ui_rect(r, ix + 10 * S / 64, iy + 40 * S / 64, 36 * S / 64, std::max(1, 3 * S / 64), 0xFFFFFF40);
        ui_rect(r, ix + 10 * S / 64, iy + 48 * S / 64, 28 * S / 64, std::max(1, 3 * S / 64), 0xFFFFFF30);
        break; }
    case 2: { // System Monitor
        ui_rounded(r, ix + 2 * S / 64, iy + 6 * S / 64, S - 4 * S / 64, S - 14 * S / 64, 8 * S / 64, 0x1E2028FF);
        ui_rect(r, ix + 6 * S / 64, iy + 10 * S / 64, S - 12 * S / 64, S - 22 * S / 64, 0x0A0E14FF);
        ui_rect(r, ix + 26 * S / 64, iy + S - 10 * S / 64, 12 * S / 64, 8 * S / 64, 0x1E2028FF);
        ui_rect(r, ix + 18 * S / 64, iy + S - 4 * S / 64, 28 * S / 64, 4 * S / 64, 0x1E2028FF);
        int gx = ix + 8 * S / 64, gy = iy + S - 22 * S / 64;
        int prev_y = gy;
        for (size_t s = 0; s < cpu_history.size() && s < 20; ++s) {
            int nx = gx + (int)(s * (48 * S / 64) / 20);
            int ny = gy - cpu_history[s] * (30 * S / 64) / 100;
            if (s > 0) ui_line(r, gx + (int)((s - 1) * (48 * S / 64) / 20), prev_y, nx, ny, 0x30D158FF);
            prev_y = ny;
        }
        break; }
    case 3: { // Browser
        ui_rounded(r, ix, iy, S, S, R, 0xF5F5F5FF);
        ui_circle(r, ix + S / 2, iy + S / 2, 24 * S / 64, 0xFF0000FF);
        ui_rect(r, ix + S / 2, iy + 8 * S / 64, 24 * S / 64, 24 * S / 64, 0xFFDD00FF);
        ui_rect(r, ix + 8 * S / 64, iy + S / 2, 24 * S / 64, 24 * S / 64, 0x00AA00FF);
        ui_circle(r, ix + S / 2, iy + S / 2, 16 * S / 64, 0xFFFFFFFF);
        ui_circle(r, ix + S / 2, iy + S / 2, 12 * S / 64, 0x4285F4FF);
        if (S >= 48) {
            ui_rounded(r, ix + 6, iy + S - 16, S - 12, 10, 5, 0xE0E0E0FF);
        }
        break; }
    case 4: { // Settings
        ui_rounded(r, ix, iy, S, S, R, 0x1C1F28FF);
        ui_circle(r, ix + S / 2, iy + S / 2, 22 * S / 64, 0x636366FF);
        ui_circle(r, ix + S / 2, iy + S / 2, 16 * S / 64, 0x1C1F28FF);
        for (int t = 0; t < 8; ++t) {
            float ang = t * M_PI / 4;
            int tx = ix + S / 2 + (int)((24 * S / 64) * cosf(ang));
            int ty = iy + S / 2 + (int)((24 * S / 64) * sinf(ang));
            ui_rect(r, tx - std::max(1, 3 * S / 64), ty - std::max(1, 3 * S / 64), std::max(2, 6 * S / 64), std::max(2, 6 * S / 64), 0x636366FF);
        }
        ui_circle(r, ix + S / 2, iy + S / 2, 10 * S / 64, 0x0A84FFFF);
        ui_circle(r, ix + S / 2, iy + S / 2, 5 * S / 64,  0x1C1F28FF);
        break; }
    case 5: { // Control Panel
        ui_rounded(r, ix, iy, S, S, R, 0x141820FF);
        for (int sl = 0; sl < 3; ++sl) {
            int sy = iy + 14 * S / 64 + sl * 14 * S / 64;
            ui_rounded(r, ix + 8 * S / 64, sy, S - 16 * S / 64, std::max(2, 6 * S / 64), 3 * S / 64, 0x3A3A3CFF);
            int fills[] = {42, 28, 50};
            int fill_w = fills[sl] * S / 64;
            ui_rounded(r, ix + 8 * S / 64, sy, fill_w, std::max(2, 6 * S / 64), 3 * S / 64, get_accent_color());
            ui_circle(r, ix + 8 * S / 64 + fill_w, sy + 3 * S / 64, 6 * S / 64, 0xFFFFFFFF);
        }
        if (S >= 48) {
            ui_circle(r, ix + 20, iy + 48, 8, 0x636366FF);
            ui_circle(r, ix + 20, iy + 48, 4, 0x141820FF);
            ui_line(r, ix + 26, iy + 44, ix + 50, iy + 60, 0x636366FF);
        }
        break; }
    case 6: { // Text Editor
        ui_rounded(r, ix + 4 * S / 64, iy + 2 * S / 64, S - 8 * S / 64, S - 4 * S / 64, 6 * S / 64, 0xF5F9FFFF);
        ui_rect(r, ix + S - 16 * S / 64, iy + 2 * S / 64,  16 * S / 64, 14 * S / 64, 0xDDE5F0FF);
        ui_line(r, ix + S - 16 * S / 64, iy + 2 * S / 64,  ix + S - 16 * S / 64, iy + 16 * S / 64, 0x99BBDDFF);
        ui_line(r, ix + S - 16 * S / 64, iy + 16 * S / 64, ix + S - 4 * S / 64, iy + 16 * S / 64,  0x99BBDDFF);
        for (int ln = 0; ln < 5; ++ln) {
            int lw[] = {42, 36, 38, 28, 22};
            ui_rect(r, ix + 10 * S / 64, iy + (22 + ln * 10) * S / 64, lw[ln] * S / 64, std::max(1, 3 * S / 64), 0x334466BB);
        }
        ui_rect(r, ix + 10 * S / 64, iy + 22 * S / 64, std::max(1, 2 * S / 64), 8 * S / 64, 0x0A84FFFF);
        break; }
    case 7: { // App Store
        ui_rounded(r, ix, iy, S, S, R, 0x0A84FFFF);
        ui_rounded(r, ix + 10 * S / 64, iy + 22 * S / 64, 44 * S / 64, 34 * S / 64, 6 * S / 64, 0xFFFFFF30);
        ui_border(r, ix + 10 * S / 64, iy + 22 * S / 64, 44 * S / 64, 34 * S / 64, 0xFFFFFFAA, std::max(1, S / 64));
        ui_line(r, ix + 20 * S / 64, iy + 22 * S / 64, ix + 20 * S / 64, iy + 14 * S / 64, 0xFFFFFFFF);
        ui_line(r, ix + 44 * S / 64, iy + 22 * S / 64, ix + 44 * S / 64, iy + 14 * S / 64, 0xFFFFFFFF);
        ui_circle(r, ix + 20 * S / 64, iy + 14 * S / 64, 4 * S / 64, 0xFFFFFFFF);
        ui_circle(r, ix + 44 * S / 64, iy + 14 * S / 64, 4 * S / 64, 0xFFFFFFFF);
        draw_text(r, ix + S / 2 - std::max(4, 4 * S / 64), iy + 28 * S / 64, "★", (S < 48) ? 1 : 2, 0xFFFFFFFF);
        break; }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  G. WINDOW CHROME  (draw a single window frame — titlebar, shadow, border)
// ─────────────────────────────────────────────────────────────────────────────
static void draw_window_chrome(SDL_Renderer *r, int i, int W, int H, int wx, int wy, int ww, int wh) {
    // Clamp to screen
    if (wx < 0) wx = 0; if (wy < 0) wy = 0;
    if (wx + ww > W) ww = W - wx;
    if (wy + wh > H - TASKBAR_H) wh = H - TASKBAR_H - wy;
    if (ww < 200 || wh < 100) return;

    bool focused = is_focused_window(i);

    // Multi-layer drop shadow
    ui_shadow(r, wx, wy, ww, wh);

    // Window body
    ui_rounded(r, wx, wy, ww, wh, WIN_RAD, C_WIN_BG);

    // Titlebar — gradient
    // Clip titlebar to top rounded part of window
    ui_gradient(r, wx+WIN_RAD, wy, ww-2*WIN_RAD, TITLEBAR_H, C_TITLEBAR, C_TITLEBAR2);
    // Side strips of titlebar
    for (int rad = WIN_RAD; rad > 0; --rad) {
        float t = 0.0f; // top portion
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
        set_col(r, C_TITLEBAR);
        SDL_RenderDrawPoint(r, wx+WIN_RAD-rad, wy+WIN_RAD-rad);
        SDL_RenderDrawPoint(r, wx+ww-WIN_RAD+rad-1, wy+WIN_RAD-rad);
    }
    // Full left/right columns of titlebar
    ui_rect(r, wx, wy+WIN_RAD, WIN_RAD, TITLEBAR_H-WIN_RAD, C_TITLEBAR);
    ui_rect(r, wx+ww-WIN_RAD, wy+WIN_RAD, WIN_RAD, TITLEBAR_H-WIN_RAD, C_TITLEBAR);
    // Actually simpler: just draw gradient across full titlebar, bg clips the corners
    ui_gradient(r, wx, wy+WIN_RAD, ww, TITLEBAR_H-WIN_RAD, C_TITLEBAR, C_TITLEBAR2);

    // Titlebar bottom divider
    uint32_t div_col = focused ? (get_accent_color() & 0xFFFFFF80) : C_BORDER;
    ui_line(r, wx, wy+TITLEBAR_H, wx+ww, wy+TITLEBAR_H, div_col);

    // Traffic lights
    ui_traffic_lights(r, wx, wy, focused);

    // Window title (centered in titlebar)
    ui_textC(r, wx+ww/2, wy+(TITLEBAR_H-fH(fMD))/2,
             windows[i].title, focused ? C_TEXT_PRI : C_TEXT_SEC, fMD);

    // Outer border
    uint32_t border_col = focused ? 0x3A4060FF : 0x2A2A2EFF;
    ui_border(r, wx, wy, ww, wh, border_col);

    // Rounded corners mask (erase content that bleeds outside)
    // (SDL_Renderer can't clip to rounded rects natively — approximate with corner circles)
}

// ─────────────────────────────────────────────────────────────────────────────
//  H. APP WINDOW CONTENT RENDERERS
// ─────────────────────────────────────────────────────────────────────────────

// Content area bounds helper
static SDL_Rect content_area(int wx, int wy, int ww, int wh) {
    return {wx+1, wy+TITLEBAR_H+1, ww-2, wh-TITLEBAR_H-1};
}

// ── Terminal (window 0) ──────────────────────────────────────
static void render_terminal(SDL_Renderer *r, int wx, int wy, int ww, int wh) {
    SDL_Rect ca = content_area(wx, wy, ww, wh);
    ui_rect(r, ca.x, ca.y, ca.w, ca.h, 0x0D1117FF);

    int cw = 0, ch = 0;
    if (fMono) TTF_SizeUTF8(fMono, "M", &cw, &ch);
    if (cw <= 0) cw = 8;
    if (ch <= 0) ch = 16;

    if (cmatrix_active) {
        // Matrix rain
        for (int col = 0; col < (ca.w/cw); ++col) {
            int speed = (col*7)%3+2;
            for (int row = 0; row < (ca.h/ch); ++row) {
                int phase = (gui_frame_counter * speed + col*13 + row*7) % 20;
                uint32_t mc = (phase == 0) ? 0xFFFFFFFF :
                              (phase < 4)  ? 0x00FF88FF :
                              (phase < 10) ? 0x00BB55FF : 0x003322FF;
                char sc[2] = {(char)(33 + (col*31+row*17+gui_frame_counter)%93), '\0'};
                ui_text(r, ca.x+4+col*cw, ca.y+4+row*ch, sc, mc, fMono);
            }
        }
        ui_text(r, ca.x+8, ca.y+4, "Press any key to exit Matrix", 0xFFFFFF60, fSM);
    } else if (bash_shell_active && vterm_screen) {
        // Draw vterm cells
        for (int row = 0; row < TERM_ROWS; ++row) {
            for (int col = 0; col < TERM_COLS; ++col) {
                VTermPos pos = {row, col};
                VTermScreenCell cell;
                if (!vterm_screen_get_cell(vterm_screen, pos, &cell)) continue;

                VTermColor fg = cell.fg, bg = cell.bg;
                vterm_screen_convert_color_to_rgb(vterm_screen, &fg);
                vterm_screen_convert_color_to_rgb(vterm_screen, &bg);

                if (cell.attrs.reverse) { VTermColor tmp=fg; fg=bg; bg=tmp; }

                int cx = ca.x+8 + col*cw;
                int cy2 = ca.y+6 + row*ch;

                // Cell background
                if (VTERM_COLOR_IS_RGB(&bg)) {
                    uint32_t bgc = ((uint32_t)bg.rgb.red<<24)|((uint32_t)bg.rgb.green<<16)|
                                   ((uint32_t)bg.rgb.blue<<8)|0xFF;
                    ui_rect(r, cx, cy2, cw, ch, bgc);
                }

                uint32_t c = cell.chars[0];
                if (c >= 32 && c < 0x10000) {
                    char buf[8] = {};
                    // UTF-8 encode
                    if (c < 0x80)      { buf[0]=(char)c; }
                    else if (c < 0x800){ buf[0]=0xC0|(c>>6); buf[1]=0x80|(c&0x3F); }
                    else               { buf[0]=0xE0|(c>>12); buf[1]=0x80|((c>>6)&0x3F);
                                         buf[2]=0x80|(c&0x3F); }
                    uint32_t fgc = VTERM_COLOR_IS_RGB(&fg) ?
                        ((uint32_t)fg.rgb.red<<24)|((uint32_t)fg.rgb.green<<16)|
                        ((uint32_t)fg.rgb.blue<<8)|0xFF : 0xE8EAEDFF;
                    ui_text(r, cx, cy2, buf, fgc, fMono);
                }
            }
        }
        // Blinking block cursor
        VTermState *vs = vterm_obtain_state(vterm_instance);
        if (vs && (gui_frame_counter/15)%2==0) {
            VTermPos cur; vterm_state_get_cursorpos(vs, &cur);
            ui_rect(r, ca.x+8+cur.col*cw, ca.y+6+cur.row*ch, 2, ch, 0x0A84FFFF);
        }
    } else {
        // VidyaOS GUI shell
        int line_y = ca.y+8;
        size_t start = terminal_logs.size()>22 ? terminal_logs.size()-22 : 0;
        for (size_t l = start; l < terminal_logs.size() && line_y < ca.y+ca.h-ch-4; ++l) {
            uint32_t tc = (terminal_logs[l].find("[OK]")!=std::string::npos) ? 0x30D158FF :
                          (terminal_logs[l].find("Error")!=std::string::npos ||
                           terminal_logs[l].find("error")!=std::string::npos) ? 0xFF453AFF :
                          (terminal_logs[l].find(">>>")!=std::string::npos ||
                           terminal_logs[l].find("js>")!=std::string::npos)  ? 0xFFD60AFF :
                          0xAEAEB2FF;
            ui_text(r, ca.x+8, line_y, terminal_logs[l].c_str(), tc, fMono);
            line_y += ch+2;
        }
        // Prompt line
        std::string prompt = python_repl_active ? ">>> " :
                             javascript_repl_active ? "js> " : "  $ ";
        std::string iline = prompt + gui_input_buffer;
        if ((gui_frame_counter/15)%2==0) iline += "█";
        // Prompt prefix
        ui_text(r, ca.x+8, line_y, "VidyaOS", get_accent_color(), fMono);
        int pw=0,ph=0; TTF_SizeUTF8(fMono,"VidyaOS",&pw,&ph);
        ui_text(r, ca.x+8+pw, line_y, iline.c_str(), C_TEXT_PRI, fMono);
    }
    // Mode badge
    if (bash_shell_active) {
        ui_rounded(r, wx+ww-80, wy+TITLEBAR_H+6, 72, 18, 9, 0x30D15840);
        ui_text(r, wx+ww-74, wy+TITLEBAR_H+9, "BASH MODE", 0x30D158FF, fXS);
    }
}

// ── File Manager (window 1) ──────────────────────────────────
static void render_files(SDL_Renderer *r, int wx, int wy, int ww, int wh) {
    SDL_Rect ca = content_area(wx, wy, ww, wh);
    ui_rect(r, ca.x, ca.y, ca.w, ca.h, C_WIN_BG);

    if (!file_manager_viewing_content.empty()) {
        // Text viewer mode
        ui_rect(r, ca.x, ca.y, ca.w, 32, C_SIDEBAR);
        ui_text(r, ca.x+16, ca.y+8, file_manager_viewing_title.c_str(), C_ACCENT, fMD);
        ui_line(r, ca.x, ca.y+32, ca.x+ca.w, ca.y+32, C_BORDER);
        int ty = ca.y+44;
        std::istringstream ss(file_manager_viewing_content);
        std::string line; int lc=0;
        while(std::getline(ss,line) && lc<30 && ty<ca.y+ca.h-40) {
            ui_text(r, ca.x+20, ty, line.c_str(), C_TEXT_PRI, fSM);
            ty += fH(fSM)+2; ++lc;
        }
        ui_btn(r, ca.x+16, ca.y+ca.h-38, 80, 28, "← Back", C_ACCENT_DIM, C_ACCENT, fSM);
        return;
    }

    const int SW = 180;  // sidebar width
    // Sidebar background
    ui_rect(r, ca.x, ca.y, SW, ca.h, C_SIDEBAR);
    ui_line(r, ca.x+SW, ca.y, ca.x+SW, ca.y+ca.h, C_BORDER);

    // Sidebar tree
    ui_text(r, ca.x+16, ca.y+12, "LOCATIONS", C_TEXT_TER, fXS);
    std::vector<std::pair<std::string,std::string>> tree = {
        {"/","/ (root)"}, {"/home","  Home"}, {"/etc","  Config"},
        {"/usr","  Programs"}, {"/var","  Variable"}, {"/etc/apt","  Packages"}
    };
    for (size_t i = 0; i < tree.size(); ++i) {
        int iy = ca.y+30+(int)i*28;
        bool sel = (tree[i].first == file_manager_current_dir);
        if (sel) ui_rounded(r, ca.x+8, iy-2, SW-16, 26, 6, C_SELECTED);
        ui_text(r, ca.x+20, iy+4, tree[i].second.c_str(),
                sel ? C_ACCENT : C_TEXT_SEC, fSM);
    }

    // File list pane
    int fx = ca.x+SW+1;
    // Toolbar
    ui_rect(r, fx, ca.y, ca.w-SW-1, 36, 0x1A1D25FF);
    ui_text(r, fx+16, ca.y+10, file_manager_current_dir.c_str(), C_TEXT_SEC, fSM);
    ui_btn(r, fx+ca.w-SW-90, ca.y+6, 80, 24, "+ New File", C_ACCENT, C_TEXT_PRI, fXS);
    ui_line(r, fx, ca.y+36, ca.x+ca.w, ca.y+36, C_BORDER);

    // Files
    int fy = ca.y+48;
    for (const auto& [name, content] : virtual_files) {
        if (fy > ca.y+ca.h-50) break;
        if (file_manager_current_dir == "/") {
            if (name.find('/',1) != std::string::npos) continue;
        } else {
            if (name.rfind(file_manager_current_dir+"/",0)!=0) continue;
            std::string sub = name.substr(file_manager_current_dir.size()+1);
            if (sub.find('/') != std::string::npos) continue;
        }
        bool issel = (name == selected_file);
        bool isdir = (content == "<dir>");
        if (issel) ui_rounded(r, fx+4, fy-2, ca.w-SW-12, 26, 4, C_SELECTED);
        else if (mouse_x>fx && mouse_x<ca.x+ca.w && mouse_y>=fy-2 && mouse_y<fy+24)
            ui_rect(r, fx+4, fy-2, ca.w-SW-12, 26, C_HOVER);

        std::string dname = name.substr(name.rfind('/')+1);
        std::string icon  = isdir ? "📁 " : "📄 ";
        std::string label = (isdir ? "[DIR] " : "") + dname;
        bool dirty = mft.isPathChanged(name);
        uint32_t tc = issel ? C_TEXT_PRI :
                      dirty ? 0xFFD60AFF :
                      isdir ? 0x0A84FFFF : C_TEXT_SEC;
        ui_text(r, fx+16, fy+4, label.c_str(), tc, fSM);

        // Size badge
        if (!isdir) {
            char sz[32]; snprintf(sz,sizeof(sz),"%zu B", content.size());
            ui_textR(r, ca.x+ca.w-16, fy+4, sz, C_TEXT_TER, fXS);
        }
        fy += 28;
    }

    // Bottom action bar
    int bay = ca.y+ca.h-36;
    ui_rect(r, fx, bay, ca.w-SW-1, 36, C_SIDEBAR);
    ui_line(r, fx, bay, ca.x+ca.w, bay, C_BORDER);
    if (!selected_file.empty()) {
        ui_btn(r, fx+16, bay+6, 80, 24, "Open", C_ACCENT, C_TEXT_PRI, fXS);
        ui_btn(r, fx+104, bay+6, 80, 24, "Delete", 0xFF443A30, C_RED_ERR, fXS);
    }
}

// ── System Monitor (window 2) ────────────────────────────────
static void render_monitor(SDL_Renderer *r, int wx, int wy, int ww, int wh) {
    SDL_Rect ca = content_area(wx, wy, ww, wh);
    ui_rect(r, ca.x, ca.y, ca.w, ca.h, C_WIN_BG);

    // Top stat cards row
    const int CW = (ca.w-32)/3, CH = 80, CY = ca.y+12;
    struct { const char *label; int val; const char *unit; uint32_t col; } cards[3] = {
        {"CPU Usage", telemetry_cpu_usage, "%", 0x30D158FF},
        {"RAM",       telemetry_ram_usage, " MB", 0x0A84FFFF},
        {"Temp",      current_sensor_value, "°C", 0xFFD60AFF}
    };
    for (int c = 0; c < 3; ++c) {
        int cx2 = ca.x+12 + c*(CW+4);
        ui_rounded(r, cx2, CY, CW, CH, 10, C_SIDEBAR);
        ui_border(r, cx2, CY, CW, CH, C_BORDER);
        char vbuf[32]; snprintf(vbuf,sizeof(vbuf),"%d%s", cards[c].val, cards[c].unit);
        ui_text(r, cx2+16, CY+12, cards[c].label, C_TEXT_TER, fXS);
        ui_text(r, cx2+16, CY+30, vbuf, cards[c].col, fXL);
        ui_progress(r, cx2+16, CY+60, CW-32, 8, (float)cards[c].val/100.f, cards[c].col);
    }

    // Graph area
    int gx=ca.x+16, gy=CY+CH+20, gw=ca.w-32, gh=ca.h-(CH+40)-24;
    ui_rounded(r, gx, gy, gw, gh, 10, C_SIDEBAR);
    ui_border(r, gx, gy, gw, gh, C_BORDER);

    // Grid lines
    for (int gl=1; gl<4; ++gl) {
        int lly = gy + gh*gl/4;
        set_col(r, 0xFFFFFF08);
        SDL_RenderDrawLine(r, gx+4, lly, gx+gw-4, lly);
    }
    // Percentage labels
    for (int gl=0; gl<=4; ++gl) {
        char lb[8]; snprintf(lb,sizeof(lb),"%d%%",100-gl*25);
        ui_text(r, gx+6, gy+gh*gl/4-6, lb, C_TEXT_TER, fXS);
    }

    // CPU line graph
    if (cpu_history.size() >= 2) {
        int bx=gx+40, by=gy+gh-8, gpw=gw-52, gph=gh-16;
        for (size_t s = 0; s < cpu_history.size()-1; ++s) {
            int x0=bx+s*gpw/(cpu_history.size()-1);
            int x1=bx+(s+1)*gpw/(cpu_history.size()-1);
            int y0=by-cpu_history[s]*gph/100;
            int y1=by-cpu_history[s+1]*gph/100;
            for (int t=-1; t<=1; ++t)
                ui_line(r,x0,y0+t,x1,y1+t, t==0?0x30D158FF:0x30D15840);
        }
    }
    // RAM line graph
    if (ram_history.size() >= 2) {
        int bx=gx+40, by=gy+gh-8, gpw=gw-52, gph=gh-16;
        int mx=*std::max_element(ram_history.begin(),ram_history.end());
        if (mx<1) mx=1;
        for (size_t s = 0; s < ram_history.size()-1; ++s) {
            int x0=bx+s*gpw/(ram_history.size()-1);
            int x1=bx+(s+1)*gpw/(ram_history.size()-1);
            int y0=by-ram_history[s]*gph/mx;
            int y1=by-ram_history[s+1]*gph/mx;
            for (int t=-1; t<=1; ++t)
                ui_line(r,x0,y0+t,x1,y1+t, t==0?0x0A84FFFF:0x0A84FF40);
        }
    }
    // Legend
    ui_circle(r,gx+48,gy+gh+10,5,0x30D158FF); ui_text(r,gx+58,gy+gh+4,"CPU",0x30D158FF,fXS);
    ui_circle(r,gx+100,gy+gh+10,5,0x0A84FFFF); ui_text(r,gx+110,gy+gh+4,"RAM",0x0A84FFFF,fXS);
}

static void parse_and_render_html(SDL_Renderer *r, const std::string& html, int start_x, int start_y, int max_w, int max_h) {
    int cur_x = start_x;
    int cur_y = start_y;
    int line_h = 20;
    
    TTF_Font* current_font = fSM;
    uint32_t current_color = 0x1D1D1FFF;
    bool is_bold = false;
    bool is_italic = false;
    bool in_link = false;
    std::string link_url = "";
    
    size_t i = 0;
    std::string text_buffer = "";
    
    auto flush_text = [&]() {
        if (text_buffer.empty()) return;
        
        int tw = 0, th = 0;
        TTF_SizeUTF8(current_font, text_buffer.c_str(), &tw, &th);
        
        if (cur_x + tw > start_x + max_w - 20) {
            cur_x = start_x;
            cur_y += line_h;
        }
        
        draw_ttf_text(r, cur_x, cur_y, text_buffer.c_str(), current_font, current_color);
        
        if (in_link) {
            ui_line(r, cur_x, cur_y + th - 2, cur_x + tw, cur_y + th - 2, current_color);
            browser_links.push_back({cur_x, cur_y, cur_x + tw, cur_y + th, link_url});
        }
        
        cur_x += tw;
        text_buffer.clear();
    };
    
    while (i < html.size()) {
        if (html[i] == '<') {
            flush_text();
            size_t tag_end = html.find('>', i);
            if (tag_end == std::string::npos) {
                text_buffer += html[i];
                i++;
                continue;
            }
            std::string tag_full = html.substr(i + 1, tag_end - i - 1);
            i = tag_end + 1;
            
            size_t space_pos = tag_full.find(' ');
            std::string tag = (space_pos == std::string::npos) ? tag_full : tag_full.substr(0, space_pos);
            
            for (auto &c : tag) c = tolower(c);
            
            if (tag == "h1") {
                current_font = fXL;
                line_h = 32;
                cur_x = start_x;
                cur_y += 10;
            } else if (tag == "/h1") {
                current_font = fSM;
                line_h = 20;
                cur_x = start_x;
                cur_y += 32;
            } else if (tag == "h2" || tag == "h3" || tag == "h4" || tag == "h5" || tag == "h6") {
                current_font = fLG;
                line_h = 26;
                cur_x = start_x;
                cur_y += 8;
            } else if (tag == "/h2" || tag == "/h3" || tag == "/h4" || tag == "/h5" || tag == "/h6") {
                current_font = fSM;
                line_h = 20;
                cur_x = start_x;
                cur_y += 26;
            } else if (tag == "p") {
                cur_x = start_x;
                cur_y += 6;
            } else if (tag == "/p") {
                cur_x = start_x;
                cur_y += 20;
            } else if (tag == "br" || tag == "br/") {
                cur_x = start_x;
                cur_y += line_h;
            } else if (tag == "b" || tag == "strong") {
                is_bold = true;
                current_color = 0x000000FF;
            } else if (tag == "/b" || tag == "/strong") {
                is_bold = false;
                current_color = 0x1D1D1FFF;
            } else if (tag == "i" || tag == "em") {
                is_italic = true;
            } else if (tag == "/i" || tag == "/em") {
                is_italic = false;
            } else if (tag == "ul") {
                cur_x = start_x;
                cur_y += 6;
            } else if (tag == "/ul") {
                cur_x = start_x;
                cur_y += 14;
            } else if (tag == "li") {
                cur_x = start_x + 16;
                ui_circle(r, cur_x - 8, cur_y + 8, 3, 0x555555FF);
            } else if (tag == "/li") {
                cur_x = start_x;
                cur_y += line_h;
            } else if (tag == "a") {
                in_link = true;
                current_color = 0x1A0DABFF;
                size_t href_pos = tag_full.find("href=");
                if (href_pos != std::string::npos) {
                    size_t q_start = tag_full.find('"', href_pos);
                    if (q_start != std::string::npos) {
                        size_t q_end = tag_full.find('"', q_start + 1);
                        if (q_end != std::string::npos) {
                            link_url = tag_full.substr(q_start + 1, q_end - q_start - 1);
                        }
                    }
                }
            } else if (tag == "/a") {
                in_link = false;
                current_color = 0x1D1D1FFF;
                link_url = "";
            } else if (tag == "img") {
                int img_w = 80;
                int img_h = 60;
                if (cur_x + img_w > start_x + max_w - 20) {
                    cur_x = start_x;
                    cur_y += line_h;
                }
                ui_rounded(r, cur_x, cur_y, img_w, img_h, 4, 0xDFDFE1FF);
                ui_border(r, cur_x, cur_y, img_w, img_h, 0xCCCCCCFF);
                ui_textC(r, cur_x + img_w/2, cur_y + img_h/2 - 6, "IMG", 0x666666FF, fXS);
                cur_x += img_w + 10;
            }
        } else {
            text_buffer += html[i];
            i++;
        }
    }
    flush_text();
}

// ── Browser (window 3) ──────────────────────────────────────
static void render_browser(SDL_Renderer *r, int wx, int wy, int ww, int wh) {
    SDL_Rect ca = content_area(wx, wy, ww, wh);
    ui_rect(r, ca.x, ca.y, ca.w, ca.h, 0xF5F5F7FF);

    // Navigation bar
    int nbh = 44;
    ui_rect(r, ca.x, ca.y, ca.w, nbh, 0xECECEEFF);
    ui_line(r, ca.x, ca.y+nbh, ca.x+ca.w, ca.y+nbh, 0xCCCCCEFF);

    // Back/Forward buttons
    ui_rounded(r, ca.x+8,  ca.y+8, 30, 28, 8, 0xD8D8DAFF);
    ui_rounded(r, ca.x+44, ca.y+8, 30, 28, 8, 0xD8D8DAFF);
    ui_textC(r, ca.x+23,  ca.y+14, "‹", 0x1D1D1FFF, fLG);
    ui_textC(r, ca.x+59,  ca.y+14, "›", 0x1D1D1FFF, fLG);

    // Address bar
    int ab_x=ca.x+82, ab_w=ca.w-140;
    ui_rounded(r, ab_x, ca.y+8, ab_w, 28, 10,
               chrome_address_active ? 0xFFFFFFFF : 0xE8E8EAFF);
    if (chrome_address_active) {
        ui_border(r, ab_x, ca.y+8, ab_w, 28, 0x0A84FFFF);
    }
    std::string url_disp = chrome_url + (chrome_address_active ? "│" : "");
    ui_textC(r, ab_x+ab_w/2, ca.y+14, url_disp.c_str(), 0x1D1D1FFF, fSM);

    // Downloads toggle button
    int dl_btn_x = ca.x + ca.w - 48;
    ui_rounded(r, dl_btn_x, ca.y+8, 36, 28, 8, downloads_panel_open ? C_ACCENT : 0xD8D8DAFF);
    ui_textC(r, dl_btn_x+18, ca.y+14, "📥", downloads_panel_open ? C_TEXT_PRI : 0x1D1D1FFF, fSM);

    // Bookmarks bar (height 28)
    int bmh = 28;
    ui_rect(r, ca.x, ca.y+nbh, ca.w, bmh, 0xF0F0F2FF);
    ui_line(r, ca.x, ca.y+nbh+bmh, ca.x+ca.w, ca.y+nbh+bmh, 0xDFDFE1FF);

    // Draw bookmarks
    auto draw_bookmark_tab = [&](int bx, const char *label, bool active) {
        ui_rounded(r, bx, ca.y+nbh+2, 90, 24, 6, active ? 0xE2E2E6FF : 0x00000000);
        ui_textC(r, bx+45, ca.y+nbh+7, label, 0x1D1D1FFF, fXS);
    };
    draw_bookmark_tab(ca.x+10, "🏠 Home", chrome_url == "index.html" || chrome_url == "vidyaos://home");
    draw_bookmark_tab(ca.x+110, "📰 News", chrome_url == "news.html" || chrome_url == "vidyaos://news");
    draw_bookmark_tab(ca.x+210, "✉️ Mail", chrome_url == "mail.html" || chrome_url == "vidyaos://mail");
    draw_bookmark_tab(ca.x+320, "🛍️ Store", chrome_url == "store.html" || chrome_url == "vidyaos://store");

    // Content area starting below bookmarks bar
    extern int browser_scroll_y;
    int cy = ca.y+nbh+bmh+8;
    browser_links.clear();

    if (browser_is_fetching()) {
        ui_textC(r, ca.x+ca.w/2, cy+40, "Loading...", 0x888888FF, fLG);
    } else {
        const FetchResult& res = browser_get_result();
        if (res.success) {
            int clip_y = cy;
            SDL_Rect old_clip;
            SDL_RenderGetClipRect(r, &old_clip);
            SDL_Rect content_clip = {ca.x, clip_y, ca.w, ca.h - (clip_y - ca.y)};
            SDL_RenderSetClipRect(r, &content_clip);
            
            parse_and_render_html(r, res.raw_body, ca.x + 20, cy - browser_scroll_y, ca.w - 40, 20000);
            
            SDL_RenderSetClipRect(r, &old_clip);
        } else if (!res.error_message.empty()) {
            ui_textC(r, ca.x+ca.w/2, cy+40, "Fetch Error", 0xCC0000FF, fXXL);
            ui_textC(r, ca.x+ca.w/2, cy+90, res.error_message.c_str(), 0x888888FF, fMD);
        } else {
            // Local files / fallback
            std::string target_file = chrome_url;
            if (target_file == "flathub.org" || target_file == "https://flathub.org") {
                ui_text(r, ca.x+20, cy+10, "Flathub App Store", 0x111111FF, fLG);
                ui_text(r, ca.x+20, cy+32, "Download sandbox Flatpak applications directly", 0x666666FF, fSM);
                ui_line(r, ca.x+20, cy+52, ca.x+ca.w-20, cy+52, 0xDDDDDDFF);
                
                struct MockFlatApp { std::string name; std::string pkg; std::string desc; uint32_t color; };
                std::vector<MockFlatApp> flat_apps = {
                    {"Visual Studio Code", "vscode", "Developer IDE & Editor", 0x007ACCFF},
                    {"VLC Media Player", "vlc", "Multimedia audio & video player", 0xFF8800FF},
                    {"Mozilla Firefox", "firefox", "Secure fast web browser client", 0xE66000FF},
                    {"GIMP Image Editor", "gimp", "GNU Image Manipulation Tool", 0x5C3A21FF}
                };
                
                int item_y = cy + 64 - browser_scroll_y;
                for (size_t a=0; a<flat_apps.size(); ++a) {
                    ui_rounded(r, ca.x+20, item_y, ca.w-40, 52, 8, 0xFFFFFFFF);
                    ui_border(r, ca.x+20, item_y, ca.w-40, 52, 0xE0E0E0FF);
                    ui_rounded(r, ca.x+30, item_y+10, 32, 32, 6, flat_apps[a].color);
                    ui_textC(r, ca.x+46, item_y+18, flat_apps[a].name.substr(0,1).c_str(), 0xFFFFFFFF, fSM);
                    ui_text(r, ca.x+72, item_y+10, flat_apps[a].name.c_str(), 0x1D1D1FFF, fSM);
                    ui_text(r, ca.x+72, item_y+28, flat_apps[a].desc.c_str(), 0x666666FF, fXS);
                    
                    bool is_inst = apt_installed_packages[flat_apps[a].pkg];
                    int bw = 90;
                    int bx = ca.x+ca.w-bw-30;
                    if (is_inst) {
                        ui_btn(r, bx, item_y+10, bw, 32, "Installed", 0xD1F2D9FF, 0x1E5631FF, fXS);
                    } else {
                        ui_btn(r, bx, item_y+10, bw, 32, "Download", C_ACCENT, C_TEXT_PRI, fXS);
                        browser_links.push_back({bx, item_y+10, bx+bw, item_y+42, "download:" + flat_apps[a].pkg});
                    }
                    item_y += 60;
                }
            }
            else {
                if (target_file.rfind("file:///var/www/", 0) == 0) {
                    target_file = "/var/www/" + target_file.substr(16);
                } else if (target_file.rfind("vidyaos://", 0) == 0) {
                    target_file = "/var/www/" + target_file.substr(10);
                } else if (target_file == "vidyaos.local") {
                    target_file = "/var/www/index.html";
                } else if (target_file == "news.local") {
                    target_file = "/var/www/news.html";
                } else if (target_file == "mail.local") {
                    target_file = "/var/www/mail.html";
                } else if (target_file == "store.local") {
                    target_file = "/var/www/store.html";
                } else if (target_file.find('/') == std::string::npos) {
                    target_file = "/var/www/" + target_file;
                }
        
                if (virtual_files.count(target_file)) {
                    parse_and_render_html(r, virtual_files[target_file], ca.x + 20, cy - browser_scroll_y, ca.w - 40, 20000);
                } else {
                    ui_textC(r, ca.x+ca.w/2, cy+40, "404", 0xCCCCCCFF, fXXL);
                    ui_textC(r, ca.x+ca.w/2, cy+90, "Page not found in VidyaOS virtual web", 0x888888FF, fMD);
                    ui_textC(r, ca.x+ca.w/2, cy+120, "Try navigating to google.com to test internet fetch!", 0x1A0DABFF, fSM);
                }
            }
        }
    }

    if (downloads_panel_open) {
        int card_w = 260;
        int card_h = 220;
        int card_x = ca.x + ca.w - card_w - 12;
        int card_y = ca.y + nbh + 4;
        
        ui_shadow(r, card_x, card_y, card_w, card_h);
        ui_rounded(r, card_x, card_y, card_w, card_h, 8, 0xFFFFFFFF);
        ui_border(r, card_x, card_y, card_w, card_h, 0xCCCCCCFF);
        
        ui_text(r, card_x+12, card_y+10, "Downloads", 0x1D1D1FFF, fSM);
        ui_line(r, card_x, card_y+30, card_x+card_w, card_y+30, 0xEEEEEEFF);
        
        if (browser_downloads.empty()) {
            ui_textC(r, card_x+card_w/2, card_y+card_h/2, "No downloads", 0x888888FF, fXS);
        } else {
            int dl_y = card_y + 36;
            for (int d = (int)browser_downloads.size()-1; d >= 0 && dl_y < card_y+card_h-30; --d) {
                const auto &dl = browser_downloads[d];
                std::string fname = dl.filename;
                if (fname.size() > 20) fname = fname.substr(0, 18) + "...";
                ui_text(r, card_x+12, dl_y, fname.c_str(), 0x1D1D1FFF, fXS);
                
                if (dl.completed) {
                    ui_textR(r, card_x+card_w-12, dl_y, "Done", 0x34A853FF, fXS);
                } else {
                    ui_progress(r, card_x+12, dl_y+16, card_w-24, 6, dl.progress/100.f, C_ACCENT);
                }
                dl_y += 36;
            }
        }
    }
}

// ── Settings (window 4) ──────────────────────────────────────
static void render_settings(SDL_Renderer *r, int wx, int wy, int ww, int wh) {
    SDL_Rect ca = content_area(wx, wy, ww, wh);
    ui_rect(r, ca.x, ca.y, ca.w, ca.h, C_WIN_BG);

    const int SW = 160;
    // Sidebar
    ui_rect(r, ca.x, ca.y, SW, ca.h, C_SIDEBAR);
    ui_line(r, ca.x+SW, ca.y, ca.x+SW, ca.y+ca.h, C_BORDER);

    const char *tabs[]={"Display","Appearance","Network","Accounts","Sound","Notifications","Updates","Hardening","Accessibility","Smart Home","Bluetooth"};
    for (int t=0; t<11; ++t) {
        ui_tab_item(r, ca.x, ca.y+8+t*38, SW, 36, tabs[t], t==settings_active_tab, fSM);
    }

    // Content pane
    int px=ca.x+SW+20, py=ca.y+20, pw=ca.w-SW-40;

    auto row_label = [&](int ry, const std::string &lbl) {
        ui_text(r, px, ry, lbl.c_str(), C_TEXT_SEC, fSM);
    };
    auto row_y = [&](int i){ return py + i*52; };

    if (settings_active_tab==0) { // Display
        ui_text(r, px, py-8, "Display Settings", C_TEXT_PRI, fLG);
        ui_line(r, px, py+16, px+pw, py+16, C_BORDER);

        row_label(row_y(1), "Scale Factor");
        char sc[16]; snprintf(sc,sizeof(sc),"%dx", current_window_scale);
        ui_text(r, px+pw-40, row_y(1), sc, C_ACCENT, fSM);
        ui_progress(r, px, row_y(1)+20, pw, 8, (float)(current_window_scale-1)/3.f, C_ACCENT);

        row_label(row_y(2), "Wallpaper");
        uint32_t wps[]={0x112233FF,0x051025FF,0x803050FF,0x052010FF};
        for (int wp=0; wp<4; ++wp) {
            int bx=px+(wp*56);
            ui_rounded(r, bx, row_y(2)+18, 48, 32, 6, wps[wp]);
            if (settings_wallpaper_idx==wp)
                ui_border(r, bx, row_y(2)+18, 48, 32, C_ACCENT, 2);
        }

        row_label(row_y(3), "Resolution");
        ui_text(r, px+pw/2, row_y(3), "Native (auto-detected)", C_TEXT_TER, fSM);
    }
    else if (settings_active_tab==1) { // Appearance
        ui_text(r, px, py-8, "Appearance", C_TEXT_PRI, fLG);
        ui_line(r, px, py+16, px+pw, py+16, C_BORDER);

        row_label(row_y(1), "Theme");
        for (size_t t=0; t<installed_themes.size(); ++t) {
            int tx=px+(int)t*90;
            bool act=(active_theme.name==installed_themes[t].name);
            ui_rounded(r, tx, row_y(1)+18, 80, 32, 8, act?C_ACCENT:C_SIDEBAR);
            if (act) ui_border(r, tx, row_y(1)+18, 80, 32, C_ACCENT, 2);
            ui_textC(r, tx+40, row_y(1)+26,
                     installed_themes[t].name.c_str(), act?C_TEXT_PRI:C_TEXT_SEC, fXS);
        }

        row_label(row_y(2), "Accent Color");
        uint32_t acs[]={0x0A84FFFF,0x30D158FF,0xFF453AFF,0xFFD60AFF,0xBF5AF2FF,0x40C8E0FF};
        for (int c=0; c<6; ++c) {
            int bx=px+(c*40);
            ui_circle(r, bx+14, row_y(2)+28, 12, acs[c]);
            if (settings_accent_idx==c)
                ui_circle(r, bx+14, row_y(2)+28, 16, 0xFFFFFF80);
        }

        row_label(row_y(3), "Dark Mode");
        ui_toggle(r, px+pw-60, row_y(3)+14, is_dark_theme);

        row_label(row_y(4), "Tiling Mode");
        ui_toggle(r, px+pw-60, row_y(4)+14, is_tiling_mode);
    }
    else if (settings_active_tab==2) { // Network
        ui_text(r, px, py-8, "Network", C_TEXT_PRI, fLG);
        ui_line(r, px, py+16, px+pw, py+16, C_BORDER);

        row_label(row_y(1), "Wi-Fi");
        ui_toggle(r, px+pw-60, row_y(1)+14, wifi_enabled);
        if (wifi_enabled) {
            ui_text(r, px+20, row_y(1)+28, net_state.ssid.c_str(), C_ACCENT, fXS);
        }

        row_label(row_y(2), "VPN");
        ui_toggle(r, px+pw-60, row_y(2)+14, vpn_enabled);

        row_label(row_y(3), "IP Address");
        ui_text(r, px+120, row_y(3), net_state.ipAddress.c_str(), C_TEXT_TER, fSM);

        row_label(row_y(4), "Available Networks");
        int ny=row_y(4)+22;
        for (const auto &n : available_networks) {
            ui_rounded(r, px, ny, pw, 28, 6, C_SIDEBAR);
            ui_text(r, px+12, ny+6, n.c_str(), C_TEXT_PRI, fSM);
            if (n==net_state.ssid)
                ui_text(r, px+pw-40, ny+6, "✓", C_ACCENT, fSM);
            ny += 34;
        }
    }
    else if (settings_active_tab==3) { // Accounts
        ui_text(r, px, py-8, "Users & Accounts", C_TEXT_PRI, fLG);
        ui_line(r, px, py+16, px+pw, py+16, C_BORDER);
        row_label(row_y(1), "Current user: " + currentUser);
        row_label(row_y(2), "Total accounts: " + std::to_string(virtual_users.size()));
        ui_btn(r, px, row_y(3), 120, 32, "+ Add User", C_ACCENT, C_TEXT_PRI, fSM);
        ui_btn(r, px+128, row_y(3), 140, 32, "Reset Password", C_SIDEBAR, C_TEXT_SEC, fSM);
        int uy=row_y(4);
        for (const auto &u: virtual_users) {
            ui_circle(r, px+18, uy+14, 14, get_accent_color());
            ui_textC(r, px+18, uy+7, u.substr(0,1).c_str(), C_TEXT_PRI, fMD);
            ui_text(r, px+40, uy+6, u.c_str(), C_TEXT_PRI, fSM);
            uy += 36;
        }
    }
    else if (settings_active_tab==4) { // Sound
        ui_text(r, px, py-8, "Sound", C_TEXT_PRI, fLG);
        ui_line(r, px, py+16, px+pw, py+16, C_BORDER);
        row_label(row_y(1), "Volume");
        ui_progress(r, px, row_y(1)+24, pw, 12, system_volume/100.f, C_ACCENT);
        char vs[16]; snprintf(vs,sizeof(vs),"%d%%",system_volume);
        ui_textR(r, px+pw, row_y(1)+20, vs, C_TEXT_PRI, fSM);
        row_label(row_y(2), "Mute");
        ui_toggle(r, px+pw-60, row_y(2)+14, system_muted);
        row_label(row_y(3), "Output device");
        ui_text(r, px+120, row_y(3), "System Default", C_TEXT_TER, fSM);
    }
    else if (settings_active_tab==5) { // Notifications
        ui_text(r, px, py-8, "Notifications", C_TEXT_PRI, fLG);
        ui_line(r, px, py+16, px+pw, py+16, C_BORDER);
        row_label(row_y(1), "Allow Notifications");
        ui_toggle(r, px+pw-60, row_y(1)+14, notifications_enabled);
        char nc[32]; snprintf(nc,sizeof(nc),"%zu notifications pending",notifications.size());
        ui_text(r, px, row_y(2), nc, C_TEXT_TER, fSM);
        ui_btn(r, px, row_y(3), 120, 32, "Clear All", 0xFF443A30, C_RED_ERR, fSM);
    }
    else if (settings_active_tab==6) { // Updates
        ui_text(r, px, py-8, "Software Update & A/B Slots", C_TEXT_PRI, fLG);
        ui_line(r, px, py+16, px+pw, py+16, C_BORDER);
        ui_text(r, px, py+24, ("Active System Partition: Slot " + active_system_slot).c_str(), C_TEXT_SEC, fXS);
        ui_text(r, px, py+38, ("Inactive Backup Partition: Slot " + inactive_system_slot).c_str(), C_TEXT_TER, fXS);
        
        ui_btn(r, px, row_y(1), 160, 36, "Check for Updates", C_ACCENT, C_TEXT_PRI, fSM);
        if (update_check_progress >= 0) {
            ui_text(r, px, row_y(2), "Checking...", C_TEXT_SEC, fSM);
            ui_progress(r, px, row_y(2)+24, pw, 10, update_check_progress/100.f, C_ACCENT);
            if (update_check_progress >= 100) {
                ui_text(r, px, row_y(3), "✓ New Update Version 1.1.0 Available!", C_GREEN, fMD);
                ui_btn(r, px, row_y(3)+32, 200, 32, "Download & Apply to Slot B", C_ACCENT, C_TEXT_PRI, fSM);
            }
        }
        if (slot_upgrade_pending) {
            ui_text(r, px, row_y(4), "Update applied successfully. System reboot required.", C_AMBER, fXS);
            ui_btn(r, px, row_y(4)+24, 200, 32, "Reboot & Swap Slots", C_RED_ERR, C_TEXT_PRI, fSM);
        }
    }
    else if (settings_active_tab==7) { // Hardening
        ui_text(r, px, py-8, "Security & Hardening", C_TEXT_PRI, fLG);
        ui_line(r, px, py+16, px+pw, py+16, C_BORDER);
        
        row_label(row_y(1), "AppArmor Policies");
        ui_toggle(r, px+pw-60, row_y(1)+14, apparmor_enabled);
        ui_text(r, px, row_y(1)+28, apparmor_enabled ? "Policies enforcing sandbox constraints." : "Unrestricted platform permissions.", C_TEXT_TER, fXS);

        row_label(row_y(2), "Firewall (nftables status)");
        ui_text(r, px+pw-100, row_y(2), "Active", C_GREEN, fSM);

        row_label(row_y(3), "Blocked Applications:");
        std::string apps[] = {"Terminal", "Browser", "App Store"};
        for (int i=0; i<3; ++i) {
            int ry = row_y(3) + 24 + i*28;
            bool blocked = !firewall_rules[apps[i]];
            ui_text(r, px+12, ry, apps[i].c_str(), C_TEXT_PRI, fSM);
            ui_rounded(r, px+pw-60, ry, 48, 20, 10, blocked ? C_RED_ERR : C_SIDEBAR);
            ui_textC(r, px+pw-36, ry+3, blocked ? "Block" : "Allow", C_TEXT_PRI, fXS);
        }
    }
    else if (settings_active_tab==8) { // Accessibility
        ui_text(r, px, py-8, "Accessibility & Locale", C_TEXT_PRI, fLG);
        ui_line(r, px, py+16, px+pw, py+16, C_BORDER);

        row_label(row_y(1), "Screen Reader (TTS)");
        ui_toggle(r, px+pw-60, row_y(1)+14, screen_reader_enabled);

        row_label(row_y(2), "Language / Locale");
        std::string locales[] = {"en_US", "es_ES", "de_DE"};
        for (int l=0; l<3; ++l) {
            int lx = px + l * 80;
            bool active = (current_locale == locales[l]);
            ui_rounded(r, lx, row_y(2)+18, 70, 28, 6, active ? C_ACCENT : C_SIDEBAR);
            ui_textC(r, lx+35, row_y(2)+24, locales[l].c_str(), active ? C_TEXT_PRI : C_TEXT_SEC, fXS);
        }

        row_label(row_y(3), "Input Method (IME)");
        ui_text(r, px+12, row_y(3)+20, "US Keyboard (Default)", C_TEXT_PRI, fSM);
    }
    else if (settings_active_tab==9) { // Smart Home
        ui_text(r, px, py-8, "MQTT Smart Home Dashboard", C_TEXT_PRI, fLG);
        ui_line(r, px, py+16, px+pw, py+16, C_BORDER);

        row_label(row_y(1), "Living Room Light");
        bool light_on = (iot_devices[0].state == "on");
        ui_rounded(r, px+pw-100, row_y(1)+10, 80, 28, 6, light_on ? C_ACCENT : C_SIDEBAR);
        ui_textC(r, px+pw-60, row_y(1)+16, light_on ? "ON" : "OFF", C_TEXT_PRI, fXS);

        row_label(row_y(2), "Smart Thermostat");
        ui_text(r, px+20, row_y(2)+24, ("Current Temp: " + iot_devices[1].state).c_str(), C_TEXT_SEC, fSM);
        ui_btn(r, px+pw-120, row_y(2)+10, 30, 28, "-", 0x2A2A2EFF, C_TEXT_PRI, fXS);
        ui_btn(r, px+pw-50, row_y(2)+10, 30, 28, "+", 0x2A2A2EFF, C_TEXT_PRI, fXS);

        row_label(row_y(3), "Smart Plug");
        bool plug_on = (iot_devices[2].state == "on");
        ui_rounded(r, px+pw-100, row_y(3)+10, 80, 28, 6, plug_on ? C_ACCENT : C_SIDEBAR);
        ui_textC(r, px+pw-60, row_y(3)+16, plug_on ? "Active" : "Idle", C_TEXT_PRI, fXS);
    }
    else if (settings_active_tab==10) { // Bluetooth
        ui_text(r, px, py-8, "Bluetooth Options", C_TEXT_PRI, fLG);
        ui_line(r, px, py+16, px+pw, py+16, C_BORDER);

        row_label(row_y(1), "Bluetooth Controller");
        ui_toggle(r, px+pw-60, row_y(1)+14, bluetooth_enabled);

        if (bluetooth_enabled) {
            row_label(row_y(2), "Paired Devices:");
            for (size_t d=0; d<bluetooth_devices.size(); ++d) {
                int ry = row_y(3) + d*36;
                ui_text(r, px+12, ry, bluetooth_devices[d].c_str(), C_TEXT_PRI, fSM);
                ui_textR(r, px+pw-20, ry, "Connected", C_GREEN, fXS);
            }
        } else {
            ui_text(r, px, row_y(2), "Bluetooth is turned off.", C_TEXT_TER, fSM);
        }
    }
}

// ── Control Panel (window 5) ─────────────────────────────────
static void render_control_panel(SDL_Renderer *r, int wx, int wy, int ww, int wh) {
    SDL_Rect ca = content_area(wx, wy, ww, wh);
    ui_rect(r, ca.x, ca.y, ca.w, ca.h, C_WIN_BG);
    int px=ca.x+16, py=ca.y+16, cw2=ca.w/2-24;

    // Left column — System
    ui_rounded(r, px, py, cw2, ca.h-32, 10, C_SIDEBAR);
    ui_border(r, px, py, cw2, ca.h-32, C_BORDER);
    ui_text(r, px+16, py+12, "SYSTEM", C_TEXT_TER, fXS);

    // Sensor Logger
    ui_text(r, px+16, py+40, "Sensor Logger", C_TEXT_PRI, fSM);
    ui_toggle(r, px+cw2-68, py+34, sensor_logging_enabled);

    // Lock
    ui_text(r, px+16, py+88, "Lock Screen", C_TEXT_PRI, fSM);
    ui_btn(r, px+16, py+108, 100, 30, "Lock Now", 0xFF443A30, C_RED_ERR, fSM);

    // Browser install
    ui_text(r, px+16, py+156, "Chrome Browser", C_TEXT_PRI, fSM);
    bool inst = apt_installed_packages["chrome"];
    ui_btn(r, px+16, py+176, 100, 30, inst?"Remove":"Install",
           inst?0xFF443A30:C_ACCENT_DIM, inst?C_RED_ERR:C_ACCENT, fSM);

    // Right column — Firewall
    int rx2=ca.x+ca.w/2+8;
    ui_rounded(r, rx2, py, cw2, ca.h-32, 10, C_SIDEBAR);
    ui_border(r, rx2, py, cw2, ca.h-32, C_BORDER);
    ui_text(r, rx2+16, py+12, "FIREWALL", C_TEXT_TER, fXS);

    ui_text(r, rx2+16, py+40, "Incoming traffic", C_TEXT_PRI, fSM);
    ui_toggle(r, rx2+cw2-68, py+34, firewall_allow_incoming);

    struct { const char *n; const char *k; } rules[]=
        {{"Terminal","Terminal"},{"Browser","Browser"},{"App Store","App Store"}};
    int ry2=py+82;
    for (auto &rl: rules) {
        ui_text(r, rx2+16, ry2, rl.n, C_TEXT_PRI, fSM);
        bool allowed=firewall_rules[rl.k];
        ui_rounded(r, rx2+cw2-70, ry2-4, 60, 24, 12,
                   allowed?0x30D15830:0xFF443A30);
        ui_text(r, rx2+cw2-58, ry2+2, allowed?"Allow":"Block",
                allowed?C_GREEN:C_RED_ERR, fXS);
        ry2 += 42;
    }
}

// ── Text Editor (window 6) ───────────────────────────────────
static void render_editor(SDL_Renderer *r, int wx, int wy, int ww, int wh) {
    SDL_Rect ca = content_area(wx, wy, ww, wh);
    ui_rect(r, ca.x, ca.y, ca.w, ca.h, 0x0D1117FF);

    // Editor menubar
    int mbh = 32;
    ui_rect(r, ca.x, ca.y, ca.w, mbh, 0x161820FF);
    ui_line(r, ca.x, ca.y+mbh, ca.x+ca.w, ca.y+mbh, C_BORDER);
    struct {const char*label; int x;} menus[]={{"File",16},{"Edit",60},{"View",104},{nullptr,0}};
    for (auto &m: menus) {
        if(!m.label) break;
        ui_text(r, ca.x+m.x, ca.y+8, m.label, C_TEXT_SEC, fSM);
    }
    // File name badge
    std::string efname = editor_current_file.empty() ? "Untitled" :
        editor_current_file.substr(editor_current_file.rfind('/')+1);
    ui_textC(r, ca.x+ca.w/2, ca.y+8, efname.c_str(), C_TEXT_TER, fSM);

    // IDE Sidebar (File Tree)
    int IDE_SW = 130;
    ui_rect(r, ca.x, ca.y+mbh, IDE_SW, ca.h-mbh, 0x161820FF);
    ui_line(r, ca.x+IDE_SW, ca.y+mbh, ca.x+IDE_SW, ca.y+ca.h, C_BORDER);
    ui_text(r, ca.x+10, ca.y+mbh+8, "WORKSPACE", C_TEXT_TER, fXS);
    
    std::vector<std::string> workspace_files = {
        "src/main.cpp",
        "src/graphics.cpp",
        "src/state.cpp",
        "include/state.h",
        "Makefile.standalone",
        "README.md"
    };
    for (size_t f_idx = 0; f_idx < workspace_files.size(); ++f_idx) {
        int fy = ca.y+mbh+28+f_idx*22;
        bool is_cur = (editor_current_file == workspace_files[f_idx]);
        ui_text(r, ca.x+12, fy, workspace_files[f_idx].c_str(), is_cur ? C_ACCENT : C_TEXT_SEC, fXS);
    }

    // IDE Bottom Terminal Panel
    int IDE_TH = 120;
    int ty_start = ca.y + ca.h - IDE_TH;
    ui_rect(r, ca.x+IDE_SW+1, ty_start, ca.w-IDE_SW-1, IDE_TH, 0x0A0D14FF);
    ui_line(r, ca.x+IDE_SW+1, ty_start, ca.x+ca.w, ty_start, C_BORDER);
    if (editor_terminal_focused) {
        ui_border(r, ca.x+IDE_SW+1, ty_start, ca.w-IDE_SW-1, IDE_TH, get_accent_color(), 1);
    }
    
    ui_rect(r, ca.x+IDE_SW+1, ty_start, ca.w-IDE_SW-1, 24, 0x161820FF);
    ui_text(r, ca.x+IDE_SW+10, ty_start+4, "TERMINAL", C_TEXT_SEC, fXS);
    
    for (int t = 0; t < 3; ++t) {
        int tx = ca.x + IDE_SW + 120 + t * 80;
        bool active = (active_term_pane_idx == t);
        ui_rounded(r, tx, ty_start+2, 70, 20, 4, active ? get_accent_color() : 0x2A2A2EFF);
        char t_lbl[16]; snprintf(t_lbl, sizeof(t_lbl), "Pane %d", t+1);
        ui_textC(r, tx+35, ty_start+6, t_lbl, active ? C_TEXT_PRI : C_TEXT_SEC, fXS);
    }
    
    int term_y = ty_start+28;
    int term_h = 14;
    const auto &logs = term_panes[active_term_pane_idx].logs;
    size_t log_start = logs.size() > 5 ? logs.size() - 5 : 0;
    for (size_t l = log_start; l < logs.size() && term_y < ca.y+ca.h-28; ++l) {
        ui_text(r, ca.x+IDE_SW+10, term_y, logs[l].c_str(), 0xAEAEB2FF, fMono);
        term_y += term_h;
    }
    
    std::string prompt_str = "$ " + term_panes[active_term_pane_idx].input_buffer;
    if (editor_terminal_focused && (gui_frame_counter/15)%2==0) {
        prompt_str += "│";
    }
    ui_text(r, ca.x+IDE_SW+10, ca.y+ca.h-20, prompt_str.c_str(), C_TEXT_PRI, fMono);

    // Line numbers gutter
    int gutter_x = ca.x + IDE_SW + 1;
    const int gutter = 36;
    ui_rect(r, gutter_x, ca.y+mbh, gutter, ca.h-mbh-IDE_TH, 0x111318FF);
    ui_line(r, gutter_x+gutter, ca.y+mbh, gutter_x+gutter, ty_start, C_BORDER);

    // Text content
    int ch2=0; if (fMono) ch2=TTF_FontLineSkip(fMono); if(!ch2) ch2=16;
    int line_y=ca.y+mbh+8;
    std::vector<std::string> lines;
    std::string cur_line;
    int cur_l=0, cur_c=0;
    for (int ci=0; ci<=(int)editor_text_content.size(); ++ci) {
        if (ci==editor_cursor_pos) { cur_l=lines.size(); cur_c=cur_line.size(); }
        if (ci==(int)editor_text_content.size()) { lines.push_back(cur_line); break; }
        if (editor_text_content[ci]=='\n') { lines.push_back(cur_line); cur_line=""; }
        else cur_line += editor_text_content[ci];
    }
    for (size_t l=0; l<lines.size() && line_y<ty_start-ch2; ++l) {
        // Line number
        char lnum[8]; snprintf(lnum,sizeof(lnum),"%zu",(l+1));
        ui_textR(r, gutter_x+gutter-6, line_y, lnum, C_TEXT_TER, fXS);
        // Code text
        ui_text(r, gutter_x+gutter+10, line_y, lines[l].c_str(), C_TEXT_PRI, fMono);
        // Cursor
        if (l==(size_t)cur_l && (gui_frame_counter/15)%2==0) {
            int cw3=0,ch3=0; TTF_SizeUTF8(fMono, lines[l].substr(0,cur_c).c_str(), &cw3, &ch3);
            ui_rect(r, gutter_x+gutter+10+cw3, line_y, 2, ch2, C_ACCENT);
        }
        line_y += ch2+1;
    }
}

// ── App Store (window 7) ─────────────────────────────────────
static void render_appstore(SDL_Renderer *r, int wx, int wy, int ww, int wh) {
    SDL_Rect ca = content_area(wx, wy, ww, wh);
    ui_rect(r, ca.x, ca.y, ca.w, ca.h, C_WIN_BG);

    const int SW=150;
    ui_rect(r, ca.x, ca.y, SW, ca.h, C_SIDEBAR);
    ui_line(r, ca.x+SW, ca.y, ca.x+SW, ca.y+ca.h, C_BORDER);

    const char *cats[]={"All Apps","Development","Utilities","Games","Themes"};
    for (int ct=0; ct<5; ++ct)
        ui_tab_item(r, ca.x, ca.y+8+ct*38, SW, 36, cats[ct], ct==appstore_active_category, fSM);

    // App list
    int ay=ca.y+12;
    for (size_t a=0; a<appstore_apps.size() && ay<ca.y+ca.h-100; ++a) {
        const auto &app=appstore_apps[a];
        bool show=false;
        if (appstore_active_category==0) show=true;
        else if (appstore_active_category==1 && app.category == "Development") show = true;
        else if (appstore_active_category==2 && app.category == "Utilities") show = true;
        else if (appstore_active_category==3 && app.category == "Games") show = true;
        else if (appstore_active_category==4 && app.category == "Themes") show = true;
        if (!show) continue;

        int rh=80;
        ui_rounded(r, ca.x+SW+8, ay, ca.w-SW-16, rh, 8, C_SIDEBAR);
        ui_border(r, ca.x+SW+8, ay, ca.w-SW-16, rh, C_BORDER);

        // App icon
        int ic_x=ca.x+SW+20, ic_y=ay+12;
        ui_rounded(r, ic_x, ic_y, 56, 56, 12, app.color);
        ui_textC(r, ic_x+28, ic_y+16, app.name.substr(0,1).c_str(), 0xFFFFFFFF, fLG);

        // App info
        ui_text(r, ic_x+68, ay+12, app.name.c_str(), C_TEXT_PRI, fMD);
        std::string ddesc=app.desc; if(ddesc.size()>50) ddesc=ddesc.substr(0,48)+"..";
        ui_text(r, ic_x+68, ay+32, ddesc.c_str(), C_TEXT_SEC, fXS);

        // Stars
        std::string stars; for(int s=0;s<app.rating;s++) stars+="★";
        ui_text(r, ic_x+68, ay+50, stars.c_str(), 0xFFD60AFF, fXS);
        ui_text(r, ic_x+68+app.rating*12, ay+50, ("  "+app.size).c_str(), C_TEXT_TER, fXS);

        // Install button
        bool is_inst=apt_installed_packages[app.package_name];
        int bw=100;
        if (is_inst) {
            ui_btn(r, ca.x+ca.w-bw-20, ay+24, bw, 32, "Remove",
                   0xFF443A30, C_RED_ERR, fSM);
        } else if (appstore_installing_idx==(int)a) {
            ui_progress(r, ca.x+ca.w-bw-20, ay+32, bw, 12,
                        appstore_progress/100.f, C_ACCENT);
            char ps[16]; snprintf(ps,sizeof(ps),"%d%%",appstore_progress);
            ui_textC(r, ca.x+ca.w-bw/2-20, ay+50, ps, C_ACCENT, fXS);
        } else {
            ui_btn(r, ca.x+ca.w-bw-20, ay+24, bw, 32, "Install",
                   C_ACCENT, C_TEXT_PRI, fSM);
        }
        ay += rh+8;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  I. WALLPAPER
// ─────────────────────────────────────────────────────────────────────────────
static void draw_wallpaper(SDL_Renderer *r, int W, int H) {
    // Multi-stop gradient
    uint32_t wp_tops[]={0x0D1117FF, 0x080A12FF, 0x1A0D22FF, 0x051208FF};
    uint32_t wp_bots[]={0x1A2236FF, 0x0D1A30FF, 0x2A1030FF, 0x0A1F0AFF};
    int wi=settings_wallpaper_idx&3;
    ui_gradient(r, 0, 0, W, H, wp_tops[wi], wp_bots[wi]);

    // Aurora shimmer (animated horizontal bands)
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    int base_phase = gui_frame_counter;
    for (int a=0; a<3; ++a) {
        float amp=40.f, freq=2.5f+a*1.2f;
        int cy2=(int)(H*0.3f + a*H*0.15f);
        uint32_t aurora_cols[]={0x00FF8820,0x0A84FF18,0xBF5AF218};
        uint8_t ar=(aurora_cols[a]>>24)&0xFF, ag=(aurora_cols[a]>>16)&0xFF,
                ab=(aurora_cols[a]>>8)&0xFF;
        for (int y=-20; y<20; ++y) {
            float fade=1.f-fabsf((float)y/20.f);
            int wave_y=(int)(cy2 + sinf((float)base_phase*0.02f + a*2.f)*amp + y);
            if (wave_y<0||wave_y>=H-TASKBAR_H) continue;
            SDL_SetRenderDrawColor(r, ar, ag, ab, (uint8_t)(fade*30));
            SDL_RenderDrawLine(r, 0, wave_y, W, wave_y);
        }
    }

    // Star field
    for (int s=0; s<80; ++s) {
        int sx=(s*73+19+s*s)%W;
        int sy=(s*37+5+s*11)%std::max(1,H-TASKBAR_H-60);
        int twinkle=(gui_frame_counter/3+s*7)%10;
        uint8_t brightness=(twinkle<5)?180:80;
        SDL_SetRenderDrawColor(r, brightness, brightness, brightness+20, brightness);
        SDL_RenderDrawPoint(r, sx, sy);
        if (s%3==0) {
            SDL_RenderDrawPoint(r, sx+1, sy);
            SDL_RenderDrawPoint(r, sx, sy+1);
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  J. LOGIN SCREEN
// ─────────────────────────────────────────────────────────────────────────────
static void draw_login(SDL_Renderer *r, int W, int H) {
    draw_wallpaper(r, W, H);

    // Dark overlay
    ui_rect(r, 0, 0, W, H, 0x00000060);

    // Login card
    int cw2=400, ch2=460;
    int cx2=W/2-cw2/2, cy2=H/2-ch2/2;

    // Card shadow
    ui_shadow(r, cx2, cy2, cw2, ch2);

    // Card background (frosted glass)
    ui_rounded(r, cx2, cy2, cw2, ch2, 16, 0x1A1F2CF0);
    ui_border(r, cx2, cy2, cw2, ch2, 0x3A3F50FF, 1);

    // Top gradient stripe
    ui_gradient(r, cx2, cy2, cw2, 4, C_ACCENT, 0x0A84FF00);

    // VidyaOS logo text
    ui_textC(r, W/2, cy2+28, "VidyaOS", C_TEXT_PRI, fXL);
    ui_textC(r, W/2, cy2+60, "Sign in to continue", C_TEXT_TER, fSM);

    // Avatar circle
    int av_r=42, avy=cy2+106;
    std::string uname=(login_selected_user_idx==0)?"root":"shri";
    ui_circle(r, W/2, avy, av_r, get_accent_color());
    ui_circle(r, W/2, avy, av_r-3, 0x1A1F2CFF);
    // Initial letter
    std::string initial=uname.substr(0,1); std::transform(initial.begin(),initial.end(),initial.begin(),::toupper);
    ui_textC(r, W/2, avy-18, initial.c_str(), C_TEXT_PRI, fXXL);

    // User selection tabs
    int tabw=100, tabh=28;
    for (int u=0; u<2; ++u) {
        const char *un=(u==0)?"root":"shri";
        int tx=W/2+(u==0?-tabw-4:4);
        bool sel=(login_selected_user_idx==u);
        ui_rounded(r, tx, cy2+168, tabw, tabh, 8, sel?C_ACCENT:0x2A2F3CFF);
        ui_textC(r, tx+tabw/2, cy2+174, un, sel?C_TEXT_PRI:C_TEXT_SEC, fSM);
    }

    // Display current user
    ui_textC(r, W/2, cy2+210, uname.c_str(), C_TEXT_PRI, fLG);

    // Password label
    ui_text(r, cx2+40, cy2+248, "Password", C_TEXT_TER, fSM);

    // Password field (pill)
    int fldx=cx2+36, fldy=cy2+268, fldw=cw2-72, fldh=44;
    ui_rounded(r, fldx, fldy, fldw, fldh, fldh/2, C_INPUT_BG);
    ui_border(r, fldx, fldy, fldw, fldh, C_BORDER, 1);
    // Masked password
    std::string masked; for(size_t i=0;i<login_pass_buffer.size();i++) masked+="● ";
    if((gui_frame_counter/15)%2==0) masked+="│";
    if (masked.empty()) ui_textC(r, fldx+fldw/2, fldy+12, "Enter password", C_TEXT_TER, fSM);
    else ui_textC(r, fldx+fldw/2, fldy+12, masked.c_str(), C_TEXT_SEC, fMD);

    // Sign In button
    int btnx=cx2+36, btny=cy2+332, btnw=cw2-72, btnh=44;
    ui_rounded(r, btnx, btny, btnw, btnh, btnh/2, get_accent_color());
    ui_textC(r, btnx+btnw/2, btny+12, "Sign In", C_TEXT_PRI, fMD);

    // Hint
    ui_textC(r, W/2, cy2+400, "shri / vidya123  ·  root / root123", C_TEXT_TER, fXS);

    // Version badge
    ui_textC(r, W/2, cy2+ch2-16, "VidyaOS v3.0 Phase 4", C_TEXT_TER, fXS);
}

// ─────────────────────────────────────────────────────────────────────────────
//  K. TASKBAR
// ─────────────────────────────────────────────────────────────────────────────
static void draw_taskbar(SDL_Renderer *r, int W, int H) {
    int ty = H - TASKBAR_H;

    // Taskbar background (frosted glass)
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    ui_rect(r, 0, ty, W, TASKBAR_H, C_TASKBAR);

    // Top border (accent line)
    uint32_t accent = get_accent_color();
    set_col(r, (accent & 0xFFFFFF00) | 0x60);
    SDL_RenderDrawLine(r, 0, ty, W, ty);
    set_col(r, (accent & 0xFFFFFF00) | 0x20);
    SDL_RenderDrawLine(r, 0, ty + 1, W, ty + 1);

    // ── Start / VidyaOS button (left) ───────────────
    int sbx = 8, sby = ty + 8, sbw = 92, sbh = 32;
    bool sb_hov = (mouse_x >= sbx && mouse_x < sbx + sbw && mouse_y >= sby && mouse_y < sby + sbh);
    ui_rounded(r, sbx, sby, sbw, sbh, sbh / 2,
               start_menu_open ? accent : (sb_hov ? C_ACCENT_DIM : 0x2A2F3CFF));
    ui_textC(r, sbx + sbw / 2, sby + 8, "⊞ VidyaOS", start_menu_open ? C_TEXT_PRI : C_TEXT_SEC, fXS);

    // ── Workspace switcher ───────────────────────────
    for (int ws = 0; ws < 4; ++ws) {
        int sx = 108 + ws * 22;
        bool sel = (current_workspace == ws);
        ui_rounded(r, sx, ty + 16, 14, 14, 7, sel ? accent : 0x3A3A3EFF);
    }

    // ── Centered Open-window dock (center) ────────────
    int total_w = 8 * 32 + 7 * 12;
    int start_x = W / 2 - total_w / 2;
    for (int i = 0; i < 8; ++i) {
        int ix = start_x + i * 44;
        bool is_open = windows[i].open && windows[i].workspace == current_workspace;
        bool focused = is_open && is_focused_window(i);

        if (i == 3 && !apt_installed_packages["chrome"]) {
            // Chrome not installed: draw gray/translucent with lock
            draw_icon(r, ix, ty + 8, 32, i);
            ui_rounded(r, ix, ty + 8, 32, 32, 7, 0x000000B0); // lock tint
            draw_text(r, ix + 12, ty + 16, "🔒", 1, 0xFFFFFF80);
        } else {
            draw_icon(r, ix, ty + 8, 32, i);
            
            // Draw open indicator dots underneath
            if (is_open) {
                if (focused) {
                    // Accent colored pill under icon
                    ui_rounded(r, ix + 10, ty + 43, 12, 3, 1, accent);
                } else {
                    // Small gray dot under icon
                    ui_circle(r, ix + 16, ty + 44, 2, C_TEXT_SEC);
                }
            }
        }
    }

    // ── System tray (right) ─────────────────────────
    int sx2 = W - 220;

    // WiFi and Volume Pill
    ui_rounded(r, sx2, ty + 6, 84, 36, 18, 0x2A2F3C80);
    ui_border(r, sx2, ty + 6, 84, 36, C_BORDER);

    // WiFi Icon (inside WiFi portion of pill, x: sx2+4..sx2+24)
    int wx2 = sx2 + 6;
    if (net_state.wifiOn) {
        for (int b = 0; b < 4; ++b) {
            uint32_t bc = (net_state.signalStrength > b) ? accent : 0x3A3A3EFF;
            ui_rect(r, wx2 + b * 4, ty + TASKBAR_H - 12 - b * 3, 3, 3 + b * 3, bc);
        }
    } else {
        set_col(r, C_RED_ERR);
        SDL_RenderDrawLine(r, wx2, ty + 14, wx2 + 12, ty + 26);
        SDL_RenderDrawLine(r, wx2 + 12, ty + 14, wx2, ty + 26);
    }

    // Volume Icon (inside Volume portion of pill, x: sx2+28..sx2+80)
    int vx = sx2 + 28;
    char vs[12];
    if (system_muted) {
        snprintf(vs, sizeof(vs), "🔇");
    } else {
        snprintf(vs, sizeof(vs), "🔊 %d%%", system_volume);
    }
    ui_text(r, vx, ty + 15, vs, C_TEXT_SEC, fXS);

    // Notification bell Pill (x: sx2+88..sx2+124)
    ui_rounded(r, sx2 + 88, ty + 6, 36, 36, 18, 0x2A2F3C80);
    ui_border(r, sx2 + 88, ty + 6, 36, 36, C_BORDER);

    int nx2 = sx2 + 88 + 18;
    bool has_notifs = !notifications.empty();
    ui_textC(r, nx2, ty + 15, "🔔", has_notifs ? C_AMBER : C_TEXT_TER, fXS);
    if (has_notifs) {
        char nb[8];
        snprintf(nb, sizeof(nb), "%zu", std::min((size_t)9, notifications.size()));
        ui_circle(r, nx2 + 8, ty + 12, 6, C_RED_ERR);
        ui_textC(r, nx2 + 8, ty + 9, nb, C_TEXT_PRI, fXS);
    }

    // Clock Pill (x: W-106..W-8)
    ui_rounded(r, W - 106, ty + 6, 98, 36, 18, 0x2A2F3C80);
    ui_border(r, W - 106, ty + 6, 98, 36, C_BORDER);

    time_t t = time(nullptr);
    struct tm *tm_info = localtime(&t);
    char time_buf[16], date_buf[16];
    strftime(time_buf, sizeof(time_buf), "%H:%M", tm_info);
    strftime(date_buf, sizeof(date_buf), "%d %b", tm_info);
    ui_textC(r, W - 57, ty + 10, time_buf, C_TEXT_PRI, fXS);
    ui_textC(r, W - 57, ty + 24, date_buf, C_TEXT_TER, fXS);
}

// ─────────────────────────────────────────────────────────────────────────────
//  L. START MENU
// ─────────────────────────────────────────────────────────────────────────────
static void draw_start_menu(SDL_Renderer *r, int W, int H) {
    int mw=360, mh=440;
    int mx=16, my=H-TASKBAR_H-mh-8;

    ui_shadow(r, mx, my, mw, mh);
    ui_rounded(r, mx, my, mw, mh, 12, 0x1A1F2CF4);
    ui_border(r, mx, my, mw, mh, C_BORDER);
    ui_gradient(r, mx, my, mw, 3, C_ACCENT, 0x0A84FF00);

    // Search bar
    ui_rounded(r, mx+12, my+16, mw-24, 36, 18, 0x2A2F3CFF);
    ui_text(r, mx+24, my+26, "🔍  Search apps…", C_TEXT_TER, fSM);

    // Pinned heading
    ui_text(r, mx+16, my+68, "PINNED", C_TEXT_TER, fXS);

    // App list
    struct {int w; const char *name;} apps[]={
        {0,"Terminal"},{1,"File Manager"},{2,"System Monitor"},
        {3,"Browser"},{4,"Settings"},{5,"Control Panel"},
        {6,"Text Editor"},{7,"App Store"},{-1,nullptr}
    };
    int ay=my+88;
    for (int a=0; apps[a].name; ++a) {
        bool hov=(mouse_x>=mx+8&&mouse_x<mx+mw-8&&mouse_y>=ay-2&&mouse_y<ay+30);
        if (hov) ui_rounded(r, mx+8, ay-2, mw-16, 32, 6, C_HOVER);
        // Mini icon using vector scaled draw_icon
        draw_icon(r, mx+16, ay+2, 24, apps[a].w);
        ui_text(r, mx+52, ay+6, apps[a].name, C_TEXT_PRI, fSM);
        ay += 34;
    }

    // Power options at bottom
    ui_line(r, mx+8, my+mh-48, mx+mw-8, my+mh-48, C_BORDER);
    ui_text(r, mx+20, my+mh-36, "🔒 Lock", C_TEXT_SEC, fSM);
    int sw2=0,sh2=0; TTF_SizeUTF8(fSM,"🔒 Lock",&sw2,&sh2);
    ui_text(r, mx+sw2+36, my+mh-36, "⏻ Shutdown", C_RED_ERR, fSM);
}

// ─────────────────────────────────────────────────────────────────────────────
//  M. NOTIFICATIONS POPUP
// ─────────────────────────────────────────────────────────────────────────────
static void draw_notifications(SDL_Renderer *r, int W, int H) {
    int pw=320, ph=std::min(380, 80+(int)notifications.size()*64);
    int px=W-pw-12, py=H-TASKBAR_H-ph-8;

    ui_shadow(r, px, py, pw, ph);
    ui_rounded(r, px, py, pw, ph, 12, 0x1A1F2CF4);
    ui_border(r, px, py, pw, ph, C_BORDER);
    ui_gradient(r, px, py, pw, 3, C_AMBER, 0xFFD60A00);

    ui_text(r, px+16, py+14, "Notifications", C_TEXT_PRI, fMD);
    ui_btn(r, px+pw-72, py+10, 60, 24, "Clear", 0x3A3A3EFF, C_TEXT_SEC, fXS);
    ui_line(r, px, py+42, px+pw, py+42, C_BORDER);

    if (notifications.empty()) {
        ui_textC(r, px+pw/2, py+ph/2-8, "No notifications", C_TEXT_TER, fMD);
        return;
    }
    int ny=py+50;
    for (size_t n=0; n<notifications.size()&&ny<py+ph-16; ++n) {
        ui_circle(r, px+24, ny+14, 5, C_AMBER);
        std::string msg=notifications[n].message;
        if (msg.size()>38) msg=msg.substr(0,36)+"…";
        ui_text(r, px+40, ny+4, msg.c_str(), C_TEXT_PRI, fSM);
        // Timestamp
        time_t ts=notifications[n].timestamp;
        struct tm *tm_i=localtime(&ts);
        char tbuf[16]; strftime(tbuf,sizeof(tbuf),"%H:%M",tm_i);
        ui_textR(r, px+pw-12, ny+4, tbuf, C_TEXT_TER, fXS);
        ui_line(r, px+8, ny+30, px+pw-8, ny+30, 0x2A2F3CFF);
        ny += 34;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  N. CONTEXT MENU
// ─────────────────────────────────────────────────────────────────────────────
static void draw_context_menu(SDL_Renderer *r) {
    int cx2=file_manager_context_menu_x, cy2=file_manager_context_menu_y;
    int mw=180, mh=180;

    ui_shadow(r, cx2, cy2, mw, mh);
    ui_rounded(r, cx2, cy2, mw, mh, 8, 0x1E2230FF);
    ui_border(r, cx2, cy2, mw, mh, C_BORDER);

    const char *opts[]={"Open","Copy","Paste","Delete","Rename"};
    uint32_t colors[]={C_TEXT_PRI,C_TEXT_PRI,C_TEXT_PRI,C_RED_ERR,C_TEXT_PRI};
    for (int o=0; o<5; ++o) {
        int oy=cy2+8+o*32;
        bool hov=(mouse_x>=cx2+4&&mouse_x<cx2+mw-4&&mouse_y>=oy&&mouse_y<oy+30);
        if (hov) ui_rounded(r, cx2+4, oy, mw-8, 30, 6, C_HOVER);
        if (o==3) ui_line(r, cx2+12, oy-4, cx2+mw-12, oy-4, C_BORDER);
        ui_text(r, cx2+20, oy+7, opts[o], colors[o], fSM);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  O. DESKTOP ICONS LAYOUT
// ─────────────────────────────────────────────────────────────────────────────
static void draw_desktop_icons(SDL_Renderer *r) {
    struct {int col; int row; int app_id; const char *label;} icons[]={
        {0,0,0,"Terminal"},
        {0,1,1,"Files"},
        {0,2,2,"Monitor"},
        {0,3,6,"Editor"},
        {1,0,4,"Settings"},
        {1,1,5,"Control"},
        {1,2,3,"Browser"},
        {1,3,7,"App Store"},
    };

    for (auto &ic: icons) {
        int ix = ICO_START_X + ic.col * ICO_STEP_X;
        int iy = ICO_START_Y + ic.row * ICO_STEP_Y;
        draw_icon(r, ix, iy, ICON_SZ, ic.app_id);
        ui_textC(r, ix+ICON_SZ/2, iy+ICON_SZ+4, ic.label, C_TEXT_PRI, fXS);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  P. MAIN DRAW FUNCTION
// ─────────────────────────────────────────────────────────────────────────────
void draw_desktop(SDL_Renderer *r, int W, int H) {
    if (!r) return;
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
    SDL_RenderClear(r);

    // ── Login screen ────────────────────────────────────
    if (!system_logged_in) {
        draw_login(r, W, H);
        return;
    }

    // ── Desktop wallpaper ───────────────────────────────
    draw_wallpaper(r, W, H);

    // ── HUD widget (top-right) ──────────────────────────
    int hx=W-220, hy=12;
    ui_rounded(r, hx, hy, 208, 76, 10, 0x1A1F2CA0);
    ui_border(r, hx, hy, 208, 76, C_BORDER_LT);
    char hs[64];
    snprintf(hs,sizeof(hs),"CPU %3d%%  RAM %dMB", telemetry_cpu_usage, telemetry_ram_usage);
    ui_text(r, hx+12, hy+10, hs, C_TEXT_SEC, fXS);
    snprintf(hs,sizeof(hs),"TEMP %d°C  NET %s", current_sensor_value,
             net_state.wifiOn?net_state.ssid.c_str():"Off");
    ui_text(r, hx+12, hy+28, hs, C_TEXT_SEC, fXS);
    ui_progress(r, hx+12, hy+52, 184, 6, telemetry_cpu_usage/100.f, get_accent_color());

    // ── Desktop icons ────────────────────────────────────
    draw_desktop_icons(r);

    // ── Snap Preview Helper ─────────────────────────────
    if (dragging_window_index != -1 && !is_tiling_mode) {
        int sx = -1, sy = -1, sw = -1, sh = -1;
        if (mouse_x < 50 && mouse_y < 50) {
            sx = 8; sy = 8; sw = W/2 - 12; sh = (H - TASKBAR_H)/2 - 12;
        } else if (mouse_x > W - 50 && mouse_y < 50) {
            sx = W/2 + 4; sy = 8; sw = W/2 - 12; sh = (H - TASKBAR_H)/2 - 12;
        } else if (mouse_x < 50 && mouse_y > H - TASKBAR_H - 50) {
            sx = 8; sy = (H - TASKBAR_H)/2 + 4; sw = W/2 - 12; sh = (H - TASKBAR_H)/2 - 12;
        } else if (mouse_x > W - 50 && mouse_y > H - TASKBAR_H - 50) {
            sx = W/2 + 4; sy = (H - TASKBAR_H)/2 + 4; sw = W/2 - 12; sh = (H - TASKBAR_H)/2 - 12;
        } else if (mouse_x < 30) {
            sx = 8; sy = 8; sw = W/2 - 12; sh = H - TASKBAR_H - 16;
        } else if (mouse_x > W - 30) {
            sx = W/2 + 4; sy = 8; sw = W/2 - 12; sh = H - TASKBAR_H - 16;
        } else if (mouse_y < 30) {
            sx = 8; sy = 8; sw = W - 16; sh = H - TASKBAR_H - 16;
        }
        if (sx != -1) {
            SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
            uint32_t acc = get_accent_color();
            SDL_SetRenderDrawColor(r, (acc>>24)&0xFF, (acc>>16)&0xFF, (acc>>8)&0xFF, 0x40);
            SDL_Rect guide = {sx, sy, sw, sh};
            SDL_RenderFillRect(r, &guide);
            SDL_SetRenderDrawColor(r, (acc>>24)&0xFF, (acc>>16)&0xFF, (acc>>8)&0xFF, 0xBB);
            SDL_RenderDrawRect(r, &guide);
        }
    }

    // ── Windows (Z-order, bottom to top) ─────────────────
    for (int z=0; z<8; ++z) {
        int i=window_z_order[z];
        if (!windows[i].open||windows[i].minimized||windows[i].workspace!=current_workspace) continue;
        int wx=windows[i].x, wy=windows[i].y, ww=windows[i].w, wh=windows[i].h;

        if (is_tiling_mode) {
            int open_count = 0;
            int index_in_tiled = -1;
            for (int w_idx = 0; w_idx < 8; ++w_idx) {
                if (windows[w_idx].open && !windows[w_idx].minimized && windows[w_idx].workspace == current_workspace) {
                    if (w_idx == i) {
                        index_in_tiled = open_count;
                    }
                    open_count++;
                }
            }
            if (index_in_tiled != -1) {
                get_tiled_bounds(open_count, index_in_tiled, W, H, wx, wy, ww, wh);
            }
        }

        // Draw window frame
        draw_window_chrome(r, i, W, H, wx, wy, ww, wh);

        // Content (clipped to content area)
        SDL_Rect clip={wx+1, wy+TITLEBAR_H+1, ww-2, wh-TITLEBAR_H-1};
        SDL_RenderSetClipRect(r, &clip);

        switch (i) {
        case 0: render_terminal(r, wx, wy, ww, wh); break;
        case 1: render_files(r,    wx, wy, ww, wh); break;
        case 2: render_monitor(r,  wx, wy, ww, wh); break;
        case 3: render_browser(r,  wx, wy, ww, wh); break;
        case 4: render_settings(r, wx, wy, ww, wh); break;
        case 5: render_control_panel(r, wx, wy, ww, wh); break;
        case 6: render_editor(r,   wx, wy, ww, wh); break;
        case 7: render_appstore(r, wx, wy, ww, wh); break;
        default: break;
        }
        SDL_RenderSetClipRect(r, nullptr);
    }

    // ── Start menu ──────────────────────────────────────
    if (start_menu_open) draw_start_menu(r, W, H);

    // ── Taskbar ─────────────────────────────────────────
    draw_taskbar(r, W, H);

    // ── Notifications popup ─────────────────────────────
    if (notifications_popup_open) draw_notifications(r, W, H);

    // ── Context menu ────────────────────────────────────
    if (file_manager_context_menu_open) draw_context_menu(r);

    // ── Developer Mode HUD ──────────────────────────────
    if (dev_mode_enabled) {
        int dx = W - 260, dy = H - 200 - TASKBAR_H;
        ui_rounded(r, dx, dy, 240, 180, 8, 0x000000CC);
        ui_border(r, dx, dy, 240, 180, 0xFFD60AFF);
        ui_text(r, dx+10, dy+10, "🛠 Developer Mode HUD", 0xFFD60AFF, fSM);
        ui_line(r, dx+10, dy+32, dx+230, dy+32, 0x444444FF);
        
        char buf[64];
        static uint32_t last_time = 0;
        static int frames = 0, last_fps = 0;
        frames++;
        uint32_t ticks = SDL_GetTicks();
        if (ticks > last_time + 1000) {
            last_fps = frames;
            frames = 0;
            last_time = ticks;
        }
        snprintf(buf, sizeof(buf), "FPS: %d", last_fps);
        ui_text(r, dx+10, dy+40, buf, C_TEXT_PRI, fXS);
        
        snprintf(buf, sizeof(buf), "Mouse: %d, %d", mouse_x, mouse_y);
        ui_text(r, dx+10, dy+60, buf, C_TEXT_PRI, fXS);
        
        snprintf(buf, sizeof(buf), "Resolution: %dx%d", W, H);
        ui_text(r, dx+10, dy+80, buf, C_TEXT_PRI, fXS);
        
        snprintf(buf, sizeof(buf), "Scale: %dx", current_window_scale);
        ui_text(r, dx+10, dy+100, buf, C_TEXT_PRI, fXS);
        
        int focused = -1;
        for (int i = 0; i < 8; ++i) {
            if (is_focused_window(i)) { focused = i; break; }
        }
        snprintf(buf, sizeof(buf), "Focused Win: %d", focused);
        ui_text(r, dx+10, dy+120, buf, C_TEXT_PRI, fXS);
        
        snprintf(buf, sizeof(buf), "CPU: %d%% RAM: %dMB", telemetry_cpu_usage, telemetry_ram_usage);
        ui_text(r, dx+10, dy+140, buf, C_TEXT_PRI, fXS);
    }

    ++gui_frame_counter;
}
