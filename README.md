App for using and creating clipboard xtemplates

# Features

- Storing of xtemplates in a file at ~/.config/xtemplate.txt
    1. Location of the default file can be changed in config
- Opportunity to use hardcoded config as include/xtemplate/xtemplate_hardcoded.h file
    1. Hardcoded xtemplates' displaying can be toggled via toolbar checkbox
- Opportunity to manage multiple xtemplate files
- Conditional blocks in xtemplate bodies via an AST (`##IF_*` / `##END`)
- Exposed rendering API via CLI mode (rendering, xtemplates listing, xtemplates' vars listing)

# Config file format

Config file is a .ini file with the following sections

| Key | Value |
| --- | ----- |
| DefaultPath | Path to fallback xtemplate file location, default is `~/.config/xtemplate.ini` |
| LastPath | Path to last used xtemplate file location, preffered over DefaultPath |
| RenderEmptyVals | If set to 'true', $varname with we replaced for even empty string, otherwise will be left as is |

# Template file format

File contains xtemplates records separated by empty lines

## Template record format

- First line is the xtemplate name
- Second line is the "TAGS:" with comma separated xtemplate's tags
- Third line is the "VARS:" with comma separated xtemplate's vars records
  - Each var record is `type; name` (semicolon-separated) with snake-case name
  - If only one word is given, it is considered as a var name with empty type
- Rest of the lines is the xtemplate's body in `"<TEMPLATE_BODY>"` tags
- Inside the body, variables are referenced by name as `$varname`

## Template vars' types

| Type | Description |
| --- | --- |
| PlainText | Conventional for actual plaintext, gray highlighting |
| XCHECKBOX | Logical variable for conditions; rendered as checkbox (`ON`/`OFF`) |
| XVARIANT:a-b-c | Choice variable for conditions; rendered as combobox with options `a`, `b`, `c` |
| XVARIANT | Same, but free-text entry when no options are listed |
| (any other) | Rendered as a text entry in the render menu |

### Depends-on syntax

Append `=controller[-...]` to the type (after any `XVARIANT:` options). Controllers are separated by `-` so commas stay reserved for the VARS record list.

Example: `XVARIANT:a-b-c=controller1-!controller2`

| Token | Meaning |
| --- | --- |
| `=var` | Enable when `var` is active / non-empty |
| `=!var` | Enable when `var` is inactive / empty |
| `=a-!b` | Multiple controllers (each updates sensitivity when it changes) |

## Conditional directives

Template's variables can be used in conditional blocks and conditional variables can be used in xtemplate too.
`$` vars' name prefix is optional here

| Directive | Meaning |
| --- | --- |
| `##IF_ON var` | Start a chain; take when checkbox var is `ON` |
| `##IF_OFF var` | Start a chain; take when checkbox var is `OFF` |
| `##IF_EQ var value` | Start a chain; take when var equals value |
| `##IF_NEQ var value` | Start a chain; take when var differs from value |
| `##IF_EMPTY var` | Start a chain; take when var is empty (equivalent to `##IF_EQ var ""`) |
| `##IF_NEMPTY var` | Start a chain; take when var is not empty (equivalent to `##IF_NEQ var ""`) |
| `##ELIF_ON/OFF/EQ/NEQ/EMPTY/NEMPTY` | Next branch in the same chain |
| `##ELSE` | Final fallback branch |
| `##END` | Closes the whole IF/ELIF/ELSE chain |

# Creating xtemplates (node canvas)

New xtemplates are built with a DrawingArea node canvas. Open/render shows serialized text.

| Action | How |
| --- | --- |
| Add block | Palette: `TEXT`, `IF_ON`, `IF_OFF`, `IF_EQ`, `IF_NEQ` |
| Nest | Select an `IF_*`, then add — or drag into its body socket |
| Reorder | Drag; green line is the snap target |
| Edit | Right-click text or condition args |
| Delete | Select + `Delete` / `Backspace` |

Declare vars in the VARS grid; use `$name` in TEXT blocks and var names as `IF_*` args.

# Cli mode

- Usage:

```
Usage: xtemplate [OPTIONS]

Options:
  -h, --help            Show this help message and exit
  --xfile PATH          Xtemplate file to use
  --xtemplate TEXT      Xtemplate to use, (default: last cached or stored default from ~/.config/xtemplate.ini accordignly to their priorities)
  --list-xtemplates     List xtemplates
  --list-vars           List vars for the choosen xtempalte
  --<var> TEXT          Set variable <var> to <value>, setting of variables is completely optional for rendering
```
