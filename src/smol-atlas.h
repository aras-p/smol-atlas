// smol-atlas: https://github.com/aras-p/smol-atlas

#pragma once

struct smol_atlas_t;
struct smol_atlas_entry_t;

smol_atlas_t* sma_create(int init_width, int init_height);
void sma_destroy(smol_atlas_t* atlas);

int sma_get_width(const smol_atlas_t* atlas);
int sma_get_height(const smol_atlas_t* atlas);

smol_atlas_entry_t* sma_pack(smol_atlas_t* atlas, int width, int height);
void sma_entry_release(smol_atlas_t* atlas, smol_atlas_entry_t* entry);

void sma_clear(smol_atlas_t* atlas);
void sma_shrink_to_fit(smol_atlas_t* atlas);
void sma_resize(smol_atlas_t* atlas, int width, int height);

int sma_entry_get_x(const smol_atlas_entry_t* entry);
int sma_entry_get_y(const smol_atlas_entry_t* entry);
int sma_entry_get_width(const smol_atlas_entry_t* entry);
int sma_entry_get_height(const smol_atlas_entry_t* entry);

void sma_debug_dump(const smol_atlas_t* atlas,
                    void(*emit_shelf)(int y, int height, int width),
                    void(*emit_free_span)(int y, int height, int x, int width),
                    void(*emit_entry)(const smol_atlas_entry_t* entry));
