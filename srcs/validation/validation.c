#include "validation.h"
#include "defines.h"
#include "error_handler.h"
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>

#define MAX_SOURCE_FILE_SIZE (10 * 1024 * 1024)

bool	validate_source_file(const char *filepath, FileValidation *out, ErrorContext *errors)
{
	memset(out, 0, sizeof(*out));
	bool	valid = true;
	if (!filepath || filepath[0] == '\0')
	{
		error_fatal(errors, NULL, 0, 0, "Empty filepath provided");
		return (false);
	}
	if (strlen(filepath) >= PATH_MAX)
	{
		error_fatal(errors, NULL, 0, 0, "Filepath too long (max %d)", PATH_MAX);
		return (false);
	}
	if (!realpath(filepath, out->canonical_path))
	{
		error_fatal(errors, filepath, 0, 0,
				"Cannot resolve filepath: %s", strerror(errno));
		return (false);
	}
	struct stat st;
	if (stat(out->canonical_path, &st) != 0)
	{
		error_fatal(errors, filepath, 0, 0,
				"Cannot access file: %s", strerror(errno));
		return (false);
	}
	out->exists = true;
	out->size = st.st_size;
	if (!S_ISREG(st.st_mode))
	{
		error_fatal(errors, filepath, 0, 0, "Not a regular file");
		return (false);
	}
	out->is_regular_file = true;
	if (access(out->canonical_path, R_OK) != 0)
	{
		error_fatal(errors, filepath, 0, 0,
				"File not readable: %s", strerror(errno));
		return (false);
	}
	out->readable = true;
	if (!validate_file_size(out->size, MAX_SOURCE_FILE_SIZE, filepath, errors))
		valid = false;
	if (!validate_file_extension(filepath, ".c"))
	{
		error_fatal(errors, filepath, 0, 0, "Expected .c file extension");
		valid = false;
	}
	return (valid);
}

bool	validate_file_extension(const char *filepath, const char *expected_ext)
{
	size_t len = strlen(filepath);
	size_t ext_len = strlen(expected_ext);

	if (len < ext_len)
		return (false);
	return (strcmp(filepath + len - ext_len, expected_ext) == 0);
}

bool	validate_file_size(size_t size, size_t max_size, const char *filepath,
			ErrorContext *errors)
{
	if (size == 0)
	{
		error_add(errors, ERROR_SYSTEM, ERROR_LEVEL_WARNING,
				filepath, 0, 0, "File is empty");
		return (true);
	}
	if (size > max_size)
	{
		error_fatal(errors, filepath, 0, 0,
				"File too large: %zu bytes (max %zu)",
				size, max_size);
		return (false);
	}
	return (true);
}

bool	validate_identifier(const char *name, size_t len, ErrorContext *errors,
			const char *filename, int line)
{
	if (len == 0)
	{
		error_semantic(errors, filename, line, 0, "Empty identifier");
		return (false);
	}
	if (!isalpha(name[0]) && name[0] != '_')
	{
		error_semantic(errors, filename, line, 0,
				"Identifiers must start with letter or underscore");
		return (false);
	}

	for (size_t i = 0; i < len; ++i)
	{
		if (!isalnum(name[i]) && name[i] != '_')
		{
			error_semantic(errors, filename, line, 0,
					"Invalid character '%c' in identifier", name[i]);
			return (false);
		}
	}

	const char *reserved[] = {
		"auto", "break", "case", "char", "const", "continue", "default",
		"do", "double", "else", "enum", "extern", "float", "for", "goto",
		"if", "inline", "int", "long", "register", "restrict", "return",
		"short", "signed", "sizeof", "static", "struct", "switch", "typedef",
		"union", "unsigned", "void", "volatile", "while", "_Bool", "_Complex",
		"_Imaginary"
	};
	for (size_t i = 0; i < sizeof(reserved) / sizeof(reserved[0]); ++i)
	{
		if (strncmp(name, reserved[i], len) == 0 && reserved[i][len] == '\0')
		{
			error_semantic(errors, filename, line, 0,
					"'%.*s' is a reserved keyword", (int)len, name);
			return (false);
		}
	}
	if (len > 255)
	{
		error_semantic(errors, filename, line, 0,
				"Identifier too long (max 255 characters)");
		return (false);
	}
	return (true);
}

bool	validate_integer_literal(const char *text, size_t len, int64_t *out_value, ErrorContext *errors, const char *filename, int line)
{
	if (len == 0)
	{
		error_lexer(errors, filename, line, 0, "Empty number literal");
		return (false);
	}
	if (len > 1 && text[0] == '0' && isdigit(text[1]))
	{
		error_lexer(errors, filename, line, 0,
				"Octal literals not supported (leading zero)");
		return (false);
	}
	errno = 0;
	char		*endptr;
	long long	value = strtoll(text, &endptr, 10);
	if (errno == ERANGE)
	{
		error_lexer(errors, filename, line, 0,
				"Integer literal out of range: %.*s", (int)len, text);
		return (false);
	}
	if (endptr != text + len)
	{
		error_lexer(errors, filename, line, 0,
				"Invalid integer literal: %.*s", (int)len, text);
		return (false);
	}
	*out_value = (int64_t)value;
	return (true);
}
