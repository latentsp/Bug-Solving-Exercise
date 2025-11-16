## CLI Todo App (C)

This is a very simple multi-user CLI todo list application written in C.  
Users log in by name, then manage their own todos (stored in a single shared SQLite database file).

### Features

- **Login by name** (no password)
- **Per-user todos** in a shared SQLite database file
- **CRUD**:
  - Create todos
  - List todos
  - Update title and status
  - Toggle complete
  - Delete
- **Clear completed** todos for the current user
- **Persistent storage** between runs

### Build and run the app

From the project root (`/Users/jonathan_yarkoni/Chaos-c`):

```bash
cc -Wall -Wextra -lsqlite3 -o do_main do_main.c
./do_main
```

- **Database file**:
  - By default, todos are stored in `do_todos.db` in the current working directory.
  - You can override the database file name at compile time:

```bash
cc -Wall -Wextra -lsqlite3 -DDO_DB_FILE=\"my_todos.db\" -o do_main do_main.c
./do_main
```

### Using the app

- **On start**:
  - You will be prompted: `Enter your name to login:`
  - Type any non-empty name; your todos will be filtered by this name.
- **Menu options**:
  - `1` – List todos for the current user
  - `2` – Create a new todo
  - `3` – Update an existing todo (title and optionally status)
  - `4` – Toggle completion status by ID
  - `5` – Delete a todo by ID (with confirmation)
  - `6` – Clear all completed todos for the current user
  - `0` – Exit (todos are saved before exit)

### Build and run the tests

Tests live in `do_tests.c` and are non-interactive; they just print test results.
They use a **separate database file** so they do not touch your real todos.

From the project root:

```bash
cc -Wall -Wextra -lsqlite3 -DDO_TEST -DDO_DB_FILE=\"do_todos_test.db\" -o do_tests do_main.c do_tests.c
./do_tests
```

The tests currently cover:

- **do_trim_newline** basic behavior
- **Basic load/save** calls
- **Persistence roundtrip**: create todos in memory, save them, reset state, reload, and verify that the todos are still present


