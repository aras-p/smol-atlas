// SPDX-License-Identifier: MIT OR Unlicense
// smol-atlas: https://github.com/aras-p/smol-atlas

#include "smol-atlas.h"

#include <assert.h>
#include <stddef.h>
#include <vector>
#include <type_traits>

// "memory pool" that allocates items from a single large array,
// and maintains a freelist of items for O(1) alloc and free.
template <typename T>
struct smol_pool_t
{
    static_assert(std::is_trivially_destructible_v<T>, "smol_pool_t type must be trivially destructible");

    union item_t
    {
        int next_index;
        alignas(alignof(T)) char data[sizeof(T)];
    };

    std::vector<item_t> m_storage;
    int m_free_index = -1;

    smol_pool_t(size_t reserve_size)
    {
        m_storage.reserve(reserve_size);
    }

    void clear()
    {
        m_storage.clear();
        m_free_index = -1;
    }

    template <typename... Args> T* alloc(Args &&... args)
    {
        // add item if no free one
        if (m_free_index == -1) {
            m_storage.emplace_back();
            m_storage.back().next_index = -1;
            m_free_index = (int)(m_storage.size()-1);
        }

        // grab item from free list
        item_t* item = m_storage.data() + m_free_index;
        m_free_index = item->next_index;

        // construct the object
        T* res = reinterpret_cast<T*>(item->data);
        new (res) T(std::forward<Args>(args)...);
        return res;
    }

    void free(T* ptr)
    {
        // add item to the free list
        item_t* item = reinterpret_cast<item_t*>(ptr);
        item->next_index = m_free_index;
        m_free_index = (int)(item - m_storage.data());
    }
    
    const T* data() const { return m_storage.empty() ? nullptr : reinterpret_cast<const T*>(m_storage.front().data); }
    T* data() { return m_storage.empty() ? nullptr : reinterpret_cast<T*>(m_storage.front().data); }
};

static inline int max_i(int a, int b)
{
    return a > b ? a : b;
}

struct smol_item_t
{
    explicit smol_item_t(int x_, int y_, int w_, int h_, int shelf_)
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
    explicit smol_free_span_t(int x_, int w_) : x(x_), width(w_), next_index(-1) {}

    int x;
    int width;
    int next_index;
};

template<typename T>
struct smol_single_list_t
{
    smol_single_list_t(int head_index = -1) : m_head_index(head_index) { }
    
    void insert(T* base, int prev_idx, int node_idx)
    {
        if (prev_idx == -1) { // this is first node
            base[node_idx].next_index = m_head_index;
            m_head_index = node_idx;
        }
        else {
            base[node_idx].next_index = base[prev_idx].next_index;
            base[prev_idx].next_index = node_idx;
        }
    }

    void remove(T* base, int prev_idx, int node_idx)
    {
        if (prev_idx != -1)
            base[prev_idx].next_index = base[node_idx].next_index;
        else
            m_head_index = base[node_idx].next_index;
    }

    int m_head_index = -1;
};

struct smol_shelf_t
{
    explicit smol_shelf_t(int y, int width, int height, int index, smol_pool_t<smol_free_span_t>& span_pool)
        : m_y(y), m_height(height), m_index(index)
    {
        smol_free_span_t* span = span_pool.alloc(0, width);
        m_free_spans.m_head_index = (int)(span - span_pool.data());
    }

    bool has_space_for(int width, smol_pool_t<smol_free_span_t>& span_pool) const
    {
        int span_idx = m_free_spans.m_head_index;
        while (span_idx != -1) {
            const smol_free_span_t* span = span_pool.data() + span_idx;
            if (width <= span->width)
                return true;
            span_idx = span->next_index;
        }
        return false;
    }

    smol_item_t* alloc_item(int w, int h, smol_pool_t<smol_item_t>& item_pool, smol_pool_t<smol_free_span_t>& span_pool)
    {
        if (h > m_height)
            return nullptr;

        // find a suitable free span
        int span_idx = m_free_spans.m_head_index;
        int prev_idx = -1;
        while (span_idx != -1) {
            const smol_free_span_t* span = span_pool.data() + span_idx;
            if (span->width >= w) {
                break;
            }
            prev_idx = span_idx;
            span_idx = span->next_index;
        }

        // no space in this shelf
        if (span_idx == -1)
            return nullptr;

        smol_free_span_t* it = span_pool.data() + span_idx;
        const int x = it->x;
        const int rest = it->width - w;
        if (rest > 0) {
            // there will be still space left in this span, adjust
            it->x += w;
            it->width -= w;
        }
        else {
            // whole span is taken, remove it
            m_free_spans.remove(span_pool.data(), prev_idx, span_idx);
            span_pool.free(it);
        }

        return item_pool.alloc(x, m_y, w, h, m_index);
    }

    void add_free_span(int x, int width, smol_pool_t<smol_free_span_t>& span_pool)
    {
        // insert into free spans list at the right position
        smol_free_span_t* free_e = span_pool.alloc(x, width);
        int free_e_idx = (int)(free_e - span_pool.data());
        bool added = false;
        int span_idx = m_free_spans.m_head_index;
        int prev_idx = -1;
        while (span_idx != -1) {
            const smol_free_span_t* span = span_pool.data() + span_idx;
            if (free_e->x < span->x) {
                // found right place, insert into the list
                m_free_spans.insert(span_pool.data(), prev_idx, free_e_idx);
                added = true;
                break;
            }
            prev_idx = span_idx;
            span_idx = span->next_index;
        }
        if (!added)
            m_free_spans.insert(span_pool.data(), prev_idx, free_e_idx);

        merge_free_spans(prev_idx, free_e, span_pool);
    }

    void free_item(smol_item_t* e, smol_pool_t<smol_item_t>& item_pool, smol_pool_t<smol_free_span_t>& span_pool)
    {
        assert(e);
        assert(e->shelf_index == m_index);
        assert(e->y == m_y);
        add_free_span(e->x, e->width, span_pool);
        item_pool.free(e);
    }

    void merge_free_spans(int prev_idx, smol_free_span_t* span, smol_pool_t<smol_free_span_t>& span_pool)
    {
        int next_idx = span->next_index;
        if (next_idx != -1) {
            smol_free_span_t* next = span_pool.data() + next_idx;
            if (span->x + span->width == next->x) {
                // merge with next
                span->width += next->width;
                m_free_spans.remove(span_pool.data(), (int)(span - span_pool.data()), next_idx);
                span_pool.free(next);
            }
        }
        if (prev_idx != -1) {
            smol_free_span_t* prev = span_pool.data() + prev_idx;
            if (prev->x + prev->width == span->x) {
                // merge with prev
                prev->width += span->width;
                m_free_spans.remove(span_pool.data(), prev_idx, (int)(span - span_pool.data()));
                span_pool.free(span);
            }
        }
    }

    smol_single_list_t<smol_free_span_t> m_free_spans;
    const int m_y;
    const int m_height;
    const int m_index;
};

struct smol_atlas_t
{
    explicit smol_atlas_t(int w, int h)
        : m_item_pool(1024), m_span_pool(1024)
    {
        m_shelves.reserve(8);
        m_width = w > 0 ? w : 64;
        m_height = h > 0 ? h : 64;
    }
    
    ~smol_atlas_t()
    {
        clear();
    }
    
    smol_atlas_item_t item_to_handle(const smol_item_t* item) const
    {
        smol_atlas_item_t res;
        if (item != nullptr) {
            int index = (int)(item - m_item_pool.data());
            assert(index >= 0 && index < m_item_pool.m_storage.size());
            res.id = index;
        }
        return res;
    }

    smol_atlas_item_t pack(int w, int h)
    {
        // find best shelf
        smol_shelf_t* best_shelf = nullptr;
        int best_score = 0x7fffffff;

        int top_y = 0;
        for (auto& shelf : m_shelves) {
            const int shelf_h = shelf.m_height;
            top_y = max_i(top_y, shelf.m_y + shelf_h);
            
            if (shelf_h < h)
                continue; // too short
            
            if (shelf_h == h) { // exact height fit, try to use it
                smol_item_t* res = shelf.alloc_item(w, h, m_item_pool, m_span_pool);
                if (res != nullptr)
                    return item_to_handle(res);
            }

            // otherwise the shelf is too tall, track best one
            int score = shelf_h - h;
            if (score < best_score && shelf.has_space_for(w, m_span_pool)) {
                best_score = score;
                best_shelf = &shelf;
            }
        }

        if (best_shelf) {
            smol_item_t* res = best_shelf->alloc_item(w, h, m_item_pool, m_span_pool);
            if (res != nullptr)
                return item_to_handle(res);
        }

        // no shelf with enough space: add a new shelf
        if (w <= m_width && h <= m_height - top_y) {
            int shelf_index = int(m_shelves.size());
            m_shelves.emplace_back(top_y, m_width, h, shelf_index, m_span_pool);
            smol_item_t* res = m_shelves.back().alloc_item(w, h, m_item_pool, m_span_pool);
            return item_to_handle(res);
        }

        // out of space
        return smol_atlas_item_t();
    }
    
    const smol_item_t* handle_to_item(smol_atlas_item_t handle) const
    {
        if (!handle.is_valid())
            return nullptr;
        int index = handle.id;
        if (index < 0 || index >= m_item_pool.m_storage.size()) // stale handle
            return nullptr;
        return m_item_pool.data() + index;
    }

    smol_item_t* handle_to_item(smol_atlas_item_t handle)
    {
        if (!handle.is_valid())
            return nullptr;
        int index = handle.id;
        if (index < 0 || index >= m_item_pool.m_storage.size()) // stale handle
            return nullptr;
        return m_item_pool.data() + index;
    }

    void free_item(smol_atlas_item_t handle)
    {
        smol_item_t* item = handle_to_item(handle);
        if (item != nullptr) {
            assert(item->shelf_index >= 0 && item->shelf_index < m_shelves.size());
            m_shelves[item->shelf_index].free_item(item, m_item_pool, m_span_pool);
        }
    }

    void clear()
    {
        m_item_pool.clear();
        m_span_pool.clear();
        m_shelves.clear();
    }

    smol_pool_t<smol_item_t> m_item_pool;
    smol_pool_t<smol_free_span_t> m_span_pool;
    std::vector<smol_shelf_t> m_shelves;
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

smol_atlas_item_t sma_item_add(smol_atlas_t* atlas, int width, int height)
{
    return atlas->pack(width, height);
}

void sma_item_remove(smol_atlas_t* atlas, smol_atlas_item_t item)
{
    atlas->free_item(item);
}

void sma_atlas_clear(smol_atlas_t* atlas, int new_width, int new_height)
{
    atlas->clear();
    if (new_width > 0) atlas->m_width = new_width;
    if (new_height > 0) atlas->m_height = new_height;
}

int sma_item_x(const smol_atlas_t* atlas, smol_atlas_item_t item)
{
    const smol_item_t* i = atlas->handle_to_item(item);
    return i ? i->x : -1;
}
int sma_item_y(const smol_atlas_t* atlas, smol_atlas_item_t item)
{
    const smol_item_t* i = atlas->handle_to_item(item);
    return i ? i->y : -1;
}
int sma_item_width(const smol_atlas_t* atlas, smol_atlas_item_t item)
{
    const smol_item_t* i = atlas->handle_to_item(item);
    return i ? i->width : -1;
}
int sma_item_height(const smol_atlas_t* atlas, smol_atlas_item_t item)
{
    const smol_item_t* i = atlas->handle_to_item(item);
    return i ? i->height : -1;
}
