#include "bbcode.h"

#include <ctype.h>
#include <string.h>

#include "http.h"

#include "tpl.h"
#include "util.h"

const char *TAGS[] = {"b", "i", "u", "s", "spoiler", "code", "q", 0};

static size_t scan_tag(const char *s, const char **tag, int *open)
{
	const char *t = s;
	*tag = 0;
	if (*t != '[') return 0;
	++t;
	t += scan_whiteskip(t);

	*open = (*t != '/');
	if (!(*open))
		++t;

	t += scan_whiteskip(t);

	for (int i=0; *tag=TAGS[i]; ++i) {
		if (case_starts(t, *tag) && (isspace(t[strlen(*tag)]) || t[strlen(*tag)] == ']'))
			break;
	}

	if (!(*tag)) return 0;

	t += strlen(*tag);
	t += scan_whiteskip(t);

	if (*t != ']') return 0;

	++t;

	return (t-s);
}

static void write_bbcode_tag(http_context *http, const char *tag, int open)
{
	if (tag == "spoiler")
		PRINT(open?S("<span class='spoiler'>"):S("</span>"));
	else if (tag == "code")
		PRINT(open?S("<pre>"):S("</pre>"));
	else if (tag == "q")
		PRINT(open?S("<span class='quote inline'><span class='qdeco'>&gt;</span>"):S("</span>"));
	else if (tag == ">")
		PRINT(open?S("<span class='quote'>"):S("</span>"));
	else
		PRINT(open?S("<"):S("</"), S(tag), S(">"));
}

static size_t scan_reference(const char *s, uint64 *post_id)
{
	size_t i=0;
	if (s[0] == '>' && s[1] == '>' && (i=scan_uint64(&s[2], post_id)))
		return 2+i;
	else
		return 0;
}

static size_t scan_quote(const char *s, size_t *depth)
{
	size_t i = 0;
	*depth = 0;
	uint64 dummy=0;
	while (isspace(s[i]) && s[i]!='\n' || s[i]=='>') {
		if (s[i] == '>') {
			if (scan_reference(&s[i], &dummy))
				break;
			else
				++(*depth);
		}
		++i;
	}
	if (*depth == 0)
		return 0;
	else
		return i;
}

void write_bbcode(http_context *http, const char *s, struct thread *current_thread)
{
	const char *tag = 0;
	int open = 0;
	ssize_t i = 0;

	const char **tag_stack_start = alloca(strlen(s)*sizeof(const char*));
	const char **tag_stack       = tag_stack_start;

	#define WRITE_ESCAPED(length) \
		do { \
			if (length > 0) \
				_print_esc_html((context*)http, ss, length); \
		} while (0)

	#define OPEN_TAG(tag) \
		do { \
			*(tag_stack++) = tag; \
			write_bbcode_tag(http, tag, 1); \
		} while (0)

	#define CLOSE_TAG(tag, length) \
		do { \
			const char **t = tag_stack - 1; \
			/* Check if tag was open to begin with */ \
			int tag_open = 0; \
			while (t >= tag_stack_start) { \
				if (*t == tag) { \
					tag_open=1; \
					break; \
				} \
				--t; \
			} \
			if (!tag_open) break; \
			/* Close overlapping tags */ \
			t = tag_stack - 1; \
			while (t >= tag_stack_start && *t != tag) { \
				write_bbcode_tag(http, *t, 0); \
				--t; \
			} \
			if (t >= tag_stack_start && *t == tag  && t >= tag_stack_start) { \
				/* Close tag */ \
				write_bbcode_tag(http, tag, 0); \
				/* Remove tag from stack */ \
				memmove(t, t+1, (size_t)tag_stack - (size_t)t - 1); \
				--tag_stack; \
			} else { \
				WRITE_ESCAPED(length); \
				++t; \
			} \
			/* Reopen overlapping tags */ \
			while (t < tag_stack) { \
				write_bbcode_tag(http, *t, 1); \
				++t; \
			} \
		} while(0)

	#define CLOSE_ALL() \
		do { \
			--tag_stack; \
			for (; tag_stack >= tag_stack_start; --tag_stack) \
				write_bbcode_tag(http, *tag_stack, 0); \
			tag_stack = tag_stack_start; \
		} while(0)

	int code = 0;
	// Quote depth of previous line
	size_t prev_quote_depth = 0;
	// Quote depth of current line
	size_t quote_depth = 0;
	// Flag indicating we just started a new line
	int newline = 1;

	for (const char *ss = s; *ss != '\0'; ) {
		if (newline) {
			prev_quote_depth = quote_depth;
			quote_depth = 0;
			i = scan_quote(ss, &quote_depth);

			// If new depth is lower, close tags
			for (size_t j=quote_depth; j<prev_quote_depth; ++j)
				CLOSE_TAG(">",0);
			// If new depth is higher, open tags
			for (size_t j=prev_quote_depth; j<quote_depth; ++j)
				OPEN_TAG(">");
			// Write > decoration
			for (size_t j=0; j<quote_depth; ++j)
				PRINT(S("<span class='qdeco'>&gt;</span>"));
			ss += i;

			newline = 0;
		}
		switch (*ss) {
			case '\r': ++ss;
			           break;
			case '\n': if (code) {
			               WRITE_ESCAPED(1);
			               ++ss;
			               break;
			           }
			           //CLOSE_TAG(">", 0);
			           newline = 1;
			           PRINT(S("<br>"));
			           ++ss;
			           break;
			case '>':  if (code) {
			               WRITE_ESCAPED(1);
			               ++ss;
			               break;
			           }
			           uint64 post_id=0;
			           if (i=scan_reference(ss,&post_id)) {
			               // Reference to a post
			               struct post *post=find_post_by_id(post_id);
			               if (post) {
			                   PRINT(S("<a href='"));
			                   print_post_url(http, post, post_thread(post) != current_thread);
			                   PRINT(S("'>"));
			                   WRITE_ESCAPED(i);
			                   PRINT(S("</a>"));
			               } else {
			                   WRITE_ESCAPED(i);
			               }
			               ss += i;
			           } else {
			               WRITE_ESCAPED(1);
			               ++ss;
			           }
			           break;
			case '[':  if (i=scan_tag(ss, &tag, &open)) {
			               if (code && !(tag == "code" && !open)) {
			                   WRITE_ESCAPED(1);
			                   ++ss;
			                   break;
			               }
			               if (tag == "code") {
			                   if (open && !code)
			                       CLOSE_ALL();
			                   if (!open & code)
			                       newline = 1; // Since [code] is a block element, treat like newline
			                   code = open;
			               }
			               if (open)
			                   OPEN_TAG(tag);
			               else
			                   CLOSE_TAG(tag, i);
			               ss += i;
			           } else {
			               WRITE_ESCAPED(1);
			               ++ss;
			           }
			           break;
			default:   i=0;
			           while (ss[i] != '\0' && ss[i] != '\r' && ss[i] != '\n' && ss[i] != '[' && ss[i] != '>') ++i;
			           WRITE_ESCAPED(i);
			           ss += i;
		}
	}

	CLOSE_ALL();

	#undef WRITE_ESCAPED
	#undef OPEN_TAG
	#undef CLOSE_TAG
	#undef CLOSE_ALL
}

void strip_bbcode(char *buf)
{
	char *s=buf;
	char *t=buf;
	while (1) {
		int open = 0;
		const char *tag = 0;
		size_t i=0;
		switch (*s) {
		case '\0':
			*t++ = '\0';
			return;
		case ' ':
		case '\t':
			*t++ = ' ';
			while (*s == ' ' || *s == '\t')
				++s;
			break;
		case '\n':
			*t++ = '\n';
			while (isspace(*t))
				++s;
			break;
		case '[':
			if (i=scan_tag(s, &tag, &open))
				s += i;
			else
				*t++ = *s++;
			break;
		default:
			while (*s != '\0' && *s != '[')
				*t++ = *s++;
		}
	}
}
