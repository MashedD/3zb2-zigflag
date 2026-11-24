#include "map_rotation.h"

#undef NDEBUG
#include <assert.h>
#include <string.h>

static void Check(const char *section, const char *current, const char *expected)
{
    FILE *file = tmpfile();
    assert(file);
    fputs("[other]\nignored\n [QC_QL]\r\n\t;cor_d\r\n # comment\n // comment\n\t\r\n"
          "awoken \"", file);
    for (int i = 0; i < 4096; i++)
        fputc('x', file);
    fputs("\"\r\n cor_f \"Title\"\r\n ;cor_d\r\ncor_ffa_d\r\n"
          "   ;cor_d\r\n[empty]\n# nothing\n[single]\nonly\n[last]\na\n", file);
    for (int i = 0; i < 64; i++)
        fputc('y', file);
    fputs(" \"Overlong map token\"\nb", file);
    rewind(file);
    char next[64] = "fallback";
    bool found = MapRotation_Next(file, section, current, next, sizeof(next));
    assert(found == (expected != NULL));
    assert(!strcmp(next, expected ? expected : "fallback"));
    fclose(file);
}

int main(void)
{
    Check("qc_ql", "AWOKEN", "cor_f");
    Check("qc_ql", "cor_f", "cor_ffa_d");
    Check("qc_ql", "cor_ffa_d", "awoken");
    Check("qc_ql", "absent", "awoken");
    Check("single", "only", "only");
    Check("last", "a", "b");
    Check("last", "b", "a");
    Check("empty", "a", NULL);
    Check("missing", "a", NULL);
    return 0;
}
