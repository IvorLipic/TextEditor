#include "DeleteAction.hpp"
#include "TextEditorModel.hpp"

DeleteAction::DeleteAction(TextEditorModel& model, std::string deletedText, LocationRange affectedRange)
    : model(model), deletedText(std::move(deletedText)), affectedRange(affectedRange) {}

void DeleteAction::execute_do() {
    model.setCursorLocation(affectedRange.start);
    model.deleteRange(affectedRange);
}

void DeleteAction::execute_undo() {
    model.setCursorLocation(affectedRange.start);
    model.insertText(deletedText);
}