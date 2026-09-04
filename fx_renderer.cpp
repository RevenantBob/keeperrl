#include "fx_renderer.h"

#include "fx_manager.h"
#include "fx_defs.h"
#include "fx_particle_system.h"
#include "fx_draw_buffers.h"

#include "sdl.h"
#include "renderer.h"

namespace fx {

static constexpr int nominalSize = Renderer::nominalSize;

struct FXRenderer::SystemDrawInfo {
  bool empty() const {
    return numParticles == 0;
  }

  IRect worldRect; // in pixels
  IVec2 fboPos;
  int firstParticle = 0, numParticles = 0;
};

FXRenderer::FXRenderer(DirectoryPath dataPath, FXManager& mgr) : mgr(mgr), texturesPath(dataPath) {
  drawBuffers = make_unique<DrawBuffers>();
}

void FXRenderer::loadTextures() {
  textures.clear();
  textureScales.clear();
  textures.reserve(EnumInfo<TextureName>::size);
  textureScales.reserve(textures.size());

  for (auto texName : ENUM_ALL(TextureName)) {
    auto& tdef = mgr[texName];
    auto path = texturesPath.file(tdef.fileName);
    int id = -1;
    for (int n = 0; n < (int)textures.size(); n++)
      if (textures[n].getPath() == path) {
        id = n;
        break;
      }

    if (id == -1) {
      id = textures.size();
      textures.emplace_back(path);
      textures.back().setParams(Texture::Filter::linear, Texture::Wrapping::clamp);
      auto tsize = textures.back().getSize(), rsize = textures.back().getRealSize();
      FVec2 scale(float(tsize.x) / float(rsize.x), float(tsize.y) / float(rsize.y));
      textureScales.emplace_back(scale);
    }
    textureIds[texName] = id;
  }
}

FXRenderer::~FXRenderer() {}

void FXRenderer::applyTexScale() {
  auto& elements = drawBuffers->elements;
  auto& texCoords = drawBuffers->texCoords;

  for (auto& elem : elements) {
    auto scale = textureScales[textureIds[elem.texName]];
    if (scale == FVec2(1.0f))
      continue;

    int end = elem.firstVertex + elem.numVertices;
    for (int i = elem.firstVertex; i < end; i++)
      texCoords[i] *= scale;
  }
}

IRect FXRenderer::visibleTiles(const View& view) {
  float scaleX = 1.0f / (view.zoomX * nominalSize);
  float scaleY = 1.0f / (view.zoomY * nominalSize);

  FVec2 topLeft = -view.offset * FVec2{scaleX, scaleY};
  FVec2 size = FVec2(view.size) * FVec2{scaleX, scaleY};

  IVec2 iTopLeft(floor(topLeft.x), floor(topLeft.y));
  IVec2 iSize(ceil(size.x), ceil(size.y));

  return IRect(iTopLeft - IVec2(1, 1), iTopLeft + iSize + IVec2(1, 1));
}

IRect FXRenderer::boundingBox(const DrawParticle* particles, int count) {
  if (count == 0)
    return IRect();

  PROFILE;
  FVec2 min = particles[0].positions[0];
  FVec2 max = min;

  // TODO: what to do with invalid data ?
  for (int n = 0; n < count; n++) {
    auto& particle = particles[n];
    for (auto& pt : particle.positions) {
      min = vmin(min, pt);
      max = vmax(max, pt);
    }
  }

  return {IVec2(min) - IVec2(1), IVec2(max) + IVec2(2)};
}

void FXRenderer::prepareOrdered() {
  PROFILE;
  auto& systems = mgr.getSystems();
  systemDraws.clear();
  systemDraws.resize(systems.size());
  orderedParticles.clear();

  for (int n = 0; n < systems.size(); n++) {
    auto& system = systems[n];
    if (system.isDead || !system.orderedDraw)
      continue;

    auto& def = mgr[system.defId];
    int first = (int)orderedParticles.size();
    for (int ssid = 0; ssid < system.subSystems.size(); ssid++)
      mgr.genQuads(orderedParticles, n, ssid);
    int count = (int)orderedParticles.size() - first;

    if (count > 0) {
      auto rect = boundingBox(&orderedParticles[first], count);
      systemDraws[n] = {rect, IVec2(), first, count};
    }
  }
}

void FXRenderer::printSystemDrawsInfo() const {
  for (int n = 0; n < (int)systemDraws.size(); n++) {
    auto& sys = systemDraws[n];
    if (sys.empty())
      continue;

    INFO << "FX System #" << n << ": " << " rect:(" << sys.worldRect.x() << ", "
         << sys.worldRect.y() << ") - (" << sys.worldRect.ex() << ", "
         << sys.worldRect.ey() << ")";
  }
}

void FXRenderer::setView(float zoomX, float zoomY, float offsetX, float offsetY, int w, int h) {
  worldView = View{zoomX, zoomY, {offsetX, offsetY}, {w, h}};
  fboView = visibleTiles(worldView);
}

void FXRenderer::drawOrdered(const int* ids, int count, float offsetX, float offsetY, Color color) {
  PROFILE;
  drawBuffers->clear();
  for (int n = 0; n < count; n++) {
    auto id = ids[n];
    if (id < 0 || id >= systemDraws.size())
      continue;
    auto& draw = systemDraws[id];
    if (draw.empty())
      continue;
    CHECK(draw.firstParticle + draw.numParticles <= orderedParticles.size());
    drawBuffers->add(&orderedParticles[draw.firstParticle], draw.numParticles);
  }

  auto view = worldView;
  view.offset += FVec2(offsetX, offsetY);

  applyTexScale();
  drawParticles(view, BlendMode::normal);
  // TODO: blend add support
  drawParticles(view, BlendMode::additive);
}

void FXRenderer::drawUnordered(Layer layer) {
  PROFILE;
  tempParticles.clear();
  drawBuffers->clear();

  auto& systems = mgr.getSystems();
  for (int n = 0; n < systems.size(); n++) {
    if (systems[n].isDead)
      continue;
    auto& system = systems[n];
    auto& ssdef = mgr[system.defId];
    if (system.orderedDraw)
      continue;

    for (int ssid = 0; ssid < system.subSystems.size(); ssid++)
      if (ssdef[ssid].layer == layer)
        mgr.genQuads(tempParticles, n, ssid);
  }

  drawBuffers->add(tempParticles.data(), tempParticles.size());
  if (drawBuffers->empty())
    return;
  applyTexScale();

  drawParticles(worldView, BlendMode::normal);
  // TODO: blend add support
  drawParticles(worldView, BlendMode::additive);
}

void FXRenderer::drawParticles(const View& view, BlendMode blendMode) {
  PROFILE;
  auto& positions = drawBuffers->positions;
  auto& texCoords = drawBuffers->texCoords;
  auto& colors = drawBuffers->colors;
  auto numVerts = (int)positions.size();
  if (numVerts == 0)
    return;

  // Non-destructively apply the view transform (this runs once per blend mode over the same
  // source data, so drawBuffers itself must stay untouched).
  static vector<FVec2> transformed;
  static vector<SDL::SDL_FColor> fcolors;
  transformed.resize(numVerts);
  fcolors.resize(numVerts);
  FVec2 zoom{view.zoomX, view.zoomY};
  for (int i = 0; i < numVerts; i++)
    transformed[i] = positions[i] * zoom + view.offset;
  for (int i = 0; i < numVerts; i++) {
    unsigned c = colors[i];
    auto* bytes = (const unsigned char*)&c;
    fcolors[i] = SDL::SDL_FColor{bytes[0] / 255.0f, bytes[1] / 255.0f, bytes[2] / 255.0f, bytes[3] / 255.0f};
  }

  static vector<int> indices;
  auto blendModeSdl = blendMode == BlendMode::additive
      ? SDL::SDL_ComposeCustomBlendMode(SDL::SDL_BLENDFACTOR_ONE, SDL::SDL_BLENDFACTOR_ONE, SDL::SDL_BLENDOPERATION_ADD,
            SDL::SDL_BLENDFACTOR_ONE, SDL::SDL_BLENDFACTOR_ONE, SDL::SDL_BLENDOPERATION_ADD)
      : SDL_BLENDMODE_BLEND;

  for (auto& elem : drawBuffers->elements) {
    auto& tdef = mgr[elem.texName];
    if (tdef.blendMode != blendMode)
      continue;
    auto& tex = textures[textureIds[elem.texName]];
    auto texId = tex.getTexId();
    SDL::SDL_SetTextureBlendMode(texId, blendModeSdl);

    int numQuads = elem.numVertices / 4;
    indices.resize(numQuads * 6);
    for (int q = 0; q < numQuads; q++) {
      int base = elem.firstVertex + q * 4;
      int i = q * 6;
      indices[i + 0] = base + 0;
      indices[i + 1] = base + 1;
      indices[i + 2] = base + 2;
      indices[i + 3] = base + 0;
      indices[i + 4] = base + 2;
      indices[i + 5] = base + 3;
    }

    SDL::SDL_RenderGeometryRaw(keeperrlRenderer, texId,
        (const float*)transformed.data(), sizeof(FVec2),
        fcolors.data(), sizeof(SDL::SDL_FColor),
        (const float*)texCoords.data(), sizeof(FVec2),
        numVerts, indices.data(), (int)indices.size(), 4);
  }
}
}
