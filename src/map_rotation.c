#include "map_rotation.h"

#include <ctype.h>
#include <string.h>

enum line_type {
    LINE_END,
    LINE_SKIP,
    LINE_SECTION,
    LINE_MAP,
};

static bool SameName(const char *left, const char *right)
{
    while (*left && tolower((unsigned char)*left) == tolower((unsigned char)*right)) {
        left++;
        right++;
    }
    return tolower((unsigned char)*left) == tolower((unsigned char)*right);
}

static enum line_type ReadEntry(FILE *file, char name[MAX_QPATH])
{
    int ch;
    do {
        ch = fgetc(file);
    } while (ch != EOF && ch != '\n' && isspace(ch));

    if (ch == EOF)
        return LINE_END;
    if (ch == '\n')
        return LINE_SKIP;

    bool section = ch == '[';
    if (section)
        ch = fgetc(file);

    size_t length = 0;
    while (ch != EOF && ch != '\n' && (section ? ch != ']' : !isspace(ch))) {
        if (length < MAX_QPATH)
            name[length++] = (char)ch;
        ch = fgetc(file);
    }

    bool valid = length > 0 && length < MAX_QPATH && (!section || ch == ']');
    name[valid ? length : 0] = '\0';

    /* The rest of a title is never another entry, regardless of its length. */
    while (ch != EOF && ch != '\n')
        ch = fgetc(file);

    if (section)
        return LINE_SECTION;
    if (!valid || name[0] == ';' || name[0] == '#' || !strncmp(name, "//", 2))
        return LINE_SKIP;
    return LINE_MAP;
}

bool MapRotation_Next(FILE *file, const char *section, const char *current,
                      char *next, size_t capacity)
{
    char name[MAX_QPATH];
    char first[MAX_QPATH] = "";
    bool in_section = false;
    bool found_current = false;
    enum line_type type;
    while ((type = ReadEntry(file, name)) != LINE_END) {
        if (type == LINE_SECTION) {
            if (in_section)
                break;
            in_section = SameName(name, section);
        } else if (in_section && type == LINE_MAP) {
            if (found_current)
                break;
            if (!first[0])
                memcpy(first, name, strlen(name) + 1);
            found_current = SameName(name, current);
        }
    }

    const char *selected = in_section && type == LINE_MAP ? name : first;
    size_t size = strlen(selected) + 1;
    if (ferror(file) || !selected[0] || size > capacity)
        return false;
    memcpy(next, selected, size);
    return true;
}
