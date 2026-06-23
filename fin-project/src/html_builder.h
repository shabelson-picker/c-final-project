#ifndef HTML_BUILDER_H
#define HTML_BUILDER_H

#include <stdio.h>

/* ===========================================================================
 * Reusable HTML report primitives: text escaping, a shared stylesheet
 * ("design system") + document open/close, SVG inlining, and opening the
 * result in a browser. Domain-agnostic - callers decide what data goes in;
 * this provides the chrome. Used by report_exporter.c.
 * ========================================================================= */

/// <summary>Escape text into an HTML stream (& &lt; &gt; ").</summary>
void html_escape(FILE *f, const char *s);

/// <summary>Write DOCTYPE + head (with the shared stylesheet) and open the body
/// wrapper. `title` is escaped into &lt;title&gt;.</summary>
void html_doc_open(FILE *f, const char *title);

/// <summary>Close the body wrapper opened by html_doc_open.</summary>
void html_doc_close(FILE *f);

/// <summary>Hero banner: an &lt;h1&gt; with the escaped `title`, plus a meta line.
/// `meta_html` is written as-is (may contain entities/markup).</summary>
void html_hero(FILE *f, const char *title, const char *meta_html);

/// <summary>Status pill: &lt;span class="pill `cls`"&gt;`text`&lt;/span&gt;
/// (cls is e.g. "ok" / "over" / "idle"). Text is written as-is.</summary>
void html_pill(FILE *f, const char *cls, const char *text);

/// <summary>Progress bar filled to `pct`%, followed by the "pct%" label.</summary>
void html_progress_bar(FILE *f, int pct);

/// <summary>Inline an .svg file into the stream, skipping its xml/doctype
/// prolog. Returns 1 if anything was written, 0 if missing/empty.</summary>
int  html_inline_svg(FILE *f, const char *svgpath);

/// <summary>Open a file in the platform default browser.</summary>
void html_open_in_browser(const char *filename);

#endif /* HTML_BUILDER_H */
