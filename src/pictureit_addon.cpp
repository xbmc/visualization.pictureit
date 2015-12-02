#include "xbmc_vis_dll.h"

#include "PictureIt/pictureit.h"
#include "PictureIt/utils.h"

#include <algorithm>

using namespace std;

PictureIt *pictureit = NULL;

vector<string> pi_presets;     // Holds all preset-names in alphabetical order

const char*            presets_root_dir        =    "";                  // Root directory holding subfolders that will be used
                                                                         // as presets and searched for images
unsigned int           presets_count           =    0;                   // Amount of presets (subfolders) found
unsigned int           preset_index            =    0;                   // Index of the currently selected preset
bool                   preset_random           =    false;               // If random preset is active
bool                   preset_locked           =    false;               // If current preset is locked

bool                   update_on_new_track     =    true;                // If we should update on new track (Kodi setting)
int                    fade_time_ms            =    2000;                // How long the crossfade between two images will take (in ms)

int                    vis_bg_enabled          =    true;                // If the transparent background behind the spectrum is enabled (Kodi setting)
const int              vis_bar_count           =    96;                  // Amount of single bars to display (will be doubled as we
                                                                         // mirror them to the right side)
GLfloat                vis_bottom_edge         =    0.98f;               // If set to 1.0 the bars would be exactly on the screen edge (Kodi setting)
const GLfloat          vis_bar_min_height      =    0.02f;               // The min height for each bar
const GLfloat          vis_bar_max_height      =    0.18f;               // The max height for each bar
GLfloat                vis_animation_speed     =    0.007f;              // Animation speed. The smaler the value, the slower AND smoother the
                                                                         // animations (Kodi setting)
const float            vis_bottom_edge_scale[] =    { 1.0, 0.98, 0.96, 0.94, 0.92, 0.90, 0.88, 0.86, 0.84, 0.82, 0.80 };



void load_presets() {
    PI_UTILS::list_dir( presets_root_dir, pi_presets, false, false, NULL, 0 );
    sort( pi_presets.begin(), pi_presets.end() );
}

// Select preset at "index"
void select_preset( unsigned int index ) {
    if ( index >= pi_presets.size() )
        return;

    preset_index = index;

    pictureit->load_images(PI_UTILS::path_join(presets_root_dir, pi_presets[preset_index]));
    pictureit->update_image();
}

//-- Render -------------------------------------------------------------------
// Called once per frame. Do all rendering here.
//-----------------------------------------------------------------------------
extern "C" void Render() {
    pictureit->render();
}

//-- Create -------------------------------------------------------------------
// Called on load. Addon should fully initalize or return error status
//-----------------------------------------------------------------------------
ADDON_STATUS ADDON_Create( void* hdl, void* props ) {
    if ( ! props )
        return ADDON_STATUS_UNKNOWN;

    pictureit = new PictureIt(64);
    pictureit->init();

    return ADDON_STATUS_NEED_SETTINGS;
}

extern "C" void Start(int iChannels, int iSamplesPerSec, int iBitsPerSample, const char* szSongName) {
    pictureit->EFX->configure("fade_time_ms", fade_time_ms);

    // Now we should have the user-settings loaded so we can try and get our presets
    if ( pi_presets.empty() ) {
        load_presets();

        // If we have some data, we select a random preset
        if ( ! pi_presets.empty() ) {
            select_preset( rand() % pi_presets.size() );
            pictureit->load_images(PI_UTILS::path_join(presets_root_dir, pi_presets[preset_index]));
        }
    }
}

extern "C" void AudioData(const float* pAudioData, int iAudioDataLength, float *pFreqData, int iFreqDataLength) {
    pictureit->audio_data(pAudioData, iAudioDataLength);
}


//-- GetInfo ------------------------------------------------------------------
// Tell XBMC our requirements
//-----------------------------------------------------------------------------
extern "C" void GetInfo(VIS_INFO* pInfo) {
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
                pictureit->update_image();
            break;
        default:
            ret = false;
            break;
    }

    return ret;
}

//-- GetPresets ---------------------------------------------------------------
// Return a list of presets to XBMC for display
//-----------------------------------------------------------------------------
extern "C" unsigned int GetPresets(char ***presets) {
    if ( pi_presets.empty() )
        load_presets();

    if ( pi_presets.empty() )
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
extern "C" void ADDON_Stop() {
    delete pictureit;
}

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

    string str = strSetting;

    if ( str == "presets_root_dir" ) {
        const char* dir = (const char*) value;
        if ( dir && !dir[0] )
            return ADDON_STATUS_NEED_SETTINGS;

        presets_root_dir = dir;
    }

    if ( str == "update_on_new_track" )
        update_on_new_track = *(bool*)value;

    else if ( str == "update_by_interval" )
        pictureit->img_update_by_interval = *(bool*)value;

    else if ( str == "img_update_interval" )
        pictureit->img_update_interval = *(int*) value;

    else if ( str == "fade_time_ms" )
        fade_time_ms = *(int*) value;

    else if ( str == "spectrum_enabled" )
        pictureit->spectrum_enabled = *(bool*)value;

    else if ( str == "vis_bg_enabled" )
        vis_bg_enabled = *(bool*)value;

    else if ( str == "vis_half_width" )
        pictureit->spectrum_width = ((*(int*) value) * 1.0f / 100);

    else if ( str == "vis_bottom_edge" ) {
        float scale[] = { 1.0, 0.98, 0.96, 0.94, 0.92, 0.90, 0.88, 0.86, 0.84, 0.82, 0.80 };
        pictureit->spectrum_position = scale[(*(int*) value)];
    }

    else if ( str == "vis_animation_speed" )
        pictureit->spectrum_animation_speed = (*(int*) value) * 0.005f / 100;

    return ADDON_STATUS_OK;
}

//-- Announce -----------------------------------------------------------------
// Receive announcements from XBMC
// !!! Add-on master function !!!
//-----------------------------------------------------------------------------
extern "C" void ADDON_Announce( const char *flag, const char *sender, const char *message, const void *pi_data ) {}
