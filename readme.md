# :warning: WIP! :warning:

# smol-atlas

2D rectangular [bin packing](https://en.wikipedia.org/wiki/Bin_packing_problem)
utility that uses the Shelf Best Height Fit heuristic, and supports item removal.
You could also call it a dynamic texture atlas allocator.

### What is it?

`smol-atlas` is a C++ library for packing small rectangles into a large rectangle.
Possible application is to assemble icons, glyphs or thumbnails into a larger
texture atlas.

This sounds simple, but finding an optimal solution is actually [very hard](https://en.wikipedia.org/wiki/NP-completeness).
There are many ways to approach the bin packing problem, but `smol-atlas` uses the Shelf Best
Height Fit heuristic.  It works by dividing the total space into "shelves", each with a certain height.
The allocator packs rectangles onto whichever shelf minimizes the amount of wasted vertical space.

`smol-atlas` is simple, fast, and works best when the rectangles have similar heights (icons, glyphs
and thumbnails are like this).  It is not a generalized bin packer, and can potentially waste a
lot of space if the rectangles vary significantly in height.

- Incoming items are placed into horizontal "shelves" based on item heights.
- Within each shelf, there is a sorted list of free spans.
- When an item is added, needed portion of the first suitable free span
  is used.
- When an item is removed, resulting free span is joined with any
  neighboring spans.
- Shelves, once created, stay at their height and location. Even if they
  become empty, they are not removed nor joined with nearby shelves.

Implementation uses STL `<vector>`, and some manual memory allocation
with just regular `new` and `delete`. Custom allocators might be nice to
do someday.

At least C++11 is required.

License is either MIT or Unlicense, whichever is more convenient for you.

### Usage

Take `src/smol-atlas.cpp` and `src/smol-atlas.h`, plop them into your project and build.
Include `src/smol-atlas.h` and use functions from there.

Do *not* use `CMakeLists.txt` at the root of this repository! That one is for building the "test / benchmark"
application, which also compiles several other texture packing libraries, and runs various tests on them.

### How good is it?

I don't know!

But, I did test it on a use case I have in mind. Within [Blender](https://www.blender.org/)
video sequence editor, I took previs timeline of [Blender Studio Gold](https://studio.blender.org/films/gold/) project,
turned thumbnails on, and zoomed & panned around it for a while. During all that time, I dumped data of all the thumbnails
that would be needed at any point.

The test then is this:
- Try to "place" the needed thumbnails into the texture atlas,
- Remove thumbnails from the atlas, if they have not been used for a number of frames.
- As is typical within context of a video timeline, most of thumbnails have the same height, but many have varying width due
  to cropping caused by strip length.

250 frames of this data, ran 30 times in a loop, featuring 4700 unique thumbnails, produces 146 thousand item additions and 
143 thousand item removals from the texture atlas. On a Windows PC (Ryzen 5950X), Visual Studio 2022 build, all of the tested
libraries produce an atlas of 3072x3072 pixels.

| Library | Time, ms |
|---------|---------:|
| **smol-atlas**                                                                                                | 38 |
| [shelf-pack-cpp](https://github.com/mapbox/shelf-pack-cpp) from Mapbox                                        | 105 |
| [Étagère](https://github.com/nical/etagere) (Rust!) from Nicolas Silva / Mozilla                              | 32 |
| [stb_rect_pack](https://github.com/nothings/stb/blob/master/stb_rect_pack.h) from Sean Barrett                | 333 |
| [RectAllocator](https://gist.github.com/andrewwillmott/f9124eb445df7b3687a666fe36d3dcdb) from Andrew Willmott | 1140 |

### TODO finish this

Etagere:

Build the dynamic libraries locally by:
- Edit `Cargo.toml` and add this section:
  ```
  [lib]
  crate-type = ["cdylib"]
  ```
- Build the dynamic library with `cargo build --release --features "ffi"`, it will be under `target/release`.
- Generate header file with `cbindgen --config cbindgen.toml --crate etagere --output etagere.h`. You might need to do
  `cargo install cbindgen` first.

