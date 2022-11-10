#pragma once

// frogger Test Game
// Brings together major engine systems to make a very frogger "game."

typedef struct frogger_game_t frogger_game_t;

typedef struct fs_t fs_t;
typedef struct heap_t heap_t;
typedef struct render_t render_t;
typedef struct wm_window_t wm_window_t;

// Create an instance of frogger test game.
frogger_game_t* frogger_game_create(heap_t* heap, fs_t* fs, wm_window_t* window, render_t* render);

// Destroy an instance of frogger test game.
void frogger_game_destroy(frogger_game_t* game);

// Per-frame update for our frogger test game.
void frogger_game_update(frogger_game_t* game);
