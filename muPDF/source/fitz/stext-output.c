#include "mupdf/fitz.h"

#define SUBSCRIPT_OFFSET 0.2f
#define SUPERSCRIPT_OFFSET -0.2f

#include <ft2build.h>
#include FT_FREETYPE_H
#include <Math.h>
#include <Crtdbg.h>

#define Max(a,b)  ((a) > (b) ? a : b)
#define Min(a,b)  ((a) < (b) ? a : b)

/* HTML output (visual formatting with preserved layout) */

static int
detect_super_script(fz_stext_line *line, fz_stext_char *ch)
{
	if (line->wmode == 0 && line->dir.x == 1 && line->dir.y == 0)
		return ch->origin.y < line->first_char->origin.y - ch->size * 0.1f;
	return 0;
}

static const char *
font_full_name(fz_context *ctx, fz_font *font)
{
	const char *name = fz_font_name(ctx, font);
	const char *s = strchr(name, '+');
	return s ? s + 1 : name;
}

static void
font_family_name(fz_context *ctx, fz_font *font, char *buf, int size, int is_mono, int is_serif)
{
	const char *name = font_full_name(ctx, font);
	char *s;
	fz_strlcpy(buf, name, size);
	s = strrchr(buf, '-');
	if (s)
		*s = 0;
	if (is_mono)
		fz_strlcat(buf, ",monospace", size);
	else
		fz_strlcat(buf, is_serif ? ",serif" : ",sans-serif", size);
}

static void
fz_print_style_begin_html(fz_context *ctx, fz_output *out, fz_font *font, float size, int sup, int color)
{
	char family[80];

	int is_bold = fz_font_is_bold(ctx, font);
	int is_italic = fz_font_is_italic(ctx, font);
	int is_serif = fz_font_is_serif(ctx, font);
	int is_mono = fz_font_is_monospaced(ctx, font);

	font_family_name(ctx, font, family, sizeof family, is_mono, is_serif);

	if (sup) fz_write_string(ctx, out, "<sup>");
	if (is_mono) fz_write_string(ctx, out, "<tt>");
	if (is_bold) fz_write_string(ctx, out, "<b>");
	if (is_italic) fz_write_string(ctx, out, "<i>");
	fz_write_printf(ctx, out, "<span style=\"font-family:%s;font-size:%gpt", family, size);
	if (color != 0)
		fz_write_printf(ctx, out, ";color:#%06x", color);
	fz_write_printf(ctx, out, "\">");
}

static void
fz_print_style_end_html(fz_context *ctx, fz_output *out, fz_font *font, float size, int sup)
{
	int is_mono = fz_font_is_monospaced(ctx, font);
	int is_bold = fz_font_is_bold(ctx,font);
	int is_italic = fz_font_is_italic(ctx, font);

	fz_write_string(ctx, out, "</span>");
	if (is_italic) fz_write_string(ctx, out, "</i>");
	if (is_bold) fz_write_string(ctx, out, "</b>");
	if (is_mono) fz_write_string(ctx, out, "</tt>");
	if (sup) fz_write_string(ctx, out, "</sup>");
}

static void
fz_print_stext_image_as_html(fz_context *ctx, fz_output *out, fz_stext_block *block)
{
	int x = block->bbox.x0;
	int y = block->bbox.y0;
	int w = block->bbox.x1 - block->bbox.x0;
	int h = block->bbox.y1 - block->bbox.y0;

	fz_write_printf(ctx, out, "<img style=\"position:absolute;top:%dpt;left:%dpt;width:%dpt;height:%dpt\" src=\"", y, x, w, h);
	fz_write_image_as_data_uri(ctx, out, block->u.i.image);
	fz_write_string(ctx, out, "\">\n");
}

void
fz_print_stext_block_as_html(fz_context *ctx, fz_output *out, fz_stext_block *block)
{
	fz_stext_line *line;
	fz_stext_char *ch;
	int x, y;

	fz_font *font = NULL;
	float size = 0;
	int sup = 0;
	int color = 0;

	for (line = block->u.t.first_line; line; line = line->next)
	{
		x = line->bbox.x0;
		y = line->bbox.y0;

		fz_write_printf(ctx, out, "<p style=\"position:absolute;white-space:pre;margin:0;padding:0;top:%dpt;left:%dpt\">", y, x);
		font = NULL;

		for (ch = line->first_char; ch; ch = ch->next)
		{
			int ch_sup = detect_super_script(line, ch);
			if (ch->font != font || ch->size != size || ch_sup != sup || ch->color != color)
			{
				if (font)
					fz_print_style_end_html(ctx, out, font, size, sup);
				font = ch->font;
				size = ch->size;
				color = ch->color;
				sup = ch_sup;
				fz_print_style_begin_html(ctx, out, font, size, sup, color);
			}

			switch (ch->c)
			{
			default:
				if (ch->c >= 32 && ch->c <= 127)
					fz_write_byte(ctx, out, ch->c);
				else
					fz_write_printf(ctx, out, "&#x%x;", ch->c);
				break;
			case '<': fz_write_string(ctx, out, "&lt;"); break;
			case '>': fz_write_string(ctx, out, "&gt;"); break;
			case '&': fz_write_string(ctx, out, "&amp;"); break;
			case '"': fz_write_string(ctx, out, "&quot;"); break;
			case '\'': fz_write_string(ctx, out, "&apos;"); break;
			case '\\': fz_write_string(ctx, out, "&bsol;"); break;
			}
		}

		if (font)
			fz_print_style_end_html(ctx, out, font, size, sup);

		fz_write_string(ctx, out, "</p>\n");
	}
}

void
fz_print_stext_page_as_html(fz_context *ctx, fz_output *out, fz_stext_page *page, int id)
{
	fz_stext_block *block;

	int w = page->mediabox.x1 - page->mediabox.x0;
	int h = page->mediabox.y1 - page->mediabox.y0;

	fz_write_printf(ctx, out, "<div id=\"page%d\" style=\"position:relative;width:%dpt;height:%dpt;background-color:white\">\n", id, w, h);

	for (block = page->first_block; block; block = block->next)
	{
		if (block->type == FZ_STEXT_BLOCK_IMAGE)
			fz_print_stext_image_as_html(ctx, out, block);
		else if (block->type == FZ_STEXT_BLOCK_TEXT)
			fz_print_stext_block_as_html(ctx, out, block);
	}

	fz_write_string(ctx, out, "</div>\n");
}

void
fz_print_stext_header_as_html(fz_context *ctx, fz_output *out)
{
	fz_write_string(ctx, out, "<!DOCTYPE html>\n");
	fz_write_string(ctx, out, "<html>\n");
	fz_write_string(ctx, out, "<head>\n");
	fz_write_string(ctx, out, "<style>\n");
	fz_write_string(ctx, out, "body{background-color:gray}\n");
	fz_write_string(ctx, out, "div{margin:1em auto}\n");
	fz_write_string(ctx, out, "</style>\n");
	fz_write_string(ctx, out, "</head>\n");
	fz_write_string(ctx, out, "<body>\n");
}

void
fz_print_stext_trailer_as_html(fz_context *ctx, fz_output *out)
{
	fz_write_string(ctx, out, "</body>\n");
	fz_write_string(ctx, out, "</html>\n");
}

/* XHTML output (semantic, little layout, suitable for reflow) */

static void
fz_print_stext_image_as_xhtml(fz_context *ctx, fz_output *out, fz_stext_block *block)
{
	int w = block->bbox.x1 - block->bbox.x0;
	int h = block->bbox.y1 - block->bbox.y0;

	fz_write_printf(ctx, out, "<p><img width=\"%d\" height=\"%d\" src=\"", w, h);
	fz_write_image_as_data_uri(ctx, out, block->u.i.image);
	fz_write_string(ctx, out, "\"/></p>\n");
}

static void
fz_print_style_begin_xhtml(fz_context *ctx, fz_output *out, fz_font *font, int sup)
{
	int is_mono = fz_font_is_monospaced(ctx, font);
	int is_bold = fz_font_is_bold(ctx, font);
	int is_italic = fz_font_is_italic(ctx, font);

	if (sup)
		fz_write_string(ctx, out, "<sup>");
	if (is_mono)
		fz_write_string(ctx, out, "<tt>");
	if (is_bold)
		fz_write_string(ctx, out, "<b>");
	if (is_italic)
		fz_write_string(ctx, out, "<i>");
}

static void
fz_print_style_end_xhtml(fz_context *ctx, fz_output *out, fz_font *font, int sup)
{
	int is_mono = fz_font_is_monospaced(ctx, font);
	int is_bold = fz_font_is_bold(ctx, font);
	int is_italic = fz_font_is_italic(ctx, font);

	if (is_italic)
		fz_write_string(ctx, out, "</i>");
	if (is_bold)
		fz_write_string(ctx, out, "</b>");
	if (is_mono)
		fz_write_string(ctx, out, "</tt>");
	if (sup)
		fz_write_string(ctx, out, "</sup>");
}

static float avg_font_size_of_line(fz_stext_char *ch)
{
	float size = 0;
	int n = 0;
	if (!ch)
		return 0;
	while (ch)
	{
		size += ch->size;
		++n;
		ch = ch->next;
	}
	return size / n;
}

static const char *tag_from_font_size(float size)
{
	if (size >= 20) return "h1";
	if (size >= 15) return "h2";
	if (size >= 12) return "h3";
	return "p";
}

static void fz_print_stext_block_as_xhtml(fz_context *ctx, fz_output *out, fz_stext_block *block)
{
	fz_stext_line *line;
	fz_stext_char *ch;

	fz_font *font = NULL;
	int sup = 0;
	int sp = 1;
	const char *tag = NULL;
	const char *new_tag;

	for (line = block->u.t.first_line; line; line = line->next)
	{
		new_tag = tag_from_font_size(avg_font_size_of_line(line->first_char));
		if (tag != new_tag)
		{
			if (tag)
			{
				if (font)
					fz_print_style_end_xhtml(ctx, out, font, sup);
				fz_write_printf(ctx, out, "</%s>", tag);
			}
			tag = new_tag;
			fz_write_printf(ctx, out, "<%s>", tag);
			if (font)
				fz_print_style_begin_xhtml(ctx, out, font, sup);
		}

		if (!sp)
			fz_write_byte(ctx, out, ' ');

		for (ch = line->first_char; ch; ch = ch->next)
		{
			int ch_sup = detect_super_script(line, ch);
			if (ch->font != font || ch_sup != sup)
			{
				if (font)
					fz_print_style_end_xhtml(ctx, out, font, sup);
				font = ch->font;
				sup = ch_sup;
				fz_print_style_begin_xhtml(ctx, out, font, sup);
			}

			sp = (ch->c == ' ');
			switch (ch->c)
			{
			default:
				if (ch->c >= 32 && ch->c <= 127)
					fz_write_byte(ctx, out, ch->c);
				else
					fz_write_printf(ctx, out, "&#x%x;", ch->c);
				break;
			case '<': fz_write_string(ctx, out, "&lt;"); break;
			case '>': fz_write_string(ctx, out, "&gt;"); break;
			case '&': fz_write_string(ctx, out, "&amp;"); break;
			case '"': fz_write_string(ctx, out, "&quot;"); break;
			case '\'': fz_write_string(ctx, out, "&apos;"); break;
			case '\\': fz_write_string(ctx, out, "&bsol;"); break;
			}
		}
	}

	if (font)
		fz_print_style_end_xhtml(ctx, out, font, sup);
	fz_write_printf(ctx, out, "</%s>\n", tag);
}

void
fz_print_stext_page_as_xhtml(fz_context *ctx, fz_output *out, fz_stext_page *page, int id)
{
	fz_stext_block *block;

	fz_write_printf(ctx, out, "<div id=\"page%d\">\n", id);

	for (block = page->first_block; block; block = block->next)
	{
		if (block->type == FZ_STEXT_BLOCK_IMAGE)
			fz_print_stext_image_as_xhtml(ctx, out, block);
		else if (block->type == FZ_STEXT_BLOCK_TEXT)
			fz_print_stext_block_as_xhtml(ctx, out, block);
	}

	fz_write_string(ctx, out, "</div>\n");
}

void
fz_print_stext_header_as_xhtml(fz_context *ctx, fz_output *out)
{
	fz_write_string(ctx, out, "<?xml version=\"1.0\"?>\n");
	fz_write_string(ctx, out, "<!DOCTYPE html");
	fz_write_string(ctx, out, " PUBLIC \"-//W3C//DTD XHTML 1.0 Strict//EN\"");
	fz_write_string(ctx, out, " \"http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd\">\n");
	fz_write_string(ctx, out, "<html xmlns=\"http://www.w3.org/1999/xhtml\">\n");
	fz_write_string(ctx, out, "<head>\n");
	fz_write_string(ctx, out, "<style>\n");
	fz_write_string(ctx, out, "p{white-space:pre-wrap}\n");
	fz_write_string(ctx, out, "</style>\n");
	fz_write_string(ctx, out, "</head>\n");
	fz_write_string(ctx, out, "<body>\n");
}

void
fz_print_stext_trailer_as_xhtml(fz_context *ctx, fz_output *out)
{
	fz_write_string(ctx, out, "</body>\n");
	fz_write_string(ctx, out, "</html>\n");
}

/* Detailed XML dump of the entire structured text data */

void
fz_print_stext_page_as_xml(fz_context *ctx, fz_output *out, fz_stext_page *page, int id)
{
	fz_stext_block *block;
	fz_stext_line *line;
	fz_stext_char *ch;

	fz_write_printf(ctx, out, "<page id=\"page%d\" width=\"%g\" height=\"%g\">\n", id,
		page->mediabox.x1 - page->mediabox.x0,
		page->mediabox.y1 - page->mediabox.y0);

	for (block = page->first_block; block; block = block->next)
	{
		switch (block->type)
		{
		case FZ_STEXT_BLOCK_TEXT:
			fz_write_printf(ctx, out, "<block bbox=\"%g %g %g %g\">\n",
					block->bbox.x0, block->bbox.y0, block->bbox.x1, block->bbox.y1);
			for (line = block->u.t.first_line; line; line = line->next)
			{
				fz_font *font = NULL;
				float size = 0;
				const char *name = NULL;

				fz_write_printf(ctx, out, "<line bbox=\"%g %g %g %g\" wmode=\"%d\" dir=\"%g %g\">\n",
						line->bbox.x0, line->bbox.y0, line->bbox.x1, line->bbox.y1,
						line->wmode,
						line->dir.x, line->dir.y);

				for (ch = line->first_char; ch; ch = ch->next)
				{
					if (ch->font != font || ch->size != size)
					{
						if (font)
							fz_write_string(ctx, out, "</font>\n");
						font = ch->font;
						size = ch->size;
						name = font_full_name(ctx, font);
						fz_write_printf(ctx, out, "<font name=\"%s\" size=\"%g\">\n", name, size);
					}
					fz_write_printf(ctx, out, "<char quad=\"%g %g %g %g %g %g %g %g\" x=\"%g\" y=\"%g\" color=\"#%06x\" c=\"",
							ch->quad.ul.x, ch->quad.ul.y,
							ch->quad.ur.x, ch->quad.ur.y,
							ch->quad.ll.x, ch->quad.ll.y,
							ch->quad.lr.x, ch->quad.lr.y,
							ch->origin.x, ch->origin.y,
							ch->color);
					switch (ch->c)
					{
					case '<': fz_write_string(ctx, out, "&lt;"); break;
					case '>': fz_write_string(ctx, out, "&gt;"); break;
					case '&': fz_write_string(ctx, out, "&amp;"); break;
					case '"': fz_write_string(ctx, out, "&quot;"); break;
					case '\'': fz_write_string(ctx, out, "&apos;"); break;
					case '\\': fz_write_string(ctx, out, "&bsol;"); break;
					default:
						   if (ch->c >= 32 && ch->c <= 127)
							   fz_write_printf(ctx, out, "%c", ch->c);
						   else
							   fz_write_printf(ctx, out, "&#x%x;", ch->c);
						   break;
					}
					fz_write_string(ctx, out, "\"/>\n");
				}

				if (font)
					fz_write_string(ctx, out, "</font>\n");

				fz_write_string(ctx, out, "</line>\n");
			}
			fz_write_string(ctx, out, "</block>\n");
			break;

		case FZ_STEXT_BLOCK_IMAGE:
			fz_write_printf(ctx, out, "<image bbox=\"%g %g %g %g\" />\n",
					block->bbox.x0, block->bbox.y0, block->bbox.x1, block->bbox.y1);
			break;
		}
	}
	fz_write_string(ctx, out, "</page>\n");
}

/* JSON dump */

void
fz_print_stext_page_as_json(fz_context *ctx, fz_output *out, fz_stext_page *page, float scale)
{
	fz_stext_block *block;
	fz_stext_line *line;
	fz_stext_char *ch;

	fz_write_printf(ctx, out, "{%q:[", "blocks");

	for (block = page->first_block; block; block = block->next)
	{
		if (block != page->first_block)
			fz_write_string(ctx, out, ",");
		switch (block->type)
		{
		case FZ_STEXT_BLOCK_TEXT:
			fz_write_printf(ctx, out, "{%q:%q,", "type", "text");
			fz_write_printf(ctx, out, "%q:{", "bbox");
			fz_write_printf(ctx, out, "%q:%d,", "x", (int)(block->bbox.x0 * scale));
			fz_write_printf(ctx, out, "%q:%d,", "y", (int)(block->bbox.y0 * scale));
			fz_write_printf(ctx, out, "%q:%d,", "w", (int)((block->bbox.x1 - block->bbox.x0) * scale));
			fz_write_printf(ctx, out, "%q:%d},", "h", (int)((block->bbox.y1 - block->bbox.y0) * scale));
			fz_write_printf(ctx, out, "%q:[", "lines");

			for (line = block->u.t.first_line; line; line = line->next)
			{
				if (line != block->u.t.first_line)
					fz_write_string(ctx, out, ",");
				fz_write_printf(ctx, out, "{%q:%d,", "wmode", line->wmode);
				fz_write_printf(ctx, out, "%q:{", "bbox");
				fz_write_printf(ctx, out, "%q:%d,", "x", (int)(line->bbox.x0 * scale));
				fz_write_printf(ctx, out, "%q:%d,", "y", (int)(line->bbox.y0 * scale));
				fz_write_printf(ctx, out, "%q:%d,", "w", (int)((line->bbox.x1 - line->bbox.x0) * scale));
				fz_write_printf(ctx, out, "%q:%d},", "h", (int)((line->bbox.y1 - line->bbox.y0) * scale));

				/* Since we force preserve-spans, the first char has the style for the entire line. */
				if (line->first_char)
				{
					fz_font *font = line->first_char->font;
					char *font_family = "sans-serif";
					char *font_weight = "normal";
					char *font_style = "normal";
					if (fz_font_is_monospaced(ctx, font)) font_family = "monospace";
					else if (fz_font_is_serif(ctx, font)) font_family = "serif";
					if (fz_font_is_bold(ctx, font)) font_weight = "bold";
					if (fz_font_is_italic(ctx, font)) font_style = "italic";
					fz_write_printf(ctx, out, "%q:{", "font");
					fz_write_printf(ctx, out, "%q:%q,", "name", fz_font_name(ctx, font));
					fz_write_printf(ctx, out, "%q:%q,", "family", font_family);
					fz_write_printf(ctx, out, "%q:%q,", "weight", font_weight);
					fz_write_printf(ctx, out, "%q:%q,", "style", font_style);
					fz_write_printf(ctx, out, "%q:%d},", "size", (int)(line->first_char->size * scale));
					fz_write_printf(ctx, out, "%q:%d,", "x", (int)(line->first_char->origin.x * scale));
					fz_write_printf(ctx, out, "%q:%d,", "y", (int)(line->first_char->origin.y * scale));
				}

				fz_write_printf(ctx, out, "%q:\"", "text");
				for (ch = line->first_char; ch; ch = ch->next)
				{
					if (ch->c == '"' || ch->c == '\\')
						fz_write_printf(ctx, out, "\\%c", ch->c);
					else if (ch->c < 32)
						fz_write_printf(ctx, out, "\\u%04x", ch->c);
					else
						fz_write_printf(ctx, out, "%C", ch->c);
				}
				fz_write_printf(ctx, out, "\"}");
			}
			fz_write_string(ctx, out, "]}");
			break;

		case FZ_STEXT_BLOCK_IMAGE:
			fz_write_printf(ctx, out, "{%q:%q,", "type", "image");
			fz_write_printf(ctx, out, "%q:{", "bbox");
			fz_write_printf(ctx, out, "%q:%d,", "x", (int)(block->bbox.x0 * scale));
			fz_write_printf(ctx, out, "%q:%d,", "y", (int)(block->bbox.y0 * scale));
			fz_write_printf(ctx, out, "%q:%d,", "w", (int)((block->bbox.x1 - block->bbox.x0) * scale));
			fz_write_printf(ctx, out, "%q:%d}}", "h", (int)((block->bbox.y1 - block->bbox.y0) * scale));
			break;
		}
	}
	fz_write_string(ctx, out, "]}");
}


#define RGB(r,g,b)          ((int)(((unsigned char)(r)|((unsigned short)((unsigned char)(g))<<8))|(((unsigned long)(unsigned char)(b))<<16)))
#define toInt(X) (int)(X+0.5)

void
fz_print_stext_page_as_xmlraw(fz_context *ctx, fz_output *out, fz_stext_page *page, int pagenum, int res, const char* version)
{
    fz_stext_block *block;
    fz_stext_line *line;
    fz_stext_char *ch;

    fz_write_printf(ctx, out, "<doc n=\"-1\" res=\"%d\" ver=\"%s\">\n", res, version);
    //CCS [
    fz_write_printf(ctx, out, "<pg id=\"%d\" bb=\"%d %d %d %d\">\n",
                    pagenum,
                    toInt(page->mediabox.x0), toInt(page->mediabox.y0),
                    toInt(page->mediabox.x1), toInt(page->mediabox.y1));
    //]CCS

    for (block = page->first_block; block; block = block->next) {
        switch (block->type) {
            case FZ_STEXT_BLOCK_TEXT:
                fz_write_printf(ctx, out, "<bl bb=\"%d %d %d %d\">\n",
                                toInt(block->bbox.x0), toInt(block->bbox.y0), toInt(block->bbox.x1), toInt(block->bbox.y1));
                for (line = block->u.t.first_line; line; line = line->next) {
                    fz_font *font = NULL;
                    float size = 0;
                    const char *name = NULL;

                    fz_write_printf(ctx, out, "<ln bb=\"%d %d %d %d\">\n",
                                    toInt(line->bbox.x0), toInt(line->bbox.y0), toInt(line->bbox.x1), toInt(line->bbox.y1));

                    for (ch = line->first_char; ch; ch = ch->next) {
                        if (ch->font != font || ch->size != size) {
                            if (font)
                                fz_write_string(ctx, out, "</sp>\n");
                            font = ch->font;
                            size = ch->size;
                            name = font_full_name(ctx, font);
                            char* s = strchr(name, '+');
                            s = s ? s + 1 : name;
                            fz_write_printf(ctx, out, "<sp ft=\"%s\" sz=\"%d\" b=\"%d\" i=\"%d\" rm=\"%d\" cl=\"0\"  bb=\"%d %d %d %d\"",
                                            s, toInt(size), fz_font_is_bold(ctx, font), fz_font_is_italic(ctx, font), ch->reender,
                                            toInt(line->bbox.x0), toInt(line->bbox.y0), toInt(line->bbox.x1), toInt(line->bbox.y1));
                            //fz_write_printf(ctx, out, "<font name=\"%s\" size=\"%g\">\n", name, size);

                            fz_stext_char *_ch = ch;
                            fz_write_printf(ctx, out, " tx=\"");
                            for (_ch = ch; _ch; _ch = _ch->next) {
                                if (ch->font != font || ch->size != size)
                                    break;
                                switch (_ch->c) {
                                    case '\0': fz_write_string(ctx, out, " "); break;
                                    case '<': fz_write_string(ctx, out, "&lt;"); break;
                                    case '>': fz_write_string(ctx, out, "&gt;"); break;
                                    case '&': fz_write_string(ctx, out, "&amp;"); break;
                                    case '"': fz_write_string(ctx, out, "&quot;"); break;
                                    case '\'': fz_write_string(ctx, out, "&apos;"); break;
									case '\\': fz_write_string(ctx, out, "&bsol;"); break;
									default:
                                        if (_ch->c >= 32 && _ch->c <= 127)
                                            fz_write_printf(ctx, out, "%c", _ch->c);
                                        else {
                                            char buf[10];
                                            int k;
                                            int n = fz_runetochar(buf, _ch->c);
                                            for (k = 0; k < n; k++)
                                                fz_write_printf(ctx, out, "%c", buf[k]);

                                        }
                                        break;
                                }
                            }
                            fz_write_string(ctx, out, "\">\n");
                        }
                        if ((ch->c == ' ') && (ch->next == NULL))
                            continue; // skip blank at end
                        int BndLeft = ((toInt(Min(ch->quad.ll.x,ch->quad.ul.x)))), BndTop = ((toInt(Min(ch->quad.ul.y, ch->quad.ur.y)))), BndRight = ((toInt(Max(ch->quad.lr.x, ch->quad.ur.x)))), BndBottom = ((toInt(Max(ch->quad.ll.y, ch->quad.lr.y))));
                        if (ch->c > ' ') {
                            if ((BndRight - BndLeft) == 0) BndLeft--;
                            if ((BndBottom - BndTop) == 0) BndTop--;
                        }
                        fz_write_printf(ctx, out, "<ch bb=\"%d %d %d %d\" uc=\"",
                                        BndLeft, BndTop, BndRight, BndBottom);
                        switch (ch->c) {
                            case '<': fz_write_string(ctx, out, "&lt;"); break;
                            case '>': fz_write_string(ctx, out, "&gt;"); break;
                            case '&': fz_write_string(ctx, out, "&amp;"); break;
                            case '"': fz_write_string(ctx, out, "&quot;"); break;
                            case '\'': fz_write_string(ctx, out, "&apos;"); break;
							case '\\': fz_write_string(ctx, out, "&bsol;"); break;
							default:
                                if (ch->c <= 32) {
                                } else
                                    if (ch->c >= 32 && ch->c <= 127)
                                        fz_write_printf(ctx, out, "%c", ch->c);
                                    else {
                                        //[CCS - add unicode char
                                        char buf[10];
                                        int k;
                                        int n = fz_runetochar(buf, ch->c);
                                        for (k = 0; k < n; k++)
                                            fz_write_printf(ctx, out, "%c", buf[k]);
                                        //]CCS

                                    }
                                    //fz_write_printf(ctx, out, "&#x%x;", ch->c);
                                    break;
                        }
                        fz_write_string(ctx, out, "\"/>\n");
                    }

                    if (font)
                        fz_write_string(ctx, out, "</sp>\n");

                    fz_write_string(ctx, out, "</ln>\n");
                }
                fz_write_string(ctx, out, "</bl>\n");
                break;

            case FZ_STEXT_BLOCK_IMAGE:
                fz_write_printf(ctx, out, "<im bb=\"%d %d %d %d\" />\n",
                                toInt(block->bbox.x0), toInt(block->bbox.y0), toInt(block->bbox.x1), toInt(block->bbox.y1));
                break;
        }
    }
    fz_write_string(ctx, out, "</pg>\n");
    fz_write_string(ctx, out, "</doc>\n");
}

static inline int
is_unicode_wspace(int c)
{
    return (c == 9 || /* TAB */
            c == 0x0a || /* HT */
            c == 0x0b || /* LF */
            c == 0x0c || /* VT */
            c == 0x0d || /* FF */
            c == 0x20 || /* CR */
            c == 0x85 || /* NEL */
            c == 0xA0 || /* No break space */
            c == 0x1680 || /* Ogham space mark */
            c == 0x180E || /* Mongolian Vowel Separator */
            c == 0x2000 || /* En quad */
            c == 0x2001 || /* Em quad */
            c == 0x2002 || /* En space */
            c == 0x2003 || /* Em space */
            c == 0x2004 || /* Three-per-Em space */
            c == 0x2005 || /* Four-per-Em space */
            c == 0x2006 || /* Five-per-Em space */
            c == 0x2007 || /* Figure space */
            c == 0x2008 || /* Punctuation space */
            c == 0x2009 || /* Thin space */
            c == 0x200A || /* Hair space */
            c == 0x2028 || /* Line separator */
            c == 0x2029 || /* Paragraph separator */
            c == 0x202F || /* Narrow no-break space */
            c == 0x205F || /* Medium mathematical space */
            c == 0x3000); /* Ideographic space */
}

static void
ccs_printWord(fz_context *ctx, fz_output *out, const char* word, fz_rect wbox, double factor, int wasblank)
{
    if (!wasblank)
        fz_write_string(ctx, out, "<BLANK/>");
    fz_write_printf(ctx, out, "<WORD Rect=\"%d,%d,%d,%d\">%s</WORD>\n",
                    toInt(wbox.x0*factor), toInt(wbox.y0*factor), toInt(wbox.x1*factor), toInt(wbox.y1*factor),
                    word);
}

#define MAXINTFONT 4000

int ccs_findFont(const char* Name, float size, int addme, char** NameArray, long* sizearray)
{
    int cPos = 0;
    long cmppos = toInt((size * 3515) / 1000);
    while (*NameArray) {
        if ((stricmp(Name, *NameArray) == 0) && (*sizearray == cmppos))
            return cPos;
        NameArray++;
        sizearray++;
        cPos++;
    }
    if (addme && cPos < MAXINTFONT) {
        if (cPos > MAXINTFONT / 2) {
            _ASSERT(1 == 0);
        }
        *NameArray = Name;
        *sizearray = cmppos;
        return cPos;
    }
    return -1;
}

void
fz_print_stext_page_as_xmltext(fz_context *ctx, fz_output *out, fz_stext_page *page, int pagenum, int lang, const char* imgname, tAddRectlist AddRectList, void* addrectdata, const char* version)
{
    fz_stext_block *block;
    fz_stext_line *line;
    fz_stext_char *ch;

    double factor = 254 / 72.0;
    char * szName[MAXINTFONT];
    long  szSize[MAXINTFONT];
    memset(&szName, 0, sizeof(szName));
    memset(&szSize, 0, sizeof(szSize));

    fz_write_printf(ctx, out, "<Document id=\"-1\" CREATOR=\"PDF\" ENGINE=\"muPDF\" Language=\"%d\" CVersion=\"%s\">\n", lang, version);

    fz_write_printf(ctx, out, "<PAGE id=\"%d\" Rect=\"%d,%d,%d,%d\" Width=\"%d\" Height=\"%d\" Image=\"%s\"> \n",
                    pagenum, toInt(page->mediabox.x0*factor), toInt(page->mediabox.y0*factor), toInt(page->mediabox.x1*factor), toInt(page->mediabox.y1*factor),
                    toInt((page->mediabox.x1 - page->mediabox.x0)*factor),
                    toInt((page->mediabox.y1 - page->mediabox.y0)*factor), imgname);

    int curFont = 0;
    for (block = page->first_block; block; block = block->next) {
        switch (block->type) {
            case FZ_STEXT_BLOCK_TEXT:
                {
                    fz_write_printf(ctx, out, "<REGION Rect=\"%d,%d,%d,%d\">\n",
                                    toInt(block->bbox.x0*factor), toInt(block->bbox.y0*factor), toInt(block->bbox.x1*factor), toInt(block->bbox.y1*factor));
                    fz_write_printf(ctx, out, "<PARAGRAPH Rect=\"%d,%d,%d,%d\">\n",
                                    toInt(block->bbox.x0*factor), toInt(block->bbox.y0*factor), toInt(block->bbox.x1*factor), toInt(block->bbox.y1*factor));
                    for (line = block->u.t.first_line; line; line = line->next) {
                        fz_font *font = NULL;
                        float size = 0;
                        const char *name = NULL;
#define MAXWORD 20000
#define charpos &(szMaxWord[nMaxWord])
                        char szMaxWord[MAXWORD + 10];
                        int nMaxWord = 0;
                        szMaxWord[0] = 0;
                        int wasblank = 1; //TRUE
                        fz_rect wbox;

                        fz_write_printf(ctx, out, "<LINE Rect=\"%d,%d,%d,%d\" FontID=\"%d\">\n",
                                        toInt(line->bbox.x0*factor), toInt(line->bbox.y0*factor), toInt(line->bbox.x1*factor), toInt(line->bbox.y1*factor), curFont);

                        for (ch = line->first_char; ch; ch = ch->next) {
                            if (ch->font != font || ch->size != size) {
                                //if (font)
                                //    fz_write_string(ctx, out, "</font>\n");
                                font = ch->font;
                                size = ch->size;
                                name = font_full_name(ctx, font);
                                int nxFont = ccs_findFont(name, size, 1, &szName, &szSize);
                                if (nxFont != curFont) {
                                    if (nMaxWord) {
                                        szMaxWord[nMaxWord] = 0;
                                        ccs_printWord(ctx, out, szMaxWord, wbox, factor, wasblank);
                                        nMaxWord = 0;
                                        wasblank = 1;
                                    }
                                    fz_write_printf(ctx, out, "<FNT ID=\"%d\"/>", nxFont);
                                }
                                curFont = nxFont;
                            }
                            if (!is_unicode_wspace(ch->c)) {
                                if (nMaxWord)
                                    wbox = fz_union_rect(wbox, fz_rect_from_quad(ch->quad));
                                else
                                    wbox = fz_rect_from_quad(ch->quad);
                            }
                            switch (ch->c) {
                                case '<': strcpy(charpos, "&lt;"); nMaxWord += strlen(charpos); break;
                                case '>': strcpy(charpos, "&gt;");  nMaxWord += strlen(charpos); break;
                                case '&': strcpy(charpos, "&amp;");  nMaxWord += strlen(charpos); break;
                                    //case '"': strcpy(charpos, "&quot;"); break;
                                    //case '\'': strcpy(charpos, "&apos;"); break;
                                default:
                                    if (is_unicode_wspace(ch->c)) {
                                        if (nMaxWord) {
                                            szMaxWord[nMaxWord] = 0;
                                            ccs_printWord(ctx, out, szMaxWord, wbox, factor, wasblank);
                                            nMaxWord = 0;
                                        }
                                        wasblank = 0;
                                    } else {
                                        if (ch->c > 32 && ch->c <= 127)
                                            szMaxWord[nMaxWord++] = ch->c;
                                        else
                                            nMaxWord += fz_runetochar(charpos, ch->c);
                                    }
                                    break;
                            }
                        }
                        if (nMaxWord) {
                            szMaxWord[nMaxWord] = 0;
                            ccs_printWord(ctx, out, szMaxWord, wbox, factor, wasblank);
                            nMaxWord = 0;
                        }
                        wasblank = 1;

                        fz_write_string(ctx, out, "</LINE>\n");
                    }
                    fz_write_string(ctx, out, "</PARAGRAPH>\n");
                    fz_write_string(ctx, out, "</REGION>\n");
                }
                break;

            case FZ_STEXT_BLOCK_IMAGE:
                //[CCS - add image export
                //fz_image_block *block = page->blocks[block_n].u.image;
                //fz_write_printf(out, "<image bbox=\"%g %g %g %g\" />\n", block->bbox.x0, block->bbox.y0, block->bbox.x1, block->bbox.y1);
                //]CCS
                if (AddRectList != NULL) {
                    //fz_image_block *block = block->u.i.image;
                    AddRectList(addrectdata, 1, toInt(block->bbox.x0*factor), toInt(block->bbox.y0*factor), toInt(block->bbox.x1*factor), toInt(block->bbox.y1*factor));
                }

                //fz_write_printf(ctx, out, "<image bbox=\"%g %g %g %g\" />\n",
                //                block->bbox.x0, block->bbox.y0, block->bbox.x1, block->bbox.y1);
                break;
        }
    }
    fz_write_string(ctx, out, "</PAGE>\n");
    {
        int fntid = 0;
        while (szName[fntid]) {
            fz_write_printf(ctx, out, "<FONT ID=\"%d\" Name=\"%s\" BOLD=\"0\" ITALIC=\"0\" Size=\"%d\"/>\n",
                            fntid, szName[fntid], szSize[fntid]);
            fntid++;
        }
    }

    fz_write_string(ctx, out, "</Document>\n");
}

/* Plain text */

void
fz_print_stext_page_as_text(fz_context *ctx, fz_output *out, fz_stext_page *page)
{
	fz_stext_block *block;
	fz_stext_line *line;
	fz_stext_char *ch;
	char utf[10];
	int i, n;

	for (block = page->first_block; block; block = block->next)
	{
		if (block->type == FZ_STEXT_BLOCK_TEXT)
		{
			for (line = block->u.t.first_line; line; line = line->next)
			{
				for (ch = line->first_char; ch; ch = ch->next)
				{
					n = fz_runetochar(utf, ch->c);
					for (i = 0; i < n; i++)
						fz_write_byte(ctx, out, utf[i]);
				}
				fz_write_string(ctx, out, "\n");
			}
			fz_write_string(ctx, out, "\n");
		}
	}
}

/* Text output writer */

enum {
	FZ_FORMAT_TEXT,
	FZ_FORMAT_HTML,
	FZ_FORMAT_XHTML,
	FZ_FORMAT_STEXT_XML,
	FZ_FORMAT_STEXT_JSON,
    FZ_FORMAT_XMLCCS,
    FZ_FORMAT_RAWXMLCCS,
};

typedef struct
{
	fz_document_writer super;
	int format;
	int number;
	fz_stext_options opts;
	fz_stext_page *page;
	fz_output *out;
} fz_text_writer;

static fz_device *
text_begin_page(fz_context *ctx, fz_document_writer *wri_, fz_rect mediabox)
{
	fz_text_writer *wri = (fz_text_writer*)wri_;

	if (wri->page)
	{
		fz_drop_stext_page(ctx, wri->page);
		wri->page = NULL;
	}

	wri->number++;

	wri->page = fz_new_stext_page(ctx, mediabox);
	return fz_new_stext_device(ctx, wri->page, &wri->opts);
}

static void
text_end_page(fz_context *ctx, fz_document_writer *wri_, fz_device *dev)
{
	fz_text_writer *wri = (fz_text_writer*)wri_;

	fz_try(ctx)
	{
		fz_close_device(ctx, dev);
		switch (wri->format)
		{
		default:
		case FZ_FORMAT_TEXT:
			fz_print_stext_page_as_text(ctx, wri->out, wri->page);
			break;
        case FZ_FORMAT_XMLCCS:
            fz_print_stext_page_as_text(ctx, wri->out, wri->page);
            break;
        case FZ_FORMAT_HTML:
			fz_print_stext_page_as_html(ctx, wri->out, wri->page, wri->number);
			break;
		case FZ_FORMAT_XHTML:
			fz_print_stext_page_as_xhtml(ctx, wri->out, wri->page, wri->number);
			break;
		case FZ_FORMAT_STEXT_XML:
			fz_print_stext_page_as_xml(ctx, wri->out, wri->page, wri->number);
			break;
		case FZ_FORMAT_STEXT_JSON:
			if (wri->number > 1)
				fz_write_string(ctx, wri->out, ",");
			fz_print_stext_page_as_json(ctx, wri->out, wri->page, 1);
			break;
		}
	}
	fz_always(ctx)
	{
		fz_drop_device(ctx, dev);
		fz_drop_stext_page(ctx, wri->page);
		wri->page = NULL;
	}
	fz_catch(ctx)
		fz_rethrow(ctx);
}

static void
text_close_writer(fz_context *ctx, fz_document_writer *wri_)
{
	fz_text_writer *wri = (fz_text_writer*)wri_;
	switch (wri->format)
	{
	case FZ_FORMAT_HTML:
		fz_print_stext_trailer_as_html(ctx, wri->out);
		break;
	case FZ_FORMAT_XHTML:
		fz_print_stext_trailer_as_xhtml(ctx, wri->out);
		break;
	case FZ_FORMAT_STEXT_XML:
		fz_write_string(ctx, wri->out, "</document>\n");
		break;
	case FZ_FORMAT_STEXT_JSON:
		fz_write_string(ctx, wri->out, "]\n");
		break;
	}
	fz_close_output(ctx, wri->out);
}

static void
text_drop_writer(fz_context *ctx, fz_document_writer *wri_)
{
	fz_text_writer *wri = (fz_text_writer*)wri_;
	fz_drop_stext_page(ctx, wri->page);
	fz_drop_output(ctx, wri->out);
}

fz_document_writer *
fz_new_text_writer_with_output(fz_context *ctx, const char *format, fz_output *out, const char *options)
{
	fz_text_writer *wri;

	wri = fz_new_derived_document_writer(ctx, fz_text_writer, text_begin_page, text_end_page, text_close_writer, text_drop_writer);
	fz_try(ctx)
	{
		fz_parse_stext_options(ctx, &wri->opts, options);

		wri->format = FZ_FORMAT_TEXT;
		if (!strcmp(format, "text"))
			wri->format = FZ_FORMAT_TEXT;
		else if (!strcmp(format, "html"))
			wri->format = FZ_FORMAT_HTML;
		else if (!strcmp(format, "xhtml"))
			wri->format = FZ_FORMAT_XHTML;
		else if (!strcmp(format, "stext"))
			wri->format = FZ_FORMAT_STEXT_XML;
		else if (!strcmp(format, "stext.xml"))
			wri->format = FZ_FORMAT_STEXT_XML;
		else if (!strcmp(format, "stext.json"))
		{
			wri->format = FZ_FORMAT_STEXT_JSON;
			wri->opts.flags |= FZ_STEXT_PRESERVE_SPANS;
		}

		wri->out = out;

		switch (wri->format)
		{
		case FZ_FORMAT_HTML:
			fz_print_stext_header_as_html(ctx, wri->out);
			break;
		case FZ_FORMAT_XHTML:
			fz_print_stext_header_as_xhtml(ctx, wri->out);
			break;
		case FZ_FORMAT_STEXT_XML:
			fz_write_string(ctx, wri->out, "<?xml version=\"1.0\"?>\n");
			fz_write_string(ctx, wri->out, "<document>\n");
			break;
		case FZ_FORMAT_STEXT_JSON:
			fz_write_string(ctx, wri->out, "[");
			break;
		}
	}
	fz_catch(ctx)
	{
		fz_free(ctx, wri);
		fz_rethrow(ctx);
	}

	return (fz_document_writer*)wri;
}

fz_document_writer *
fz_new_text_writer(fz_context *ctx, const char *format, const char *path, const char *options)
{
	fz_output *out = fz_new_output_with_path(ctx, path ? path : "out.txt", 0);
	fz_document_writer *wri = NULL;
	fz_try(ctx)
		wri = fz_new_text_writer_with_output(ctx, format, out, options);
	fz_catch(ctx)
	{
		fz_drop_output(ctx, out);
		fz_rethrow(ctx);
	}
	return wri;
}
