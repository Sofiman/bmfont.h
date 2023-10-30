#ifndef UTILS_H
#define UTILS_H

#define MAGIC_SIZE 3
const char MAGIC[MAGIC_SIZE] = { 'B', 'M', 'F' };

enum block_kind
{
    BLOCK_INFO = 1,
    BLOCK_COMMON = 2,
    BLOCK_PAGES = 3,
    BLOCK_CHARS = 4,
    BLOCK_KERNING = 5,
};

#define INFO_BLOCK_MIN_SIZE 15
struct info_block
{
    int16_t font_size;
    uint8_t mode; // 0: smooth, 1: unicode, 2: italic, 3: bold, 4: fixedHeight
    uint8_t charset;
    uint16_t stretch_h;
    uint8_t aa;
    uint8_t padding_up;
    uint8_t padding_right;
    uint8_t padding_down;
    uint8_t padding_left;
    uint8_t spacing_horiz;
    uint8_t spacing_vert;
    uint8_t outline;
    char font_name[];
};

#define COMMON_BLOCK_MIN_SIZE 15
struct common_block
{
    uint16_t line_height;
    uint16_t base;
    uint16_t scale_w;
    uint16_t scale_h;
    uint16_t pages;
    uint8_t bit_field; // 0-6 reserved, 7: packed
    uint8_t alpha_chn1;
    uint8_t red_chn1;
    uint8_t green_chn1;
    uint8_t blue_chn1;
};

#define PAGE_BLOCK_MIN_SIZE 1

struct char_block
{
    uint32_t id;
    uint16_t x;
    uint16_t y;
    uint16_t width;
    uint16_t height;
    int16_t xoffset;
    int16_t yoffset;
    int16_t xadvance;
    uint8_t page;
    uint8_t chn1;
};
#define CHAR_BLOCK_SIZE sizeof(struct char_block)

struct bmpfont_char
{
    uint16_t x;
    uint16_t y;
    uint16_t width;
    uint16_t height;
    int16_t xoffset;
    int16_t yoffset;
    int16_t xadvance;
};

#define RANGE_MAX_DELTA 6
struct range
{
    uint32_t start;
    uint32_t end;
    uint32_t total;
};

/*
static void print_unicode(unsigned int codepoint)
{
    if (codepoint <= 0x7F)
    {
        // 1-byte UTF-8 character
        printf("%c", (char)codepoint);
    }
    else if (codepoint <= 0x7FF)
    {
        // 2-byte UTF-8 character
        printf("%c%c", (char)(0xC0 | (codepoint >> 6)),
               (char)(0x80 | (codepoint & 0x3F)));
    }
    else if (codepoint <= 0xFFFF)
    {
        // 3-byte UTF-8 character
        printf("%c%c%c", (char)(0xE0 | (codepoint >> 12)),
               (char)(0x80 | ((codepoint >> 6) & 0x3F)),
               (char)(0x80 | (codepoint & 0x3F)));
    }
    else if (codepoint <= 0x10FFFF)
    {
        // 4-byte UTF-8 character
        printf("%c%c%c%c", (char)(0xF0 | (codepoint >> 18)),
               (char)(0x80 | ((codepoint >> 12) & 0x3F)),
               (char)(0x80 | ((codepoint >> 6) & 0x3F)),
               (char)(0x80 | (codepoint & 0x3F)));
    }
}
*/

#define MIN(A, B) ((A > B) ? (B) : (A))
#define MAX(A, B) ((A > B) ? (A) : (B))

#endif /* !UTILS_H */
