#pragma once

class ClipboardObserver {
public:
    virtual ~ClipboardObserver() = default;
    virtual void updateClipboard() = 0;
};