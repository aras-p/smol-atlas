// SPDX-License-Identifier: MIT OR Unlicense
// smol-atlas: https://github.com/aras-p/smol-atlas

#include "../src/smol-atlas.h"

#include <stdio.h>
#include <stdlib.h>

#if defined(_MSC_VER)
#define BREAK_IN_DEBUGGER() __debugbreak()
#elif defined(__clang__)
#define BREAK_IN_DEBUGGER() __builtin_debugtrap()
#else
#define BREAK_IN_DEBUGGER()
#endif

#define CHECK(expression) \
    if (!(expression)) { \
        printf("ERROR: %s in %s at %s:%i\n", #expression, __FUNCTION__, __FILE__, __LINE__); \
        BREAK_IN_DEBUGGER(); \
        exit(1); \
    }

#define CHECK_EQ(a, b) \
    if ((a) != (b)) { \
        printf("ERROR: %i != %i in %s at %s:%i\n", (a), (b), __FUNCTION__, __FILE__, __LINE__); \
        BREAK_IN_DEBUGGER(); \
        exit(1); \
    }

#define CHECK_ITEM(e, x, y, w, h) { \
    CHECK_EQ(x, sma_item_x(e)); \
    CHECK_EQ(y, sma_item_y(e)); \
    CHECK_EQ(w, sma_item_width(e)); \
    CHECK_EQ(h, sma_item_height(e)); }

static void test_same_height_on_same_shelf()
{
    smol_atlas_t* atlas = sma_atlas_create(64, 64);

    smol_atlas_item_t* e1 = sma_item_add(atlas, 10, 10);
    smol_atlas_item_t* e2 = sma_item_add(atlas, 10, 10);
    smol_atlas_item_t* e3 = sma_item_add(atlas, 10, 10);

    CHECK_ITEM(e1, 0, 0, 10, 10);
    CHECK_ITEM(e2, 10, 0, 10, 10);
    CHECK_ITEM(e3, 20, 0, 10, 10);

    sma_atlas_destroy(atlas);
}

static void test_larger_height_new_shelf()
{
    smol_atlas_t* atlas = sma_atlas_create(64, 64);

    smol_atlas_item_t* e1 = sma_item_add(atlas, 10, 10);
    smol_atlas_item_t* e2 = sma_item_add(atlas, 10, 15);
    smol_atlas_item_t* e3 = sma_item_add(atlas, 10, 20);

    CHECK_ITEM(e1, 0, 0, 10, 10);
    CHECK_ITEM(e2, 0, 10, 10, 15);
    CHECK_ITEM(e3, 0, 25, 10, 20);

    sma_atlas_destroy(atlas);
}

static void test_shorter_height_existing_best_shelf()
{
    smol_atlas_t* atlas = sma_atlas_create(64, 64);

    smol_atlas_item_t* e1 = sma_item_add(atlas, 10, 10);
    smol_atlas_item_t* e2 = sma_item_add(atlas, 10, 15);
    smol_atlas_item_t* e3 = sma_item_add(atlas, 10, 20);
    smol_atlas_item_t* e4 = sma_item_add(atlas, 10, 9);

    CHECK_ITEM(e1, 0, 0, 10, 10);
    CHECK_ITEM(e2, 0, 10, 10, 15);
    CHECK_ITEM(e3, 0, 25, 10, 20);
    CHECK_ITEM(e4, 10, 0, 10, 9); // shorter one

    sma_atlas_destroy(atlas);
}

static void test_pack_uses_free_space()
{
    smol_atlas_t* atlas = sma_atlas_create(64, 64);

    smol_atlas_item_t* e1 = sma_item_add(atlas, 10, 10);
    smol_atlas_item_t* e2 = sma_item_add(atlas, 10, 10);
    smol_atlas_item_t* e3 = sma_item_add(atlas, 10, 10);

    sma_item_remove(atlas, e2);

    smol_atlas_item_t* e4 = sma_item_add(atlas, 10, 10);
    CHECK_ITEM(e4, 10, 0, 10, 10);

    sma_atlas_destroy(atlas);
}

static void test_pack_uses_least_wasteful_free_space()
{
    smol_atlas_t* atlas = sma_atlas_create(64, 64);

    smol_atlas_item_t* e1 = sma_item_add(atlas, 10, 10);
    smol_atlas_item_t* e2 = sma_item_add(atlas, 10, 15);
    smol_atlas_item_t* e3 = sma_item_add(atlas, 10, 20);

    sma_item_remove(atlas, e3);
    sma_item_remove(atlas, e2);
    sma_item_remove(atlas, e1);

    smol_atlas_item_t* e4 = sma_item_add(atlas, 10, 13);
    CHECK_ITEM(e4, 0, 10, 10, 13);

    sma_atlas_destroy(atlas);
}

static void test_pack_makes_new_shelf_if_free_entries_more_wasteful()
{
    smol_atlas_t* atlas = sma_atlas_create(64, 64);

    smol_atlas_item_t* e1 = sma_item_add(atlas, 10, 10);
    smol_atlas_item_t* e2 = sma_item_add(atlas, 10, 15);

    sma_item_remove(atlas, e2);

    smol_atlas_item_t* e3 = sma_item_add(atlas, 10, 10);
    CHECK_ITEM(e3, 10, 0, 10, 10);

    sma_atlas_destroy(atlas);
}

static void test_pack_considers_max_dimensions_for_space_reuse()
{
    smol_atlas_t* atlas = sma_atlas_create(64, 64);

    sma_item_add(atlas, 10, 10);
    smol_atlas_item_t* e2 = sma_item_add(atlas, 10, 15);

    sma_item_remove(atlas, e2);

    smol_atlas_item_t* e3 = sma_item_add(atlas, 10, 13);
    CHECK_ITEM(e3, 0, 10, 10, 13);

    sma_item_remove(atlas, e3);

    smol_atlas_item_t* e4 = sma_item_add(atlas, 10, 14);
    CHECK_ITEM(e4, 0, 10, 10, 14);

    sma_atlas_destroy(atlas);
}

static void test_pack_results_minimal_size()
{
    smol_atlas_t* atlas = sma_atlas_create(30, 45);

    smol_atlas_item_t* res0 = sma_item_add(atlas, 10, 10);
    smol_atlas_item_t* res1 = sma_item_add(atlas, 5, 15);
    smol_atlas_item_t* res2 = sma_item_add(atlas, 25, 15);
    smol_atlas_item_t* res3 = sma_item_add(atlas, 10, 20);

    CHECK_ITEM(res0, 0, 0, 10, 10);
    CHECK_ITEM(res1, 0, 10, 5, 15);
    CHECK_ITEM(res2, 5, 10, 25, 15);
    CHECK_ITEM(res3, 0, 25, 10, 20);

    CHECK_EQ(30, sma_atlas_width(atlas));
    CHECK_EQ(45, sma_atlas_height(atlas));

    sma_atlas_destroy(atlas);
}

static void test_pack_shelf_coalescing()
{
    smol_atlas_t* atlas = sma_atlas_create(100, 10);

    // ABBCDDDD__
    smol_atlas_item_t* rA = sma_item_add(atlas, 10, 10);
    smol_atlas_item_t* rB = sma_item_add(atlas, 20, 10);
    smol_atlas_item_t* rC = sma_item_add(atlas, 10, 10);
    smol_atlas_item_t* rD = sma_item_add(atlas, 40, 10);
    CHECK_ITEM(rA, 0, 0, 10, 10);
    CHECK_ITEM(rB, 10, 0, 20, 10);
    CHECK_ITEM(rC, 30, 0, 10, 10);
    CHECK_ITEM(rD, 40, 0, 40, 10);

    // _BB_DDDD__
    sma_item_remove(atlas, rA);
    sma_item_remove(atlas, rC);
    // ____DDDD__
    sma_item_remove(atlas, rB);
    
    // EEE_DDDD__
    smol_atlas_item_t* rE = sma_item_add(atlas, 30, 10);
    CHECK_ITEM(rE, 0, 0, 30, 10);
    
    // __________
    sma_item_remove(atlas, rD);
    sma_item_remove(atlas, rE);
    
    // FFFFFFFFF_
    smol_atlas_item_t* rF = sma_item_add(atlas, 90, 10);
    CHECK_ITEM(rF, 0, 0, 90, 10);
    
    CHECK_EQ(100, sma_atlas_width(atlas));
    CHECK_EQ(10, sma_atlas_height(atlas));

    sma_atlas_destroy(atlas);
}

static void test_clear()
{
    smol_atlas_t* atlas = sma_atlas_create(10, 10);

    smol_atlas_item_t* e1 = sma_item_add(atlas, 10, 10);
    CHECK_ITEM(e1, 0, 0, 10, 10);

    sma_atlas_clear(atlas);

    smol_atlas_item_t* e2 = sma_item_add(atlas, 10, 10);
    CHECK_ITEM(e2, 0, 0, 10, 10);

    sma_atlas_destroy(atlas);
}

int run_smol_atlas_tests()
{
    printf("Run smol-atlas unit tests...\n");
    test_same_height_on_same_shelf();
    test_larger_height_new_shelf();
    test_shorter_height_existing_best_shelf();
    test_pack_uses_free_space();
    test_pack_uses_least_wasteful_free_space();
    test_pack_makes_new_shelf_if_free_entries_more_wasteful();
    test_pack_considers_max_dimensions_for_space_reuse();
    test_pack_results_minimal_size();
    test_pack_shelf_coalescing();
    test_clear();

    return 0;
}
