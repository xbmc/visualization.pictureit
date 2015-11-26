#include "pictureit/effects/crossfade.h"
#include "pictureit/utils.h"

#include <stdio.h>
#include <string>
#include <sys/time.h>

void EFXCrossfade::configure(const char *key, int value) {
    std::string k = key;
    if ( k == "fade_time_ms" )
        fade_time_ms = value;
}

bool EFXCrossfade::render(GLuint old_texture, GLuint new_texture) {
    // Very first frame so we reset/initialize all variables properly
    if ( initial ) {
        initial = false;
        fade_current = 0.0f;
        fade_offset_ms = get_current_time_ms() % fade_time_ms;
    }

    // Fade out old image
    if ( fade_current < 1.0f )
        PI_UTILS::draw_image( old_texture, 0.0f, 0.0f, 1.0f - fade_current );

    // Fade in new image
    if ( fade_offset_ms && fade_current < 1.0f ) {
        fade_current = (float) ( ( get_current_time_ms() - fade_offset_ms ) % fade_time_ms ) / fade_time_ms;

        if (fade_current < fade_last) {
            fade_last       = 0.0f;
            fade_current    = 1.0f;
            fade_offset_ms  = 0;
        } else {
            fade_last = fade_current;
        }

        PI_UTILS::draw_image( new_texture, 0.0f, 0.0f, fade_current );
        return false;
    }

    // When we reach this point, the crossfade effect finished. Therefor
    // we set :initial: back to :true: so that the values get initialized again
    // when this effect gets called the next time
    initial = true;

    // We let whoever used us know that the we are done with our effect
    return true;
}

long int EFXCrossfade::get_current_time_ms() {
    struct timeval current_time;
    gettimeofday( &current_time, NULL );

    return current_time.tv_sec * 1000 + current_time.tv_usec / 1000;
}
