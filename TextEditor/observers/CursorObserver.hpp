#pragma once
#include "../Location.hpp"

class CursorObserver {
public:
    virtual void updateCursorLocation(const Location& loc) = 0;
    virtual ~CursorObserver() = default;
};
