#pragma once
#include "EditAction.hpp"
#include "TextEditorModelFwd.hpp"
#include "LocationRange.hpp"
#include <string>

class DeleteAction : public EditAction {
    TextEditorModel& model;
    std::string deletedText;
    LocationRange affectedRange;
    
public:
    DeleteAction(TextEditorModel& model, std::string deletedText, LocationRange affectedRange);
    void execute_do() override;
    void execute_undo() override;
};