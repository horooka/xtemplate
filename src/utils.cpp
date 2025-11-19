#include "xtemplate/xtemplate.hpp"
#include <fstream>
#include <gtkmm.h>
#include <sstream>

std::string trim(const std::string &s) {
    auto start = std::find_if_not(s.begin(), s.end(), ::isspace);
    auto end = std::find_if_not(s.rbegin(), s.rend(), ::isspace).base();
    if (start >= end)
        return std::string{};
    return std::string(start, end);
}

std::vector<std::string> split_by_comma(const std::string &s) {
    std::vector<std::string> result;
    std::istringstream stream(s);
    std::string token;
    while (std::getline(stream, token, ','))
        result.push_back(trim(token));
    return result;
}

void set_margin(Gtk::Widget &widget, int margin_horizontal,
                int margin_vertical) {
    widget.set_margin_top(margin_vertical);
    widget.set_margin_left(margin_horizontal);
    widget.set_margin_right(margin_horizontal);
    widget.set_margin_bottom(margin_vertical);
};

int parse_content(const std::string &content,
                  Glib::RefPtr<Gtk::ListStore> liststore_tempalates,
                  const XTemplateCols &xtemplate_cols, bool is_hardcoded) {
    std::stringstream ss_content(content);
    std::string line;
    bool template_body_parsing = false;
    while (std::getline(ss_content, line)) {
        if (line.empty())
            continue;

        Gtk::TreeModel::Row new_row = *liststore_tempalates->append();
        new_row[xtemplate_cols.is_hardcoded] = is_hardcoded;
        new_row[xtemplate_cols.name] = line;
        while (std::getline(ss_content, line)) {
            if (line == "<TEMPLATE_BODY>") {
                template_body_parsing = true;
                break;
            }
            size_t tags_specifier_pos = line.find("TAGS:");
            if (tags_specifier_pos != std::string::npos) {
                std::vector<std::string> tags =
                    split_by_comma(line.substr(tags_specifier_pos + 5));
                new_row[xtemplate_cols.tags] = tags;
            }

            size_t vars_specifier_pos = line.find("VARS:");
            if (vars_specifier_pos != std::string::npos) {
                std::vector<std::string> vars_records =
                    split_by_comma(line.substr(vars_specifier_pos + 5));
                std::vector<std::string> vars_names, vars_types;
                for (const std::string &var_record : vars_records) {
                    size_t pos = var_record.rfind(';');
                    if (pos != std::string::npos) {
                        vars_types.push_back(trim(var_record.substr(0, pos)));
                        vars_names.push_back(trim(var_record.substr(pos + 1)));
                    } else {
                        vars_types.push_back(var_record);
                        vars_names.push_back("");
                    }
                }
                new_row[xtemplate_cols.vars_types] = vars_types;
                new_row[xtemplate_cols.vars_names] = vars_names;
            }
        }
        if (!template_body_parsing)
            continue;

        std::string body;
        while (std::getline(ss_content, line)) {
            if (line.empty() && !template_body_parsing)
                break;
            if (line == "</TEMPLATE_BODY>") {
                template_body_parsing = false;
                continue;
            }
            if (!body.empty())
                body += "\n";
            body += line;
        }
        new_row[xtemplate_cols.body] = body;
    }
    return 0;
}

int parse_and_apply_config(const std::string &file_path,
                           std::string &xtemplate_file_path,
                           std::string &last_path, std::string &errors) {
    Glib::KeyFile key_file;
    try {
        key_file.load_from_file(file_path);
    } catch (const Glib::FileError &ex) {
        errors += "- " + ex.what() + "\n";
        return ex.code();
    }
    auto try_get_str = [&](const std::string &group_name,
                           const std::string &key) {
        try {
            if (!key_file.has_key(group_name, key))
                return std::string();
            return std::string(key_file.get_string(group_name, key));
        } catch (...) {
            return std::string();
        }
    };

    std::string default_path = try_get_str("Main", "DefaultPath");
    last_path = try_get_str("Main", "LastPath");

    if (!last_path.empty())
        xtemplate_file_path = last_path;
    if (!default_path.empty())
        xtemplate_file_path = default_path;
    return 0;
}

int write_config_lastpath(const std::string &file_path,
                          const std::string &last_path, std::string &errors) {
    Glib::KeyFile key_file;
    try {
        key_file.set_string("Main", "LastPath", last_path);
    } catch (const Glib::KeyFileError &ex) {
        errors += "- " + ex.what() + "\n";
        return 1;
    }

    try {
        key_file.save_to_file(file_path);
    } catch (const Glib::FileError &ex) {
        errors += "- " + ex.what() + "\n";
        return ex.code();
    }
    return 0;
}

int parse_xtemplate_file(const std::string &file_path,
                         Glib::RefPtr<Gtk::ListStore> liststore_tempalates,
                         const XTemplateCols &xtemplate_cols,
                         std::string &errors) {
    errno = 0;
    std::ifstream file(file_path, std::ios::in);
    int err = errno;
    if (!file.is_open() || err != 0) {
        errors += std::string("- ") + strerror(err) + "\n";
        return err;
    }
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    return parse_content(content, liststore_tempalates, xtemplate_cols, false);
}

int parse_xtemplate_content_hardcoded(
    const std::string &content,
    Glib::RefPtr<Gtk::ListStore> liststore_tempalates,
    const XTemplateCols &xtemplate_cols) {
    return parse_content(content, liststore_tempalates, xtemplate_cols, true);
}

int write_xtemplate_file(
    const std::string &file_path,
    const Glib::RefPtr<Gtk::ListStore> &liststore_tempalates,
    const XTemplateCols &xtemplate_cols) {
    errno = 0;
    std::ofstream file(file_path);
    int err = errno;
    if (!file.is_open() || err != 0)
        return err;
    for (const Gtk::TreeModel::Row &row : liststore_tempalates->children()) {
        if (row[xtemplate_cols.is_hardcoded])
            continue;
        std::string name = static_cast<Glib::ustring>(row[xtemplate_cols.name]);
        file << trim(name) << "\n";

        std::vector<std::string> tags = row[xtemplate_cols.tags];
        if (!tags.empty()) {
            file << "TAGS: ";
            for (size_t i = 0; i < tags.size(); ++i) {
                file << tags[i];
                if (i + 1 < tags.size())
                    file << ", ";
            }
            file << "\n";
        }

        std::vector<std::string> vars_names = row[xtemplate_cols.vars_names];
        if (!vars_names.empty()) {
            file << "VARS: ";
            std::vector<std::string> vars_types =
                row[xtemplate_cols.vars_types];
            for (size_t i = 0; i < vars_names.size(); ++i) {
                file << vars_types[i] << "; ";
                file << vars_names[i];
                if (i + 1 < vars_names.size())
                    file << ", ";
            }
            file << "\n";
        }

        std::string body = static_cast<Glib::ustring>(row[xtemplate_cols.body]);
        file << "<TEMPLATE_BODY>\n" << trim(body) << "\n</TEMPLATE_BODY>\n\n";
    }
    return 0;
}
