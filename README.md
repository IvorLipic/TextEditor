# TextEditor

A GTK4-based text editor implemented in C++17, featuring undo/redo, a multi-entry clipboard and dynamic plugin loading.

## Prerequisites

- **GTK4** (with development headers)
- **C++17** compiler (g++)
- **pkg-config** (MSYS2/MinGW64 on Windows, or native on Linux)
- **bash** — the build script uses a bash shell (MSYS2 on Windows)

## Build & Run

```bash
./TextEditor/build_and_run.sh
```

This compiles the main executable, builds plugins, copies `style.css` into the build directory, and launches the editor.

Alternatively, run `./TextEditor/build_and_run.sh` from the root of the repository — the script expects to be executed from that location.

### Manual Build

From the repository root:

```bash
g++ -std=c++17 -Wall -Wextra -g -DTEXTEDITOR_EXPORTS \
    TextEditor/main.cpp TextEditor/TextEditor.cpp \
    TextEditor/TextEditorModel.cpp TextEditor/ClipboardStack.cpp \
    TextEditor/DeleteAction.cpp TextEditor/InsertAction.cpp \
    TextEditor/UndoManager.cpp \
    -I TextEditor -I TextEditor/observers -I TextEditor/plugins \
    $(pkg-config --cflags --libs gtk4) \
    -o build/text_editor.exe
```

Then run the editor from the `build/` directory alongside `style.css` and the `plugins/` folder.

## Design Patterns

| Pattern | Key Classes | Purpose |
|---|---|---|
| **Template Method** | `TextEditor::on_draw()`, event handlers | GTK4 defines the drawing and event dispatch skeleton; `TextEditor` overrides specific steps (drawing, key handling) |
| **Observer** | `CursorObserver`, `TextObserver`, `ClipboardObserver`, `UndoObserver`; subjects: `TextEditorModel`, `ClipboardStack`, `UndoManager` | Subjects notify registered observers when state changes — cursor movement, text edits, clipboard updates, undo/redo availability |
| **Command** | `EditAction` (interface), `InsertAction`, `DeleteAction` (concrete), `UndoManager` (invoker) | Each edit is a command object with `execute_do()` / `execute_undo()`, enabling full undo/redo stack |
| **Singleton** | `UndoManager` | Private constructor, static `getInstance()`, deleted copy operations — single undo manager across the application |
| **Iterator** | `TextEditorModel::allLines()`, `linesRange()` | Uniform iteration over document lines without exposing the underlying `std::vector` |
| **Strategy** | `Plugin` (interface), `StatisticsPlugin`, `CapitalLetterPlugin` | Plugins implement the same interface but provide different algorithms; loaded dynamically at runtime |

## Architecture

| Component | Responsibility |
|---|---|
| `TextEditorModel` | Core document data, cursor position, selection range |
| `TextEditor` | GTK drawing area widget, key event handling, rendering |
| `UndoManager` | Manages undo/redo command stacks |
| `ClipboardStack` | LIFO multi-entry clipboard |
| `CursorObserver` | Notified when cursor location changes |
| `TextObserver` | Notified when document text changes |
| `ClipboardObserver` | Notified when clipboard content changes |
| `UndoObserver` | Notified when undo/redo availability changes |
| `EditAction` / `InsertAction` / `DeleteAction` | Concrete command implementations |

## Plugin System

Plugins are dynamically loaded at runtime from the `plugins/` subdirectory. Each plugin is a shared library (`.dll` / `.so` / `.dylib`) exposing two C-linkage factory functions:

```cpp
extern "C" Plugin* create_plugin();
extern "C" void    destroy_plugin(Plugin* p);
```

### Included Plugins

- **StatisticsPlugin** — displays character, word, and line counts
- **CapitalLetterPlugin** — capitalizes selected text

## Features

- File open / save (`.txt` files via GTK file dialog)
- Undo / Redo with command history
- Cut / Copy / Paste (with "Paste and Take" that removes the entry from the clipboard)
- Cursor navigation to document start / end
- Status bar showing line number, column number, and total lines
- Custom CSS styling (`style.css`)

## Acknowledgements

Course: [Oblikovni obrasci u programiranju](https://www.fer.unizg.hr/predmet/ooup)
- Plugin menu with dynamically discovered plugins
