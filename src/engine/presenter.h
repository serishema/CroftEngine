#pragma once

#include "core/magic.h"
#include "hid/inputhandler.h"

#include <filesystem>
#include <gl/cimgwrapper.h>
#include <gl/window.h>
#include <unordered_set>

namespace loader::file
{
struct Palette;
struct Room;
struct TextureTile;
struct Portal;

namespace level
{
class Level;
}
} // namespace loader::file

namespace audio
{
class SoundEngine;
}

namespace ui
{
class CachedFont;

namespace debug
{
class DebugView;
}
} // namespace ui

namespace render
{
class RenderPipeline;
class TextureAnimator;

namespace scene
{
class ScreenOverlay;
class CSM;
class MaterialManager;
class ShaderManager;
class Renderer;
} // namespace scene
} // namespace render

namespace gl
{
class Font;
template<typename>
class Texture2DArray;
template<typename>
class Image;
} // namespace gl

namespace engine
{
class AudioEngine;
class Engine;
class ObjectManager;
class CameraController;

class Presenter final
{
public:
  explicit Presenter(const std::filesystem::path& rootPath, bool fullscreen, const glm::ivec2& resolution);
  ~Presenter();

  void playVideo(const std::filesystem::path& path);

  void renderWorld(const ObjectManager& objectManager,
                   const std::vector<loader::file::Room>& rooms,
                   const CameraController& cameraController,
                   const std::unordered_set<const loader::file::Portal*>& waterEntryPortals);
  void drawLevelName(const loader::file::Palette& palette, const std::string& levelName);

  auto& getAudioEngine()
  {
    return *m_audioEngine;
  }

  [[nodiscard]] const auto& getAudioEngine() const
  {
    return *m_audioEngine;
  }

  const auto& getMaterialManager()
  {
    return m_materialManager;
  }

  [[nodiscard]] const auto& getMaterialManager() const
  {
    return m_materialManager;
  }

  void setHealthBarTimeout(const core::Frame& f)
  {
    m_healthBarTimeout = f;
  }

  [[nodiscard]] const hid::InputHandler& getInputHandler() const
  {
    return *m_inputHandler;
  }

  // ReSharper disable once CppMemberFunctionMayBeConst
  audio::SoundEngine& getSoundEngine();
  void drawBars(const loader::file::Palette& palette, const ObjectManager& objectManager);
  void animateUV(const std::vector<loader::file::TextureTile>& textureTiles);

  void initTextures(loader::file::level::Level& level, const std::string& animatedTextureId);
  void assignTextures(const gl::SRGBA8* data, int z, int mipmapLevel);
  void initAudio(Engine& engine, const loader::file::level::Level* level, const std::filesystem::path& audioRoot);

  [[nodiscard]] const ui::CachedFont& getTrFont() const
  {
    Expects(m_trFont != nullptr);
    return *m_trFont;
  }

  [[nodiscard]] const auto& getTextureAnimator() const
  {
    BOOST_ASSERT(m_textureAnimator != nullptr);
    return *m_textureAnimator;
  }

  [[nodiscard]] auto& getTextureAnimator()
  {
    BOOST_ASSERT(m_textureAnimator != nullptr);
    return *m_textureAnimator;
  }

  [[nodiscard]] const auto& getRenderer() const
  {
    BOOST_ASSERT(m_renderer != nullptr);
    return *m_renderer;
  }

  [[nodiscard]] auto& getRenderer()
  {
    BOOST_ASSERT(m_renderer != nullptr);
    return *m_renderer;
  }

  [[nodiscard]] const auto& getScreenOverlay() const
  {
    BOOST_ASSERT(m_screenOverlay != nullptr);
    return *m_screenOverlay;
  }

  [[nodiscard]] auto& getScreenOverlay()
  {
    BOOST_ASSERT(m_screenOverlay != nullptr);
    return *m_screenOverlay;
  }

  void drawLoadingScreen(const std::string& state);
  void preFrame();
  [[nodiscard]] bool shouldClose() const;

  void setTrFont(std::unique_ptr<ui::CachedFont>&& font);

  void swapBuffers();

private:
  static constexpr int32_t CSMResolution = 1024;

  const std::unique_ptr<gl::Window> m_window;

  std::unique_ptr<AudioEngine> m_audioEngine;
  const std::shared_ptr<render::scene::Renderer> m_renderer;
  const gl::CImgWrapper m_splashImage;
  gl::CImgWrapper m_splashImageScaled;
  const std::unique_ptr<gl::Font> m_abibasFont;
  const std::unique_ptr<gl::Font> m_debugFont;
  core::Health m_drawnHealth = core::LaraHealth;
  core::Frame m_healthBarTimeout = -40_frame;
  const std::unique_ptr<hid::InputHandler> m_inputHandler;
  std::unique_ptr<ui::CachedFont> m_trFont;
  std::unique_ptr<render::TextureAnimator> m_textureAnimator;
  std::shared_ptr<gl::Texture2DArray<gl::SRGBA8>> m_allTextures;
  const std::unique_ptr<ui::debug::DebugView> m_debugView;

  const std::shared_ptr<render::scene::ShaderManager> m_shaderManager{};
  const std::shared_ptr<render::scene::CSM> m_csm{};
  const std::unique_ptr<render::scene::MaterialManager> m_materialManager{};

  const std::unique_ptr<render::RenderPipeline> m_renderPipeline;
  const std::unique_ptr<render::scene::ScreenOverlay> m_screenOverlay;

  core::Frame m_uvAnimTime = 0_frame;
  bool m_crtEffect = true;
  bool m_showDebugInfo = false;

  void scaleSplashImage();
};
} // namespace engine
