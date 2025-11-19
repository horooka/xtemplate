#include "xtemplate/xtemplate.hpp"
#include "xtemplate/xtemplate_hardcoded.h"
#include <gdkmm.h>
#include <glibmm.h>
#include <gtkmm.h>
#include <iostream>

class XGtkmm3Template : public Gtk::Window {
    public:
        XGtkmm3Template(const Glib::RefPtr<Gio::Application> &app_ref)
            : config_path(std::string(getenv("HOME")) +
                          "/.config/xtemplate.ini"),
              xtemplate_file_path(std::string(getenv("HOME")) +
                                  "/.config/xtemplate.txt"),
              app(app_ref) {
            setup_gresources();
            setup_ui();
            setup_toolbar();
            setup_signals();
            setup_data();

            show_all_children();
        }

    protected:
        void show_notification(const Glib::ustring &title,
                               const Glib::ustring &message, int timeout) {
            Glib::ustring id = Glib::ustring::format(
                "notif-%lu-%d", static_cast<unsigned long>(time(nullptr)),
                rand() % 1000);
            auto notification = Gio::Notification::create(title);
            notification->set_body(message);
            notification->set_priority(Gio::NotificationPriority(0));
            Glib::signal_idle().connect([this, notification, id]() {
                if (app) {
                    app->send_notification(id, notification);
                }
                return false;
            });
            Glib::signal_timeout().connect_seconds(
                [this, id]() {
                    if (app) {
                        app->withdraw_notification(id);
                    }
                    return false;
                },
                timeout);
        }

        void render_xtemplate(const std::vector<Gtk::Entry *> &entries_vars,
                              const std::string &body, std::string &result) {
            std::string new_text = body;
            int var_num = 1;
            for (const Gtk::Entry *entry_var : entries_vars) {
                std::string replacement = entry_var->get_text();
                if (replacement.empty()) {
                    var_num++;
                    continue;
                }
                std::string placeholder = "$" + std::to_string(var_num);
                size_t pos = 0;
                while ((pos = new_text.find(placeholder, pos)) !=
                       std::string::npos) {
                    new_text.replace(pos, placeholder.length(), replacement);
                    pos += replacement.length();
                }
                var_num++;
            }
            result = new_text;
        }

        bool on_exit_clicked() {
            if (no_changes)
                return false;
            int ret = write_xtemplate_file(
                xtemplate_file_path, liststore_xtemplates, xtemplate_cols);
            if (ret == 0)
                return false;
            Gtk::MessageDialog dialog(
                *this,
                "Error with saving xtemplates: errno = " + std::to_string(ret),
                false, Gtk::MESSAGE_INFO, Gtk::BUTTONS_OK, true);
            dialog.run();
            return true;
        }

        void on_xtemplate_selection_changed() {
            Gtk::TreeModel::iterator curr_xtemplate =
                treeview_xtemplates.get_selection()->get_selected();
            if (!curr_xtemplate)
                return;
            Gtk::TreeModel::iterator true_curr_xtemplate =
                filter_xtemplates->convert_iter_to_child_iter(*curr_xtemplate);

            std::vector<std::string> tags =
                (*true_curr_xtemplate)[xtemplate_cols.tags];
            Glib::ustring tags_str;
            for (size_t i = 0; i < tags.size(); ++i) {
                if (i > 0)
                    tags_str += ", ";
                tags_str += tags[i];
            }
            label_tags.set_text(tags_str);
        }

        void on_select_xtemplates_file_clicked() {
            Gtk::FileChooserDialog dialog("Select xtemplates file",
                                          Gtk::FILE_CHOOSER_ACTION_OPEN);
            dialog.add_button("Cancel", Gtk::RESPONSE_CANCEL);
            dialog.add_button("Open", Gtk::RESPONSE_ACCEPT);
            dialog.set_default_response(Gtk::RESPONSE_ACCEPT);
            if (dialog.run() == Gtk::RESPONSE_ACCEPT) {
                xtemplate_file_path = dialog.get_filename();
                liststore_xtemplates->clear();
                (void)parse_xtemplate_content_hardcoded(
                    XTEMPLATE_CONTENT_HARDCODED, liststore_xtemplates,
                    xtemplate_cols);
                std::string errors;
                (void)parse_xtemplate_file(xtemplate_file_path,
                                           liststore_xtemplates, xtemplate_cols,
                                           errors);
                if (!errors.empty()) {
                    Gtk::MessageDialog dialog(
                        "Error parsing xtemplates file:\n\n" + errors, false,
                        Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true);
                    dialog.run();
                }
                label_xtemplates_file.set_text(xtemplate_file_path);

                last_xtemplate_file_path = xtemplate_file_path;
                errors.clear();
                int ret_update = write_config_lastpath(
                    config_path, last_xtemplate_file_path, errors);
                if (ret_update != 0) {
                    Gtk::MessageDialog dialog(
                        "Error writing config file:\n\n" + errors, false,
                        Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true);
                    dialog.run();
                }
            }
        }

        void on_open_xtemplate_clicked() {
            Gtk::TreeModel::iterator curr_xtemplate =
                treeview_xtemplates.get_selection()->get_selected();
            if (!curr_xtemplate)
                return;
            if (app_state == STATE_XTEMPLATE_OPENED) {
                stack_main.set_visible_child(frame_xtemplates);
                app_state = STATE_SEARCH;
                return;
            }
            app_state = STATE_XTEMPLATE_OPENED;

            if (textview_xtemplate_result.get_parent())
                textview_xtemplate_result.unparent();
            for (Gtk::Widget *child : vbox_xtemplate_form.get_children()) {
                vbox_xtemplate_form.remove(*child);
            }
            Gtk::TreeModel::Row true_curr_row =
                *filter_xtemplates->convert_iter_to_child_iter(*curr_xtemplate);
            std::vector<std::string> vars_types =
                true_curr_row[xtemplate_cols.vars_types];
            std::vector<std::string> vars_names =
                true_curr_row[xtemplate_cols.vars_names];
            std::vector<Gtk::Entry *> entries_vars;
            Gtk::Grid *grid_vars = Gtk::make_managed<Gtk::Grid>();
            grid_vars->set_column_spacing(10);
            grid_vars->set_row_spacing(10);
            vbox_xtemplate_form.pack_start(*grid_vars, Gtk::PACK_SHRINK);
            size_t row_idx = 0;
            for (size_t i = 0; i < vars_names.size(); ++i) {
                Gtk::Frame *frame_var_type = Gtk::make_managed<Gtk::Frame>();
                Gtk::Label *label_var_type = Gtk::make_managed<Gtk::Label>();
                set_margin(*label_var_type, 5, 0);
                frame_var_type->add(*label_var_type);
                grid_vars->attach(*frame_var_type, 0, row_idx, 1, 1);
                label_var_type->set_halign(Gtk::ALIGN_START);
                std::string var_type = vars_types[i];
                if (var_type == "PlainText")
                    label_var_type->set_name("darkgray");
                label_var_type->set_text(var_type);
                Gtk::Label *label_var_name = Gtk::make_managed<Gtk::Label>();
                grid_vars->attach(*label_var_name, 1, row_idx, 1, 1);
                label_var_name->set_halign(Gtk::ALIGN_START);
                label_var_name->set_text(vars_names[i]);
                Gtk::Entry *entry_var = Gtk::make_managed<Gtk::Entry>();
                grid_vars->attach(*entry_var, 2, row_idx, 1, 1);
                entry_var->set_text("");
                entries_vars.push_back(entry_var);
                row_idx++;
            }

            Gtk::Button *button_result = Gtk::make_managed<Gtk::Button>();
            button_result->set_label("Copy");
            vbox_xtemplate_form.pack_start(*button_result, Gtk::PACK_SHRINK);
            Gtk::Frame *frame_result = Gtk::make_managed<Gtk::Frame>();
            vbox_xtemplate_form.pack_start(*frame_result,
                                           Gtk::PACK_EXPAND_WIDGET);
            Gtk::ScrolledWindow *scrolled_result =
                Gtk::make_managed<Gtk::ScrolledWindow>();
            frame_result->add(*scrolled_result);
            scrolled_result->set_name("white_background");
            scrolled_result->add(textview_xtemplate_result);

            Glib::RefPtr<Gtk::TextBuffer> buffer =
                textview_xtemplate_result.get_buffer();
            buffer->set_text(
                static_cast<Glib::ustring>(true_curr_row[xtemplate_cols.body]));
            button_result->signal_clicked().connect([this, buffer, entries_vars,
                                                     true_curr_row]() {
                std::string result;
                render_xtemplate(entries_vars,
                                 static_cast<Glib::ustring>(
                                     true_curr_row[xtemplate_cols.body]),
                                 result);
                buffer->set_text(result);
                std::string cmd = "xclip -selection clipboard";
                FILE *pipe = popen(cmd.c_str(), "w");
                if (pipe) {
                    fwrite(result.c_str(), 1, result.size(), pipe);
                    pclose(pipe);
                    show_notification("Success",
                                      "Successfylly copied to clipboard", 2);
                }
            });
            set_margin(textview_xtemplate_result, 10, 10);
            vbox_xtemplate_form.show_all();
            stack_main.set_visible_child(vbox_xtemplate_form);
        }

        void on_new_xtemplate_clicked() {
            if (app_state == STATE_XTEMPLATE_CREATION) {
                stack_main.set_visible_child(frame_xtemplates);
                app_state = STATE_SEARCH;
                return;
            }
            app_state = STATE_XTEMPLATE_CREATION;
            for (Gtk::Widget *child : vbox_new_xtemplate_form.get_children()) {
                vbox_new_xtemplate_form.remove(*child);
            }

            Gtk::Grid *grid_name_tags = Gtk::make_managed<Gtk::Grid>();
            grid_name_tags->set_column_spacing(10);
            grid_name_tags->set_row_spacing(10);
            vbox_new_xtemplate_form.pack_start(*grid_name_tags,
                                               Gtk::PACK_SHRINK);
            Gtk::Label *label_new_name = Gtk::make_managed<Gtk::Label>();
            grid_name_tags->attach(*label_new_name, 0, 0, 1, 1);
            label_new_name->set_text("Name");
            Gtk::Entry *entry_new_name = Gtk::make_managed<Gtk::Entry>();
            grid_name_tags->attach(*entry_new_name, 1, 0, 1, 1);
            entry_new_name->set_text("");
            Gtk::Label *label_new_tags = Gtk::make_managed<Gtk::Label>();
            grid_name_tags->attach(*label_new_tags, 0, 1, 1, 1);
            label_new_tags->set_text("Tags");
            Gtk::Entry *entry_new_tags = Gtk::make_managed<Gtk::Entry>();
            grid_name_tags->attach(*entry_new_tags, 1, 1, 1, 1);
            entry_new_tags->set_tooltip_text("Tags comma-separated");
            entry_new_tags->set_text("");
            Gtk::Label *label_new_vars = Gtk::make_managed<Gtk::Label>();
            vbox_new_xtemplate_form.pack_start(*label_new_vars,
                                               Gtk::PACK_SHRINK);
            label_new_vars->set_text("Variables' types and names");
            Gtk::Grid *grid_new_vars = Gtk::make_managed<Gtk::Grid>();
            grid_new_vars->set_column_spacing(10);
            grid_new_vars->set_row_spacing(10);
            vbox_new_xtemplate_form.pack_start(*grid_new_vars,
                                               Gtk::PACK_SHRINK);
            Gtk::Button *button_new_var = Gtk::make_managed<Gtk::Button>();
            button_new_var->set_label("Append variable");
            button_new_var->set_margin_top(20);
            vbox_new_xtemplate_form.pack_start(*button_new_var,
                                               Gtk::PACK_SHRINK);
            button_new_var->signal_clicked().connect([this, grid_new_vars]() {
                int next_var_num = 0;
                while (grid_new_vars->get_child_at(1, next_var_num)) {
                    Gtk::Widget *child =
                        grid_new_vars->get_child_at(1, next_var_num);
                    if (!child)
                        break;
                    next_var_num++;
                }

                Gtk::Label *label_var_name = Gtk::make_managed<Gtk::Label>();
                grid_new_vars->attach(*label_var_name, 0, next_var_num, 1, 1);
                label_var_name->set_halign(Gtk::ALIGN_START);
                label_var_name->set_text("$" +
                                         std::to_string(next_var_num + 1));
                Gtk::Entry *entry_type = Gtk::make_managed<Gtk::Entry>();
                entry_type->set_text("");
                grid_new_vars->attach(*entry_type, 1, next_var_num, 1, 1);
                Gtk::Entry *entry_name = Gtk::make_managed<Gtk::Entry>();
                entry_name->set_text("");
                grid_new_vars->attach(*entry_name, 2, next_var_num, 1, 1);
                grid_new_vars->show_all();
            });
            Gtk::Label *label_new_body = Gtk::make_managed<Gtk::Label>();
            label_new_body->set_text("Body");
            vbox_new_xtemplate_form.pack_start(*label_new_body,
                                               Gtk::PACK_SHRINK);
            Gtk::Frame *frame_new_body = Gtk::make_managed<Gtk::Frame>();
            vbox_new_xtemplate_form.pack_start(*frame_new_body,
                                               Gtk::PACK_EXPAND_WIDGET);
            Gtk::ScrolledWindow *scrolled_new_body =
                Gtk::make_managed<Gtk::ScrolledWindow>();
            frame_new_body->add(*scrolled_new_body);
            scrolled_new_body->set_name("white_background");
            Gtk::TextView *textview_new_body =
                Gtk::make_managed<Gtk::TextView>();
            scrolled_new_body->add(*textview_new_body);
            textview_new_body->set_editable(true);

            Gtk::Button *button_create_xtemplate =
                Gtk::make_managed<Gtk::Button>();
            vbox_new_xtemplate_form.pack_start(*button_create_xtemplate,
                                               Gtk::PACK_SHRINK);
            button_create_xtemplate->set_label("Create");
            button_create_xtemplate->set_sensitive(false);
            entry_new_name->signal_changed().connect(
                [this, button_create_xtemplate, entry_new_name,
                 textview_new_body]() {
                    button_create_xtemplate->set_sensitive(
                        !entry_new_name->get_text().empty() &&
                        !textview_new_body->get_buffer()->get_text().empty());
                });
            textview_new_body->get_buffer()->signal_changed().connect(
                [this, button_create_xtemplate, entry_new_name,
                 textview_new_body]() {
                    button_create_xtemplate->set_sensitive(
                        !textview_new_body->get_buffer()->get_text().empty() &&
                        !entry_new_name->get_text().empty());
                });
            button_create_xtemplate->signal_clicked().connect(
                [this, grid_new_vars, entry_new_name, entry_new_tags,
                 textview_new_body]() {
                    std::vector<std::string> vars_types, vars_names;
                    int row = 0;
                    while (grid_new_vars->get_child_at(1, row) &&
                           grid_new_vars->get_child_at(2, row)) {
                        Gtk::Widget *child_type =
                            grid_new_vars->get_child_at(1, row);
                        Gtk::Widget *child_name =
                            grid_new_vars->get_child_at(2, row);
                        if (child_type && child_name) {
                            Gtk::Entry *entry_type =
                                dynamic_cast<Gtk::Entry *>(child_type);
                            Gtk::Entry *entry_name =
                                dynamic_cast<Gtk::Entry *>(child_name);
                            if (entry_type && entry_name) {
                                vars_types.push_back(entry_type->get_text());
                                vars_names.push_back(entry_name->get_text());
                            }
                        }
                        ++row;
                    }
                    Gtk::TreeModel::Row new_row =
                        *(liststore_xtemplates->append());
                    new_row[xtemplate_cols.name] = entry_new_name->get_text();
                    new_row[xtemplate_cols.vars_types] = vars_types;
                    new_row[xtemplate_cols.vars_names] = vars_names;
                    new_row[xtemplate_cols.tags] =
                        split_by_comma(entry_new_tags->get_text());
                    new_row[xtemplate_cols.body] =
                        textview_new_body->get_buffer()->get_text();
                    treeview_xtemplates.get_selection()->select(
                        filter_xtemplates->children().begin());
                    no_changes = false;
                    app_state = STATE_SEARCH;
                    stack_main.set_visible_child(frame_xtemplates);
                });
            set_margin(*textview_new_body, 10, 10);
            label_new_vars->set_margin_top(20);
            label_new_body->set_margin_top(20);
            vbox_new_xtemplate_form.show_all();
            stack_main.set_visible_child(vbox_new_xtemplate_form);
        }

        void on_delete_xtemplate_clicked() {
            if (app_state != STATE_SEARCH) {
                stack_main.set_visible_child(frame_xtemplates);
                app_state = STATE_SEARCH;
                return;
            }
            Gtk::TreeModel::iterator curr_xtemplate =
                treeview_xtemplates.get_selection()->get_selected();
            if (!curr_xtemplate)
                return;
            Gtk::MessageDialog dialog("Are u sure?", false,
                                      Gtk::MESSAGE_QUESTION,
                                      Gtk::BUTTONS_YES_NO, true);
            dialog.set_default_response(Gtk::RESPONSE_NO);
            if (dialog.run() == Gtk::RESPONSE_YES) {
                Gtk::TreeModel::iterator true_curr_xtemplate =
                    filter_xtemplates->convert_iter_to_child_iter(
                        *curr_xtemplate);
                liststore_xtemplates->erase(true_curr_xtemplate);
                if (liststore_xtemplates->children())
                    treeview_xtemplates.get_selection()->select(
                        filter_xtemplates->children().begin());
            }
        }

        void on_display_hardcoded_toggled() {
            display_hardcoded_xtemplates = !display_hardcoded_xtemplates;
            image_display_hardcoded_xtemplates.set(
                display_hardcoded_xtemplates
                    ? pixbuf_display_hardcoded_xtemplates
                    : pixbuf_display_hardcoded_xtemplates_disabled);
            filter_xtemplates->refilter();
        }

        void on_clear_filtering_clicked() {
            entry_search_tag.set_text("");
            if (!display_hardcoded_xtemplates)
                on_display_hardcoded_toggled();
            filter_xtemplates->refilter();
        }

        bool filter_xtemplates_by_tag_or_hardcoded(
            const Gtk::TreeModel::const_iterator &iter) {
            if (!iter)
                return false;
            if (app_state != STATE_SEARCH)
                on_open_xtemplate_clicked();

            // Tag filtering
            const std::vector<std::string> &tags = (*iter)[xtemplate_cols.tags];
            Glib::ustring search_text = entry_search_tag.get_text();
            if (search_text.empty() && !display_hardcoded_xtemplates)
                return !iter->get_value(xtemplate_cols.is_hardcoded);
            auto regex = Glib::Regex::create(",\\s*");
            std::vector<Glib::ustring> searched_tags =
                regex->split(search_text);
            for (auto &search_tag : searched_tags) {
                Glib::ustring trimmed_tag = trim(search_tag);
                if (trimmed_tag.empty())
                    continue;
                bool found = false;
                for (const auto &tag : tags) {
                    if (Glib::ustring(tag).find(trimmed_tag) !=
                        Glib::ustring::npos) {
                        found = true;
                        break;
                    }
                }
                if (!found)
                    return false;
            }

            // Hardcoded filtering
            if (!display_hardcoded_xtemplates)
                return !iter->get_value(xtemplate_cols.is_hardcoded);

            return true;
        }

    private:
        void setup_gresources() {
            pixbuf_select_xtemplates_file = Gdk::Pixbuf::create_from_resource(
                "/org/icons/select-xtemplates-file.png");
            pixbuf_open_xtemplate = Gdk::Pixbuf::create_from_resource(
                "/org/icons/open-xtemplate.png");
            pixbuf_new_xtemplate = Gdk::Pixbuf::create_from_resource(
                "/org/icons/new-xtemplate.png");
            pixbuf_delete_xtemplate = Gdk::Pixbuf::create_from_resource(
                "/org/icons/delete-xtemplate.png");
            pixbuf_display_hardcoded_xtemplates =
                Gdk::Pixbuf::create_from_resource(
                    "/org/icons/display-hardcoded-xtemplates.png");
            pixbuf_display_hardcoded_xtemplates_disabled =
                Gdk::Pixbuf::create_from_resource(
                    "/org/icons/display-hardcoded-xtemplates-disabled.png");
            pixbuf_clear_filtering = Gdk::Pixbuf::create_from_resource(
                "/org/icons/clear-filtering.png");
        }

        void setup_ui() {
            auto css = Gtk::CssProvider::create();
            css->load_from_data(R"(
                * {
                    font-family: Sans;
                    font-size: 14px;
                }
                #white_background{
                    background-image: none;
                    background-color: white;
                }
                #darkgray {
                    color: darkgray;
                }
            )");
            auto screen = get_screen();
            Gtk::StyleContext::add_provider_for_screen(
                screen, css, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

            add(vbox_main);
            vbox_main.set_hexpand(true);
            vbox_main.set_vexpand(true);
            set_size_request(800, 600);
            vbox_main.pack_start(label_xtemplates_file, Gtk::PACK_SHRINK);
            label_xtemplates_file.set_halign(Gtk::ALIGN_CENTER);
            vbox_main.pack_start(toolbar_main, Gtk::PACK_SHRINK);

            vbox_main.pack_start(stack_main, Gtk::PACK_EXPAND_WIDGET);
            stack_main.add(frame_xtemplates);
            stack_main.set_homogeneous(true);
            frame_xtemplates.add(scrolled_xtemplates);
            scrolled_xtemplates.add(treeview_xtemplates);
            treeview_xtemplates.set_headers_visible(false);
            treeview_xtemplates.append_column(treecolumn_xtemplate_name);
            treecolumn_xtemplate_name.pack_start(renderer_xtemplate_name, true);
            treecolumn_xtemplate_name.add_attribute(
                renderer_xtemplate_name.property_text(), xtemplate_cols.name);
            treecolumn_xtemplate_name.set_cell_data_func(
                renderer_xtemplate_name,
                [this](Gtk::CellRenderer *renderer,
                       const Gtk::TreeModel::iterator &iter) {
                    auto cell_text =
                        static_cast<Gtk::CellRendererText *>(renderer);
                    if (!cell_text)
                        return;

                    bool is_hardcoded = (*iter)[xtemplate_cols.is_hardcoded];
                    cell_text->property_foreground_rgba() =
                        is_hardcoded ? Gdk::RGBA("gray") : Gdk::RGBA("black");
                });

            scrolled_xtemplates.set_name("white_background");
            scrolled_xtemplates.set_hexpand(true);
            scrolled_xtemplates.set_vexpand(true);

            stack_main.add(vbox_xtemplate_form);
            textview_xtemplate_result.set_editable(false);
            stack_main.add(vbox_new_xtemplate_form);
            frame_xtemplates.show_all();
            stack_main.set_visible_child(frame_xtemplates);

            vbox_main.pack_start(hbox_bottom, false, true);
            hbox_bottom.pack_start(label_tag, false, true);
            label_tag.set_text("Tags: ");
            hbox_bottom.pack_start(label_tags, false, true);
            hbox_bottom.pack_start(entry_search_tag, true, true);
            entry_search_tag.set_placeholder_text(
                "Searched tags comma-separated");
            hbox_bottom.pack_start(button_search_tag, false, true);
            button_search_tag.set_label("Search");
            set_margin(vbox_main, 10, 10);
            set_margin(frame_xtemplates, 10, 0);
            set_margin(treeview_xtemplates, 10, 10);
            set_margin(vbox_xtemplate_form, 10, 10);
            set_margin(hbox_bottom, 10, 5);
        }

        void setup_toolbar() {
            toolbar_main.set_toolbar_style(Gtk::TOOLBAR_BOTH_HORIZ);
            toolbar_main.set_icon_size(Gtk::ICON_SIZE_SMALL_TOOLBAR);

            toolbar_main.insert(toolbutton_select_xtemplates_file, -1);
            toolbutton_select_xtemplates_file.set_tooltip_text(
                "Select xtemplates file");
            image_select_xtemplates_file =
                Gtk::Image(pixbuf_select_xtemplates_file);
            toolbutton_select_xtemplates_file.set_icon_widget(
                image_select_xtemplates_file);
            toolbar_main.insert(separator1, -1);

            toolbar_main.insert(toolbutton_open_xtemplate, -1);
            toolbutton_open_xtemplate.set_tooltip_text("Open xtemplate");
            image_open_xtemplate = Gtk::Image(pixbuf_open_xtemplate);
            toolbutton_open_xtemplate.set_icon_widget(image_open_xtemplate);
            toolbar_main.insert(toolbutton_new_xtemplate, -1);
            toolbutton_new_xtemplate.set_tooltip_text("New xtemplate");
            image_new_xtemplate = Gtk::Image(pixbuf_new_xtemplate);
            toolbutton_new_xtemplate.set_icon_widget(image_new_xtemplate);
            toolbar_main.insert(toolbutton_delete_xtemplate, -1);
            toolbutton_delete_xtemplate.set_tooltip_text("Delete xtemplate");
            image_delete_xtemplate = Gtk::Image(pixbuf_delete_xtemplate);
            toolbutton_delete_xtemplate.set_icon_widget(image_delete_xtemplate);
            toolbar_main.insert(separator2, -1);

            toolbar_main.insert(toolbutton_display_hardcoded_xtemplates, -1);
            toolbutton_display_hardcoded_xtemplates.set_tooltip_text(
                "Display hardcoded xtemplates");
            image_display_hardcoded_xtemplates =
                Gtk::Image(pixbuf_display_hardcoded_xtemplates);
            toolbutton_display_hardcoded_xtemplates.set_icon_widget(
                image_display_hardcoded_xtemplates);
            toolbar_main.insert(toolbutton_clear_filtering, -1);
            toolbutton_clear_filtering.set_tooltip_text("Clear filtering");
            image_clear_filtering = Gtk::Image(pixbuf_clear_filtering);
            toolbutton_clear_filtering.set_icon_widget(image_clear_filtering);
        }

        void setup_signals() {
            signal_delete_event().connect([this](GdkEventAny *event) {
                (void)event;
                bool ret = on_exit_clicked();
                return ret;
            });
            treeview_xtemplates.get_selection()->signal_changed().connect(
                sigc::mem_fun(
                    *this, &XGtkmm3Template::on_xtemplate_selection_changed));
            treeview_xtemplates.signal_row_activated().connect(
                [this](const Gtk::TreePath &path, Gtk::TreeViewColumn *column) {
                    (void)path;
                    (void)column;
                    on_open_xtemplate_clicked();
                });
            button_search_tag.signal_clicked().connect(
                [this]() { filter_xtemplates->refilter(); });
            toolbutton_select_xtemplates_file.signal_clicked().connect(
                [this]() { on_select_xtemplates_file_clicked(); });
            toolbutton_open_xtemplate.signal_clicked().connect(
                [this]() { on_open_xtemplate_clicked(); });
            toolbutton_new_xtemplate.signal_clicked().connect(
                [this]() { on_new_xtemplate_clicked(); });
            toolbutton_delete_xtemplate.signal_clicked().connect(
                [this]() { on_delete_xtemplate_clicked(); });
            toolbutton_display_hardcoded_xtemplates.signal_clicked().connect(
                [this]() { on_display_hardcoded_toggled(); });
            toolbutton_clear_filtering.signal_clicked().connect(
                [this]() { on_clear_filtering_clicked(); });
        }

        void setup_data() {
            liststore_xtemplates = Gtk::ListStore::create(xtemplate_cols);
            filter_xtemplates =
                Gtk::TreeModelFilter::create(liststore_xtemplates);
            filter_xtemplates->set_visible_func(sigc::mem_fun(
                *this,
                &XGtkmm3Template::filter_xtemplates_by_tag_or_hardcoded));
            treeview_xtemplates.set_model(filter_xtemplates);

            std::string errors;
            (void)parse_and_apply_config(config_path, xtemplate_file_path,
                                         last_xtemplate_file_path, errors);
            if (!errors.empty()) {
                Gtk::MessageDialog dialog(
                    "Error parsing xtemplate.ini:\n\n" + errors, false,
                    Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true);
                dialog.run();
            }
            (void)parse_xtemplate_content_hardcoded(XTEMPLATE_CONTENT_HARDCODED,
                                                    liststore_xtemplates,
                                                    xtemplate_cols);
            errors.clear();
            (void)parse_xtemplate_file(xtemplate_file_path,
                                       liststore_xtemplates, xtemplate_cols,
                                       errors);
            if (!errors.empty()) {
                Gtk::MessageDialog dialog(
                    "Error parsing xtemplates file:\n\n" + errors, false,
                    Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true);
                dialog.run();
            }
            label_xtemplates_file.set_text(xtemplate_file_path);

            if (!filter_xtemplates->children().empty())
                treeview_xtemplates.get_selection()->select(
                    filter_xtemplates->children().begin());
            on_display_hardcoded_toggled();
        }

        Gtk::Box vbox_main{Gtk::ORIENTATION_VERTICAL, 10};
        Gtk::Toolbar toolbar_main;
        Gtk::ToolButton toolbutton_select_xtemplates_file,
            toolbutton_open_xtemplate, toolbutton_new_xtemplate,
            toolbutton_delete_xtemplate,
            toolbutton_display_hardcoded_xtemplates, toolbutton_clear_filtering;
        Gtk::Stack stack_main;
        Gtk::Frame frame_xtemplates;
        Gtk::ScrolledWindow scrolled_xtemplates;
        Gtk::TreeView treeview_xtemplates;
        Gtk::TreeViewColumn treecolumn_xtemplate_name;
        Gtk::CellRendererText renderer_xtemplate_name;
        Gtk::Box vbox_xtemplate_form{Gtk::ORIENTATION_VERTICAL, 10};
        Gtk::Box vbox_new_xtemplate_form{Gtk::ORIENTATION_VERTICAL, 10};
        Gtk::TextView textview_xtemplate_result;
        Gtk::Box hbox_bottom{Gtk::ORIENTATION_HORIZONTAL, 10};
        Gtk::Button button_search_tag;
        Gtk::Entry entry_search_tag;
        Gtk::Label label_tag;
        Gtk::Label label_tags;
        Gtk::Label label_xtemplates_file;
        int app_state = STATE_SEARCH;
        std::string config_path, xtemplate_file_path,
            default_xtemplate_file_path, last_xtemplate_file_path;
        Glib::RefPtr<Gdk::Pixbuf> pixbuf_select_xtemplates_file,
            pixbuf_open_xtemplate, pixbuf_new_xtemplate,
            pixbuf_delete_xtemplate, pixbuf_display_hardcoded_xtemplates,
            pixbuf_display_hardcoded_xtemplates_disabled,
            pixbuf_clear_filtering;
        Gtk::Image image_select_xtemplates_file, image_open_xtemplate,
            image_new_xtemplate, image_delete_xtemplate,
            image_display_hardcoded_xtemplates, image_clear_filtering;
        Gtk::SeparatorToolItem separator1, separator2;
        Glib::RefPtr<Gio::Application> app;
        bool no_changes = true;
        bool display_hardcoded_xtemplates = false;

        Glib::RefPtr<Gtk::TreeModelFilter> filter_xtemplates;
        Glib::RefPtr<Gtk::ListStore> liststore_xtemplates;
        XTemplateCols xtemplate_cols;
};

int run(int argc, char *argv[]) {
    auto app = Gtk::Application::create(argc, argv, "github.x-gtkmm3-template");
    XGtkmm3Template window(app);
    return app->run(window);
}
