#include <SDL.h>
#define TSF_IMPLEMENTATION
#include <tsf.h>
#define TML_IMPLEMENTATION
#include <tml.h>
#include <string>

// Holds the global instance pointer
static tsf* g_TinySoundFont;

// Holds global MIDI playback state
static double g_Msec;               //current playback time
static tml_message* g_MidiMessage;  //next message to be played

// Callback function called by the audio thread
static void AudioCallback(void* data, Uint8 *stream, int len)
{
	//Number of samples to process
	int SampleBlock, SampleCount = (len / (2 * sizeof(float))); //2 output channels
	for (SampleBlock = TSF_RENDER_EFFECTSAMPLEBLOCK; SampleCount; SampleCount -= SampleBlock, stream += (SampleBlock * (2 * sizeof(float))))
	{
		//We progress the MIDI playback and then process TSF_RENDER_EFFECTSAMPLEBLOCK samples at once
		if (SampleBlock > SampleCount) SampleBlock = SampleCount;

		//Loop through all MIDI messages which need to be played up until the current playback time
		for (g_Msec += SampleBlock * (1000.0 / 44100.0); g_MidiMessage && g_Msec >= g_MidiMessage->time; g_MidiMessage = g_MidiMessage->next)
		{
			switch (g_MidiMessage->type)
			{
				case TML_PROGRAM_CHANGE: //channel program (preset) change (special handling for 10th MIDI channel with drums)
					tsf_channel_set_presetnumber(g_TinySoundFont, g_MidiMessage->channel, g_MidiMessage->program, (g_MidiMessage->channel == 9));
					fprintf( stdout, "tm=%06.0f : evtm=%06d : Ch %2d PROG CHANGE    PG %3d\n", g_Msec, g_MidiMessage->time, g_MidiMessage->channel, g_MidiMessage->program);
					break;
				case TML_NOTE_ON: //play a note
					tsf_channel_note_on(g_TinySoundFont, g_MidiMessage->channel, g_MidiMessage->key, g_MidiMessage->velocity / 127.0f);
					if (g_MidiMessage->velocity == 0)
						fprintf( stdout, "tm=%06.0f : evtm=%06d : Ch %2d NOTE OFF       Note %3d\n", g_Msec, g_MidiMessage->time, g_MidiMessage->channel, g_MidiMessage->key);
					else
						 fprintf( stdout, "tm=%06.0f : evtm=%06d : Ch %2d NOTE ON        Note %3d  Vel %3d\n", g_Msec, g_MidiMessage->time, g_MidiMessage->channel, g_MidiMessage->key, g_MidiMessage->velocity);
					break;
				case TML_NOTE_OFF: //stop a note
					tsf_channel_note_off(g_TinySoundFont, g_MidiMessage->channel, g_MidiMessage->key);
					fprintf( stdout, "tm=%06.0f : evtm=%06d : Ch %2d NOTE OFF       Note %3d\n", g_Msec, g_MidiMessage->time, g_MidiMessage->channel, g_MidiMessage->key);
					break;
				case TML_PITCH_BEND: //pitch wheel modification
					tsf_channel_set_pitchwheel(g_TinySoundFont, g_MidiMessage->channel, g_MidiMessage->pitch_bend);
					fprintf( stdout, "tm=%06.0f : evtm=%06d : Ch %2d BENDER         Val %3d\n", g_Msec, g_MidiMessage->time, g_MidiMessage->channel, g_MidiMessage->pitch_bend);
					break;
				case TML_CONTROL_CHANGE: //MIDI controller messages
					tsf_channel_midi_control(g_TinySoundFont, g_MidiMessage->channel, g_MidiMessage->control, g_MidiMessage->control_value);
					fprintf( stdout, "tm=%06.0f : evtm=%06d : Ch %2d CTRL CHANGE    Ctrl %3d Val %3d\n", g_Msec, g_MidiMessage->time, g_MidiMessage->channel, g_MidiMessage->control, g_MidiMessage->control_value);
					break;
			}
		}

		// Render the block of audio samples in float format
		tsf_render_float(g_TinySoundFont, (float*)stream, SampleBlock, 0);
	}
}

int main(int argc, char *argv[])
{
	// This implements a small program that you can launch without
	// parameters for a default file & soundfont, or with these arguments:
	//
	// ./example3-... <yourfile>.mid <yoursoundfont>.sf2

	tml_message* TinyMidiLoader = NULL;

	// Define the desired audio output format we request
	SDL_AudioSpec OutputAudioSpec;
	OutputAudioSpec.freq = 44100;
	OutputAudioSpec.format = AUDIO_F32;
	OutputAudioSpec.channels = 2;
	OutputAudioSpec.samples = 4096 >> 1;
	OutputAudioSpec.callback = AudioCallback;

	// Initialize the audio system
	if (SDL_AudioInit(TSF_NULL) < 0)
	{
		fprintf(stderr, "Could not initialize audio hardware or driver\n");
		return 1;
	}

	//Venture (Original WIP) by Ximon
	//https://musescore.com/user/2391686/scores/841451
	//License: Creative Commons copyright waiver (CC0)
	const char * def_midpath = argc >= 2 ? argv[1] : _PROJECT_SRC_PATH_ "/ext/tsf/examples/venture.mid";
	TinyMidiLoader = tml_load_filename(def_midpath);
	if (!TinyMidiLoader)
	{
		fprintf(stderr, "Could not load MIDI file: %s\n", def_midpath);
		return 1;
	}

	//Set up the global MidiMessage pointer to the first MIDI message
	g_MidiMessage = TinyMidiLoader;

	tsf_init_lut();

	// Load the SoundFont from a file
	const char * def_sfpath = argc >= 3 ? argv[2] : _PROJECT_SRC_PATH_ "/ext/tsf/examples/florestan-subset.sf2";
#if 0
	const char * homepath;

#ifdef _WIN32
	homepath = std::getenv("USERPROFILE");
	def_sfpath = "E:\\Games\\SoundFont2\\Arachno SoundFont - Version 1.0.sf2";
#else
	homepath = std::getenv("HOME");
	std::string sfpathstr = std::string(homepath) + std::string("/Documents/Arachno SoundFont - Version 1.0.sf2");
	def_sfpath = sfpathstr.c_str();
#endif
#endif
	g_TinySoundFont = tsf_load_filename(def_sfpath);
	if (!g_TinySoundFont)
	{
		fprintf(stderr, "Could not load SoundFont: %s\n", def_sfpath);
		return 1;
	}
	printf("Using SoundFont: %s\n", def_sfpath);

	//Initialize preset on special 10th MIDI channel to use percussion sound bank (128) if available
	tsf_channel_set_bank_preset(g_TinySoundFont, 9, 128, 0);

	// Set the SoundFont rendering output mode
	tsf_set_output(g_TinySoundFont, TSF_STEREO_INTERLEAVED, OutputAudioSpec.freq, -6 /* dB */);

	// Request the desired audio output format
	if (SDL_OpenAudio(&OutputAudioSpec, TSF_NULL) < 0)
	{
		fprintf(stderr, "Could not open the audio hardware or the desired audio output format\n");
		return 1;
	}

	// Start the actual audio playback here
	// The audio thread will begin to call our AudioCallback function
	SDL_PauseAudio(0);

	//Wait until the entire MIDI file has been played back (until the end of the linked message list is reached)
	while (g_MidiMessage != NULL) SDL_Delay(100);

	// We could call tsf_close(g_TinySoundFont) and tml_free(TinyMidiLoader)
	// here to free the memory and resources but we just let the OS clean up
	// because the process ends here.
	return 0;
}
