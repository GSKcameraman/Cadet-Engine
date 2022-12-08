#pragma once

#include "transform.h"
#include "heap.h"

typedef struct audio_system_t audio_system_t;

typedef struct audio_listener_t audio_listener_t;

typedef struct audio_source_t audio_source_t;

typedef struct speech_t speech_t;

//initializing the audio system for use, returns the ready-to-run audio system.
audio_system_t* init_system(heap_t* heap);

//set the global volume for the system
void set_global_volume(audio_system_t* system, float vol);

//initializing audio source.
audio_source_t* init_source(heap_t* heap, transform_t* transform, audio_system_t* system);

//setting audio source's .wav file location.
int load_wav(audio_source_t* source, const char* path);

//play the audio source
unsigned int play_source(audio_source_t* source);

//play the audio source, but in 2d environment.
unsigned int play_source_2d(audio_source_t* source);

//stops an audiosource from playing
void stop_source(audio_source_t* source);

//set loop for audio source
void source_setlooping(audio_source_t* source, int looping);

//sets up volume for a audio source
void set_wav_volume(audio_source_t* source, float vol);

//initializing a speech sound.
speech_t* speech_init(heap_t* heap, audio_system_t* system);

//plays a speech
unsigned int play_speech(speech_t* speech);

//stops a speech
void stop_speech(speech_t* speech);

//set volume for a speech.
void set_speech_volume(speech_t* speech, float vol);

//set up speech text of speech
void set_text(speech_t* speech, const char* text);

//initializing the listener.
audio_listener_t* listener_init(heap_t* heap, audio_system_t* system, transform_t* transform);


//updates the position of the audio listener based on the new position.
void update_listener(audio_listener_t* listener);

//destroys the audio system. do before shutdown.
void audio_system_destroy(audio_system_t* system);

//destroys the audio source.
void audio_source_destroy(audio_source_t* source);

//destroys a speech object.
void speech_destroy(speech_t* speech);

//destroys a listener
void listener_destroy(audio_listener_t* listener);
