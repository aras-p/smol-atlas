// smol-atlas: https://github.com/aras-p/smol-atlas

#include "smol-atlas.h"

#include <assert.h>
#include <memory>
#include <vector>

static inline int max_i(int a, int b)
{
    return a > b ? a : b;
}

struct smol_atlas_entry_t
{
    explicit smol_atlas_entry_t(int x_, int y_, int w_, int h_, int shelf_)
    : x(x_), y(y_), width(w_), height(h_), shelf_index(shelf_)
    {
    }

    int x;
    int y;
    int width;
    int height;
    int shelf_index;
};

struct smol_free_span_t
{
    explicit smol_free_span_t(int x_, int w_) : x(x_), width(w_), next(nullptr) {}
    
    int x;
    int width;
    smol_free_span_t* next;
};

struct smol_shelf_t
{
    explicit smol_shelf_t(int y, int width, int height, int index)
        : m_y(y), m_width(width), m_height(height), m_index(index)
    {
        m_free_spans = new smol_free_span_t(0, width);
    }
    ~smol_shelf_t()
    {
        // release the entries
        for (smol_atlas_entry_t* e : m_entries) {
            delete e;
        }

        // release the spans list
        smol_free_span_t* span = m_free_spans;
        while (span) {
            smol_free_span_t* ptr = span;
            span = span->next;
            delete ptr;
        }
    }

    int calc_max_free_span() const
    {
        smol_free_span_t* it = m_free_spans;
        int width = 0;
        while (it != nullptr) {
            width = max_i(width, it->width);
            it = it->next;
        }
        return width;
    }

    smol_atlas_entry_t* alloc_entry(int w, int h)
    {
        if (h > m_height)
            return nullptr;

        // find a suitable free span
        smol_free_span_t* it = m_free_spans;
        smol_free_span_t* prev = nullptr;
        while (it != nullptr) {
            if (it->width >= w) {
                break;
            }
            prev = it;
            it = it->next;
        }

        // no space in this shelf
        if (it == nullptr)
            return nullptr;

        const int x = it->x;
        const int rest = it->width - w;
        if (rest > 0) {
            // there will be still space left in this span, adjust
            it->x += w;
            it->width -= w;
        }
        else {
            // whole span is taken, remove it
            free_spans_remove(prev, it);
        }

        smol_atlas_entry_t* e = new smol_atlas_entry_t(x, m_y, w, h, m_index);
        m_entries.emplace_back(e);
        return e;
    }

    void add_free_span(int x, int width)
    {
        // insert into free spans list at the right position
        smol_free_span_t* free_e = new smol_free_span_t(x, width);
        smol_free_span_t* it = m_free_spans;
        smol_free_span_t* prev = nullptr;
        bool added = false;
        while (it != nullptr) {
            if (free_e->x < it->x) {
                // found right place, insert into the list
                free_span_insert(prev, free_e);
                added = true;
                break;
            }
            prev = it;
            it = it->next;
        }
        if (!added)
            free_span_insert(prev, free_e);

        merge_free_spans(prev, free_e);
    }

    void free_entry(smol_atlas_entry_t* e)
    {
        assert(e);
        assert(e->shelf_index == m_index);
        assert(e->y == m_y);
        add_free_span(e->x, e->width);

        // remove the actual entry
        auto it = std::find(m_entries.begin(), m_entries.end(), e);
        assert(it != m_entries.end());
        size_t index = std::distance(m_entries.begin(), it);
        if (index < m_entries.size() - 1) {
            m_entries[index] = m_entries.back();
        }
        m_entries.pop_back();
        delete e;
    }

    void free_span_insert(smol_free_span_t* prev, smol_free_span_t* span)
    {
        if (prev == nullptr) { // this is first node
            span->next = m_free_spans;
            m_free_spans = span;
        }
        else {
            span->next = prev->next;
            prev->next = span;
        }
    }

    void free_spans_remove(smol_free_span_t* prev, smol_free_span_t* span)
    {
        if (prev)
            prev->next = span->next;
        else
            m_free_spans = span->next;
        delete span;
    }

    void merge_free_spans(smol_free_span_t* prev, smol_free_span_t* span)
    {
        smol_free_span_t* next = span->next;
        if (next != nullptr && span->x + span->width == next->x) {
            // merge with next
            span->width += next->width;
            free_spans_remove(span, next);
        }
        if (prev != nullptr && prev->x + prev->width == span->x) {
            // merge with prev
            prev->width += span->width;
            free_spans_remove(prev, span);
        }
    }

    void resize(int w)
    {
        if (w == m_width)
            return;

        if (w > m_width) {
            add_free_span(m_width, w - m_width);
        }
        else {
            // remove or adjust any free spans beyond new width
            smol_free_span_t* it = m_free_spans;
            smol_free_span_t* prev = nullptr;
            while (it != nullptr) {
                smol_free_span_t* next = it->next;
                if (it->x >= w) {
                    free_spans_remove(prev, it);
                    it = next;
                }
                else {
                    if (it->x + it->width > w) {
                        it->width = w - it->x;
                    }
                    prev = it;
                    it = next;
                }
            }

        }

        m_width = w;
    }

    const int m_y;
    int m_width;
    const int m_height;
    const int m_index;

    std::vector<smol_atlas_entry_t*> m_entries;
    smol_free_span_t* m_free_spans;
};


struct smol_atlas_t
{
    explicit smol_atlas_t(int w, int h, bool grow)
    {
        m_width = w > 0 ? w : 64;
        m_height = h > 0 ? h : 64;
        m_auto_grow = grow;
    }

    smol_atlas_entry_t* pack(int w, int h)
    {
        // find best shelf
        smol_shelf_t* best_shelf = nullptr;
        int best_score = 0x7fffffff;

        int top_y = 0;
        for (auto& shelf : m_shelves) {
            const int shelf_h = shelf->m_height;
            top_y = max_i(top_y, shelf->m_y + shelf_h);

            if (shelf_h < h)
                continue; // too short

            if (shelf->calc_max_free_span() < w)
                continue; // does not have enough space

            if (shelf_h == h) // exact height fit, use it
                return shelf->alloc_entry(w, h);

            // otherwise the shelf is too tall, track best one
            int score = shelf_h - h;
            if (score < best_score) {
                best_score = score;
                best_shelf = shelf.get();
            }
        }

        if (best_shelf)
            return best_shelf->alloc_entry(w, h);

        // no shelf with enough space: add a new shelf
        if (w <= m_width && h <= m_height - top_y) {
            int shelf_index = int(m_shelves.size());
            m_shelves.emplace_back(std::make_unique<smol_shelf_t>(top_y, m_width, h, shelf_index));
            return m_shelves.back()->alloc_entry(w, h);
        }

        // if we can't grow, fail packing now
        if (!m_auto_grow)
            return nullptr;

        // have to grow the atlas:
        // - double the smaller dimension (if equal, grow width)
        // - make sure to accomodate the pack request
        int new_w = m_width;
        if (m_width <= m_height || w > m_width) {
            new_w = max_i(w, m_width) * 2;
        }
        int new_h = m_height;
        if (m_height < m_width || h > m_height) {
            new_h = max_i(h, m_height) * 2;
        }

        // resize and retry the packing
        resize(new_w, new_h);
        return pack(w, h);
    }

    void shrink_to_fit()
    {
        if (m_shelves.empty())
            return;

        int new_w = 0;
        int new_h = 0;

        for (const auto& shelf : m_shelves) {
            new_h = max_i(new_h, shelf->m_y + shelf->m_height);
            for (const auto& entry : shelf->m_entries) {
                new_w = max_i(new_w, entry->x + entry->width);
            }
        }

        resize(new_w, new_h);
    }

    void free_entry(smol_atlas_entry_t* entry)
    {
        assert(entry->shelf_index >= 0 && entry->shelf_index < m_shelves.size());
        m_shelves[entry->shelf_index]->free_entry(entry);
    }

    void clear()
    {
        m_shelves.clear();
    }

    void resize(int w, int h)
    {
        if (w == m_width && h == m_height)
            return;
        m_width = w;
        m_height = h;
        for (auto& shelf : m_shelves) {
            shelf->resize(m_width);
        }
    }

    std::vector<std::unique_ptr<smol_shelf_t>> m_shelves;
    int m_width;
    int m_height;
    bool m_auto_grow;
};

smol_atlas_t* sma_create(int init_width, int init_height, bool auto_grow)
{
    return new smol_atlas_t(init_width, init_height, auto_grow);
}

void sma_destroy(smol_atlas_t* atlas)
{
    delete atlas;
}

int sma_get_width(const smol_atlas_t* atlas)
{
    return atlas->m_width;
}

int sma_get_height(const smol_atlas_t* atlas)
{
    return atlas->m_height;
}

smol_atlas_entry_t* sma_pack(smol_atlas_t* atlas, int width, int height)
{
    return atlas->pack(width, height);
}

void sma_entry_release(smol_atlas_t* atlas, smol_atlas_entry_t* entry)
{
    atlas->free_entry(entry);
}

void sma_clear(smol_atlas_t* atlas)
{
    atlas->clear();
}

void sma_shrink_to_fit(smol_atlas_t* atlas)
{
    atlas->shrink_to_fit();
}

void sma_resize(smol_atlas_t* atlas, int width, int height)
{
    atlas->resize(width, height);
}

int sma_entry_get_x(const smol_atlas_entry_t* entry)
{
    return entry->x;
}
int sma_entry_get_y(const smol_atlas_entry_t* entry)
{
    return entry->y;
}
int sma_entry_get_width(const smol_atlas_entry_t* entry)
{
    return entry->width;
}
int sma_entry_get_height(const smol_atlas_entry_t* entry)
{
    return entry->height;
}
