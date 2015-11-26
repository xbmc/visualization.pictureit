#pragma once

#include <string>
#include <vector>

#include <GL/gl.h>

using namespace std;

namespace PI_UTILS {
    const char* path_join(string a, string b);
    int list_dir(const char *path, vector<string> &store, bool recursive = false, bool incl_full_path = true, const char *file_filter[] = NULL, int filter_size = 0);

    bool load_image(const char *img_path, GLuint texture_id);
    void draw_image(GLuint texture_id, float x = 0, float y = 0, float opacity = 1.0f);
}