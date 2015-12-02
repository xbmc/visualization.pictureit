#include "xbmc_vis_dll.h"

#include "PictureIt/pictureit.h"
#include "PictureIt/utils.h"

#include <algorithm>

using namespace std;

PictureIt *pictureit = NULL;

const char* img_directory   =   NULL;
const char* img_effect      =   NULL;

bool    img_pick_random         = false;
bool    img_update_by_interval  = false;
int     img_update_interval     = 0;

bool    spectrum_enabled    =   false;
int     spectrum_bar_count  =   0;
float   spectrum_width      =   0;
float   spectrum_position   =   0;

bool    spectrum_mirror_vertical    = false;
bool    spectrum_mirror_horizontal  = false;
float   spectrum_animation_speed    = 0;


bool    preset_pick_random  =   false;
bool    img_update_by_track =   false;


unsigned int    presets_count   =    0;
unsigned int    preset_index    =    0;
bool            preset_random   =    false;
bool            preset_locked   =    false;

vector<string> PRESETS; // Holds all preset-names in alphabetical order

void load_presets() {
    PI_UTILS::list_dir( img_directory, PRESETS, false, false, NULL, 0 );
    sort( PRESETS.begin(), PRESETS.end() );
}

// Select preset at "index"
void select_preset( unsigned int index ) {
    if ( index >= PRESETS.size() )
        return;

    preset_index = index;

    pictureit->load_images(PI_UTILS::path_join(img_directory, PRESETS[preset_index]));
    pictureit->update_image();
}

void configure_efx( const char* efx_name ) {
    string efx = efx_name;

    if ( efx == "Crossfade" ) {
        pictureit->set_img_efx(EFXS::CROSSFADE);
        pictureit->EFX->configure("fade_time_ms", 250);
    }
    
    else if ( efx == "Slide" ) {
        pictureit->set_img_efx(EFXS::SLIDE);
        pictureit->EFX->configure("fade_time_ms", 250);
    }
}

//-- Render -------------------------------------------------------------------
// Called once per frame. Do all rendering here.
//-----------------------------------------------------------------------------
extern "C" void Render() {
    if ( pictureit != NULL)
        pictureit->render();
}

//-- Create -------------------------------------------------------------------
// Called on load. Addon should fully initalize or return error status
//-----------------------------------------------------------------------------
ADDON_STATUS ADDON_Create( void* hdl, void* props ) {
    if ( ! props )
        return ADDON_STATUS_UNKNOWN;

    return ADDON_STATUS_NEED_SETTINGS;
}

extern "C" void Start(int iChannels, int iSamplesPerSec, int iBitsPerSample, const char* szSongName) {
    pictureit = new PictureIt( spectrum_bar_count );

    configure_efx( img_effect );

    pictureit->img_pick_random              = img_pick_random;
    pictureit->img_update_by_interval       = img_update_by_interval;
    pictureit->img_update_interval          = img_update_interval;

    pictureit->spectrum_enabled             = spectrum_enabled;
    pictureit->spectrum_position            = spectrum_position;
    pictureit->spectrum_width               = spectrum_width;
    pictureit->spectrum_mirror_vertical     = spectrum_mirror_vertical;
    pictureit->spectrum_mirror_horizontal   = spectrum_mirror_horizontal;
    pictureit->spectrum_animation_speed     = spectrum_animation_speed;

    // Now we should have the user-settings loaded so we can try and get our presets
    if ( PRESETS.empty() ) {
        load_presets();

        // If we have some data, we select a random preset
        if ( ! PRESETS.empty() ) {
            if (preset_pick_random)
                select_preset( rand() % PRESETS.size() );
            else
                select_preset( 0 );

            pictureit->load_images(PI_UTILS::path_join(img_directory, PRESETS[preset_index]));
        }
    }
}

extern "C" void AudioData(const float* pAudioData, int iAudioDataLength, float *pFreqData, int iFreqDataLength) {
    if ( pictureit != NULL)
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
                        select_preset( rand() % PRESETS.size() );
                } else {
                    preset_index += 1;
                    if ( preset_index >= PRESETS.size() )
                        preset_index = 0;
                }
            }
            break;
        case VIS_ACTION_PREV_PRESET:
            if ( ! preset_locked ) {
                if ( preset_random ) {
                        select_preset( rand() % PRESETS.size() );
                } else {
                    preset_index -= 1;
                    if ( preset_index < 0 )
                        preset_index = PRESETS.size();
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
            if ( img_update_by_track )
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
    if ( PRESETS.empty() )
        load_presets();

    if ( PRESETS.empty() )
        return 0;

    presets_count = PRESETS.size();
    char **g_presets = (char**) malloc( sizeof(char*) * presets_count );

    for( unsigned int i = 0; i < presets_count; i++ )
        g_presets[i] = (char*) PRESETS[i].c_str();

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

    /** Background related **/
    if ( str == "img.directory" ) {
        const char* dir = (const char*) value;
        if ( (dir != NULL) && ( dir[0] == '\0' ) )
            return ADDON_STATUS_NEED_SETTINGS;

        img_directory = dir;
    }

    else if ( str == "img.effect")
        img_effect = (const char*) value;

    else if ( str == "img.pick_random" )
        img_pick_random = *(bool*)value;

    else if ( str == "preset.pick_random" )
        preset_pick_random = *(bool*)value;

    else if ( str == "img.update_by_track" )
        img_update_by_track = *(bool*)value;

    else if ( str == "img.update_by_interval" )
        img_update_by_interval = *(bool*)value;

    else if ( str == "img.update_interval" )
        img_update_interval = *(int*) value;


    /** Spectrum related **/
    else if ( str == "spectrum.enabled" )
        spectrum_enabled = *(bool*)value;

    else if ( str == "spectrum.bar_count" )
        spectrum_bar_count = (*(int*) value);

    else if ( str == "spectrum.width" )
        spectrum_width = ((*(int*) value) * 1.0f / 100);

    else if ( str == "spectrum.position" )
        spectrum_position = -(*(float*) value);

    else if ( str == "spectrum.mirror_vertical" )
        spectrum_mirror_vertical = *(bool*)value;

    else if ( str == "spectrum.mirror_horizontal" )
        spectrum_mirror_horizontal = *(bool*)value;

    else if ( str == "spectrum.animation_speed" )
        spectrum_animation_speed = (*(int*) value) * 0.005f / 100;

    return ADDON_STATUS_OK;
}

//-- Announce -----------------------------------------------------------------
// Receive announcements from XBMC
// !!! Add-on master function !!!
//-----------------------------------------------------------------------------
extern "C" void ADDON_Announce( const char *flag, const char *sender, const char *message, const void *pi_data ) {}