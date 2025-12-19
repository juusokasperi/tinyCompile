#ifndef VALIDATION_H
# define VALIDATION_H

# include <stdbool.h>
# include <stddef.h>
# include <sys/stat.h>
# include <limits.h>
# include "error_handler.h"

typedef struct {
	bool	exists;
	bool	readable;
	bool	is_regular_file;
	size_t	size;
	char	canonical_path[PATH_MAX];
} FileValidation;

bool	validate_source_file(const char *filepath, FileValidation *out,
				ErrorContext *errors);
bool	validate_file_extension(const char *filepath, const char *expected_ext);
bool	validate_file_size(size_t size, size_t max_size, const char *filepath,
				ErrorContext *errors);
bool	validate_identifier(const char *name, size_t len, ErrorContext *errors,
				const char *filename, int line);
bool	validate_integer_literal(const char *text, size_t len, int64_t *out_value,
				ErrorContext *errors, const char *filename, int line);

#endif // VALIDATION_H
