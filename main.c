// Copyright Mikhail ysph Subbotin
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "json.h"

#define is_digit(c) ((unsigned)((c) - '0') < 10)

double pow_n(int N, int n)
{
	N <<= n;
	while (n--)
		N += N << 2;
	return N;
}

int parse_whitespace(FILE *fptr, int *restrict position, int *restrict line)
{
	int8_t ch;
	while (1)
	{
		ch = fgetc(fptr);
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

int parse_number(FILE *fptr, int *restrict position, int *restrict line, item_t *parent)
{
	char ch;
	int check_once;

	int negative = 1; // 1 or -1
	int negative_exp = 0;
	double result = 0;
	double fraction = 0;
	int exponent = 0;
	int frac_digits = -1;
	int is_first_digit = 0;

	parent->type = json_number;
	fseek(fptr, -1, SEEK_CUR);
	ch = fgetc(fptr);

	if (feof(fptr))
		return FATAL_ERROR;
	if (ch == '-')
	{
		negative = -1;
		ch = fgetc(fptr);
		*position += 1;
	}
	if (ch == '0')
	{
		ch = fgetc(fptr);
		*position += 1;
		if (feof(fptr))
			return FATAL_ERROR;
		else
			goto PARSE_CHOICE;
	}
	else if (is_digit(ch))
	{
		result = (result * 10) + (ch - '0');
		while (1)
		{
			ch = fgetc(fptr);
			*position += 1;
			if (is_digit(ch))
				result = (result * 10) + (ch - '0');
			else
				break;
		};
		result *= negative;
	PARSE_CHOICE:
		if (ch == '.')
			goto PARSE_FRACTION;
		if (ch == 'e' || ch == 'E')
			goto PARSE_EXPONENT;
		goto STOP_PARSING;
	}
	else if (feof(fptr))
		return FATAL_ERROR;
	else
	{
		fprintf(stderr, "SyntaxError: Unexpected non-digit token '%c' at line (%d), position (%d)\n", ch, *line, *position);
		return FATAL_ERROR;
	}

PARSE_FRACTION:
	check_once = 0;

	while (1)
	{
		ch = fgetc(fptr);
		*position += 1;

		if (is_digit(ch))
		{
			fraction = (fraction * 10) + (ch - '0');
			frac_digits += 1;
			check_once = 1;
			continue;
		}
		else if (!check_once)
		{
			fprintf(stderr, "SyntaxError: Unterminated fractional number at line (%d), position (%d)\n", *line, *position);
			return FATAL_ERROR;
		}
		else if (ch != 'e' || ch != 'E')
		{
			goto STOP_PARSING;
		}
		break;//goto PARSE_EXPONENT;
	}
PARSE_EXPONENT:
	ch = fgetc(fptr);
	*position += 1;
	if (feof(fptr))
		return FATAL_ERROR;
	if (ch == '-' || ch == '+')
		negative_exp = (ch == '-') ? 1 : 0;
	else if (is_digit(ch))
	{
		fseek(fptr, -1, SEEK_CUR);
		*position -= 1;
	}
	else
	{
		fprintf(stderr, "SyntaxError: Exponent part is missing a number at line (%d), position (%d)\n", *line, *position);
		return FATAL_ERROR;
	}
	while (1)
	{
		ch = fgetc(fptr);
		*position += 1;
		if (feof(fptr))
			return FATAL_ERROR;
		if (is_digit(ch))
		{
			is_first_digit = 1;
			exponent = (exponent * 10) + (ch - '0');
			continue;
		}
		else if (!is_first_digit)
		{
			fprintf(stderr, "SyntaxError: Exponent part is missing a number at line (%d), position (%d)\n", *line, *position);
			return FATAL_ERROR;
		}
		goto STOP_PARSING;
	}

STOP_PARSING:
	if (frac_digits > -1)
	{
		result += ((fraction / pow_n(10, frac_digits)) * negative);
	}
	if (negative_exp && exponent > 0)
	{
		result = result / pow_n(10, exponent - 1);
	}
	else if (!negative_exp && exponent > 0)
	{
		result = pow_n(result, exponent);
	}
	parent->value = (double *)malloc(sizeof(double));
	*(double *)parent->value = result;

	if (!feof(fptr))
	{
		fseek(fptr, -1, SEEK_CUR);
		*position -= 1;
	}
	return ch;
}

int parse_string(FILE *fptr, int *restrict position, int *restrict line, item_t *parent)
{
	char ch;
	uint32_t hexxx;
	uint32_t hexxx2;
	int str_size = 1;

	parent->type = json_string;
	parent->value = (char *)malloc(str_size);
PARSE_NEXT:
	while (!feof(fptr))
	{
		ch = fgetc(fptr);
		*position += 1;
		if (ch == '"')
			return 0;

		switch (ch) // check if ch is a control character
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
			ch = fgetc(fptr);
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
					ch = fgetc(fptr);
					*position += 1;
					if (feof(fptr))
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
					ch = fgetc(fptr);
					*position += 1;
					if (ch != '\\')
					{
						fseek(fptr, -1, SEEK_CUR);
						*position -= 1;
						goto PARSE_NEXT;
					}

					ch = fgetc(fptr);
					*position += 1;
					if (ch != 'u')
					{
						goto PARSE_SOLIDUS_CHAR;
					}
					hexxx2 = 0;
					for (int i = 0; i < 4; ++i)
					{
						ch = fgetc(fptr);
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
					hexxx = 0x10000 + (((hexxx & 0x3FF) << 10) | (hexxx2 & 0x3FF));
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
					*((char *)parent->value + str_size - (len + 1)) = hexxx | fByte[len];
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
		parent->size = str_size;
	}

	if (feof(fptr))
	{
		fprintf(stderr, "SyntaxError: Unterminated string at line (%d), position (%d)\n", *line, *position);
		return FATAL_ERROR;
	}
	fprintf(stderr, "SyntaxError: Unexpected token '%c' at line (%d), position (%d)\n", ch, *line, *position);
	return FATAL_ERROR;
}

int parse_array(FILE *fptr, int *restrict position, int *restrict line, item_t *parent)
{
	char returnCode;

	parent->items = (item_t **)malloc(0);
	parent->size = 0;
	parent->type = json_array;

	returnCode = parse_whitespace(fptr, position, line);
	if (feof(fptr))
	{
		fprintf(stderr, "SyntaxError: Unexpected end of JSON input at line (%d), position (%d)\n", *line, *position);
		return FATAL_ERROR;
	}
	if (returnCode == ']')
		return 0; // empty array
	fseek(fptr, -1, SEEK_CUR);
	*position -= 1;

	parent->size += 1;
	parent->items = (item_t **)realloc(parent->items, sizeof(item_t **) * parent->size);
	parent->items[parent->size - 1] = (item_t *)malloc(sizeof(item_t));

	returnCode = parse_value(fptr, position, line, parent->items[parent->size - 1]);
	if (returnCode == FATAL_ERROR)
	{
		return FATAL_ERROR;
	}
	if (returnCode == ']')
		return 0;
	else if (feof(fptr))
	{
		fprintf(stderr, "SyntaxError: End of data when ',' or ']' was expected at line (%d), position (%d)\n", *line, *position);
		return FATAL_ERROR;
	}
	else if (returnCode != ',')
	{
		fprintf(stderr, "SyntaxError: Expected ',' or ']' after array element at line (%d), position (%d)\n", *line, *position);
		return FATAL_ERROR;
	}
	while (returnCode == ',')
	{
		parent->size += 1;
		parent->items = (item_t **)realloc(parent->items, sizeof(item_t **) * parent->size);
		parent->items[parent->size - 1] = (item_t *)malloc(sizeof(item_t));
		returnCode = parse_value(fptr, position, line, parent->items[parent->size - 1]);
	}
	if (returnCode == FATAL_ERROR)
	{
		return FATAL_ERROR;
	}
	if (returnCode == ']')
	{
		return 0;
	}
	fprintf(stderr, "SyntaxError: End of data when ',' or ']' was expected at line (%d), position (%d)\n", *line, *position);
	return FATAL_ERROR;
}

int parse_object(FILE *fptr, int *restrict position, int *restrict line, item_t *parent)
{
	char returnCode;

	parent->items = (item_t **)malloc(0);
	parent->type = json_object;
	parent->size = 0;

	int comma_before = 0;
PARSE_OBJECT:
	returnCode = parse_whitespace(fptr, position, line);
	if (feof(fptr))
	{
		fprintf(stderr, "SyntaxError: Unexpected end of JSON input at line (%d), position (%d)\n", *line, *position);
		return FATAL_ERROR;
	}
	if (comma_before && returnCode != '"')
	{
		fprintf(stderr, "SyntaxError: Expected double-quoted property name at line (%d), position (%d)\n", *line, *position);
		return FATAL_ERROR;
	}
	if (returnCode == '}')
		return 0; // empty object
	if (returnCode != '"' && returnCode != '}')
	{
		fprintf(stderr, "SyntaxError: Expected property name or '}' at line (%d), position (%d)\n", *line, *position);
		return FATAL_ERROR;
	}
	parent->size += 2; // there should be value for each key
	parent->items = (item_t **)realloc(parent->items, sizeof(item_t **) * parent->size);
	parent->items[parent->size - 2] = (item_t *)malloc(sizeof(item_t));
	parent->items[parent->size - 2]->type = json_string;
	if (parse_string(fptr, position, line, parent->items[parent->size - 2]) == FATAL_ERROR)
	{
		parent->size -= 1;
		return FATAL_ERROR;
	}
	returnCode = parse_whitespace(fptr, position, line);
	if (returnCode == FATAL_ERROR)
	{
		parent->size -= 1;
		return FATAL_ERROR;
	}
	if (feof(fptr))
	{
		fprintf(stderr, "SyntaxError: End of data after property name when ':' was expected at line (%d), position (%d)\n", *line, *position);
		parent->size -= 1;
		return FATAL_ERROR;
	}
	if (returnCode != ':')
	{
		fprintf(stderr, "SyntaxError: Expected ':' after property name at line (%d), position (%d)\n", *line, *position);
		parent->size -= 1;
		return FATAL_ERROR;
	}
	parent->items[parent->size - 1] = (item_t *)malloc(sizeof(item_t));
	returnCode = parse_value(fptr, position, line, parent->items[parent->size - 1]);
	if (returnCode == FATAL_ERROR)
		return FATAL_ERROR;

	if (returnCode == ',')
	{
		comma_before = 1;
		goto PARSE_OBJECT;
	}
	if (feof(fptr))
	{
		fprintf(stderr, "SyntaxError: Unexpected end of JSON input at line (%d), position (%d)\n", *line, *position);
		return FATAL_ERROR;
	}

	if (returnCode != '}')
	{
		fprintf(stderr, "SyntaxError: Unexpected token %c at line (%d), position (%d)\n", returnCode, *line, *position);
		return FATAL_ERROR;
	}
	return 0;
}

int parse_bool(FILE *fptr, int *restrict position, int *restrict line, char *bool_str)
{
	char ch;
	*position -= 1;
	size_t str_len = strlen(bool_str);
	for (size_t i = 1; i < str_len; ++i)
	{
		ch = fgetc(fptr);
		*position += 1;
		if (ch == bool_str[i])
			continue;
		else if (feof(fptr))
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
int parse_value(FILE *fptr, int *restrict position, int *restrict line, item_t *parent)
{
	char ch = parse_whitespace(fptr, position, line);
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
		if (parse_number(fptr, position, line, parent) == FATAL_ERROR)
			return FATAL_ERROR;
		break;
	case '"':
		if (parse_string(fptr, position, line, parent) == FATAL_ERROR)
			return FATAL_ERROR;
		break;
	case '{':
		if (parse_object(fptr, position, line, parent) == FATAL_ERROR)
			return FATAL_ERROR;
		break;
	case '[': // parse_array
		if (parse_array(fptr, position, line, parent) == FATAL_ERROR)
			return FATAL_ERROR;
		break;
	case 't':
		if (parse_bool(fptr, position, line, "true") == FATAL_ERROR)
			return FATAL_ERROR;
		parent->type = json_true;
		break;
	case 'n':
		if (parse_bool(fptr, position, line, "null") == FATAL_ERROR)
			return FATAL_ERROR;
		parent->type = json_null;
		break;
	case 'f':
		if (parse_bool(fptr, position, line, "false") == FATAL_ERROR)
			return FATAL_ERROR;
		parent->type = json_false;
		break;
	default:
		if (feof(fptr))
			fprintf(stderr, "SyntaxError: Unexpected end of data at line (%d), position (%d)\n", *line, *position);
		else
			fprintf(stderr, "SyntaxError: Unexpected token '%c' at line (%d), position (%d)\n", ch, *line, *position);
		return FATAL_ERROR;
	}
	return parse_whitespace(fptr, position, line);
}

int print_all(item_t *head, int indent_s, size_t depth)
{
	double numbuh;
	// indent spacing
	char space[indent_s + 1];
	for (int i = 0; i < indent_s; ++i)
		space[i] = '_';
	space[indent_s] = '\0';

	switch (head->type)
	{
	case json_string:
		printf("\"%s\"", (char *)head->value);
		break;
	case json_number:
		numbuh = (*(double *)head->value);
		if (numbuh == (int64_t)numbuh)
			printf("%ld", (int64_t)numbuh);
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
		if (head->size)
		{
			depth++;
			putchar('\n');
			for (size_t i = 0; i < depth; ++i)
				printf("%s", space);
		}
		for (size_t i = 0; i < head->size; ++i)
		{
			print_all(head->items[i], indent_s, depth);
			if (i != head->size - 1)
			{
				printf(",");
			}
			else
			{
				depth--;
			}
			putchar('\n');
			for (size_t i = 0; i < depth; ++i)
				printf("%s", space);
		}
		printf("]");
		break;
	case json_object:
		printf("{");
		if (head->size)
		{
			depth++;
			printf("\n");
			for (size_t i = 0; i < depth; ++i)
				printf("%s", space);
		}
		for (size_t i = 0; i < head->size; ++i)
		{
			print_all(head->items[i], indent_s, depth);
			if ((i != head->size - 1) && (i % 2))
			{
				puts(",");
				for (size_t i = 0; i < depth; ++i)
					printf("%s", space);
			}
			else if ((i != head->size - 1) && !(i % 2))
			{
				putchar(':');
			}
			else if (i == head->size - 1)
			{
				putchar('\n');
				depth--;
				for (size_t i = 0; i < depth; ++i)
					printf("%s", space);
			}
			else
			{
				depth--;
			}
		}
		putchar('}');
		break;
	default:
		puts("else");
		break;
	}
	return 0;
}
int free_everything(item_t *Node)
{
	switch (Node->type)
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
		for (size_t i = 0; i < Node->size; ++i)
			free_everything(Node->items[i]);
		free(Node->items);
		break;
	case json_object:
		for (size_t i = 0; i < Node->size; ++i)
			free_everything(Node->items[i]);
		free(Node->items);
		break;
	}
	free(Node);
	return 0;
}
int parse_json(char *str)
{
	FILE *fptr;
	fptr = fopen(str, "r");
	if (fptr)
	{
		item_t *head;
		head = (item_t *)malloc(sizeof(item_t));

		int position = 0, line = 1, returnCode;

		returnCode = parse_value(fptr, &position, &line, head);
		if (!feof(fptr) && returnCode != FATAL_ERROR)
			fprintf(stderr, "Syntax error: Unexpected non-whitespace character after JSON data at line (%d), position (%d)\n", line, position);
		else if (returnCode == FATAL_ERROR)
		{
		}
		else
		{
			print_all(head, 2, 0);
			printf("\n");
		}
		free_everything(head);
		fclose(fptr);

		return 0;
	}

	return -1;
}
int main(int argc, char *argv[])
{
	if (argc == 1)
	{
		fprintf(stderr, "Error: there is no file to parse.\n");
		return 1;
	}
	printf("File -- %s\n\n", argv[argc - 1]);

	if (parse_json(argv[1]))
	{
		fprintf(stderr, "Error: file cannot be accessed.\n");
		return 1;
	}
	return 0;
}
