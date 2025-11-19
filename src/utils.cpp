#include "xtemplate/xtemplate.hpp"
#include <format>
#include <fstream>
#include <gtkmm.h>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

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

// Controllers after '=' use '-' so VARS commas stay record separators,
// and so XVARIANT:a-b-c=ctrl1-!ctrl2 keeps options and deps distinct.
static std::vector<std::string> split_by_dash(const std::string &s) {
    std::vector<std::string> result;
    size_t start = 0;
    while (start <= s.size()) {
        size_t dash = s.find('-', start);
        if (dash == std::string::npos) {
            std::string tok = trim(s.substr(start));
            if (!tok.empty())
                result.push_back(tok);
            break;
        }
        std::string tok = trim(s.substr(start, dash - start));
        if (!tok.empty())
            result.push_back(tok);
        start = dash + 1;
    }
    return result;
}

bool finalize_vars_dependencies(
    std::vector<std::string> &vars_types,
    const std::vector<std::string> &vars_names,
    std::vector<std::vector<int>> &vars_dependent_idxs, std::string &errors) {
    vars_dependent_idxs.assign(vars_names.size(), {});
    if (vars_types.size() != vars_names.size()) {
        errors += "- vars types/names size mismatch\n";
        return false;
    }

    std::unordered_map<std::string, unsigned int> var_to_idx_map;
    for (size_t i = 0; i < vars_names.size(); ++i)
        var_to_idx_map[vars_names[i]] = static_cast<unsigned int>(i);

    // controller_name -> list of (dependent_idx, inverted)
    std::unordered_map<std::string, std::vector<std::pair<int, bool>>>
        controller_to_deps;

    for (size_t i = 0; i < vars_types.size(); ++i) {
        std::string &var_type = vars_types[i];
        const std::string &var_name = vars_names[i];
        size_t eq = var_type.find('=');
        if (eq == std::string::npos)
            continue;

        std::vector<std::string> deps = split_by_dash(var_type.substr(eq + 1));
        var_type = trim(var_type.substr(0, eq));

        for (std::string dep : deps) {
            dep = trim(dep);
            if (dep.empty())
                continue;
            bool inverted = false;
            if (dep[0] == '!') {
                inverted = true;
                dep = trim(dep.substr(1));
            }
            if (dep.empty()) {
                errors += std::format(
                    "- Variable {} has empty name after '!' in depends-on\n",
                    var_name);
                continue;
            }
            if (dep == var_name) {
                errors += std::format(
                    "- Variable {} cannot be dependent on itself\n", var_name);
                continue;
            }
            if (var_to_idx_map.find(dep) == var_to_idx_map.end()) {
                errors += std::format(
                    "- Variable {} is dependent on undefined variable {}\n",
                    var_name, dep);
                continue;
            }
            controller_to_deps[dep].push_back({static_cast<int>(i), inverted});
        }
    }

    for (const auto &kv : controller_to_deps) {
        unsigned int ctrl = var_to_idx_map[kv.first];
        for (const auto &dep : kv.second) {
            int enc = dep.second ? -(dep.first + 1) : dep.first;
            vars_dependent_idxs[ctrl].push_back(enc);
        }
    }
    return true;
}

std::string format_var_type_with_deps(
    const std::string &clean_type, size_t var_idx,
    const std::vector<std::string> &vars_names,
    const std::vector<std::vector<int>> &vars_dependent_idxs) {
    std::vector<std::string> dep_tokens;
    for (size_t ctrl = 0; ctrl < vars_dependent_idxs.size(); ++ctrl) {
        for (int enc : vars_dependent_idxs[ctrl]) {
            bool inverted = enc < 0;
            size_t dep_idx = inverted ? static_cast<size_t>(-(enc + 1))
                                      : static_cast<size_t>(enc);
            if (dep_idx != var_idx)
                continue;
            if (ctrl >= vars_names.size())
                continue;
            dep_tokens.push_back((inverted ? "!" : "") + vars_names[ctrl]);
        }
    }
    if (dep_tokens.empty())
        return clean_type;
    std::string out = clean_type + "=";
    for (size_t i = 0; i < dep_tokens.size(); ++i) {
        if (i)
            out += "-";
        out += dep_tokens[i];
    }
    return out;
}

void set_margin(Gtk::Widget &widget, int margin_horizontal,
                int margin_vertical) {
    widget.set_margin_top(margin_vertical);
    widget.set_margin_left(margin_horizontal);
    widget.set_margin_right(margin_horizontal);
    widget.set_margin_bottom(margin_vertical);
};

int read_file_ate(const std::string &path, std::string &out) {
    errno = 0;
    std::ifstream ifs(path, std::ios::binary | std::ios::ate);
    const int open_err = errno;
    if (!ifs.is_open())
        return open_err != 0 ? open_err : ENOENT;
    errno = 0;
    const std::streamsize sz = ifs.tellg();
    if (sz < 0)
        return errno != 0 ? errno : EIO;
    ifs.seekg(0, std::ios::beg);
    out.resize(static_cast<std::size_t>(sz));
    if (sz > 0 && !ifs.read(out.data(), sz)) {
        out.clear();
        return errno != 0 ? errno : EIO;
    }
    return 0;
}

int parse_content(const std::string &content,
                  Glib::RefPtr<Gtk::ListStore> liststore_xtemplates,
                  const XTemplateCols &xtemplate_cols, bool is_hardcoded,
                  std::string &errors) {
    std::stringstream ss_content(content);
    std::string line;
    bool xtemplate_body_parsing = false;
    while (std::getline(ss_content, line)) {
        if (line.empty())
            continue;

        Gtk::TreeModel::Row new_row = *liststore_xtemplates->append();
        new_row[xtemplate_cols.is_hardcoded] = is_hardcoded;
        new_row[xtemplate_cols.name] = line;
        const std::string template_name = line;
        while (std::getline(ss_content, line)) {
            if (line == "<TEMPLATE_BODY>") {
                xtemplate_body_parsing = true;
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
                        vars_types.push_back("");
                        vars_names.push_back(trim(var_record));
                    }
                }
                std::vector<std::vector<int>> vars_dependent_idxs;
                std::string dep_errors;
                if (!finalize_vars_dependencies(vars_types, vars_names,
                                                vars_dependent_idxs,
                                                dep_errors)) {
                    // hard mismatch — keep empty deps, still store types/names
                    vars_dependent_idxs.assign(vars_names.size(), {});
                }
                if (!dep_errors.empty())
                    errors += "In template '" + template_name +
                              "' (VARS):\n" + dep_errors;
                new_row[xtemplate_cols.vars_types] = vars_types;
                new_row[xtemplate_cols.vars_names] = vars_names;
                new_row[xtemplate_cols.vars_dependent_idxs] =
                    vars_dependent_idxs;
            }
        }
        if (!xtemplate_body_parsing)
            continue;

        std::string body;
        while (std::getline(ss_content, line)) {
            if (line.empty() && !xtemplate_body_parsing)
                break;
            if (line == "</TEMPLATE_BODY>") {
                xtemplate_body_parsing = false;
                continue;
            }
            if (!body.empty())
                body += "\n";
            body += line;
        }
        new_row[xtemplate_cols.body] = body;

        ParseDiagnostics body_diag;
        Node *root = Node::parse(body, &body_diag);
        Node::destroy(root);
        if (!body_diag.empty())
            errors += "In template '" + template_name + "' (body):\n" +
                      body_diag.join();
    }
    return 0;
}

int parse_content_cli(const std::string &content,
                      std::vector<XTemplateColsCLI> &xtemplates,
                      bool is_hardcoded, std::string &errors) {
    std::stringstream ss_content(content);
    std::string line;
    bool xtemplate_body_parsing = false;
    while (std::getline(ss_content, line)) {
        if (line.empty())
            continue;

        XTemplateColsCLI new_xtemplate;
        new_xtemplate.is_hardcoded = is_hardcoded;
        new_xtemplate.name = line;
        const std::string template_name = line;
        while (std::getline(ss_content, line)) {
            if (line == "<TEMPLATE_BODY>") {
                xtemplate_body_parsing = true;
                break;
            }
            size_t tags_specifier_pos = line.find("TAGS:");
            if (tags_specifier_pos != std::string::npos) {
                std::vector<std::string> tags =
                    split_by_comma(line.substr(tags_specifier_pos + 5));
                new_xtemplate.tags = tags;
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
                        vars_types.push_back("");
                        vars_names.push_back(trim(var_record));
                    }
                }
                std::vector<std::vector<int>> vars_dependent_idxs;
                std::string dep_errors;
                (void)finalize_vars_dependencies(vars_types, vars_names,
                                                 vars_dependent_idxs,
                                                 dep_errors);
                if (!dep_errors.empty())
                    errors += "In template '" + template_name +
                              "' (VARS):\n" + dep_errors;
                new_xtemplate.vars_types = vars_types;
                new_xtemplate.vars_names = vars_names;
            }
        }
        if (!xtemplate_body_parsing)
            continue;

        std::string body;
        while (std::getline(ss_content, line)) {
            if (line.empty() && !xtemplate_body_parsing)
                break;
            if (line == "</TEMPLATE_BODY>") {
                xtemplate_body_parsing = false;
                continue;
            }
            if (!body.empty())
                body += "\n";
            body += line;
        }
        new_xtemplate.body = body;

        ParseDiagnostics body_diag;
        Node *root = Node::parse(body, &body_diag);
        Node::destroy(root);
        if (!body_diag.empty())
            errors += "In template '" + template_name + "' (body):\n" +
                      body_diag.join();

        xtemplates.push_back(new_xtemplate);
    }
    return 0;
}

int parse_and_apply_config(const std::string &file_path,
                           XTemplateConfig &config, std::string &errors) {
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
    auto try_get_bool = [&](const std::string group_name,
                            const std::string &key) {
        try {
            if (!key_file.has_key(group_name, key))
                return false;
            return key_file.get_boolean(group_name, key);
        } catch (...) {
            return false;
        }
    };

    std::string default_path = try_get_str("Main", "DefaultPath");
    std::string last_path = try_get_str("Main", "LastPath");
    bool render_empty_vals = try_get_bool("Main", "RenderEmptyVals");

    if (!default_path.empty()) {
        config.default_path = default_path;
        config.active_path = default_path;
    }
    if (!last_path.empty()) {
        config.last_path = last_path;
        config.active_path = last_path;
    }
    config.render_empty_vals = render_empty_vals;
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
                         Glib::RefPtr<Gtk::ListStore> liststore_xtemplates,
                         const XTemplateCols &xtemplate_cols,
                         std::string &errors) {
    std::string content;
    if (const int err = read_file_ate(file_path, content); err != 0) {
        errors += std::string("- Error during opening xtemplate file: ") +
                  strerror(err) + "\n";
        return err;
    }
    return parse_content(content, liststore_xtemplates, xtemplate_cols, false,
                         errors);
}

int parse_xtemplate_content_hardcoded(
    const std::string &content,
    Glib::RefPtr<Gtk::ListStore> liststore_xtemplates,
    const XTemplateCols &xtemplate_cols) {
    std::string ignored;
    return parse_content(content, liststore_xtemplates, xtemplate_cols, true,
                         ignored);
}

int parse_xtemplate_file_cli(const std::string &file_path,
                             std::vector<XTemplateColsCLI> &xtemplates,
                             std::string &errors) {
    std::string content;
    if (const int err = read_file_ate(file_path, content); err != 0) {
        errors += std::string("- Error during opening xtemplate file: ") +
                  strerror(err) + "\n";
        return err;
    }
    return parse_content_cli(content, xtemplates, false, errors);
}

int parse_xtemplate_content_hardcoded_cli(
    const std::string &content, std::vector<XTemplateColsCLI> &xtemplates) {
    std::string ignored;
    return parse_content_cli(content, xtemplates, true, ignored);
}

int write_xtemplate_file(
    const std::string &file_path,
    const Glib::RefPtr<Gtk::ListStore> &liststore_xtemplates,
    const XTemplateCols &xtemplate_cols) {
    errno = 0;
    std::ofstream file(file_path);
    int err = errno;
    if (!file.is_open() || err != 0)
        return err;
    for (const Gtk::TreeModel::Row &row : liststore_xtemplates->children()) {
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
            std::vector<std::vector<int>> vars_dependent_idxs =
                row[xtemplate_cols.vars_dependent_idxs];
            if (vars_dependent_idxs.size() < vars_names.size())
                vars_dependent_idxs.resize(vars_names.size());
            for (size_t i = 0; i < vars_names.size(); ++i) {
                std::string type_out = format_var_type_with_deps(
                    i < vars_types.size() ? vars_types[i] : "", i, vars_names,
                    vars_dependent_idxs);
                file << type_out << "; ";
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
