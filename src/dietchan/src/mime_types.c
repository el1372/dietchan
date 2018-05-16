#include "mime_types.h"

#include <libowfat/case.h>

const struct mime_type mime_types[] = {
	{"image/jpeg",      {".jpg", ".jpeg", ".jpe", ".jfif", 0}},
	{"image/jpg",       {".jpg", ".jpeg", ".jpe", ".jfif", 0}},
	{"image/png",       {".png", 0}},
	{"image/gif",       {".gif", 0}},
	{"video/webm",      {".webm", 0}},
	{"application/pdf", {".pdf", 0}},
	{"text/pain",       {".txt", 0}},
	{"text/css",        {".css", 0}},
	{"text/javascript", {".js", 0}},
	{"text/html",       {".html", ".html", 0}},
	{"text/xml",        {".xml", 0}},
	{"image/svg+xml",   {".svg", 0}},
	{0}
};

const char *allowed_mime[] = {
	"image/jpeg", "image/jpg", "image/png", "image_gif", "application/pdf", 0
};

const char* get_extension_for_mime_type(const char *mime_type)
{
	for (int i=0; mime_types[i].identifier; ++i) {
		if (case_equals(mime_types[i].identifier, mime_type))
			return mime_types[i].extensions[0];
	}
	return "";
}

const char* get_mime_type_for_extension(const char *ext)
{
	if (ext) {
		for (int i=0; mime_types[i].identifier; ++i) {
			for (int j=0; mime_types[i].extensions[j]; ++j) {
				if (case_equals(mime_types[i].extensions[j], ext))
					return mime_types[i].identifier;
			}
		}
	}
	return "application/octet-stream";
}

int is_valid_extension(const char *mime_type, const char *ext)
{
	if (!ext)
		return 0;
	for (int i=0; mime_types[i].identifier; ++i) {
		if (case_equals(mime_types[i].identifier, mime_type)) {
			for (int j=0; mime_types[i].extensions[j]; ++j) {
				if (case_equals(mime_types[i].extensions[j], ext))
					return 1;
			}
		}
	}
	return 0;
}

int is_mime_allowed(const char *mime_type)
{
	for (int i=0; allowed_mime[i]; ++i) {
		if (case_equals(allowed_mime[i], mime_type))
			return 1;
	}
	return 0;
}
