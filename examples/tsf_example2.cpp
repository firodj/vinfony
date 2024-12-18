#define TSF_IMPLEMENTATION
#include <tsf.h>
#include <SDL.h>
//#include "../tsf.h"
//#include "minisdl_audio.h"

// Holds the global instance pointer
static tsf* g_TinySoundFont;

// A Mutex so we don't call note_on/note_off while rendering audio samples
static SDL_mutex* g_Mutex;

static void AudioCallback(void* data, Uint8 *stream, int len)
{
	// Render the audio samples in float format
	int SampleCount = (len / (2 * sizeof(float))); //2 output channels
	SDL_LockMutex(g_Mutex); //get exclusive lock
	tsf_render_float(g_TinySoundFont, (float*)stream, SampleCount, 0);
	SDL_UnlockMutex(g_Mutex);
}

int main(int argc, char *argv[])
{
	int i, Notes[7] = { 48, 50, 52, 53, 55, 57, 59 };

	// Define the desired audio output format we request
	SDL_AudioSpec OutputAudioSpec;
	OutputAudioSpec.freq = 44100;
	OutputAudioSpec.format = AUDIO_F32;
	OutputAudioSpec.channels = 2;
	OutputAudioSpec.samples = 4096;
	OutputAudioSpec.callback = AudioCallback;

	tsf_debug_wantlearn();

	// Initialize the audio system
	if (SDL_AudioInit(TSF_NULL) < 0)
	{
		fprintf(stderr, "Could not initialize audio hardware or driver\n");
		return 1;
	}

	// Load the SoundFont from a file
	const char * def_sfpath = "ext/tsf/examples/florestan-subset.sf2";
	g_TinySoundFont = tsf_load_filename(def_sfpath);
	if (!g_TinySoundFont)
	{
		fprintf(stderr, "Could not load SoundFont: %s\n", def_sfpath);
		return 1;
	}

	// Set the SoundFont rendering output mode
	tsf_set_output(g_TinySoundFont, TSF_STEREO_INTERLEAVED, OutputAudioSpec.freq, 0);

	// Create the mutex
	g_Mutex = SDL_CreateMutex();

	// Request the desired audio output format
	if (SDL_OpenAudio(&OutputAudioSpec, TSF_NULL) < 0)
	{
		fprintf(stderr, "Could not open the audio hardware or the desired audio output format\n");
		return 1;
	}

	// Start the actual audio playback here
	// The audio thread will begin to call our AudioCallback function
	SDL_PauseAudio(0);

#if 0
	// Loop through all the presets in the loaded SoundFont
	for (i = 0; i < tsf_get_presetcount(g_TinySoundFont); i++)
	{
		//Get exclusive mutex lock, end the previous note and play a new note
		printf("Play note %d with preset #%d '%s'\n", Notes[i % 7], i, tsf_get_presetname(g_TinySoundFont, i));
		SDL_LockMutex(g_Mutex);
		tsf_note_off(g_TinySoundFont, i - 1, Notes[(i - 1) % 7]);
		tsf_note_on(g_TinySoundFont, i, Notes[i % 7], 1.0f);
		SDL_UnlockMutex(g_Mutex);
		SDL_Delay(1000);
	}
#elif 0
	int patch = 0;
	for (int vel=0; vel <= 10; vel++) {
		float velocity = vel/10.0;
		printf("Play note %d (vel:%.2f) with preset #%d '%s'\n", Notes[i % 7], velocity, patch, tsf_get_presetname(g_TinySoundFont, patch));
		SDL_LockMutex(g_Mutex);
		tsf_note_off(g_TinySoundFont, patch - 1, Notes[0]);
		tsf_note_on(g_TinySoundFont, patch, Notes[0], velocity);
		SDL_UnlockMutex(g_Mutex);
		SDL_Delay(500);
	}
#else
	int patch = 0;
	float velocity = 0.8;
	tsf_channel_set_presetindex(g_TinySoundFont, 0, patch);
	tsf_channel_set_midipan(g_TinySoundFont, 0, 8192);
	tsf_channel_set_midivolume(g_TinySoundFont, 0, 16383);
	tsf_channel_note_on(g_TinySoundFont, 0, Notes[0], velocity);

	for (int vol=20; vol >= 0; vol--) {
		SDL_Delay(100);

		tsf_channel_set_midifilter(g_TinySoundFont, 0, vol * 0x7F / 20, 0x50);
		//tsf_channel_set_midifilter(g_TinySoundFont, 0, 0x20, vol * 0x7F / 20);
		//tsf_channel_set_midipan(g_TinySoundFont, 0, vol * 16383/20);
		//tsf_channel_midi_control(g_TinySoundFont, 0, 1, vol * 0x7F / 20);
		printf("Set ctrl value %d\n", vol);
	}
	SDL_Delay(100);
	tsf_channel_note_off_all(g_TinySoundFont, 0);
	SDL_Delay(1000);

#endif
	// We could call tsf_close(g_TinySoundFont) and SDL_DestroyMutex(g_Mutex)
	// here to free the memory and resources but we just let the OS clean up
	// because the process ends here.
	return 0;
}
