#ifndef EDITOR_H
#define EDITOR_H

#include <stddef.h>

int editor_is_active(void);
const char* editor_name(void);
void editor_open(const char* name);
void editor_close(void);
void editor_handle_key(int key);
void editor_show_help(void);
void editor_show_document(void);
void editor_list_documents(void);
size_t editor_document_count(void);

#endif
