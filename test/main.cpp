// SPDX-License-Identifier: MIT OR Unlicense
// smol-atlas: https://github.com/aras-p/smol-atlas

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

#include <algorithm>
#include <string>
#include <unordered_map>
#include <vector>

#define TEST_ON_MAPBOX 1
#define TEST_ON_ETAGERE HAVE_ETAGERE
#define TEST_ON_STB_RECTPACK 1
#define TEST_ON_AW_RECTALLOCATOR 1

static constexpr int ATLAS_SIZE_INIT = 1024;
static constexpr int ATLAS_GROW_BY = 512;

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
    s_unique_entries.clear();
    s_entry_map.clear();
    s_test_entries.clear();
    s_test_frames.clear();

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
    printf("Test data '%s': %i frames; %i unique %i total entries\n", filename, int(s_test_frames.size()), int(s_unique_entries.size()), int(s_test_entries.size()));
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
static void dump_to_svg(T& atlas, const std::unordered_map<int, typename T::Entry>& entries, const char* dumpname)
{
    FILE* f = fopen(dumpname, "wb");
    if (!f)
        return;
    
    int width = atlas.width();
    int height = atlas.height();

    dump_svg_header(f, std::max(width, 4096), std::max(height, 4096));
    for (auto it : entries) {
        int key = it.first;
        typename T::Entry& e = it.second;
        int x = atlas.entry_x(e);
        int y = atlas.entry_y(e);
        int w = atlas.entry_width(e);
        int h = atlas.entry_height(e);
        dump_svg_rect(f, x, y, w, h, (key * 12841) & 255, (key * 24571) & 255, (key * 36947) & 255);
    }
    atlas.dump_svg_extra_info(f);
    dump_svg_footer(f, width, height, (int)entries.size());
    fclose(f);
}

template<typename T>
size_t count_total_entries_size(T& atlas, const std::unordered_map<int, typename T::Entry>& entries)
{
    size_t total = 0;
    for (auto it : entries) {
        typename T::Entry& e = it.second;
        int w = atlas.entry_width(e);
        int h = atlas.entry_height(e);
        total += w * h;
    }
    return total;
}

template<typename T>
void grow_atlas_and_repack(T& atlas, std::unordered_map<int, typename T::Entry>& entries, int e_id, int e_width, int e_height)
{
    struct EntryInfo {
        int id;
        int w;
        int h;
    };
    std::vector<EntryInfo> infos;
    infos.reserve(entries.size() + 1);
    for (const auto& kvp : entries) {
        EntryInfo i{kvp.first, atlas.entry_width(kvp.second), atlas.entry_height(kvp.second) };
        infos.emplace_back(i);
    }
    // make sure to include the entry that caused out of space condition in the caller
    infos.emplace_back(EntryInfo{e_id, e_width, e_height});
    
    // sort repacked entries by decreasing height, improves behavior of most (all?) libraries
    std::sort(infos.begin(), infos.end(), [](const EntryInfo& a, const EntryInfo& b) {
        return a.h > b.h;
    });

    int new_width = atlas.width();
    int new_height = atlas.height();

    bool failed = false;
    typename T::Entry ret_res = {};
    while(true) {
        entries.clear();
        
        // try to reinitialize and repack into atlas.
        //
        // important! first try to just repack without changing the atlas
        // size. otherwise algorithms that do not really support item removal
        // will mostly keep on growing even if their current space might be
        // mostly "removed" items.
        //
        // Curiously, this "first just repack" seems to help others too.
        atlas.reinitialize(new_width, new_height);

        failed = false;
        for (const auto& i : infos) {
            typename T::Entry res = atlas.pack(i.w, i.h);
            if (!atlas.entry_valid(res)) {
                failed = true;
                break;
            }
            assert(atlas.entry_valid(res));
            entries.insert({i.id, res});
        }
        if (!failed)
            return;
        
        // Failed packing into current atlas size, increase it.
        if (new_width <= new_height)
            new_width += ATLAS_GROW_BY;
        else
            new_height += ATLAS_GROW_BY;
    }
}

template<typename T>
static void test_atlas_on_data(const char* name, const char* dumpname, int free_after_frames)
{
    printf("Run %8s: ", name);
    clock_t t0 = clock();
    T atlas(ATLAS_SIZE_INIT, ATLAS_SIZE_INIT);

    std::vector<int> id_to_timestamp(s_unique_entries.size(), -free_after_frames);
    std::unordered_map<int, typename T::Entry> live_entries;
    
    constexpr int TEST_RUN_COUNT = 30;

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
                typename T::Entry res;
                auto it = live_entries.find(test_entry.id);
                if (it != live_entries.end()) {
                    res = it->second;
                }
                else {
                    res = atlas.pack(test_entry.width, test_entry.height);
                    ++insertions;
                    if (atlas.entry_valid(res)) {
                        live_entries.insert({test_entry.id, res});
                    }
                    else {
                        grow_atlas_and_repack(atlas, live_entries, test_entry.id, test_entry.width, test_entry.height);
                    }
                }
                id_to_timestamp[test_entry.id] = timestamp;
            }

            // free entries that were not used for a number of frames
            for (auto it = live_entries.begin(); it != live_entries.end(); ) {
                assert(atlas.entry_valid(it->second));
                int key = it->first;
                int frames_ago = timestamp - id_to_timestamp[key];
                if (frames_ago > free_after_frames) {
                    atlas.release(it->second);
                    ++removals;
                    it = live_entries.erase(it);
                    id_to_timestamp[key] = -free_after_frames;
                }
                else {
                    ++it;
                }
            }
            ++timestamp;
        }
    }

    clock_t t1 = clock();
    double dur = (t1 - t0) / double(CLOCKS_PER_SEC);
    
    int width = atlas.width();
    int height = atlas.height();
    size_t entry_total = count_total_entries_size(atlas, live_entries);
    printf("%i (+%i/-%i %i runs): %ix%i (%.1fMpix) used %.1f%%, %.1fms\n",
           (int)live_entries.size(), insertions, removals, TEST_RUN_COUNT,
           width, height, width * height / 1.0e6,
           entry_total * 100.0 / (width * height),
           dur * 1000.0);
    
    dump_to_svg(atlas, live_entries, dumpname);
}

static int rand_size() { return ((pcg32() & 127) + 1); } // up to 128

template<typename T>
static void test_atlas_synthetic(const char* name, const char* dumpname)
{
    printf("Run %8s on synthetic: ", name);
    clock_t t0 = clock();
    T atlas(ATLAS_SIZE_INIT, ATLAS_SIZE_INIT);
    
    constexpr int INIT_ENTRY_COUNT = 2000;
    constexpr int LOOP_RUN_COUNT = 50;
    constexpr float LOOP_FRACTION = 0.3f;
    
    pcg_state = 1;

    int insertions = 0;
    int removals = 0;
    int id_counter = 1;
    std::unordered_map<int, typename T::Entry> entries;

    // insert a bunch of initial entries
    for (int i = 0; i < INIT_ENTRY_COUNT; ++i) {
        int w = rand_size();
        int h = rand_size();
        int id = id_counter++;

        typename T::Entry res = atlas.pack(w, h);
        ++insertions;
        if (atlas.entry_valid(res)) {
            entries.insert({id, res});
        }
        else {
            grow_atlas_and_repack(atlas, entries, id, w, h);
        }
    }
    
    // run removal/insertion loops
    for (int run = 0; run < LOOP_RUN_COUNT; ++run) {
        
        // remove a bunch of entries
        for (auto it = entries.begin(); it != entries.end(); ) {
            assert(atlas.entry_valid(it->second));
            float rnd = (pcg32() & 1023) / 1024.0f;
            if (rnd < LOOP_FRACTION) {
                atlas.release(it->second);
                ++removals;
                it = entries.erase(it);
            }
            else {
                ++it;
            }
        }
        
        // add a bunch of entries
        for (int i = 0; i < INIT_ENTRY_COUNT * LOOP_FRACTION; ++i) {
            int w = rand_size();
            int h = rand_size();
            int id = id_counter++;
            typename T::Entry res = atlas.pack(w, h);
            ++insertions;
            if (atlas.entry_valid(res)) {
                entries.insert({id, res});
            }
            else {
                grow_atlas_and_repack(atlas, entries, id, w, h);
            }
        }
    }
    
    clock_t t1 = clock();
    double dur = (t1 - t0) / double(CLOCKS_PER_SEC);
    
    int width = atlas.width();
    int height = atlas.height();
    size_t entry_total = count_total_entries_size(atlas, entries);
    printf("%i (+%i/-%i): %ix%i (%.1fMpix) used %.1f%%, %.1fms\n",
           (int)entries.size(), insertions, removals,
           width, height, width * height / 1.0e6,
           entry_total * 100.0 / (width * height),
           dur * 1000.0);

    dump_to_svg(atlas, entries, dumpname);
}

// -------------------------------------------------------------------

#include "../external/mapbox-shelf-pack-cpp/include/mapbox/shelf-pack.hpp"

struct test_on_mapbox
{
    typedef mapbox::Bin* Entry;
    
    test_on_mapbox(int width, int height)
    {
        m_atlas = new mapbox::ShelfPack(width, height);
    }
    ~test_on_mapbox()
    {
        delete m_atlas;
    }
    void reinitialize(int width, int height)
    {
        delete m_atlas;
        m_atlas = new mapbox::ShelfPack(width, height);
    }
    
    Entry pack(int width, int height) { return m_atlas->packOne(-1, width, height); }
    void release(Entry& e) { m_atlas->unref(*e); }
    int width() const { return m_atlas->width(); }
    int height() const { return m_atlas->height(); }

    bool entry_valid(const Entry& e) const { return e != nullptr; }
    int entry_x(const Entry& e) const { return e->x; }
    int entry_y(const Entry& e) const { return e->y; }
    int entry_width(const Entry& e) const { return e->w; }
    int entry_height(const Entry& e) const { return e->h; }
    
    void dump_svg_extra_info(FILE* f)
    {
    }

    mapbox::ShelfPack* m_atlas;
};

#define STB_RECT_PACK_IMPLEMENTATION
#include "../external/stb_rect_pack.h"

struct test_on_stb_rectpack
{
    struct Entry {
        int x, y;
        int w, h;
        bool valid;
    };

    test_on_stb_rectpack(int width, int height)
        : m_context(), m_width(width), m_height(height), m_nodes(width)
    {
        stbrp_init_target(&m_context, width, height, m_nodes.data(), (int)m_nodes.size());
    }
    ~test_on_stb_rectpack()
    {
    }
    void reinitialize(int width, int height)
    {
        //m_removed.clear();
        m_nodes.resize(width);
        m_width = width;
        m_height = height;
        stbrp_init_target(&m_context, width, height, m_nodes.data(), (int)m_nodes.size());
    }

    Entry pack(int width, int height)
    {
        stbrp_rect r;
        r.id = 0;
        r.w = width;
        r.h = height;
        r.x = 0;
        r.y = 0;
        r.was_packed = 0;
        stbrp_pack_rects(&m_context, &r, 1);

        Entry e;
        e.x = r.x;
        e.y = r.y;
        e.w = width;
        e.h = height;
        e.valid = r.was_packed != 0;
        return e;
    }
    void release(Entry& e)
    {
        //m_removed.push_back(e);
    }
    int width() const { return m_width; }
    int height() const { return m_height; }

    bool entry_valid(const Entry& e) const { return e.valid; }
    int entry_x(const Entry& e) const { return e.x; }
    int entry_y(const Entry& e) const { return e.y; }
    int entry_width(const Entry& e) const { return e.w; }
    int entry_height(const Entry& e) const { return e.h; }
    
    void dump_svg_extra_info(FILE* f)
    {
        //for (auto e : m_removed) {
        //    dump_svg_rect(f, e.x + e.w/8, e.y + e.h/8, e.w-e.w/4, e.h-e.h/4, 230, 230, 230);
        //}
    }
    
    stbrp_context m_context;
    int m_width, m_height;
    std::vector<stbrp_node> m_nodes;
    //std::vector<Entry> m_removed;
};

#include "../external/andrewwillmott_rectallocator/RectAllocator.h"

struct test_on_aw_rectallocator
{
    struct Entry {
        nHL::tRectRef handle;
        int x, y;
        int w, h;
    };

    test_on_aw_rectallocator(int width, int height)
        : m_atlas({width, height}), m_width(width), m_height(height)
    {
    }
    ~test_on_aw_rectallocator()
    {
    }
    void reinitialize(int width, int height)
    {
        m_atlas.Clear({width, height});
        m_width = width;
        m_height = height;
    }

    Entry pack(int width, int height)
    {
        Entry e;
        e.handle = m_atlas.Alloc({width, height});
        if (e.handle != nHL::kNullRectRef) {
            const nHL::cRectInfo& info = m_atlas.RectInfo(e.handle);
            e.x = info.mOrigin.x;
            e.y = info.mOrigin.y;
            e.w = width;
            e.h = height;
        }
        return e;
    }
    void release(Entry& e) { m_atlas.Free(e.handle); }
    int width() const { return m_width; }
    int height() const { return m_height; }

    bool entry_valid(const Entry& e) const { return e.handle != nHL::kNullRectRef; }
    int entry_x(const Entry& e) const { return e.x; }
    int entry_y(const Entry& e) const { return e.y; }
    int entry_width(const Entry& e) const { return e.w; }
    int entry_height(const Entry& e) const { return e.h; }
    
    void dump_svg_extra_info(FILE* f)
    {
    }

    nHL::cRectAllocator m_atlas;
    int m_width, m_height;
};

#if HAVE_ETAGERE

#include "../external/etagere/etagere.h"

struct test_on_etagere
{
    struct Entry {
        EtagereAllocation e;
        int w;
        int h;
        bool valid;
    };

    test_on_etagere(int width, int height)
    {
        m_width = width;
        m_height = height;
        m_atlas = etagere_atlas_allocator_new(width, height);
    }
    ~test_on_etagere()
    {
        etagere_atlas_allocator_delete(m_atlas);
    }
    void reinitialize(int width, int height)
    {
        etagere_atlas_allocator_delete(m_atlas);
        m_width = width;
        m_height = height;
        m_atlas = etagere_atlas_allocator_new(width, height);
    }
    
    Entry pack(int width, int height)
    {
        Entry res;
        EtagereStatus status = etagere_atlas_allocator_allocate(m_atlas, width, height, &res.e);
        res.w = width;
        res.h = height;
        res.valid = status != 0;
        return res;
    }
    void release(Entry& e) { etagere_atlas_allocator_deallocate(m_atlas, e.e.id); }
    int width() const { return m_width; }
    int height() const { return m_height; }
    
    bool entry_valid(const Entry& e) const { return e.valid; }
    int entry_x(const Entry& e) const { return e.e.rectangle.min_x; }
    int entry_y(const Entry& e) const { return e.e.rectangle.min_y; }
    int entry_width(const Entry& e) const { return e.w; }
    int entry_height(const Entry& e) const { return e.h; }
    
    void dump_svg_extra_info(FILE* f)
    {
    }

    EtagereAtlasAllocator* m_atlas;
    int m_width;
    int m_height;
};

#endif // #if HAVE_ETAGERE

#include "../src/smol-atlas.h"

struct test_on_smol
{
    typedef smol_atlas_item_t* Entry;
    
    test_on_smol(int width, int height)
    {
        m_atlas = sma_atlas_create(width, height);
    }
    ~test_on_smol()
    {
        sma_atlas_destroy(m_atlas);
    }
    void reinitialize(int width, int height)
    {
        sma_atlas_destroy(m_atlas);
        m_atlas = sma_atlas_create(width, height);
    }
    
    Entry pack(int width, int height) { return sma_item_add(m_atlas, width, height); }
    void release(Entry& e) { sma_item_remove(m_atlas, e); }
    int width() const { return sma_atlas_width(m_atlas); }
    int height() const { return sma_atlas_height(m_atlas); }
    
    bool entry_valid(const Entry& e) const { return e != nullptr; }
    int entry_x(const Entry& e) const { return sma_item_x(e); }
    int entry_y(const Entry& e) const { return sma_item_y(e); }
    int entry_width(const Entry& e) const { return sma_item_width(e); }
    int entry_height(const Entry& e) const { return sma_item_height(e); }

    void dump_svg_extra_info(FILE* f)
    {
    }

    smol_atlas_t* m_atlas;
};

// -------------------------------------------------------------------

int run_smol_atlas_tests();

int main()
{
    run_smol_atlas_tests();
    
    #if TEST_ON_MAPBOX
    test_atlas_synthetic<test_on_mapbox>("mapbox", "out_syn_mapbox.svg");
    #endif
    #if TEST_ON_ETAGERE
    test_atlas_synthetic<test_on_etagere>("etagere", "out_syn_etagere.svg");
    #endif
    #if TEST_ON_STB_RECTPACK
    test_atlas_synthetic<test_on_stb_rectpack>("rectpack", "out_syn_rectpack.svg");
    #endif
    #if TEST_ON_AW_RECTALLOCATOR
    test_atlas_synthetic<test_on_aw_rectallocator>("awralloc", "out_syn_awralloc.svg");
    #endif
    test_atlas_synthetic<test_on_smol>("smol", "out_syn_smol.svg");

    load_test_data("test/thumbs-gold.txt");
    const int free_frames_gold = 30;
    #if TEST_ON_MAPBOX
    test_atlas_on_data<test_on_mapbox>("mapbox", "out_data_gold_mapbox.svg", free_frames_gold);
    #endif
    #if TEST_ON_ETAGERE
    test_atlas_on_data<test_on_etagere>("etagere", "out_data_gold_etagere.svg", free_frames_gold);
    #endif
    #if TEST_ON_STB_RECTPACK
    test_atlas_on_data<test_on_stb_rectpack>("rectpack", "out_data_gold_rectpack.svg", free_frames_gold);
    #endif
    #if TEST_ON_AW_RECTALLOCATOR
    test_atlas_on_data<test_on_aw_rectallocator>("awralloc", "out_data_gold_awralloc.svg", free_frames_gold);
    #endif
    test_atlas_on_data<test_on_smol>("smol", "out_data_gold_smol.svg", free_frames_gold);

    load_test_data("test/thumbs-wingit.txt");
    const int free_frames_wingit = 40;
    #if TEST_ON_MAPBOX
    test_atlas_on_data<test_on_mapbox>("mapbox", "out_data_wingit_mapbox.svg", free_frames_wingit);
    #endif
    #if TEST_ON_ETAGERE
    test_atlas_on_data<test_on_etagere>("etagere", "out_data_wingit_etagere.svg", free_frames_wingit);
    #endif
    #if TEST_ON_STB_RECTPACK
    test_atlas_on_data<test_on_stb_rectpack>("rectpack", "out_data_wingit_rectpack.svg", free_frames_wingit);
    #endif
    #if TEST_ON_AW_RECTALLOCATOR
    test_atlas_on_data<test_on_aw_rectallocator>("awralloc", "out_data_wingit_awralloc.svg", free_frames_wingit);
    #endif
    test_atlas_on_data<test_on_smol>("smol", "out_data_wingit_smol.svg", free_frames_wingit);

    load_test_data("test/thumbs-sprite-fright.txt");
    const int free_frames_sprite = 5;
    #if TEST_ON_MAPBOX
    test_atlas_on_data<test_on_mapbox>("mapbox", "out_data_spritefright_mapbox.svg", free_frames_sprite);
    #endif
    #if TEST_ON_ETAGERE
    test_atlas_on_data<test_on_etagere>("etagere", "out_data_spritefright_etagere.svg", free_frames_sprite);
    #endif
    #if TEST_ON_STB_RECTPACK
    test_atlas_on_data<test_on_stb_rectpack>("rectpack", "out_data_spritefright_rectpack.svg", free_frames_sprite);
    #endif
    #if TEST_ON_AW_RECTALLOCATOR
    test_atlas_on_data<test_on_aw_rectallocator>("awralloc", "out_data_spritefright_awralloc.svg", free_frames_sprite);
    #endif
    test_atlas_on_data<test_on_smol>("smol", "out_data_spritefright_smol.svg", free_frames_sprite);

    return 0;
}
