#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sqlite3.h>

/* constants */
#define DO_MAX_TODOS 1000
#define DO_MAX_NAME_LEN 64
#define DO_MAX_TITLE_LEN 256
#ifndef DO_DB_FILE
#define DO_DB_FILE "do_todos.db"
#endif

/* ANSI color codes for prettier CLI */
#define DO_COLOR_RESET "\x1b[0m"
#define DO_COLOR_BOLD "\x1b[1m"
#define DO_COLOR_RED "\x1b[31m"
#define DO_COLOR_GREEN "\x1b[32m"
#define DO_COLOR_YELLOW "\x1b[33m"
#define DO_COLOR_CYAN "\x1b[36m"

/* todo structure */
typedef struct {
    int id;
    char owner_name[DO_MAX_NAME_LEN];
    char title[DO_MAX_TITLE_LEN];
    int completed; /* 0 = not completed, 1 = completed */
} do_todo;

/* global storage */
static do_todo do_todos[DO_MAX_TODOS];
static int do_todo_count = 0;
static char do_current_user[DO_MAX_NAME_LEN] = "";
static int do_next_id = 1;

/* function declarations (do_* style) */
int do_load_data(void);
int do_save_data(void);
void do_login(void);
void do_show_menu(void);
void do_list_todos(void);
void do_create_todo(void);
int do_find_todo_index_by_id(int id, const char *owner_name);
void do_update_todo(void);
void do_toggle_complete(void);
void do_delete_todo(void);
void do_clear_completed(void);
void do_main_loop(void);
/* helpers mainly for tests */
void do_reset_state(void);
void do_add_todo_for_test(const char *owner_name, const char *title, int completed);
int do_todo_exists_for_user(const char *owner_name, const char *title, int completed);
int do_get_todo_count_for_user(const char *owner_name);
int do_string_contains_case_insensitive(const char *haystack, const char *needle);

/* utility: trim newline from fgets */
void do_trim_newline(char *s) {
    if (s == NULL) {
        return;
    }
    size_t len = strlen(s);
    if (len == 0) {
        return;
    }
    if (s[len - 1] == '\n') {
        s[len - 1] = '\0';
    }
}

void do_reset_state(void) {
    do_todo_count = 0;
    do_next_id = 1;
    do_current_user[0] = '\0';
}

void do_add_todo_for_test(const char *owner_name, const char *title, int completed) {
    if (owner_name == NULL || title == NULL) {
        return;
    }
    if (do_todo_count >= DO_MAX_TODOS) {
        return;
    }

    do_todo *t = &do_todos[do_todo_count];
    t->id = do_next_id++;
    strncpy(t->owner_name, owner_name, DO_MAX_NAME_LEN - 1);
    t->owner_name[DO_MAX_NAME_LEN - 1] = '\0';
    strncpy(t->title, title, DO_MAX_TITLE_LEN - 1);
    t->title[DO_MAX_TITLE_LEN - 1] = '\0';
    t->completed = completed ? 1 : 0;

    do_todo_count++;
}

int do_todo_exists_for_user(const char *owner_name, const char *title, int completed) {
    if (owner_name == NULL || title == NULL) {
        return 0;
    }

    for (int i = 0; i < do_todo_count; i++) {
        do_todo *t = &do_todos[i];
        if (strcmp(t->owner_name, owner_name) == 0 &&
            strcmp(t->title, title) == 0 &&
            t->completed == (completed ? 1 : 0)) {
            return 1;
        }
    }

    return 0;
}

int do_get_todo_count_for_user(const char *owner_name) {
    if (owner_name == NULL) {
        return 0;
    }

    int count = 0;
    for (int i = 0; i < do_todo_count; i++) {
        do_todo *t = &do_todos[i];
        if (strcmp(t->owner_name, owner_name) == 0) {
            count++;
        }
    }
    return count;
}

int do_string_contains_case_insensitive(const char *haystack, const char *needle) {
    if (haystack == NULL || needle == NULL) {
        return 0;
    }

    size_t needle_len = strlen(needle);
    if (needle_len == 0) {
        return 0;
    }

    for (const char *h = haystack; *h != '\0'; h++) {
        size_t i;
        for (i = 0; i < needle_len; i++) {
            char hc = h[i];
            char nc = needle[i];
            if (hc == '\0') {
                return 0;
            }
            if (tolower((unsigned char)hc) != tolower((unsigned char)nc)) {
                break;
            }
        }
        if (i == needle_len) {
            return 1;
        }
    }

    return 0;
}

int do_load_data(void) {
    sqlite3 *db = NULL;
    sqlite3_stmt *stmt = NULL;
    int rc;

    do_todo_count = 0;
    do_next_id = 1;

    rc = sqlite3_open(DO_DB_FILE, &db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, DO_COLOR_RED "Error: cannot open database file '%s': %s\n" DO_COLOR_RESET,
                DO_DB_FILE, sqlite3_errmsg(db));
        if (db != NULL) {
            sqlite3_close(db);
        }
        return -1;
    }

    const char *create_sql =
        "CREATE TABLE IF NOT EXISTS todos ("
        "id INTEGER PRIMARY KEY, "
        "owner TEXT NOT NULL, "
        "title TEXT NOT NULL, "
        "completed INTEGER NOT NULL"
        ");";

    rc = sqlite3_exec(db, create_sql, NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, DO_COLOR_RED "Error: failed to create todos table: %s\n" DO_COLOR_RESET,
                sqlite3_errmsg(db));
        sqlite3_close(db);
        return -1;
    }

    const char *select_sql =
        "SELECT id, owner, title, completed "
        "FROM todos "
        "ORDER BY id;";

    rc = sqlite3_prepare_v2(db, select_sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, DO_COLOR_RED "Error: failed to prepare select: %s\n" DO_COLOR_RESET,
                sqlite3_errmsg(db));
        sqlite3_close(db);
        return -1;
    }

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        if (do_todo_count >= DO_MAX_TODOS) {
            break;
        }

        do_todo *t = &do_todos[do_todo_count];
        t->id = sqlite3_column_int(stmt, 0);

        const unsigned char *owner_text = sqlite3_column_text(stmt, 1);
        const unsigned char *title_text = sqlite3_column_text(stmt, 2);
        int completed_val = sqlite3_column_int(stmt, 3);

        if (owner_text == NULL) {
            owner_text = (const unsigned char *)"";
        }
        if (title_text == NULL) {
            title_text = (const unsigned char *)"";
        }

        strncpy(t->owner_name, (const char *)owner_text, DO_MAX_NAME_LEN - 1);
        t->owner_name[DO_MAX_NAME_LEN - 1] = '\0';

        strncpy(t->title, (const char *)title_text, DO_MAX_TITLE_LEN - 1);
        t->title[DO_MAX_TITLE_LEN - 1] = '\0';

        t->completed = completed_val ? 1 : 0;

        if (t->id >= do_next_id) {
            do_next_id = t->id + 1;
        }

        do_todo_count++;
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);

    return 0;
}

int do_save_data(void) {
    sqlite3 *db = NULL;
    sqlite3_stmt *stmt = NULL;
    int rc;

    rc = sqlite3_open(DO_DB_FILE, &db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, DO_COLOR_RED "Error: cannot open database file '%s': %s\n" DO_COLOR_RESET,
                DO_DB_FILE, sqlite3_errmsg(db));
        if (db != NULL) {
            sqlite3_close(db);
        }
        return -1;
    }

    const char *create_sql =
        "CREATE TABLE IF NOT EXISTS todos ("
        "id INTEGER PRIMARY KEY, "
        "owner TEXT NOT NULL, "
        "title TEXT NOT NULL, "
        "completed INTEGER NOT NULL"
        ");";

    rc = sqlite3_exec(db, create_sql, NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, DO_COLOR_RED "Error: failed to create todos table: %s\n" DO_COLOR_RESET,
                sqlite3_errmsg(db));
        sqlite3_close(db);
        return -1;
    }

    rc = sqlite3_exec(db, "BEGIN TRANSACTION;", NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, DO_COLOR_RED "Error: failed to begin transaction: %s\n" DO_COLOR_RESET,
                sqlite3_errmsg(db));
        sqlite3_close(db);
        return -1;
    }

    rc = sqlite3_exec(db, "DELETE FROM todos;", NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, DO_COLOR_RED "Error: failed to clear todos table: %s\n" DO_COLOR_RESET,
                sqlite3_errmsg(db));
        sqlite3_exec(db, "ROLLBACK;", NULL, NULL, NULL);
        sqlite3_close(db);
        return -1;
    }

    const char *insert_sql =
        "INSERT INTO todos (id, owner, title, completed) "
        "VALUES (?1, ?2, ?3, ?4);";

    rc = sqlite3_prepare_v2(db, insert_sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, DO_COLOR_RED "Error: failed to prepare insert: %s\n" DO_COLOR_RESET,
                sqlite3_errmsg(db));
        sqlite3_exec(db, "ROLLBACK;", NULL, NULL, NULL);
        sqlite3_close(db);
        return -1;
    }

    for (int i = 0; i < do_todo_count; i++) {
        do_todo *t = &do_todos[i];

        sqlite3_bind_int(stmt, 1, t->id);
        sqlite3_bind_text(stmt, 2, t->owner_name, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, t->title, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 4, t->completed);

        rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE) {
            fprintf(stderr, DO_COLOR_RED "Error: failed to insert todo: %s\n" DO_COLOR_RESET,
                    sqlite3_errmsg(db));
            sqlite3_finalize(stmt);
            sqlite3_exec(db, "ROLLBACK;", NULL, NULL, NULL);
            sqlite3_close(db);
            return -1;
        }

        sqlite3_reset(stmt);
        sqlite3_clear_bindings(stmt);
    }

    sqlite3_finalize(stmt);

    rc = sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, DO_COLOR_RED "Error: failed to commit transaction: %s\n" DO_COLOR_RESET,
                sqlite3_errmsg(db));
        sqlite3_exec(db, "ROLLBACK;", NULL, NULL, NULL);
        sqlite3_close(db);
        return -1;
    }

    sqlite3_close(db);
    return 0;
}

void do_login(void) {
    char buffer[DO_MAX_NAME_LEN];

    while (1) {
        printf(DO_COLOR_BOLD "Enter your name to login: " DO_COLOR_RESET);
        if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
            printf("\n" DO_COLOR_RED "Error reading input. Please try again.\n" DO_COLOR_RESET);
            continue;
        }

        do_trim_newline(buffer);

        if (buffer[0] == '\0') {
            printf(DO_COLOR_RED "Name cannot be empty. Please try again.\n" DO_COLOR_RESET);
            continue;
        }

        strncpy(do_current_user, buffer, DO_MAX_NAME_LEN - 1);
        do_current_user[DO_MAX_NAME_LEN - 1] = '\0';

        printf(DO_COLOR_GREEN "Hello, %s!\n\n" DO_COLOR_RESET, do_current_user);
        break;
    }
}

void do_show_menu(void) {
    printf("\n" DO_COLOR_CYAN DO_COLOR_BOLD "--- TODO MENU ---" DO_COLOR_RESET "\n");
    printf(DO_COLOR_YELLOW "1" DO_COLOR_RESET ". List todos\n");
    printf(DO_COLOR_YELLOW "2" DO_COLOR_RESET ". Create todo\n");
    printf(DO_COLOR_YELLOW "3" DO_COLOR_RESET ". Update todo\n");
    printf(DO_COLOR_YELLOW "4" DO_COLOR_RESET ". Toggle complete\n");
    printf(DO_COLOR_YELLOW "5" DO_COLOR_RESET ". Delete todo (by ID or text)\n");
    printf(DO_COLOR_YELLOW "6" DO_COLOR_RESET ". Clear completed\n");
    printf(DO_COLOR_YELLOW "0" DO_COLOR_RESET ". Exit\n");
    printf(DO_COLOR_BOLD "Enter choice: " DO_COLOR_RESET);
}

void do_list_todos(void) {
    int found = 0;
    printf("\n" DO_COLOR_CYAN "Your todos:" DO_COLOR_RESET "\n");
    for (int i = 0; i < do_todo_count; i++) {
        do_todo *t = &do_todos[i];
        if (strcmp(t->owner_name, do_current_user) == 0) {
            const char *status_color = t->completed ? DO_COLOR_GREEN : DO_COLOR_YELLOW;
            printf("ID: %d | %s[%c]%s %s%s%s\n",
                   t->id,
                   status_color,
                   t->completed ? 'X' : ' ',
                   DO_COLOR_RESET,
                   DO_COLOR_BOLD,
                   t->title,
                   DO_COLOR_RESET);
            found = 1;
        }
    }
    if (!found) {
        printf(DO_COLOR_YELLOW "(no todos yet)\n" DO_COLOR_RESET);
    }
}

void do_create_todo(void) {
    if (do_todo_count >= DO_MAX_TODOS) {
        printf(DO_COLOR_RED "Cannot create more todos (limit reached).\n" DO_COLOR_RESET);
        return;
    }

    char title[DO_MAX_TITLE_LEN];
    printf(DO_COLOR_BOLD "Enter todo title: " DO_COLOR_RESET);
    if (fgets(title, sizeof(title), stdin) == NULL) {
        printf(DO_COLOR_RED "Error reading title.\n" DO_COLOR_RESET);
        return;
    }
    do_trim_newline(title);
    if (title[0] == '\0') {
        printf(DO_COLOR_RED "Title cannot be empty.\n" DO_COLOR_RESET);
        return;
    }

    do_todo *t = &do_todos[do_todo_count];
    t->id = do_next_id++;
    strncpy(t->owner_name, do_current_user, DO_MAX_NAME_LEN - 1);
    t->owner_name[DO_MAX_NAME_LEN - 1] = '\0';
    strncpy(t->title, title, DO_MAX_TITLE_LEN - 1);
    t->title[DO_MAX_TITLE_LEN - 1] = '\0';
    t->completed = 0;

    do_todo_count++;

    if (do_save_data() == 0) {
        printf(DO_COLOR_GREEN "Todo created with ID %d.\n" DO_COLOR_RESET, t->id);
    } else {
        printf(DO_COLOR_RED "Todo created but failed to save.\n" DO_COLOR_RESET);
    }
}

int do_find_todo_index_by_id(int id, const char *owner_name) {
    for (int i = 0; i < do_todo_count; i++) {
        do_todo *t = &do_todos[i];
        if (t->id == id && strcmp(t->owner_name, owner_name) == 0) {
            return i;
        }
    }
    return -1;
}

void do_update_todo(void) {
    int id;
    char buffer[32];

    printf("Enter ID of todo to update: ");
    if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
        printf(DO_COLOR_RED "Error reading input.\n" DO_COLOR_RESET);
        return;
    }

    id = atoi(buffer);
    if (id <= 0) {
        printf(DO_COLOR_RED "Invalid ID.\n" DO_COLOR_RESET);
        return;
    }

    int index = do_find_todo_index_by_id(id, do_current_user);
    if (index < 0) {
        printf(DO_COLOR_RED "Todo with ID %d not found for user %s.\n" DO_COLOR_RESET, id, do_current_user);
        return;
    }

    do_todo *t = &do_todos[index];
    char title[DO_MAX_TITLE_LEN];
    printf("Current title: %s%s%s\n", DO_COLOR_BOLD, t->title, DO_COLOR_RESET);
    printf(DO_COLOR_BOLD "Enter new title (leave empty to keep current): " DO_COLOR_RESET);
    if (fgets(title, sizeof(title), stdin) == NULL) {
        printf(DO_COLOR_RED "Error reading title.\n" DO_COLOR_RESET);
        return;
    }
    do_trim_newline(title);
    if (title[0] != '\0') {
        strncpy(t->title, title, DO_MAX_TITLE_LEN - 1);
        t->title[DO_MAX_TITLE_LEN - 1] = '\0';
    }

    printf("Current status: %s%s%s\n",
           t->completed ? DO_COLOR_GREEN : DO_COLOR_YELLOW,
           t->completed ? "completed" : "not completed",
           DO_COLOR_RESET);
    printf(DO_COLOR_BOLD "Toggle status? (y/N): " DO_COLOR_RESET);
    if (fgets(buffer, sizeof(buffer), stdin) != NULL) {
        if (buffer[0] == 'y' || buffer[0] == 'Y') {
            t->completed = t->completed ? 0 : 1;
        }
    }

    if (do_save_data() == 0) {
        printf(DO_COLOR_GREEN "Todo updated.\n" DO_COLOR_RESET);
    } else {
        printf(DO_COLOR_RED "Todo updated but failed to save.\n" DO_COLOR_RESET);
    }
}

void do_toggle_complete(void) {
    int id;
    char buffer[32];

    printf("Enter ID of todo to toggle: ");
    if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
        printf(DO_COLOR_RED "Error reading input.\n" DO_COLOR_RESET);
        return;
    }

    id = atoi(buffer);
    if (id <= 0) {
        printf(DO_COLOR_RED "Invalid ID.\n" DO_COLOR_RESET);
        return;
    }

    int index = do_find_todo_index_by_id(id, do_current_user);
    if (index < 0) {
        printf(DO_COLOR_RED "Todo with ID %d not found for user %s.\n" DO_COLOR_RESET, id, do_current_user);
        return;
    }

    do_todo *t = &do_todos[index];
    t->completed = t->completed ? 0 : 1;

    if (do_save_data() == 0) {
        printf(DO_COLOR_GREEN "Todo status toggled.\n" DO_COLOR_RESET);
    } else {
        printf(DO_COLOR_RED "Todo status toggled but failed to save.\n" DO_COLOR_RESET);
    }
}

void do_delete_todo(void) {
    char buffer[32];

    printf(DO_COLOR_BOLD "Enter ID or text of todo to delete: " DO_COLOR_RESET);
    if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
        printf(DO_COLOR_RED "Error reading input.\n" DO_COLOR_RESET);
        return;
    }

    do_trim_newline(buffer);
    if (buffer[0] == '\0') {
        printf(DO_COLOR_RED "Input cannot be empty.\n" DO_COLOR_RESET);
        return;
    }

    size_t len = strlen(buffer);
    int is_numeric = 1;
    for (size_t i = 0; i < len; i++) {
        if (!isdigit((unsigned char)buffer[i])) {
            is_numeric = 0;
            break;
        }
    }

    if (is_numeric) {
        int id = atoi(buffer);
        if (id <= 0) {
            printf(DO_COLOR_RED "Invalid ID.\n" DO_COLOR_RESET);
            return;
        }

        int index = do_find_todo_index_by_id(id, do_current_user);
        if (index < 0) {
            printf(DO_COLOR_RED "Todo with ID %d not found for user %s.\n" DO_COLOR_RESET, id, do_current_user);
            return;
        }

        printf(DO_COLOR_RED "Are you sure you want to delete todo with ID %d? (y/N): " DO_COLOR_RESET, id);
        if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
            printf(DO_COLOR_RED "Error reading input.\n" DO_COLOR_RESET);
            return;
        }
        if (!(buffer[0] == 'y' || buffer[0] == 'Y')) {
            printf(DO_COLOR_YELLOW "Delete cancelled.\n" DO_COLOR_RESET);
            return;
        }

        for (int i = index; i < do_todo_count - 1; i++) {
            do_todos[i] = do_todos[i + 1];
        }
        do_todo_count--;

        if (do_save_data() == 0) {
            printf(DO_COLOR_GREEN "Todo deleted.\n" DO_COLOR_RESET);
        } else {
            printf(DO_COLOR_RED "Todo deleted but failed to save.\n" DO_COLOR_RESET);
        }
    } else {
        int removed = 0;
        for (int i = 0; i < do_todo_count; i++) {
            do_todo *t = &do_todos[i];
            if (strcmp(t->owner_name, do_current_user) == 0 &&
                do_string_contains_case_insensitive(t->title, buffer)) {
                printf(DO_COLOR_RED "Delete todo ID %d: \"%s\"? (y/N): " DO_COLOR_RESET, t->id, t->title);
                if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
                    printf(DO_COLOR_RED "Error reading input.\n" DO_COLOR_RESET);
                    break;
                }
                if (buffer[0] == 'y' || buffer[0] == 'Y') {
                    for (int j = i; j < do_todo_count - 1; j++) {
                        do_todos[j] = do_todos[j + 1];
                    }
                    do_todo_count--;
                    removed++;
                    i--;
                }
            }
        }

        if (removed == 0) {
            printf(DO_COLOR_YELLOW "No todos matching \"%s\" found for user %s.\n" DO_COLOR_RESET,
                   buffer, do_current_user);
            return;
        }

        if (do_save_data() == 0) {
            printf(DO_COLOR_GREEN "Deleted %d todos matching \"%s\".\n" DO_COLOR_RESET, removed, buffer);
        } else {
            printf(DO_COLOR_RED "Todos deleted but failed to save.\n" DO_COLOR_RESET);
        }
    }
}

void do_clear_completed(void) {
    int write_index = 0;
    int removed = 0;

    for (int i = 0; i < do_todo_count; i++) {
        do_todo *t = &do_todos[i];
        if (strcmp(t->owner_name, do_current_user) == 0 && t->completed) {
            removed++;
            continue;
        }
        if (write_index != i) {
            do_todos[write_index] = do_todos[i];
        }
        write_index++;
    }

    do_todo_count = write_index;

    if (removed == 0) {
        printf(DO_COLOR_YELLOW "No completed todos to clear.\n" DO_COLOR_RESET);
        return;
    }

    if (do_save_data() == 0) {
        printf(DO_COLOR_GREEN "Cleared %d completed todos.\n" DO_COLOR_RESET, removed);
    } else {
        printf(DO_COLOR_RED "Completed todos cleared but failed to save.\n" DO_COLOR_RESET);
    }
}

void do_main_loop(void) {
    char buffer[32];

    while (1) {
        do_show_menu();
        if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
            printf("\n" DO_COLOR_RED "Error reading input.\n" DO_COLOR_RESET);
            continue;
        }

        int choice = atoi(buffer);
        switch (choice) {
            case 1:
                do_list_todos();
                break;
            case 2:
                do_create_todo();
                break;
            case 3:
                do_update_todo();
                break;
            case 4:
                do_toggle_complete();
                break;
            case 5:
                do_delete_todo();
                break;
            case 6:
                do_clear_completed();
                break;
            case 0:
                printf(DO_COLOR_CYAN "Exiting...\n" DO_COLOR_RESET);
                return;
            default:
                printf(DO_COLOR_RED "Invalid choice. Please try again.\n" DO_COLOR_RESET);
                break;
        }
    }
}

int do_main(void) {
    if (do_load_data() != 0) {
        printf(DO_COLOR_RED "Warning: could not load existing data.\n" DO_COLOR_RESET);
    }

    do_login();
    do_main_loop();

    if (do_save_data() != 0) {
        printf(DO_COLOR_RED "Warning: could not save data on exit.\n" DO_COLOR_RESET);
    }

    return 0;
}

#ifndef DO_TEST
int main(void) {
    return do_main();
}
#endif