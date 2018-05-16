#ifndef MIME_TYPES_H
#define MIME_TYPES_H

struct mime_type {
	char *identifier;
	char *extensions[8];
};

extern const struct mime_type mime_types[];
extern const char *allowed_mime[];

const char* get_extension_for_mime_type(const char *mime_type);
const char* get_mime_type_for_extension(const char *ext);
int is_valid_extension(const char *mime_type, const char *ext);
int is_mime_allowed(const char *mime_type);

#endif // MIME_TYPES_H
