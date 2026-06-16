#include "InsertAction.hpp"
#include "TextEditorModel.hpp"

InsertAction::InsertAction(TextEditorModel& model, std::string insertedText, LocationRange affectedRange)
    : model(model), insertedText(std::move(insertedText)), affectedRange(affectedRange) {}

void InsertAction::execute_do() {
    model.setCursorLocation(affectedRange.start);
    model.insertText(insertedText);
}

void InsertAction::execute_undo() {
    model.setCursorLocation(affectedRange.start);
    model.deleteRange(affectedRange);
}