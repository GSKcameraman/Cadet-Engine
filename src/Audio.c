#pragma once

#include "Audio.h"
#include "SoLoud/soloud_c.h"
#include "transform.h"
#include "heap.h"


typedef struct audio_system_t {
	Soloud* soloud;
	heap_t* heap;
}audio_system_t;

typedef struct audio_source_t {
	transform_t* transform;
	audio_system_t* system;
	Wav* wav;
	heap_t* heap;
}audio_source_t;

typedef struct audio_listener_t {
	transform_t* transform;
	audio_system_t* system;
	heap_t* heap;
}audio_listener_t;


typedef struct speech_t {
	Speech* speech;
	audio_system_t* system;
	heap_t* heap;
}speech_t;


audio_system_t* init_system(heap_t* heap){
	
	audio_system_t* system = heap_alloc(heap, sizeof(audio_system_t), 8);
	system->heap = heap;
	system->soloud = Soloud_create();
	Soloud_initEx(system->soloud, SOLOUD_CLIP_ROUNDOFF | SOLOUD_ENABLE_VISUALIZATION, SOLOUD_AUTO, SOLOUD_AUTO, SOLOUD_AUTO, SOLOUD_AUTO);
	return system;
}

audio_source_t* init_source(heap_t* heap, transform_t* transform, audio_system_t* system) {
	audio_source_t* audiosource = heap_alloc(heap, sizeof(audio_source_t), 8);
	
	audiosource->heap = heap;
	audiosource->transform = transform;
	audiosource->system = system;
	audiosource->wav = Wav_create();

	return audiosource;
}

void set_global_volume(audio_system_t* system ,float vol) {
	Soloud_setGlobalVolume(system->soloud, vol);
}

int load_wav(audio_source_t* source, const char* path) {
	return Wav_load(source->wav, path);
}

unsigned int play_source(audio_source_t* source) {
	return Soloud_play3d(source->system->soloud, source->wav,
		source->transform->translation.x,
		source->transform->translation.y,
		source->transform->translation.z);
}

unsigned int play_source_2d(audio_source_t* source) {
	return Soloud_play(source->system->soloud, source->wav);
}

void stop_source(audio_source_t* source) {
	Wav_stop(source->wav);
}

void source_setlooping(audio_source_t* source, int looping) {
	Wav_setLooping(source->wav, looping);
}

void set_wav_volume(audio_source_t* source, float vol) {
	Wav_setVolume(source->wav, vol);
}

speech_t* speech_init(heap_t* heap, audio_system_t* system) {
	speech_t* speech = heap_alloc(heap, sizeof(speech_t), 8);

	speech->heap = heap;
	speech->speech = Speech_create();
	speech->system = system;

	return speech;
}

void set_text(speech_t* speech, const char* text) {
	Speech_setText(speech->speech, text);
}

unsigned int play_speech(speech_t* speech) {
	return Soloud_play(speech->system->soloud, speech->speech);
}

void stop_speech(speech_t* speech) {
	Speech_stop(speech->speech);
}

void set_speech_volume(speech_t* speech, float vol) {
	Speech_setVolume(speech->speech, vol);
}

void audio_system_destroy(audio_system_t* system) {
	Soloud_deinit(system->soloud);
	Soloud_destroy(system->soloud);
	heap_free(system->heap, system);
}

void audio_source_destroy(audio_source_t* source) {
	Wav_destroy(source->wav);
}

void speech_destroy(speech_t* speech) {
	Speech_destroy(speech->speech);
	heap_free(speech->heap, speech);

}

audio_listener_t* listener_init(heap_t* heap, audio_system_t* system,transform_t* transform) {
	audio_listener_t* lis = heap_alloc(heap, sizeof(audio_listener_t), 8);

	lis->heap = heap;
	lis->system = system;
	lis->transform = transform;

	return lis;
}

void update_listener(audio_listener_t* listener) {
	Soloud_set3dListenerPosition(listener->system->soloud,
		listener->transform->translation.x,
		listener->transform->translation.y,
		listener->transform->translation.z);
}

void listener_destroy(audio_listener_t* listener) {
	heap_free(listener->heap, listener);
}
