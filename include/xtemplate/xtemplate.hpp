#include <gtkmm.h>
#include <glibmm.h>

class XTemplateCols : public Gtk::TreeModel::ColumnRecord {
    public:
        XTemplateCols() {
            add(name);
            add(is_hardcoded);
            add(vars_types);
            add(vars_names);
            add(tags);
            add(body);
        }

    Gtk::TreeModelColumn<Glib::ustring> name;
    Gtk::TreeModelColumn<bool> is_hardcoded;
    Gtk::TreeModelColumn<std::vector<std::string>> vars_types;
    Gtk::TreeModelColumn<std::vector<std::string>> vars_names;
    Gtk::TreeModelColumn<std::vector<std::string>> tags;
    Gtk::TreeModelColumn<Glib::ustring> body;
};

const int STATE_SEARCH = 0;
const int STATE_XTEMPLATE_OPENED = 1;
const int STATE_XTEMPLATE_CREATION = 2;

std::string trim(const std::string &s);
std::vector<std::string> split_by_comma(const std::string &s);
void set_margin(Gtk::Widget& widget, int margin_horizontal, int margin_vertical);
int parse_and_apply_config(const std::string &file_path,
                                     std::string &xtemplate_file_path,
                                     std::string &last_path,
                                     std::string &errors);
int write_config_lastpath(const std::string &file_path,
                          const std::string &last_path,
                          std::string &errors);
int parse_xtemplate_content_hardcoded(const std::string &content, Glib::RefPtr<Gtk::ListStore> liststore_tempalates, const XTemplateCols &xtemplate_cols);
int parse_xtemplate_file(const std::string &file_path, Glib::RefPtr<Gtk::ListStore> liststore_templates, const XTemplateCols& xtemplate_cols, std::string &errors);
int write_xtemplate_file(const std::string &file_path, const Glib::RefPtr<Gtk::ListStore> &liststore_templates, const XTemplateCols& xtemplate_cols);
int run(int argc, char *argv[]);
