#pragma once
#include "TE_API.hpp"
#include <vector>
#include <string>
#include <set>
#include <optional>
#include <iostream>
#include <algorithm>
#include <memory>
#include "Location.hpp"
#include "LocationRange.hpp"
#include "observers/CursorObserver.hpp"
#include "observers/TextObserver.hpp"
#include "EditAction.hpp"
#include "InsertAction.hpp"
#include "DeleteAction.hpp"
#include "UndoManager.hpp"

class TEXTEDITOR_API TextEditorModel {
private:
    std::vector<std::string> lines;
    Location cursorLocation;
    LocationRange selectionRange;
    std::optional<Location> selectionAnchor;
    std::set<CursorObserver*> cursorObservers;
    std::set<TextObserver*> textObservers;

public:
    TextEditorModel(const std::string& text);

    using StringVectorIterator = std::vector<std::string>::const_iterator;
    using Iterator = std::pair<StringVectorIterator, StringVectorIterator>;
    
    Iterator allLines() const {
        return {lines.begin(), lines.end()};
    }

    Iterator linesRange(int index1, int index2) const {
        index1 = std::max(0, index1);
        index2 = std::min(static_cast<int>(lines.size()), index2);
        return {lines.begin() + index1, lines.begin() + index2};
    }

    void attachCursorObserver(CursorObserver* obs) { cursorObservers.insert(obs); }
    void detachCursorObserver(CursorObserver* obs) { cursorObservers.erase(obs); }
    void attachTextObserver(TextObserver* obs) { textObservers.insert(obs); }
    void detachTextObserver(TextObserver* obs) { textObservers.erase(obs); }

    // Getters
    const std::vector<std::string>& getLines() const { return lines; }
    const Location& getCursorLocation() const { return cursorLocation; }
    const LocationRange& getSelectionRange() const { return selectionRange; }
    const std::optional<Location>& getSelectionAnchor() const { return selectionAnchor; }

    //Setters
    void setSelectionRange(const LocationRange& range);
    void setCursorLocation(const Location& loc);

    void clearSelectionAnchor() { selectionAnchor.reset(); }

    void moveCursorLeft(bool shift);
    void moveCursorRight(bool shift);
    void moveCursorUp(bool shift);
    void moveCursorDown(bool shift);

    void deleteBefore();
    void deleteAfter();
    void deleteRange(const LocationRange& range);

    void insertChar(char c);
    void insertText(const std::string& text);

private:
    void notifyCursorObservers();
    void notifyTextObservers();
    std::string getTextInRange(const LocationRange& range) const;
    void updateSelectionAfterMove(Location oldLoc, bool shift);
};
