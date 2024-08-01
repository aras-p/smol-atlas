#include <cstdarg>
#include <cstdint>
#include <cstdlib>
#include <ostream>
#include <new>

constexpr static const uint32_t ETAGERE_FLAGS_VERTICAL_SHELVES = 1;

/// Options to tweak the behavior of the atlas allocator.
struct AllocatorOptions;

/// A shelf-packing dynamic texture atlas allocator tracking each allocation individually and with support
/// for coalescing empty shelves.
struct EtagereAtlasAllocator;

struct EtagereAllocatorOptions {
  int32_t width_alignment;
  int32_t height_alignment;
  int32_t num_columns;
  uint32_t flags;
};

/// 1 means OK, 0 means error.
using EtagereStatus = uint32_t;

struct EtagereRectangle {
  int32_t min_x;
  int32_t min_y;
  int32_t max_x;
  int32_t max_y;
};

using EtagereAllocationId = uint32_t;

struct EtagereAllocation {
  EtagereRectangle rectangle;
  EtagereAllocationId id;
};



extern "C" {

EtagereAtlasAllocator *etagere_atlas_allocator_new(int32_t width, int32_t height);

EtagereAtlasAllocator *etagere_atlas_allocator_with_options(int32_t width,
                                                            int32_t height,
                                                            const EtagereAllocatorOptions *options);

void etagere_atlas_allocator_delete(EtagereAtlasAllocator *allocator);

EtagereStatus etagere_atlas_allocator_allocate(EtagereAtlasAllocator *allocator,
                                               int32_t width,
                                               int32_t height,
                                               EtagereAllocation *allocation);

void etagere_atlas_allocator_deallocate(EtagereAtlasAllocator *allocator, EtagereAllocationId id);

void etagere_atlas_allocator_clear(EtagereAtlasAllocator *allocator);

int32_t etagere_atlas_allocator_allocated_space(const EtagereAtlasAllocator *allocator);

int32_t etagere_atlas_allocator_free_space(const EtagereAtlasAllocator *allocator);

EtagereRectangle etagere_atlas_allocator_get(const EtagereAtlasAllocator *allocator,
                                             EtagereAllocationId id);

EtagereStatus etagere_atlas_allocator_dump_svg(const EtagereAtlasAllocator *allocator,
                                               const char *file_name);

} // extern "C"
