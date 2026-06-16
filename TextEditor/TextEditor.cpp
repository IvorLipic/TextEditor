#include "TextEditor.hpp"

TextEditor::TextEditor(TextEditorModel* model)
    : model(model)
{
    drawingArea = gtk_drawing_area_new();
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(drawingArea), on_draw, this, nullptr);
    gtk_widget_set_hexpand(drawingArea, TRUE);
    gtk_widget_set_vexpand(drawingArea, TRUE);
    gtk_widget_set_focusable(drawingArea, TRUE);
    gtk_widget_set_can_focus(drawingArea, TRUE);
    gtk_widget_set_size_request(drawingArea, 400, 300);

    // Click focus handler
    GtkGesture* click_gesture = gtk_gesture_click_new();
    gtk_event_controller_set_propagation_phase(GTK_EVENT_CONTROLLER(click_gesture), GTK_PHASE_CAPTURE);
    g_signal_connect(click_gesture, "pressed", G_CALLBACK(+[](GtkGestureClick*, int, double, double, gpointer user_data) {
        TextEditor* self = static_cast<TextEditor*>(user_data);
        gtk_widget_grab_focus(self->drawingArea);
    }), this);
    gtk_widget_add_controller(drawingArea, GTK_EVENT_CONTROLLER(click_gesture));
    g_signal_connect(drawingArea, "realize", G_CALLBACK(+[](GtkWidget* widget, gpointer) {
        gtk_widget_grab_focus(widget);
    }), nullptr);

    model->attachCursorObserver(this);
    model->attachTextObserver(this);
    clipboard.attachObserver(this);
    UndoManager::getInstance().attachObserver(this);

    GtkEventController* keyController = gtk_event_controller_key_new();
    g_signal_connect(keyController, "key-pressed", G_CALLBACK(on_key_pressed), this);
    gtk_widget_add_controller(drawingArea, keyController);
}

TextEditor::~TextEditor() {
    model->detachCursorObserver(this);
    model->detachTextObserver(this);
    clipboard.detachObserver(this);
    UndoManager::getInstance().detachObserver(this);
}

void TextEditor::on_draw(GtkDrawingArea*, cairo_t* cr, int, int, gpointer user_data) {
    TextEditor* editor = static_cast<TextEditor*>(user_data);
    TextEditorModel::Iterator lines_iterator = editor->model->allLines();

    // Set up Cairo font: use monospaced font for alignment
    cairo_select_font_face(cr, "Courier New", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 24);

    // Get font metrics
    cairo_font_extents_t font_extents;
    cairo_font_extents(cr, &font_extents);
    const double lineHeight = font_extents.height;
    double y = lineHeight; // Start drawing lines from this y position

    LocationRange sel = editor->model->getSelectionRange();
    Location selStart = sel.start;
    Location selEnd = sel.end;
    int lineIndex = 0; // Tracks which line we are rendering

    for (auto it = lines_iterator.first; it != lines_iterator.second; ++it) {
        cairo_move_to(cr, 10, y); // Move drawing position to the left margin (x = 10)

        // === SELECTION HIGHLIGHTING BLOCK ===
        // Check if this line is part of the selection
        if (lineIndex >= selStart.row && lineIndex <= selEnd.row) {
            
            int lineLen = it->size(); // Current line length

            // Compute selection start and end columns for this line
            int lineSelStart = (lineIndex == sel.start.row) ? sel.start.col : 0;
            int lineSelEnd = (lineIndex == sel.end.row) ? sel.end.col : lineLen;

            // Clamp columns to line boundaries
            lineSelStart = std::clamp(lineSelStart, 0, lineLen);
            lineSelEnd = std::clamp(lineSelEnd, 0, lineLen);

            // Ensure proper ordering
            if (lineSelStart > lineSelEnd) {
                std::swap(lineSelStart, lineSelEnd);
            }

            if(lineSelStart != lineSelEnd) {
                // Get substrings to measure widths of text
                std::string before = it->substr(0, lineSelStart); // text before selection
                std::string selected = it->substr(lineSelStart, lineSelEnd - lineSelStart); // selected text

                // Measure how far to move (x offset) to start drawing highlight
                cairo_text_extents_t ext_before, ext_selected;
                cairo_text_extents(cr, before.c_str(), &ext_before);
                cairo_text_extents(cr, selected.c_str(), &ext_selected);
                double x = 10 + ext_before.x_advance;

                cairo_set_source_rgb(cr, 0.8, 0.9, 1.0);

                // Use font ascent to position the top of the rectangle,
                // and font height for the full height of the highlight box.
                cairo_rectangle(
                    cr,
                    x,
                    y - font_extents.ascent,
                    ext_selected.x_advance,
                    font_extents.height
                );
                cairo_fill(cr);
            }
        }

        // === TEXT RENDERING ===
        cairo_move_to(cr, 10, y); // RESET move_to after rectangle fill
        cairo_set_source_rgb(cr, 0, 0, 0);
        cairo_show_text(cr, it->c_str());
        y += lineHeight;
        lineIndex++;
    }

    // === CURSOR DRAWING ===
    const auto& lines = editor->model->getLines();
    Location cursor = editor->model->getCursorLocation();

    if(cursor.row >= (int)lines.size()) { return; }

    // Get all text up to the cursor's column so we can measure its width
    std::string linePrefix = lines[cursor.row].substr(0, cursor.col);
    cairo_text_extents_t extents;
    cairo_text_extents(cr, linePrefix.c_str(), &extents);

    double cursor_x = 10.0 + extents.x_advance;
    double cursor_y = lineHeight * cursor.row + 2;

    // Draw the cursor as a vertical red line
    cairo_set_source_rgb(cr, 1, 0, 0);
    cairo_set_line_width(cr, 1.0);
    cairo_move_to(cr, cursor_x, cursor_y);
    cairo_line_to(cr, cursor_x, cursor_y + lineHeight - 1);
    cairo_stroke(cr);
}

std::string TextEditor::getSelectedText() const {
    LocationRange range = model->getSelectionRange();
    if (range.start == range.end) return "";

    if (range.end < range.start) {
        std::swap(range.start, range.end);
    }
    
    const auto& lines = model->getLines();

    range.start.row = std::clamp(range.start.row, 0, (int)lines.size() - 1);
    range.end.row = std::clamp(range.end.row, 0, (int)lines.size() - 1);
    
    range.start.col = std::clamp(range.start.col, 0, (int)lines[range.start.row].length());
    range.end.col = std::clamp(range.end.col, 0, (int)lines[range.end.row].length());
    
    // Handle single line selection
    if (range.start.row == range.end.row) {
        return lines[range.start.row].substr(range.start.col, range.end.col - range.start.col);
    }
    
    // Multi-line selection
    std::string result;
    // First line
    result = lines[range.start.row].substr(range.start.col) + "\n";
    // Middle lines (if any)
    for (int i = range.start.row + 1; i < range.end.row; i++) {
        result += lines[i] + "\n";
    }
    // Last line
    result += lines[range.end.row].substr(0, range.end.col);
    
    return result;
}

void TextEditor::handleCopy() {
    std::string selected = getSelectedText();
    if (!selected.empty()) {
        clipboard.push(selected);
    }
}

void TextEditor::handleCut() {
    LocationRange range = model->getSelectionRange();
    if (range.start == range.end) return;

    std::string selected = getSelectedText();
    if (!selected.empty()) {
        clipboard.push(selected);

        if (range.end < range.start) {
            std::swap(range.start, range.end);
        }

        model->deleteRange(range);

        Location cursor = model->getCursorLocation();
        model->setSelectionRange({cursor, cursor});
        model->clearSelectionAnchor();
    }
}

void TextEditor::handlePaste(bool removeFromStack) {
    if (clipboard.isEmpty()) return;
    std::string text = removeFromStack ? clipboard.pop() : clipboard.peek();
    model->insertText(text);
}

void TextEditor::updateCursorLocation(const Location&) {
    gtk_widget_queue_draw(drawingArea);
    triggerUIUpdate();
}

void TextEditor::updateText() {
    gtk_widget_queue_draw(drawingArea);
    triggerUIUpdate();
}

void TextEditor::updateClipboard() {
    triggerUIUpdate();
}

void TextEditor::updateUndoStatus() {
    triggerUIUpdate();
}

void TextEditor::triggerUIUpdate() {
    if (updateCallback) {
        updateCallback();
    }
}

gboolean TextEditor::on_key_pressed(GtkEventControllerKey*, guint keyval, guint, GdkModifierType state, gpointer user_data) {
    TextEditor* editor = static_cast<TextEditor*>(user_data);
    bool shift = (state & GDK_SHIFT_MASK);
    bool ctrl = (state & GDK_CONTROL_MASK);

    // Handle clipboard operations
    if (ctrl) {
        switch (keyval) {
            case GDK_KEY_c:
                editor->handleCopy();
                return GDK_EVENT_STOP;
            case GDK_KEY_x:
                editor->handleCut();
                return GDK_EVENT_STOP;
            case GDK_KEY_v:
            case GDK_KEY_V:
                editor->handlePaste(shift);
                return GDK_EVENT_STOP;
            case GDK_KEY_z:
                UndoManager::getInstance().undo();
                return GDK_EVENT_STOP;
            case GDK_KEY_y:
                UndoManager::getInstance().redo();
                return GDK_EVENT_STOP;
        }
    }

    // Handle printable characters
    if ((keyval >= 32 && keyval <= 126) || keyval == GDK_KEY_Return) {
        char c = (keyval == GDK_KEY_Return) ? '\n' : (char)keyval;
        editor->model->insertChar(c);
        return GDK_EVENT_STOP;
    }

    // Handle movement and editing keys
    switch (keyval) {
        case GDK_KEY_Left:
            editor->model->moveCursorLeft(shift);
            break;
        case GDK_KEY_Right:
            editor->model->moveCursorRight(shift);
            break;
        case GDK_KEY_Up:
            editor->model->moveCursorUp(shift);
            break;
        case GDK_KEY_Down:
            editor->model->moveCursorDown(shift);
            break;
        case GDK_KEY_BackSpace:
        case GDK_KEY_Delete: {
            auto selection = editor->model->getSelectionRange();
            if (!(selection.start == selection.end)) {
                editor->model->deleteRange(selection);
                editor->model->setSelectionRange({editor->model->getCursorLocation(), editor->model->getCursorLocation()});
            } else if (keyval == GDK_KEY_BackSpace) {
                editor->model->deleteBefore();
            } else {
                editor->model->deleteAfter();
            }
            return GDK_EVENT_STOP;
        }
        default:
            return GDK_EVENT_PROPAGATE;
    }
    return GDK_EVENT_STOP;
}
