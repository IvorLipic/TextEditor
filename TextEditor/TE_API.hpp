#pragma once

#ifdef _WIN32
  #ifdef TEXTEDITOR_EXPORTS
    // When building the main executable
    #define TEXTEDITOR_API __declspec(dllexport)
  #else
    // When building a plugin
    #define TEXTEDITOR_API __declspec(dllimport)
  #endif
#else // Linux/macOS
  #define TEXTEDITOR_API __attribute__((visibility("default")))
#endif