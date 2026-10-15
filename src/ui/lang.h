#pragma once
// localisation (lang.c): TR("English text") returns the text in the current language
#define LANG_COUNT 2
extern const char *LANG_CODES[LANG_COUNT], *LANG_NAMES[LANG_COUNT];
void lang_load(const char *code);
const char *TR(const char *english);
int lang_index(void);
