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
static void dump_svg_footer(FILE* f, int width, int height, int entries)
{
    fprintf(f, "<rect x=\"10\" y=\"10\" width=\"%i\" height=\"%i\" fill=\"none\" stroke=\"black\" stroke-width=\"5\" />\n", width, height);
    fprintf(f, "<text x=\"10\" y=\"300\" font-family=\"Arial\" font-size=\"280\" fill=\"black\" stroke=\"white\" stroke-width=\"5\">%ix%i %i</text>\n", width, height, entries);
    fprintf(f, "</svg>");
}
static void dump_svg_rect(FILE* f, int x, int y, int width, int height, int r, int g, int b)
{
    fprintf(f, "<rect x=\"%i\" y=\"%i\" width=\"%i\" height=\"%i\" fill=\"rgb(%i,%i,%i)\" />\n",
        x + 10, y + 10, width, height, r, g, b);
}

// -------------------------------------------------------------------

template<typename T>
static void test_atlas_on_data(const char* name, const char* dumpname)
{
    printf("Test %s...\n", name);
    clock_t t0 = clock();
    T atlas;
    
    std::unordered_map<int, int> id_to_timestamp;
    std::unordered_map<int, typename T::Entry*> live_entries;
    
    constexpr int FREE_AFTER_FRAMES = 30;

    int insertions = 0;
    int removals = 0;
    int timestamp = 0;
    for (int run = 0; run < TEST_RUN_COUNT; ++run) {
        for (int frame_idx = 0; frame_idx < s_test_frames.size(); ++frame_idx) {
            size_t frame_start_idx = s_test_frames[frame_idx].first;
            size_t frame_size = s_test_frames[frame_idx].second;

            // process frame data for which entries are visible
            for (size_t test_idx = frame_start_idx; test_idx < frame_start_idx + frame_size; ++test_idx) {
                const TestEntry& test_entry = s_unique_entries[s_test_entries[test_idx]];
                typename T::Entry* res = nullptr;
                auto it = live_entries.find(test_entry.id);
                if (it != live_entries.end()) {
                    res = it->second;
                }
                else {
                    res = atlas.pack(test_entry.width, test_entry.height);
                    ++insertions;
                    live_entries.insert({test_entry.id, res});
                }
                id_to_timestamp[test_entry.id] = timestamp;
            }

            // free entries that were not used for a number of frames
            for (auto it = live_entries.begin(); it != live_entries.end(); ) {
                int key = it->first;
                int frames_ago = timestamp - id_to_timestamp[key];
                if (frames_ago > FREE_AFTER_FRAMES) {
                    atlas.release(it->second);
                    ++removals;
                    it = live_entries.erase(it);
                    id_to_timestamp.erase(key);
                }
                else {
                    ++it;
                }
            }
            ++timestamp;
        }
    }
    atlas.shrink();

    clock_t t1 = clock();
    double dur = (t1 - t0) / double(CLOCKS_PER_SEC);
    
    int width = atlas.width();
    int height = atlas.height();
    printf("- inserted %i removed %i (over %i runs, %i frames): got %ix%i atlas (%.1fKpix), took %.2fms\n",
           insertions, removals, TEST_RUN_COUNT, (int)s_test_frames.size(),
           width, height, width * height / 1000.0, dur * 1000.0);
    
    // write to svg
    FILE* f = fopen(dumpname, "wb");
    if (f)
    {
        dump_svg_header(f, width < 1000 ? width : 4200, height < 1000 ? height : 4200);
        for (auto it : live_entries) {
            int key = it.first;
            typename T::Entry* e = it.second;
            int x = atlas.entry_x(e);
            int y = atlas.entry_y(e);
            int w = atlas.entry_width(e);
            int h = atlas.entry_height(e);
            dump_svg_rect(f, x, y, w, h, (key * 12841) & 255, (key * 24571) & 255, (key * 36947) & 255);
        }
        dump_svg_footer(f, width, height, (int)live_entries.size());
        fclose(f);
    }
}

struct test_on_mapbox
{
    typedef mapbox::Bin Entry;
    
    test_on_mapbox()
    {
        mapbox::ShelfPack::ShelfPackOptions options;
        options.autoResize = true;
        m_atlas = new mapbox::ShelfPack(1024, 1024, options);
    }
    ~test_on_mapbox()
    {
        delete m_atlas;
    }
    
    Entry* pack(int width, int height) { return m_atlas->packOne(-1, width, height); }
    void release(Entry* e) { m_atlas->unref(*e); }
    void shrink() { m_atlas->shrink(); }
    int width() const { return m_atlas->width(); }
    int height() const { return m_atlas->height(); }
    
    int entry_x(const Entry* e) const { return e->x; }
    int entry_y(const Entry* e) const { return e->y; }
    int entry_width(const Entry* e) const { return e->w; }
    int entry_height(const Entry* e) const { return e->h; }

    mapbox::ShelfPack* m_atlas;
};

struct test_on_smol
{
    typedef smol_atlas_entry_t Entry;
    
    test_on_smol()
    {
        m_atlas = sma_create(1024, 1024);
    }
    ~test_on_smol()
    {
        sma_destroy(m_atlas);
    }
    
    Entry* pack(int width, int height) { return sma_pack(m_atlas, width, height); }
    void release(Entry* e) { sma_entry_release(m_atlas, e); }
    void shrink() { sma_shrink_to_fit(m_atlas); }
    int width() const { return sma_get_width(m_atlas); }
    int height() const { return sma_get_height(m_atlas); }
    
    int entry_x(const Entry* e) const { return sma_entry_get_x(e); }
    int entry_y(const Entry* e) const { return sma_entry_get_y(e); }
    int entry_width(const Entry* e) const { return sma_entry_get_width(e); }
    int entry_height(const Entry* e) const { return sma_entry_get_height(e); }

    smol_atlas_t* m_atlas;
};

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
    test_atlas_on_data<test_on_mapbox>("mapbox", "out_mapbox.svg");
    test_atlas_on_data<test_on_smol>("smol", "out_smol.svg");

    return 0;
}
