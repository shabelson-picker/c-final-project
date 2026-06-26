#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "html_builder.h"

void html_escape(FILE *f, const char *s) {
    for (; *s; s++) {
        switch (*s) {
            case '&': fputs("&amp;",  f); break;
            case '<': fputs("&lt;",   f); break;
            case '>': fputs("&gt;",   f); break;
            case '"': fputs("&quot;", f); break;
            default:  fputc(*s, f);       break;
        }
    }
}

/* Shared modern stylesheet + document open. */
void html_doc_open(FILE *f, const char *title) {
    fprintf(f, "<!DOCTYPE html>\n<html lang=\"en\">\n<head>\n<meta charset=\"utf-8\">\n"
               "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n<title>");
    html_escape(f, title);
    fprintf(f, "</title>\n");
    fprintf(f,
        "<style>\n"
        ":root{--bg:#0f172a;--panel:#ffffff;--ink:#1e293b;--muted:#64748b;"
        "--line:#e2e8f0;--accent:#6366f1;--accent2:#8b5cf6;--ok:#16a34a;--warn:#dc2626;--amber:#d97706}\n"
        "*{box-sizing:border-box}\n"
        "body{font-family:'Segoe UI',-apple-system,Roboto,Helvetica,Arial,sans-serif;"
        "margin:0;background:#f1f5f9;color:var(--ink);line-height:1.5}\n"
        ".wrap{max-width:1100px;margin:0 auto;padding:32px 20px 64px}\n"
        ".hero{background:linear-gradient(120deg,var(--accent),var(--accent2));color:#fff;"
        "border-radius:16px;padding:28px 32px;box-shadow:0 10px 30px rgba(99,102,241,.25)}\n"
        ".hero h1{margin:0;font-size:28px;letter-spacing:.3px}\n"
        ".hero .meta{margin:6px 0 0;color:rgba(255,255,255,.85);font-size:13px}\n"
        "h2{font-size:18px;margin:34px 0 12px;padding-bottom:6px;border-bottom:2px solid var(--line)}\n"
        ".card{background:var(--panel);border:1px solid var(--line);border-radius:14px;"
        "padding:6px 18px 18px;box-shadow:0 1px 3px rgba(15,23,42,.06);margin-bottom:18px}\n"
        ".kpis{display:flex;flex-wrap:wrap;gap:14px;margin:18px 0}\n"
        ".kpi{flex:1;min-width:150px;background:var(--panel);border:1px solid var(--line);"
        "border-radius:14px;padding:16px 18px;box-shadow:0 1px 3px rgba(15,23,42,.06)}\n"
        ".kpi .n{font-size:26px;font-weight:700;color:var(--accent)}\n"
        ".kpi .l{font-size:12px;color:var(--muted);text-transform:uppercase;letter-spacing:.6px}\n"
        "table{border-collapse:collapse;width:100%%;font-size:13px}\n"
        "th,td{padding:9px 12px;text-align:left;border-bottom:1px solid var(--line)}\n"
        "th{color:var(--muted);font-size:11px;text-transform:uppercase;letter-spacing:.5px}\n"
        "tbody tr:hover{background:#f8fafc} td.crit{color:var(--warn);font-weight:600}\n"
        ".pill{display:inline-block;padding:2px 10px;border-radius:999px;font-size:11px;font-weight:600}\n"
        ".pill.ok{background:#dcfce7;color:var(--ok)} .pill.over{background:#fee2e2;color:var(--warn)}\n"
        ".pill.idle{background:#f1f5f9;color:var(--muted)}\n"
        ".bar{height:8px;border-radius:999px;background:var(--line);overflow:hidden;min-width:90px}\n"
        ".bar>span{display:block;height:100%%;background:linear-gradient(90deg,var(--ok),#22c55e)}\n"
        "pre.gantt{font-family:Consolas,'Courier New',monospace;font-size:12px;line-height:1.4;"
        "background:#0f172a;color:#e2e8f0;border-radius:12px;padding:14px;overflow-x:auto}\n"
        "pre.gantt .opt{font-weight:bold;color:#fff} pre.gantt .exp{color:#94a3b8} pre.gantt .pess{color:var(--amber)}\n"
        "pre.gantt .crit{color:#f87171} pre.gantt .noncrit{color:#4ade80}\n"
        ".legend{color:var(--muted);font-size:12px} .warn{color:var(--warn);font-weight:600}\n"
        "svg{max-width:100%%;height:auto}\n"
        "</style>\n</head>\n<body>\n<div class=\"wrap\">\n");
}

void html_doc_close(FILE *f) {
    fprintf(f, "</div>\n</body>\n</html>\n");
}

void html_hero(FILE *f, const char *title, const char *meta_html) {
    fprintf(f, "<div class=\"hero\"><h1>");
    html_escape(f, title);
    fprintf(f, "</h1>\n<p class=\"meta\">%s</p></div>\n", meta_html);
}

void html_pill(FILE *f, const char *cls, const char *text) {
    fprintf(f, "<span class=\"pill %s\">%s</span>", cls, text);
}

void html_progress_bar(FILE *f, int pct) {
    fprintf(f, "<div class=\"bar\"><span style=\"width:%d%%\"></span></div>%d%%", pct, pct);
}

int html_inline_svg(FILE *f, const char *svgpath) {
    FILE *s = fopen(svgpath, "rb");
    long sz;
    size_t rd;
    char *buf, *start;

    if (!s) return 0;
    fseek(s, 0, SEEK_END);
    sz = ftell(s);
    fseek(s, 0, SEEK_SET);
    if (sz <= 0) { fclose(s); return 0; }

    buf = (char *)malloc((size_t)sz + 1);
    if (!buf) { fclose(s); return 0; }
    rd = fread(buf, 1, (size_t)sz, s);
    buf[rd] = '\0';
    fclose(s);

    start = strstr(buf, "<svg");
    if (start) fwrite(start, 1, strlen(start), f);
    free(buf);
    return start != NULL;
}
