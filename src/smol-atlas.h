// SPDX-License-Identifier: MIT OR Unlicense
// smol-atlas: https://github.com/aras-p/smol-atlas

#pragma once

// 2D rectangular bin packing utility that uses the Shelf Best Height Fit
// heuristic, and supports item removal. You could also call it a
// dynamic texture atlas allocator.

struct smol_atlas_t;
struct smol_atlas_item_t;

/// Create atlas of given size.
smol_atlas_t* sma_create(int width, int height);

/// Destroy the atlas.
void sma_destroy(smol_atlas_t* atlas);

/// Get atlas width.
int sma_get_width(const smol_atlas_t* atlas);

/// Get atlas height.
int sma_get_height(const smol_atlas_t* atlas);

/// Add an item of (width x height) size into the atlas.
/// Use `sma_item_x` and `sma_item_y` to query the resulting item location.
/// Item can later be removed with `sma_remove`.
/// Returned pointer is valid until atlas is cleared or destroyed, or the item is removed.
/// Returns NULL if there is no more space left.
smol_atlas_item_t* sma_add(smol_atlas_t* atlas, int width, int height);

/// Remove a previously added item from the atlas.
/// The item pointer becomes invalid and can no longer be used.
void sma_remove(smol_atlas_t* atlas, smol_atlas_item_t* item);

/// Clear the atlas. This invalidates any previously returned item pointers.
void sma_clear(smol_atlas_t* atlas);

/// Get item X coordinate.
int sma_item_x(const smol_atlas_item_t* item);
/// Get item Y coordinate.
int sma_item_y(const smol_atlas_item_t* item);
/// Get item width.
int sma_item_width(const smol_atlas_item_t* item);
/// Get item height.
int sma_item_height(const smol_atlas_item_t* item);
