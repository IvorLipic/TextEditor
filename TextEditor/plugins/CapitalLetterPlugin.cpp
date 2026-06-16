#include "Plugin.hpp"
#include <cctype>
#include "../TextEditorModel.hpp"

class CapitalLetterPlugin : public Plugin {
public:
    std::string getName() const override {
        return "Capitalize Words";
    }

    std::string getDescription() const override {
        return "Capitalizes the first letter of each word.";
    }

    void execute(TextEditorModel& model, UndoManager&, ClipboardStack&) override {
        std::vector<std::string> currentLines = model.getLines(); 

        if(currentLines.empty()) return;

        for (std::string& line : currentLines) {
            bool newWord = true;
            for (char& c : line) {
                if (std::isspace(c)) {
                    newWord = true;
                } else if (newWord) {
                    c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
                    newWord = false;
                }
            }
        }
        
        std::string capitalizedFullText = "";
        for (size_t i = 0; i < currentLines.size(); ++i) {
            capitalizedFullText += currentLines[i];
            if (i < currentLines.size() - 1) {
                capitalizedFullText += "\n";
            }
        }

        LocationRange fullDocumentRange = {{0, 0}, {(int)model.getLines().size() - 1, (int)model.getLines().back().length()}};
        model.deleteRange(fullDocumentRange);
        model.insertText(capitalizedFullText);
    }
};

extern "C" Plugin* create_plugin() {
    return new CapitalLetterPlugin();
}

extern "C" void destroy_plugin(Plugin* plugin) {
    delete plugin;
}