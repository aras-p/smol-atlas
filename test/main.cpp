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

    dump_svg_header(f, width < 1000 ? width : 8200, height < 1000 ? height : 8200);
    for (auto it : entries) {
        int key = it.first;
        typename T::Entry& e = it.second;
        int x = atlas.entry_x(e);
        int y = atlas.entry_y(e);
        int w = atlas.entry_width(e);
        int h = atlas.entry_height(e);
        dump_svg_rect(f, x, y, w, h, (key * 12841) & 255, (key * 24571) & 255, (key * 36947) & 255);
    }
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
static void test_atlas_on_data(const char* name, const char* dumpname)
{
    printf("Run %8s on data file: ", name);
    clock_t t0 = clock();
    T atlas;
    
    std::unordered_map<int, int> id_to_timestamp;
    std::unordered_map<int, typename T::Entry> live_entries;
    
    constexpr int TEST_RUN_COUNT = 20;
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
                typename T::Entry res;
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
    size_t entry_total = count_total_entries_size(atlas, live_entries);
    printf("%i (+%i/-%i %i runs): atlas %ix%i (%.1fMpix) used %.1fMpix (%.1f%%), %.1fms\n",
           (int)live_entries.size(), insertions, removals, TEST_RUN_COUNT,
           width, height, width * height / 1.0e6,
           entry_total / 1.0e6,
           entry_total * 100.0 / (width * height),
           dur * 1000.0);
    
    dump_to_svg(atlas, live_entries, dumpname);
}

static int rand_size() { return ((pcg32() & 127) + 1); } // up to 128, quantized at increments of 4

template<typename T>
static void test_atlas_synthetic(const char* name, const char* dumpname)
{
    printf("Run %8s on synthetic: ", name);
    clock_t t0 = clock();
    T atlas;
    
    constexpr int INIT_ENTRY_COUNT = 5000;
    constexpr int LOOP_RUN_COUNT = 50;
    constexpr float LOOP_FRACTION = 0.4f;
    
    pcg_state = 1;

    int insertions = 0;
    int removals = 0;
    int id_counter = 1;
    std::unordered_map<int, typename T::Entry> entries;

    // insert a bunch of initial entries
    for (int i = 0; i < INIT_ENTRY_COUNT; ++i) {
        int w = rand_size();
        int h = rand_size();
        typename T::Entry res = atlas.pack(w, h);
        ++insertions;
        entries.insert({id_counter++, res});
    }
    
    // run removal/insertion loops
    for (int run = 0; run < LOOP_RUN_COUNT; ++run) {
        
        // remove a bunch of entries
        for (auto it = entries.begin(); it != entries.end(); ) {
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
            typename T::Entry res = atlas.pack(w, h);
            ++insertions;
            entries.insert({id_counter++, res});
        }
    }
    
    atlas.shrink();

    clock_t t1 = clock();
    double dur = (t1 - t0) / double(CLOCKS_PER_SEC);
    
    int width = atlas.width();
    int height = atlas.height();
    size_t entry_total = count_total_entries_size(atlas, entries);
    printf("%i (+%i/-%i): atlas %ix%i (%.1fMpix) used %.1fMpix (%.1f%%), %.1fms\n",
           (int)entries.size(), insertions, removals,
           width, height, width * height / 1.0e6,
           entry_total / 1.0e6,
           entry_total * 100.0 / (width * height),
           dur * 1000.0);

    dump_to_svg(atlas, entries, dumpname);
}

#include "../external/mapbox-shelf-pack-cpp/include/mapbox/shelf-pack.hpp"

struct test_on_mapbox
{
    typedef mapbox::Bin* Entry;
    
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
    
    Entry pack(int width, int height) { return m_atlas->packOne(-1, width, height); }
    void release(Entry e) { m_atlas->unref(*e); }
    void shrink() { m_atlas->shrink(); }
    int width() const { return m_atlas->width(); }
    int height() const { return m_atlas->height(); }
    
    int entry_x(const Entry e) const { return e->x; }
    int entry_y(const Entry e) const { return e->y; }
    int entry_width(const Entry e) const { return e->w; }
    int entry_height(const Entry e) const { return e->h; }

    mapbox::ShelfPack* m_atlas;
};

struct test_on_smol
{
    typedef smol_atlas_entry_t* Entry;
    
    test_on_smol()
    {
        m_atlas = sma_create(1024, 1024);
    }
    ~test_on_smol()
    {
        sma_destroy(m_atlas);
    }
    
    Entry pack(int width, int height) { return sma_pack(m_atlas, width, height); }
    void release(Entry e) { sma_entry_release(m_atlas, e); }
    void shrink() { sma_shrink_to_fit(m_atlas); }
    int width() const { return sma_get_width(m_atlas); }
    int height() const { return sma_get_height(m_atlas); }
    
    int entry_x(const Entry e) const { return sma_entry_get_x(e); }
    int entry_y(const Entry e) const { return sma_entry_get_y(e); }
    int entry_width(const Entry e) const { return sma_entry_get_width(e); }
    int entry_height(const Entry e) const { return sma_entry_get_height(e); }

    smol_atlas_t* m_atlas;
};

// -------------------------------------------------------------------

int run_smol_atlas_tests();

int main()
{
    run_smol_atlas_tests();
    
    test_atlas_synthetic<test_on_mapbox>("mapbox", "out_syn_mapbox.svg");
    test_atlas_synthetic<test_on_smol>("smol", "out_syn_smol.svg");

    load_test_data("test/thumbs-gold.txt");
    test_atlas_on_data<test_on_mapbox>("mapbox", "out_data_mapbox.svg");
    test_atlas_on_data<test_on_smol>("smol", "out_data_smol.svg");

    return 0;
}
