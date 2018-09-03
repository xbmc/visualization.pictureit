#include <kodi/addon-instance/Visualization.h>

#include <algorithm>
#include <atomic>
#include <dirent.h>
#include <fnmatch.h>
#include <GL/gl.h>
#include <map>
#include <random>
#include <sys/time.h>
#include <thread>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#define STBI_ONLY_BMP
#include "stb_image.h"

#include "mrfft.h"


typedef std::vector<std::string> td_vec_str;
typedef std::map<std::string, td_vec_str> td_map_data;

static const char *img_filter[] = { "*.jpg", "*.jpeg", "*.png" };


#define log(ADDON_LOG_DEBUG, ...) _log(ADDON_LOG_DEBUG, __VA_ARGS__, NULL)
void _log(const char *arg, ... ) {
    va_list arguments;

    fprintf(stdout, "[visualization.pictureit]:");
    for (va_start(arguments, arg); arg != NULL;
            arg = va_arg(arguments, const char *)) {
        fprintf(stdout, " %s", arg);
    }
    fprintf(stdout, "\n");

    va_end(arguments);
}


static long int get_current_time_ms() {
    struct timeval current_time;
    gettimeofday(&current_time, NULL);

    return current_time.tv_sec * 1000 + current_time.tv_usec / 1000;
}


class CVisPictureIt : public kodi::addon::CAddonBase,
                      public kodi::addon::CInstanceVisualization {
public:
    CVisPictureIt();
    virtual ~CVisPictureIt();

    virtual ADDON_STATUS Create() override;
    virtual bool GetPresets(std::vector<std::string>& presets) override;
    virtual int GetActivePreset() override;
    virtual bool PrevPreset() override;
    virtual bool NextPreset() override;
    virtual bool LoadPreset(int select) override;
    virtual bool RandomPreset() override;
    virtual bool Start(int channels, int samplesPerSec, int bitsPerSample,
                       std::string songName) override;
    virtual void Stop() override;
    virtual void Render() override;
    virtual void AudioData(const float*, int, float*, int) override;
    virtual bool UpdateTrack(const VisTrack &track) override;

    /*
     * Settings
     */
    bool update_by_interval = true;
    bool update_on_new_track = true;
    int img_update_interval = 180;
    int vis_enabled = true;
    int vis_bg_enabled = true;

    // Used to define some "padding" left and right.
    // If set to 1.0 the bars will go to the screen edge
    GLfloat vis_width = 0.8f;

    // If set to 1.0 the bars would be exactly on the screen edge
    GLfloat vis_bottom_edge = 0.98f;

    // Animation speed. The smaler the value, the slower
    // and smoother the animations
    GLfloat vis_animation_speed = 0.007f;

private:
    std::string path_join(std::string a, std::string b);
    int list_dir(const char *path, td_vec_str &store, bool recursive = false,
                bool incl_full_path = true, int filter_size = 0,
                const char *file_filter[] = NULL);
    int get_next_img_pos();
    void load_presets(const char* path);
    void load_data(const char* path);
    void select_preset(unsigned int index);
    void load_next_image();
    void draw_image(GLuint img_tex_id, float opacity);
    void draw_bars(int i, GLfloat x1, GLfloat x2);
    void start_render();
    void finish_render();

    std::thread *data_loader;
    std::atomic<bool> data_loader_active;
    std::atomic<bool> data_loaded;

    std::thread *img_loader;
    std::atomic<bool> img_loader_active;
    std::atomic<bool> img_loaded;

    unsigned char *img_data;
    int img_width, img_height, img_channels = 0;

    std::unique_ptr<MRFFT> tranform;

    /*
    * "img_tex_ids" holds the texture-ids for images:
    *  0: The current displayed image.
    *  1: The next image which fades in.
    *  2: The previously displayed image, which gets recycled.
    */
    GLuint img_tex_ids[3] = {};

    // When set to "true", a new image will be crossfaded
    bool update_img = false;

    // Root directory holding subfolders that will be used as presets and
    // searched for images
    std::string presets_root_dir = "";

    // Amount of presets (subfolders) found
    unsigned int presets_count = 0;

    // Index of the currently selected preset
    unsigned int preset_index = -1;

    // If random preset is active
    bool preset_random = false;

    // If current preset is locked
    bool preset_locked = false;

    int img_current_pos = 0;

    // Time (in sec.) where we last updated the image
    time_t img_last_updated = time(0);

    // The last alpha value of our image
    GLfloat fade_last = 0.0f;

    // The current alpha value of our image
    GLfloat fade_current = 0.0f;

    // How long the crossfade between two images will take (in ms)
    int fade_time_ms = 2000;

    // Used in combination with "fade_time_ms" to calculate the new alpha value
    // for the next frame
    int fade_offset_ms = 0;

    // Amount of single bars to display (will be doubled as we mirror them to
    // the right side)
    static const int vis_bar_count = 96;

    // The min height for each bar
    const GLfloat vis_bar_min_height = 0.02f;

    // The max height for each bar
    const GLfloat vis_bar_max_height = 0.18f;

    const float vis_bottom_edge_scale[11] = {
        1.0, 0.98, 0.96, 0.94, 0.92, 0.90, 0.88, 0.86, 0.84, 0.82, 0.80
    };

    // Whatever we get from AudioData
    GLfloat vis_bar_heights[vis_bar_count] = {};

    // The previous value we got from AudioData (used to calculate some form
    // of gravity. The bigger the difference, the faster the bars will move)
    GLfloat pvis_bar_heights[vis_bar_count] = {};

    // Used to smoothen the animation on a "per frame" basis.
    GLfloat cvis_bar_heights[vis_bar_count] = {};

    // Holds all preset-names in alphabetical order
    td_vec_str pi_presets;

    // Always holds the images for the currently selected preset and
    // will be updated upon preset change
    td_vec_str pi_images;

    // Map consisting of key = preset-name, value = vector of all associated
    // images
    td_map_data pi_data;

    int prev_freq_data_length = 0;
};


CVisPictureIt::CVisPictureIt() {
    kodi::Log(ADDON_LOG_DEBUG, "Constructor");

    data_loaded = false;
    img_loader_active = false;
    img_loaded = false;

    img_data == nullptr;
}

CVisPictureIt::~CVisPictureIt() {
    kodi::Log(ADDON_LOG_DEBUG, "Destructor");
    if (img_loader != nullptr) {
        if (data_loader->joinable()) {
            data_loader->join();
        }
        delete data_loader;
    }

    if (img_loader != nullptr) {
        if (img_loader->joinable()) {
            img_loader->join();
        }
        delete img_loader;
    }

    for (auto ptr : pi_data) {
        ptr.second.clear();
    }
    pi_data.clear();

    glDeleteTextures(3, img_tex_ids);
}

ADDON_STATUS CVisPictureIt::Create() {
    kodi::Log(ADDON_LOG_DEBUG, "Create");

    presets_root_dir = kodi::GetSettingString("presets_root_dir");

    update_on_new_track = kodi::GetSettingBoolean("update_on_new_track");
    update_by_interval = kodi::GetSettingBoolean("update_by_interval");
    img_update_interval = kodi::GetSettingInt("img_update_interval");
    fade_time_ms = kodi::GetSettingInt("fade_time_ms");
    vis_enabled = kodi::GetSettingBoolean("vis_enabled");
    vis_bg_enabled = kodi::GetSettingBoolean("vis_bg_enabled");

    vis_width = kodi::GetSettingInt("vis_half_width");
    vis_width = vis_width * 1.0f / 100;

    vis_animation_speed = kodi::GetSettingInt("vis_animation_speed");
    vis_animation_speed = vis_animation_speed * 0.005f / 100;

    float scale[] = {1.0, 0.98, 0.96, 0.94, 0.92, 0.90, 0.88, 0.86, 0.84,
                     0.82, 0.80};
    vis_bottom_edge = scale[kodi::GetSettingInt("vis_bottom_edge")];

    return ADDON_STATUS_OK;
}

bool CVisPictureIt::GetPresets(std::vector<std::string> &presets) {
    kodi::Log(ADDON_LOG_DEBUG, "GetPresets");

    load_presets(presets_root_dir.c_str());
    if (pi_presets.empty()) {
        return false;
    }

    for (unsigned int i = 0; i < pi_presets.size(); i++) {
        kodi::Log(ADDON_LOG_DEBUG, "Adding preset: %s", pi_presets[i].c_str());
        presets.push_back(pi_presets[i].c_str());
    }

    return true;
}

int CVisPictureIt::GetActivePreset() {
    kodi::Log(ADDON_LOG_DEBUG, "GetActivePreset");
    return static_cast<int>(preset_index);
}

bool CVisPictureIt::NextPreset() {
    kodi::Log(ADDON_LOG_DEBUG, "NextPreset");
    select_preset((preset_index + 1) % pi_presets.size());
    return true;
}

bool CVisPictureIt::PrevPreset() {
    kodi::Log(ADDON_LOG_DEBUG, "PrevPreset");
    select_preset((preset_index - 1) % pi_presets.size());
    return true;
}

bool CVisPictureIt::LoadPreset(int select) {
    kodi::Log(ADDON_LOG_DEBUG, "LoadPreset");
    select_preset(select % pi_presets.size());
    return true;
}

bool CVisPictureIt::RandomPreset() {
    kodi::Log(ADDON_LOG_DEBUG, "RandomPreset");
    select_preset((int)((std::rand() / (float)RAND_MAX) * pi_presets.size()));
    return true;
}

bool CVisPictureIt::Start(int iChannels, int iSamplesPerSec,
                          int iBitsPerSample, std::string szSongName) {
    kodi::Log(ADDON_LOG_DEBUG, "Start");
    data_loader = new std::thread (&CVisPictureIt::load_data, this,
                                    presets_root_dir.c_str());
    return true;
}

void CVisPictureIt::Stop() {
    kodi::Log(ADDON_LOG_DEBUG, "Stop");
}

bool CVisPictureIt::UpdateTrack(const VisTrack &track) {
    kodi::Log(ADDON_LOG_DEBUG, "UpdateTrack");
    if (update_on_new_track) {
        update_img = true;
    }
    return true;
}

void CVisPictureIt::Render() {
    start_render();

    // reached next update-intervall
    if (update_by_interval &&
            time(0) >= (img_last_updated + img_update_interval)) {
        update_img = true;
    }

    if (update_img && ! data_loader_active) {
        kodi::Log(ADDON_LOG_DEBUG, "Requesting new image...");
        update_img = false;
        img_last_updated = time(0);

        if (img_tex_ids[2] != 0) {
            glDeleteTextures(1, &img_tex_ids[2]);
        }

        if (!img_loader_active) {
            img_loader = new std::thread(&CVisPictureIt::load_next_image, this);
        }
    }

    if (img_loaded) {
        img_loaded = false;
        if (img_loader->joinable()) {
            img_loader->join();
        }
        delete img_loader;
        img_loader = nullptr;

        if (img_data != nullptr) {
            GLuint texture[1];
            glGenTextures(1, texture);

            glBindTexture(GL_TEXTURE_2D, texture[0]);
                        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, img_width, img_height, 0,
                        GL_RGBA, GL_UNSIGNED_BYTE, img_data);

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,     GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,     GL_CLAMP_TO_EDGE);

            glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);

            stbi_image_free(img_data);
            img_data = nullptr;

            img_tex_ids[1] = texture[0];
        }

        fade_current = 0.0f;
        fade_offset_ms = get_current_time_ms() % fade_time_ms;
    }

    // If we are within a crossfade, fade out the current image
    if (fade_current < 1.0f) {
        draw_image(img_tex_ids[0], 1.0f - fade_current);
    } else {
        draw_image(img_tex_ids[0], 1.0f);
    }

    if (fade_offset_ms && fade_current < 1.0f) {
        fade_current = ((float) ((get_current_time_ms() - fade_offset_ms)
                        % fade_time_ms) / fade_time_ms);

        if (fade_current < fade_last) {
            fade_last = 0.0f;
            fade_current = 1.0f;
            fade_offset_ms = 0;

            // Recycle the current image.
            img_tex_ids[2] = img_tex_ids[0];

            // Display the next image from now on.
            img_tex_ids[0] = img_tex_ids[1];
        } else {
            fade_last = fade_current;
        }

        draw_image(img_tex_ids[1], fade_current);
    }

    if (vis_enabled) {
        // If set to "true" we draw a transparent background which goes
        // behind the spectrum.
        if (vis_bg_enabled) {
            glPushMatrix();
                glEnable(GL_BLEND);
                glColor4f(0.0f, 0.0f, 0.0f, 0.7f);
                glBegin(GL_QUADS);
                    glVertex2f( 1.0f, (vis_bottom_edge - vis_bar_max_height) - (1.0f - vis_bottom_edge));
                    glVertex2f(-1.0f, (vis_bottom_edge - vis_bar_max_height) - (1.0f - vis_bottom_edge));
                    glVertex2f(-1.0f, 1.0f);
                    glVertex2f( 1.0f, 1.0f);
                glEnd();
                glDisable(GL_BLEND);
            glPopMatrix();
        }

        // Finally we draw all of our bars
        // The mirrored bars will get drawn within the "draw_bars".
        // This needs to be done to ensure the exact same height-value for both
        // the left and the (mirrored) right bar
        glPushMatrix();
            glColor3f(1.0f, 1.0f, 1.0f);

            GLfloat x1, x2;
            float bar_width = vis_width / vis_bar_count;
            for (int i = 1; i <= vis_bar_count; i++) {
                // calculate position
                x1 = (vis_width * -1) + (i * bar_width) - bar_width;
                x2 = (vis_width * -1) + (i * bar_width);

                // "add" a gap (which is 1/4 of the initial bar_width)
                // to both the left and right side of the bar
                x1 = x1 + (bar_width / 4);
                x2 = x2 - (bar_width / 4);

                draw_bars((i-1), x1, x2);
            }
        glPopMatrix();
    }

    finish_render();
}

void CVisPictureIt::AudioData(const float* pAudioData, int iAudioDataLength,
                              float *pFreqData, int iFreqDataLength) {
    if (! vis_enabled) {
        return;
    }

    iFreqDataLength = iAudioDataLength / 2;
    iFreqDataLength -= iFreqDataLength % 2;

    float freq_data[iFreqDataLength];

    // This part is essentially the same as what Kodi would do if we'd set "pInfo->bWantsFreq = true" (in the GetInfo methode)
    // However, even though Kodi can do windowing (Hann window) they set the flag for it to "false" (hardcoded).
    // So I just copied the "rfft.h" and "rfft.cpp", renamed the classe to "MRFFT" (otherwise we'd use the original) and set
    // the flag to "true".
    // Further this gives us the ability to change the response if needed (They return the magnitude per default I believe)
    if (prev_freq_data_length != iFreqDataLength || ! tranform) {
        tranform.reset(new MRFFT(iFreqDataLength, true));
        prev_freq_data_length = iFreqDataLength;
    }

    tranform->calc(pAudioData, freq_data);

    // Now that we have our values (as I said, the magnitude I guess) we just display them without a whole
    // lot of modification.
    // This is the part where the pre-processing should happen so we get a nice spectrum
    for (int i = 0; i < vis_bar_count; i++) {
        if (i >= iFreqDataLength) {
            break;
        }

        float height = freq_data[i];

        if (height > vis_bar_max_height) {
            height = vis_bar_max_height;
        } else if (height < vis_bar_min_height) {
            height = vis_bar_min_height;
        }

        vis_bar_heights[i] = height;
    }
}


std::string CVisPictureIt::path_join(std::string a, std::string b) {
    /**
     * Join file/folder path
     */

    // a ends with "/"
    if (a.substr(a.length() - 1, a.length()) == "/") {
        a = a.substr(0, a.size() - 1);
    }

    // b starts with "/"
    if (b.substr(0, 1) == "/") {
        b = b.substr(1, b.size());
    }

    // b ends with "/"
    if (b.substr(b.length() - 1, b.length()) == "/") {
        b = b.substr(0, b.size() -1);
    }

    return a + "/" + b;
}

int CVisPictureIt::list_dir(const char *path, td_vec_str &store,bool recursive,
                            bool incl_full_path, int filter_size,
                            const char *file_filter[]) {
    /**
     * List the contents of a directory (excluding "." files/folders)
     */
    std::string p = path;
    struct dirent *entry;
    DIR *dp;

    dp = opendir(path);
    if (dp == nullptr) {
        return false;
    }

    bool add = false;
    char* name;
    while ((entry = readdir(dp))) {
        name = entry->d_name;

        if (entry->d_type == DT_DIR && name && name[0] != '.') {
            if (! file_filter) {
                add = true;
            }

            if (recursive) {
                list_dir(path_join(p, name).c_str(), store, recursive,
                         incl_full_path, filter_size, file_filter);
            }
        } else if (entry->d_type != DT_DIR && name && name[0] != '.') {
            if (file_filter) {
                for (unsigned int i = 0; i < filter_size / sizeof(file_filter[0]); i++) {
                    if (fnmatch(file_filter[i], name, FNM_CASEFOLD) == 0) {
                        add = true;
                        break;
                    }
                }
            }
        }

        if (add) {
            if (incl_full_path) {
                store.push_back(path_join(p, name).c_str());
            } else {
                store.push_back(name);
            }
            add = false;
        }
    }

    closedir(dp);
    return 0;
}

int CVisPictureIt::get_next_img_pos() {
    std::random_device rd;
    std::mt19937 engine{rd()};
    std::uniform_int_distribution<int> dist(0, pi_images.size() - 1);

    int num = dist(engine);
    if (num == img_current_pos)
        return get_next_img_pos();

    img_current_pos = num;
    return img_current_pos;
}

void CVisPictureIt::load_presets(const char* path) {
    kodi::Log(ADDON_LOG_DEBUG, "Loading presets...");

    if (path && !path[0]) {
        return;
    }

    list_dir(path, pi_presets, false, false, 0, NULL);
    std::sort(pi_presets.begin(), pi_presets.end());

    if (pi_presets.empty()) {
        pi_presets.push_back("Default");
    }
}

void CVisPictureIt::load_data(const char* path) {
    /**
     * Load presets and all associated images
     */
    kodi::Log(ADDON_LOG_DEBUG, "Gathering images...");
    data_loader_active = true;

    if (path && !path[0]) {
        data_loader_active = false;
        return;
    }

    td_vec_str images;
    if (pi_presets[0] == "Default" ) {
        list_dir(path, images, true, true, sizeof(img_filter), img_filter);
        pi_data[pi_presets[0]] = images;
        images.clear();
    } else {
        for (unsigned int i = 0; i < pi_presets.size(); i++) {
            list_dir(path_join(path, pi_presets[i]).c_str(), images, true,
                               true, sizeof(img_filter), img_filter);

            // Preset empty or can't be accessed
            if (images.size() <= 0) {
                pi_presets.erase(pi_presets.begin() + i);
                continue;
            }

            pi_data[pi_presets[i]] = images;
            images.clear();
        }
    }

    select_preset((rand() % pi_presets.size()));
    if (preset_index > -1 && pi_data.empty()) {
        kodi::Log(ADDON_LOG_DEBUG, "Selecting initial preset...");
    }

    data_loaded = true;
    data_loader_active = false;
}

void CVisPictureIt::select_preset(unsigned int index) {
    /**
     * Select preset at "index"
     */
    if (index >= pi_presets.size()) {
        return;
    }

    preset_index = index;
    pi_images = pi_data[pi_presets[preset_index]];

    update_img = true;
}

void CVisPictureIt::load_next_image() {
    img_loader_active = true;

    if (! pi_images.empty()) {
        auto path = pi_images[get_next_img_pos()].c_str();

        kodi::Log(ADDON_LOG_DEBUG, "Loading image: %s", path);
        img_data = stbi_load(path, &img_width, &img_height, &img_channels,
                            STBI_rgb_alpha);

        if (img_data == nullptr) {
            kodi::Log(ADDON_LOG_ERROR,
                      "Failed loading image: %s", path);
            load_next_image();
            return;
        }
    } else {
        img_data = nullptr;
    }

    img_loaded = true;
    img_loader_active = false;
}

void CVisPictureIt::draw_image(GLuint img_tex_id, float opacity) {
    /**
     * Draw the image with a certain opacity (opacity is used to cross-fade two images)
     */
    if (! img_tex_id) {
        return;
    }

    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glBindTexture(GL_TEXTURE_2D, img_tex_id);

    if (! img_tex_id) {
        glColor4f(0.0f, 0.0f, 0.0f, opacity);
    } else {
        glColor4f(1.0f, 1.0f, 1.0f, opacity);
    }

    glBegin(GL_TRIANGLES);
        glTexCoord2f(0.0f, 1.0f); glVertex2f(-1.0f,  1.0f);  // Bottom Left
        glTexCoord2f(0.0f, 0.0f); glVertex2f(-1.0f, -1.0f);  // Top Left
        glTexCoord2f(1.0f, 0.0f); glVertex2f( 1.0f, -1.0f);  // Top Right
    glEnd();
    glBegin(GL_TRIANGLES);
        glTexCoord2f(0.0f, 1.0f); glVertex2f(-1.0f,  1.0f);  // Bottom Left
        glTexCoord2f(1.0f, 0.0f); glVertex2f( 1.0f, -1.0f);  // Top Right
        glTexCoord2f(1.0f, 1.0f); glVertex2f( 1.0f,  1.0f);  // Bottom Right
    glEnd();

    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);
}

void CVisPictureIt::draw_bars(int i, GLfloat x1, GLfloat x2) {
    /**
     * Draw a single bar
     * i = index of the bar from left to right
     * x1 + x2 = width and position of the bar
     */

    if (::fabs(cvis_bar_heights[i] - vis_bar_heights[i]) > 0) {
        // The bigger the difference between the current and previous heights, the faster
        // we want the bars to move.
        // The "10.0" is a random value I choose after some testing.
        float gravity = ::fabs(cvis_bar_heights[i] - pvis_bar_heights[i]) / 10.0;

        if (cvis_bar_heights[i] < vis_bar_heights[i]) {
            cvis_bar_heights[i] += vis_animation_speed + gravity;
        } else {
            cvis_bar_heights[i] -= vis_animation_speed + gravity;
        }
    }

    pvis_bar_heights[i] = vis_bar_heights[i];
    GLfloat y2 = vis_bottom_edge - cvis_bar_heights[i];

    glBegin(GL_QUADS);
        glVertex2f(x1, y2);               // Top Left
        glVertex2f(x2, y2);               // Top Right
        glVertex2f(x2, vis_bottom_edge);  // Bottom Right
        glVertex2f(x1, vis_bottom_edge);  // Bottom Left
    glEnd();

    // This is the mirrored part on the right side
    glBegin(GL_QUADS);
        glVertex2f(-x2, y2);               // Top Left
        glVertex2f(-x1, y2);               // Top Right
        glVertex2f(-x1, vis_bottom_edge);  // Bottom Right
        glVertex2f(-x2, vis_bottom_edge);  // Bottom Left
    glEnd();
}

void CVisPictureIt::start_render() {
    /**
     * Some initial OpenGL stuff
     */
    // save OpenGL original state
    glPushAttrib(GL_ENABLE_BIT | GL_TEXTURE_BIT);

    // Clear The Screen And The Depth Buffer
    glClear(GL_COLOR_BUFFER_BIT);

    // OpenGL projection matrix setup
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();

    // Coordinate-System:
    //     screen top left:     (-1, -1)
    //     screen center:       ( 0,  0)
    //     screen bottom right: ( 1,  1)
    glOrtho(-1, 1, 1, -1, -1, 1);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
}

void CVisPictureIt::finish_render() {
    /**
     * inishing off OpenGl
     */
    // return OpenGL matrices to original state
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();

    // restore OpenGl original state
    glPopAttrib();
}

ADDONCREATOR(CVisPictureIt)
