# smol-atlas

2D rectangular [bin packing](https://en.wikipedia.org/wiki/Bin_packing_problem)
utility that uses the Shelf Best Height Fit heuristic, and supports **item removal**.
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
Include `src/smol-atlas.h` and use functions from there. Something like:

```c++
smol_atlas_t* atlas = sma_atlas_create(100, 100);
// add a 70x30 item
smol_atlas_item_t* item = sma_item_add(atlas, 70, 30);
if (item) {
    // where did it end up?
    int x = sma_item_x(item);
    int y = sma_item_y(item);

    // can also remove it at some point
    sma_item_remove(atlas, item);
}
sma_atlas_destroy(atlas);
```

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
- When no longer can fit new thumbnails, remove "old" ones from the atlas.
- If still can not fit new thumbnails, try to enlarge the atlas.
- As is typical within context of a video timeline, most of thumbnails have the same height, but many have varying width due
  to cropping caused by strip length.

250 frames of this data, ran 30 times in a loop, featuring 4700 unique thumbnails, produces 160 thousand item additions and 
150 thousand item removals from the texture atlas. All of the tested libraries produce an atlas of 1536x1536 pixels.
"Win" time is Ryzen 5950X (VS2022), "Mac" time is M1 Max (Xcode15).

| Library                                            | GCs     |Repacks/grows | Allocs  | Mac time, ms | Win time, ms | Look |
|----------------------------------------------------|--------:|-------------:|--------:|-------------:|-------------:|------|
| **smol-atlas**                                     | 800     | **127**      | **168** | **9**        | **10**       | <img src="/img/gold_smol.svg" width="100" /> |
| [Étagère][1] (Rust!) from Nicolas Silva / Mozilla  | 876     | 185          | 738     | 13           | 15           | <img src="/img/gold_etagere.svg" width="100" /> |
| [shelf-pack-cpp][2] from Mapbox                    | 1027    | 426          | 521051  | 54           | 70           | <img src="/img/gold_mapbox.svg" width="100" /> |
| [stb_rect_pack][3] from Sean Barrett               | **576** | 578          | 610     | 97           | 114          | <img src="/img/gold_rectpack.svg" width="100" /> |
| [RectAllocator][4] from Andrew Willmott            | 912     | 248          | 331     | 306          | 387          | <img src="/img/gold_awralloc.svg" width="100" /> |

[1]: https://github.com/nical/etagere
[2]: https://github.com/mapbox/shelf-pack-cpp
[3]: https://github.com/nothings/stb/blob/master/stb_rect_pack.h
[4]: https://gist.github.com/andrewwillmott/f9124eb445df7b3687a666fe36d3dcdb

My strategy for atlas resizing is the same for all the cases tested.
- Initial atlas size is 1024x1024.
- When an item no longer fits into atlas, even after removing old items:
  - First just try to repack all the items into atlas *of the same size*.
    - This particularly makes cases that *do not actually support item removal* (like `stb_rect_pack`) "actually work",
      since it gives them a chance to "clean up" all the would-be-unused space.
    - But it also seems to help all or most of the other libraries too!
  - If the items can't be repacked into the same atlas size, increase the size and try again. However, I am not
    simply doubling the size, but rather increasing the smaller dimension by increments of 512.

`smol-atlas` seems to be a tiny bit faster than `Étagère`, faster than Mapbox `shelf-pack-cpp`, and quite a lot
faster than the slightly mis-used STB `stb_rect_pack` library ("mis-used" because it does not natively support
item removal).

### So is Étagère good?

Yes it does look good ([github](https://github.com/nical/etagere) / [blog post](https://nical.github.io/posts/etagere.html)).
Unlike `smol-atlas`, it is presumably way better tested, given that it is part of Firefox.
But it is written in Rust, which may or might not suit your needs.

For testing it *here*, I compiled it as a shared library to be used from C. Notes to myself how I did it:

Build the dynamic libraries locally by:
- Edit `Cargo.toml` and add this section:
  ```
  [lib]
  crate-type = ["cdylib"]
  ```
- Create file `.cargo/config.toml` with contents:
  ```
  [target.aarch64-apple-darwin]
  rustflags = ["-C", "link-args=-Wl,-install_name,@rpath/libetagere.dylib"]
  ```
- Build the dynamic library with `cargo build --release --features "ffi"`, it will be under `target/release`.
- Generate header file with `cbindgen --config cbindgen.toml --crate etagere --output etagere.h`. You might need to do
  `cargo install cbindgen` first.
- These get copied into `external/etagere` of this project, for Windows (x64) and macOS (arm64).

### Further reading

- [A Thousand Ways to Pack the Bin](https://github.com/juj/RectangleBinPack/blob/master/RectangleBinPack.pdf) by Jukka Jylänki (2010)
  is a good overview. It does not talk about item removal though.
- [Improving texture atlas allocation in WebRender](https://nical.github.io/posts/etagere.html) by Nicolas Silva (2021) is really
  good!
  - Describes development journey through various atlasing approaches.
  - Rust-based [Étagère](https://github.com/nical/etagere) library that is used by Firefox, that is mentioned above. Similar to
    `smol-atlas`, it uses a shelf packing algorithm.
  - Also Rust-based [Guillotière](https://github.com/nical/guillotiere) that uses a "guillotine" algorithm with tree based
    data structure to merge removed areas. I could not get it to build for C consumption to test here though. But then, I don't
    really know how to Rust things...
- [Mapbox shelf-pack-cpp](https://github.com/mapbox/shelf-pack-cpp) is a C++ port of `shelf-pack` Javascript library from Mapbox.

### Possible future plans

Not sure if any of this will happen, but here's a list of things that would be interesting to try:
- Usage: check/see whether it *actually* makes sense to ~put it into Blender's video sequence editor for thumbnails :)~
  - Update: for now ([Blender 4.3](https://developer.blender.org/docs/release_notes/4.3/sequencer/)), I made thumbnails in Blender's VSE
    use a much simpler scheme that rebuilds whole atlas every frame.
- Write a blog post about this mayhaps?
- API: change to return "handles" instead of raw item pointers. They would be both smaller and safer.
- API: provide ways of passing your own memory allocation functions.
- Algo: play around with different ways of allocating items. E.g. instead of "first fit" within the shelf, maybe a "best fit" or
  "worst fit" would work better? Or maybe a full fledged "allocator" of 1D space within the shelf?
