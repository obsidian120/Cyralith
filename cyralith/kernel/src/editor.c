#include "editor.h"
#include "cyralithfs.h"
#include "console.h"
#include "keyboard.h"
#include "string.h"

#define EDITOR_NAME "Lumen"
#define EDITOR_VERSION "1.2"
#define EDITOR_MAX_DOCS 8
#define EDITOR_BUFFER_MAX 2048
#define EDITOR_PATH_MAX 96
#define EDITOR_SCREEN_ROWS 25U
#define EDITOR_TEXT_TOP 1U
#define EDITOR_TEXT_ROWS 21U
#define EDITOR_TEXT_WIDTH 74U
#define EDITOR_GUTTER_WIDTH 5U
#define EDITOR_STATUS_MAX 78U
#define EDITOR_PROMPT_MAX 48U

typedef struct {
    int used;
    char name[32];
    char path[EDITOR_PATH_MAX];
    char buffer[EDITOR_BUFFER_MAX];
    size_t length;
} editor_document_t;

typedef enum {
    EDITOR_PROMPT_NONE = 0,
    EDITOR_PROMPT_FIND,
    EDITOR_PROMPT_GOTO
} editor_prompt_mode_t;

static editor_document_t docs[EDITOR_MAX_DOCS];
static editor_document_t* current_doc = (editor_document_t*)0;
static int active = 0;
static size_t cursor_index = 0U;
static size_t viewport_line = 0U;
static size_t preferred_col = 0U;
static int dirty = 0;
static int quit_armed = 0;
static editor_prompt_mode_t prompt_mode = EDITOR_PROMPT_NONE;
static char status_text[EDITOR_STATUS_MAX + 1U];
static char prompt_input[EDITOR_PROMPT_MAX + 1U];
static size_t prompt_len = 0U;

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

static void append_char_bounded(char* dst, size_t* pos, size_t max, char ch) {
    if (*pos + 1U >= max) {
        return;
    }
    dst[*pos] = ch;
    (*pos)++;
    dst[*pos] = '\0';
}

static void append_text_bounded(char* dst, size_t* pos, size_t max, const char* src) {
    while (*src != '\0' && *pos + 1U < max) {
        dst[*pos] = *src;
        (*pos)++;
        src++;
    }
    dst[*pos] = '\0';
}

static void append_dec_bounded(char* dst, size_t* pos, size_t max, uint32_t value) {
    char buf[16];
    size_t len = 0U;
    if (value == 0U) {
        append_char_bounded(dst, pos, max, '0');
        return;
    }
    while (value > 0U && len < sizeof(buf)) {
        buf[len++] = (char)('0' + (value % 10U));
        value /= 10U;
    }
    while (len > 0U) {
        append_char_bounded(dst, pos, max, buf[--len]);
    }
}

static int editor_is_visible_char(char ch) {
    if (ch >= 32 && ch <= 126) {
        return 1;
    }
    return 0;
}

static void editor_normalize_loaded_text(editor_document_t* doc) {
    size_t src = 0U;
    size_t dst = 0U;
    int has_visible = 0;

    if (doc == (editor_document_t*)0) {
        return;
    }

    while (src < doc->length && dst + 1U < EDITOR_BUFFER_MAX) {
        char ch = doc->buffer[src++];
        if (ch == '\r') {
            continue;
        }
        if (ch == '\n' || ch == '\t') {
            doc->buffer[dst++] = ch;
            continue;
        }
        if (editor_is_visible_char(ch) != 0) {
            if (ch != ' ') {
                has_visible = 1;
            }
            doc->buffer[dst++] = ch;
        }
    }

    while (dst > 0U && doc->buffer[dst - 1U] == ' ') {
        dst--;
    }

    if (has_visible == 0) {
        dst = 0U;
    }

    doc->length = dst;
    doc->buffer[dst] = '\0';
}

static void write_padded_line(const char* text) {
    size_t i = 0U;
    while (text[i] != '\0' && i < 80U) {
        console_putc(text[i]);
        i++;
    }
    while (i < 80U) {
        console_putc(' ');
        i++;
    }
    console_putc('\n');
}

static int char_equal_ci(char a, char b) {
    if (a >= 'A' && a <= 'Z') {
        a = (char)(a + ('a' - 'A'));
    }
    if (b >= 'A' && b <= 'Z') {
        b = (char)(b + ('a' - 'A'));
    }
    return a == b;
}

static size_t editor_total_lines(void) {
    size_t lines = 1U;
    if (current_doc == (editor_document_t*)0) {
        return 1U;
    }
    for (size_t i = 0U; i < current_doc->length; ++i) {
        if (current_doc->buffer[i] == '\n') {
            lines++;
        }
    }
    return lines;
}

static void editor_line_col_from_index(size_t index, size_t* line_out, size_t* col_out) {
    size_t line = 0U;
    size_t col = 0U;
    if (current_doc == (editor_document_t*)0) {
        *line_out = 0U;
        *col_out = 0U;
        return;
    }
    if (index > current_doc->length) {
        index = current_doc->length;
    }
    for (size_t i = 0U; i < index; ++i) {
        if (current_doc->buffer[i] == '\n') {
            line++;
            col = 0U;
        } else {
            col++;
        }
    }
    *line_out = line;
    *col_out = col;
}

static size_t editor_index_from_line_col(size_t target_line, size_t target_col) {
    size_t line = 0U;
    size_t col = 0U;
    size_t i = 0U;
    if (current_doc == (editor_document_t*)0) {
        return 0U;
    }

    while (i < current_doc->length && line < target_line) {
        if (current_doc->buffer[i] == '\n') {
            line++;
        }
        i++;
    }

    while (i < current_doc->length && current_doc->buffer[i] != '\n' && col < target_col) {
        i++;
        col++;
    }
    return i;
}

static size_t editor_line_start_index(size_t target_line) {
    return editor_index_from_line_col(target_line, 0U);
}

static void editor_set_status(const char* text) {
    editor_copy_limited(status_text, text, sizeof(status_text));
}

static void editor_ensure_cursor_visible(void) {
    size_t line = 0U;
    size_t col = 0U;
    editor_line_col_from_index(cursor_index, &line, &col);
    preferred_col = col;
    if (line < viewport_line) {
        viewport_line = line;
    } else if (line >= viewport_line + EDITOR_TEXT_ROWS) {
        viewport_line = line - EDITOR_TEXT_ROWS + 1U;
    }
}

static void editor_insert_char(char ch) {
    if (current_doc == (editor_document_t*)0) {
        return;
    }
    if (current_doc->length + 1U >= EDITOR_BUFFER_MAX) {
        editor_set_status("No more room in this file.");
        return;
    }
    for (size_t i = current_doc->length; i > cursor_index; --i) {
        current_doc->buffer[i] = current_doc->buffer[i - 1U];
    }
    current_doc->buffer[cursor_index] = ch;
    current_doc->length++;
    current_doc->buffer[current_doc->length] = '\0';
    cursor_index++;
    dirty = 1;
    quit_armed = 0;
    editor_ensure_cursor_visible();
}

static void editor_delete_back(void) {
    if (current_doc == (editor_document_t*)0 || cursor_index == 0U) {
        return;
    }
    for (size_t i = cursor_index - 1U; i < current_doc->length; ++i) {
        current_doc->buffer[i] = current_doc->buffer[i + 1U];
    }
    current_doc->length--;
    current_doc->buffer[current_doc->length] = '\0';
    cursor_index--;
    dirty = 1;
    quit_armed = 0;
    editor_ensure_cursor_visible();
}

static int editor_find_next_ci(const char* needle, size_t start, size_t* found_at) {
    size_t needle_len = kstrlen(needle);
    if (current_doc == (editor_document_t*)0 || needle_len == 0U) {
        return 0;
    }
    if (start > current_doc->length) {
        start = 0U;
    }
    for (size_t i = start; i + needle_len <= current_doc->length; ++i) {
        size_t j = 0U;
        while (j < needle_len && char_equal_ci(current_doc->buffer[i + j], needle[j]) != 0) {
            j++;
        }
        if (j == needle_len) {
            *found_at = i;
            return 1;
        }
    }
    for (size_t i = 0U; i + needle_len <= start && i + needle_len <= current_doc->length; ++i) {
        size_t j = 0U;
        while (j < needle_len && char_equal_ci(current_doc->buffer[i + j], needle[j]) != 0) {
            j++;
        }
        if (j == needle_len) {
            *found_at = i;
            return 1;
        }
    }
    return 0;
}

static void editor_start_prompt(editor_prompt_mode_t mode, const char* status) {
    prompt_mode = mode;
    prompt_len = 0U;
    prompt_input[0] = '\0';
    quit_armed = 0;
    editor_set_status(status);
}

static void editor_finish_prompt(void) {
    prompt_mode = EDITOR_PROMPT_NONE;
    prompt_len = 0U;
    prompt_input[0] = '\0';
}

static void editor_save_current(void) {
    int rc;
    if (current_doc == (editor_document_t*)0) {
        return;
    }
    rc = afs_write_file(current_doc->path, current_doc->buffer);
    if (rc == AFS_OK) {
        dirty = 0;
        editor_set_status("Saved.");
    } else if (rc == AFS_ERR_DENIED) {
        editor_set_status("Save blocked: missing permission.");
    } else {
        editor_set_status("Save failed.");
    }
    quit_armed = 0;
}

static void editor_move_left(void) {
    if (cursor_index == 0U) {
        return;
    }
    cursor_index--;
    editor_ensure_cursor_visible();
    quit_armed = 0;
}

static void editor_move_right(void) {
    if (current_doc == (editor_document_t*)0 || cursor_index >= current_doc->length) {
        return;
    }
    cursor_index++;
    editor_ensure_cursor_visible();
    quit_armed = 0;
}

static void editor_move_up(void) {
    size_t line = 0U;
    size_t col = 0U;
    editor_line_col_from_index(cursor_index, &line, &col);
    if (line == 0U) {
        return;
    }
    if (preferred_col == 0U && col != 0U) {
        preferred_col = col;
    }
    line--;
    cursor_index = editor_index_from_line_col(line, preferred_col);
    editor_ensure_cursor_visible();
    quit_armed = 0;
}

static void editor_move_down(void) {
    size_t line = 0U;
    size_t col = 0U;
    size_t total_lines = editor_total_lines();
    editor_line_col_from_index(cursor_index, &line, &col);
    if (line + 1U >= total_lines) {
        return;
    }
    if (preferred_col == 0U && col != 0U) {
        preferred_col = col;
    }
    line++;
    cursor_index = editor_index_from_line_col(line, preferred_col);
    editor_ensure_cursor_visible();
    quit_armed = 0;
}

static void editor_page_up(void) {
    size_t line = 0U;
    size_t col = 0U;
    editor_line_col_from_index(cursor_index, &line, &col);
    if (line > EDITOR_TEXT_ROWS) {
        line -= EDITOR_TEXT_ROWS;
    } else {
        line = 0U;
    }
    cursor_index = editor_index_from_line_col(line, col);
    editor_ensure_cursor_visible();
    quit_armed = 0;
}

static void editor_page_down(void) {
    size_t line = 0U;
    size_t col = 0U;
    size_t total_lines = editor_total_lines();
    editor_line_col_from_index(cursor_index, &line, &col);
    line += EDITOR_TEXT_ROWS;
    if (line >= total_lines) {
        line = total_lines - 1U;
    }
    cursor_index = editor_index_from_line_col(line, col);
    editor_ensure_cursor_visible();
    quit_armed = 0;
}

static void editor_draw_top_bar(void) {
    char line[81];
    size_t pos = 0U;
    line[0] = '\0';
    append_text_bounded(line, &pos, sizeof(line), " LUMEN ");
    append_text_bounded(line, &pos, sizeof(line), EDITOR_VERSION);
    append_text_bounded(line, &pos, sizeof(line), "  |  ");
    append_text_bounded(line, &pos, sizeof(line), current_doc != (editor_document_t*)0 ? current_doc->path : "(no file)");
    append_text_bounded(line, &pos, sizeof(line), dirty != 0 ? "  [modified]" : "  [saved]");
    write_padded_line(line);
}

static void editor_draw_text_rows(void) {
    char linebuf[81];
    size_t cursor_line = 0U;
    size_t cursor_col = 0U;
    editor_line_col_from_index(cursor_index, &cursor_line, &cursor_col);

    for (size_t row = 0U; row < EDITOR_TEXT_ROWS; ++row) {
        size_t line_no = viewport_line + row;
        size_t total_lines = editor_total_lines();
        size_t pos = 0U;
        linebuf[0] = '\0';

        if (line_no < total_lines) {
            size_t display_no = line_no + 1U;
            char gutter[8];
            size_t gpos = 0U;
            gutter[0] = '\0';
            append_dec_bounded(gutter, &gpos, sizeof(gutter), (uint32_t)display_no);
            while (gpos < 4U) {
                gutter[gpos++] = ' ';
            }
            gutter[gpos] = '\0';
            append_text_bounded(linebuf, &pos, sizeof(linebuf), line_no == cursor_line ? ">" : " ");
            append_text_bounded(linebuf, &pos, sizeof(linebuf), gutter);

            {
                size_t idx = editor_line_start_index(line_no);
                size_t col = 0U;
                int inserted_cursor = 0;
                while (idx < current_doc->length && current_doc->buffer[idx] != '\n' && pos + 1U < sizeof(linebuf)) {
                    size_t text_cols = pos > EDITOR_GUTTER_WIDTH ? (pos - EDITOR_GUTTER_WIDTH) : 0U;
                    if (line_no == cursor_line && cursor_col == col && inserted_cursor == 0) {
                        if (text_cols < EDITOR_TEXT_WIDTH) {
                            append_char_bounded(linebuf, &pos, sizeof(linebuf), '|');
                        }
                        inserted_cursor = 1;
                        text_cols = pos > EDITOR_GUTTER_WIDTH ? (pos - EDITOR_GUTTER_WIDTH) : 0U;
                    }
                    if (text_cols >= EDITOR_TEXT_WIDTH) {
                        break;
                    }
                    append_char_bounded(linebuf, &pos, sizeof(linebuf), current_doc->buffer[idx]);
                    idx++;
                    col++;
                }
                if (line_no == cursor_line && cursor_col == col && inserted_cursor == 0 && pos + 1U < sizeof(linebuf)) {
                    size_t text_cols = pos > EDITOR_GUTTER_WIDTH ? (pos - EDITOR_GUTTER_WIDTH) : 0U;
                    if (text_cols < EDITOR_TEXT_WIDTH) {
                        append_char_bounded(linebuf, &pos, sizeof(linebuf), '|');
                    }
                }
            }
        } else {
            append_text_bounded(linebuf, &pos, sizeof(linebuf), "~");
        }
        write_padded_line(linebuf);
    }
}

static void editor_draw_status_rows(void) {
    char status[81];
    char prompt[81];
    size_t pos = 0U;
    size_t cursor_line = 0U;
    size_t cursor_col = 0U;
    status[0] = '\0';
    prompt[0] = '\0';

    editor_line_col_from_index(cursor_index, &cursor_line, &cursor_col);
    append_text_bounded(status, &pos, sizeof(status), " File ");
    append_text_bounded(status, &pos, sizeof(status), current_doc != (editor_document_t*)0 ? current_doc->name : "(none)");
    append_text_bounded(status, &pos, sizeof(status), "  |  Line ");
    append_dec_bounded(status, &pos, sizeof(status), (uint32_t)(cursor_line + 1U));
    append_text_bounded(status, &pos, sizeof(status), ", Col ");
    append_dec_bounded(status, &pos, sizeof(status), (uint32_t)(cursor_col + 1U));
    append_text_bounded(status, &pos, sizeof(status), "  |  Bytes ");
    append_dec_bounded(status, &pos, sizeof(status), (uint32_t)(current_doc != (editor_document_t*)0 ? current_doc->length : 0U));
    write_padded_line(status);

    pos = 0U;
    if (prompt_mode == EDITOR_PROMPT_FIND) {
        append_text_bounded(prompt, &pos, sizeof(prompt), " Find text: ");
        append_text_bounded(prompt, &pos, sizeof(prompt), prompt_input);
        append_char_bounded(prompt, &pos, sizeof(prompt), '|');
    } else if (prompt_mode == EDITOR_PROMPT_GOTO) {
        append_text_bounded(prompt, &pos, sizeof(prompt), " Go to line: ");
        append_text_bounded(prompt, &pos, sizeof(prompt), prompt_input);
        append_char_bounded(prompt, &pos, sizeof(prompt), '|');
    } else {
        append_text_bounded(prompt, &pos, sizeof(prompt), " ");
        append_text_bounded(prompt, &pos, sizeof(prompt), status_text);
    }
    write_padded_line(prompt);

    write_padded_line(" Save ^S   Exit ^Q   Find ^F   Goto ^G   Move arrows   PgUp/PgDn scroll ");
}

static void editor_render(void) {
    console_clear();
    editor_draw_top_bar();
    editor_draw_text_rows();
    editor_draw_status_rows();
}

static void editor_load_from_fs(editor_document_t* doc) {
    int rc;
    if (doc == (editor_document_t*)0) {
        return;
    }
    rc = afs_read_file(doc->path, doc->buffer, sizeof(doc->buffer));
    if (rc >= 0) {
        doc->length = (size_t)rc;
        if (doc->length >= EDITOR_BUFFER_MAX) {
            doc->length = EDITOR_BUFFER_MAX - 1U;
        }
        doc->buffer[doc->length] = '\0';
        editor_normalize_loaded_text(doc);
        return;
    }
    doc->length = 0U;
    doc->buffer[0] = '\0';
}

static editor_document_t* editor_find(const char* path) {
    for (size_t i = 0U; i < EDITOR_MAX_DOCS; ++i) {
        if (docs[i].used != 0 && kstrcmp(docs[i].path, path) == 0) {
            return &docs[i];
        }
    }
    return (editor_document_t*)0;
}

static editor_document_t* editor_create(const char* name, const char* path) {
    for (size_t i = 0U; i < EDITOR_MAX_DOCS; ++i) {
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

void editor_show_help(void) {
    editor_set_status("Lumen: arrows move, Enter inserts line break, Ctrl+F searches, Ctrl+G jumps.");
    if (active != 0) {
        editor_render();
    } else {
        console_writeln("Lumen shortcuts: Ctrl+S save, Ctrl+Q quit, Ctrl+F find, Ctrl+G go to line.");
    }
}

void editor_show_document(void) {
    if (active != 0) {
        editor_set_status("Document is already visible in the editor view.");
        editor_render();
        return;
    }
    console_writeln("Lumen document preview is available while the editor is open.");
}

void editor_list_documents(void) {
    console_writeln("Open notes in memory:");
    for (size_t i = 0U; i < EDITOR_MAX_DOCS; ++i) {
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
    for (size_t i = 0U; i < EDITOR_MAX_DOCS; ++i) {
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
            console_writeln("[Lumen] Could not create file in CyralithFS.");
            return;
        }
    }

    editor_load_from_fs(doc);
    current_doc = doc;
    active = 1;
    cursor_index = current_doc->length;
    viewport_line = 0U;
    preferred_col = 0U;
    editor_ensure_cursor_visible();
    dirty = 0;
    quit_armed = 0;
    prompt_mode = EDITOR_PROMPT_NONE;
    prompt_len = 0U;
    prompt_input[0] = '\0';
    editor_set_status("Lumen ready. Write normally, save with Ctrl+S, leave with Ctrl+Q.");
    editor_render();
}

void editor_close(void) {
    active = 0;
    current_doc = (editor_document_t*)0;
    cursor_index = 0U;
    viewport_line = 0U;
    preferred_col = 0U;
    dirty = 0;
    quit_armed = 0;
    prompt_mode = EDITOR_PROMPT_NONE;
    prompt_len = 0U;
    prompt_input[0] = '\0';
    console_clear();
}

static void editor_execute_prompt(void) {
    size_t found_at = 0U;
    if (prompt_mode == EDITOR_PROMPT_FIND) {
        if (prompt_len == 0U) {
            editor_set_status("Find cancelled.");
        } else if (editor_find_next_ci(prompt_input, cursor_index + 1U, &found_at) != 0) {
            cursor_index = found_at;
            editor_ensure_cursor_visible();
            editor_set_status("Match found.");
        } else {
            editor_set_status("No match found.");
        }
    } else if (prompt_mode == EDITOR_PROMPT_GOTO) {
        int line = katoi(prompt_input);
        if (line <= 0) {
            editor_set_status("Enter a valid line number.");
        } else {
            size_t total_lines = editor_total_lines();
            size_t target = (size_t)(line - 1);
            if (target >= total_lines) {
                target = total_lines - 1U;
            }
            cursor_index = editor_index_from_line_col(target, 0U);
            editor_ensure_cursor_visible();
            editor_set_status("Jumped to line.");
        }
    }
    editor_finish_prompt();
}

void editor_handle_key(int key) {
    if (active == 0) {
        return;
    }

    if (prompt_mode != EDITOR_PROMPT_NONE) {
        if (key == KEY_CTRL_C || key == KEY_CTRL_Q) {
            editor_finish_prompt();
            editor_set_status("Prompt cancelled.");
            editor_render();
            return;
        }
        if (key == KEY_BACKSPACE) {
            if (prompt_len > 0U) {
                prompt_len--;
                prompt_input[prompt_len] = '\0';
            }
            editor_render();
            return;
        }
        if (key == KEY_ENTER) {
            editor_execute_prompt();
            editor_render();
            return;
        }
        if (key > 0 && key < 256 && prompt_len + 1U < sizeof(prompt_input)) {
            prompt_input[prompt_len++] = (char)key;
            prompt_input[prompt_len] = '\0';
            editor_render();
        }
        return;
    }

    if (key == KEY_PAGEUP) {
        editor_page_up();
        editor_render();
        return;
    }
    if (key == KEY_PAGEDOWN) {
        editor_page_down();
        editor_render();
        return;
    }
    if (key == KEY_LEFT) {
        editor_move_left();
        editor_render();
        return;
    }
    if (key == KEY_RIGHT) {
        editor_move_right();
        editor_render();
        return;
    }
    if (key == KEY_UP) {
        editor_move_up();
        editor_render();
        return;
    }
    if (key == KEY_DOWN) {
        editor_move_down();
        editor_render();
        return;
    }
    if (key == KEY_CTRL_S) {
        editor_save_current();
        editor_render();
        return;
    }
    if (key == KEY_CTRL_F) {
        editor_start_prompt(EDITOR_PROMPT_FIND, "Find the next matching text.");
        editor_render();
        return;
    }
    if (key == KEY_CTRL_G) {
        editor_start_prompt(EDITOR_PROMPT_GOTO, "Jump to a line number.");
        editor_render();
        return;
    }
    if (key == KEY_CTRL_Q) {
        if (dirty != 0 && quit_armed == 0) {
            quit_armed = 1;
            editor_set_status("Unsaved changes. Press Ctrl+Q again to exit without saving.");
            editor_render();
            return;
        }
        editor_close();
        return;
    }
    if (key == KEY_CTRL_C) {
        editor_set_status("Lumen shortcuts: Ctrl+S save, Ctrl+Q exit, Ctrl+F find, Ctrl+G goto.");
        quit_armed = 0;
        editor_render();
        return;
    }
    if (key == KEY_BACKSPACE) {
        editor_delete_back();
        editor_render();
        return;
    }
    if (key == KEY_ENTER) {
        editor_insert_char('\n');
        editor_render();
        return;
    }
    if (key == '\t') {
        for (int i = 0; i < 4; ++i) {
            editor_insert_char(' ');
        }
        editor_render();
        return;
    }
    if (key > 0 && key < 256) {
        editor_insert_char((char)key);
        editor_render();
    }
}
