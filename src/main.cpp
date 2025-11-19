#include "xtemplate/nodes.hpp"
#include "xtemplate/xtemplate.hpp"
#include "xtemplate/xtemplate_hardcoded.h"
#include <iostream>
#include <unordered_map>

int main(int argc, char *argv[]) {
    if (argc == 1) {
        // GUI mode
        return run(argc, argv);
    } else {
        // Cli mode
        int i = 1;
        std::string xfile;
        std::string xtemplate_name;
        bool list_xtemplates = false;
        bool list_vars = false;
        std::unordered_map<std::string, std::string> vars;
        while (i < argc) {
            std::string arg = argv[i];
            if (arg.compare("-h") == 0) {
                std::cout << "Usage: xtemplate [OPTIONS]\n\n"
                             "Options:\n"
                             "  -h, --help            Show this help message "
                             "and exit\n"
                             "  --xfile PATH          Xtemplate file to use\n"
                             "  --xtemplate TEXT      Xtemplate to use, "
                             "(default: last cached or stored default from "
                             "~/.config/xtemplate.ini accordignly to their "
                             "priorities)\n"
                             "  --list-xtemplates     List xtemplates\n"
                             "  --list-vars           List vars for the "
                             "choosen xtempalte\n"
                             "  --<var> TEXT          Set variable <var> to "
                             "<value>, setting of variables is completely "
                             "optional for rendering\n";
                return 0;
            } else if ((arg.compare("--xfile") == 0) && (i + 1 < argc)) {
                xfile = std::string(argv[++i]);
            } else if ((arg.compare("--xtemplate") == 0) && (i + 1 < argc)) {
                xtemplate_name = std::string(argv[++i]);
            } else if ((arg.compare("--list-xtemplates") == 0)) {
                list_xtemplates = true;
            } else if ((arg.compare("--list-vars") == 0)) {
                list_vars = true;
            } else if ((arg.find("--") == 0) && (i + 1 < argc)) {
                vars.insert(std::make_pair(arg.substr(2), argv[++i]));
            }
            i++;
        }
        if (xtemplate_name.empty() && !list_xtemplates) {
            std::cerr << "Use --xtemplate arg for selecting concrete xtemplate "
                         "from xtemplates file"
                      << std::endl;
            return 1;
        }
        XTemplateConfig config;
        std::string config_path =
            std::string(getenv("HOME")) + "/.config/xtemplate.ini";
        config.default_path =
            std::string(getenv("HOME")) + "/.config/xtemplate.txt";
        std::string last_path;
        std::string errors;
        int ret = parse_and_apply_config(config_path, config, errors);
        if (!errors.empty()) {
            std::cerr << "Error parsing config file: " << errors << std::endl;
            return ret;
        }
        errors.clear();
        std::vector<XTemplateColsCLI> xtemplates;
        ret = parse_xtemplate_content_hardcoded_cli(XTEMPLATE_CONTENT_HARDCODED,
                                                    xtemplates);
        ret = parse_xtemplate_file_cli(config.active_path, xtemplates, errors);
        if (!errors.empty()) {
            std::cerr << "Error parsing xtemplate file: " << errors
                      << std::endl;
            return ret;
        }
        if (list_xtemplates) {
            for (auto &xtemplate : xtemplates) {
                std::cout << xtemplate.name
                          << (xtemplate.is_hardcoded ? " (hardcoded)" : "")
                          << std::endl;
            }
            return 0;
        }
        XTemplateColsCLI matched_xtemplate;
        for (auto &xtemplate : xtemplates) {
            if (xtemplate_name == xtemplate.name) {
                matched_xtemplate = xtemplate;
                break;
            }
        }
        if (matched_xtemplate.name.empty()) {
            std::cerr << "No xtemplate with name " << xtemplate_name
                      << " found in xtemplates file" << std::endl;
            return 1;
        }
        if (list_vars) {
            const std::vector<std::string> &types =
                matched_xtemplate.vars_types;
            const std::vector<std::string> &names =
                matched_xtemplate.vars_names;
            for (size_t i = 0; i < types.size() && i < names.size(); i++) {
                std::cout << names[i] << ": " << types[i] << std::endl;
            }
            return 0;
        }
        std::string result;
        render_xtemplate(matched_xtemplate.body, vars, result, config.render_empty_vals);
        std::cout << result;
        return 0;
    }
}
