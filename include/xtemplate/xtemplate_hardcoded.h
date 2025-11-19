// Hardcoded config example
const char* XTEMPLATE_CONTENT_HARDCODED = R"(
hardcoded stdout
TAGS: hardcoded, cout, reference
VARS: std::string &;output_str
<TEMPLATE_BODY>
std::cout << $1 << std::endl;
</TEMPLATE_BODY>

hardcoded stderr
TAGS: hardcoded, cerr, reference
VARS: std::string &;output_str
<TEMPLATE_BODY>
std::cerr << $1 << std::endl;
</TEMPLATE_BODY>
)";
