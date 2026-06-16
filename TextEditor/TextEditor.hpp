#pragma once
#include <gtk/gtk.h>
#include <iostream>
#include <algorithm>
#include <functional>
#include "TextEditorModel.hpp"
#include "ClipboardStack.hpp"
#include "observers/UndoObserver.hpp"

class TextEditor : public CursorObserver, 
                   public TextObserver, 
                   public ClipboardObserver, 
                   public UndoObserver{
private:
    GtkWidget* drawingArea;
    TextEditorModel* model;
public:
    TextEditor(TextEditorModel* model);
    ~TextEditor();
    GtkWidget* widget() const { return drawingArea; }
    ClipboardStack clipboard;

    void setUpdateCallback(std::function<void()> callback) { updateCallback = callback; }

    void updateCursorLocation(const Location& loc) override;
    void updateText() override;
    void updateClipboard() override;
    void updateUndoStatus() override;

    void handleCut();
    void handlePaste(bool removeFromStack);
    void handleCopy();
private:
    static void on_draw(GtkDrawingArea* area, cairo_t* cr, int width, int height, gpointer user_data);
    static gboolean on_key_pressed(GtkEventControllerKey* controller, guint keyval, guint keycode, GdkModifierType state, gpointer user_data);
    std::function<void()> updateCallback;
    void triggerUIUpdate();

    std::string getSelectedText() const;
};
