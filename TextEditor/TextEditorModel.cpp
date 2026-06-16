#include "TextEditorModel.hpp"

TextEditorModel::TextEditorModel(const std::string& text) {
    // Split input text into lines by newline
    size_t start = 0, end;
    while ((end = text.find('\n', start)) != std::string::npos) {
        lines.push_back(text.substr(start, end - start));
        start = end + 1;
    }
    lines.push_back(text.substr(start));  // Last line
    cursorLocation = Location{0, 0};
    selectionRange = LocationRange{cursorLocation, cursorLocation};
}

void TextEditorModel::moveCursorLeft(bool shift) {
    Location oldLoc = getCursorLocation();
    if (cursorLocation.col > 0) {
        cursorLocation.col--;
    } else if (cursorLocation.row > 0) { 
        // Move up a line to the last character
        cursorLocation.row--;
        cursorLocation.col = lines[cursorLocation.row].length();
    } else {
        return;
    }
    updateSelectionAfterMove(oldLoc, shift);
}

void TextEditorModel::moveCursorRight(bool shift) {
    Location oldLoc = getCursorLocation();
    if (cursorLocation.col < (int)lines[cursorLocation.row].length()) {
        cursorLocation.col++;
    } else if (cursorLocation.row + 1 < static_cast<int>(lines.size())) { 
        // Move down a line to the first character
        cursorLocation.row++;
        cursorLocation.col = 0;
    } else {
        return;
    }
    updateSelectionAfterMove(oldLoc, shift);
}

void TextEditorModel::moveCursorUp(bool shift) {
    Location oldLoc = getCursorLocation();
    if (cursorLocation.row > 0) {
        cursorLocation.row--;
        // If current line is longer, adjust cursor position to the last character of next line
        cursorLocation.col = std::min(cursorLocation.col, (int)lines[cursorLocation.row].length());
        updateSelectionAfterMove(oldLoc, shift);
    }
}

void TextEditorModel::moveCursorDown(bool shift) {
    Location oldLoc = getCursorLocation();
    if (cursorLocation.row + 1 < static_cast<int>(lines.size())) {
        cursorLocation.row++;
        cursorLocation.col = std::min(cursorLocation.col, (int)lines[cursorLocation.row].length());
        updateSelectionAfterMove(oldLoc, shift);
    }
}

void TextEditorModel::updateSelectionAfterMove(Location oldLoc, bool shift) {
    if (shift) {
        if (!selectionAnchor.has_value()) {
            selectionAnchor = oldLoc;
        }
        setSelectionRange(LocationRange{*selectionAnchor, cursorLocation});
    } else {
        selectionRange = LocationRange{cursorLocation, cursorLocation};
        selectionAnchor.reset();
    }
    notifyCursorObservers();
}

void TextEditorModel::setSelectionRange(const LocationRange& range) {
    if (range.start < range.end) {
        selectionRange = range;
    } else {
        selectionRange = LocationRange{range.end, range.start};
    }
}

void TextEditorModel::setCursorLocation(const Location& loc) {
    cursorLocation = loc;
}

void TextEditorModel::deleteBefore() {
    if (cursorLocation.col > 0) {
        // Single character deletion
        Location start = {cursorLocation.row, cursorLocation.col - 1};
        Location end = cursorLocation;
        
        if(!UndoManager::getInstance().isInUndoRedo()) {
            std::string deleted = getTextInRange({start, end});
            auto action = std::make_unique<DeleteAction>(*this, deleted, LocationRange{start, end});
            UndoManager::getInstance().push(std::move(action));
        }
        
        lines[cursorLocation.row].erase(cursorLocation.col - 1, 1);
        cursorLocation.col--;
    } else if (cursorLocation.row > 0) {
        // Line merging deletion
        Location start = {cursorLocation.row - 1, (int)lines[cursorLocation.row - 1].size()};
        Location end = cursorLocation;

        if(!UndoManager::getInstance().isInUndoRedo()) {
            std::string deleted = "\n";
            auto action = std::make_unique<DeleteAction>(*this, deleted, LocationRange{start, end});
            UndoManager::getInstance().push(std::move(action));
        }
        
        std::string& currentLine = lines[cursorLocation.row];
        cursorLocation.row--;
        cursorLocation.col = lines[cursorLocation.row].size();
        lines[cursorLocation.row] += currentLine;
        lines.erase(lines.begin() + cursorLocation.row + 1);
    }
    notifyTextObservers();
    notifyCursorObservers();
}

void TextEditorModel::deleteAfter() {
    if (cursorLocation.col < (int)lines[cursorLocation.row].size()) {
        Location start = cursorLocation;
        Location end = {cursorLocation.row, cursorLocation.col + 1};
        
        if(!UndoManager::getInstance().isInUndoRedo()) {
            std::string deleted = getTextInRange({start, end});
            auto action = std::make_unique<DeleteAction>(*this, deleted, LocationRange{start, end});
            UndoManager::getInstance().push(std::move(action));
        }
        
        lines[cursorLocation.row].erase(cursorLocation.col, 1);
    } else if (cursorLocation.row + 1 < (int)(lines.size())) {
        Location start = cursorLocation;
        Location end = {cursorLocation.row + 1, 0};

        if(!UndoManager::getInstance().isInUndoRedo()) {
            std::string deleted = "\n";
            auto action = std::make_unique<DeleteAction>(*this, deleted, LocationRange{start, end});
            UndoManager::getInstance().push(std::move(action));
        }
        
        lines[cursorLocation.row] += lines[cursorLocation.row + 1];
        lines.erase(lines.begin() + cursorLocation.row + 1);
    }
    notifyTextObservers();
    notifyCursorObservers();
}

void TextEditorModel::deleteRange(const LocationRange& r) {
    if (lines.empty()) return;

    Location start = r.start;
    Location end = r.end;
    if (end < start) std::swap(start, end);

    start.row = std::clamp(start.row, 0, (int)lines.size() - 1);
    end.row = std::clamp(end.row, 0, (int)lines.size() - 1);

    if(!UndoManager::getInstance().isInUndoRedo()) {
        std::string deleted = getTextInRange({start, end});
        auto action = std::make_unique<DeleteAction>(*this, deleted, LocationRange{start, end});
        UndoManager::getInstance().push(std::move(action));
    }

    start.col = std::clamp(start.col, 0, (int)lines[start.row].length());
    end.col = std::clamp(end.col, 0, (int)lines[end.row].length());

    if (start.row == end.row) {
        lines[start.row].erase(start.col, end.col - start.col);
    } else {
        std::string& startLine = lines[start.row];
        std::string& endLine = lines[end.row];
        startLine.erase(start.col);
        startLine += endLine.substr(end.col);
        lines.erase(lines.begin() + start.row + 1, lines.begin() + end.row + 1);
    }

    if (lines.empty()) lines.push_back("");

    cursorLocation.row = std::min(start.row, (int)lines.size() - 1);
    cursorLocation.col = std::min(start.col, (int)lines[cursorLocation.row].size());

    selectionRange = LocationRange{cursorLocation, cursorLocation};

    notifyTextObservers();
    notifyCursorObservers();
}

void TextEditorModel::notifyCursorObservers() {
    for (CursorObserver* obs : cursorObservers) {
        obs->updateCursorLocation(cursorLocation);
    }
}

void TextEditorModel::notifyTextObservers() {
    for (auto* obs : textObservers) {
        obs->updateText();
    }
}

void TextEditorModel::insertChar(char c) {
    Location originalCursorLocation = cursorLocation;

    // Handle selection first
    if (!(selectionRange.start == selectionRange.end)) {
        deleteRange(selectionRange);
        originalCursorLocation = cursorLocation;
    }

    // Handle newline
    if (c == '\n') {
        std::string currentLine = lines[cursorLocation.row];
        std::string beforeCursor = currentLine.substr(0, cursorLocation.col);
        std::string afterCursor = currentLine.substr(cursorLocation.col);
        
        lines[cursorLocation.row] = beforeCursor;
        lines.insert(lines.begin() + cursorLocation.row + 1, afterCursor);
        
        cursorLocation.row++;
        cursorLocation.col = 0;
    } 
    // Handle regular character
    else {
        if (lines.empty()) lines.push_back("");
        if (cursorLocation.row >= (int)lines.size()) {
            cursorLocation.row = lines.size() - 1;
            cursorLocation.col = lines[cursorLocation.row].length();
        }
        lines[cursorLocation.row].insert(cursorLocation.col, 1, c);
        cursorLocation.col++;
    }

    Location endLocationAfterInsert = cursorLocation;

    if(!UndoManager::getInstance().isInUndoRedo()) {
        auto action = std::make_unique<InsertAction>(*this, std::string(1, c), 
                                                     LocationRange{originalCursorLocation, endLocationAfterInsert});
        UndoManager::getInstance().push(std::move(action));
    }

    selectionRange = LocationRange{cursorLocation, cursorLocation};
    notifyTextObservers();
    notifyCursorObservers();
}

void TextEditorModel::insertText(const std::string& text) {
    Location originalCursorLocation = cursorLocation;

    // Handle selection first
    if (!(selectionRange.start == selectionRange.end)) {
        deleteRange(selectionRange);
        originalCursorLocation = cursorLocation;
    }

    if (lines.empty()) lines.push_back("");

    size_t currentTextPos = 0;
    Location insertStartLoc = cursorLocation;
    
    while (currentTextPos < text.length()) {
        size_t newlinePos = text.find('\n', currentTextPos);
        std::string segment = (newlinePos == std::string::npos) ? text.substr(currentTextPos) : text.substr(currentTextPos, newlinePos - currentTextPos);

        // Ensure current line exists and cursor is within bounds for insert
        if (cursorLocation.row >= (int)lines.size()) {
            cursorLocation.row = lines.size() - 1;
            cursorLocation.col = lines[cursorLocation.row].length();
        }
        if (cursorLocation.col > (int)lines[cursorLocation.row].length()) {
            cursorLocation.col = lines[cursorLocation.row].length();
        }

        // Insert segment
        lines[cursorLocation.row].insert(cursorLocation.col, segment);
        cursorLocation.col += segment.length();

        if (newlinePos != std::string::npos) {
            std::string& currentLine = lines[cursorLocation.row];
            std::string afterNewline = currentLine.substr(cursorLocation.col);
            currentLine.erase(cursorLocation.col);
            
            lines.insert(lines.begin() + cursorLocation.row + 1, afterNewline);
            cursorLocation.row++;
            cursorLocation.col = 0;
            currentTextPos = newlinePos + 1;
        } else {
            currentTextPos = text.length();
        }
    }

    Location endLocationAfterInsert = cursorLocation;

    if (!UndoManager::getInstance().isInUndoRedo()) {
        auto action = std::make_unique<InsertAction>(*this, text, 
                                                     LocationRange{insertStartLoc, endLocationAfterInsert});
        UndoManager::getInstance().push(std::move(action));
    }

    selectionRange = LocationRange{cursorLocation, cursorLocation};
    notifyTextObservers();
    notifyCursorObservers();
}

std::string TextEditorModel::getTextInRange(const LocationRange& range) const {
    if (lines.empty()) return "";
    
    Location start = range.start;
    Location end = range.end;
    if (end < start) std::swap(start, end);

    start.row = std::clamp(start.row, 0, (int)lines.size() - 1);
    end.row = std::clamp(end.row, 0, (int)lines.size() - 1);

    const std::string& startLine = lines[start.row];
    const std::string& endLine = lines[end.row];

    start.col = std::clamp(start.col, 0, (int)(startLine.length()));
    end.col = std::clamp(end.col, 0, (int)(endLine.length()));

    if (start.row == end.row) {
        return startLine.substr(start.col, end.col - start.col);
    }

    std::string result;
    result += startLine.substr(start.col) + "\n";
    
    for (int i = start.row + 1; i < end.row; i++) {
        result += lines[i] + "\n";
    }

    result += endLine.substr(0, end.col);
    return result;
}

