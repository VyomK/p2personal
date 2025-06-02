#ifndef NAIVE_OPS_H
#define NAIVE_OPS_H

#include "document.h"

// === Edit Commands ===
int naive_insert(document *doc, size_t pos, const char *content);
int naive_delete(document *doc, size_t pos, size_t len);

// === Formatting Commands ===
int naive_newline(document *doc, size_t pos);
int naive_heading(document *doc, size_t level, size_t pos);
int naive_bold(document *doc, size_t start, size_t end);
int naive_italic(document *doc, size_t start, size_t end);
int naive_blockquote(document *doc, size_t pos);
int naive_ordered_list(document *doc, size_t pos);
int naive_unordered_list(document *doc, size_t pos);
int naive_code(document *doc, size_t start, size_t end);
int naive_horizontal_rule(document *doc, size_t pos);
int naive_link(document *doc, size_t start, size_t end, const char *url);

// === Internal Helper ===
int naive_insert_raw(document *doc, size_t working_pos, size_t snapshot_pos, const char *content);
int naive_newline_raw(document *doc, size_t working_pos, size_t snapshot_pos);

#endif // NAIVE_OPS_H
