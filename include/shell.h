#ifndef SHELL_H
#define SHELL_H

#include <string>

void print_help();
void execute_os_command(const std::string& line_str);
void shell_thread_entry(void *p1, void *p2, void *p3);
void set_default_metadata(const std::string& path, bool is_dir);

#endif // SHELL_H
