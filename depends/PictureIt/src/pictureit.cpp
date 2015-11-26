#include "pictureit/pictureit.h"
#include "pictureit/utils.h"

#include <GL/gl.h>
//#include <fnmatch.h>
//#include <dirent.h>


PictureIt::~PictureIt() {
    delete EFX;
    delete SPECTRUM;

    glDeleteTextures(2, img_texture_ids);
}

void PictureIt::init() {
    switch (efx) {
        case EFFECT::Crossfade:
            EFX = new EFXCrossfade();
            break;
    }

    if ( spectrum_enabled )
        SPECTRUM = new Spectrum(spectrum_bar_count);

    glGenTextures(2, img_texture_ids);
}

void PictureIt::start_render() {
    // save OpenGL original state
    glPushAttrib( GL_ENABLE_BIT | GL_TEXTURE_BIT );

    // Clear The Screen And The Depth Buffer
    glClear( GL_COLOR_BUFFER_BIT );

    // OpenGL projection matrix setup
    glMatrixMode( GL_PROJECTION );
    glPushMatrix();
    glLoadIdentity();

    // Coordinate-System:
    //     screen top left:     ( -1, -1 )
    //     screen center:       (  0,  0 )
    //     screen bottom right: (  1,  1 )
    glOrtho( -1, 1, 1, -1, -1, 1 );

    glMatrixMode( GL_MODELVIEW );
    glPushMatrix();
    glLoadIdentity();
}

void PictureIt::finish_render() {
    // return OpenGL matrices to original state
    glPopMatrix();
    glMatrixMode( GL_PROJECTION );
    glPopMatrix();

    // restore OpenGl original state
    glPopAttrib();
}

const char* PictureIt::get_random_image() {
    int index = rand() % images.size();

    return images[index].c_str();
}

bool PictureIt::render() {
    start_render();

    if ( update_by_interval && effect_finished && time(0) >= ( img_last_updated + img_update_interval ))
        image_update = true;

    if (image_update == true) {
        img_last_updated = time(0);
        effect_finished = false;
        image_update    = false;

        bool success = PI_UTILS::load_image( get_random_image(), img_texture_ids[1] );

        if ( ! success ) {
            // Faild loading image, so when drawing the next frame we immediatelly try to get a new one
            // If we'd do it in the "load_image" methode we could end up in an endless-loop in the main-thread
            // if only broken images are available within a preset
            image_update = true;
        }
    }

    if ( effect_finished ) {
        // From now on we keep drawing the current image ourself up to the point
        // where a new image will be displayed which will be done by an effect again
        PI_UTILS::draw_image(img_texture_ids[0]);
    } else {
        if ( glIsTexture(img_texture_ids[0]) )
            effect_finished = EFX->render(img_texture_ids[0], img_texture_ids[1]);
        else
            effect_finished = EFX->render(0, img_texture_ids[1]);

        // Effect finished, therefore we have to swapp the position of both textures
        if (effect_finished) {
            swap(img_texture_ids[0], img_texture_ids[1]);

            // e.g. Kodi needs that. Without it, it looks like one frame is missing once
            // the effect finished.
            // It doesn't hurt so it's fine for now
            PI_UTILS::draw_image(img_texture_ids[0]);
        }
    }

    if ( SPECTRUM && spectrum_enabled )
        SPECTRUM->draw_spectrum(1.0f);

    finish_render();

    return effect_finished;
}

void PictureIt::audio_data( const float *audio_data, int audio_data_length ) {
    if ( SPECTRUM && spectrum_enabled )
        SPECTRUM->audio_data( audio_data, audio_data_length );
}
void PictureIt::update_image(bool force_update) {
    if (force_update)
        image_update = true;
    else
        if (effect_finished)
            image_update = true;
}

void PictureIt::load_images(const char *image_root_dir) {
    images.clear();
    PI_UTILS::list_dir(image_root_dir, images, true, true, image_filter, sizeof(image_filter));
}
