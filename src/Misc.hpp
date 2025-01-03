#pragma once

#include "jdksmidi/world.h"
#include "jdksmidi/track.h"
#include "jdksmidi/multitrack.h"
#include "jdksmidi/filereadmultitrack.h"
#include "jdksmidi/fileread.h"
#include "jdksmidi/fileshow.h"
#include "jdksmidi/sequencer.h"
#include "jdksmidi/msg.h"

typedef unsigned int GLuint;

namespace vinfony
{
	void DumpMIDITimedBigMessage( const jdksmidi::MIDITimedBigMessage *msg );
    bool LoadTextureFromFile(const char* filename, unsigned int* out_texture, int* out_width, int* out_height);
}