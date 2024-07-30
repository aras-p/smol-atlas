[![Build Status](https://circleci.com/gh/mapbox/shelf-pack-cpp.svg?style=shield)](https://circleci.com/gh/mapbox/shelf-pack-cpp)

## shelf-pack-cpp

C++ port of [shelf-pack](https://github.com/mapbox/shelf-pack)

A 2D rectangular [bin packing](https://en.wikipedia.org/wiki/Bin_packing_problem)
data structure that uses the Shelf Best Height Fit heuristic.


### What is it?

`shelf-pack` is a library for packing little rectangles into a big rectangle.  This sounds simple enough,
but finding an optimal packing is a problem with [NP-Complete](https://en.wikipedia.org/wiki/NP-completeness)
complexity.  One useful application of bin packing is to assemble icons or glyphs into a sprite texture.

There are many ways to approach the bin packing problem, but `shelf-pack` uses the Shelf Best
Height Fit heuristic.  It works by dividing the total space into "shelves", each with a certain height.
The allocator packs rectangles onto whichever shelf minimizes the amount of wasted vertical space.

`shelf-pack` is simple, fast, and works best when the rectangles have similar heights (icons and glyphs
are like this).  It is not a generalized bin packer, and can potentially waste a lot of space if the
rectangles vary significantly in height.


### Usage

#### Basic Usage

```cpp
#include <mapbox/shelf-pack.hpp>
#include <iostream>

using namespace mapbox;

void main(void) {

    // Initialize the sprite with a width and height..
    ShelfPack sprite(64, 64);

    // Pack bins one at a time..
    for (int i = 0; i < 5; i++) {
        Bin* bin = sprite.packOne(-1, 10, 10);
        // `packOne()` accepts parameters: `id`, `width`, `height`
        // and returns a pointer to a single allocated Bin object..
        // `id` is optional - pass `-1` and shelf-pack will make up a number for you..

        if (bin) {
            std::cout << *bin << std::endl;
        } else {
            std::cout << "out of space" << std::endl;
        }

        /* output:
        Bin { id: 1, x: 0, y: 0, w: 10, h: 10, maxw: 10, maxh: 10, refcount: 1 }
        Bin { id: 2, x: 32, y: 0, w: 10, h: 10, maxw: 10, maxh: 10, refcount: 1 }
        Bin { id: 3, x: 0, y: 32, w: 10, h: 10, maxw: 10, maxh: 10, refcount: 1 }
        Bin { id: 4, x: 32, y: 32, w: 10, h: 10, maxw: 10, maxh: 10, refcount: 1 }
        out of space
        */
    }

    // Clear sprite and start over..
    sprite.clear();

    // Or, resize sprite by passing larger dimensions..
    sprite.resize(128, 128);   // width, height
}
```


#### Batch packing

```cpp
#include <mapbox/shelf-pack.hpp>
#include <iostream>

void main(void) {

    // If you don't want to think about the size of the sprite,
    // the `autoResize` option will allow it to grow as needed..
    ShelfPack::ShelfPackOptions options;
    options.autoResize = true;
    ShelfPack sprite(10, 10, options);

    // Bins can be allocated in batches..
    // Each bin should be initialized with `id`, `w` (width), `h` (height)..
    std::vector<Bin> bins;
    bins.emplace_back(-1, 10, 10);
    bins.emplace_back(-1, 10, 12);
    bins.emplace_back(-1, 10, 12);
    bins.emplace_back(-1, 10, 10);

    // `pack()` returns a vector of Bin* pointers, with `x`, `y`, `w`, `h` values..
    std::vector<Bin*> results = sprite.pack(bins);

    for (const auto& bin : results) {
        std::cout << *bin << std::endl;
    }
    /* output:
    Bin { id: 1, x: 0, y: 0, w: 10, h: 10, maxw: 10, maxh: 10, refcount: 1 }
    Bin { id: 2, x: 0, y: 10, w: 10, h: 12, maxw: 10, maxh: 12, refcount: 1 }
    Bin { id: 3, x: 10, y: 10, w: 10, h: 12, maxw: 10, maxh: 12, refcount: 1 }
    Bin { id: 4, x: 10, y: 0, w: 10, h: 10, maxw: 10, maxh: 10, refcount: 1 }
    */

    // If you don't mind letting ShelfPack modify your objects,
    // the `inPlace` option will assign `id`, `x`, `y` values to the incoming Bins.
    // Fancy!
    std::vector<Bin> myBins;
    myBins.emplace_back(-1, 12, 24);
    myBins.emplace_back(-1, 12, 12);
    myBins.emplace_back(-1, 10, 10);

    ShelfPack::PackOptions options;
    options.inPlace = true;

    sprite.pack(myBins, options);

    for (const auto& mybin : myBins) {
        std::cout << mybin << std::endl;
    }

    /* output:
    { id: 5, x: 0, y: 22, w: 12, h: 24, maxw: 12, maxh: 24, refcount: 1 }
    { id: 6, x: 20, y: 10, w: 12, h: 12, maxw: 12, maxh: 12, refcount: 1 }
    { id: 7, x: 20, y: 0, w: 10, h: 10, maxw: 10, maxh: 10, refcount: 1 }
    */
}

```

#### Reference Counting

```cpp
#include <mapbox/shelf-pack.hpp>
#include <iostream>
#include <array>

void main(void) {

    // Initialize the sprite with a width and height..
    ShelfPack sprite(64, 64);
    Bin* bin;

    // Allocated bins are automatically reference counted.
    // They start out having a refcount of 1.
    std::array<int32_t, 3> ids = { 100, 101, 102 };
    for(const auto& id : ids) {
        bin = sprite.packOne(id, 16, 16);
        std::cout << *bin << std::endl;
    }

    /* output:
    Bin { id: 100, x: 0, y: 0, w: 16, h: 16, maxw: 16, maxh: 16, refcount: 1 }
    Bin { id: 101, x: 16, y: 0, w: 16, h: 16, maxw: 16, maxh: 16, refcount: 1 }
    Bin { id: 102, x: 32, y: 0, w: 16, h: 16, maxw: 16, maxh: 16, refcount: 1 }
    */

    // If you try to pack the same id again, shelf-pack will not re-pack it.
    // Instead, it will increment the reference count automatically..
    Bin* bin102 = sprite.packOne(102, 16, 16);
    std::cout << *bin102 << std::endl;

    /* output:
    Bin { id: 102, x: 32, y: 0, w: 16, h: 16, maxw: 16, maxh: 16, refcount: 2 }
    */

    // You can also manually increment the reference count..
    Bin* bin101 = sprite.getBin(101);
    sprite.ref(*bin101);
    std::cout << *bin101 << std::endl;

    /* output:
    Bin { id: 101, x: 16, y: 0, w: 16, h: 16, maxw: 16, maxh: 16, refcount: 2 }
    */

    // ...and decrement it!
    Bin* bin100 = sprite.getBin(100);
    sprite.unref(*bin100);
    std::cout << *bin100 << std::endl;

    /* output:
    Bin { id: 100, x: 0, y: 0, w: 16, h: 16, maxw: 16, maxh: 16, refcount: 0 }
    */

    // Bins with a refcount of 0 are considered free space.
    // Next time a bin is packed, shelf-back tries to reuse free space first.
    // See how Bin 103 gets allocated at [0,0] - Bin 100's old spot!
    Bin* bin103 = sprite.packOne(103, 16, 15);
    std::cout << *bin103 << std::endl;

    /* output:
    Bin { id: 103, x: 0, y: 0, w: 16, h: 15, maxw: 16, maxh: 16, refcount: 1 }
    */

    // Bin 103 may be smaller (16x15) but it knows 16x16 was its original size.
    // If that space becomes free again, a 16x16 bin will still fit there.
    sprite.unref(*bin103);
    Bin* bin104 = sprite.packOne(104, 16, 16);
    std::cout << *bin104 << std::endl;

    /* output:
    Bin { id: 104, x: 0, y: 0, w: 16, h: 16, maxw: 16, maxh: 16, refcount: 1 }
    */

```


### Documentation

Complete API documentation can be found on the JavaScript version of the project:
http://mapbox.github.io/shelf-pack/docs/


### See also

J. Jylänky, "A Thousand Ways to Pack the Bin - A Practical
Approach to Two-Dimensional Rectangle Bin Packing,"
http://clb.demon.fi/files/RectangleBinPack.pdf, 2010
