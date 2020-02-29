#pragma once

#include <stdbool.h>

void strip_whitespace(char *str);
void strip_quotes(char *str);

// strcat that does nothing if dest or src is NULL
char *lenient_strcat(char *dest, const char *src);
char *lenient_strncat(char *dest, const char *src, size_t len);

// strcmp that also handles null pointers.
int lenient_strcmp(char *a, char *b);

// Splits an argument string, keeping quotes intact
char **split_args(const char *str, int *argc);
void free_argv(int argc, char **argv);

int unescape_string(char *string);
char *join_args(char **argv, int argc);

// Split string into 2 by delim, handle quotes
char *argsep(char **stringp, const char *delim, char *matched_delim);

// Expand a path using shell replacements such as $HOME and ~
bool expand_path(char **path);
