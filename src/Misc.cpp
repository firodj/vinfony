
#include "Misc.hpp"

#include <fmt/core.h>
#include <fmt/color.h>
#include "stb_image.h"
#include <glad/gl.h>

namespace vinfony
{
    void DumpMIDITimedBigMessage( const jdksmidi::MIDITimedBigMessage *msg )
    {
        if ( msg )
        {
            char msgbuf[1024];

            // note that Sequencer generate SERVICE_BEAT_MARKER in files dump,
            // but files themselves not contain this meta event...
            // see MIDISequencer::beat_marker_msg.SetBeatMarker()
            if ( msg->IsBeatMarker() )
            {
                //fprintf( stdout, "%8ld : %s <------------------>", msg->GetTime(), msg->MsgToText( msgbuf, 1024 ) );
                fmt::print("{}: {} <------------------>", msg->GetTime(), msg->MsgToText(msgbuf, 1024));
            }
            else if ( msg->IsSystemExclusive() )
            {
                //fprintf( stdout, "SYSEX length: %d", msg->GetSysEx()->GetLengthSE() );
                fmt::print("SYSEX length: {}", msg->GetSysEx()->GetLengthSE());
            }
            else if ( msg->IsKeySig() ) {
                fmt::print("Key Signature  ");
                msg->GetKeySigSharpFlats();
                const char * keynmes[] = {
                    "F", "G♭", "G", "A♭", "A", "B♭", "B" ,"C", "C♯", "D", "D♯", "E", "F", "F#", "G",
                };
                int i = msg->GetKeySigSharpFlats();
                if (i >= -7 && i <= 7) {
                    fmt::print(keynmes[i+7]);
                }
                if (msg->GetKeySigMajorMinor() == 1)
                    fmt::print(" Minor");
                else
                    fmt::print(" Major");
            } else if ( msg->IsTimeSig() ) {
                fmt::print("Time Signature  {}/{}  Clks/Metro.={} 32nd/Quarter={}",
                    (int)msg->GetTimeSigNumerator(),
                    (int)msg->GetTimeSigDenominator(),
                    (int)msg->GetTimeSigMidiClocksPerMetronome(),
                    (int)msg->GetTimeSigNum32ndPerMidiQuarterNote()
                );

            } else if (msg->IsTempo()) {
                fmt::print("Tempo    {} BPM ({} usec/beat)", msg->GetTempo32()/32.0, msg->GetTempo());
            } else if (msg->IsEndOfTrack()) {
                fmt::print("End of Track");
            } else {
                //fprintf( stdout, "%8ld : %s", msg->GetTime(), msg->MsgToText( msgbuf, 1024 ) );
                fmt::print("{} : {}", msg->GetTime(), msg->MsgToText( msgbuf, 1024 ) );

                if (msg->GetSysEx()) {
                    const unsigned char *buf = msg->GetSysEx()->GetBuf();
                    int len = msg->GetSysEx()->GetLengthSE();
                    std::string str;
                    for ( int i = 0; i < len; ++i ) {
                        if (buf[i] >= 0x20 && buf[i] <= 0x7F)
                            str.push_back( (char)buf[i] );
                    }
                    fmt::print(" Data: {}", str);
                }
            }

            //fprintf( stdout, "\n" );
        }
    }

    // Simple helper function to load an image into a OpenGL texture with common settings
    bool LoadTextureFromFile(const char* filename, unsigned int* out_texture, int* out_width, int* out_height)
    {
        // Load from file
        int image_width = 0;
        int image_height = 0;
        unsigned char* image_data = stbi_load(filename, &image_width, &image_height, NULL, 4);
        if (image_data == NULL) {
            fmt::println("Error LoadTextureFromFile {}", filename);
            return false;
        }

        // Create a OpenGL texture identifier
        GLuint image_texture;
        glGenTextures(1, &image_texture);
        glBindTexture(GL_TEXTURE_2D, image_texture);

        // Setup filtering parameters for display
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); // This is required on WebGL for non power-of-two textures
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE); // Same

        // Upload pixels into texture
    #if defined(GL_UNPACK_ROW_LENGTH) && !defined(__EMSCRIPTEN__)
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    #endif
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image_width, image_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image_data);
        stbi_image_free(image_data);

        *out_texture = (unsigned int)image_texture;
        *out_width = image_width;
        *out_height = image_height;

        return true;
    }

}