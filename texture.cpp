/* Copyright (C) 2013-2014 Michal Brzozowski (rusolis@poczta.fm)

   This file is part of KeeperRL.

   KeeperRL is free software; you can redistribute it and/or modify it under the terms of the
   GNU General Public License as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   KeeperRL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without
   even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along with this program.
   If not, see http://www.gnu.org/licenses/ . */

#include "stdafx.h"
#include "texture.h"
#include "debug.h"

Texture::Texture() = default;

Texture::Texture(const FilePath& fileName) : path(fileName) {
  SDL::SDL_Surface* image = SDL::IMG_Load(fileName.getPath());
  CHECK(image) << SDL::IMG_GetError();
  CHECK(loadFromMaybe(image)) << "Couldn't load image: " << fileName << ". Error: " << SDL::SDL_GetError();
  SDL::SDL_DestroySurface(image);
}

Texture::Texture(const FilePath& filename, int px, int py, int w, int h) : path(filename) {
  SDL::SDL_Surface* image = SDL::IMG_Load(path->getPath());
  CHECK(image) << SDL::IMG_GetError();
  SDL::SDL_Rect offset;
  offset.x = 0;
  offset.y = 0;
  SDL::SDL_Rect src{px, py, w, h};
  SDL::SDL_Surface* sub = createSurface(src.w, src.h);
  CHECK(sub) << SDL::SDL_GetError();
  CHECK(SDL_BlitSurface(image, &src, sub, &offset)) << SDL::SDL_GetError();
  SDL::SDL_DestroySurface(image);
  CHECK(loadFromMaybe(sub)) << "Couldn't load image: " << *path << ". Error: " << SDL::SDL_GetError();
  SDL::SDL_DestroySurface(sub);
}

Texture::Texture(Color color, int width, int height) {
  vector<Color> colors(width * height, color);
  texId = SDL::SDL_CreateTexture(keeperrlRenderer, SDL::SDL_PIXELFORMAT_RGBA32, SDL::SDL_TEXTUREACCESS_STATIC,
      width, height);
  CHECK(texId) << SDL::SDL_GetError();
  SDL::SDL_SetTextureBlendMode(texId, SDL_BLENDMODE_BLEND);
  setParams(Filter::nearest, Wrapping::clamp);
  CHECK(SDL::SDL_UpdateTexture(texId, nullptr, colors.data(), width * 4)) << SDL::SDL_GetError();

  realSize = size = Vec2(width, height);
}

void Texture::setParams(Filter filter, Wrapping wrap) {
  if (texId) {
    auto scaleMode = filter == Filter::nearest ? SDL::SDL_SCALEMODE_NEAREST : SDL::SDL_SCALEMODE_LINEAR;
    SDL::SDL_SetTextureScaleMode(texId, scaleMode);
    addressMode = wrap == Wrapping::repeat ? SDL::SDL_TEXTURE_ADDRESS_WRAP : SDL::SDL_TEXTURE_ADDRESS_CLAMP;
  }
}

Texture::Texture(SDL::SDL_Surface* surface) {
  CHECK(loadFromMaybe(surface)) << "Error loading texture: " << SDL::SDL_GetError();
}

Texture::Texture(Texture&& tex) noexcept {
  *this = std::move(tex);
}

Texture::~Texture() {
  if (texId)
    SDL::SDL_DestroyTexture(texId);
}

Texture& Texture::operator=(Texture&& tex) noexcept {
  if (texId)
    SDL::SDL_DestroyTexture(texId);
  size = tex.size;
  realSize = tex.realSize;
  addressMode = tex.addressMode;
  texId = tex.texId;
  path = tex.path;
  tex.texId = nullptr;
  return *this;
}

bool Texture::loadPixels(unsigned char* pixels) {
  return SDL::SDL_UpdateTexture(texId, nullptr, pixels, (int)size.x * 4);
}

bool Texture::loadFromMaybe(SDL::SDL_Surface* image) {
  if (texId) {
    SDL::SDL_DestroyTexture(texId);
    texId = nullptr;
  }
  texId = SDL::SDL_CreateTextureFromSurface(keeperrlRenderer, image);
  if (!texId)
    return false;
  // SDL_CreateTextureFromSurface() ends by copying the *source surface's own* blend mode onto the
  // new texture (see SDL_UpdateTextureFromSurface() in SDL_render.c), overriding its own format-based
  // auto-detection. Some source surfaces (e.g. TileSet's atlas, built via SDL_BlitSurface with
  // SDL_BLENDMODE_NONE so tiles get copied verbatim instead of alpha-composited into each other) are
  // deliberately non-blending for that CPU-side compositing step, which has nothing to do with how the
  // resulting GPU texture should render -- force real alpha blending here regardless.
  SDL::SDL_SetTextureBlendMode(texId, SDL_BLENDMODE_BLEND);
  setParams(Filter::nearest, Wrapping::clamp);
  size = realSize = Vec2(image->w, image->h);
  return true;
}

optional<Texture> Texture::loadMaybe(const FilePath& path) {
  if (SDL::SDL_Surface* image = SDL::IMG_Load(path.getPath())) {
    Texture ret;
    bool ok = ret.loadFromMaybe(image);
    SDL::SDL_DestroySurface(image);
    if (ok) {
      ret.path = path;
      return std::move(ret);
    }
  }
  return none;
}

SDL::SDL_Surface* Texture::createSurface(int w, int h) {
  SDL::SDL_Surface* ret = SDL::SDL_CreateSurface(w, h, SDL::SDL_PIXELFORMAT_RGBA32);
  CHECK(ret) << "Failed to create surface " << w << ":" << h << ": " << SDL::SDL_GetError();
  return ret;
}
