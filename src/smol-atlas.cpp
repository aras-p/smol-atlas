// SPDX-License-Identifier: MIT OR Unlicense
// smol-atlas: https://github.com/aras-p/smol-atlas

#include "smol-atlas.h"

#include <assert.h>
#include <vector>

static inline int max_i(int a, int b)
{
    return a > b ? a : b;
}

struct smol_atlas_item_t
{
    explicit smol_atlas_item_t(int x_, int y_, int w_, int h_, int shelf_)
    : x(x_), y(y_), width(w_), height(h_), item_index(-1), shelf_index(shelf_)
    {
    }

    int x;
    int y;
    int width;
    int height;
    int item_index;
    int shelf_index;
};

struct smol_free_span_t
{
    explicit smol_free_span_t(int x_, int w_) : x(x_), width(w_), next(nullptr) {}
    
    int x;
    int width;
    smol_free_span_t* next;
};

template<typename T>
struct smol_single_list_t
{
    smol_single_list_t(T* h) : m_head(h) { }
    ~smol_single_list_t()
    {
        T* node = m_head;
        while (node != nullptr) {
            T* ptr = node;
            node = node->next;
            delete ptr;
        }
    }
    
    void insert(T* prev, T* node)
    {
        if (prev == nullptr) { // this is first node
            node->next = m_head;
            m_head = node;
        }
        else {
            node->next = prev->next;
            prev->next = node;
        }
    }

    void remove(T* prev, T* node)
    {
        if (prev)
            prev->next = node->next;
        else
            m_head = node->next;
        delete node;
    }

    T* m_head = nullptr;
};

struct smol_shelf_t
{
    explicit smol_shelf_t(int y, int width, int height, int index)
        : m_y(y), m_height(height), m_index(index), m_free_spans(new smol_free_span_t(0, width))
    {
    }
    ~smol_shelf_t()
    {
        // release the entries
        for (smol_atlas_item_t* e : m_entries) {
            delete e;
        }
    }

    bool has_space_for(int width) const
    {
        smol_free_span_t* it = m_free_spans.m_head;
        while (it != nullptr) {
            if (width <= it->width)
                return true;
            it = it->next;
        }
        return false;
    }

    smol_atlas_item_t* alloc_item(int w, int h)
    {
        if (h > m_height)
            return nullptr;

        // find a suitable free span
        smol_free_span_t* it = m_free_spans.m_head;
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
            m_free_spans.remove(prev, it);
        }

        smol_atlas_item_t* e = new smol_atlas_item_t(x, m_y, w, h, m_index);
        e->item_index = (int)m_entries.size();
        m_entries.emplace_back(e);
        return e;
    }

    void add_free_span(int x, int width)
    {
        // insert into free spans list at the right position
        smol_free_span_t* free_e = new smol_free_span_t(x, width);
        smol_free_span_t* it = m_free_spans.m_head;
        smol_free_span_t* prev = nullptr;
        bool added = false;
        while (it != nullptr) {
            if (free_e->x < it->x) {
                // found right place, insert into the list
                m_free_spans.insert(prev, free_e);
                added = true;
                break;
            }
            prev = it;
            it = it->next;
        }
        if (!added)
            m_free_spans.insert(prev, free_e);

        merge_free_spans(prev, free_e);
    }

    void free_item(smol_atlas_item_t* e)
    {
        assert(e);
        assert(e->shelf_index == m_index);
        assert(e->y == m_y);
        add_free_span(e->x, e->width);

        // remove the actual item
        assert(e->item_index == std::distance(m_entries.begin(), std::find(m_entries.begin(), m_entries.end(), e)));
        int index = e->item_index;
        if (index < m_entries.size() - 1) {
            m_entries[index] = m_entries.back();
            m_entries[index]->item_index = index;
        }
        m_entries.pop_back();
        delete e;
    }

    void merge_free_spans(smol_free_span_t* prev, smol_free_span_t* span)
    {
        smol_free_span_t* next = span->next;
        if (next != nullptr && span->x + span->width == next->x) {
            // merge with next
            span->width += next->width;
            m_free_spans.remove(span, next);
        }
        if (prev != nullptr && prev->x + prev->width == span->x) {
            // merge with prev
            prev->width += span->width;
            m_free_spans.remove(prev, span);
        }
    }

    const int m_y;
    const int m_height;
    const int m_index;

    std::vector<smol_atlas_item_t*> m_entries;
    smol_single_list_t<smol_free_span_t> m_free_spans;
};


struct smol_atlas_t
{
    explicit smol_atlas_t(int w, int h)
    {
        m_width = w > 0 ? w : 64;
        m_height = h > 0 ? h : 64;
    }
    
    ~smol_atlas_t()
    {
        clear();
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
            
            if (shelf_h == h) { // exact height fit, try to use it
                smol_atlas_item_t* res = shelf->alloc_item(w, h);
                if (res != nullptr)
                    return res;
            }

            // otherwise the shelf is too tall, track best one
            int score = shelf_h - h;
            if (score < best_score && shelf->has_space_for(w)) {
                best_score = score;
                best_shelf = shelf;
            }
        }

        if (best_shelf) {
            smol_atlas_item_t* res = best_shelf->alloc_item(w, h);
            if (res != nullptr)
                return res;
        }

        // no shelf with enough space: add a new shelf
        if (w <= m_width && h <= m_height - top_y) {
            int shelf_index = int(m_shelves.size());
            smol_shelf_t* shelf = new smol_shelf_t(top_y, m_width, h, shelf_index);
            m_shelves.emplace_back(shelf);
            return shelf->alloc_item(w, h);
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
        for (auto s : m_shelves)
            delete s;
        m_shelves.clear();
    }

    std::vector<smol_shelf_t*> m_shelves;
    int m_width;
    int m_height;
};

smol_atlas_t* sma_atlas_create(int width, int height)
{
    return new smol_atlas_t(width, height);
}

void sma_atlas_destroy(smol_atlas_t* atlas)
{
    delete atlas;
}

int sma_atlas_width(const smol_atlas_t* atlas)
{
    return atlas->m_width;
}

int sma_atlas_height(const smol_atlas_t* atlas)
{
    return atlas->m_height;
}

smol_atlas_item_t* sma_item_add(smol_atlas_t* atlas, int width, int height)
{
    return atlas->pack(width, height);
}

void sma_item_remove(smol_atlas_t* atlas, smol_atlas_item_t* item)
{
    atlas->free_item(item);
}

void sma_atlas_clear(smol_atlas_t* atlas)
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
