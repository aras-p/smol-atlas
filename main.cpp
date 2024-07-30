#include "external/mapbox-shelf-pack-cpp/include/mapbox/shelf-pack.hpp"

#include <cassert>
#include <iostream>
#include <ctime>
#include <stdio.h>
#include <vector>
#include <stdint.h>

using namespace mapbox;

// https://en.wikipedia.org/wiki/Permuted_congruential_generator
static uint64_t       state      = 0x4d595df4d0f33173;      // Or something seed-dependent
static uint64_t const multiplier = 6364136223846793005u;
static uint64_t const increment  = 1442695040888963407u;    // Or an arbitrary odd constant

static uint32_t rotr32(uint32_t x, unsigned r)
{
    return x >> r | x << (-r & 31);
}

uint32_t pcg32(void)
{
    uint64_t x = state;
    unsigned count = (unsigned)(x >> 59);       // 59 = 64 - 5

    state = x * multiplier + increment;
    x ^= x >> 18;                               // 18 = (64 - 27)/2
    return rotr32((uint32_t)(x >> 27), count);  // 27 = 32 - 5
}

void pcg32_init(uint64_t seed)
{
    state = seed + increment;
    (void)pcg32();
}


/*
 * Custom ostream<< overload for printing Bin details
 * Useful for debugging.
 *
 * @param    {ostream&}    os    output stream to print to
 * @param    {const Bin&}  bin   Bin to print
 * @returns  {ostream&}    output stream
 *
 * @example
 * std::cout << *sprite.getBin(5) << std::endl;
 */
std::ostream& operator<<(std::ostream& os, const Bin& bin) {
    os << "Bin { id: " << bin.id
        << ", x: " << bin.x << ", y: " << bin.y
        << ", w: " << bin.w << ", h: " << bin.h
        << ", maxw: " << bin.maxw << ", maxh: " << bin.maxh
        << ", refcount: " << bin.refcount() << " }";
    return os;
}


void testVersion() {
    std::cout << "has a version";
    std::string version(SHELF_PACK_VERSION);
    assert(version.length());
    std::cout << " - " << version << " - OK" << std::endl;
}

void testPack1() {
    std::cout << "batch pack() allocates same height bins on existing shelf";

    ShelfPack sprite(64, 64);
    std::vector<Bin> bins;
    std::vector<Bin*> results;

    bins.emplace_back(-1, 10, 10);
    bins.emplace_back(-1, 10, 10);
    bins.emplace_back(-1, 10, 10);
    results = sprite.pack(bins);

    //  x: 0, y: 0, w: 10, h: 10
    assert(results[0]->id == 1);
    assert(results[0]->x == 0);
    assert(results[0]->y == 0);
    assert(results[0]->w == 10);
    assert(results[0]->h == 10);
    assert(results[0]->maxw == 10);
    assert(results[0]->maxh == 10);

    //  x: 10, y: 0, w: 10, h: 10
    assert(results[1]->id == 2);
    assert(results[1]->x == 10);
    assert(results[1]->y == 0);
    assert(results[1]->w == 10);
    assert(results[1]->h == 10);
    assert(results[1]->maxw == 10);
    assert(results[1]->maxh == 10);

    //  x: 20, y: 0, w: 10, h: 10
    assert(results[2]->id == 3);
    assert(results[2]->x == 20);
    assert(results[2]->y == 0);
    assert(results[2]->w == 10);
    assert(results[2]->h == 10);
    assert(results[2]->maxw == 10);
    assert(results[2]->maxh == 10);

    std::cout << " - OK" << std::endl;
}

void testPack2() {
    std::cout << "batch pack() allocates larger bins on new shelf";

    ShelfPack sprite(64, 64);
    std::vector<Bin> bins;
    std::vector<Bin*> results;

    bins.emplace_back(-1, 10, 10);
    bins.emplace_back(-1, 10, 15);
    bins.emplace_back(-1, 10, 20);
    results = sprite.pack(bins);

    //  x: 0, y: 0, w: 10, h: 10
    assert(results[0]->id == 1);
    assert(results[0]->x == 0);
    assert(results[0]->y == 0);
    assert(results[0]->w == 10);
    assert(results[0]->h == 10);
    assert(results[0]->maxw == 10);
    assert(results[0]->maxh == 10);

    //  x: 0, y: 10, w: 10, h: 15
    assert(results[1]->id == 2);
    assert(results[1]->x == 0);
    assert(results[1]->y == 10);
    assert(results[1]->w == 10);
    assert(results[1]->h == 15);
    assert(results[1]->maxw == 10);
    assert(results[1]->maxh == 15);

    //  x: 0, y: 25, w: 10, h: 20
    assert(results[2]->id == 3);
    assert(results[2]->x == 0);
    assert(results[2]->y == 25);
    assert(results[2]->w == 10);
    assert(results[2]->h == 20);
    assert(results[2]->maxw == 10);
    assert(results[2]->maxh == 20);

    std::cout << " - OK" << std::endl;
}

void testPack3() {
    std::cout << "batch pack() allocates shorter bins on existing shelf, minimizing waste";

    ShelfPack sprite(64, 64);
    std::vector<Bin> bins;
    std::vector<Bin*> results;

    bins.emplace_back(-1, 10, 10);
    bins.emplace_back(-1, 10, 15);
    bins.emplace_back(-1, 10, 20);
    bins.emplace_back(-1, 10,  9);
    results = sprite.pack(bins);

    //  x: 0, y: 0, w: 10, h: 10
    assert(results[0]->id == 1);
    assert(results[0]->x == 0);
    assert(results[0]->y == 0);
    assert(results[0]->w == 10);
    assert(results[0]->h == 10);
    assert(results[0]->maxw == 10);
    assert(results[0]->maxh == 10);

    //  x: 0, y: 10, w: 10, h: 15
    assert(results[1]->id == 2);
    assert(results[1]->x == 0);
    assert(results[1]->y == 10);
    assert(results[1]->w == 10);
    assert(results[1]->h == 15);
    assert(results[1]->maxw == 10);
    assert(results[1]->maxh == 15);

    //  x: 0, y: 25, w: 10, h: 20
    assert(results[2]->id == 3);
    assert(results[2]->x == 0);
    assert(results[2]->y == 25);
    assert(results[2]->w == 10);
    assert(results[2]->h == 20);
    assert(results[2]->maxw == 10);
    assert(results[2]->maxh == 20);

    //  x: 10, y: 0, w: 10, h: 9
    assert(results[3]->id == 4);
    assert(results[3]->x == 10);
    assert(results[3]->y == 0);
    assert(results[3]->w == 10);
    assert(results[3]->h == 9);
    assert(results[3]->maxw == 10);
    assert(results[3]->maxh == 10);

    std::cout << " - OK" << std::endl;
}

void testPack4() {
    std::cout << "batch pack() sets `id`, `x`, `y` properties on bins with `inPlace` option";

    ShelfPack sprite(64, 64);
    std::vector<Bin> bins;

    ShelfPack::PackOptions options;
    options.inPlace = true;

    bins.emplace_back(-1, 10, 10);
    bins.emplace_back(-1, 10, 10);
    bins.emplace_back(-1, 10, 10);
    sprite.pack(bins, options);

    //  x: 0, y: 0, w: 10, h: 10
    assert(bins[0].id == 1);
    assert(bins[0].x == 0);
    assert(bins[0].y == 0);
    assert(bins[0].w == 10);
    assert(bins[0].h == 10);

    //  x: 10, y: 0, w: 10, h: 10
    assert(bins[1].id == 2);
    assert(bins[1].x == 10);
    assert(bins[1].y == 0);
    assert(bins[1].w == 10);
    assert(bins[1].h == 10);

    //  x: 20, y: 0, w: 10, h: 10
    assert(bins[2].id == 3);
    assert(bins[2].x == 20);
    assert(bins[2].y == 0);
    assert(bins[2].w == 10);
    assert(bins[2].h == 10);

    std::cout << " - OK" << std::endl;
}

void testPack5() {
    std::cout << "batch pack() skips bins if not enough room";

    ShelfPack sprite(20, 20);
    std::vector<Bin> bins;
    std::vector<Bin*> results;

    ShelfPack::PackOptions options;
    options.inPlace = true;

    bins.emplace_back(-1, 10, 10);
    bins.emplace_back(-1, 10, 10);
    bins.emplace_back(-1, 10, 30);   // should skip
    bins.emplace_back(-1, 10, 10);
    results = sprite.pack(bins, options);

    //  x: 0, y: 0, w: 10, h: 10
    assert(results[0]->id == 1);
    assert(results[0]->x == 0);
    assert(results[0]->y == 0);
    assert(results[0]->w == 10);
    assert(results[0]->h == 10);
    assert(results[0]->maxw == 10);
    assert(results[0]->maxh == 10);

    //  x: 10, y: 0, w: 10, h: 10
    assert(results[1]->id == 2);
    assert(results[1]->x == 10);
    assert(results[1]->y == 0);
    assert(results[1]->w == 10);
    assert(results[1]->h == 10);
    assert(results[1]->maxw == 10);
    assert(results[1]->maxh == 10);

    //  x: 0, y: 10, w: 10, h: 10
    assert(results[2]->id == 4);
    assert(results[2]->x == 0);
    assert(results[2]->y == 10);
    assert(results[2]->w == 10);
    assert(results[2]->h == 10);
    assert(results[2]->maxw == 10);
    assert(results[2]->maxh == 10);

    //  x: 0, y: 0, w: 10, h: 10
    assert(bins[0].id == 1);
    assert(bins[0].x == 0);
    assert(bins[0].y == 0);
    assert(bins[0].w == 10);
    assert(bins[0].h == 10);

    //  x: 10, y: 0, w: 10, h: 10
    assert(bins[1].id == 2);
    assert(bins[1].x == 10);
    assert(bins[1].y == 0);
    assert(bins[1].w == 10);
    assert(bins[1].h == 10);

    //  x: -1, y: -1, w: 10, h: 30
    assert(bins[2].id == -1);
    assert(bins[2].x == -1);
    assert(bins[2].y == -1);
    assert(bins[2].w == 10);
    assert(bins[2].h == 30);

    //  x: 0, y: 10, w: 10, h: 10
    assert(bins[3].id == 4);
    assert(bins[3].x == 0);
    assert(bins[3].y == 10);
    assert(bins[3].w == 10);
    assert(bins[3].h == 10);

    std::cout << " - OK" << std::endl;
}

void testPack6() {
    std::cout << "batch pack() results in minimal sprite width and height";

    std::vector<Bin> bins;
    std::vector<Bin*> results;
    bins.emplace_back(-1, 10, 10);
    bins.emplace_back(-1, 5,  15);
    bins.emplace_back(-1, 25, 15);
    bins.emplace_back(-1, 10, 20);

    ShelfPack::ShelfPackOptions options;
    options.autoResize = true;

    ShelfPack sprite(10, 10, options);
    results = sprite.pack(bins);

    //  x: 0, y: 0, w: 10, h: 10
    assert(results[0]->id == 1);
    assert(results[0]->x == 0);
    assert(results[0]->y == 0);
    assert(results[0]->w == 10);
    assert(results[0]->h == 10);
    assert(results[0]->maxw == 10);
    assert(results[0]->maxh == 10);

    //  x: 0, y: 10, w: 5, h: 15
    assert(results[1]->id == 2);
    assert(results[1]->x == 0);
    assert(results[1]->y == 10);
    assert(results[1]->w == 5);
    assert(results[1]->h == 15);
    assert(results[1]->maxw == 5);
    assert(results[1]->maxh == 15);

    //  x: 5, y: 10, w: 25, h: 15
    assert(results[2]->id == 3);
    assert(results[2]->x == 5);
    assert(results[2]->y == 10);
    assert(results[2]->w == 25);
    assert(results[2]->h == 15);
    assert(results[2]->maxw == 25);
    assert(results[2]->maxh == 15);

    //  x: 0, y: 25, w: 10, h: 20
    assert(results[3]->id == 4);
    assert(results[3]->x == 0);
    assert(results[3]->y == 25);
    assert(results[3]->w == 10);
    assert(results[3]->h == 20);
    assert(results[3]->maxw == 10);
    assert(results[3]->maxh == 20);

    // Since shelf-pack doubles width/height when packing bins one by one
    // (first width, then height) this would result in a 50x60 sprite here.
    // But this can be shrunk to a 30x45 sprite.
    assert(sprite.width() == 30);
    assert(sprite.height() == 45);

    std::cout << " - OK" << std::endl;
}

void testPackOne1() {
    std::cout << "packOne() allocates bins with numeric id";

    ShelfPack sprite(64, 64);
    Bin* bin = sprite.packOne(1000, 10, 10);

    assert(bin->id == 1000);
    assert(bin->x == 0);
    assert(bin->y == 0);
    assert(bin->w == 10);
    assert(bin->h == 10);
    assert(bin->maxw == 10);
    assert(bin->maxh == 10);

    assert(sprite.getBin(1000) == bin);

    std::cout << " - OK" << std::endl;
}

void testPackOne2() {
    std::cout << "packOne() generates incremental numeric ids, if id not provided";

    ShelfPack sprite(64, 64);
    Bin* bin1 = sprite.packOne(-1, 10, 10);
    Bin* bin2 = sprite.packOne(-1, 10, 10);

    assert(bin1->id == 1);
    assert(bin2->id == 2);

    std::cout << " - OK" << std::endl;
}

void testPackOne3() {
    std::cout << "packOne() does not generate an id that collides with an existing id";

    ShelfPack sprite(64, 64);
    Bin* bin1 = sprite.packOne(1, 10, 10);
    Bin* bin2 = sprite.packOne(-1, 10, 10);

    assert(bin1->id == 1);
    assert(bin2->id == 2);

    std::cout << " - OK" << std::endl;
}

void testPackOne4() {
    std::cout << "packOne() does not reallocate a bin with an existing id";

    ShelfPack sprite(64, 64);
    Bin* bin1 = sprite.packOne(1000, 10, 10);

    assert(bin1->id == 1000);
    assert(bin1->x == 0);
    assert(bin1->y == 0);
    assert(bin1->w == 10);
    assert(bin1->h == 10);
    assert(bin1->maxw == 10);
    assert(bin1->maxh == 10);

    Bin* bin2 = sprite.packOne(1000, 10, 10);

    assert(bin2->id == 1000);
    assert(bin2->x == 0);
    assert(bin2->y == 0);
    assert(bin2->w == 10);
    assert(bin2->h == 10);
    assert(bin2->maxw == 10);
    assert(bin2->maxh == 10);

    assert(bin1 == bin2);   // same bin

    std::cout << " - OK" << std::endl;
}

void testPackOne5() {
    std::cout << "packOne() allocates same height bins on existing shelf";

    ShelfPack sprite(64, 64);
    Bin* bin1 = sprite.packOne(-1, 10, 10);
    Bin* bin2 = sprite.packOne(-1, 10, 10);
    Bin* bin3 = sprite.packOne(-1, 10, 10);

    //  x: 0, y: 0, w: 10, h: 10
    assert(bin1->id == 1);
    assert(bin1->x == 0);
    assert(bin1->y == 0);
    assert(bin1->w == 10);
    assert(bin1->h == 10);
    assert(bin1->maxw == 10);
    assert(bin1->maxh == 10);

    //  x: 10, y: 0, w: 10, h: 10
    assert(bin2->id == 2);
    assert(bin2->x == 10);
    assert(bin2->y == 0);
    assert(bin2->w == 10);
    assert(bin2->h == 10);
    assert(bin2->maxw == 10);
    assert(bin2->maxh == 10);

    //  x: 20, y: 0, w: 10, h: 10
    assert(bin3->id == 3);
    assert(bin3->x == 20);
    assert(bin3->y == 0);
    assert(bin3->w == 10);
    assert(bin3->h == 10);
    assert(bin3->maxw == 10);
    assert(bin3->maxh == 10);

    std::cout << " - OK" << std::endl;
}

void testPackOne6() {
    std::cout << "packOne() allocates larger bins on new shelf";

    ShelfPack sprite(64, 64);
    Bin* bin1 = sprite.packOne(-1, 10, 10);
    Bin* bin2 = sprite.packOne(-1, 10, 15);
    Bin* bin3 = sprite.packOne(-1, 10, 20);

    //  x: 0, y: 0, w: 10, h: 10
    assert(bin1->id == 1);
    assert(bin1->x == 0);
    assert(bin1->y == 0);
    assert(bin1->w == 10);
    assert(bin1->h == 10);
    assert(bin1->maxw == 10);
    assert(bin1->maxh == 10);

    //  x: 0, y: 10, w: 10, h: 15
    assert(bin2->id == 2);
    assert(bin2->x == 0);
    assert(bin2->y == 10);
    assert(bin2->w == 10);
    assert(bin2->h == 15);
    assert(bin2->maxw == 10);
    assert(bin2->maxh == 15);

    //  x: 0, y: 25, w: 10, h: 20
    assert(bin3->id == 3);
    assert(bin3->x == 0);
    assert(bin3->y == 25);
    assert(bin3->w == 10);
    assert(bin3->h == 20);
    assert(bin3->maxw == 10);
    assert(bin3->maxh == 20);

    std::cout << " - OK" << std::endl;
}

void testPackOne7() {
    std::cout << "packOne() allocates shorter bins on existing shelf, minimizing waste";

    ShelfPack sprite(64, 64);
    Bin* bin1 = sprite.packOne(-1, 10, 10);
    Bin* bin2 = sprite.packOne(-1, 10, 15);
    Bin* bin3 = sprite.packOne(-1, 10, 20);
    Bin* bin4 = sprite.packOne(-1, 10,  9);

    //  x: 0, y: 0, w: 10, h: 10
    assert(bin1->id == 1);
    assert(bin1->x == 0);
    assert(bin1->y == 0);
    assert(bin1->w == 10);
    assert(bin1->h == 10);
    assert(bin1->maxw == 10);
    assert(bin1->maxh == 10);

    //  x: 0, y: 10, w: 10, h: 15
    assert(bin2->id == 2);
    assert(bin2->x == 0);
    assert(bin2->y == 10);
    assert(bin2->w == 10);
    assert(bin2->h == 15);
    assert(bin2->maxw == 10);
    assert(bin2->maxh == 15);

    //  x: 0, y: 25, w: 10, h: 20
    assert(bin3->id == 3);
    assert(bin3->x == 0);
    assert(bin3->y == 25);
    assert(bin3->w == 10);
    assert(bin3->h == 20);
    assert(bin3->maxw == 10);
    assert(bin3->maxh == 20);

    //  x: 10, y: 0, w: 10, h: 9
    assert(bin4->id == 4);
    assert(bin4->x == 10);
    assert(bin4->y == 0);
    assert(bin4->w == 10);
    assert(bin4->h == 9);
    assert(bin4->maxw == 10);
    assert(bin4->maxh == 10);

    std::cout << " - OK" << std::endl;
}

void testPackOne8() {
    std::cout << "packOne() returns NULL if not enough room";

    ShelfPack sprite(10, 10);
    Bin* bin1 = sprite.packOne(-1, 10, 10);

    //  x: 0, y: 0, w: 10, h: 10
    assert(bin1->id == 1);
    assert(bin1->x == 0);
    assert(bin1->y == 0);
    assert(bin1->w == 10);
    assert(bin1->h == 10);
    assert(bin1->maxw == 10);
    assert(bin1->maxh == 10);

    Bin* bin2 = sprite.packOne(-1, 10, 10);
    assert(bin2 == NULL);

    std::cout << " - OK" << std::endl;
}

void testPackOne9() {
    std::cout << "packOne() allocates in free bin if possible";

    ShelfPack sprite(64, 64);
    sprite.packOne(1, 10, 10);
    sprite.packOne(2, 10, 10);
    sprite.packOne(3, 10, 10);

    Bin* bin2 = sprite.getBin(2);
    sprite.unref(*bin2);

    Bin* bin4 = sprite.packOne(4, 10, 10);

    //  x: 10, y: 0, w: 10, h: 10
    assert(bin4->id == 4);
    assert(bin4->x == 10);
    assert(bin4->y == 0);
    assert(bin4->w == 10);
    assert(bin4->h == 10);
    assert(bin4->maxw == 10);
    assert(bin4->maxh == 10);

    assert(bin4 == bin2);   // reused bin2

    std::cout << " - OK" << std::endl;
}

void testPackOne10() {
    std::cout << "packOne() allocates new bin in least wasteful free bin";

    ShelfPack sprite(64, 64);
    sprite.packOne(1, 10, 10);
    sprite.packOne(2, 10, 15);
    sprite.packOne(3, 10, 20);

    Bin* bin2 = sprite.getBin(2);

    sprite.unref(*sprite.getBin(3));
    sprite.unref(*sprite.getBin(2));
    sprite.unref(*sprite.getBin(1));

    Bin* bin4 = sprite.packOne(4, 10, 13);

    //  x: 0, y: 10, w: 10, h: 13
    assert(bin4->id == 4);
    assert(bin4->x == 0);
    assert(bin4->y == 10);
    assert(bin4->w == 10);
    assert(bin4->h == 13);
    assert(bin4->maxw == 10);
    assert(bin4->maxh == 15);

    assert(bin4 == bin2);   // reused bin2

    std::cout << " - OK" << std::endl;
}

void testPackOne11() {
    std::cout << "packOne() avoids free bin if all are more wasteful than packing on a shelf";

    ShelfPack sprite(64, 64);
    sprite.packOne(1, 10, 10);
    sprite.packOne(2, 10, 15);

    Bin* bin2 = sprite.getBin(2);
    sprite.unref(*bin2);

    Bin* bin3 = sprite.packOne(3, 10, 10);

    //  x: 10, y: 0, w: 10, h: 10
    assert(bin3->id == 3);
    assert(bin3->x == 10);
    assert(bin3->y == 0);
    assert(bin3->w == 10);
    assert(bin3->h == 10);
    assert(bin3->maxw == 10);
    assert(bin3->maxh == 10);

    assert(bin3 != bin2);   // did not reuse bin2

    std::cout << " - OK" << std::endl;
}

void testPackOne12() {
    std::cout << "packOne() considers max bin dimensions when reusing a free bin";

    ShelfPack sprite(64, 64);
    sprite.packOne(1, 10, 10);
    Bin* bin2 = sprite.packOne(2, 10, 15);

    sprite.unref(*bin2);

    Bin* bin3 = sprite.packOne(3, 10, 13);

    //  x: 0, y: 10, w: 10, h: 13
    assert(bin3->id == 3);
    assert(bin3->x == 0);
    assert(bin3->y == 10);
    assert(bin3->w == 10);
    assert(bin3->h == 13);
    assert(bin3->maxw == 10);
    assert(bin3->maxh == 15);

    assert(bin3 == bin2);   // reused bin2

    sprite.unref(*bin3);

    Bin* bin4 = sprite.packOne(4, 10, 14);

    //  x: 0, y: 10, w: 10, h: 14
    assert(bin4->id == 4);
    assert(bin4->x == 0);
    assert(bin4->y == 10);
    assert(bin4->w == 10);
    assert(bin4->h == 14);
    assert(bin4->maxw == 10);
    assert(bin4->maxh == 15);

    assert(bin4 == bin2);   // reused bin2
    assert(bin4 == bin3);   // reused bin3

    std::cout << " - OK" << std::endl;
}


void testGetBin1() {
    std::cout << "getBin() returns NULL if Bin not found";

    ShelfPack sprite(64, 64);
    Bin* bin = sprite.getBin(1);

    assert(bin == NULL);

    std::cout << " - OK" << std::endl;
}

void testGetBin2() {
    std::cout << "getBin() gets a Bin by numeric id";

    ShelfPack sprite(64, 64);
    Bin* bin = sprite.packOne(1, 10, 10);
    Bin* bin1 = sprite.getBin(1);

    assert(bin == bin1);

    std::cout << " - OK" << std::endl;
}


void testRef() {
    std::cout << "ref() increments the Bin refcount and updates stats";

    ShelfPack sprite(64, 64);

    Bin* bin1 = sprite.packOne(1, 10, 10);
    assert(bin1->refcount() == 1);
    assert(sprite.ref(*bin1) == 2);
    assert(bin1->refcount() == 2);

    Bin* bin2 = sprite.packOne(2, 10, 10);
    assert(bin2->refcount() == 1);
    assert(sprite.ref(*bin2) == 2);
    assert(bin2->refcount() == 2);

    Bin* bin3 = sprite.packOne(3, 10, 15);
    assert(bin3->refcount() == 1);
    assert(sprite.ref(*bin3) == 2);
    assert(bin3->refcount() == 2);

    std::cout << " - OK" << std::endl;
}

void testUnref1() {
    std::cout << "unref() decrements the Bin refcount and updates stats";

    ShelfPack sprite(64, 64);

    // setup
    Bin* bin1 = sprite.packOne(1, 10, 10);
    sprite.ref(*bin1);
    Bin* bin2 = sprite.packOne(2, 10, 10);
    sprite.ref(*bin2);
    Bin* bin3 = sprite.packOne(3, 10, 15);
    sprite.ref(*bin3);

    assert(bin1->refcount() == 2);
    assert(bin2->refcount() == 2);
    assert(bin3->refcount() == 2);

    assert(sprite.unref(*bin3) == 1);
    assert(bin3->refcount() == 1);
    assert(sprite.unref(*bin3) == 0);
    assert(bin3->refcount() == 0);
    assert(sprite.getBin(3) == NULL);

    assert(sprite.unref(*bin2) == 1);
    assert(bin2->refcount() == 1);
    assert(sprite.unref(*bin2) == 0);
    assert(bin2->refcount() == 0);
    assert(sprite.getBin(2) == NULL);

    std::cout << " - OK" << std::endl;
}

void testUnref2() {
    std::cout << "unref() does nothing if refcount is already 0";

    ShelfPack sprite(64, 64);
    Bin* bin = sprite.packOne(1, 10, 10);

    assert(sprite.unref(*bin) == 0);
    assert(bin->refcount() == 0);

    assert(sprite.unref(*bin) == 0);  // still 0
    assert(bin->refcount() == 0);     // still 0

    std::cout << " - OK" << std::endl;
}

void testClear() {
    std::cout << "clear succeeds";

    ShelfPack sprite(10, 10);
    Bin* bin1 = sprite.packOne(-1, 10, 10);

    //  x: 0, y: 0, w: 10, h: 10
    assert(bin1->id == 1);
    assert(bin1->x == 0);
    assert(bin1->y == 0);
    assert(bin1->w == 10);
    assert(bin1->h == 10);
    assert(bin1->maxw == 10);
    assert(bin1->maxh == 10);

    Bin* bin2 = sprite.packOne(-1, 10, 10);
    assert(bin2 == NULL);

    sprite.clear();

    Bin* bin3 = sprite.packOne(-1, 10, 10);

    //  x: 0, y: 0, w: 10, h: 10
    assert(bin3->id == 1);    // clear resets max id
    assert(bin3->x == 0);
    assert(bin3->y == 0);
    assert(bin3->w == 10);
    assert(bin3->h == 10);
    assert(bin3->maxw == 10);
    assert(bin3->maxh == 10);

    std::cout << " - OK" << std::endl;
}

void testShrink() {
    std::cout << "shrink succeeds";

    ShelfPack sprite(20, 20);
    sprite.packOne(-1, 10, 5);

    assert(sprite.width() == 20);
    assert(sprite.height() == 20);

    sprite.shrink();

    assert(sprite.width() == 10);
    assert(sprite.height() == 5);

    std::cout << " - OK" << std::endl;
}

void testResize1() {
    std::cout << "resize larger succeeds";

    ShelfPack sprite(10, 10);
    Bin* bin1 = sprite.packOne(1, 10, 10);

    //  x: 0, y: 0, w: 10, h: 10
    assert(bin1->id == 1);
    assert(bin1->x == 0);
    assert(bin1->y == 0);
    assert(bin1->w == 10);
    assert(bin1->h == 10);
    assert(bin1->maxw == 10);
    assert(bin1->maxh == 10);

    assert(sprite.resize(20, 10) == true);   // grow width
    assert(sprite.width() == 20);
    assert(sprite.height() == 10);

    Bin* bin2 = sprite.packOne(2, 10, 10);

    //  x: 10, y: 0, w: 10, h: 10
    assert(bin2->id == 2);
    assert(bin2->x == 10);
    assert(bin2->y == 0);
    assert(bin2->w == 10);
    assert(bin2->h == 10);
    assert(bin2->maxw == 10);
    assert(bin2->maxh == 10);

    assert(sprite.resize(20, 20) == true);   // grow height
    assert(sprite.width() == 20);
    assert(sprite.height() == 20);

    Bin* bin3 = sprite.packOne(3, 10, 10);

    //  x: 0, y: 10, w: 10, h: 10
    assert(bin3->id == 3);
    assert(bin3->x == 0);
    assert(bin3->y == 10);
    assert(bin3->w == 10);
    assert(bin3->h == 10);
    assert(bin3->maxw == 10);
    assert(bin3->maxh == 10);

    std::cout << " - OK" << std::endl;
}

void testResize2() {
    std::cout << "autoResize grows sprite dimensions by width then height";

    ShelfPack::ShelfPackOptions options;
    options.autoResize = true;

    ShelfPack sprite(10, 10, options);

    Bin* bin1 = sprite.packOne(1, 10, 10);

    //  x: 0, y: 0, w: 10, h: 10
    assert(bin1->id == 1);
    assert(bin1->x == 0);
    assert(bin1->y == 0);
    assert(bin1->w == 10);
    assert(bin1->h == 10);
    assert(bin1->maxw == 10);
    assert(bin1->maxh == 10);

    assert(sprite.width() == 10);
    assert(sprite.height() == 10);

    Bin* bin2 = sprite.packOne(2, 10, 10);

    //  x: 10, y: 0, w: 10, h: 10
    assert(bin2->id == 2);
    assert(bin2->x == 10);
    assert(bin2->y == 0);
    assert(bin2->w == 10);
    assert(bin2->h == 10);
    assert(bin2->maxw == 10);
    assert(bin2->maxh == 10);

    assert(sprite.width() == 20);
    assert(sprite.height() == 10);

    Bin* bin3 = sprite.packOne(3, 10, 10);

    //  x: 0, y: 10, w: 10, h: 10
    assert(bin3->id == 3);
    assert(bin3->x == 0);
    assert(bin3->y == 10);
    assert(bin3->w == 10);
    assert(bin3->h == 10);
    assert(bin3->maxw == 10);
    assert(bin3->maxh == 10);

    assert(sprite.width() == 20);
    assert(sprite.height() == 20);

    Bin* bin4 = sprite.packOne(4, 10, 10);

    //  x: 10, y: 10, w: 10, h: 10
    assert(bin4->id == 4);
    assert(bin4->x == 10);
    assert(bin4->y == 10);
    assert(bin4->w == 10);
    assert(bin4->h == 10);
    assert(bin4->maxw == 10);
    assert(bin4->maxh == 10);

    assert(sprite.width() == 20);
    assert(sprite.height() == 20);

    Bin* bin5 = sprite.packOne(5, 10, 10);

    //  x: 20, y: 0, w: 10, h: 10
    assert(bin5->id == 5);
    assert(bin5->x == 20);
    assert(bin5->y == 0);
    assert(bin5->w == 10);
    assert(bin5->h == 10);
    assert(bin5->maxw == 10);
    assert(bin5->maxh == 10);

    assert(sprite.width() == 40);
    assert(sprite.height() == 20);

    std::cout << " - OK" << std::endl;
}

void testResize3() {
    std::cout << "autoResize accommodates big bin requests";

    ShelfPack::ShelfPackOptions options;
    options.autoResize = true;

    ShelfPack sprite(10, 10, options);

    Bin* bin1 = sprite.packOne(1, 20, 10);

    //  x: 0, y: 0, w: 20, h: 10
    assert(bin1->id == 1);
    assert(bin1->x == 0);
    assert(bin1->y == 0);
    assert(bin1->w == 20);
    assert(bin1->h == 10);
    assert(bin1->maxw == 20);
    assert(bin1->maxh == 10);

    assert(sprite.width() == 40);
    assert(sprite.height() == 10);

    Bin* bin2 = sprite.packOne(2, 10, 40);

    //  x: 0, y: 10, w: 10, h: 40
    assert(bin2->id == 2);
    assert(bin2->x == 0);
    assert(bin2->y == 10);
    assert(bin2->w == 10);
    assert(bin2->h == 40);
    assert(bin2->maxw == 10);
    assert(bin2->maxh == 40);

    assert(sprite.width() == 40);
    assert(sprite.height() == 80);

    std::cout << " - OK" << std::endl;
}

static int rand_size() { return (pcg32() & 63) * 4 + 3; }

void testBench()
{
    std::cout << "Bench" << std::endl;
    ShelfPack::ShelfPackOptions options;
    options.autoResize = true;

    const int count = 100000;
    std::vector<Bin*> bins;
    bins.reserve(count);

    pcg32_init(1);

    // pack initial thumbs
    // Mac M1 Max O2: 30.85ms
    std::clock_t t0 = std::clock();
    ShelfPack sprite(10, 10, options);
    for (int i = 0; i < count; ++i) {
        int w = rand_size();
        int h = rand_size();
        Bin *res = sprite.packOne(-1, w, h);
        if (!res) throw std::runtime_error("out of space");
        bins.push_back(res);
    }
    sprite.shrink();
    std::clock_t t1 = std::clock();
    double dur = (t1 - t0) / (double) CLOCKS_PER_SEC;
    printf("- packed %i, got %ix%i atlas, took %.2fms\n", count, sprite.width(), sprite.height(), dur*1000.0);

    assert(sprite.width() == 65280);
    assert(sprite.height() == 55962);

    // remove half of thumbs
    for (int i = 0; i < bins.size(); i += 2)
    {
        sprite.unref(*bins[i]);
    }

    // pack half of thumbs again
    // Mac M1 Max O2: 2798.90ms
    t0 = std::clock();
    for (int i = 0; i < bins.size(); i += 2)
    {
        int w = rand_size();
        int h = rand_size();
        Bin *res = sprite.packOne(-1, w, h);
        if (!res) throw std::runtime_error("out of space");
        bins[i] = res;
    }
    t1 = std::clock();
    dur = (t1 - t0) / (double) CLOCKS_PER_SEC;
    printf("- packed %i, got %ix%i atlas, took %.2fms\n", count/2, sprite.width(), sprite.height(), dur*1000.0);
    assert(sprite.width() == 65280);
    assert(sprite.height() == 55962);
}


int main() {
    std::cout << std::endl << "version" << std::endl << std::string(70, '-') << std::endl;
    testVersion();

    std::cout << std::endl << "pack()" << std::endl << std::string(70, '-') << std::endl;
    testPack1();
    testPack2();
    testPack3();
    testPack4();
    testPack5();
    testPack6();

    std::cout << std::endl << "packOne()" << std::endl << std::string(70, '-') << std::endl;
    testPackOne1();
    testPackOne2();
    testPackOne3();
    testPackOne4();
    testPackOne5();
    testPackOne6();
    testPackOne7();
    testPackOne8();
    testPackOne9();
    testPackOne10();
    testPackOne11();
    testPackOne12();

    std::cout << std::endl << "getBin()" << std::endl << std::string(70, '-') << std::endl;
    testGetBin1();
    testGetBin2();

    std::cout << std::endl << "ref()" << std::endl << std::string(70, '-') << std::endl;
    testRef();

    std::cout << std::endl << "unref()" << std::endl << std::string(70, '-') << std::endl;
    testUnref1();
    testUnref2();

    std::cout << std::endl << "clear()" << std::endl << std::string(70, '-') << std::endl;
    testClear();

    std::cout << std::endl << "resize()" << std::endl << std::string(70, '-') << std::endl;
    testResize1();
    testResize2();
    testResize3();

    testBench();

    return 0;
}
