/*
 *  Copyright (C) 2018-2021 Team Kodi (https://kodi.tv)
 *  Copyright (C) 2015-2019 LinuxWhatElse
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "pictureit.h"

#include "mrfft.h"
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#define STBI_ONLY_BMP
#include "stb_image.h"

#include <algorithm>
#include <random>

namespace
{
const std::string img_filter = ".jpg|.jpeg|.png";
}

CVisPictureIt::CVisPictureIt()
  : m_dataLoaderActive(false),
    m_dataLoaded(false),
    m_imgLoaderActive(false),
    m_imgLoaded(false)
{
}

CVisPictureIt::~CVisPictureIt()
{
  if (m_dataLoader != nullptr)
  {
    if (m_dataLoader->joinable())
    {
      m_dataLoader->join();
    }

    m_dataLoader = nullptr;
  }

  if (m_imgLoader != nullptr)
  {
    if (m_imgLoader->joinable())
    {
      m_imgLoader->join();
    }

    m_imgLoader = nullptr;
  }

  for (auto ptr : m_piData)
  {
    ptr.second.clear();
  }
  m_piData.clear();

  glDeleteTextures(3, m_imgTextureIds);
}

ADDON_STATUS CVisPictureIt::Create()
{
  m_presetsRootDir = kodi::addon::GetSettingString("presets_root_dir");

  m_updateOnNewTrack = kodi::addon::GetSettingBoolean("update_on_new_track");
  m_updateByInterval = kodi::addon::GetSettingBoolean("update_by_interval");
  m_imgUpdateInterval = kodi::addon::GetSettingInt("img_update_interval");
  m_fadeTimeMs = kodi::addon::GetSettingInt("fade_time_ms");
  m_visEnabled = kodi::addon::GetSettingBoolean("vis_enabled");
  m_visBgEnabled = kodi::addon::GetSettingBoolean("vis_bg_enabled");

  m_visWidth = kodi::addon::GetSettingInt("vis_half_width");
  m_visWidth = m_visWidth * 1.0f / 100;

  m_visAnimationSpeed = kodi::addon::GetSettingInt("vis_animation_speed");
  m_visAnimationSpeed = m_visAnimationSpeed * 0.005f / 100;

  float scale[] = {1.0, 0.98, 0.96, 0.94, 0.92, 0.90, 0.88, 0.86, 0.84, 0.82, 0.80};
  m_visBottomEdge = scale[kodi::addon::GetSettingInt("vis_bottom_edge")];

  return ADDON_STATUS_OK;
}

bool CVisPictureIt::GetPresets(std::vector<std::string>& presets)
{
  load_presets(m_presetsRootDir);
  if (m_piPresets.empty())
    return false;

  for (unsigned int i = 0; i < m_piPresets.size(); i++)
    presets.push_back(m_piPresets[i]);

  return true;
}

int CVisPictureIt::GetActivePreset()
{
  return static_cast<int>(m_presetIndex);
}

bool CVisPictureIt::NextPreset()
{
  select_preset((m_presetIndex + 1) % m_piPresets.size());
  return true;
}

bool CVisPictureIt::PrevPreset()
{
  select_preset((m_presetIndex - 1) % m_piPresets.size());
  return true;
}

bool CVisPictureIt::LoadPreset(int select)
{
  select_preset(select % m_piPresets.size());
  return true;
}

bool CVisPictureIt::RandomPreset()
{
  select_preset((int)((std::rand() / (float)RAND_MAX) * m_piPresets.size()));
  return true;
}

bool CVisPictureIt::Start(int iChannels, int iSamplesPerSec,
                          int iBitsPerSample, const std::string& szSongName)
{
  if (!m_shadersLoaded)
  {
    std::string fraqShader = kodi::addon::GetAddonPath("resources/shaders/" GL_TYPE_STRING "/frag.glsl");
    std::string vertShader = kodi::addon::GetAddonPath("resources/shaders/" GL_TYPE_STRING "/vert.glsl");
    if (!LoadShaderFiles(vertShader, fraqShader) || !CompileAndLink())
      return false;
    m_shadersLoaded = true;
  }

  glGenBuffers(1, &m_vertexVBO);
  glGenBuffers(1, &m_indexVBO);

  if (!m_dataLoader)
    m_dataLoader = std::make_shared<std::thread>(&CVisPictureIt::load_data, this, m_presetsRootDir);

  m_initialized = true;

  return true;
}

void CVisPictureIt::Stop()
{
  if (!m_initialized)
    return;

  m_initialized = false;

  glDeleteBuffers(1, &m_vertexVBO);
  m_vertexVBO = 0;
  glDeleteBuffers(1, &m_indexVBO);
  m_indexVBO = 0;
}

bool CVisPictureIt::UpdateTrack(const kodi::addon::VisualizationTrack& track)
{
  if (m_updateOnNewTrack)
  {
    m_updateImg = true;
  }
  return true;
}

void CVisPictureIt::Render()
{
  if (!m_initialized)
    return;

  start_render();

  // reached next update-intervall
  if (m_updateByInterval && time(0) >= (m_imgLastUpdated + m_imgUpdateInterval))
  {
    m_updateImg = true;
  }

  if (m_updateImg && ! m_dataLoaderActive)
  {
    kodi::Log(ADDON_LOG_DEBUG, "Requesting new image...");
    m_updateImg = false;
    m_imgLastUpdated = time(0);

    if (m_imgTextureIds[2] != 0)
    {
      glDeleteTextures(1, &m_imgTextureIds[2]);
    }

    if (!m_imgLoaderActive && !m_imgLoader)
    {
      m_imgLoader = std::make_shared<std::thread>(&CVisPictureIt::load_next_image, this);
    }
  }

  if (m_imgLoaded)
  {
    m_imgLoaded = false;
    if (m_imgLoader && m_imgLoader->joinable())
    {
      m_imgLoader->join();
    }
    m_imgLoader = nullptr;

    if (m_imgData != nullptr)
    {
      GLuint texture[1];
      glGenTextures(1, texture);

      glBindTexture(GL_TEXTURE_2D, texture[0]);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_imgWidth, m_imgHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, m_imgData);

      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,     GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,     GL_CLAMP_TO_EDGE);

      stbi_image_free(m_imgData);
      m_imgData = nullptr;

      m_imgTextureIds[1] = texture[0];
    }

    m_fadeCurrent = 0.0f;
    m_fadeOffsetMs = static_cast<long>(std::chrono::duration<double>(std::chrono::high_resolution_clock::now().time_since_epoch()).count() * 1000.0) % m_fadeTimeMs;
  }

  // If we are within a crossfade, fade out the current image
  if (m_fadeCurrent < 1.0f)
  {
    draw_image(m_imgTextureIds[0], 1.0f - m_fadeCurrent);
  }
  else
  {
    draw_image(m_imgTextureIds[0], 1.0f);
  }

  if (m_fadeOffsetMs && m_fadeCurrent < 1.0f)
  {
    m_fadeCurrent = ((float) ((static_cast<long>(std::chrono::duration<double>(std::chrono::high_resolution_clock::now().time_since_epoch()).count() * 1000.0) - m_fadeOffsetMs) % m_fadeTimeMs) / m_fadeTimeMs);
    if (m_fadeCurrent < m_fadeLast)
    {
      m_fadeLast = 0.0f;
      m_fadeCurrent = 1.0f;
      m_fadeOffsetMs = 0;

      // Recycle the current image.
      m_imgTextureIds[2] = m_imgTextureIds[0];

      // Display the next image from now on.
      m_imgTextureIds[0] = m_imgTextureIds[1];
    }
    else
    {
      m_fadeLast = m_fadeCurrent;
    }

    draw_image(m_imgTextureIds[1], m_fadeCurrent);
  }

  if (m_visEnabled)
  {
    m_textureUsed = false;
    EnableShader();

    // If set to "true" we draw a transparent background which goes
    // behind the spectrum.
    if (m_visBgEnabled)
    {
      sLight framedTextures[4];
      framedTextures[0].color = framedTextures[1].color = framedTextures[2].color = framedTextures[3].color = sColor(0.0f, 0.0f, 0.0f, 0.7f);
      framedTextures[0].vertex = sPosition(1.0f, (m_visBottomEdge - m_visBarMaxHeight) - (1.0f - m_visBottomEdge));
      framedTextures[1].vertex = sPosition(-1.0f,(m_visBottomEdge - m_visBarMaxHeight) - (1.0f - m_visBottomEdge));
      framedTextures[2].vertex = sPosition(-1.0f, 1.0f);
      framedTextures[3].vertex = sPosition( 1.0f, 1.0f);

      glEnable(GL_BLEND);
      glBufferData(GL_ARRAY_BUFFER, sizeof(sLight)*4, framedTextures, GL_STATIC_DRAW);
      glDrawElements(GL_TRIANGLE_STRIP, 4, GL_UNSIGNED_BYTE, 0);
      glDisable(GL_BLEND);
    }

    // Finally we draw all of our bars
    // The mirrored bars will get drawn within the "draw_bars".
    // This needs to be done to ensure the exact same height-value for both
    // the left and the (mirrored) right bar
    GLfloat x1, x2;
    float bar_width = m_visWidth / m_visBarCount;
    for (int i = 1; i <= m_visBarCount; i++)
    {
      // calculate position
      x1 = (m_visWidth * -1) + (i * bar_width) - bar_width;
      x2 = (m_visWidth * -1) + (i * bar_width);

      // "add" a gap (which is 1/4 of the initial bar_width)
      // to both the left and right side of the bar
      x1 = x1 + (bar_width / 4);
      x2 = x2 - (bar_width / 4);

      draw_bars((i-1), x1, x2);
    }

    DisableShader();
  }

  finish_render();
}

void CVisPictureIt::AudioData(const float* pAudioData, size_t iAudioDataLength)
{
  if (!m_visEnabled || !m_initialized)
    return;

  size_t iFreqDataLength = iAudioDataLength / 2;
  iFreqDataLength -= iFreqDataLength % 2;

  float* freq_data = new float[iFreqDataLength];

  // This part is essentially the same as what Kodi would do if we'd set "pInfo->bWantsFreq = true" (in the GetInfo methode)
  // However, even though Kodi can do windowing (Hann window) they set the flag for it to "false" (hardcoded).
  // So I just copied the "rfft.h" and "rfft.cpp", renamed the classe to "MRFFT" (otherwise we'd use the original) and set
  // the flag to "true".
  // Further this gives us the ability to change the response if needed (They return the magnitude per default I believe)
  if (m_prevFreqDataLength != iFreqDataLength || ! m_tranform)
  {
    m_tranform.reset(new MRFFT(iFreqDataLength, true));
    m_prevFreqDataLength = iFreqDataLength;
  }

  m_tranform->calc(pAudioData, freq_data);

  // Now that we have our values (as I said, the magnitude I guess) we just display them without a whole
  // lot of modification.
  // This is the part where the pre-processing should happen so we get a nice spectrum
  for (int i = 0; i < m_visBarCount; i++)
  {
    if (i >= iFreqDataLength)
    {
      break;
    }

    float height = freq_data[i];

    if (height > m_visBarMaxHeight)
    {
      height = m_visBarMaxHeight;
    }
    else if (height < m_visBarMinHeight)
    {
      height = m_visBarMinHeight;
    }

    m_visBarHeights[i] = height;
  }

  delete[] freq_data;
}


std::string CVisPictureIt::path_join(std::string a, std::string b)
{
  /**
   * Join file/folder path
   */

  // a ends with "/"
  if (a.substr(a.length() - 1, a.length()) == "/")
  {
    a = a.substr(0, a.size() - 1);
  }

  // b starts with "/"
  if (b.substr(0, 1) == "/")
  {
    b = b.substr(1, b.size());
  }

  // b ends with "/"
  if (b.substr(b.length() - 1, b.length()) == "/")
  {
    b = b.substr(0, b.size() -1);
  }

  return a + "/" + b;
}

bool CVisPictureIt::list_dir(const std::string& path, td_vec_str &store, bool recursive,
                             bool incl_full_path, std::string file_filter)
{
  std::vector<kodi::vfs::CDirEntry> items;
  if (!kodi::vfs::GetDirectory(path, file_filter, items))
    return false;

  bool add = false;
  for (const auto& item : items)
  {
    if (item.IsFolder())
    {
      if (file_filter.empty())
      {
        add = true;
      }

      if (recursive)
        list_dir(item.Path(), store, recursive, incl_full_path, file_filter);
    }
    else if (!file_filter.empty())
    {
      add = true;
    }

    if (add)
    {
      if (incl_full_path)
      {
        store.push_back(item.Path());
      }
      else
      {
        store.push_back(item.Label());
      }

      add = false;
    }
  }

  return true;
}

int CVisPictureIt::get_next_img_pos()
{
  std::random_device rd;
  std::mt19937 engine{rd()};
  std::uniform_int_distribution<int> dist(0, m_piImages.size() - 1);

  int num = dist(engine);
  if (num == m_imgCurrentPos && m_get_next_img_pos_Calls++ < 10) // try only 10 times to prevent possible dead loop
    return get_next_img_pos();

  m_get_next_img_pos_Calls = 0;
  m_imgCurrentPos = num;
  return m_imgCurrentPos;
}

void CVisPictureIt::load_presets(const std::string& path)
{
  kodi::Log(ADDON_LOG_DEBUG, "Loading presets...");

  if (path.empty())
  {
    return;
  }

  list_dir(path, m_piPresets, false, false, "");
  std::sort(m_piPresets.begin(), m_piPresets.end());

  if (m_piPresets.empty())
  {
    m_piPresets.push_back("Default");
  }
}

void CVisPictureIt::load_data(const std::string& path)
{
  const std::lock_guard<std::mutex> lock(m_mutex);

  /**
   * Load presets and all associated images
   */
  kodi::Log(ADDON_LOG_DEBUG, "Gathering images...");
  m_dataLoaderActive = true;

  if (path.empty())
  {
    m_dataLoaderActive = false;
    return;
  }

  td_vec_str images;
  if (m_piPresets[0] == "Default" )
  {
    list_dir(path, images, true, true, img_filter);
    m_piData[m_piPresets[0]] = images;
    images.clear();
  }
  else
  {
    for (unsigned int i = 0; i < m_piPresets.size(); i++)
    {
      list_dir(path_join(path, m_piPresets[i]).c_str(), images, true, true, img_filter);

      // Preset empty or can't be accessed
      if (images.size() <= 0)
      {
        m_piPresets.erase(m_piPresets.begin() + i);
        continue;
      }

      m_piData[m_piPresets[i]] = images;
      images.clear();
    }
  }

  select_preset((rand() % m_piPresets.size()));
  if (m_presetIndex > -1 && m_piData.empty())
  {
    kodi::Log(ADDON_LOG_DEBUG, "Selecting initial preset...");
  }

  m_dataLoaded = true;
  m_dataLoaderActive = false;
}

void CVisPictureIt::select_preset(unsigned int index)
{
  /**
   * Select preset at "index"
   */
  if (index >= m_piPresets.size())
  {
    return;
  }

  m_presetIndex = index;
  m_piImages = m_piData[m_piPresets[m_presetIndex]];

  m_updateImg = true;
}

void CVisPictureIt::load_next_image()
{
  const std::lock_guard<std::mutex> lock(m_mutex);

  m_imgLoaderActive = true;

  if (!m_piImages.empty())
  {
    auto path = m_piImages[get_next_img_pos()].c_str();
    if (path == m_last_path)
      return;
    m_last_path = path;

    kodi::Log(ADDON_LOG_DEBUG, "Loading image: %s", path);
    m_imgData = stbi_load(path, &m_imgWidth, &m_imgHeight, &m_imgChannels, STBI_rgb_alpha);

    if (m_imgData == nullptr)
    {
      kodi::Log(ADDON_LOG_ERROR, "Failed loading image: %s", path);
      load_next_image();
      return;
    }
  }
  else
  {
    m_imgData = nullptr;
  }

  m_imgLoaded = true;
  m_imgLoaderActive = false;
}

void CVisPictureIt::draw_image(GLuint img_tex_id, float opacity)
{
  /**
   * Draw the image with a certain opacity (opacity is used to cross-fade two images)
   */
  if (!img_tex_id)
  {
    return;
  }

  sLight framedTextures[4];

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  glBindTexture(GL_TEXTURE_2D, img_tex_id);

  if (!img_tex_id)
  {
    framedTextures[0].color = framedTextures[1].color = framedTextures[2].color = framedTextures[3].color = sColor(0.0f, 0.0f, 0.0f, opacity);
  }
  else
  {
    framedTextures[0].color = framedTextures[1].color = framedTextures[2].color = framedTextures[3].color = sColor(1.0f, 1.0f, 1.0f, opacity);
  }

  framedTextures[0].vertex = sPosition(-1.0f, -1.0f);
  framedTextures[0].coord = sCoord(0.0f, 0.0f);
  framedTextures[1].vertex = sPosition(1.0f, -1.0f);
  framedTextures[1].coord = sCoord(1.0f, 0.0f);
  framedTextures[2].vertex = sPosition(1.0f, 1.0f);
  framedTextures[2].coord = sCoord(1.0f, 1.0f);
  framedTextures[3].vertex = sPosition(-1.0f, 1.0f);
  framedTextures[3].coord = sCoord(0.0f, 1.0f);
  m_textureUsed = true;
  EnableShader();
  glBufferData(GL_ARRAY_BUFFER, sizeof(sLight)*4, framedTextures, GL_STATIC_DRAW);
  glDrawElements(GL_TRIANGLE_STRIP, 4, GL_UNSIGNED_BYTE, 0);
  DisableShader();

  glDisable(GL_BLEND);
}

void CVisPictureIt::draw_bars(int i, GLfloat x1, GLfloat x2)
{
  /**
   * Draw a single bar
   * i = index of the bar from left to right
   * x1 + x2 = width and position of the bar
   */

  if (::fabs(m_cvisBarHeights[i] - m_visBarHeights[i]) > 0)
  {
    // The bigger the difference between the current and previous heights, the faster
    // we want the bars to move.
    // The "10.0" is a random value I choose after some testing.
    float gravity = ::fabs(m_cvisBarHeights[i] - m_pvisBarHeights[i]) / 10.0;

    if (m_cvisBarHeights[i] < m_visBarHeights[i])
    {
      m_cvisBarHeights[i] += m_visAnimationSpeed + gravity;
    }
    else
    {
      m_cvisBarHeights[i] -= m_visAnimationSpeed + gravity;
    }
  }

  m_pvisBarHeights[i] = m_visBarHeights[i];
  GLfloat y2 = m_visBottomEdge - m_cvisBarHeights[i];

  sLight framedTextures[4];
  framedTextures[0].color = framedTextures[1].color = framedTextures[2].color = framedTextures[3].color = sColor(1.0f, 1.0f, 1.0f, 1.0f);

  framedTextures[0].vertex = sPosition(x1, y2);               // Top Left
  framedTextures[1].vertex = sPosition(x2, y2);               // Top Right
  framedTextures[2].vertex = sPosition(x2, m_visBottomEdge);  // Bottom Right
  framedTextures[3].vertex = sPosition(x1, m_visBottomEdge);  // Bottom Left
  glBufferData(GL_ARRAY_BUFFER, sizeof(sLight)*4, framedTextures, GL_STATIC_DRAW);
  glDrawElements(GL_TRIANGLE_STRIP, 4, GL_UNSIGNED_BYTE, 0);

  framedTextures[0].vertex = sPosition(-x2, y2);               // Top Left
  framedTextures[1].vertex = sPosition(-x1, y2);               // Top Right
  framedTextures[2].vertex = sPosition(-x1, m_visBottomEdge);  // Bottom Right
  framedTextures[3].vertex = sPosition(-x2, m_visBottomEdge);  // Bottom Left
  glBufferData(GL_ARRAY_BUFFER, sizeof(sLight)*4, framedTextures, GL_STATIC_DRAW);
  glDrawElements(GL_TRIANGLE_STRIP, 4, GL_UNSIGNED_BYTE, 0);
}

void CVisPictureIt::start_render()
{
  /**
   * Some initial OpenGL stuff
   */
  // Clear The Screen And The Depth Buffer
  glClear(GL_COLOR_BUFFER_BIT);

  glBindBuffer(GL_ARRAY_BUFFER, m_vertexVBO);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_indexVBO);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GLubyte)*4, m_index, GL_STATIC_DRAW);

  glVertexAttribPointer(m_hVertex, 4, GL_FLOAT, GL_TRUE, sizeof(sLight), BUFFER_OFFSET(offsetof(sLight, vertex)));
  glEnableVertexAttribArray(m_hVertex);

  glVertexAttribPointer(m_hColor, 4, GL_FLOAT, GL_TRUE, sizeof(sLight), BUFFER_OFFSET(offsetof(sLight, color)));
  glEnableVertexAttribArray(m_hColor);

  glVertexAttribPointer(m_hCoord, 2, GL_FLOAT, GL_TRUE, sizeof(sLight), BUFFER_OFFSET(offsetof(sLight, coord)));
  glEnableVertexAttribArray(m_hCoord);
}

void CVisPictureIt::finish_render()
{
  glDisableVertexAttribArray(m_hVertex);
  glDisableVertexAttribArray(m_hColor);
  glDisableVertexAttribArray(m_hCoord);
}

void CVisPictureIt::OnCompiledAndLinked()
{
  // Variables passed directly to the Vertex shader
  m_projMatLoc = glGetUniformLocation(ProgramHandle(), "u_projectionMatrix");
  m_modelViewMatLoc = glGetUniformLocation(ProgramHandle(), "u_modelViewMatrix");
  m_textureIdLoc = glGetUniformLocation(ProgramHandle(), "u_textureId");

  m_hVertex = glGetAttribLocation(ProgramHandle(), "a_vertex");
  m_hColor = glGetAttribLocation(ProgramHandle(), "a_color");
  m_hCoord = glGetAttribLocation(ProgramHandle(), "a_coord");
}

bool CVisPictureIt::OnEnabled()
{
  // This is called after glUseProgram()
  glUniformMatrix4fv(m_projMatLoc, 1, GL_FALSE, glm::value_ptr(m_projMat));
  glUniformMatrix4fv(m_modelViewMatLoc, 1, GL_FALSE, glm::value_ptr(m_modelMat));
  glUniform1i(m_textureIdLoc, m_textureUsed);

  return true;
}

ADDONCREATOR(CVisPictureIt)
