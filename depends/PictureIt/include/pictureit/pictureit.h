#pragma once

#include "pictureit/spectrum.h"

#include "pictureit/effects/effects.h"

#include <string>
#include <vector>
#include <sys/time.h>

using namespace std;

class PictureIt {
    private:
        /*
        :img_texture_ids: holds the texture-ids for images:
           0: The current displayed image.
           1: The next image which fades in.
        */
        GLuint img_texture_ids[2] =  {};
        bool   image_update       =  true;
        time_t img_last_updated     =  time(0);
        bool   effect_finished    =  true;

        vector<string> images;

        const char *image_filter[3] = { "*.jpg", "*.png", "*.jpeg" };

        void start_render();
        void finish_render();
        const char* get_random_image();

    public:
        EFXBase  *EFX      = NULL;
        Spectrum *SPECTRUM = NULL;

        // Values that can be configured by whoever implements PictureIt
        EFFECT efx                  = EFFECT::Crossfade;
        bool   update_by_interval   = true;
        int    img_update_interval  = 180;

        bool   spectrum_enabled     = true;
        int    spectrum_bar_count   = 64;
        float  spectrum_width       = 1.0f;

        ~PictureIt();

        void init();

        //void configure(const char *key, int value);
        void update_image(bool force_update = false);

        void load_images(const char *image_root_dir);
        
        bool render();

        void audio_data( const float *audio_data, int audio_data_length );
};