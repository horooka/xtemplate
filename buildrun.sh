glib-compile-resources --target=icons_resource.c --generate-source icons.gresource.xml

g++ -g src/main.cpp src/xtemplate.cpp src/utils.cpp icons_resource.c -o xtemplate -Iinclude $(pkg-config --cflags --libs gtkmm-3.0) -Wall -Wextra && ./xtemplate
