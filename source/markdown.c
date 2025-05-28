#include "markdown.h"
#include "memory.h"
#include "document.h"
#include "stdbool.h"

#define SUCCESS 0
#define INVALID_CURSOR_POS -1
#define DELETED_POSITION -2
#define OUTDATED_VERSION -3 /*IGNORE: COMP9017*/

// === Init and Free ===
document *markdown_init(void)
{

    document *doc = (document *)Calloc(1, sizeof(document));
    doc->head = NULL;
    doc->tail = NULL;
    doc->num_characters = 0;
    doc->num_chunks = 0;
    doc->version = 0;

    return doc;
}

void markdown_free(document *doc)
{
    if (doc)
    {
        Chunk *curr = doc->head;

        while (curr)
        {
            Chunk *temp = curr;
            curr = curr->next;
            free_chunk(temp);
        }

        free(doc);
    }
}

// === Edit Commands ===
int markdown_insert(document *doc, uint64_t version, size_t pos, const char *content)
{
    (void)version;
    size_t content_size = strlen(content);

    // if (version == 0) IMPLEMENT LATER: CAUSING TESTING PROBLEMS RN
    if (doc->head == NULL)
    {
        if (pos != 0)
        {
            /*HANDLE INCORRECT POSITION HERE
                i. possibly leave incorrect position to be handled by server, so server
                code checks for valid position and it's not handled here.
                For the client side, server will itself send the processed commands.

                ii. there are return codes given, maybe they make position handling
                easier in server code ? Decided to handle for now. Formatting markdown
                functions do return int type

            */
            return INVALID_CURSOR_POS;
        }
        else
        {

            Chunk *new_chunk = (Chunk *)Calloc(1, sizeof(Chunk));
            size_t cap = calculate_cap(content_size + 1);

            char *text = (char *)Calloc(cap, sizeof(char));
            text = memcpy(text, content, content_size + 1);
            init_chunk(new_chunk, PLAIN, content_size, cap, text, 0, NULL, NULL);

            doc->head = new_chunk;
            doc->tail = new_chunk;
            doc->num_characters = content_size;
            doc->num_chunks++;

            return SUCCESS;
        }
    }

    if (pos > doc->num_characters)
    {
        return INVALID_CURSOR_POS;
    }

    size_t local_pos = -1;
    Chunk *curr = locate_chunk(doc, pos, &local_pos);

    chunk_insert(curr, local_pos, content, content_size);
    doc->num_characters += content_size;

    return SUCCESS;
}

int markdown_delete(document *doc, uint64_t version, size_t pos, size_t len)
{
    (void)doc;
    (void)version;
    (void)pos;
    (void)len;
    return SUCCESS;
}

// === Formatting Commands ===
int markdown_newline(document *doc, uint64_t version, size_t pos)
{
    (void)doc;
    (void)version;
    (void)pos;
    /*
    TODO: Ordered List shifting logic
    */

    if (pos > doc->num_characters)
    {
        return INVALID_CURSOR_POS;
    }

    if (doc->head == NULL)
    {

        Chunk *new_chunk = (Chunk *)Calloc(1, sizeof(Chunk));
        size_t cap = calculate_cap(1 + 1);
        char *text = (char *)Calloc(cap, sizeof(char));
        text[0] = '\n';
        text[1] = '\0';
        init_chunk(new_chunk, PLAIN, 1, cap, text, 0, NULL, NULL);

        doc->head = new_chunk;
        doc->tail = new_chunk;
        doc->num_characters = 1;
        doc->num_chunks++;

        return SUCCESS;
    }

    size_t local_pos;
    Chunk *curr = locate_chunk(doc, pos, &local_pos);

    size_t num_remaining = curr->len - local_pos;

    Chunk *new = (Chunk *)Calloc(1, sizeof(Chunk));
    size_t cap = calculate_cap(num_remaining + 1);
    char *new_text = (char *)Calloc(cap, sizeof(char));

    memmove(new_text, curr->text + local_pos, num_remaining);
    new_text[num_remaining] = '\0';
    init_chunk(new, PLAIN, num_remaining, cap, new_text, 0, curr->next, curr);

    if (curr->next)
    {
        curr->next->previous = new;
    }

    if (curr == doc->tail)
    {
        doc->tail = new;
    }

    doc->num_characters++;
    doc->num_chunks++;

    curr->text[local_pos] = '\n';
    curr->text[local_pos + 1] = '\0';
    curr->next = new;
    curr->len = local_pos + 1;

    if (new->next && new->next->type == ORDERED_LIST_ITEM)
    {
        new->next->index_OL = 1;
        renumber_list_from(new->next);
    }

    return SUCCESS;
}

int markdown_heading(document *doc, uint64_t version, size_t level, size_t pos)
{
    (void)version;

    if (level < 1 || level > 3)
    {
        return INVALID_CURSOR_POS;
    }

    if (pos > doc->num_characters)
    {
        return INVALID_CURSOR_POS;
    }

    const char *prefix = NULL;
    chunk_type type = PLAIN;

    if (level == 1)
    {
        prefix = "# ";
        type = HEADING1;
    }
    else if (level == 2)
    {
        prefix = "## ";
        type = HEADING2;
    }
    else
    {
        prefix = "### ";
        type = HEADING3;
    }

    size_t prefix_len = strlen(prefix);

    // === Case 1: Empty document ===
    if (doc->head == NULL)
    {
        size_t cap = calculate_cap(prefix_len + 1);
        char *text = Calloc(cap, sizeof(char));
        memcpy(text, prefix, prefix_len);
        text[prefix_len] = '\0';

        Chunk *new_chunk = Calloc(1, sizeof(Chunk));
        init_chunk(new_chunk, type, prefix_len, cap, text, 0, NULL, NULL);

        doc->head = new_chunk;
        doc->tail = new_chunk;
        doc->num_chunks = 1;
        doc->num_characters = prefix_len;

        return SUCCESS;
    }

    // === Case 2: Locate target chunk ===
    size_t local_pos;
    Chunk *curr = locate_chunk(doc, pos, &local_pos);

    bool at_line_start = (pos == 0 || local_pos == 0);

    if (at_line_start)
    {
        chunk_insert(curr, local_pos, prefix, prefix_len);
        doc->num_characters += prefix_len;
        curr->type = type;
        return SUCCESS;
    }

    // === Case 3: Not at line start — split and format chunk ===
    split_and_format_chunk(doc, curr, local_pos, prefix, prefix_len, type);

    return SUCCESS;
}

int markdown_bold(document *doc, uint64_t version, size_t start, size_t end)
{
    (void)doc;
    (void)version;
    (void)start;
    (void)end;
    return SUCCESS;
}

int markdown_italic(document *doc, uint64_t version, size_t start, size_t end)
{
    (void)doc;
    (void)version;
    (void)start;
    (void)end;
    return SUCCESS;
}

int markdown_blockquote(document *doc, uint64_t version, size_t pos)
{
    (void)version;

    if (pos > doc->num_characters)
    {
        return INVALID_CURSOR_POS;
    }

    const char *prefix = "> ";
    size_t prefix_len = 2;
    chunk_type type = BLOCKQUOTE;

    // === Case 1: Empty document ===
    if (doc->head == NULL)
    {
        size_t cap = calculate_cap(prefix_len + 1);
        char *text = Calloc(cap, sizeof(char));
        memcpy(text, prefix, prefix_len);
        text[prefix_len] = '\0';

        Chunk *new_chunk = Calloc(1, sizeof(Chunk));
        init_chunk(new_chunk, type, prefix_len, cap, text, 0, NULL, NULL);

        doc->head = new_chunk;
        doc->tail = new_chunk;
        doc->num_chunks = 1;
        doc->num_characters = prefix_len;

        return SUCCESS;
    }

    // === Case 2: Locate target chunk ===
    size_t local_pos;
    Chunk *curr = locate_chunk(doc, pos, &local_pos);

    // === Check if at line start ===
    bool at_line_start = (pos == 0 || local_pos == 0);

    if (at_line_start)
    {
        chunk_insert(curr, local_pos, prefix, prefix_len);
        doc->num_characters += prefix_len;
        curr->type = type;
        return SUCCESS;
    }

    // === Case 3: Not at line start — split and format chunk ===
    split_and_format_chunk(doc, curr, local_pos, prefix, prefix_len, BLOCKQUOTE);

    return SUCCESS;
}

int markdown_ordered_list(document *doc, uint64_t version, size_t pos)
{
    (void)version;

    if (pos > doc->num_characters)
        return INVALID_CURSOR_POS;

    const size_t prefix_len = 3; 

    // Case 1: empty document
    if (doc->head == NULL)
    {
        size_t len = prefix_len + 1; 
        size_t cap = calculate_cap(len);
        char *text = Calloc(cap, sizeof(char));
        memcpy(text, "1. \n", len);
        text[len] = '\0';

        Chunk *c = Calloc(1, sizeof(Chunk));
        init_chunk(c, ORDERED_LIST_ITEM, len, cap, text, 1, NULL, NULL);

        doc->head = doc->tail = c;
        doc->num_chunks = 1;
        doc->num_characters = len;
        return SUCCESS;
    }

    
    size_t local_pos;
    Chunk *curr = locate_chunk(doc, pos, &local_pos);
    bool at_line_start = (pos == 0 || local_pos == 0);

    
    int my_index = prev_ol_index(curr) + 1;
    if (my_index > 9)
        my_index = 9;

    char prefix[4];
    prefix[0] = '0' + my_index;
    prefix[1] = '.';
    prefix[2] = ' ';
    prefix[3] = '\0';

    // === Case 2: at start of line - insert in-place
    if (at_line_start)
    {
        chunk_insert(curr, local_pos, prefix, prefix_len);
        curr->type = ORDERED_LIST_ITEM;
        curr->index_OL = my_index;
        doc->num_characters += prefix_len;

        renumber_list_from(curr);
        return SUCCESS;
    }

    // === Case 3: mid-line split
    size_t tail_len = curr->len - local_pos;
    const char *tail_text = curr->text + local_pos;

    // Create new chunk with: prefix + tail_text
    size_t new_len = prefix_len + tail_len;
    size_t new_cap = calculate_cap(new_len + 1);
    char *new_text = Calloc(new_cap, sizeof(char));
    memcpy(new_text, prefix, prefix_len);
    memcpy(new_text + prefix_len, tail_text, tail_len);
    new_text[new_len] = '\0';

    Chunk *new = Calloc(1, sizeof(Chunk));
    init_chunk(new, ORDERED_LIST_ITEM, new_len, new_cap, new_text, my_index, curr->next, curr);

    if (curr->next)
        curr->next->previous = new;
    else
        doc->tail = new;

    curr->next = new;

    // Truncate current chunk
    curr->text[local_pos] = '\n';
    curr->text[local_pos + 1] = '\0';
    curr->len = local_pos + 1;

    doc->num_chunks++;
    doc->num_characters += prefix_len + 1;

    renumber_list_from(new);
    return SUCCESS;
}

int markdown_unordered_list(document *doc, uint64_t version, size_t pos)
{
    (void)version;

    if (pos > doc->num_characters)
    {
        return INVALID_CURSOR_POS;
    }

    const char *prefix = "- ";
    size_t prefix_len = 2;
    chunk_type type = UNORDERED_LIST_ITEM;

    // === Case 1: Empty document ===
    if (doc->head == NULL)
    {
        size_t cap = calculate_cap(prefix_len + 1);
        char *text = Calloc(cap, sizeof(char));
        memcpy(text, prefix, prefix_len);
        text[prefix_len] = '\0';

        Chunk *new_chunk = Calloc(1, sizeof(Chunk));
        init_chunk(new_chunk, type, prefix_len, cap, text, 0, NULL, NULL);

        doc->head = new_chunk;
        doc->tail = new_chunk;
        doc->num_chunks = 1;
        doc->num_characters = prefix_len;

        return SUCCESS;
    }

    // === Case 2: Locate target chunk ===
    size_t local_pos;
    Chunk *curr = locate_chunk(doc, pos, &local_pos);

    // === Check if at line start ===
    bool at_line_start = (pos == 0 || local_pos == 0);

    if (at_line_start)
    {
        chunk_insert(curr, local_pos, prefix, prefix_len);
        doc->num_characters += prefix_len;
        curr->type = type;
        return SUCCESS;
    }

    // === Case 3: Not at line start — split and format chunk ===
    split_and_format_chunk(doc, curr, local_pos, prefix, prefix_len, UNORDERED_LIST_ITEM);
    return SUCCESS;
}

int markdown_code(document *doc, uint64_t version, size_t start, size_t end)
{
    (void)doc;
    (void)version;
    (void)start;
    (void)end;
    return SUCCESS;
}

int markdown_horizontal_rule(document *doc, uint64_t version, size_t pos)
{
    (void)version;

    if (pos > doc->num_characters)
    {
        return INVALID_CURSOR_POS;
    }

    const char *hr_text = "---\n";
    const size_t hr_len = 4;

    // === Case 1: Empty document ===
    if (doc->head == NULL)
    {
        char *text = Calloc(hr_len + 1, sizeof(char));
        memcpy(text, hr_text, hr_len + 1); // includes '\0'

        Chunk *hr = Calloc(1, sizeof(Chunk));
        init_chunk(hr, HORIZONTAL_RULE, hr_len, hr_len + 1, text, 0, NULL, NULL);

        doc->head = hr;
        doc->tail = hr;
        doc->num_chunks = 1;
        doc->num_characters = hr_len;
        return SUCCESS;
    }

    // === Locate insertion point ===
    size_t local_pos;
    Chunk *curr = locate_chunk(doc, pos, &local_pos);
    bool at_line_start = (pos == 0 || local_pos == 0);

    if (at_line_start)
    {
        // Insert HRULE before curr
        char *text = Calloc(hr_len + 1, sizeof(char));
        memcpy(text, hr_text, hr_len + 1);

        Chunk *hr = Calloc(1, sizeof(Chunk));
        init_chunk(hr, HORIZONTAL_RULE, hr_len, hr_len + 1, text, 0, curr, curr->previous);

        if (curr->previous)
        {
            curr->previous->next = hr;
        }
        else
        {
            doc->head = hr;
        }

        curr->previous = hr;
        doc->num_chunks++;
        doc->num_characters += hr_len;
        return SUCCESS;
    }

    // === Not at line start: split curr, insert HRULE in middle ===
    size_t tail_len = curr->len - local_pos;
    size_t tail_cap = calculate_cap(tail_len + 1);
    char *tail_text = Calloc(tail_cap, sizeof(char));
    memcpy(tail_text, curr->text + local_pos, tail_len);
    tail_text[tail_len] = '\0';

    Chunk *tail = Calloc(1, sizeof(Chunk));
    init_chunk(tail, PLAIN, tail_len, tail_cap, tail_text, 0, curr->next, NULL);

    char *hr_buf = Calloc(hr_len + 1, sizeof(char));
    memcpy(hr_buf, hr_text, hr_len + 1);

    Chunk *hr = Calloc(1, sizeof(Chunk));
    init_chunk(hr, HORIZONTAL_RULE, hr_len, hr_len + 1, hr_buf, 0, tail, curr);

    if (curr->next)
    {
        curr->next->previous = tail;
    }
    else
    {
        doc->tail = tail;
    }

    tail->previous = hr;
    curr->next = hr;

    curr->len = local_pos + 1;
    curr->text[local_pos] = '\n';
    curr->text[local_pos + 1] = '\0';

    doc->num_chunks += 2;
    doc->num_characters += hr_len + 1; // +1 for inserted newline

    if (hr->next && hr->next->next && hr->next->next->type == ORDERED_LIST_ITEM)
    {
        hr->next->next->index_OL = 1;
        renumber_list_from(hr->next->next);
    }
    return SUCCESS;
}

int markdown_link(document *doc, uint64_t version, size_t start, size_t end, const char *url)
{
    (void)doc;
    (void)version;
    (void)start;
    (void)end;
    (void)url;
    return SUCCESS;
}

// === Utilities ===
void markdown_print(const document *doc, FILE *stream)
{
    (void)doc;
    (void)stream;
}

char *markdown_flatten(const document *doc)
{
    if (!doc || doc->num_characters == 0)
        return Malloc(1);

    char *output = Malloc(doc->num_characters + 1); // +1 for null terminator
    size_t offset = 0;

    for (Chunk *c = doc->head; c; c = c->next)
    {
        memcpy(output + offset, c->text, c->len);
        offset += c->len;
    }

    output[offset] = '\0';
    return output;
}

// === Versioning ===
void markdown_increment_version(document *doc)
{
    doc->version += 1;
}
