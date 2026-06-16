#!/bin/bash

# Define common compiler flags
CXXFLAGS="-std=c++17 -Wall -Wextra -g"
GTK_CFLAGS=$(pkg-config --cflags gtk4)
GTK_LIBS=$(pkg-config --libs gtk4)

# Output directories
BUILD_DIR="./build"
PLUGIN_DIR="${BUILD_DIR}/plugins"
mkdir -p "${BUILD_DIR}"
mkdir -p "${PLUGIN_DIR}"

# --- Copy Assets ---
echo "Copying assets..."
cp TextEditor/style.css "${BUILD_DIR}/style.css" || { echo "Failed to copy style.css!"; exit 1; }

# --- Compile Main Executable ---
echo "Compiling main executable..."
g++ ${CXXFLAGS} -DTEXTEDITOR_EXPORTS \
    TextEditor/main.cpp TextEditor/TextEditor.cpp TextEditor/TextEditorModel.cpp \
    TextEditor/ClipboardStack.cpp TextEditor/DeleteAction.cpp TextEditor/InsertAction.cpp \
    TextEditor/UndoManager.cpp \
    -o "${BUILD_DIR}/text_editor.exe" \
    -I TextEditor \
    -I TextEditor/observers \
    -I TextEditor/plugins \
    ${GTK_CFLAGS} \
    ${GTK_LIBS} \
    -lstdc++fs \
    -Wl,--out-implib,"${BUILD_DIR}/text_editor.lib" || { echo "Main executable compilation failed!"; exit 1; } # -ldl for dynamic loading on Linux/macOS, -lstdc++fs for filesystem

# --- Compile Plugins ---
echo "Compiling plugins..."

# StatisticsPlugin
g++ ${CXXFLAGS} -fPIC -shared TextEditor/plugins/StatisticsPlugin.cpp -o "${PLUGIN_DIR}/StatisticsPlugin.dll" \
    -I TextEditor \
    -I TextEditor/plugins \
    -I TextEditor/observers \
    ${GTK_CFLAGS} \
    ${GTK_LIBS} \
    -lstdc++fs \
    -L"${BUILD_DIR}" -ltext_editor || { echo "StatisticsPlugin compilation failed!"; exit 1; }

# CapitalLetterPlugin
g++ ${CXXFLAGS} -fPIC -shared TextEditor/plugins/CapitalLetterPlugin.cpp -o "${PLUGIN_DIR}/CapitalLetterPlugin.dll" \
    -I TextEditor \
    -I TextEditor/plugins \
    -I TextEditor/observers \
    ${GTK_CFLAGS} \
    ${GTK_LIBS} \
    -lstdc++fs \
    -L"${BUILD_DIR}" -ltext_editor || { echo "CapitalLetterPlugin compilation failed!"; exit 1; }


# --- Run the application ---
echo "Running TextEditor..."
cd "${BUILD_DIR}" && ./text_editor.exe & exit 0