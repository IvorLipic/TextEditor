#pragma once
#include "EditAction.hpp"
#include "TextEditorModelFwd.hpp"
#include "LocationRange.hpp"
#include <string>

class InsertAction : public EditAction {
    TextEditorModel& model;
    std::string insertedText;
    LocationRange affectedRange;
    
public:
    InsertAction(TextEditorModel& model, std::string insertedText, LocationRange affectedRange);
    void execute_do() override;
    void execute_undo() override;
};