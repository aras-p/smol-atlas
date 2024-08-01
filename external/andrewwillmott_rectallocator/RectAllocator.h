// File:       RectAllocator.h
// Author(s):  Andrew Willmott
// Copyright:  2016
//
// Adapted from https://gist.github.com/andrewwillmott/f9124eb445df7b3687a666fe36d3dcdb as of 2024 Aug 1

#pragma once

#include <vector>
#include <assert.h>

namespace nHL
{
    struct Vec2i
    {
        Vec2i(int x_, int y_) : x(x_), y(y_) {}
        int x;
        int y;
    };
    #define vl_0 Vec2i{0,0}
    inline Vec2i operator-(const Vec2i& a, const Vec2i& b)
    {
        return Vec2i{a.x-b.x, a.y-b.y};
    }
    inline bool operator==(const Vec2i& a, const Vec2i& b)
    {
        return a.x == b.x && a.y == b.y;
    }

    typedef int tRectRef;
    const tRectRef kNullRectRef = -1;


    struct cRectInfo
    {
        Vec2i mOrigin = vl_0;
        Vec2i mSize   = vl_0;

        cRectInfo() {}
        cRectInfo(Vec2i size) : mSize(size) {}
        cRectInfo(Vec2i origin, Vec2i size) : mOrigin(origin), mSize(size) {}
    };

    class cRectAllocator
    ///< Allocates & frees subrectangles out of a master rectangle dynamically, and
    ///< reasonably efficiently.
    {
    public:
        cRectAllocator(Vec2i pageSize = vl_0, int expected = 0);   //!< Initialise rectangle of the given size

        Vec2i    PageSize() const;      ///< Returns current page size
        void     Clear(Vec2i pageSize); ///< Clear all allocated rects; invalidates all refs.

        tRectRef Alloc(Vec2i size);     ///< Returns ref to rect of given size, or kNullRectRef if space is exhausted
        void     Free (tRectRef ref);   ///< Free previously allocated rect.

        const cRectInfo& RectInfo(tRectRef ref) const; ///< Returns the allocated rectangle for the given ref.

        bool IsValid(tRectRef ref) const;
        ///< Returns true if the given ref is valid.

        // Intended for debug/visualisation use
        void AddUsedRects  (std::vector<tRectRef>* refs) const;   //!< Add all used rects to given list
        void AddUnusedRects(std::vector<tRectRef>* refs) const;   //!< Add all unused rects to given list

    protected:
        tRectRef Alloc(tRectRef ref, Vec2i size);

        tRectRef AllocRef();
        void     FreeRef(tRectRef ref);

        // Data
        struct cSubRect : public cRectInfo
        {
            tRectRef mParent = -1;
            tRectRef mLeft   = -1;
            tRectRef mRight  = -1;
            bool     mEmpty  = true;

            cSubRect() {};
            cSubRect(const cRectInfo& r) : cRectInfo(r) {}

            bool HasChildren() const { return mLeft >= 0; }
        };

        typedef std::vector<cSubRect> tSubRects;
        typedef std::vector<tRectRef> tFreeSubRects;

        tSubRects      mSubRects;       //!< Binary tree of allocated rects
        tFreeSubRects  mFreeSubRects;   //!< Available slots
    };



    // Inlines

    inline const cRectInfo& cRectAllocator::RectInfo(tRectRef ref) const
    {
        assert(IsValid(ref));
        return mSubRects[ref];
    }
    inline bool cRectAllocator::IsValid(tRectRef ref) const
    {
        return ref >= 0 && ref < int(mSubRects.size()) && !mSubRects[ref].HasChildren();
    }
}
