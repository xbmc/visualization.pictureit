#include "pictureit/utils.h"

#include <fnmatch.h>
#include <GL/gl.h>

#include <dirent.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

namespace PI_UTILS {
    const char* path_join(string a, string b) {
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
    
    int list_dir(const char *path, vector<string> &store, bool recursive, bool incl_full_path, const char *file_filter[], int filter_size) {
        string p = path;
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
                    return PI_UTILS::list_dir( PI_UTILS::path_join( p, name), store, recursive, incl_full_path, file_filter, filter_size );
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
                    store.push_back( PI_UTILS::path_join( p, name ) );
                else
                    store.push_back( name );
                add = false;
            }
        }
    
        closedir(dp);
        return 0;
    }

    bool load_image(const char *img_path, GLuint texture_id) {
        if ( ! texture_id )
            return false;
    
        int x, y, n;
        unsigned char *data = stbi_load(img_path, &x, &y, &n, 0);
    
        if(data == nullptr)
            return false;
    
        glBindTexture(GL_TEXTURE_2D, texture_id);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, x, y, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
    
        stbi_image_free(data);
    
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,  GL_LINEAR );
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,  GL_LINEAR );
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,      GL_CLAMP_TO_EDGE );
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,      GL_CLAMP_TO_EDGE );
    
        glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
    
        return true;
    }
    
    void draw_image(GLuint texture_id, float x, float y, float opacity) {
        if ( ! texture_id )
            return;
    
        glEnable( GL_TEXTURE_2D );
        glEnable( GL_BLEND );
    
        glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
    
        glBindTexture( GL_TEXTURE_2D, texture_id );
    
        if ( ! texture_id )
            glColor4f( 0.0f, 0.0f, 0.0f, opacity );
        else
            glColor4f( 1.0f, 1.0f, 1.0f, opacity );
    
    
        GLfloat tl[2] = { 0.0f - x, 0.0f - x };  // Top Left
        GLfloat tr[2] = { 1.0f - x, 0.0f - x };  // Top Right
        GLfloat bl[2] = { 0.0f - y, 1.0f - y };  // Bottom Left
        GLfloat br[2] = { 1.0f - y, 1.0f - y };  // Bottom Right
    
        glBegin( GL_TRIANGLES );
            glTexCoord2f( tl[0], tl[1] ); glVertex2f( -1.0f, -1.0f );  // Top Left
            glTexCoord2f( tr[0], tr[1] ); glVertex2f(  1.0f, -1.0f );  // Top Right
            glTexCoord2f( br[0], br[1] ); glVertex2f(  1.0f,  1.0f );  // Bottom Right
        glEnd();
        glBegin( GL_TRIANGLES );
            glTexCoord2f( br[0], br[1] ); glVertex2f(  1.0f,  1.0f );  // Bottom Right
            glTexCoord2f( bl[0], bl[1] ); glVertex2f( -1.0f,  1.0f );  // Bottom Left
            glTexCoord2f( tl[0], tl[1] ); glVertex2f( -1.0f, -1.0f );  // Top Left
        glEnd();
    
        glDisable( GL_TEXTURE_2D );
        glDisable( GL_BLEND );
    }
}