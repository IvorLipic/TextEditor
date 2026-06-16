#include "TextEditor.hpp"
#include <fstream>
#include <filesystem>
#include "plugins/Plugin.hpp"

#ifdef _WIN32
#include <windows.h>
#define DLOPEN(path) LoadLibraryA(path)
#define DLSYM(handle, symbol) GetProcAddress((HMODULE)handle, symbol)
#define DLCLOSE(handle) FreeLibrary((HMODULE)handle)
#define PLUGIN_EXTENSION ".dll"
#else // Linux/macOS
#include <dlfcn.h>
#define DLOPEN(path) dlopen(path, RTLD_LAZY)
#define DLSYM(handle, symbol) dlsym(handle, symbol)
#define DLCLOSE(handle) dlclose(handle)
#ifdef __APPLE__
#define PLUGIN_EXTENSION ".dylib"
#else
#define PLUGIN_EXTENSION ".so"
#endif
#endif

// Function pointers for plugin factory
typedef Plugin* (*create_plugin_func)();
typedef void (*destroy_plugin_func)(Plugin*);

// Struct to hold loaded plugin info
struct LoadedPlugin {
    void* handle; // Opaque handle from dlopen/LoadLibrary
    std::unique_ptr<Plugin> plugin;
    destroy_plugin_func destroy_func; // Pointer to the destroy function
};

// Global vector to keep track of loaded plugins
std::vector<LoadedPlugin> g_loaded_plugins;

// Structure to hold application data
typedef struct {
    GtkApplication *app;
    TextEditorModel *model;
    TextEditor *editor;
    GSimpleAction *undo_action;
    GSimpleAction *redo_action;
    GSimpleAction *cut_action;
    GSimpleAction *copy_action;
    GSimpleAction *paste_action;
    GSimpleAction *paste_take_action;
    GSimpleAction *delete_action;
    GSimpleAction *clear_action;
    GSimpleAction *cursor_start_action;
    GSimpleAction *cursor_end_action;
    std::string current_file;
    GtkWidget *status_bar_label;
} AppData;

struct PluginActionData {
    AppData* app_data;
    size_t loaded_plugin_index;
};

static void load_plugins(GtkApplication *app, AppData *app_data, GMenu *plugins_menu) {
    std::filesystem::path plugin_dir = "plugins"; // Assuming a 'plugins' subdirectory
    if (!std::filesystem::exists(plugin_dir) || !std::filesystem::is_directory(plugin_dir)) {
        std::cerr << "Plugin directory not found: " << plugin_dir << std::endl;
        return;
    }

    for (const auto& entry : std::filesystem::directory_iterator(plugin_dir)) {
        if (entry.is_regular_file() && entry.path().extension() == PLUGIN_EXTENSION) {
            std::string plugin_path = entry.path().string();
            std::cout << "Attempting to load plugin: " << plugin_path << std::endl;

            void* handle = DLOPEN(plugin_path.c_str());
            if (!handle) {
                std::cerr << "Failed to load library " << plugin_path << std::endl;
                continue;
            }

            create_plugin_func create_func = reinterpret_cast<create_plugin_func>(
                                             reinterpret_cast<void*>(DLSYM(handle, "create_plugin")));
            destroy_plugin_func destroy_func = reinterpret_cast<destroy_plugin_func>(
                                               reinterpret_cast<void*>(DLSYM(handle, "destroy_plugin")));

            if (!create_func || !destroy_func) {
                std::cerr << "Failed to find create_plugin or destroy_plugin in " << plugin_path << std::endl;
                DLCLOSE(handle);
                continue;
            }

            Plugin* plugin_raw = create_func();
            if (plugin_raw) {
                std::cout << "Loaded plugin: " << plugin_raw->getName() << std::endl;
                g_loaded_plugins.push_back({handle, std::unique_ptr<Plugin>(plugin_raw), destroy_func});

                PluginActionData* action_data = new PluginActionData();
                action_data->app_data = app_data;
                action_data->loaded_plugin_index = g_loaded_plugins.size() - 1;

                static std::vector<std::unique_ptr<PluginActionData>> g_plugin_action_datas; // Use unique_ptr for safety
                g_plugin_action_datas.push_back(std::unique_ptr<PluginActionData>(action_data));

                // Add plugin to menu
                std::string action_name = "app.plugin_" + plugin_raw->getName(); // Unique action name
                // Replace spaces with underscores for action name, or use a hash
                std::replace(action_name.begin(), action_name.end(), ' ', '_');

                GSimpleAction *plugin_action = g_simple_action_new(action_name.c_str() + 4, NULL); // +4 to skip "app."
                g_signal_connect(plugin_action, "activate", G_CALLBACK(+[](GSimpleAction*, GVariant*, gpointer data) {
                    // Cast data back to our helper struct
                    PluginActionData* pad = static_cast<PluginActionData*>(data);
                    LoadedPlugin& current_lp = g_loaded_plugins[pad->loaded_plugin_index];
                    current_lp.plugin->execute(
                        *(pad->app_data->model),
                        UndoManager::getInstance(),
                        pad->app_data->editor->clipboard
                    );
                }), action_data); // Pass a pointer to the LoadedPlugin in the vector
                g_action_map_add_action(G_ACTION_MAP(app), G_ACTION(plugin_action));

                g_menu_append(plugins_menu, plugin_raw->getName().c_str(), action_name.c_str());
            } else {
                DLCLOSE(handle);
            }
        }
    }
}

static void open_response_callback(GObject *source_object, GAsyncResult *result, gpointer user_data) {
    AppData *app_data = (AppData *)user_data;
    GtkFileDialog *dialog = GTK_FILE_DIALOG(source_object);
    
    GFile *file = gtk_file_dialog_open_finish(dialog, result, NULL);
    if (file) {
        char *path = g_file_get_path(file);
        
        // Read the file content
        std::ifstream input_file(path);
        if (input_file.is_open()) {
            std::string content((std::istreambuf_iterator<char>(input_file)), 
                std::istreambuf_iterator<char>());
            
            // Update the model with new content
            LocationRange fullDocumentRange;
            const auto& current_model_lines = app_data->model->getLines();
            if (current_model_lines.empty()) {
                fullDocumentRange = {{0,0}, {0,0}};
            } else {
                fullDocumentRange = {{0, 0},
                                     {(int)current_model_lines.size() - 1,
                                      (int)current_model_lines.back().size()}};
            }
            UndoManager::getInstance().setInUndoRedo(true);
            app_data->model->deleteRange(fullDocumentRange);
            app_data->model->insertText(content);
            UndoManager::getInstance().setInUndoRedo(false);
            UndoManager::getInstance().clearRedoStack();
            UndoManager::getInstance().clearUndoStack();
            app_data->current_file = path;
            
            // Update window title
            GtkWindow *window = GTK_WINDOW(gtk_application_get_active_window(app_data->app));
            std::string title = "GTK4 Text Editor - " + std::string(g_file_get_basename(file));
            gtk_window_set_title(window, title.c_str());
            
            input_file.close();
        }
        
        g_free(path);
        g_object_unref(file);
    }
}

static void save_response_callback(GObject *source_object, GAsyncResult *result, gpointer user_data) {
    AppData *app_data = (AppData *)user_data;
    GtkFileDialog *dialog = GTK_FILE_DIALOG(source_object);
    
    GFile *file = gtk_file_dialog_save_finish(dialog, result, NULL);
    if (file) {
        char *path = g_file_get_path(file);
        
        // Ensure the file has .txt extension
        std::string file_path(path);
        if (file_path.size() < 4 || file_path.substr(file_path.size() - 4) != ".txt") {
            file_path += ".txt";
            GFile *new_file = g_file_new_for_path(file_path.c_str());
            g_object_unref(file);
            file = new_file;
            g_free(path);
            path = g_file_get_path(file);
        }
        
        // Save the content
        std::ofstream output_file(file_path);
        if (output_file.is_open()) {
            const auto& lines = app_data->model->getLines();
            for (const auto& line : lines) {
                output_file << line << "\n";
            }
            output_file.close();
            
            app_data->current_file = file_path;
            
            // Update window title
            GtkWindow *window = GTK_WINDOW(gtk_application_get_active_window(app_data->app));
            std::string title = "GTK4 Text Editor - " + std::string(g_file_get_basename(file));
            gtk_window_set_title(window, title.c_str());
        }
        
        g_free(path);
        g_object_unref(file);
    }
}

// Action handlers
static void on_open(GSimpleAction*, GVariant*, gpointer user_data) {
    AppData *app_data = (AppData *)user_data;
    GtkWindow *window = GTK_WINDOW(gtk_application_get_active_window(app_data->app));

    // Create a file dialog
    GtkFileDialog *dialog = gtk_file_dialog_new();  
    gtk_file_dialog_set_title(dialog, "Open File");
    
    // Create and add filter
    GtkFileFilter *filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "Text Files");
    gtk_file_filter_add_pattern(filter, "*.txt");

    // Create list store and add filter
    GListStore *filters_store = g_list_store_new(GTK_TYPE_FILE_FILTER);
    g_list_store_append(filters_store, filter);
    
    // Convert to GListModel interface
    GListModel *filters = G_LIST_MODEL(filters_store);
    gtk_file_dialog_set_filters(dialog, filters);

    g_object_unref(filters_store);
    
    gtk_file_dialog_open(dialog, window, NULL, (GAsyncReadyCallback)open_response_callback, app_data);
}

static void on_save(GSimpleAction*, GVariant*, gpointer user_data) {
    AppData *app_data = (AppData *)user_data;
    GtkWindow *window = GTK_WINDOW(gtk_application_get_active_window(app_data->app));

    if (!app_data->current_file.empty()) {
        // Save to current file
        std::ofstream output_file(app_data->current_file);
        if (output_file.is_open()) {
            const auto& lines = app_data->model->getLines();
            for (const auto& line : lines) {
                output_file << line << "\n";
            }
            output_file.close();
        }
    } else {
        GtkFileDialog *dialog = gtk_file_dialog_new();
        gtk_file_dialog_set_title(dialog, "Save File");
        
        GtkFileFilter *filter = gtk_file_filter_new();
        gtk_file_filter_set_name(filter, "Text Files");
        gtk_file_filter_add_pattern(filter, "*.txt");

        GListStore *filters_store = g_list_store_new(GTK_TYPE_FILE_FILTER);
        g_list_store_append(filters_store, filter);
        
        GListModel *filters = G_LIST_MODEL(filters_store);
        gtk_file_dialog_set_filters(dialog, filters);

        g_object_unref(filters_store);
        
        gtk_file_dialog_set_initial_name(dialog, "untitled.txt");
        gtk_file_dialog_save(dialog, window, NULL, (GAsyncReadyCallback)save_response_callback, app_data);
    }
}

static void on_exit(GSimpleAction*, GVariant*, gpointer user_data) {
    AppData *app_data = (AppData *)user_data;
    g_application_quit(G_APPLICATION(app_data->app));
}

static void on_undo(GSimpleAction*, GVariant*, gpointer) {
    UndoManager::getInstance().undo();
}

static void on_redo(GSimpleAction*, GVariant*, gpointer) {
    UndoManager::getInstance().redo();
}

static void on_cut(GSimpleAction*, GVariant*, gpointer user_data) {
    AppData *app_data = (AppData *)user_data;
    app_data->editor->handleCut();
}

static void on_copy(GSimpleAction*, GVariant*, gpointer user_data) {
    AppData *app_data = (AppData *)user_data;
    app_data->editor->handleCopy();
}

static void on_paste(GSimpleAction*, GVariant*, gpointer user_data) {
    AppData *app_data = (AppData *)user_data;
    app_data->editor->handlePaste(false);
}

static void on_paste_take(GSimpleAction*, GVariant*, gpointer user_data) {
    AppData *app_data = (AppData *)user_data;
    app_data->editor->handlePaste(true);
}

static void on_delete(GSimpleAction*, GVariant*, gpointer user_data) {
    AppData *app_data = (AppData *)user_data;
    app_data->model->deleteRange(app_data->model->getSelectionRange());
}

static void on_clear(GSimpleAction*, GVariant*, gpointer user_data) {
    AppData *app_data = (AppData *)user_data;
    const auto& lines = app_data->model->getLines();
    Location startOfDocument = Location({0, 0});
    Location endOfDocument = Location({
        (int)lines.size() - 1,
        (int)lines[lines.size() - 1].size()
    });
    app_data->model->deleteRange(LocationRange({startOfDocument, endOfDocument}));
}

static void on_cursor_start(GSimpleAction*, GVariant*, gpointer user_data) {
    AppData *app_data = (AppData *)user_data;
    app_data->model->setCursorLocation({0, 0});
    gtk_widget_queue_draw(app_data->editor->widget());
}

static void on_cursor_end(GSimpleAction*, GVariant*, gpointer user_data) {
    AppData *app_data = (AppData *)user_data;
    const auto& lines = app_data->model->getLines();
    int lastRow = lines.size() - 1;
    int lastCol = lines[lastRow].size();
    app_data->model->setCursorLocation({lastRow, lastCol});
    gtk_widget_queue_draw(app_data->editor->widget());
}

// Update the status bar
void update_status_bar(AppData *app_data) {
    if (!app_data || !app_data->model || !app_data->status_bar_label) {
        return;
    }

    Location cursor_loc = app_data->model->getCursorLocation();
    size_t total_lines = app_data->model->getLines().size();

    std::string status_text = "Line: " + std::to_string(cursor_loc.row + 1) +
                              ", Col: " + std::to_string(cursor_loc.col + 1) +
                              " | Lines: " + std::to_string(total_lines);

    gtk_label_set_text(GTK_LABEL(app_data->status_bar_label), status_text.c_str());
}

// Update UI state based on model changes
void update_ui_state(AppData *app_data) {
    LocationRange selectionRange = app_data->model->getSelectionRange();
    bool has_selection = !(selectionRange.start == selectionRange.end);
    bool can_undo = UndoManager::getInstance().canUndo();
    bool can_redo = UndoManager::getInstance().canRedo();
    bool can_paste = !app_data->editor->clipboard.isEmpty();

    g_simple_action_set_enabled(app_data->undo_action, can_undo);
    g_simple_action_set_enabled(app_data->redo_action, can_redo);
    g_simple_action_set_enabled(app_data->cut_action, has_selection);
    g_simple_action_set_enabled(app_data->copy_action, has_selection);
    g_simple_action_set_enabled(app_data->paste_action, can_paste);
    g_simple_action_set_enabled(app_data->paste_take_action, can_paste);
    g_simple_action_set_enabled(app_data->delete_action, has_selection);
    g_simple_action_set_enabled(app_data->clear_action, true);
    g_simple_action_set_enabled(app_data->cursor_start_action, true);
    g_simple_action_set_enabled(app_data->cursor_end_action, true);

    update_status_bar(app_data);
}

// Callback for window close
static gboolean on_window_close(GtkWindow*, gpointer) {
    return FALSE;  // Let GTK destroy the window
}

// Callback when application is activated
static void app_activate(GApplication *app, gpointer user_data) {
    AppData *app_data = (AppData *)user_data;
    
    std::string initText = "Hello, World!\nA long long long long long long line.\nAnother line.";
    app_data->model = new TextEditorModel(initText);
    app_data->editor = new TextEditor(app_data->model);

    // Connect editor to UI updates
    app_data->editor->setUpdateCallback([app_data]() {
        update_ui_state(app_data);
    });

    GtkWidget* window = gtk_application_window_new(app_data->app);
    gtk_window_set_default_size(GTK_WINDOW(window), 800, 600);
    gtk_window_set_title(GTK_WINDOW(window), "GTK4 Text Editor");

    // Create main vertical box
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_window_set_child(GTK_WINDOW(window), vbox);

    // Create menu bar
    GMenu *menubar = g_menu_new();
    GMenu *file_menu = g_menu_new();
    GMenu *edit_menu = g_menu_new();
    GMenu *move_menu = g_menu_new();
    GMenu *plugins_menu = g_menu_new();

    g_menu_append(file_menu, "Open", "app.open");
    g_menu_append(file_menu, "Save", "app.save");
    g_menu_append(file_menu, "Exit", "app.exit");
    g_menu_append_submenu(menubar, "File", G_MENU_MODEL(file_menu));

    g_menu_append(edit_menu, "Undo", "app.undo");
    g_menu_append(edit_menu, "Redo", "app.redo");
    g_menu_append(edit_menu, "Cut", "app.cut");
    g_menu_append(edit_menu, "Copy", "app.copy");
    g_menu_append(edit_menu, "Paste", "app.paste");
    g_menu_append(edit_menu, "Paste and Take", "app.paste_take");
    g_menu_append(edit_menu, "Delete Selection", "app.delete");
    g_menu_append(edit_menu, "Clear Document", "app.clear");
    g_menu_append_submenu(menubar, "Edit", G_MENU_MODEL(edit_menu));

    g_menu_append(move_menu, "Cursor to Document Start", "app.cursor_start");
    g_menu_append(move_menu, "Cursor to Document End", "app.cursor_end");
    g_menu_append_submenu(menubar, "Move", G_MENU_MODEL(move_menu));

    g_menu_append_submenu(menubar, "Plugins", G_MENU_MODEL(plugins_menu));

    GtkWidget *menu_bar = gtk_popover_menu_bar_new_from_model(G_MENU_MODEL(menubar));
    gtk_box_append(GTK_BOX(vbox), menu_bar);

     // Load plugins
    load_plugins(GTK_APPLICATION(app), app_data, plugins_menu);

    // Create toolbar
    GtkWidget *toolbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_add_css_class(toolbar, "toolbar");

    auto create_tool_button = [](const gchar *icon_name, const gchar *action) {
        GtkWidget *button = gtk_button_new_from_icon_name(icon_name);
        gtk_actionable_set_action_name(GTK_ACTIONABLE(button), action);
        gtk_widget_set_tooltip_text(button, action + 4); // Skip "app."
        return button;
    };

    gtk_box_append(GTK_BOX(toolbar), create_tool_button("edit-undo-symbolic", "app.undo"));
    gtk_box_append(GTK_BOX(toolbar), create_tool_button("edit-redo-symbolic", "app.redo"));
    gtk_box_append(GTK_BOX(toolbar), gtk_separator_new(GTK_ORIENTATION_VERTICAL));
    gtk_box_append(GTK_BOX(toolbar), create_tool_button("edit-cut-symbolic", "app.cut"));
    gtk_box_append(GTK_BOX(toolbar), create_tool_button("edit-copy-symbolic", "app.copy"));
    gtk_box_append(GTK_BOX(toolbar), create_tool_button("edit-paste-symbolic", "app.paste"));

    gtk_box_append(GTK_BOX(vbox), toolbar);

    GtkWidget *scrolled_window = gtk_scrolled_window_new();
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled_window), app_data->editor->widget());
    gtk_box_set_homogeneous(GTK_BOX(vbox), FALSE);
    gtk_box_append(GTK_BOX(vbox), scrolled_window);
    gtk_widget_set_vexpand(scrolled_window, TRUE);
    gtk_widget_set_hexpand(scrolled_window, TRUE);

    // Create status bar
    app_data->status_bar_label = gtk_label_new("Line: 1, Col: 1 | Lines: 1");
    gtk_widget_set_halign(app_data->status_bar_label, GTK_ALIGN_START);
    gtk_widget_set_margin_start(app_data->status_bar_label, 10);
    gtk_widget_set_margin_end(app_data->status_bar_label, 10);
    gtk_widget_set_margin_bottom(app_data->status_bar_label, 5);
    gtk_box_append(GTK_BOX(vbox), app_data->status_bar_label);

    g_signal_connect(window, "close-request", G_CALLBACK(on_window_close), NULL);
    
    gtk_window_present(GTK_WINDOW(window));
    update_ui_state(app_data); // Initial UI state

    gtk_widget_grab_focus(app_data->editor->widget());

    // Initial status bar update
    update_status_bar(app_data);
}

// Callback when application starts up
static void app_startup(GApplication *app, gpointer user_data) {
    AppData *app_data = (AppData *)user_data;

    // Define actions
    const GActionEntry app_entries[] = {
        {"open", on_open, NULL, NULL, NULL, {0,0,0}},
        {"save", on_save, NULL, NULL, NULL, {0,0,0}},
        {"exit", on_exit, NULL, NULL, NULL, {0,0,0}},
        {"undo", on_undo, NULL, NULL, NULL, {0,0,0}},
        {"redo", on_redo, NULL, NULL, NULL, {0,0,0}},
        {"cut", on_cut, NULL, NULL, NULL, {0,0,0}},
        {"copy", on_copy, NULL, NULL, NULL, {0,0,0}},
        {"paste", on_paste, NULL, NULL, NULL, {0,0,0}},
        {"paste_take", on_paste_take, NULL, NULL, NULL, {0,0,0}},
        {"delete", on_delete, NULL, NULL, NULL, {0,0,0}},
        {"clear", on_clear, NULL, NULL, NULL, {0,0,0}},
        {"cursor_start", on_cursor_start, NULL, NULL, NULL, {0,0,0}},
        {"cursor_end", on_cursor_end, NULL, NULL, NULL, {0,0,0}}
    };

    g_action_map_add_action_entries(G_ACTION_MAP(app), app_entries, G_N_ELEMENTS(app_entries), app_data);

    // Get references to actions for state management
    app_data->undo_action = G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(app), "undo"));
    app_data->redo_action = G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(app), "redo"));
    app_data->cut_action = G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(app), "cut"));
    app_data->copy_action = G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(app), "copy"));
    app_data->paste_action = G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(app), "paste"));
    app_data->paste_take_action = G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(app), "paste_take"));
    app_data->delete_action = G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(app), "delete"));
    app_data->clear_action = G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(app), "clear"));
    app_data->cursor_start_action = G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(app), "cursor_start"));
    app_data->cursor_end_action = G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(app), "cursor_end"));

    // Load CSS
    GtkCssProvider *provider = gtk_css_provider_new();
    gtk_css_provider_load_from_path(provider, "style.css");
    gtk_style_context_add_provider_for_display(
        gdk_display_get_default(),
        GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
    );
}

// Callback when application shuts down
static void app_shutdown(GApplication*, gpointer user_data) {
    AppData *app_data = (AppData *)user_data;
    // Clean up resources
    delete app_data->editor;
    delete app_data->model;

    // Unload plugins
    for (auto& lp : g_loaded_plugins) {
        if (lp.plugin && lp.destroy_func) {
            lp.destroy_func(lp.plugin.release()); // Release unique_ptr, pass raw pointer to destroy func
        }
        if (lp.handle) {
            DLCLOSE(lp.handle);
        }
    }
    g_loaded_plugins.clear(); // Clear the vector after unloading

    free(app_data);
}

int main(int argc, char *argv[]) {
    // Allocate application data
    AppData *app_data = (AppData *)g_malloc0(sizeof(AppData));
    
    // Create the application
    app_data->app = gtk_application_new("com.ivor.oop.texteditor", G_APPLICATION_DEFAULT_FLAGS);
    
    // Connect signals
    g_signal_connect(app_data->app, "activate", G_CALLBACK(app_activate), app_data);
    g_signal_connect(app_data->app, "startup", G_CALLBACK(app_startup), app_data);
    g_signal_connect(app_data->app, "shutdown", G_CALLBACK(app_shutdown), app_data);
    
    // Run the application
    int status = g_application_run(G_APPLICATION(app_data->app), argc, argv);
    
    // The application object will be automatically freed when the last reference is dropped
    return status;
}
