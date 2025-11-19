#pragma once

// Hardcoded config example
inline const char* XTEMPLATE_CONTENT_HARDCODED = R"(
hardcoded stdout
TAGS: hardcoded, cout, reference
VARS: std::string &;output_str
<TEMPLATE_BODY>
std::cout << $output_str << std::endl;
</TEMPLATE_BODY>

hardcoded conditional stdout
TAGS: hardcoded, conditional, reference
VARS: XCHECKBOX; use_level, XVARIANT:info-warn-error=use_level; level, std::string; msg
<TEMPLATE_BODY>
##IF_ON use_level
  ##IF_NEQ level error
    ##IF_NEMPTY msg
std::cout << "[$level] " << "$msg" << std::endl;
    ##ELSE
std::cout << "[$level] without message" << std::endl;
    ##END
  ##ELSE
    ##IF_NEMPTY msg
std::cout << !!![error]!!! << "$msg" << std::endl;
    ##ELSE
std::cout << !!![error]!!! without message << std::endl;
    ##END
  ##END
##ELSE
  ##IF_NEMPTY msg
std::cout << "$msg" << std::endl;
  ##ELSE
std::cout << "Hi!" std::endl;
  ##END
##END
</TEMPLATE_BODY>
)";
