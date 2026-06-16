#include "ClipboardStack.hpp"

void ClipboardStack::push(const std::string& text) {
    texts.push_back(text);
    notifyObservers();
}

std::string ClipboardStack::pop() {
    if (texts.empty()) return "";
    std::string text = texts.back();
    texts.pop_back();
    notifyObservers();
    return text;
}

std::string ClipboardStack::peek() const {
    return texts.empty() ? "" : texts.back();
}

bool ClipboardStack::isEmpty() const {
    return texts.empty();
}

void ClipboardStack::clear() {
    texts.clear();
    notifyObservers();
}

void ClipboardStack::attachObserver(ClipboardObserver* obs) {
    observers.insert(obs);
}

void ClipboardStack::detachObserver(ClipboardObserver* obs) {
    observers.erase(obs);
}

void ClipboardStack::notifyObservers() {
    for (auto* obs : observers) {
        obs->updateClipboard();
    }
}