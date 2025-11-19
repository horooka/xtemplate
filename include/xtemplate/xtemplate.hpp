#include "xtemplate/nodes.hpp"
#include <gtkmm.h>
#include <glibmm.h>

class XTemplateCols : public Gtk::TreeModel::ColumnRecord {
    public:
        XTemplateCols() {
            add(name);
            add(is_hardcoded);
            add(vars_types);
            add(vars_names);
            add(vars_dependent_idxs);
            add(tags);
            add(body);
        }

    Gtk::TreeModelColumn<Glib::ustring> name;
    Gtk::TreeModelColumn<bool> is_hardcoded;
    Gtk::TreeModelColumn<std::vector<std::string>> vars_types;
    Gtk::TreeModelColumn<std::vector<std::string>> vars_names;
    // Per-controller list of dependent var indexes.
    // Encoding: >=0 → enable when controller is active;
    //           <0  → enable when controller is inactive (row = -(enc+1)).
    Gtk::TreeModelColumn<std::vector<std::vector<int>>> vars_dependent_idxs;
    Gtk::TreeModelColumn<std::vector<std::string>> tags;
    Gtk::TreeModelColumn<Glib::ustring> body;
};

// Used in cli mode
struct XTemplateColsCLI {
    XTemplateColsCLI() {}

    std::string name;
    bool is_hardcoded;
    std::vector<std::string> vars_types;
    std::vector<std::string> vars_names;
    // std::vector<std::vector<int>> vars_dependent_idxs;
    std::vector<std::string> tags;
    std::string body;
};

struct XTemplateConfig {
    std::string active_path;
    std::string default_path;
    std::string last_path;
    bool render_empty_vals;
};

const int STATE_SEARCH = 0;
const int STATE_XTEMPLATE_OPENED = 1;
const int STATE_XTEMPLATE_CREATION = 2;

void set_margin(Gtk::Widget& widget, int margin_horizontal, int margin_vertical);
std::string trim(const std::string &s);
std::vector<std::string> split_by_comma(const std::string &s);
// Strip Type=deps from vars_types and fill vars_dependent_idxs.
// Soft: invalid deps are skipped and appended to errors; parsing continues.
// Returns false only on hard size mismatch.
bool finalize_vars_dependencies(std::vector<std::string> &vars_types,
                                const std::vector<std::string> &vars_names,
                                std::vector<std::vector<int>> &vars_dependent_idxs,
                                std::string &errors);
// Rebuild "Type=dep,!other" suffix from dependent index table.
std::string format_var_type_with_deps(
    const std::string &clean_type, size_t var_idx,
    const std::vector<std::string> &vars_names,
    const std::vector<std::vector<int>> &vars_dependent_idxs);
int parse_and_apply_config(const std::string &file_path,
                                     XTemplateConfig &config,
                                     std::string &errors);
int write_config_lastpath(const std::string &file_path,
                          const std::string &last_path,
                          std::string &errors);
int parse_xtemplate_content_hardcoded(const std::string &content, Glib::RefPtr<Gtk::ListStore> liststore_xtemplates, const XTemplateCols &xtemplate_cols);
int parse_xtemplate_file(const std::string &file_path, Glib::RefPtr<Gtk::ListStore> liststore_xtemplates, const XTemplateCols& xtemplate_cols, std::string &errors);
int parse_xtemplate_content_hardcoded_cli(const std::string &content, std::vector<XTemplateColsCLI> &xtemplates);
int parse_xtemplate_file_cli(const std::string &file_path, std::vector<XTemplateColsCLI> &xtemplates, std::string &errors);
int write_xtemplate_file(const std::string &file_path, const Glib::RefPtr<Gtk::ListStore> &liststore_xtemplates, const XTemplateCols& xtemplate_cols);
int run(int argc, char *argv[]);
