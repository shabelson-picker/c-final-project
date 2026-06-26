#ifndef FILE_BROWSER_H
#define FILE_BROWSER_H

/* Returns the directory of the running executable with trailing backslash. */
void get_exe_dir(char *dir, int max);

/*
 * Interactive indexed file browser.
 * Starts in the exe directory. Dirs and files are numbered.
 * User enters an index to navigate into a dir or select a file.
 * Returns 1 if a file was selected (path written to out_path), 0 if cancelled.
 */
int file_browser(char *out_path, int max_len);

/* Navigate to a directory and select it. Returns 1 on selection, 0 on cancel. */
int dir_browser(char *out_dir, int max_len);

#endif /* FILE_BROWSER_H */
