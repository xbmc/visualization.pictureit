#include "pictureit/spectrum.h"

#include <stdio.h>
#include <math.h>

Spectrum::Spectrum(int bar_count, GLfloat animation_speed, GLfloat bottom_edge) {
    this->bar_count    = bar_count;
    this->bar_heights  = new GLfloat[bar_count];
    this->cbar_heights = new GLfloat[bar_count];
    this->pbar_heights = new GLfloat[bar_count];

    this->animation_speed = animation_speed;
    this->bottom_edge     = bottom_edge;
}

Spectrum::~Spectrum() {}

void Spectrum::audio_data(const float *audio_data, int audio_data_length) {
    /**
    ToDo: Use a library for FFT and windowing
    - The values in :this->bar_heights: will be used to draw the bars and the values have to be between 0.0f and 1.0f
      (1.0f would mean the bar goes all the way up to the screen meaning we should define a max value)
    - :this->bar_count: tells us how many bars will be drawn
    - :this->cbar_heights|pbar_heights: are used by :draw_spectrum: to smoothen the animation and shall NOT be used here

    So all that this function has to do is write proper values into :this->bar_heights: whenever it gets called
    */
}


// Draw a single bar
//   i = index of the bar from left to right
//   x1 + x2 = width and position of the bar
void Spectrum::draw_bars( int i, GLfloat x1, GLfloat x2 ) {

    if ( ::fabs( cbar_heights[i] - bar_heights[i] ) > 0 ) {
        // The bigger the difference between the current and previous heights, the faster
        // we want the bars to move.
        // The "10.0" is a random value I choose after some testing.
        float gravity = ::fabs( cbar_heights[i] - pbar_heights[i] ) / 10.0;

        if ( cbar_heights[i] < bar_heights[i] )
            cbar_heights[i] += animation_speed + gravity;
        else
            cbar_heights[i] -= animation_speed + gravity;
    }

    pbar_heights[i] = bar_heights[i];
    GLfloat y2 = bottom_edge - cbar_heights[i];

    glBegin(GL_TRIANGLES);
        glVertex2f( x1, y2 );           // Top Left
        glVertex2f( x2, y2 );           // Top Right
        glVertex2f( x2, bottom_edge );  // Bottom Right
    glEnd();
    glBegin(GL_TRIANGLES);
        glVertex2f( x2, bottom_edge );  // Bottom Right
        glVertex2f( x1, bottom_edge );  // Bottom Left
        glVertex2f( x1, y2 );           // Top Left
    glEnd();

    // This is the mirrored part on the right side
    glBegin(GL_TRIANGLES);
        glVertex2f( -x2, y2 );           // Top Left
        glVertex2f( -x1, y2 );           // Top Right
        glVertex2f( -x1, bottom_edge );  // Bottom Right
    glEnd();
    glBegin(GL_TRIANGLES);
        glVertex2f( -x1, bottom_edge );  // Bottom Right
        glVertex2f( -x2, bottom_edge );  // Bottom Left
        glVertex2f( -x2, y2 );           // Top Left
    glEnd();

}

void Spectrum::draw_spectrum( GLfloat width ) {
    // The mirrored bars will get drawn within the "draw_bars".
    // This needs to be done to ensure the exact same height-value for both
    // the left and the (mirrored) right bar
    glPushMatrix();
        glColor3f( 1.0f, 1.0f, 1.0f );

        GLfloat x1, x2;
        float bar_width = width / bar_count;
        for ( int i = 1; i <= bar_count; i++ ) {
            // calculate position
            x1 = ( width * -1 ) + ( i * bar_width ) - bar_width;
            x2 = ( width * -1 ) + ( i * bar_width );

            // "add" a gap (which is 1/4 of the initial bar_width)
            // to both the left and right side of the bar
            x1 = x1 + ( bar_width / 4 );
            x2 = x2 - ( bar_width / 4 );

            draw_bars( (i-1), x1, x2 );
        }
    glPopMatrix();
}
