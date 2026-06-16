#include "UndoManager.hpp"

UndoManager* UndoManager::instance = nullptr;

UndoManager& UndoManager::getInstance() {
    if (!instance) {
        instance = new UndoManager();
    }
    return *instance;
}

void UndoManager::undo() {
    if (!canUndo()) return;

    inUndoRedo = true;
    
    auto action = std::move(undoStack.top());
    undoStack.pop();

    action->execute_undo();
    redoStack.push(std::move(action));

    inUndoRedo = false;

    notifyObservers();
}

void UndoManager::redo() {
    if (!canRedo()) return;

    inUndoRedo = true;
    
    auto action = std::move(redoStack.top());
    redoStack.pop();

    action->execute_do();

    undoStack.push(std::move(action));

    inUndoRedo = false;

    notifyObservers();
}

void UndoManager::push(std::unique_ptr<EditAction> action) {
    clearRedoStack();
    undoStack.push(std::move(action));
    notifyObservers();
}

void UndoManager::clearRedoStack() {
    redoStack = std::stack<std::unique_ptr<EditAction>>();
    notifyObservers();
}

void UndoManager::clearUndoStack() {
    undoStack = std::stack<std::unique_ptr<EditAction>>();
    notifyObservers();
}

bool UndoManager::canUndo() const {
    return !undoStack.empty();
}

bool UndoManager::canRedo() const {
    return !redoStack.empty();
}

void UndoManager::attachObserver(UndoObserver* observer) {
    observers.insert(observer);
}

void UndoManager::detachObserver(UndoObserver* observer) {
    observers.erase(observer);
}

void UndoManager::notifyObservers() {
    for (auto* observer : observers) {
        observer->updateUndoStatus();
    }
}