#pragma once
#include "TE_API.hpp"
#include <stack>
#include <memory>
#include <set>
#include "EditAction.hpp"
#include "observers/UndoObserver.hpp"

class TEXTEDITOR_API UndoManager {
private:
    std::stack<std::unique_ptr<EditAction>> undoStack;
    std::stack<std::unique_ptr<EditAction>> redoStack;
    std::set<UndoObserver*> observers;

    bool inUndoRedo = false;
    
    static UndoManager* instance; 
    UndoManager() = default; 

public:
    // Singleton access
    static UndoManager& getInstance();
    UndoManager(const UndoManager&) = delete;
    UndoManager& operator=(const UndoManager&) = delete;

    void undo();
    void redo();
    void push(std::unique_ptr<EditAction> action);
    void clearRedoStack();
    void clearUndoStack();

    bool isInUndoRedo() const { return inUndoRedo; }
    void setInUndoRedo(bool val) { inUndoRedo = val; }

    void attachObserver(UndoObserver* observer);
    void detachObserver(UndoObserver* observer);
    void notifyObservers();

    bool canUndo() const;
    bool canRedo() const;
};