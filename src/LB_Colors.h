#pragma once
//
// Legacy colour names for code that talks to gfx() directly.
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
// What is still here: the bare pre-1.6.5 names (BLACK, RED, ...), because
// GFX Library for Arduino 1.6.5 renamed them to RGB565_*. They are RGB565
// values, for the gfx() escape hatch only.
//
// !! Never mix the two !!
//
//       display.fillScreen(LB_BLACK);          // lb_color_t  - correct
//       display.gfx()->fillScreen(BLACK);      // uint16_t    - correct
//       display.gfx()->fillScreen(LB_BLACK);   // WRONG, silently truncates
//
// Define LB_NO_LEGACY_COLORS to suppress the bare names if they collide with
// something in your own code.

#include <Arduino_GFX_Library.h>

#ifndef LB_NO_LEGACY_COLORS
  #ifndef BLACK
    #define BLACK   RGB565_BLACK
    #define WHITE   RGB565_WHITE
    #define RED     RGB565_RED
    #define GREEN   RGB565_GREEN
    #define BLUE    RGB565_BLUE
    #define YELLOW  RGB565_YELLOW
    #define MAGENTA RGB565_MAGENTA
    #define CYAN    RGB565_CYAN
  #endif
  #ifndef LIME
    #define LIME    RGB565_LIME
  #endif
  #ifndef ORANGE
    #define ORANGE  RGB565_ORANGE
  #endif
#endif
