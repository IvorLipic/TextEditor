#pragma once

class TextObserver {
public:
    virtual ~TextObserver() = default;
    virtual void updateText() = 0;
};
