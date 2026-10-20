/*
 * Unit tests for parse.c's hand-rolled glob replacement (glob_match,
 * shush_glob), which are file-scope static. Pulling in parse.c's source
 * directly (rather than re-copying its functions) means these tests always
 * exercise the real, current implementation, not a hand-maintained copy that
 * can silently drift out of sync.
 *
 * parse.c references four globals normally defined in shush.c (which we
 * don't link here, since its main() would collide with this test's own).
 * Stub them -- parse.c's other functions reference them, even though this
 * test never calls those functions, so the linker still needs them defined.
 */
#include "test_common.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../../session.h"
int last_exit_status;
Session session;
char *script_name = NULL;
char **script_args = NULL;
int script_argc = 0;

#include "../../parse.c"

static void make_file(const char *name)
{
    FILE *f = fopen(name, "w");
    if (f)
        fclose(f);
}

static int contains(char **arr, size_t n, const char *s)
{
    for (size_t i = 0; i < n; i++)
    {
        if (strcmp(arr[i], s) == 0)
            return 1;
    }
    return 0;
}

static void free_matches(char **matches, size_t count)
{
    for (size_t i = 0; i < count; i++)
        free(matches[i]);
    free(matches);
}

int main(void)
{
    char tmpl[] = "/tmp/shush_glob_test_XXXXXX";
    char *dir = mkdtemp(tmpl);
    CHECK(dir != NULL, "mkdtemp succeeded");
    if (!dir)
    {
        TEST_SUMMARY();
    }

    char orig_cwd[4096];
    CHECK(getcwd(orig_cwd, sizeof(orig_cwd)) != NULL, "captured original cwd");
    CHECK(chdir(dir) == 0, "chdir into fixture directory");

    make_file("a.txt");
    make_file("b.txt");
    make_file("c.log");
    make_file("file1.txt");
    make_file("file2.txt");
    make_file(".hidden");
    make_file(".hiddenrc");
    mkdir("dirA", 0755);
    mkdir("dirB", 0755);

    size_t count;
    char **m;

    m = shush_glob("*.txt", &count);
    CHECK_EQ_INT(count, 4, "*.txt matches 4 files");
    CHECK(m && contains(m, count, "a.txt") && contains(m, count, "b.txt") &&
              contains(m, count, "file1.txt") && contains(m, count, "file2.txt"),
          "*.txt matches exactly a.txt/b.txt/file1.txt/file2.txt");
    free_matches(m, count);

    m = shush_glob("file?.txt", &count);
    CHECK_EQ_INT(count, 2, "file?.txt matches 2 files");
    free_matches(m, count);

    m = shush_glob("[ab].txt", &count);
    CHECK_EQ_INT(count, 2, "[ab].txt matches a.txt and b.txt");
    CHECK(m && contains(m, count, "a.txt") && contains(m, count, "b.txt"),
          "[ab].txt matches the right two files");
    free_matches(m, count);

    m = shush_glob("[!a]*.txt", &count);
    CHECK_EQ_INT(count, 3, "[!a]*.txt excludes a.txt, matches the other 3");
    free_matches(m, count);

    m = shush_glob("*", &count);
    CHECK(m && !contains(m, count, ".hidden") && !contains(m, count, ".hiddenrc"),
          "* does not match dotfiles by default");
    free_matches(m, count);

    m = shush_glob(".hid*", &count);
    CHECK_EQ_INT(count, 2, ".hid* (pattern itself starts with '.') matches dotfiles");
    free_matches(m, count);

    m = shush_glob("nomatch*.xyz", &count);
    CHECK_NULL(m, "no match returns NULL (caller falls back to literal pattern)");

    m = shush_glob("d*", &count);
    CHECK_EQ_INT(count, 2, "d* matches both directories");
    CHECK(m && strcmp(m[0], "dirA") == 0 && strcmp(m[1], "dirB") == 0,
          "d* results are sorted alphabetically");
    free_matches(m, count);

    CHECK(chdir(orig_cwd) == 0, "restored original cwd");

    char path[4200];
    const char *files[] = {"a.txt", "b.txt", "c.log", "file1.txt",
                           "file2.txt", ".hidden", ".hiddenrc"};
    for (size_t i = 0; i < sizeof(files) / sizeof(files[0]); i++)
    {
        snprintf(path, sizeof(path), "%s/%s", dir, files[i]);
        unlink(path);
    }
    snprintf(path, sizeof(path), "%s/dirA", dir);
    rmdir(path);
    snprintf(path, sizeof(path), "%s/dirB", dir);
    rmdir(path);
    rmdir(dir);

    TEST_SUMMARY();
}
