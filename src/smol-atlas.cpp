// SPDX-License-Identifier: MIT OR Unlicense
// smol-atlas: https://github.com/aras-p/smol-atlas

#include "smol-atlas.h"

#include <assert.h>
#include <memory>
#include <vector>

static inline int max_i(int a, int b)
{
    return a > b ? a : b;
}

struct smol_atlas_item_t
{
    explicit smol_atlas_item_t(int x_, int y_, int w_, int h_, int shelf_)
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
        : m_y(y), m_height(height), m_index(index)
    {
        m_free_spans = new smol_free_span_t(0, width);
    }
    ~smol_shelf_t()
    {
        // release the entries
        for (smol_atlas_item_t* e : m_entries) {
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

    smol_atlas_item_t* alloc_item(int w, int h)
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

        smol_atlas_item_t* e = new smol_atlas_item_t(x, m_y, w, h, m_index);
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

    void free_item(smol_atlas_item_t* e)
    {
        assert(e);
        assert(e->shelf_index == m_index);
        assert(e->y == m_y);
        add_free_span(e->x, e->width);

        // remove the actual item
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

    const int m_y;
    const int m_height;
    const int m_index;

    std::vector<smol_atlas_item_t*> m_entries;
    smol_free_span_t* m_free_spans;
};


struct smol_atlas_t
{
    explicit smol_atlas_t(int w, int h)
    {
        m_width = w > 0 ? w : 64;
        m_height = h > 0 ? h : 64;
    }

    smol_atlas_item_t* pack(int w, int h)
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
                return shelf->alloc_item(w, h);

            // otherwise the shelf is too tall, track best one
            int score = shelf_h - h;
            if (score < best_score) {
                best_score = score;
                best_shelf = shelf.get();
            }
        }

        if (best_shelf)
            return best_shelf->alloc_item(w, h);

        // no shelf with enough space: add a new shelf
        if (w <= m_width && h <= m_height - top_y) {
            int shelf_index = int(m_shelves.size());
            m_shelves.emplace_back(std::make_unique<smol_shelf_t>(top_y, m_width, h, shelf_index));
            return m_shelves.back()->alloc_item(w, h);
        }

        // out of space
        return nullptr;
    }

    void free_item(smol_atlas_item_t* item)
    {
        if (item == nullptr)
            return;
        assert(item->shelf_index >= 0 && item->shelf_index < m_shelves.size());
        m_shelves[item->shelf_index]->free_item(item);
    }

    void clear()
    {
        m_shelves.clear();
    }

    std::vector<std::unique_ptr<smol_shelf_t>> m_shelves;
    int m_width;
    int m_height;
};

smol_atlas_t* sma_create(int width, int height)
{
    return new smol_atlas_t(width, height);
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

smol_atlas_item_t* sma_add(smol_atlas_t* atlas, int width, int height)
{
    return atlas->pack(width, height);
}

void sma_remove(smol_atlas_t* atlas, smol_atlas_item_t* item)
{
    atlas->free_item(item);
}

void sma_clear(smol_atlas_t* atlas)
{
    atlas->clear();
}

int sma_item_x(const smol_atlas_item_t* item)
{
    return item->x;
}
int sma_item_y(const smol_atlas_item_t* item)
{
    return item->y;
}
int sma_item_width(const smol_atlas_item_t* item)
{
    return item->width;
}
int sma_item_height(const smol_atlas_item_t* item)
{
    return item->height;
}
