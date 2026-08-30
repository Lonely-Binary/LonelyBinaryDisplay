#pragma once
//
// Colour-name compatibility, in ONE place.
//
// GFX Library for Arduino 1.6.5 renamed the short macros (BLACK, RED, ...) to
// RGB565_*. Every example sketch used to carry its own copy of this patch; it
// lives here now, so a rename upstream is a one-line fix instead of a sweep
// through every sketch and lesson.
//
// Define LB_NO_LEGACY_COLORS before including the library to suppress the bare
// names if they collide with something in your own code.

#include <Arduino_GFX_Library.h>

// Canonical names — always available, never collide.
#define LB_BLACK   RGB565_BLACK
#define LB_WHITE   RGB565_WHITE
#define LB_RED     RGB565_RED
#define LB_GREEN   RGB565_GREEN
#define LB_BLUE    RGB565_BLUE
#define LB_YELLOW  RGB565_YELLOW
#define LB_MAGENTA RGB565_MAGENTA
#define LB_CYAN    RGB565_CYAN
#define LB_ORANGE  RGB565_ORANGE
#define LB_LIME    RGB565_LIME
#define LB_GREY    RGB565_DARKGREY

#ifndef LB_NO_LEGACY_COLORS
  // Short names, as they were before 1.6.5 — keeps existing sketches compiling.
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
