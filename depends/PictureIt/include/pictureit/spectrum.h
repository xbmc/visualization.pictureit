#pragma once

#include <GL/gl.h>

class Spectrum {

    private:
        GLfloat  animation_speed = 0.0f;  // Animation speed. The smaler the value, the slower AND smoother the
        GLfloat  bottom_edge     = 0.0f;  // If set to 1.0 the bars would be exactly on the screen edge (Kodi setting)

        int      bar_count;      // Amount of single bars to display (will be doubled as we
        GLfloat  *bar_heights;
        GLfloat  *pbar_heights;
        GLfloat  *cbar_heights;

    public:
        Spectrum(int bar_count, GLfloat animation_speed=0.007f, GLfloat bottom_edge=1.0f);
        ~Spectrum();

        void audio_data( const float *audio_data, int audio_data_length );

        void draw_bars( int i, GLfloat x1, GLfloat x2 );
        void draw_spectrum( GLfloat width );
};
