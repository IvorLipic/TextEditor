#pragma once
#include "TE_API.hpp"
#include <vector>
#include <string>
#include <set>
#include "observers/ClipboardObserver.hpp"

class TEXTEDITOR_API ClipboardStack {
private:
    std::vector<std::string> texts;
    std::set<ClipboardObserver*> observers;

public:
    void push(const std::string& text);
    std::string pop();
    std::string peek() const;
    bool isEmpty() const;
    void clear();
    
    void attachObserver(ClipboardObserver* obs);
    void detachObserver(ClipboardObserver* obs);

private:
    void notifyObservers();
};