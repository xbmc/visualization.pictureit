#include <xbmc_vis_dll.h>
#include <libXBMC_addon.h>

#include "PictureIt/pictureit.h"
#include "PictureIt/utils.h"

#include <algorithm>

using namespace std;

PictureIt *pictureit = NULL;
ADDON::CHelper_libXBMC_addon *KODI = NULL;
VIS_PROPS *vis_props = NULL;

char img_directory[1024]  =   "";
char img_mode[32]         =   "";
char img_effect[32]       =   "";

bool    img_pick_random         = false;
bool    img_update_by_interval  = false;
int     img_update_interval     = 0;

bool    spectrum_enabled    =   false;
int     spectrum_bar_count  =   0;
float   spectrum_width      =   0.0f;
float   spectrum_position   =   0.0f;

bool    spectrum_mirror_vertical    = false;
bool    spectrum_mirror_horizontal  = false;
float   spectrum_animation_speed    = 0.0f;


bool    preset_enabled      =   false;
bool    preset_pick_random  =   false;
bool    img_update_by_track =   false;


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
    if ( index < PRESETS.size() ) {
        preset_index = index;
        pictureit->load_images(PI_UTILS::path_join(img_directory, PRESETS[preset_index]));
        pictureit->update_image();
    }
}

void set_effect( const char *efx_name ) {
    int int_val = 0;

    if ( strcmp( efx_name, "Crossfade" ) == 0 ) {
        pictureit->set_img_transition_efx( EFX::CROSSFADE );

        KODI->GetSetting( "crossfade.fade_ms", &int_val );
        pictureit->efx->configure("fade_time_ms", int_val);
    }
    else if ( strcmp( efx_name, "Slide" ) == 0 ) {
        pictureit->set_img_transition_efx( EFX::SLIDE );

        KODI->GetSetting( "slide.fade_ms", &int_val );
        pictureit->efx->configure("slide_time_ms", int_val);
    }
}

void set_mode( const char *img_mode) {
    if ( strcmp( img_mode, "Stretch" ) == 0 )
        pictureit->efx->image_mode = MODE::STRETCH;

    if ( strcmp( img_mode, "Center" ) == 0 )
        pictureit->efx->image_mode = MODE::CENTER;

    else if ( strcmp( img_mode, "Scale" ) == 0 )
        pictureit->efx->image_mode = MODE::SCALE;

    else if ( strcmp( img_mode, "Zoom" ) == 0 )
        pictureit->efx->image_mode = MODE::ZOOM;
}

//-- Render -------------------------------------------------------------------
// Called once per frame. Do all rendering here.
//-----------------------------------------------------------------------------
extern "C" void Render() {
    if ( pictureit )
        pictureit->render();
}

//-- Create -------------------------------------------------------------------
// Called on load. Addon should fully initalize or return error status
//-----------------------------------------------------------------------------
ADDON_STATUS ADDON_Create( void* hdl, void* props ) {
    if ( ! props )
        return ADDON_STATUS_UNKNOWN;

    vis_props = (struct VIS_PROPS *)props;

    if ( !KODI )
        KODI = new ADDON::CHelper_libXBMC_addon;

    if ( !KODI->RegisterMe(hdl) ) {
        delete KODI, KODI=NULL;

        return ADDON_STATUS_PERMANENT_FAILURE;
    }

    return ADDON_STATUS_NEED_SETTINGS;
}

extern "C" void Start(int iChannels, int iSamplesPerSec, int iBitsPerSample, const char* szSongName) {
    pictureit = new PictureIt( spectrum_bar_count );

    set_effect( img_effect );
    set_mode( img_mode );

    pictureit->window_width                 = vis_props->width;
    pictureit->window_height                = vis_props->height;

    pictureit->img_pick_random              = img_pick_random;
    pictureit->img_update_by_interval       = img_update_by_interval;
    pictureit->img_update_interval          = img_update_interval;

    pictureit->spectrum_enabled             = spectrum_enabled;
    pictureit->spectrum_position            = spectrum_position;
    pictureit->spectrum_width               = spectrum_width;
    pictureit->spectrum_mirror_vertical     = spectrum_mirror_vertical;
    pictureit->spectrum_mirror_horizontal   = spectrum_mirror_horizontal;
    pictureit->spectrum_animation_speed     = spectrum_animation_speed;

    if ( preset_enabled )
        load_presets();

    // If we have some data, we select a preset
    if ( ! PRESETS.empty() ) {
        if ( preset_pick_random )
            select_preset( rand() % PRESETS.size() );
        else
            select_preset( 0 );
    } else {
        // No presets found so we load whatever we find in the current directory
        pictureit->load_images( img_directory );
        pictureit->update_image();
    }
}

extern "C" void AudioData(const float* pAudioData, int iAudioDataLength, float *pFreqData, int iFreqDataLength) {
    if ( pictureit )
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
        return 0;

    unsigned int presets_count = PRESETS.size();
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
    delete KODI, KODI = NULL;
    delete vis_props, vis_props = NULL;
    delete pictureit, pictureit = NULL;
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
extern "C" ADDON_STATUS ADDON_SetSetting( const char *id, const void *value ) {
    if ( ! id || ! value )
        return ADDON_STATUS_UNKNOWN;

    /** Background related **/
    if ( strcmp( id, "img.directory" ) == 0 ) {
        strcpy( img_directory, (const char*) value );
        if ( ! KODI->CanOpenDirectory(img_directory) )
            return ADDON_STATUS_PERMANENT_FAILURE;
    }

    else if ( strcmp( id, "img.mode") == 0 )
        strcpy( img_mode, (const char*) value );

    else if ( strcmp( id, "img.effect") == 0 )
        strcpy( img_effect, (const char*) value );

    else if ( strcmp( id, "img.pick_random" ) == 0 )
        img_pick_random = *(bool*) value;

    else if ( strcmp( id, "preset.enabled" ) == 0 )
        preset_enabled = *(bool*) value;

    else if ( strcmp( id, "preset.pick_random" ) == 0 )
        preset_pick_random = *(bool*) value;

    else if ( strcmp( id, "img.update_by_track" ) == 0 )
        img_update_by_track = *(bool*) value;

    else if ( strcmp( id, "img.update_by_interval" ) == 0 )
        img_update_by_interval = *(bool*) value;

    else if ( strcmp( id, "img.update_interval" ) == 0 )
        img_update_interval = *(int*) value;


    /** Spectrum related **/
    else if ( strcmp( id, "spectrum.enabled" ) == 0 )
        spectrum_enabled = *(bool*) value;

    else if ( strcmp( id, "spectrum.bar_count" ) == 0 )
        spectrum_bar_count = (*(int*) value);

    else if ( strcmp( id, "spectrum.width" ) == 0 )
        spectrum_width = (*(int*) value) * 1.0f / 100;

    else if ( strcmp( id, "spectrum.position" ) == 0 )
        // We inverse the value just because -1.0 feel more like "bottom" than 1.0
        spectrum_position = -(*(float*) value);

    else if ( strcmp( id, "spectrum.mirror_vertical" ) == 0 )
        spectrum_mirror_vertical = *(bool*) value;

    else if ( strcmp( id, "spectrum.mirror_horizontal" ) == 0 )
        spectrum_mirror_horizontal = *(bool*) value;

    else if ( strcmp( id, "spectrum.animation_speed" ) == 0 )
        spectrum_animation_speed = (*(int*) value) * 0.005f / 100;

    return ADDON_STATUS_OK;
}

//-- Announce -----------------------------------------------------------------
// Receive announcements from XBMC
// !!! Add-on master function !!!
//-----------------------------------------------------------------------------
extern "C" void ADDON_Announce( const char *flag, const char *sender, const char *message, const void *data ) {}
