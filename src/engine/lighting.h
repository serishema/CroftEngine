#pragma once

#include "core/units.h"
#include "qs/qs.h"

#include <gl/buffer.h>
#include <glm/vec4.hpp>
#include <gsl/gsl-lite.hpp>
#include <gslu.h>
#include <limits>
#include <memory>

namespace render::scene
{
class Node;
}

namespace engine::world
{
struct Room;
}

namespace engine
{
struct ShaderLight
{
  glm::vec4 position{std::numeric_limits<float>::quiet_NaN()};
  float brightness = 0;
  float fadeDistance = 0;
  // NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays, modernize-avoid-c-arrays)
  float _pad[2]{0.0f, 0.0f};

  bool operator==(const ShaderLight& rhs) const
  {
    return position == rhs.position && brightness == rhs.brightness && fadeDistance == rhs.fadeDistance;
  }

  bool operator!=(const ShaderLight& rhs) const
  {
    return !(*this == rhs);
  }

  static gslu::nn_shared<gl::ShaderStorageBuffer<ShaderLight>> getEmptyBuffer()
  {
    static std::weak_ptr<gl::ShaderStorageBuffer<ShaderLight>> instance;
    if(auto tmp = instance.lock())
      return gsl::not_null{tmp};

    auto tmp = gsl::make_shared<gl::ShaderStorageBuffer<ShaderLight>>("empty-lights-buffer");
    instance = tmp.get();
    return tmp;
  }
};
static_assert(sizeof(ShaderLight) == 32, "Invalid Light struct size");

struct Lighting
{
  core::Brightness ambient{-1.0f};

  Lighting() = default;

  void update(const core::Shade& shade, const world::Room& baseRoom);

  void bind(render::scene::Node& node) const;

private:
  void fadeAmbient(const core::Shade& shade)
  {
    const auto targetAmbient = toBrightness(shade);
    if(ambient.get() < 0)
      ambient = targetAmbient;
    else
      ambient += (targetAmbient - ambient) / 50.0f;
  }

  gslu::nn_shared<gl::ShaderStorageBuffer<ShaderLight>> m_buffer{ShaderLight::getEmptyBuffer()};
};
} // namespace engine
