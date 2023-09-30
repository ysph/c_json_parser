// Copyright Mikhail ysph Subbotin
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include "json.h"

#define is_digit(c) ((unsigned)((c) - '0') < 10)
#define min(a, b) (((a) < (b)) ? (a) : (b))

size_t g_pos = 0;
size_t file_size = 0;

static inline int is_EOF(void)
{
	return (g_pos >= file_size + 1);
}
static inline double pow_n(int N, int n)
{
	N <<= n;
	while (n--)
		N += N << 2;
	return N;
}
int parse_whitespace(const char *fstr, int *restrict position, int *restrict line)
{
	int8_t ch;
	while (1)
	{
		ch = fstr[g_pos++];
		*position += 1;

		switch (ch)
		{
		case '\n':
			*line += 1;
			*position = 0;
			__attribute__((fallthrough));
		case ' ':
		case '\t':
		case '\r':
			continue;
		}
		return ch;
	}
	return FATAL_ERROR;
}
int parse_number(const char *fstr, int *restrict position, int *restrict line, item_t *parent)
{
	int8_t ch;
	int negative = 1; // 1 or -1
	int negative_exp = 0;
	int exponent = 0;
	int frac_digits = -1;
	double result = 0;
	double fraction = 0;

	parent->type = json_number;

	ch = fstr[g_pos - 1];
	if (is_EOF())
		return FATAL_ERROR;
	if (ch == '-')
	{
		negative = -1;
		ch = fstr[g_pos++];
		*position += 1;
	}
	if (is_digit(ch) && ch != '0')
	{
		while (is_digit(ch))
		{
			result = (result * 10) + (ch - '0');
			ch = fstr[g_pos++];
			*position += 1;
		}
	}
	else if (ch == '0')
	{
		ch = fstr[g_pos++];
		*position += 1;
		if (is_EOF())
			return FATAL_ERROR;
	}
	else if (is_EOF())
		return FATAL_ERROR;
	else
	{
		fprintf(stderr, "SyntaxError: Unexpected non-digit token '%c' at line (%d), position (%d)\n", ch, *line, *position);
		return FATAL_ERROR;
	}
	if (ch == 'e' || ch == 'E')
		goto PARSE_EXPONENT;
	if (ch != '.')
		goto PARSE_STOP;

	// parse_fraction
	ch = fstr[g_pos++];
	*position += 1;
	if (!is_digit(ch))
	{
		fprintf(stderr, "SyntaxError: Unterminated fractional number at line (%d), position (%d)\n", *line, *position);
		return FATAL_ERROR;
	}
	while (is_digit(ch))
	{
		fraction = (fraction * 10) + (ch - '0');
		frac_digits += 1;

		ch = fstr[g_pos++];
		*position += 1;
	};
	if (ch != 'e' && ch != 'E')
		goto PARSE_STOP;
PARSE_EXPONENT:
	ch = fstr[g_pos++];
	*position += 1;
	if (is_EOF())
		return FATAL_ERROR;
	if (ch == '-' || ch == '+')
	{
		negative_exp = ch - '+';
		ch = fstr[g_pos++];
		*position += 1;
	}
	if (!is_digit(ch))
	{
		fprintf(stderr, "SyntaxError: Exponent part is missing a number at line (%d), position (%d)\n", *line, *position);
		return FATAL_ERROR;
	}
	while (is_digit(ch))
	{
		exponent = (exponent * 10) + (ch - '0');
		ch = fstr[g_pos++];
		*position += 1;
	}

PARSE_STOP:
	frac_digits = min(frac_digits, FLOAT_DIGITS);
	if (frac_digits > -1)
		result += (fraction / pow_n(10, frac_digits));
	exponent = min(exponent, FLOAT_DIGITS);
	if (exponent)
		result = (negative_exp) ? (result / pow_n(10, exponent - 1)) : (pow_n(result, exponent));
	result *= negative;

	parent->value = (double *)malloc(sizeof(double));
	*(double *)parent->value = result;

	if (!is_EOF())
	{
		g_pos--;
		*position -= 1;
	}
	return ch;
}

int parse_string(const char *fstr, int *restrict position, int *restrict line, item_t *parent)
{
	int8_t ch;
	uint32_t hexxx;
	uint32_t hexxx2;
	int str_size = 1;

	parent->type = json_string;
	parent->value = (char *)malloc(str_size);
PARSE_NEXT:
	while (!is_EOF())
	{
		ch = fstr[g_pos++];
		*position += 1;
		if (ch == '"')
			return 0;

		switch (ch) // check if char is a control character
		{
		case NUL:
		case BEL:
		case BS:
		case TAB:
		case LF:
		case VT:
		case FF:
		case CR:
		case SUB:
		case ESC:
			fprintf(stderr, "SyntaxError: Bad control character at line (%d), position (%d)\n", *line, *position);
			return FATAL_ERROR;
		}

		if (ch == '\\')
		{
			ch = fstr[g_pos++];
			*position += 1;
		PARSE_SOLIDUS_CHAR:
			switch (ch)
			{
			case '"':
			case '\\':
			case '/':
				break;
			case 'b':
				ch = '\b';
				break;
			case 'f':
				ch = '\f';
				break;
			case 'n':
				ch = '\n';
				break;
			case 'r':
				ch = '\r';
				break;
			case 't':
				ch = '\t';
				break;
			case 'u':
				hexxx = 0;
				int len = 1;

				for (int i = 0; i < 4; i++)
				{
					ch = fstr[g_pos++];
					*position += 1;
					if (is_EOF())
						return FATAL_ERROR;
					if (is_digit(ch))
						hexxx = (hexxx << 4) + (ch - '0');
					else if (ch >= 'a' && ch <= 'f')
						hexxx = 10 + (hexxx << 4) + (ch - 'a');
					else if (ch >= 'A' && ch <= 'F')
						hexxx = 10 + (hexxx << 4) + (ch - 'A');
					else
					{
						fprintf(stderr, "SyntaxError: Bad unicode escape at line (%d), position (%d)\n", *line, *position);
						return FATAL_ERROR;
					}
				}
				if (hexxx >= 0xd800 && hexxx <= 0xdbff)
				{
					ch = fstr[g_pos++];
					*position += 1;
					if (ch != '\\')
					{
						*position -= 1;
						g_pos--;
						goto PARSE_NEXT;
					}

					ch = fstr[g_pos++];
					*position += 1;
					if (ch != 'u')
						goto PARSE_SOLIDUS_CHAR;

					hexxx2 = 0;
					for (int i = 0; i < 4; ++i)
					{
						ch = fstr[g_pos++];
						*position += 1;
						if (ch >= '0' && ch <= '9')
							hexxx2 = (hexxx2 << 4) + (ch - '0');
						else if (ch >= 'a' && ch <= 'f')
							hexxx2 = 10 + (hexxx2 << 4) + (ch - 'a');
						else if (ch >= 'A' && ch <= 'F')
							hexxx2 = 10 + (hexxx2 << 4) + (ch - 'A');
						else
						{
							fprintf(stderr, "SyntaxError: Bad unicode escape at line (%d), position (%d)\n", *line, *position);
							return FATAL_ERROR;
						}
					}
					hexxx = 0x10000 + (((hexxx & 0x3ff) << 10) | (hexxx2 & 0x3ff));
				}

				len = 4;
				if (hexxx < 0x80)
					len = 1;
				else if (hexxx < 0x800)
					len = 2;
				else if (hexxx < 0x10000)
					len = 3;

				str_size += len;

				parent->value = (char *)realloc((char *)parent->value, str_size * sizeof(char));
				switch (len)
				{
				case 4:
					*((char *)parent->value + str_size - (len - 2)) = ((hexxx | 0x80) & 0xbf);
					hexxx >>= 6;
					__attribute__((fallthrough));
				case 3:
					*((char *)parent->value + str_size - (len - 1)) = ((hexxx | 0x80) & 0xbf);
					hexxx >>= 6;
					__attribute__((fallthrough));
				case 2:
					*((char *)parent->value + str_size - len) = ((hexxx | 0x80) & 0xbf);
					hexxx >>= 6;
					__attribute__((fallthrough));
				case 1:
					*((char *)parent->value + str_size - (len + 1)) = hexxx | fByte[len - 1];
				}
				goto PARSE_NEXT;
			default:
				fprintf(stderr, "SyntaxError: Bad escaped character at line (%d), position (%d)\n", *line, *position);
				return FATAL_ERROR;
			}
		}
		str_size += 1;
		parent->value = (char *)realloc((char *)parent->value, str_size * sizeof(char));
		((char *)parent->value)[str_size - 2] = ch;
		((char *)parent->value)[str_size - 1] = '\0';
	}

	if (is_EOF())
	{
		fprintf(stderr, "SyntaxError: Unterminated string at line (%d), position (%d)\n", *line, *position);
		return FATAL_ERROR;
	}
	fprintf(stderr, "SyntaxError: Unexpected token '%c' at line (%d), position (%d)\n", ch, *line, *position);
	return FATAL_ERROR;
}

int parse_array(const char *fstr, int *restrict position, int *restrict line, item_t *parent)
{
	int8_t rc;
	int size = 1;
	parent->type = json_array;

	rc = parse_whitespace(fstr, position, line);
	if (is_EOF())
	{
		parent->type = json_undefined;
		fprintf(stderr, "SyntaxError: Unexpected end of JSON input at line (%d), position (%d)\n", *line, *position);
		return FATAL_ERROR;
	}
	if (rc == ']')
	{
		parent->type |= EMPTY_ITEM;
		return 0;
	}
	g_pos--;
	*position -= 1;

	parent->value = (item_t **)malloc(sizeof(item_t **) * size);
	((item_t **)parent->value)[size - 1] = malloc(sizeof(item_t));

	rc = parse_value(fstr, position, line, ((item_t **)parent->value)[size - 1]);
	if (rc == FATAL_ERROR)
	{
		((item_t **)parent->value)[size - 1]->type |= LAST_ITEM;
		return FATAL_ERROR;
	}
	if (rc == ']')
	{
		((item_t **)parent->value)[size - 1]->type |= LAST_ITEM;
		return 0;
	}
	else if (is_EOF())
	{
		((item_t **)parent->value)[size - 1]->type |= LAST_ITEM;
		fprintf(stderr, "SyntaxError: End of data when ',' or ']' was expected at line (%d), position (%d)\n", *line, *position);
		return FATAL_ERROR;
	}
	else if (rc != ',')
	{
		((item_t **)parent->value)[size - 1]->type |= LAST_ITEM;
		fprintf(stderr, "SyntaxError: Expected ',' or ']' after array element at line (%d), position (%d)\n", *line, *position);
		return FATAL_ERROR;
	}
	while (rc == ',')
	{
		size += 1;
		parent->value = realloc(parent->value, sizeof(item_t **) * size);
		((item_t **)parent->value)[size - 1] = (item_t *)malloc(sizeof(item_t));
		rc = parse_value(fstr, position, line, ((item_t **)parent->value)[size - 1]);
	}
	((item_t **)parent->value)[size - 1]->type |= LAST_ITEM;
	if (rc == FATAL_ERROR)
		return FATAL_ERROR;
	if (rc == ']')
		return 0;
	fprintf(stderr, "SyntaxError: End of data when ',' or ']' was expected at line (%d), position (%d)\n", *line, *position);
	return FATAL_ERROR;
}

int parse_object(const char *fstr, int *restrict position, int *restrict line, item_t *parent)
{
	int8_t rc;
	int size = 0;
	parent->type = json_object;

PARSE_OBJECT:
	rc = parse_whitespace(fstr, position, line);
	if (is_EOF())
	{
		parent->type = json_undefined;
		fprintf(stderr, "SyntaxError: Unexpected end of JSON input at line (%d), position (%d)\n", *line, *position);
		return FATAL_ERROR;
	}
	if (size > 0 && rc != '"')
	{
		parent->type = json_undefined;
		fprintf(stderr, "SyntaxError: Expected double-quoted property name at line (%d), position (%d)\n", *line, *position);
		return FATAL_ERROR;
	}
	if (rc == '}')
	{
		parent->type |= EMPTY_ITEM;
		return 0;
	}
	if (rc != '"' && rc != '}')
	{
		parent->type = json_undefined;
		fprintf(stderr, "SyntaxError: Expected property name or '}' at line (%d), position (%d)\n", *line, *position);
		return FATAL_ERROR;
	}
	if (!size)
		parent->value = malloc(0);
	size += 2;
	parent->value = realloc(parent->value, sizeof(item_t **) * size);
	((item_t **)parent->value)[size - 2] = malloc(sizeof(item_t));
	((item_t **)parent->value)[size - 2]->type = json_string;
	if (parse_string(fstr, position, line, ((item_t **)parent->value)[size - 2]) == FATAL_ERROR)
	{
		((item_t **)parent->value)[size - 2]->type |= LAST_ITEM;
		size -= 1;
		return FATAL_ERROR;
	}
	rc = parse_whitespace(fstr, position, line);
	if (rc == FATAL_ERROR)
	{
		((item_t **)parent->value)[size - 2]->type |= LAST_ITEM;
		size -= 1;
		return FATAL_ERROR;
	}
	if (is_EOF())
	{
		fprintf(stderr, "SyntaxError: End of data after property name when ':' was expected at line (%d), position (%d)\n", *line, *position);
		((item_t **)parent->value)[size - 2]->type |= LAST_ITEM;
		size -= 1;
		return FATAL_ERROR;
	}
	if (rc != ':')
	{
		((item_t **)parent->value)[size - 2]->type |= LAST_ITEM;
		fprintf(stderr, "SyntaxError: Expected ':' after property name at line (%d), position (%d)\n", *line, *position);
		size -= 1;
		return FATAL_ERROR;
	}
	((item_t **)parent->value)[size - 1] = malloc(sizeof(item_t));
	((item_t **)parent->value)[size - 1]->type = json_undefined;
	rc = parse_value(fstr, position, line, ((item_t **)parent->value)[size - 1]);
	if (rc == ',')
		goto PARSE_OBJECT;

	((item_t **)parent->value)[size - 1]->type |= LAST_ITEM;
	if (rc == FATAL_ERROR)
		return FATAL_ERROR;
	if (is_EOF())
	{
		fprintf(stderr, "SyntaxError: Unexpected end of JSON input at line (%d), position (%d)\n", *line, *position);
		return FATAL_ERROR;
	}
	if (rc != '}')
	{
		fprintf(stderr, "SyntaxError: Unexpected token %c at line (%d), position (%d)\n", rc, *line, *position);
		return FATAL_ERROR;
	}
	return 0;
}
int parse_bool(const char *fstr, int *restrict position, int *restrict line, char *bool_str)
{
	int8_t ch;
	*position -= 1;
	size_t str_len = strlen(bool_str);
	for (size_t i = 1; i < str_len; ++i)
	{
		ch = fstr[g_pos++];
		*position += 1;
		if (ch == bool_str[i])
			continue;
		else if (is_EOF())
		{
			fprintf(stderr, "SyntaxError: Unexpected end of JSON input at line (%d), position (%d)\n", *line, *position);
			return FATAL_ERROR;
		}
		else
		{
			fprintf(stderr, "SyntaxError: Expected '%c' instead of '%c' at line (%d), position (%d)\n", bool_str[i], ch, *line, *position);
			return FATAL_ERROR;
		}
	}
	return 0;
}
int parse_value(const char *fstr, int *restrict position, int *restrict line, item_t *parent)
{
	int8_t ch = parse_whitespace(fstr, position, line);
	switch (ch)
	{
	case '-':
	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
		if (parse_number(fstr, position, line, parent) == FATAL_ERROR)
			return FATAL_ERROR;
		break;
	case '"':
		if (parse_string(fstr, position, line, parent) == FATAL_ERROR)
			return FATAL_ERROR;
		break;
	case '{':
		if (parse_object(fstr, position, line, parent) == FATAL_ERROR)
			return FATAL_ERROR;
		break;
	case '[':
		if (parse_array(fstr, position, line, parent) == FATAL_ERROR)
			return FATAL_ERROR;
		break;
	case 't':
		if (parse_bool(fstr, position, line, "true") == FATAL_ERROR)
			return FATAL_ERROR;
		parent->type = json_true;
		break;
	case 'n':
		if (parse_bool(fstr, position, line, "null") == FATAL_ERROR)
			return FATAL_ERROR;
		parent->type = json_null;
		break;
	case 'f':
		if (parse_bool(fstr, position, line, "false") == FATAL_ERROR)
			return FATAL_ERROR;
		parent->type = json_false;
		break;
	default:
		if (is_EOF())
			fprintf(stderr, "SyntaxError: Unexpected end of data at line (%d), position (%d)\n", *line, *position);
		else
			fprintf(stderr, "SyntaxError: Unexpected token '%c' at line (%d), position (%d)\n", ch, *line, *position);
		return FATAL_ERROR;
	}
	return parse_whitespace(fstr, position, line);
}

int print_all(item_t *head, int indent_s, size_t depth)
{
	int i;
	size_t j;
	double numbuh;
	// indent spacing
	char space[indent_s + 1];
	for (int i = 0; i < indent_s; ++i)
		space[i] = '_';
	space[indent_s] = '\0';

	switch (head->type & 0xf)
	{
	case json_string:
		printf("\"%s\"", (char *)head->value);
		break;
	case json_number:
		numbuh = (*(double *)head->value);
		if (numbuh == (int32_t)numbuh)
			printf("%d", (int32_t)numbuh);
		else
			printf("%lf", numbuh);
		break;
	case json_false:
		printf("false");
		break;
	case json_true:
		printf("true");
		break;
	case json_null:
		printf("null");
		break;
	case json_array:
		putchar('[');
		++depth;
		i = 0;
		while (1)
		{
			if (head->type & EMPTY_ITEM)
				break;
			putchar('\n');
			for (j = 0; j < depth; ++j)
			{
				printf("%s", space);
			}
			print_all(((item_t **)head->value)[i], indent_s, depth);
			if (((item_t **)head->value)[i]->type & LAST_ITEM)
			{
				putchar('\n');
				--depth;
				for (j = 0; j < depth; ++j)
				{
					printf("%s", space);
				}
				break;
			}
			putchar(',');
			i++;
		}
		putchar(']');
		--depth;
		break;
	case json_object:
		putchar('{');
		++depth;
		i = 0;
		while (1)
		{
			if (head->type & EMPTY_ITEM)
				break;
			if (!(i % 2))
			{
				putchar('\n');

				for (j = 0; j < depth; ++j)
				{
					printf("%s", space);
				}
			}
			print_all(((item_t **)head->value)[i], indent_s, depth);
			if (((item_t **)head->value)[i]->type & LAST_ITEM)
			{
				putchar('\n');
				--depth;
				for (j = 0; j < depth; ++j)
				{
					printf("%s", space);
				}
				break;
			}
			if (i % 2)
				putchar(',');
			else
				putchar(':');
			i++;
		}
		putchar('}');
		--depth;
		break;
	}
	return 0;
}
int free_everything(item_t *Node)
{
	size_t i = 0;
	int res = 0;
	switch (Node->type & 0xf)
	{
	// case json_false:
	// case json_true:
	// case json_null:
	//	break;
	case json_number:
	case json_string:
		free(Node->value);
		break;
	case json_array:
	case json_object:
		i = 0;
		if (Node->type & EMPTY_ITEM)
			break;
		while (1)
		{
			res = ((item_t **)Node->value)[i]->type & LAST_ITEM;
			free_everything(((item_t **)Node->value)[i]);
			if (res)
				break;
			++i;
		}
		free(Node->value);
		break;
	}
	free(Node);
	return 0;
}
int parse_json(char *filename)
{
	// info
	int position = 0; // at line
	int line = 1;
	int rc;
	// handling file
	int fd;
	struct stat s;
	const char *fstr;
	fd = open(filename, O_RDONLY);
	if (fd < 0)
	{
		fprintf(stderr, "Error: Open %s failed (%s)\n", filename, strerror(errno));
		return errno;
	}
	if (fstat(fd, &s) < 0)
	{
		fprintf(stderr, "Error: Open %s failed (%s)\n", filename, strerror(errno));
		return errno;
	}
	file_size = s.st_size;
	fstr = mmap(0, file_size, PROT_READ, MAP_PRIVATE, fd, 0);
	if (fstr == MAP_FAILED)
	{
		fprintf(stderr, "Error: Open %s failed (%s)\n", filename, strerror(errno));
		return errno;
	}
	// if everything is in order, then continue
	g_pos = 0;
	item_t *head;
	head = (item_t *)malloc(sizeof(item_t));
	rc = parse_value(fstr, &position, &line, head);
	if (!is_EOF() && rc != FATAL_ERROR)
		fprintf(stderr, "SyntaxError: Unexpected non-whitespace character after JSON data at line (%d), position (%d)\n", line, position);
	else if (rc == FATAL_ERROR)
	{
	}
	else
	{
		print_all(head, 2, 0);
		putchar('\n');
	}
	free_everything(head);
	close(fd);
	
	return 0;
}
int main(int argc, char *argv[])
{
	int i;
	if (argc == 1)
	{
		fprintf(stderr, "Error: there is no file to parse.\n");
		return -1;
	}
	for (i = 1; i < argc; ++i)
	{
		printf("File -- %s\n", argv[i]);
		if (parse_json(argv[i]) < 0)
			return -1;
		if (i != argc - 1)
			putchar('\n');
	}

	return 0;
}
