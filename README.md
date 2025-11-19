App for using and creating clipboard templates

# Features

- Storing of templates in a file at ~/.config/xtemplate.txt 
    1. Location of the default file can be changed in config
- Opportunity to hardcode config as include/xtemplate/xtemplate_hardcoded.h file
    1. Hardcoded templates' displaying can be toggled via toolbar checkbox
- Opportunity to manage multiple template files

# Config file format
Config file is a .ini file with the following sections

| Key | Value |
| --- | ----- |
| DefaultPath | Path to default template file location, used when LastLocation is not set |
| LastPath | Path to last used template file location |

# Template file format
File contains templates records separated by empty lines

## Template record format:

- First line is the template name
- Second line is the "TAGS:" with comma separated template's tags
- Third line is the "VARS:" with comma separated template's vars records
    - each var record is a colon separated var type and var name (if only one word is given, it is considered as var type)
- rest of the lines is the template's body in `"<TEMPLATE_BODY>"` tags
