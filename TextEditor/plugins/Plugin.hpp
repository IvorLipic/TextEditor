#pragma once

#include <string>
#include "../TextEditorModelFwd.hpp"
#include "../ClipboardStack.hpp"

class UndoManager;

class Plugin {
public:
    virtual ~Plugin() = default;
    virtual std::string getName() const = 0;
    virtual std::string getDescription() const = 0;
    virtual void execute(TextEditorModel& model, UndoManager& undoManager, ClipboardStack& clipboardStack) = 0;
};