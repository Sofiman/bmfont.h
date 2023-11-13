#include <ctype.h>
#include <err.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_GIF
#define STBI_NO_HDR
#define STBI_NO_PIC
#include "stb_image.h"
#include "utils.h"

static void check_magic(FILE *fnt)
{
    char buf[MAGIC_SIZE];
    if (fread(buf, sizeof(char), sizeof(buf), fnt) != sizeof(buf)
        && memcmp(buf, MAGIC, MAGIC_SIZE) != 0)
    {
        fclose(fnt);
        err(EXIT_FAILURE, "Invalid magic");
    }
}

static void to_snake_case(char *str)
{
    while (*str != '\0')
    {
        if (isspace(*str))
            *str = '_';
        else
            *str = isspace(*str) ? '_' : tolower(*str);
        ++str;
    }
}

static void to_macro_name(char *str)
{
    while (*str != '\0')
    {
        if (*str == '-' || *str == '.')
            *str = '_';
        else
            *str = toupper(*str);
        ++str;
    }
}

static void remove_h(char *str)
{
    size_t i = 0;
    while (str[i] != '\0')
        ++i;
    if (i > 1 && str[i - 1] == 'H' && str[i - 2] == '_')
        str[i - 2] = '\0';
}

struct parser_state
{
    char *header_name;
    FILE *out;

    size_t size;
    size_t slices;
    uint16_t page_count;
    char **pages;
};

int read_fully(void *buf, size_t size, FILE *f)
{
    size_t total = 0;
    size_t len = 0;
    char *b = buf;
    while (total < size && (len = fread(b + total, 1, size - total, f)) != 0)
        total += len;
    return total == size;
}

void read_block(FILE *f, void **block, size_t min_size, uint32_t *size)
{
    if (!read_fully(size, sizeof(uint32_t), f))
        err(EXIT_FAILURE, "Failed to read block size");

    if (*size < min_size)
        errx(EXIT_FAILURE,
             "Block size too small for kind (size: %ub, expected: >=%zub)",
             *size, min_size);

    *block = malloc(*size);
    if (*block == NULL)
        errx(EXIT_FAILURE, "Not enough memory to allocate block (size: %ub)",
             *size);

    if (!read_fully(*block, *size, f))
        err(EXIT_FAILURE, "Failed to read full block (size: %ub)", *size);
}

const size_t BMFONT_CHAR_STRUCT_SIZE = 2 + 2 + 2 + 2 + 2 + 2 + 2;
const size_t BMFONT_SLICE_STRUCT_SIZE = 4 + 4 + sizeof(void *);
const size_t BMFONT_STRUCT_SIZE = 2 + sizeof(void *);

const char *HEADER_PREFACE =
    ""
    "#include <stdint.h>\n\n"
    "#ifndef BMFONT_TYPES\n"
    "#    define BMFONT_TYPES\n\n"
    "struct bmfont_char\n"
    "{\n"
    "    uint16_t x;\n"
    "    uint16_t y;\n"
    "    uint16_t width;\n"
    "    uint16_t height;\n"
    "    int16_t xoffset;\n"
    "    int16_t yoffset;\n"
    "    int16_t xadvance;\n"
    "};\n\n"
    "struct bmfont_slice\n"
    "{\n"
    "    uint32_t start;\n"
    "    uint32_t end;\n"
    "    const struct bmfont_char *chars;\n"
    "};\n\n"
    "struct bmfont\n"
    "{\n"
    "    uint16_t slices_count;\n"
    "    const struct bmfont_slice *slices;\n"
    "};\n\n"
    "const struct bmfont_char *bmfont_char_info(const struct bmfont *font,\n"
    "                                           unsigned int id)\n"
    "{\n"
    "    size_t left = 0;\n"
    "    size_t right = font->slices_count;\n"
    "    int found = 0;\n\n"
    "    while (left < right && !found)\n"
    "    {\n"
    "        size_t m = left + (right - left) / 2;\n"
    "        if (id > font->slices[m].end)\n"
    "            left = m + 1;\n"
    "        else\n"
    "        {\n"
    "            if (id >= font->slices[m].start)\n"
    "                found = 1;\n"
    "            right = m;\n"
    "        }\n"
    "    }\n\n"
    "    if (found)\n"
    "    {\n"
    "        const struct bmfont_char *chr =\n"
    "            font->slices[right].chars + (id - font->slices[right].start);\n"
    "        if (chr->width != 0 || chr->xadvance != 0)\n"
    "            return chr;\n"
    "    }\n"
    "    return NULL;\n"
    "}\n"
    "#endif /* !BMFONT_TYPES */\n\n";

void handle_info_block(void *buf, struct parser_state *state)
{
    struct info_block *block = buf;
    printf("Detected `%s` (size %u)\n", block->font_name, block->font_size);

    char *name;
    asprintf(&name, "bmfont_%s_%u.h", block->font_name, block->font_size);
    if (name == NULL)
        errx(EXIT_FAILURE, "Failed to allocate output file name string");

    to_snake_case(name);
    if (state->out == NULL)
    {
        printf("Saving output to `%s`\n", name);
        state->out = fopen(name, "w");

        if (state->out == NULL)
            err(EXIT_FAILURE, "Failed to open output file");
    }

    to_macro_name(name);
    fprintf(state->out, "#ifndef %s\n#define %s\n\n", name, name);

    remove_h(name);
    state->header_name = name;
    fprintf(state->out, "%s", HEADER_PREFACE);
}

void handle_common_block(void *buf, struct parser_state *state)
{
    struct common_block *block = buf;
    printf("Line Height: %hi\n", block->line_height);

    state->pages = malloc(block->pages * sizeof(char *));
    if (state->pages == NULL)
        errx(EXIT_FAILURE, "Failed to allocate memory for pages");
}

void handle_page_block(void *buf, struct parser_state *state)
{
    char *block = buf;
    printf("Detected page: %s\n", block);
    state->pages[state->page_count++] = strdup(block);
}

static inline int in_range(struct range range, uint32_t id)
{
    uint32_t left = range.start;
    if (left > RANGE_MAX_DELTA)
        left -= RANGE_MAX_DELTA;
    else
        left = 0;
    uint32_t right = range.end + RANGE_MAX_DELTA;
    return id >= left && id <= right;
}

void handle_char_block(void *buf, uint32_t size, struct parser_state *state)
{
    size_t count = size / CHAR_BLOCK_SIZE;
    printf("Detected %lu chars\n", count);

    size_t used = 0;
    size_t cap = 4;
    struct range *ranges = malloc(cap * sizeof(struct range));
    if (ranges == NULL)
        errx(EXIT_FAILURE, "Failed to allocate memory for char range analysis");

    struct char_block *block = buf;
    for (size_t i = 0; i < count; ++i)
    {
        uint32_t id = block[i].id;
        size_t j = 0;
        while (j < used && !in_range(ranges[j], id))
            ++j;
        if (j == used)
        {
            if (j == cap)
            {
                cap *= 2;
                ranges = realloc(ranges, cap * sizeof(struct range));
            }
            ranges[j].start = id;
            ranges[j].end = id;
            ranges[j].total = 0;
            used++;

            if (used > 1)
                fprintf(state->out, "};\n");
            fprintf(state->out, "const struct bmfont_char %s_SLICE_%li[] = {\n",
                    state->header_name, j);
        }
        else
        {
            assert(id > ranges[j].start
                   && "Chars are not sorted in ascending order");
            for (++ranges[j].end; ranges[j].end < id; ++ranges[j].end)
            {
                fprintf(state->out,
                        "    { 0, 0, 0, 0, 0, 0, 0 }, // (invalid) %u\n",
                        ranges[j].end);
                state->size += BMFONT_CHAR_STRUCT_SIZE;
            }
        }
        ranges[j].total++;
        fprintf(
            state->out, "    { %hu, %hu, %hu, %hu, %hi, %hi, %hi }, // %u\n",
            block[i].x, block[i].y, block[i].width, block[i].height,
            block[i].xoffset, block[i].yoffset, block[i].xadvance, block[i].id);
    }

    if (used > 1)
        fprintf(state->out, "};\n");

    fprintf(state->out, "const struct bmfont_slice %s_SLICES[] = {\n",
            state->header_name);

    state->size += count * BMFONT_CHAR_STRUCT_SIZE;
    state->size += used * BMFONT_SLICE_STRUCT_SIZE;

    printf("Successfully generated %zu slices:\n", used);
    for (size_t i = 0; i < used; ++i)
    {
        uint32_t cap = ranges[i].end - ranges[i].start + 1;
        double occupency = ranges[i].total;
        occupency /= cap;
        printf("- Range %lu: %u - %u (%u/%u -> %.2f%%)\n", i, ranges[i].start,
               ranges[i].end, ranges[i].total, cap, occupency * 100);

        fprintf(state->out, "    { %u, %u, %s_SLICE_%li },\n", ranges[i].start,
                ranges[i].end, state->header_name, i);
    }

    fprintf(state->out, "};\n");
    state->slices = used;

    free(ranges);
}

int extract_block(FILE *f, struct parser_state *state)
{
    int v = fgetc(f);
    if (v == EOF)
        return 0;

    uint32_t size = 0;
    void *buf = NULL;

    switch (v)
    {
    case BLOCK_INFO:
        read_block(f, &buf, INFO_BLOCK_MIN_SIZE, &size);
        handle_info_block(buf, state);
        break;
    case BLOCK_COMMON:
        read_block(f, &buf, COMMON_BLOCK_MIN_SIZE, &size);
        handle_common_block(buf, state);
        break;
    case BLOCK_PAGES:
        read_block(f, &buf, PAGE_BLOCK_MIN_SIZE, &size);
        handle_page_block(buf, state);
        break;
    case BLOCK_CHARS:
        read_block(f, &buf, CHAR_BLOCK_SIZE, &size);
        handle_char_block(buf, size, state);
        break;
    case BLOCK_KERNING:
        read_block(f, &buf, PAGE_BLOCK_MIN_SIZE, &size);
        printf("kerning block\n");
        break;
    default:
        errx(EXIT_FAILURE, "Unknown block ID %i", v);
        break;
    }

    free(buf);
    return 1;
}

void dump_page(struct parser_state *state, const char *page)
{
    int width;
    int height;
    unsigned char *img = stbi_load(page, &width, &height, NULL, STBI_grey);
    if (img == NULL)
        err(EXIT_FAILURE, "Failed to load page");

    size_t total = (width * height) / 8;
    state->size += total * sizeof(unsigned char);
    fprintf(state->out, "const unsigned char %s_DATA[%lu] = {\n",
            state->header_name, total);

    for (size_t y = 0; y < height; ++y)
    {
        fprintf(state->out, "   ");
        for (size_t x = 0; x < width; x += 8)
        {
            uint8_t buf = 0;
            for (uint8_t b = 0; b < 8; ++b)
                buf = (buf << 1) | (img[y * width + x + b] & 1);
            fprintf(state->out, " 0x%02x,", buf);
        }
        fprintf(state->out, "\n");
    }

    fprintf(state->out, "};\n");

    free(img);
}

int main(int argc, char **argv)
{
    if (argc < 2 || argc > 3)
        errx(EXIT_FAILURE, "Usage: %s font.fnt <out.h>", argv[0]);

    FILE *fnt = fopen(argv[1], "rb");
    if (fnt == NULL)
        err(EXIT_FAILURE, "Could not open font file");

    check_magic(fnt);
    char version = fgetc(fnt);
    if (version == EOF)
        err(EXIT_FAILURE, "Could not read version");
    if (version != 3)
        errx(EXIT_FAILURE, "Unsupported font verison (%hhi)", version);

    FILE *out = argc == 3 ? fopen(argv[2], "w") : NULL;

    struct parser_state state = { .out = out,
                                  .header_name = NULL,
                                  .size = 0,
                                  .slices = 0,
                                  .page_count = 0,
                                  .pages = NULL };
    while (extract_block(fnt, &state))
        continue;

    if (state.out != NULL)
    {
        printf("Exporting pages...\n");

        for (size_t i = 0; i < state.page_count; ++i)
            dump_page(&state, state.pages[i]);

        fprintf(state.out, "const struct bmfont %s = { %lu, %s_SLICES };\n",
                state.header_name, state.slices, state.header_name);
        state.size += BMFONT_STRUCT_SIZE;

        fprintf(state.out, "#endif /* !%s_H */\n", state.header_name);
        printf("Successfully saved output file (%lib).\n", state.size);
    }

    if (state.pages != NULL)
    {
        for (size_t i = 0; i < state.page_count; ++i)
            free(state.pages[i]);
        free(state.pages);
    }
    fclose(state.out);
    fclose(fnt);
    free(state.header_name);

    return 0;
}
