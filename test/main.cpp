#include "../src/smol-atlas.h"
#include "../external/mapbox-shelf-pack-cpp/include/mapbox/shelf-pack.hpp"

#include <stdint.h>
#include <stdio.h>
#include <time.h>

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// -------------------------------------------------------------------

// https://www.reedbeta.com/blog/hash-functions-for-gpu-rendering/
static uint32_t pcg_state;

static uint32_t pcg32()
{
    uint32_t state = pcg_state;
    pcg_state = pcg_state * 747796405u + 2891336453u;
    uint32_t word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

// -------------------------------------------------------------------

static constexpr int TEST_RUN_COUNT = 10;

struct TestEntry {
    int id;
    int width;
    int height;
};
static std::vector<TestEntry> s_unique_entries;
static std::unordered_map<std::string, int> s_entry_map;
static std::vector<int> s_test_entries;
static std::vector<std::pair<int, int>> s_test_frames;

static void load_test_data(const char* filename)
{
    FILE* f = fopen(filename, "rt");
    if (!f) {
        printf("ERROR: could not open test file '%s'\n", filename);
        exit(1);
    }
    char buf[1000];
    int frame_start_idx = -1;
    while (fgets(buf, sizeof(buf)-1, f) != NULL) {
        int frame, width, height, crop, minx, miny, maxx, maxy;
        int64_t ptr;


        if (sscanf(buf, "FRAME %i", &frame) == 1) {
            if (frame_start_idx >= 0)
                s_test_frames.push_back(std::make_pair(frame_start_idx, (int)s_test_entries.size() - frame_start_idx));
            frame_start_idx = (int)s_test_entries.size();
        }
        else if (sscanf(buf, "img %lli %i %i crop %i %i %i %i %i", &ptr, &width, &height, &crop, &minx, &miny, &maxx, &maxy) == 8) {
            std::string strbuf = buf;
            int id;
            auto it = s_entry_map.find(strbuf);
            if (it == s_entry_map.end()) {
                id = (int)s_entry_map.size();
                s_entry_map.insert(std::make_pair(strbuf, id));
                s_unique_entries.push_back({id, maxx+1, maxy+1});
            }
            else {
                id = it->second;
            }
            s_test_entries.push_back(id);
        }
        else {
            break;
        }
    }
    if (frame_start_idx >= 0)
        s_test_frames.push_back(std::make_pair(frame_start_idx, (int)s_test_entries.size() - frame_start_idx));
    fclose(f);
    printf("- test file '%s': %i frames, %i unique entries, %i total entries\n", filename, int(s_test_frames.size()), int(s_unique_entries.size()), int(s_test_entries.size()));
}


// -------------------------------------------------------------------

static void dump_svg_header(FILE* f, int width, int height)
{
    fprintf(f, "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 %i %i\">\n", width + 20, height + 20);
}
static void dump_svg_footer(FILE* f, int width, int height)
{
    fprintf(f, "<rect x=\"10\" y=\"10\" width=\"%i\" height=\"%i\" fill=\"none\" stroke=\"black\" stroke-width=\"5\" />\n", width, height);
    fprintf(f, "<text x=\"10\" y=\"500\" font-family=\"Arial\" font-size=\"450\" fill=\"black\" stroke=\"white\" stroke-width=\"5\">%i x %i</text>\n", width, height);
    fprintf(f, "</svg>");
}
static void dump_svg_rect(FILE* f, int x, int y, int width, int height, int r, int g, int b)
{
    fprintf(f, "<rect x=\"%i\" y=\"%i\" width=\"%i\" height=\"%i\" fill=\"rgb(%i,%i,%i)\" />\n",
        x + 10, y + 10, width, height, r, g, b);
}

// -------------------------------------------------------------------

static void dump_mapbox_atlas_to_svg(const char* filename, int width, int height, const std::unordered_set<mapbox::Bin*>& bins)
{
    using namespace mapbox;
    pcg_state = 3;
    FILE* f = fopen(filename, "wb");
    if (!f)
        return;
    dump_svg_header(f, width < 1000 ? width : 8192, height < 1000 ? height : 8192);
    for (const Bin* bin : bins) {
        dump_svg_rect(f, bin->x, bin->y, bin->w, bin->h, (bin->id * 12841) & 255, (bin->id * 24571) & 255, (bin->id * 36947) & 255);
    }
    dump_svg_footer(f, width, height);
    fclose(f);
}

static void test_mapbox()
{
    using namespace mapbox;
    printf("Test mapbox_shelf_pack_cpp...\n");

    clock_t t0 = clock();

    ShelfPack::ShelfPackOptions options;
    options.autoResize = true;
    ShelfPack atlas(1024, 1024, options);

    std::unordered_set<Bin*> bins_cur, bins_prev, bins_live;
    int max_w = 0, max_h = 0, max_frame = -1;

    for (int run = 0; run < TEST_RUN_COUNT; ++run) {
        for (int frame_idx = 0; frame_idx < s_test_frames.size(); ++frame_idx) {
            size_t frame_start_idx = s_test_frames[frame_idx].first;
            size_t frame_size = s_test_frames[frame_idx].second;

            bins_cur.clear();
            for (size_t test_idx = frame_start_idx; test_idx < frame_start_idx + frame_size; ++test_idx) {
                const TestEntry& entry = s_unique_entries[s_test_entries[test_idx]];
                Bin* res = atlas.packOne(entry.id, entry.width, entry.height);
                if (!res) {
                    printf("ERROR: out of space\n");
                    exit(1);
                }
                bins_live.insert(res);
                bins_cur.insert(res);
                bins_prev.erase(res);
            }

            // free bins that were from previous frame, and unused in this frame
            for (Bin* bin : bins_prev) {
                int rc = atlas.unref(*bin);
                if (rc == 0)
                    bins_live.erase(bin);
            }
            bins_prev.swap(bins_cur);
            atlas.shrink();
            int width = atlas.width();
            int height = atlas.height();
            if (width > max_w) {
                max_w = width;
                max_frame = frame_idx;
            }
            if (height > max_h) {
                max_h = height;
                max_frame = frame_idx;
            }
            
            if (frame_idx == 50 && run == 0) {
                dump_mapbox_atlas_to_svg("res_mapbox_1.svg", max_w, max_h, bins_live);
            }
            if (frame_idx == 232 && run == TEST_RUN_COUNT - 1) {
                dump_mapbox_atlas_to_svg("res_mapbox_232.svg", max_w, max_h, bins_live);
            }
        }
    }
    
    clock_t t1 = clock();
    double dur = (t1 - t0) / double(CLOCKS_PER_SEC);
    printf("- packed total %zi, got %ix%i atlas (max frame %i), took %.2fms\n", s_test_entries.size(), max_w, max_h, max_frame, dur * 1000.0);
    dump_mapbox_atlas_to_svg("res_mapbox_end.svg", max_w, max_h, bins_live);

    // now put all test entries and dump that
    for (size_t test_idx = 0; test_idx < s_test_entries.size(); ++test_idx) {
        const TestEntry& entry = s_unique_entries[s_test_entries[test_idx]];
        Bin* res = atlas.packOne(entry.id, entry.width, entry.height);
        if (!res) {
            printf("ERROR: out of space\n");
            exit(1);
        }
        bins_cur.insert(res);
        bins_prev.erase(res);
        bins_live.insert(res);
    }
    bins_prev.swap(bins_cur);
    atlas.shrink();
    printf("- packed whole %zi, got %ix%i atlas\n", s_test_entries.size(), atlas.width(), atlas.height());
    dump_mapbox_atlas_to_svg("res_mapbox_whole.svg", atlas.width(), atlas.height(), bins_live);
}

// -------------------------------------------------------------------

static FILE* s_svg_file;

static void svg_emit_shelf(int y, int height, int width)
{
    //fprintf(s_svg_file, "<line x1=\"%i\" y1=\"%i\" x2=\"%i\" y2=\"%i\" stroke=\"rgb(0,0,0)\" stroke-width=\"5\" />\n",
    //        10, y + height + 10, width + 10, y + height + 10);
}
static void svg_emit_free_span(int y, int height, int x, int width)
{
    //fprintf(s_svg_file, "<rect x=\"%i\" y=\"%i\" width=\"%i\" height=\"%i\" fill=\"none\" stroke=\"gray\" stroke-width=\"2\" />\n",
    //        x + 10, y + height/3 + 10, width, height / 3);
}
static void svg_emit_entry(const smol_atlas_entry_t* e)
{
    dump_svg_rect(
        s_svg_file,
        sma_entry_get_x(e), sma_entry_get_y(e), sma_entry_get_width(e), sma_entry_get_height(e),
        pcg32() & 255, pcg32() & 255, pcg32() & 255);
}

static void dump_smol_atlas_to_svg(const char* filename, const smol_atlas_t* atlas)
{
    pcg_state = 3;
    s_svg_file = fopen(filename, "wb");
    if (!s_svg_file)
        return;
    
    const int width = sma_get_width(atlas);
    const int height = sma_get_height(atlas);
    dump_svg_header(s_svg_file, width < 1000 ? width : 8192, height < 1000 ? height : 8192);
    sma_debug_dump(atlas, svg_emit_shelf, svg_emit_free_span, svg_emit_entry);
    dump_svg_footer(s_svg_file, width, height);
    fclose(s_svg_file);
    s_svg_file = nullptr;
}

static void test_smol()
{
    printf("Test smol-atlas...\n");

    clock_t t0 = clock();

    smol_atlas_t* atlas = sma_create(1024, 1024);

    std::unordered_map<int, smol_atlas_entry_t*> entries_cur, entries_prev, entries_live;
    std::unordered_map<int, int> entries_refcount;
    int max_w = 0, max_h = 0, max_frame = -1;

    for (int run = 0; run < TEST_RUN_COUNT; ++run) {
        for (int frame_idx = 0; frame_idx < s_test_frames.size(); ++frame_idx) {
            size_t frame_start_idx = s_test_frames[frame_idx].first;
            size_t frame_size = s_test_frames[frame_idx].second;

            entries_cur.clear();
            for (size_t test_idx = frame_start_idx; test_idx < frame_start_idx + frame_size; ++test_idx) {
                const TestEntry& entry = s_unique_entries[s_test_entries[test_idx]];
                smol_atlas_entry_t* res;
                auto it = entries_live.find(entry.id);
                if (it != entries_live.end()) {
                    res = it->second;
                }
                else {
                    res = sma_pack(atlas, entry.width, entry.height);
                    entries_live.insert({entry.id, res});
                }
                entries_refcount[entry.id]++;
                if (!res) {
                    printf("ERROR: out of space\n");
                    exit(1);
                }
                entries_cur.insert({entry.id, res});
                entries_prev.erase(entry.id);
            }

            // free entries that were from previous frame, and unused in this frame
            for (auto& kvp : entries_prev) {
                int rc = --entries_refcount[kvp.first];
                if (rc == 0) {
                    sma_entry_release(atlas, kvp.second);
                    entries_live.erase(kvp.first);
                }
            }
            entries_prev.swap(entries_cur);
            sma_shrink_to_fit(atlas);
            int width = sma_get_width(atlas);
            int height = sma_get_height(atlas);
            if (width > max_w) {
                max_w = width;
                max_frame = frame_idx;
            }
            if (height > max_h) {
                max_h = height;
                max_frame = frame_idx;
            }

            if (frame_idx == 50 && run == 0) {
                dump_smol_atlas_to_svg("res_smol_1.svg", atlas);
            }
            if (frame_idx == 232 && run == TEST_RUN_COUNT - 1) {
                dump_smol_atlas_to_svg("res_smol_232.svg", atlas);
            }
        }
    }

    clock_t t1 = clock();
    double dur = (t1 - t0) / double(CLOCKS_PER_SEC);
    printf("- packed total %zi, got %ix%i atlas (max frame %i), took %.2fms\n", s_test_entries.size(), max_w, max_h, max_frame, dur * 1000.0);
    dump_smol_atlas_to_svg("res_smol_end.svg", atlas);

    // now put all test entries and dump that
    for (size_t test_idx = 0; test_idx < s_test_entries.size(); ++test_idx) {
        const TestEntry& entry = s_unique_entries[s_test_entries[test_idx]];

        smol_atlas_entry_t* res;
        auto it = entries_live.find(entry.id);
        if (it != entries_live.end()) {
            res = it->second;
        }
        else {
            res = sma_pack(atlas, entry.width, entry.height);
            entries_live.insert({entry.id, res});
        }
        if (!res) {
            printf("ERROR: out of space\n");
            exit(1);
        }
    }
    sma_shrink_to_fit(atlas);
    printf("- packed whole %zi, got %ix%i atlas\n", s_test_entries.size(), sma_get_width(atlas), sma_get_height(atlas));
    dump_smol_atlas_to_svg("res_smol_whole.svg", atlas);

    sma_destroy(atlas);
}

// -------------------------------------------------------------------

#if 0
static int rand_size() { return (pcg32() & 63) * 4 + 3; }

static void benchmark_mapbox()
{
    std::cout << "Bench mapbox" << std::endl;
    ShelfPack::ShelfPackOptions options;
    options.autoResize = true;

    const int count = 20000;
    std::vector<Bin*> bins;
    bins.reserve(count);


    // pack initial thumbs
    // Mac M1 Max O2: 3.1ms
    // Win Ryzen 5950X /O2: 4.0ms
    pcg_state = 1;
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
    printf("- packed      %i, got %ix%i atlas, took %.2fms\n", count, sprite.width(), sprite.height(), dur*1000.0);
    dump_atlas_to_svg("res_mapbox1.svg", sprite.width(), sprite.height(), bins.size(), bins.data());

    assert(sprite.width() == 32639);
    assert(sprite.height() == 27989);

    // remove half of thumbs
    for (int i = 0; i < bins.size(); i += 2)
    {
        sprite.unref(*bins[i]);
    }

    // pack half of thumbs again
    // Mac M1 Max O2: 174.1ms
    // Win Ryzen 5950X /O2: 136.0ms
    pcg_state = 2;
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
    printf("- packed half %i, got %ix%i atlas, took %.2fms\n", count/2, sprite.width(), sprite.height(), dur*1000.0);
    dump_atlas_to_svg("res_mapbox2.svg", sprite.width(), sprite.height(), bins.size(), bins.data());
    assert(sprite.width() == 32639);
    assert(sprite.height() == 27989);

    // pack a bunch, removing one and adding one
    // Win Ryzen 5950X /O2: 3230.0ms
    pcg_state = 3;
    t0 = std::clock();
    for (int i = 0; i < count * 10; ++i)
    {
        int idx = pcg32() % count;
        sprite.unref(*bins[idx]);
        int w = rand_size();
        int h = rand_size();
        Bin *res = sprite.packOne(-1, w, h);
        if (!res) throw std::runtime_error("out of space");
        bins[idx] = res;
    }
    t1 = std::clock();
    dur = (t1 - t0) / (double) CLOCKS_PER_SEC;
    printf("- packed rand %i, got %ix%i atlas, took %.2fms\n", count * 10, sprite.width(), sprite.height(), dur*1000.0);
    dump_atlas_to_svg("res_mapbox3.svg", sprite.width(), sprite.height(), bins.size(), bins.data());
    assert(sprite.width() == 32639);
    assert(sprite.height() == 27989);
}
#endif

// -------------------------------------------------------------------

int run_smol_atlas_tests();

int main()
{
    run_smol_atlas_tests();    

    //benchmark_mapbox();
    load_test_data("test/thumbs-gold.txt");

    test_mapbox();
    test_smol();

    return 0;
}
