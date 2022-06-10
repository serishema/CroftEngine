#pragma once

#include "render/scene/node.h"

#include <cstdint>
#include <gl/buffer.h>
#include <gl/pixel.h>
#include <glm/mat4x4.hpp>
#include <string>

namespace engine::world
{
class World;
}

namespace engine::ghosting
{
struct GhostFrame;

class GhostModel final : public render::scene::Node
{
public:
  GhostModel()
      : render::scene::Node{"ghost"}
  {
    setColor(gl::SRGB8{51, 51, 204});
  }

  void apply(const engine::world::World& world, const GhostFrame& frame);

  [[nodiscard]] const auto& getMeshMatricesBuffer() const
  {
    return m_meshMatricesBuffer;
  }

  [[nodiscard]] auto getRoomId() const
  {
    return m_roomId;
  }

  void setColor(const gl::SRGB8& color)
  {
    bind("u_color",
         [color = glm::vec3{color.channels}
                  / 255.0f](const render::scene::Node*, const render::scene::Mesh& /*mesh*/, gl::Uniform& uniform)
         {
           uniform.set(color);
         });
  }

private:
  mutable gl::ShaderStorageBuffer<glm::mat4> m_meshMatricesBuffer{"mesh-matrices-ssb"};
  uint32_t m_roomId = 0;
};
} // namespace engine::ghosting
