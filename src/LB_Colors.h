#pragma once
//
// Legacy colour names, as raw RGB565.
//
// !! The LB_* colour names are NOT here any more !!
//
//   LB_BLACK, LB_RED and the rest now come from Lonely Binary GFX as
//   `lb_color_t` constants, and they are what the drawing API takes:
//
//       display.fillScreen(LB_BLACK);          // right
//
//   This file used to #define those same names as RGB565 macros. Because a
//   macro is substituted before anything else, it silently replaced the GFX
//   constants and every colour came out wrong - red rendered as green - while
//   compiling cleanly and running happily. It took a byte-level comparison
//   against Arduino_GFX's own output to find. Do not reintroduce them.
//
// What is still here: the bare names (BLACK, RED, ...) as plain RGB565
// values, for code that handles raw pixels - pushImage(), framebuffer(), or
// LVGL. They used to come from Arduino_GFX (RGB565_BLACK and so on); the
// values are written out now because Arduino_GFX is no longer a dependency.
//
// !! Never mix the two !!
//
//       display.fillScreen(LB_BLACK);          // lb_color_t  - correct
//       framebuffer[i] = BLACK;                // uint16_t    - correct
//       framebuffer[i] = LB_BLACK;             // WRONG, silently truncates
//
// Define LB_NO_LEGACY_COLORS to suppress the bare names if they collide with
// something in your own code.

#ifndef LB_NO_LEGACY_COLORS
  #ifndef BLACK
    #define BLACK   0x0000
    #define WHITE   0xFFFF
    #define RED     0xF800
    #define GREEN   0x07E0
    #define BLUE    0x001F
    #define YELLOW  0xFFE0
    #define MAGENTA 0xF81F
    #define CYAN    0x07FF
  #endif
  #ifndef LIME
    #define LIME    0x07E0
  #endif
  #ifndef ORANGE
    #define ORANGE  0xFD20
  #endif
#endif
