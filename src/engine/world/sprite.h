#pragma once

#include "core/id.h"

#include <glm/glm.hpp>
#include <gsl/gsl-lite.hpp>

namespace render::scene
{
class Mesh;
}

namespace engine::world
{
struct Sprite
{
  core::TextureId textureId{uint16_t(0)};

  glm::vec2 uv0;
  glm::vec2 uv1;

  glm::ivec2 render0;
  glm::ivec2 render1;

  std::shared_ptr<render::scene::Mesh> yBoundMesh;
  std::shared_ptr<render::scene::Mesh> billboardMesh;
};

struct SpriteSequence
{
  core::TypeId type{uint16_t(0)};
  gsl::span<const Sprite> sprites{};
};
} // namespace engine::world
