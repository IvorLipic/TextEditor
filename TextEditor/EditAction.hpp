#pragma once
#include "TE_API.hpp"

class TEXTEDITOR_API EditAction {
public:
    virtual ~EditAction() = default;
    virtual void execute_do() = 0;
    virtual void execute_undo() = 0;
};