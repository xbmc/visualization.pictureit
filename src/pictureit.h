#pragma once

#include <kodi/addon-instance/Visualization.h>
#include <kodi/gui/gl/GL.h>
#include <kodi/gui/gl/Shader.h>
#include <glm/gtc/type_ptr.hpp>

#include <atomic>
#include <mutex>
#include <thread>

struct sPosition
{
  sPosition() : x(0.0f), y(0.0f), z(0.0f), u(1.0f) {}
  sPosition(float x, float y, float z = 0.0f) : x(x), y(y), z(z), u(1.0f) {}
  float x,y,z,u;
};

struct sCoord
{
  sCoord() : u(0.0f), v(0.0f) {}
  sCoord(float u, float v) : u(u), v(v) {}
  float u,v;
};

struct sColor
{
  sColor() : r(0.0f), g(0.0f), b(0.0f), a(1.0f) {}
  sColor(float r, float g, float b, float a = 1.0f) : r(r), g(g), b(b), a(a) {}
  float r,g,b,a;
};

struct sLight
{
  sPosition vertex;
  sColor color;
  sCoord coord;
};

class MRFFT;

typedef std::vector<std::string> td_vec_str;
typedef std::map<std::string, td_vec_str> td_map_data;

class ATTRIBUTE_HIDDEN CVisPictureIt : public kodi::addon::CAddonBase,
                                       public kodi::addon::CInstanceVisualization,
                                       public kodi::gui::gl::CShaderProgram
{
public:
  CVisPictureIt();
  ~CVisPictureIt() override;

  ADDON_STATUS Create() override;
  bool GetPresets(std::vector<std::string>& presets) override;
  int GetActivePreset() override;
  bool PrevPreset() override;
  bool NextPreset() override;
  bool LoadPreset(int select) override;
  bool RandomPreset() override;
  bool Start(int channels, int samplesPerSec, int bitsPerSample,
              std::string songName) override;
  void Stop() override;
  void Render() override;
  void AudioData(const float*, int, float*, int) override;
  bool UpdateTrack(const kodi::addon::VisualizationTrack& track) override;

  // kodi::gui::gl::CShaderProgram
  void OnCompiledAndLinked() override;
  bool OnEnabled() override;

private:
  std::string path_join(std::string a, std::string b);
  bool list_dir(const std::string& path, td_vec_str &store, bool recursive = false,
               bool incl_full_path = true, std::string file_filter = "");
  int get_next_img_pos();
  void load_presets(const std::string& path);
  void load_data(const std::string& path);
  void select_preset(unsigned int index);
  void load_next_image();
  void draw_image(GLuint img_tex_id, float opacity);
  void draw_bars(int i, GLfloat x1, GLfloat x2);
  void start_render();
  void finish_render();

  /*
   * Settings
   */
  bool m_updateByInterval = true;
  bool m_updateOnNewTrack = true;
  int m_imgUpdateInterval = 180;
  int m_visEnabled = true;
  int m_visBgEnabled = true;

  // Used to define some "padding" left and right.
  // If set to 1.0 the bars will go to the screen edge
  GLfloat m_visWidth = 0.8f;

  // If set to 1.0 the bars would be exactly on the screen edge
  GLfloat m_visBottomEdge = 0.98f;

  // Animation speed. The smaler the value, the slower
  // and smoother the animations
  GLfloat m_visAnimationSpeed = 0.007f;

  std::shared_ptr<std::thread> m_dataLoader;
  std::atomic<bool> m_dataLoaderActive;
  std::atomic<bool> m_dataLoaded;

  std::shared_ptr<std::thread> m_imgLoader;
  std::atomic<bool> m_imgLoaderActive;
  std::atomic<bool> m_imgLoaded;

  unsigned char* m_imgData = nullptr;
  int m_imgWidth, m_imgHeight, m_imgChannels = 0;

  std::unique_ptr<MRFFT> m_tranform;

  /*
   * "m_imgTextureIds" holds the texture-ids for images:
   *  0: The current displayed image.
   *  1: The next image which fades in.
   *  2: The previously displayed image, which gets recycled.
   */
  GLuint m_imgTextureIds[3] = {};

  // When set to "true", a new image will be crossfaded
  bool m_updateImg = false;

  // Root directory holding subfolders that will be used as presets and
  // searched for images
  std::string m_presetsRootDir = "";

  // Amount of presets (subfolders) found
  unsigned int m_presetsCount = 0;

  // Index of the currently selected preset
  unsigned int m_presetIndex = -1;

  int m_imgCurrentPos = 0;

  // Time (in sec.) where we last updated the image
  time_t m_imgLastUpdated = time(0);

  // The last alpha value of our image
  GLfloat m_fadeLast = 0.0f;

  // The current alpha value of our image
  GLfloat m_fadeCurrent = 0.0f;

  // How long the crossfade between two images will take (in ms)
  int m_fadeTimeMs = 2000;

  // Used in combination with "m_fadeTimeMs" to calculate the new alpha value
  // for the next frame
  int m_fadeOffsetMs = 0;

  // Amount of single bars to display (will be doubled as we mirror them to
  // the right side)
  static const int m_visBarCount = 96;

  // The min height for each bar
  const GLfloat m_visBarMinHeight = 0.02f;

  // The max height for each bar
  const GLfloat m_visBarMaxHeight = 0.18f;

  const float m_visBottomEdgeScale[11] =
  {
    1.0, 0.98, 0.96, 0.94, 0.92, 0.90, 0.88, 0.86, 0.84, 0.82, 0.80
  };

  // Whatever we get from AudioData
  GLfloat m_visBarHeights[m_visBarCount] = {};

  // The previous value we got from AudioData (used to calculate some form
  // of gravity. The bigger the difference, the faster the bars will move)
  GLfloat m_pvisBarHeights[m_visBarCount] = {};

  // Used to smoothen the animation on a "per frame" basis.
  GLfloat m_cvisBarHeights[m_visBarCount] = {};

  // Holds all preset-names in alphabetical order
  td_vec_str m_piPresets;

  // Always holds the images for the currently selected preset and
  // will be updated upon preset change
  td_vec_str m_piImages;

  // Map consisting of key = preset-name, value = vector of all associated
  // images
  td_map_data m_piData;

  int m_prevFreqDataLength = 0;

  bool m_textureUsed = false;

  // OpenGL projection matrix setup
  // Coordinate-System:
  //     screen top left:     (-1, -1)
  //     screen center:       ( 0,  0)
  //     screen bottom right: ( 1,  1)
  const glm::mat4 m_projMat = glm::ortho(-1.0f, 1.0f, 1.0f, -1.0f, -1.0f, 1.0f);
  const glm::mat4 m_modelMat = glm::mat4(1.0f);
  const GLubyte m_index[4] = {0, 1, 3, 2};

  GLint m_projMatLoc = -1;
  GLint m_modelViewMatLoc = -1;
  GLint m_textureIdLoc = -1;
  GLint m_hVertex = -1;
  GLint m_hCoord = -1;
  GLint m_hColor = -1;

  GLuint m_vertexVBO = 0;
  GLuint m_indexVBO = 0;

  GLuint m_texture = 0;

  bool m_initialized = false;
  bool m_shadersLoaded = false;

  unsigned int m_get_next_img_pos_Calls = 0;
  std::string m_last_path;
  std::mutex m_mutex;
};
