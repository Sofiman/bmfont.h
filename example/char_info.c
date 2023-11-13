#include <stddef.h>
#include <stdio.h>

#include "bmfont_x.h" // <- Replace this with the generated font C file

static unsigned char const utf8_byte_count[] = {
    //  0 1 2 3 4 5 6 7 8 9 A B C D E F
    1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 2, 2, 3, 4
};

static inline int utf8_is_valid(unsigned int codepoint)
{
    return (codepoint <= 0x10FFFF)
        && ((codepoint < 0xD800) || (codepoint > 0xDFFF));
}

static int utf8_next_codepoint(const char **str, unsigned int *codepoint)
{
    if (**str == 0)
        return 1; // UTF8_END_OF_STRING
    if ((**str & 0xC0) == 0x80)
        return 1; // UTF8_ERR_INVALID_START

    unsigned char bytes = utf8_byte_count[(**str & 0xFF) >> 4];
    if (bytes == 0)
    {
        *codepoint = 0xFFFD;
        ++(*str); // advance by in the string
    }
    else if (bytes == 1)
    {
        *codepoint = **str;
        ++(*str); // advance by in the string
    }
    else
    {
        unsigned int code = (*(*str)++) & ((1 << (8 - bytes)) - 1);
        for (--bytes; bytes > 0 && ((**str) & 0xC0) == 0x80; --bytes, ++(*str))
        {
            code = (code << 6) | ((**str) & 0x3F);
        }
        if (bytes != 0)
        {
            if (**str == 0)
            {
                return 2; // UTF8_ERR_INCOMPLETE_MULTIBYTE
            }
            code = 0xFFFD;
        }
        else if (!utf8_is_valid(code))
        {
            code = 0xFFFD;
        }
        *codepoint = code;
    }
    return 0;
}

int main(int argc, char **argv)
{
    if (argc != 2)
        return 1;

    const char *str = argv[1];
    unsigned int cp;
    int res;
    while ((res = utf8_next_codepoint(&str, &cp)) == 2)
        continue;

    if (res != 0)
        return 2;

    const struct bmfont_char *chr =
        bmfont_char_info(&BMFONT_LATIN_MODERN_MATH_142, cp);
    printf("%u> ", cp);
    if (chr == NULL)
        printf("NULL\n");
    else
        printf("X: %u, Y: %u, W: %u, H: %u\n", chr->x, chr->y, chr->width,
               chr->height);
    return 0;
}
