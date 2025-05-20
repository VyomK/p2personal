#include "markdown.h"
#include "memory.h"
#include "document.h"

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

    size_t content_size = strlen(content);

    if (doc->version == 0)
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
    Chunk* curr = locate_chunk(doc, pos, &local_pos); 

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
