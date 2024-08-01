// File:       RectAllocator.cpp
// Author(s):  Andrew Willmott
// Copyright:  2016
//
// Adapted from https://gist.github.com/andrewwillmott/f9124eb445df7b3687a666fe36d3dcdb as of 2024 Aug 1

#include "RectAllocator.h"

using namespace nHL;

cRectAllocator::cRectAllocator(Vec2i pageSize, int expected)
{
    if (expected)
    {
        mSubRects.reserve(expected);
        mFreeSubRects.reserve(expected);
    }

    if (pageSize.x)
        mSubRects.push_back(cRectInfo(pageSize));
}

void cRectAllocator::Clear(Vec2i pageSize)
{
    mSubRects.clear();
    mFreeSubRects.clear();

    mSubRects.push_back(cRectInfo(pageSize));
}


tRectRef cRectAllocator::Alloc(Vec2i size)
{
    return Alloc(0, size);
}

void cRectAllocator::Free(tRectRef ref)
{
    assert(!mSubRects[ref].mEmpty);

    assert(ref < int(mSubRects.size()));

    mSubRects[ref].mEmpty = true;

    for (ref = mSubRects[ref].mParent; ref >= 0; ref = mSubRects[ref].mParent)
    {
        assert(mSubRects[ref].mEmpty);

        tRectRef left  = mSubRects[ref].mLeft;
        tRectRef right = mSubRects[ref].mRight;

        assert(left  < int(mSubRects.size()));
        assert(right < int(mSubRects.size()));

        if (mSubRects[left].HasChildren() || !mSubRects[left] .mEmpty)
            break;

        if (mSubRects[right].HasChildren() || !mSubRects[right].mEmpty)
            break;

        // Both leaf nodes now empty -- collapse and recurse

        // delete child nodes
        mSubRects[ref].mLeft  = -1;
        mSubRects[ref].mRight = -1;

        FreeRef(left);
        FreeRef(right);
    }
}

void cRectAllocator::AddUsedRects (std::vector<tRectRef>* refs) const
{
    for (int i = 0; i < int(mSubRects.size()); i++)
        if (!mSubRects[i].HasChildren() && !mSubRects[i].mEmpty)
            refs->push_back(i);
}

void cRectAllocator::AddUnusedRects(std::vector<tRectRef>* refs) const
{
    for (int i = 0; i < int(mSubRects.size()); i++)
        if (!mSubRects[i].HasChildren() && mSubRects[i].mEmpty)
            refs->push_back(i);
}

// Internal
tRectRef cRectAllocator::Alloc(tRectRef ref, Vec2i size)
{
    cSubRect* subRect = &mSubRects[ref];

    if (subRect->mSize.x < size.x || subRect->mSize.y < size.y)
        return kNullRectRef;    // Not big enough to fit this guy...

    if (subRect->HasChildren())
    {
        // Try both branches.
        assert(subRect->mEmpty);
        assert(subRect->mRight >= 0);

        tRectRef child = Alloc(subRect->mLeft, size);
        if (child < 0)
            child = Alloc(subRect->mRight, size);

        return child;
    }

    // we've reached a leaf node
    if (!subRect->mEmpty)
        return -1;

    Vec2i rsize = subRect->mSize - size;

    if (rsize == vl_0)
    {
        // Exact match
        subRect->mEmpty = false;
        return ref;
    }

    tRectRef left  = AllocRef();
    tRectRef right = AllocRef();

    subRect = &mSubRects[ref];  // Update in case of realloc

    subRect->mLeft  = left;
    subRect->mRight = right;

    // Split to give largest remainder
    if (rsize.x > rsize.y)
    {
        mSubRects[left ] = cRectInfo(subRect->mOrigin, Vec2i( size.x, subRect->mSize.y));
        mSubRects[right] = cRectInfo(subRect->mOrigin, Vec2i(rsize.x, subRect->mSize.y));
        mSubRects[right].mOrigin.x += size.x;
    }
    else
    {
        mSubRects[left ] = cRectInfo(subRect->mOrigin, Vec2i(subRect->mSize.x,  size.y));
        mSubRects[right] = cRectInfo(subRect->mOrigin, Vec2i(subRect->mSize.x, rsize.y));
        mSubRects[right].mOrigin.y += size.y;
    }

    mSubRects[left ].mParent = ref;
    mSubRects[right].mParent = ref;

    // allocate rect in first child
    return Alloc(subRect->mLeft, size);
}

tRectRef cRectAllocator::AllocRef()
{
    if (!mFreeSubRects.empty())
    {
        tRectRef ref = mFreeSubRects.back();
        mFreeSubRects.pop_back();
        return ref;
    }

    tRectRef ref = int(mSubRects.size());
    mSubRects.resize(ref + 1);

    return ref;
}

inline void cRectAllocator::FreeRef(tRectRef ref)
{
    assert(ref >= 0);

    mFreeSubRects.push_back(ref);
}
