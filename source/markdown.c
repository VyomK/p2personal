#include "../libs/markdown.h"
#include "../wrappers/memory.h"

#define SUCCESS 0
#define REJECT -1

// === Init and Free ===
document *markdown_init(void)
{

    document *doc = (document *)Calloc(1, sizeof(document));

    doc->head = NULL;
    doc->tail = NULL;
    doc->num_chunks = 0;
    doc->num_characters = 0;
    doc->version = 0;
    return doc;
}

void markdown_free(document *doc)
{
    (void)doc;
}

// === Edit Commands ===
int markdown_insert(document *doc, uint64_t version, size_t pos, const char *content)
{
    (void)doc;
    (void)version;
    (void)pos;
    (void)content;

    size_t content_size = strlen(content);

    if (doc->version == 0)
    {

        Chunk *init = (Chunk *)Calloc(1, sizeof(Chunk));

        doc->head = init;
        doc->tail = init;

        init->type = PLAIN;
        init->index_OL = 0;
        init->next = NULL;
        init->previous = NULL;

        init->len = (content_size + 1 > 128) ? (content_size + 1) : (128);
        init->text = Strndup(content, init->len);

    }

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
    return SUCCESS;
}

int markdown_heading(document *doc, uint64_t version, size_t level, size_t pos)
{
    (void)doc;
    (void)version;
    (void)level;
    (void)pos;
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
    (void)doc;
    (void)version;
    (void)pos;
    return SUCCESS;
}

int markdown_ordered_list(document *doc, uint64_t version, size_t pos)
{
    (void)doc;
    (void)version;
    (void)pos;
    return SUCCESS;
}

int markdown_unordered_list(document *doc, uint64_t version, size_t pos)
{
    (void)doc;
    (void)version;
    (void)pos;
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
    (void)doc;
    (void)version;
    (void)pos;
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
    (void)doc;
    return NULL;
}

// === Versioning ===
void markdown_increment_version(document *doc)
{
    doc->version += 1;
}
