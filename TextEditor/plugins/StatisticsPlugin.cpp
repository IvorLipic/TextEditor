#include "Plugin.hpp"
#include <gtk/gtk.h>
#include <sstream>
#include <algorithm>
#include "../TextEditorModel.hpp"

class StatisticsPlugin : public Plugin {
public:
    std::string getName() const override {
        return "Statistics";
    }

    std::string getDescription() const override {
        return "Counts lines, words, and characters in the document.";
    }

    void execute(TextEditorModel& model, UndoManager&, ClipboardStack&) override {
        const auto& lines = model.getLines();
        int lineCount = lines.size();
        int wordCount = 0;
        int charCount = 0;

        for (const auto& line : lines) {
            charCount += line.length();

            std::stringstream ss(line);
            std::string word;
            while (ss >> word) {
                wordCount++;
            }
        }

        std::string message = "Document Statistics:\n";
        message += "Lines: " + std::to_string(lineCount) + "\n";
        message += "Words: " + std::to_string(wordCount) + "\n";
        message += "Characters: " + std::to_string(charCount);

        // Display in a GTK dialog
        GtkWindow* parent_window = GTK_WINDOW(gtk_application_get_active_window(GTK_APPLICATION(g_application_get_default())));
        GtkAlertDialog* alert_dialog = gtk_alert_dialog_new("%s", message.c_str());
        gtk_alert_dialog_show(alert_dialog, parent_window);
        g_object_unref(alert_dialog);
    }
};

extern "C" Plugin* create_plugin() {
    return new StatisticsPlugin();
}

extern "C" void destroy_plugin(Plugin* plugin) {
    delete plugin;
}