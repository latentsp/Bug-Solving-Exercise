/* Very basic manual tests for the todo CLI core logic.
 * Build examples (from project directory):
 *   cc -Wall -Wextra -lsqlite3 -o do_main do_main.c
 *   cc -Wall -Wextra -lsqlite3 -DDO_TEST -DDO_DB_FILE=\"do_todos_test.db\" -o do_tests do_main.c do_tests.c
 *
 * These tests are minimal and focus on non-interactive helpers.
 */

#include <stdio.h>
#include <string.h>

/* forward declarations from do_main.c so we can call them here */
int do_load_data(void);
int do_save_data(void);
void do_trim_newline(char *s);
void do_reset_state(void);
void do_add_todo_for_test(const char *owner_name, const char *title, int completed);
int do_todo_exists_for_user(const char *owner_name, const char *title, int completed);
int do_get_todo_count_for_user(const char *owner_name);

/* test helper */
static void do_test_result(const char *name, int passed) {
    printf("[TEST] %s: %s\n", name, passed ? "OK" : "FAIL");
}

static void do_test_trim_newline(void) {
    char s1[] = "hello\n";
    do_trim_newline(s1);
    do_test_result("do_trim_newline removes newline", strcmp(s1, "hello") == 0);

    char s2[] = "no-newline";
    do_trim_newline(s2);
    do_test_result("do_trim_newline keeps string without newline", strcmp(s2, "no-newline") == 0);
}

static void do_test_load_and_save_empty(void) {
    /* This test is basic and just calls the functions to ensure they link and run.
     * In a real test, we would isolate the file system or use a temp file.
     */
    int r1 = do_load_data();
    int r2 = do_save_data();
    do_test_result("do_load_data basic call", r1 == 0);
    do_test_result("do_save_data basic call", r2 == 0);
}

static void do_test_persistence_roundtrip(void) {
    const char *owner = "tester";

    do_reset_state();
    do_add_todo_for_test(owner, "first task", 0);
    do_add_todo_for_test(owner, "second task", 1);

    int save_result = do_save_data();

    do_reset_state();
    int load_result = do_load_data();

    int count = do_get_todo_count_for_user(owner);
    int has_first = do_todo_exists_for_user(owner, "first task", 0);
    int has_second = do_todo_exists_for_user(owner, "second task", 1);

    int passed = (save_result == 0) &&
                 (load_result == 0) &&
                 (count >= 2) &&
                 has_first &&
                 has_second;

    do_test_result("persistence roundtrip for two todos", passed);
}

int do_run_tests(void) {
    do_test_trim_newline();
    do_test_load_and_save_empty();
    do_test_persistence_roundtrip();
    return 0;
}

int main(void) {
    return do_run_tests();
}


