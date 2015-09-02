#include "xbmc_vis_dll.h"

#include <stdio.h>
#include <sys/time.h>

#include <math.h>
#include <algorithm>

#include <string>
#include <vector>
#include <map>

#include <dirent.h>
#include <fnmatch.h>

#include <GL/gl.h>
#include "SOIL.h"

/*
* "img_tex_ids" holds the texture-ids for images:
*  0: The current displayed image.
*  1: The next image which fades in.
*  2: The previously displayed image, which gets recycled.
*/
GLuint                 img_tex_ids[3]          =    {};

bool                   update_img              =    false;               // When set to "true", a new image will be crossfaded

const char*            presets_root_dir        =    "";                  // Root directory holding subfolders that will be used as presets and searched for images
unsigned int           presets_count           =    0;                   // Amount of presets (subfolders) found
unsigned int           preset_index            =    0;                   // Index of the currently selected preset
bool                   preset_random           =    false;               // If random preset is active
bool                   preset_locked           =    false;               // If current preset is locked

bool                   update_by_interval      =    true;                // If we should update by intervall (Kodi setting)
bool                   update_on_new_track     =    true;                // If we should update on new track (Kodi setting)

static int             img_update_interval     =    180;                 // How often the images will get updated (in sec.) (Kodi setting)
int                    img_current_pos         =    0;
time_t                 img_last_updated        =    time(0);             // Time (in sec.) where we last updated the image

GLfloat                fade_last               =    0.0f;                // The last alpha value of our image
GLfloat                fade_current            =    0.0f;                // The current alpha value of our image
int                    fade_time_ms            =    2000;                // How long the crossfade between two images will take (in ms)
int                    fade_offset_ms          =    0;                   // Used in combination with "fade_time_ms" to calculate the new alpha value for the next frame

int                    vis_enabled             =    true;                // If the spectrum is enabled (Kodi setting)
int                    vis_bg_enabled          =    true;                // If the transparent background behind the spectrum is enabled (Kodi setting)
const int              vis_bar_count           =    96;                  // Amount of single bars to display (will be doubled as we mirror them to the right side)
GLfloat                vis_width               =    0.8f;                // Used to define some "padding" left and right. If set to 1.0 the bars will go to the screen edge (Kodi setting)
GLfloat                vis_bottom_edge         =    0.98f;               // If set to 1.0 the bars would be exactly on the screen edge (Kodi setting)
const GLfloat          vis_bar_min_height      =    0.02f;               // The min height for each bar
const GLfloat          vis_bar_max_height      =    0.18f;               // The max height for each bar
GLfloat                vis_animation_speed     =    0.007f;              // Animation speed. The smaler the value, the slower AND smoother the animations (Kodi setting)
const float            vis_bottom_edge_scale[] =    { 1.0, 0.98, 0.96, 0.94, 0.92, 0.90, 0.88, 0.86, 0.84, 0.82, 0.80 };

// In here we store the audiodata used to "visualize" our bars.
// vis_bar_heights  = whatever we get from AudioData
// pvis_bar_heights = the previous value we got from AudioData (used to calculate some form of gravity. The bigger the difference, the faster the bars will move)
// cvis_bar_heights = used to smoothen the animation on a "per frame" basis.
GLfloat                vis_bar_heights[vis_bar_count], pvis_bar_heights[vis_bar_count], cvis_bar_heights[vis_bar_count] = {};

typedef     std::vector<std::string>              td_vec_str;
typedef     std::map<std::string, td_vec_str>     td_map_data;

td_vec_str      pi_presets;     // Holds all preset-names in alphabetical order
td_vec_str      pi_images;      // Always holds the images for the currently selected preset and will be updated upon preset change
td_map_data     pi_data;        // Map consisting of key = preset-name, value = vector of all associated images


static const char *img_filter[] = { "*.jpg", "*.png", "*.jpeg" };

static long int get_current_time_ms() {
    struct timeval current_time;
    gettimeofday( &current_time, NULL );

    return current_time.tv_sec * 1000 + current_time.tv_usec / 1000;
}

// Join file/folder path
const char* path_join(std::string a, std::string b) {
    // Apparently Windows does understand a "/" just fine...
    // Haven't tested it though, but for now I'm just believing it

    // a ends with "/"
    if ( a.substr( a.length() - 1, a.length() ) == "/" )
        a = a.substr( 0, a.size() - 1 );

    // b starts with "/"
    if ( b.substr( 0, 1 ) == "/" )
        b = b.substr( 1, b.size() );

    // b ends with "/"
    if ( b.substr( b.length() - 1, b.length() ) == "/" )
        b = b.substr( 0, b.size() -1 );

    return ( a + "/" + b ).c_str();
}

// List the contents of a directory (excluding "." files/folders)
int list_dir(const char *path, td_vec_str &store, bool recursive = false, bool incl_full_path = true, int filter_size = 0, const char *file_filter[] = NULL ) {
    std::string p = path;
    struct dirent *entry;
    DIR *dp;

    dp = opendir(path);
    if (dp == NULL)
        return false;

    bool add = false;
    char* name;
    while ((entry = readdir(dp))) {
        name = entry->d_name;

        if ( entry->d_type == DT_DIR && name && name[0] != '.' ) {
            if ( ! file_filter )
                add = true;

            if ( recursive )
                return list_dir( path_join( p, name), store, recursive, incl_full_path, filter_size, file_filter );
        } else if ( entry->d_type != DT_DIR && name && name[0] != '.' ) {
            if ( file_filter ) {
                for ( unsigned int i = 0; i < filter_size / sizeof( file_filter[0] ); i++) {
                    if ( fnmatch( file_filter[i], name, FNM_CASEFOLD ) == 0) {
                        add = true;
                        break;
                    }
                }
            }
        }

        if ( add ) {
            if ( incl_full_path )
                store.push_back( path_join( p, name ) );
            else
                store.push_back( name );
            add = false;
        }
    }

    closedir(dp);
    return 0;
}

int get_next_img_pos() {
    if ( (unsigned)img_current_pos < ( pi_images.size()-1 ) )
        img_current_pos++;
    else
        img_current_pos = 0;
    return img_current_pos;
}

// Load presets and all associated images
void load_data( const char* path ) {
    if ( path && !path[0] )
        return;

    list_dir( path, pi_presets, false, false, 0, NULL );
    std::sort( pi_presets.begin(), pi_presets.end() );


    td_vec_str images;

    if ( ! pi_presets.empty() ) {
        for ( unsigned int i = 0; i < pi_presets.size(); i++ ) {
            list_dir( path_join( path, pi_presets[i] ), images, true, true, sizeof(img_filter), img_filter );

            // Preset empty or can't be accessed
            if ( images.size() <= 0 ) {
                pi_presets.erase(pi_presets.begin() + i);
                continue;
            }

            // Randomize our new image-set
            std::random_shuffle( images.begin(), images.end() );

            pi_data[pi_presets[i]] = images;

            images.clear();
        }

    } else {
        // No presets found, let's see if we can't find images in the root-dir
        // itself and add it to a "Default" preset
        pi_presets.push_back( "Default" );
        list_dir( path, images, true, true, sizeof(img_filter), img_filter );

        std::random_shuffle( images.begin(), images.end() );

        pi_data[pi_presets[0]] = images;

        images.clear();
    }
}

// Select preset at "index"
void select_preset( unsigned int index ) {
    if ( index >= pi_presets.size() )
        return;

    preset_index = index;
    pi_images = pi_data[ pi_presets[preset_index] ];

    update_img = true;
}

// Load an image into an OpenGL texture and return the texture-id
GLuint load_image( GLuint img_tex_id = 0 ) {
    if ( pi_images.empty() )
        return 0;

    int pos = get_next_img_pos();
    if ( ! img_tex_id )
        img_tex_id = SOIL_load_OGL_texture( pi_images[pos].c_str(), SOIL_LOAD_AUTO, SOIL_CREATE_NEW_ID, SOIL_FLAG_INVERT_Y );
    else
        img_tex_id = SOIL_load_OGL_texture( pi_images[pos].c_str(), SOIL_LOAD_AUTO, img_tex_id, SOIL_FLAG_INVERT_Y );

    if ( ! img_tex_id )
        return 0;

    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,  GL_LINEAR );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,  GL_LINEAR );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,      GL_CLAMP_TO_EDGE );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,      GL_CLAMP_TO_EDGE );

    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);

    return img_tex_id;
}

// Draw the image with a certain opacity (opacity is used to cross-fade two images)
void draw_image( GLuint img_tex_id, float opacity ) {
    if ( ! img_tex_id )
        return;

    glEnable( GL_TEXTURE_2D );
    glEnable( GL_BLEND );

    glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );

    glBindTexture( GL_TEXTURE_2D, img_tex_id );

    if ( ! img_tex_id )
        glColor4f( 0.0f, 0.0f, 0.0f, opacity );
    else
        glColor4f( 1.0f, 1.0f, 1.0f, opacity );

    glBegin( GL_QUADS );
        glTexCoord2f( 0.0f, 1.0f ); glVertex2f( -1.0f, -1.0f );
        glTexCoord2f( 1.0f, 1.0f ); glVertex2f(  1.0f, -1.0f );
        glTexCoord2f( 1.0f, 0.0f ); glVertex2f(  1.0f,  1.0f );
        glTexCoord2f( 0.0f, 0.0f ); glVertex2f( -1.0f,  1.0f );
    glEnd();

    glDisable( GL_TEXTURE_2D );
    glDisable( GL_BLEND );
}

// Draw a single bar
//   i = index of the bar from left to right
//   x1 + x2 = width and position of the bar
void draw_bars( int i, GLfloat x1, GLfloat x2 ) {

    if ( ::fabs( cvis_bar_heights[i] - vis_bar_heights[i] ) > 0 ) {
        // The bigger the difference between the current and previous heights, the faster
        // we want the bars to move.
        // The "10.0" is a random value I choose after some testing.
        float gravity = ::fabs( cvis_bar_heights[i] - pvis_bar_heights[i] ) / 10.0;

        if ( cvis_bar_heights[i] < vis_bar_heights[i] )
            cvis_bar_heights[i] += vis_animation_speed + gravity;
        else
            cvis_bar_heights[i] -= vis_animation_speed + gravity;
    }

    pvis_bar_heights[i] = vis_bar_heights[i];
    GLfloat y2 = vis_bottom_edge - cvis_bar_heights[i];

    glBegin(GL_QUADS);
        glVertex2f( x1, y2 );               // Top Left
        glVertex2f( x2, y2 );               // Top Right
        glVertex2f( x2, vis_bottom_edge );  // Bottom Right
        glVertex2f( x1, vis_bottom_edge );  // Bottom Left
    glEnd();

    // This is the mirrored part on the right side
    glBegin(GL_QUADS);
        glVertex2f( -x2, y2 );               // Top Left
        glVertex2f( -x1, y2 );               // Top Right
        glVertex2f( -x1, vis_bottom_edge );  // Bottom Right
        glVertex2f( -x2, vis_bottom_edge );  // Bottom Left
    glEnd();
}

// Some initial OpenGL stuff
void start_render() {
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

// Finishing off OpenGl
void finish_render() {
    // return OpenGL matrices to original state
    glPopMatrix();
    glMatrixMode( GL_PROJECTION );
    glPopMatrix();

    // restore OpenGl original state
    glPopAttrib();
}

//-- Render -------------------------------------------------------------------
// Called once per frame. Do all rendering here.
//-----------------------------------------------------------------------------
extern "C" void Render() {
    start_render();

    // reached next update-intervall
    if ( update_by_interval && time(0) >= ( img_last_updated + img_update_interval ))
        update_img = true;

    if ( update_img == true ) {
        update_img = false;
        img_last_updated = time(0);

        GLuint tex_id = load_image( img_tex_ids[2] );

        if ( tex_id ) {
            // Load a new image to the next position.
            img_tex_ids[1] = tex_id;
        } else {
            // Faild loading image, so when drawing the next frame we immediatelly try to get a new one
            // If we'd do it in the "load_image" methode we could end up in an endless-loop if only broken
            // images are available within a preset
            update_img = true;
        }

        fade_current = 0.0f;
        fade_offset_ms = get_current_time_ms() % fade_time_ms;

    }

    // If we are within a crossfade, fade out the current image
    if ( fade_current < 1.0f )
        draw_image( img_tex_ids[0], 1.0f - fade_current );
    else
        draw_image( img_tex_ids[0], 1.0f );

    if ( fade_offset_ms && fade_current < 1.0f ) {
        fade_current = (float) ( ( get_current_time_ms() - fade_offset_ms ) % fade_time_ms ) / fade_time_ms;

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

        draw_image( img_tex_ids[1], fade_current );
    }

    if ( vis_enabled ) {
        // If set to "true" we draw a transparent background which goes behind the spectrum
        if ( vis_bg_enabled ) {
            glPushMatrix();
                glEnable( GL_BLEND );
                glColor4f( 0.0f, 0.0f, 0.0f, 0.7f );
                glBegin( GL_QUADS );
                    glVertex2f(  1.0f, ( vis_bottom_edge - vis_bar_max_height ) - ( 1.0f - vis_bottom_edge ) );
                    glVertex2f( -1.0f, ( vis_bottom_edge - vis_bar_max_height ) - ( 1.0f - vis_bottom_edge ) );
                    glVertex2f( -1.0f, 1.0f );
                    glVertex2f(  1.0f, 1.0f );
                glEnd();
                glDisable( GL_BLEND );
            glPopMatrix();
        }

        // Finally we draw all of our bars
        // The mirrored bars will get drawn within the "draw_bars".
        // This needs to be done to ensure the exact same height-value for both
        // the left and the (mirrored) right bar
        glPushMatrix();
            glColor3f( 1.0f, 1.0f, 1.0f );

            GLfloat x1, x2;
            float bar_width = vis_width / vis_bar_count;
            for ( int i = 1; i <= vis_bar_count; i++ ) {
                // calculate position
                x1 = ( vis_width * -1 ) + ( i * bar_width ) - bar_width;
                x2 = ( vis_width * -1 ) + ( i * bar_width );

                // "add" a gap (which is 1/4 of the initial bar_width)
                // to both the left and right side of the bar
                x1 = x1 + ( bar_width / 4 );
                x2 = x2 - ( bar_width / 4 );

                draw_bars( (i-1), x1, x2 );
            }
        glPopMatrix();
    }

    finish_render();
}

//-- Create -------------------------------------------------------------------
// Called on load. Addon should fully initalize or return error status
//-----------------------------------------------------------------------------
ADDON_STATUS ADDON_Create( void* hdl, void* props ) {
    if ( ! props )
        return ADDON_STATUS_UNKNOWN;
    
    // Seed the psuedo-random number generator
    std::srand( time(0) );

    return ADDON_STATUS_NEED_SETTINGS;
}

extern "C" void Start(int iChannels, int iSamplesPerSec, int iBitsPerSample, const char* szSongName) {
    // Now we should have the user-settings loaded so we can try and get our data (presets/images)
    if ( pi_data.empty() ) {
        load_data( presets_root_dir );

        // If we have some data, we select a random preset
        if ( ! pi_data.empty() )
            select_preset( (rand() % pi_presets.size()) );
    }
}

// The includes and variables are defined here just because I'm still not too keen on the whole
// rfft thing
// At some point it might make sense to switch to FFTW for better performance once a proper spectrum
// is in place
#include "mrfft.h"
#include <memory>
std::unique_ptr<MRFFT> m_transform;
extern "C" void AudioData(const float* pAudioData, int iAudioDataLength, float *pFreqData, int iFreqDataLength) {
    if ( ! vis_enabled )
        return;

    iFreqDataLength = iAudioDataLength/2;
    float freq_data[iFreqDataLength];

    // This part is essentially the same as what Kodi would do if we'd set "pInfo->bWantsFreq = true" (in the GetInfo methode)
    // However, even though Kodi can do windowing (Hann window) they set the flag for it to "false" (hardcoded).
    // So I just copied the "rfft.h" and "rfft.cpp", renamed the classe to "MRFFT" (otherwise we'd use the original) and set
    // the flag to "true".
    // Further this gives us the ability to change the response if needed (They return the magnitude per default I believe)
    if ( ! m_transform )
        m_transform.reset(new MRFFT(iFreqDataLength, true));

    m_transform->calc(pAudioData, freq_data);

    // Now that we have our values (as I said, the magnitude I guess) we just display them without a whole
    // lot of modification.
    // This is the part where the pre-processing should happen so we get a nice spectrum
    for ( int i = 0; i < vis_bar_count; i++ ) {
        if ( i >= iFreqDataLength )
            break;

        float height = freq_data[i];

        if ( height > vis_bar_max_height )
            height = vis_bar_max_height;
        else if ( height < vis_bar_min_height )
            height = vis_bar_min_height;

        vis_bar_heights[i] = height;
    }
}


//-- GetInfo ------------------------------------------------------------------
// Tell XBMC our requirements
//-----------------------------------------------------------------------------
extern "C" void GetInfo(VIS_INFO* pInfo) {
    // We do the fft ourself because Kodi doesn't do windowing beforehand
    pInfo->bWantsFreq = false;
    pInfo->iSyncDelay = 0;
}


//-- GetSubModules ------------------------------------------------------------
// Return any sub modules supported by this vis
//-----------------------------------------------------------------------------
extern "C" unsigned int GetSubModules(char ***names) {
    return 0;
}

//-- OnAction -----------------------------------------------------------------
// Handle XBMC actions such as next preset, lock preset, album art changed etc
//-----------------------------------------------------------------------------
extern "C" bool OnAction(long flags, const void *param) {
    bool ret = true;

    switch ( flags ) {
        case VIS_ACTION_LOAD_PRESET:
            if ( param )
                select_preset( (*( (int*) param )) );
            else
                ret = false;
            break;
        case VIS_ACTION_NEXT_PRESET:
            if ( ! preset_locked ) {
                if ( preset_random ) {
                        select_preset( rand() % pi_presets.size() );
                } else {
                    preset_index += 1;
                    if ( preset_index >= pi_presets.size() )
                        preset_index = 0;
                }
            }
            break;
        case VIS_ACTION_PREV_PRESET:
            if ( ! preset_locked ) {
                if ( preset_random ) {
                        select_preset( rand() % pi_presets.size() );
                } else {
                    preset_index -= 1;
                    if ( preset_index < 0 )
                        preset_index = pi_presets.size();
                }
            }
            break;
        case VIS_ACTION_RANDOM_PRESET:
            preset_random = !preset_random;
            break;
        case VIS_ACTION_LOCK_PRESET:
            preset_locked = !preset_locked;
            break;
        case VIS_ACTION_UPDATE_TRACK:
            if ( update_on_new_track )
                update_img = true;
            break;
        default:
            ret = false;
    }

    return ret;
}

//-- GetPresets ---------------------------------------------------------------
// Return a list of presets to XBMC for display
//-----------------------------------------------------------------------------
extern "C" unsigned int GetPresets(char ***presets) {
    if ( pi_data.empty() )
        load_data( presets_root_dir );

    if ( pi_data.empty() )
        return 0;

    presets_count = pi_presets.size();
    char **g_presets = (char**) malloc( sizeof(char*) * presets_count );

    for( unsigned int i = 0; i < presets_count; i++ )
        g_presets[i] = (char*) pi_presets[i].c_str();

    *presets = g_presets;

    return presets_count;
}

//-- GetPreset ----------------------------------------------------------------
// Return the index of the current playing preset
//-----------------------------------------------------------------------------
extern "C" unsigned GetPreset() {
    return preset_index;
}

//-- IsLocked -----------------------------------------------------------------
// Returns true if this add-on use settings
//-----------------------------------------------------------------------------
extern "C" bool IsLocked() {
    return preset_locked;
}

//-- Stop ---------------------------------------------------------------------
// This dll must cease all runtime activities
// !!! Add-on master function !!!
//-----------------------------------------------------------------------------
extern "C" void ADDON_Stop() {}

//-- Destroy ------------------------------------------------------------------
// Do everything before unload of this add-on
// !!! Add-on master function !!!
//-----------------------------------------------------------------------------
extern "C" void ADDON_Destroy() {}

//-- HasSettings --------------------------------------------------------------
// Returns true if this add-on use settings
// !!! Add-on master function !!!
//-----------------------------------------------------------------------------
extern "C" bool ADDON_HasSettings() {
    return true;
}

//-- GetStatus ---------------------------------------------------------------
// Returns the current Status of this visualisation
// !!! Add-on master function !!!
//-----------------------------------------------------------------------------
extern "C" ADDON_STATUS ADDON_GetStatus() {
    return ADDON_STATUS_OK;
}

//-- GetSettings --------------------------------------------------------------
// Return the settings for XBMC to display
// !!! Add-on master function !!!
//-----------------------------------------------------------------------------
extern "C" unsigned int ADDON_GetSettings(ADDON_StructSetting ***sSet) {
    return 0;
}

//-- FreeSettings --------------------------------------------------------------
// Free the settings struct passed from XBMC
// !!! Add-on master function !!!
//-----------------------------------------------------------------------------

extern "C" void ADDON_FreeSettings() {}

//-- SetSetting ---------------------------------------------------------------
// Set a specific Setting value (called from XBMC)
// !!! Add-on master function !!!
//-----------------------------------------------------------------------------
extern "C" ADDON_STATUS ADDON_SetSetting( const char *strSetting, const void* value ) {
    if ( ! strSetting || ! value )
        return ADDON_STATUS_UNKNOWN;

    std::string str = strSetting;

    if ( str == "presets_root_dir" ) {
        const char* dir = (const char*) value;
        if ( dir && !dir[0] )
            return ADDON_STATUS_NEED_SETTINGS;
        presets_root_dir = dir;
    }

    if ( str == "update_on_new_track" )
        update_on_new_track = *(bool*)value;

    if ( str == "update_by_interval" )
        update_by_interval = *(bool*)value;

    if ( str == "img_update_interval" )
        img_update_interval = (*(int*) value) * 60;

    if ( str == "fade_time_ms" )
        fade_time_ms = (*(int*) value) * 1000;

    if ( str == "vis_enabled" )
        vis_enabled = *(bool*)value;

    if ( str == "vis_bg_enabled" )
        vis_bg_enabled = *(bool*)value;

    if ( str == "vis_half_width" )
        vis_width = ((*(int*) value) * 1.0f / 100);

    if ( str == "vis_bottom_edge" ) {
        float scale[] = { 1.0, 0.98, 0.96, 0.94, 0.92, 0.90, 0.88, 0.86, 0.84, 0.82, 0.80 };
        vis_bottom_edge = scale[(*(int*) value)];
    }

    if ( str == "vis_animation_speed" )
        vis_animation_speed = (*(int*) value) * 0.005f / 100;

    return ADDON_STATUS_OK;
}

//-- Announce -----------------------------------------------------------------
// Receive announcements from XBMC
// !!! Add-on master function !!!
//-----------------------------------------------------------------------------
extern "C" void ADDON_Announce( const char *flag, const char *sender, const char *message, const void *pi_data ) {}
