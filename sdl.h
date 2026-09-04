#pragma once

#ifdef WINDOWS
#ifndef _WINDOWS_
#define _WINDOWS_
#define APIENTRY __attribute__((__stdcall__))
#define WINGDIAPI __attribute__((dllimport))
#endif
#else
#define GL_GLEXT_PROTOTYPES 1
#endif

#include "stdafx.h"
#include "extern/lodepng.h"

namespace SDL {
#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include <SDL3/SDL_opengl_glext.h>

// SDL3_image isn't vendored in this checkout, and the game only ever loads PNGs,
// so provide a minimal IMG_Load/IMG_GetError-compatible shim backed by the
// already-vendored lodepng decoder instead of pulling in a whole second library.
inline SDL_Surface* IMG_Load(const char* file) {
  unsigned char* pixels = nullptr;
  unsigned width = 0, height = 0;
  if (unsigned error = ::lodepng_decode32_file(&pixels, &width, &height, file)) {
    SDL_SetError("lodepng error %u loading %s", error, file);
    return nullptr;
  }
  SDL_Surface* surface = SDL_CreateSurface((int)width, (int)height, SDL_PIXELFORMAT_RGBA32);
  if (surface)
    for (unsigned y = 0; y < height; ++y)
      memcpy((Uint8*)surface->pixels + y * surface->pitch, pixels + y * width * 4, width * 4);
  free(pixels);
  return surface;
}

inline const char* IMG_GetError() {
  return SDL_GetError();
}

// SDL3 flattened SDL_KeyboardEvent's nested "keysym" substruct directly onto the event
// (scancode/key/mod) and dropped the SDL_Keysym type entirely. This codebase uses SDL_Keysym
// on its own as a lightweight keybinding value though, so keep an equivalent struct around.
struct SDL_Keysym {
  SDL_Scancode scancode = SDL_SCANCODE_UNKNOWN;
  SDL_Keycode sym = SDLK_UNKNOWN;
  SDL_Keymod mod = SDL_KMOD_NONE;
};
}

#undef TECHNOLOGY
#undef TRANSPARENT

using SDL::Uint8;

typedef SDL::SDL_Event Event;
typedef SDL::SDL_EventType EventType;

// SDL3 requires a window for SDL_StartTextInput/SDL_StopTextInput (SDL2 didn't). The game only
// ever has one window, so track it here instead of threading it through every GUI element.
extern SDL::SDL_Window* keeperrlMainWindow;
