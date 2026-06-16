#pragma once

class UndoObserver {
public:
    virtual ~UndoObserver() = default;
    virtual void updateUndoStatus() = 0;
};