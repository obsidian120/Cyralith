#include "editor.h"
#include "aurorafs.h"
#include "console.h"
#include "keyboard.h"
#include "string.h"

#define EDITOR_NAME "Lumen"
#define EDITOR_VERSION "0.6"
#define EDITOR_MAX_DOCS 8
#define EDITOR_BUFFER_MAX 2048
#define EDITOR_LINE_MAX 160
#define EDITOR_PATH_MAX 96

typedef struct {
    int used;
    char name[32];
    char path[EDITOR_PATH_MAX];
    char buffer[EDITOR_BUFFER_MAX];
    size_t length;
} editor_document_t;

static editor_document_t docs[EDITOR_MAX_DOCS];
static editor_document_t* current_doc = (editor_document_t*)0;
static int active = 0;
static char line_buffer[EDITOR_LINE_MAX];
static size_t line_len = 0U;

static void editor_copy_limited(char* dst, const char* src, size_t max) {
    size_t i = 0U;
    if (max == 0U) {
        return;
    }
    while (src[i] != '\0' && i + 1U < max) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static void editor_status_bar(const char* status) {
    console_writeln("+----------------------------------------------------------+");
    console_write("| ");
    console_write(EDITOR_NAME);
    console_write(" ");
    console_write(EDITOR_VERSION);
    console_write(" | ");
    console_write(current_doc != (editor_document_t*)0 ? current_doc->path : "(no file)");
    console_putc('\n');
    console_write("| ");
    console_write(status);
    console_putc('\n');
    console_write("| Ctrl+S Save | Ctrl+Q Quit | .help Help | .show View      ");
    console_putc('\n');
    console_writeln("+----------------------------------------------------------+");
}

static void editor_prompt(void) {
    console_write("lumen:");
    console_write(current_doc->name);
    console_write("> ");
}

static void build_doc_path(const char* name, char* out, size_t max) {
    char cwd[EDITOR_PATH_MAX];
    size_t pos = 0U;
    afs_pwd(cwd, sizeof(cwd));

    if (name[0] == '/') {
        size_t i = 0U;
        while (name[i] != '\0' && pos + 1U < max) {
            out[pos++] = name[i++];
        }
        out[pos] = '\0';
        return;
    }

    for (size_t i = 0U; cwd[i] != '\0' && pos + 1U < max; ++i) {
        out[pos++] = cwd[i];
    }
    if (pos > 1U && out[pos - 1U] != '/' && pos + 1U < max) {
        out[pos++] = '/';
    }
    for (size_t i = 0U; name[i] != '\0' && pos + 1U < max; ++i) {
        out[pos++] = name[i];
    }
    out[pos] = '\0';
}

static editor_document_t* editor_find(const char* path) {
    for (size_t i = 0; i < EDITOR_MAX_DOCS; ++i) {
        if (docs[i].used != 0 && kstrcmp(docs[i].path, path) == 0) {
            return &docs[i];
        }
    }
    return (editor_document_t*)0;
}

static editor_document_t* editor_create(const char* name, const char* path) {
    for (size_t i = 0; i < EDITOR_MAX_DOCS; ++i) {
        if (docs[i].used == 0) {
            docs[i].used = 1;
            docs[i].length = 0U;
            docs[i].buffer[0] = '\0';
            editor_copy_limited(docs[i].name, name, sizeof(docs[i].name));
            editor_copy_limited(docs[i].path, path, sizeof(docs[i].path));
            return &docs[i];
        }
    }
    return (editor_document_t*)0;
}

static void editor_append_line(const char* line) {
    size_t line_size;
    size_t needed;

    if (current_doc == (editor_document_t*)0) {
        return;
    }

    line_size = kstrlen(line);
    needed = line_size + 1U;
    if (current_doc->length + needed >= EDITOR_BUFFER_MAX) {
        editor_status_bar("Not enough room left in this file.");
        return;
    }

    for (size_t i = 0; i < line_size; ++i) {
        current_doc->buffer[current_doc->length++] = line[i];
    }
    current_doc->buffer[current_doc->length++] = '\n';
    current_doc->buffer[current_doc->length] = '\0';
}

static void editor_load_from_fs(editor_document_t* doc) {
    int rc;
    if (doc == (editor_document_t*)0) {
        return;
    }
    rc = afs_read_file(doc->path, doc->buffer, sizeof(doc->buffer));
    if (rc >= 0) {
        doc->length = (size_t)rc;
        return;
    }
    doc->length = 0U;
    doc->buffer[0] = '\0';
}

void editor_show_help(void) {
    editor_status_bar("Nano-style shortcuts and simple line editing are ready.");
    console_writeln("Commands inside Lumen:");
    console_writeln("  Type text and press Enter to add a new line.");
    console_writeln("  Ctrl+S or :save / .save  -> save file to AuroraFS");
    console_writeln("  Ctrl+Q or :quit / .quit  -> close editor");
    console_writeln("  :show  or .show          -> show the current file");
    console_writeln("  :clear or .clear         -> clear the current file");
    console_writeln("  Ctrl+C                   -> cancel the current line");
    editor_prompt();
}

void editor_show_document(void) {
    editor_status_bar("Viewing current file content.");
    console_writeln("-------------------- file content --------------------");
    if (current_doc == (editor_document_t*)0 || current_doc->length == 0U) {
        console_writeln("(empty)");
    } else {
        console_write(current_doc->buffer);
        if (current_doc->buffer[current_doc->length - 1U] != '\n') {
            console_putc('\n');
        }
    }
    console_writeln("------------------------------------------------------");
}

void editor_list_documents(void) {
    console_writeln("Open notes in memory:");
    for (size_t i = 0; i < EDITOR_MAX_DOCS; ++i) {
        if (docs[i].used != 0) {
            console_write("  - ");
            console_write(docs[i].name);
            console_write(" -> ");
            console_writeln(docs[i].path);
        }
    }
}

size_t editor_document_count(void) {
    size_t count = 0U;
    for (size_t i = 0; i < EDITOR_MAX_DOCS; ++i) {
        if (docs[i].used != 0) {
            count++;
        }
    }
    return count;
}

int editor_is_active(void) {
    return active;
}

const char* editor_name(void) {
    return EDITOR_NAME;
}

void editor_open(const char* name) {
    char path[EDITOR_PATH_MAX];
    editor_document_t* doc;
    int rc;

    build_doc_path(name, path, sizeof(path));
    doc = editor_find(path);
    if (doc == (editor_document_t*)0) {
        doc = editor_create(name, path);
    }

    if (doc == (editor_document_t*)0) {
        console_writeln("[Lumen] No free note slots left.");
        return;
    }

    if (afs_exists(path) == 0) {
        rc = afs_touch(path);
        if (rc != AFS_OK) {
            console_writeln("[Lumen] Could not create file in AuroraFS.");
            return;
        }
    }

    editor_load_from_fs(doc);
    current_doc = doc;
    active = 1;
    line_len = 0U;
    line_buffer[0] = '\0';

    console_writeln("");
    editor_status_bar("Lumen opened. This view is inspired by nano.");
    editor_prompt();
}

void editor_close(void) {
    active = 0;
    line_len = 0U;
    line_buffer[0] = '\0';
    current_doc = (editor_document_t*)0;
}

static void editor_save_current(void) {
    int rc;
    rc = afs_write_file(current_doc->path, current_doc->buffer);
    if (rc == AFS_OK) {
        editor_status_bar("Saved to AuroraFS.");
    } else if (rc == AFS_ERR_DENIED) {
        editor_status_bar("Save blocked: you do not have permission here.");
    } else {
        editor_status_bar("Save failed.");
    }
}

static void editor_run_line(void) {
    line_buffer[line_len] = '\0';
    console_putc('\n');

    if (line_len == 0U) {
        editor_append_line("");
        editor_prompt();
        return;
    }

    if (line_buffer[0] == ':' || line_buffer[0] == '.') {
        if (kstrcmp(line_buffer, ":help") == 0 || kstrcmp(line_buffer, ".help") == 0) {
            editor_show_help();
        } else if (kstrcmp(line_buffer, ":show") == 0 || kstrcmp(line_buffer, ".show") == 0) {
            editor_show_document();
        } else if (kstrcmp(line_buffer, ":clear") == 0 || kstrcmp(line_buffer, ".clear") == 0) {
            current_doc->length = 0U;
            current_doc->buffer[0] = '\0';
            editor_status_bar("File cleared in memory.");
        } else if (kstrcmp(line_buffer, ":save") == 0 || kstrcmp(line_buffer, ".save") == 0) {
            editor_save_current();
        } else if (kstrcmp(line_buffer, ":quit") == 0 || kstrcmp(line_buffer, ".quit") == 0) {
            editor_status_bar("Closing Lumen.");
            editor_close();
            return;
        } else {
            editor_status_bar("Unknown editor command. Use .help or :help.");
        }
    } else {
        editor_append_line(line_buffer);
    }

    line_len = 0U;
    line_buffer[0] = '\0';
    if (editor_is_active() != 0) {
        editor_prompt();
    }
}

void editor_handle_key(int key) {
    if (active == 0) {
        return;
    }

    if (key == KEY_PAGEUP) {
        console_scroll_page_up();
        return;
    }

    if (key == KEY_PAGEDOWN) {
        console_scroll_page_down();
        return;
    }

    if (console_is_scrollback_active() != 0) {
        console_scroll_to_bottom();
        editor_prompt();
        for (size_t i = 0U; i < line_len; ++i) {
            console_putc(line_buffer[i]);
        }
    }

    if (key == KEY_CTRL_C) {
        line_len = 0U;
        line_buffer[0] = '\0';
        console_writeln("^C");
        editor_prompt();
        return;
    }

    if (key == KEY_CTRL_S) {
        editor_save_current();
        editor_prompt();
        return;
    }

    if (key == KEY_CTRL_Q) {
        editor_status_bar("Closing Lumen.");
        editor_close();
        return;
    }

    if (key == KEY_BACKSPACE) {
        if (line_len > 0U) {
            line_len--;
            line_buffer[line_len] = '\0';
            console_backspace();
        }
        return;
    }

    if (key == KEY_ENTER) {
        editor_run_line();
        return;
    }

    if (key > 0 && key < 256 && line_len < EDITOR_LINE_MAX - 1U) {
        line_buffer[line_len++] = (char)key;
        line_buffer[line_len] = '\0';
        console_putc((char)key);
    }
}
