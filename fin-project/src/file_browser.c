#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <io.h>
#include <direct.h>
#include <windows.h>
#include "file_browser.h"
#include "constants.h"
#include "ui.h"

#define MAX_ENTRIES 256

void get_exe_dir(char *dir, int max) {
    char exe[MAX_PATH_LEN];
    char *sep;
    GetModuleFileNameA(NULL, exe, max);
    sep = strrchr(exe, '\\');
    if (sep) {
        int len = (int)(sep - exe) + 1;  /* include trailing backslash */
        if (len < max) { strncpy(dir, exe, len); dir[len] = '\0'; return; }
    }
    dir[0] = '\0';
}

typedef struct {
    char name[MAX_PATH_LEN];
    int  is_dir;
} FileEntry;

static int cmp_entries(const void *a, const void *b) {
    const FileEntry *fa = (const FileEntry *)a;
    const FileEntry *fb = (const FileEntry *)b;
    if (fa->is_dir != fb->is_dir) return fb->is_dir - fa->is_dir; /* dirs first */
    return strcmp(fa->name, fb->name);
}

static int load_entries(FileEntry *entries, int max) {
    struct _finddata_t fd;
    intptr_t handle;
    int count = 0;

    handle = _findfirst("*", &fd);
    if (handle == -1) return 0;

    do {
        if (strcmp(fd.name, ".") == 0) continue;
        if (count >= max) break;
        strncpy(entries[count].name, fd.name, MAX_PATH_LEN - 1);
        entries[count].name[MAX_PATH_LEN - 1] = '\0';
        entries[count].is_dir = (fd.attrib & _A_SUBDIR) ? 1 : 0;
        count++;
    } while (_findnext(handle, &fd) == 0);

    _findclose(handle);
    qsort(entries, count, sizeof(FileEntry), cmp_entries);
    return count;
}

typedef enum { MODE_FILE, MODE_DIR } BrowserMode;

static void draw_panel(const FileEntry *entries, int count, const char *cwd, BrowserMode mode) {
    int i;
    printf("\n  +------+----------\n");
    printf("  | %s\n", cwd);
    printf("  +------+----------\n");
    for (i = 0; i < count; i++) {
        const char *tag = entries[i].is_dir ? "DIR" : "   ";
        printf("  | %3d  | [%s] %s\n", i + 1, tag, entries[i].name);
    }
    printf("  +------+----------\n");
    if (mode == MODE_FILE)
        printf("  Enter index to open dir / select file\n");
    else
        printf("  Enter index to open dir  |  [-2] Select this folder\n");
    printf("  [-1] New directory  [0] Cancel\n");
}

static int browser_loop(char *out_path, int max_len, BrowserMode mode) {
    FileEntry *entries = (FileEntry *)malloc(MAX_ENTRIES * sizeof(FileEntry));
    char cwd[MAX_PATH_LEN];
    char original_cwd[MAX_PATH_LEN];
    int count, choice;

    if (!entries) { printf("  Error: out of memory.\n"); return 0; }

    _getcwd(original_cwd, MAX_PATH_LEN);
    {
        char exe_dir[MAX_PATH_LEN];
        get_exe_dir(exe_dir, MAX_PATH_LEN);
        if (exe_dir[0]) _chdir(exe_dir);
    }

    while (1) {
        screen_clear();
        _getcwd(cwd, MAX_PATH_LEN);
        count = load_entries(entries, MAX_ENTRIES);
        draw_panel(entries, count, cwd, mode);

        printf("  > ");
        if (scanf("%d", &choice) != 1) {
            int c; while ((c = getchar()) != '\n' && c != EOF) {}
            printf("  Invalid input.\n");
            continue;
        }
        { int c; while ((c = getchar()) != '\n' && c != EOF) {} }

        if (choice == 0) { _chdir(original_cwd); free(entries); return 0; }

        if (choice == -2 && mode == MODE_DIR) {
            strncpy(out_path, cwd, max_len - 1);
            out_path[max_len - 1] = '\0';
            _chdir(original_cwd);
            free(entries);
            return 1;
        }

        if (choice == -1) {
            char dirname[MAX_PATH_LEN];
            printf("  New directory name: ");
            if (fgets(dirname, MAX_PATH_LEN, stdin)) {
                size_t len = strlen(dirname);
                if (len > 0 && dirname[len - 1] == '\n') dirname[len - 1] = '\0';
                if (dirname[0] != '\0') {
                    if (_mkdir(dirname) == 0) printf("  Created '%s'.\n", dirname);
                    else                      printf("  Could not create directory.\n");
                }
            }
            continue;
        }

        if (choice < 1 || choice > count) { printf("  Out of range.\n"); continue; }

        if (entries[choice - 1].is_dir) {
            _chdir(entries[choice - 1].name);
        } else if (mode == MODE_FILE) {
            snprintf(out_path, max_len, "%s\\%s", cwd, entries[choice - 1].name);
            _chdir(original_cwd);
            free(entries);
            return 1;
        } else {
            printf("  Navigate into directories or press -2 to select this folder.\n");
        }
    }
}

int file_browser(char *out_path, int max_len) {
    return browser_loop(out_path, max_len, MODE_FILE);
}

int dir_browser(char *out_dir, int max_len) {
    return browser_loop(out_dir, max_len, MODE_DIR);
}

