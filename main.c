// Copyright Mikhail ysph Subbotin
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "json.h"

#define FATAL_ERROR -2

static const int CC_SIZE = 10;
static const int CONTROL_CHARS[] = {0, 7, 8, 9, 10, 11, 12, 13, 26, 27}; // search in ascii table

double pow_n(int N, int n)
{
	N <<= n;
	while (n--)
		N += N << 2;
	return N;
}

int parse_number(FILE *fptr, int *position, int *line, item_t *parent)
{
	int negative = 1; // 1 or -1
	int negative_exp = 0;
	double result = 0;
	double fraction = 0;
	int exponent = 0;

	char ch;
	int check_once;
	int frac_digits = -1;
	int is_first_digit = 0;
	fseek(fptr, -1, SEEK_CUR);
	if ((ch = fgetc(fptr)) == EOF)
		return EOF;
	if (ch == '-')
	{
		negative = -1;
		ch = fgetc(fptr);
	}
	if (ch == '0')
	{
		if ((ch = fgetc(fptr)) == EOF)
			return EOF;
		else
			goto PARSE_CHOICE;
	}
	else if (ch >= '1' && ch <= '9')
	{
		result = (result * 10) + (ch - '0');
		while ((ch = fgetc(fptr)) != EOF)
		{
			if (ch >= '0' && ch <= '9')
			{
				result = (result * 10) + (ch - '0');
				continue;
			}
			result *= negative;
		PARSE_CHOICE:
			if (ch == '.')
			{
				goto PARSE_FRACTION;
			}
			if (ch == 'e' || ch == 'E')
			{
				goto PARSE_EXPONENT;
			}
			goto STOP_PARSING;
		}
		return EOF;
	}
	else if (ch == EOF)
	{
		return EOF;
	}
	else
	{
		printf("SyntaxError: Unexpected token %c\n", ch);
		return FATAL_ERROR;
	}

PARSE_FRACTION:
	check_once = 0;
	while ((ch = fgetc(fptr)) != EOF)
	{
		if ((check_once == 0) && !(ch >= '0' && ch <= '9'))
		{
			check_once = 1;
			printf("SyntaxError: Unexpected token %c\n", ch);
			return FATAL_ERROR;
		}
		if (ch >= '0' && ch <= '9')
		{
			fraction = (fraction * 10) + (ch - '0');
			frac_digits += 1;
			check_once = 1;
			continue;
		}

		if (ch == 'e' || ch == 'E')
			goto PARSE_EXPONENT;
		goto STOP_PARSING;
	}
	return EOF;

PARSE_EXPONENT:
	if ((ch = fgetc(fptr)) == EOF)
		return EOF;
	if (ch == '-' || ch == '+')
	{
		negative_exp = (ch == '-') ? 1 : 0;
	}
	else if (ch >= '0' && ch <= '9')
	{
		fseek(fptr, -1, SEEK_CUR);
	}
	else
	{
		printf("SyntaxError: Unexpected token %c\n", ch);
		return FATAL_ERROR;
	}
	while (1)
	{
		ch = fgetc(fptr);
		if (ch == EOF)
			return EOF;
		if (ch >= '0' && ch <= '9')
		{
			is_first_digit = 1;
			exponent = (exponent * 10) + (ch - '0');
			continue;
		}
		else if (!is_first_digit)
		{
			printf("SyntaxError: Unexpected token %c\n", ch);
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

	fseek(fptr, -1, SEEK_CUR);
	return ch;
}

int parse_string(FILE *fptr, int *position, int *line, item_t *parent)
{
	char ch;
	int str_size = parent->size = 1;

	parent->value = (char *)malloc(str_size * sizeof(char));
	((char *)parent->value)[0] = '\0';

PARSE_NEXT:
	while ((ch = fgetc(fptr)) != EOF)
	{
		if (ch == '"')
			return 0;

		for (int i = 0; i < CC_SIZE; i++)
			if (ch == CONTROL_CHARS[i])
				return ch;

		if (ch == '\\')
		{
			if ((ch = fgetc(fptr)) == EOF)
				return EOF;

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
				static const unsigned char fByte[7] = {0x00, 0x00, 0xc0, 0xe0, 0xf0, 0xf8, 0xfc};

				uint32_t hexxx = 0;
				int len;

				for (int i = 0; i < 4; i++)
				{
					if ((ch = fgetc(fptr)) == EOF)
						return EOF;
					if (ch >= '0' && ch <= '9')
					{
						hexxx = (hexxx << 4) + (ch - '0');
					}
					else if (ch >= 'a' && ch <= 'f')
					{
						hexxx = 10 + (hexxx << 4) + (ch - 'a');
					}
					else if (ch >= 'A' && ch <= 'F')
					{
						hexxx = 10 + (hexxx << 4) + (ch - 'A');
					}
					else
					{
						printf("SyntaxError: Unexpected string in JSON\n");
						return FATAL_ERROR;
					}
				}
				len = 4;
				if (hexxx < 0x80)
					len = 1;
				else if (hexxx < 0x800)
					len = 2;
				else if (hexxx < 0x10000)
					len = 3;

				str_size += len;

				parent->value = (char *)realloc(parent->value, str_size * sizeof(char));
				switch (len)
				{
				case 4:
					*((char *)parent->value + str_size - 1) = ((hexxx | 0x80) & 0xbf);
					hexxx >>= 6;
				case 3:
					*((char *)parent->value + str_size - 2) = ((hexxx | 0x80) & 0xbf);
					hexxx >>= 6;
				case 2:
					*((char *)parent->value + str_size - 3) = ((hexxx | 0x80) & 0xbf);
					hexxx >>= 6;
				case 1:
					*((char *)parent->value + str_size - 4) = hexxx | fByte[len];
				}
				goto PARSE_NEXT;
			default:
				printf("SyntaxError: Unexpected token %c in JSON\n", ch);
				return FATAL_ERROR;
			}
		}

		str_size += 1;
		parent->value = (char *)realloc(parent->value, str_size * sizeof(char));
		((char *)parent->value)[str_size - 2] = ch;
		((char *)parent->value)[str_size - 1] = '\0';

		parent->size = str_size;
	}
	if (ch == EOF)
	{
		puts("SyntaxError: Unexpected end of JSON input");
		return FATAL_ERROR;
	}
	printf("SyntaxError: Unexpected token %c in JSON\n", ch);
	return FATAL_ERROR;
}
int parse_whitespace(FILE *fptr, int *position, int *line)
{
	char ch;
	while ((ch = fgetc(fptr)) != EOF)
	{
		switch (ch)
		{
		case '\n':
			*line += 1;
			*position = 0;
			break;
		case '\t':
			*position += 1;
			break;
		case ' ':
		case '\r':
			*position += 1;
			break;
		default:
			return ch;
		}
	}
	return EOF;
}
int parse_value(FILE *fptr, int *position, int *line, item_t *parent)
{
	int size_bool_str;
	char bool_str[6];

	char returnCode;
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
		parent->type = json_number;
		returnCode = parse_number(fptr, position, line, parent);

		if (returnCode == FATAL_ERROR)
			return FATAL_ERROR;
		if (returnCode == EOF)
			return EOF;
		return parse_whitespace(fptr, position, line);
	case '"':
		parent->type = json_string;
		returnCode = parse_string(fptr, position, line, parent);
		if (returnCode == FATAL_ERROR)
			return FATAL_ERROR;
		return parse_whitespace(fptr, position, line);
	case '{':
		parent->items = (item_t **)malloc(0);
		parent->type = json_object;
		parent->size = 0;

		int comma_before = 0;
	PARSE_OBJECT:
		returnCode = parse_whitespace(fptr, position, line);
		if (returnCode == EOF)
		{
			puts("SyntaxError: Unexpected end of JSON input");
			return FATAL_ERROR;
		}
		if (comma_before && returnCode != '"')
		{
			printf("SyntaxError: Unexpected token %c\n", returnCode);
			return FATAL_ERROR;
		}
		if (returnCode == '}')
			return parse_whitespace(fptr, position, line); // empty object
		if (returnCode != '"')
		{
			printf("SyntaxError: Unexpected token %c\n", returnCode);
			return FATAL_ERROR;
		}
		parent->size += 2; // there should be value for each key
		parent->items = (item_t **)realloc(parent->items, sizeof(item_t **) * parent->size);
		parent->items[parent->size - 2] = (item_t *)malloc(sizeof(item_t));
		parent->items[parent->size - 2]->type = json_string;
		returnCode = parse_string(fptr, position, line, parent->items[parent->size - 2]);
		if (returnCode == FATAL_ERROR)
			return FATAL_ERROR;
		returnCode = parse_whitespace(fptr, position, line);
		if (returnCode == FATAL_ERROR)
			return FATAL_ERROR;
		if (returnCode == EOF)
		{
			puts("SyntaxError: Unexpected end of JSON input");
			return EOF;
		}
		if (returnCode != ':')
			return returnCode;

		parent->items[parent->size - 1] = (item_t *)malloc(sizeof(item_t));
		returnCode = parse_value(fptr, position, line, parent->items[parent->size - 1]);
		if (returnCode == FATAL_ERROR)
			return FATAL_ERROR;
		if (returnCode == ',')
		{
			comma_before = 1;
			goto PARSE_OBJECT;
		}
		if (returnCode == EOF)
		{
			puts("SyntaxError: Unexpected end of JSON input");
			return FATAL_ERROR;
		}
		if (returnCode != '}')
		{
			printf("SyntaxError: Unexpected token %c\n", returnCode);
			return FATAL_ERROR;
		}
		return parse_whitespace(fptr, position, line);
	case '[': // parse_array
		parent->items = (item_t **)malloc(0);
		parent->size = 0;
		parent->type = json_array;

		returnCode = parse_whitespace(fptr, position, line);
		if (returnCode == EOF)
		{
			puts("SyntaxError: Unexpected end of JSON input");
			return FATAL_ERROR;
		}
		if (returnCode == ']')
			return parse_whitespace(fptr, position, line); // empty array
		fseek(fptr, -1, SEEK_CUR);

		parent->size += 1;
		parent->items = (item_t **)realloc(parent->items, sizeof(item_t **) * parent->size);
		parent->items[parent->size - 1] = (item_t *)malloc(sizeof(item_t));

		returnCode = parse_value(fptr, position, line, parent->items[parent->size - 1]);
		if (returnCode == FATAL_ERROR)
			return FATAL_ERROR;
		if (returnCode == ']')
			return parse_whitespace(fptr, position, line);
		else if (returnCode != ',')
		{
			printf("SyntaxError: Unexpected token %c\n", returnCode);
			return FATAL_ERROR;
		}
		while (returnCode != ']' && returnCode == ',')
		{
			parent->size += 1;
			parent->items = (item_t **)realloc(parent->items, sizeof(item_t **) * parent->size);
			parent->items[parent->size - 1] = (item_t *)malloc(sizeof(item_t));
			returnCode = parse_value(fptr, position, line, parent->items[parent->size - 1]);
		}
		if (returnCode == FATAL_ERROR)
			return FATAL_ERROR;
		if (returnCode != EOF)
			return parse_whitespace(fptr, position, line);

		puts("SyntaxError: Unexpected end of JSON input");
		return FATAL_ERROR;
	case 't':
		size_bool_str = sizeof("true");
		strcpy(bool_str, "true");
		goto PARSE_BOOLEAN_STR;
	case 'n':
		size_bool_str = sizeof("null");
		strcpy(bool_str, "null");
		goto PARSE_BOOLEAN_STR;
	case 'f':
		size_bool_str = sizeof("false");
		strcpy(bool_str, "false");

	PARSE_BOOLEAN_STR:
		for (int i = 1; i < size_bool_str - 1; ++i)
		{
			ch = fgetc(fptr);
			if (ch == bool_str[i])
				continue;
			else if (ch == EOF)
			{
				puts("SyntaxError: Unexpected end of JSON input");
				return FATAL_ERROR;
			}
			else
			{
				printf("Error: Expected '%c' instead of '%c'\n", bool_str[i], ch);
				return FATAL_ERROR;
			}
		}
		if (strcmp(bool_str, "true") == 0)
			parent->type = json_true;
		else if (strcmp(bool_str, "null") == 0)
			parent->type = json_null;
		else
			parent->type = json_false;

		return parse_whitespace(fptr, position, line);
	default:
		printf("SyntaxError: Unexpected token '%c'\n", ch);
		return FATAL_ERROR;
	}
}

int print_all(item_t *head, int indent_s, int64_t depth)
{ // indent spacing
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
		double numbuh = *(double *)head->value;
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
		printf("[");
		if (head->size)
		{
			depth++;
			printf("\n");
			for (int64_t i = 0; i < depth; ++i)
				printf("%s", space);
		}
		for (int i = 0; i < head->size; ++i)
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
			printf("\n");
			for (int64_t i = 0; i < depth; ++i)
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
			for (int64_t i = 0; i < depth; ++i)
				printf("%s", space);
		}
		for (int i = 0; i < head->size; ++i)
		{
			print_all(head->items[i], indent_s, depth);
			if ((i != head->size - 1) && (i % 2))
			{
				printf(",\n");
				for (int64_t i = 0; i < depth; ++i)
					printf("%s", space);
			}
			else if ((i != head->size - 1) && !(i % 2))
			{
				printf(":");
			}
			else if (i == head->size - 1)
			{
				printf("\n");
				depth--;
				for (int64_t i = 0; i < depth; ++i)
					printf("%s", space);
			}
			else
			{
				depth--;
			}
		}
		printf("}");
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
		for (int i = 0; i < Node->size; ++i)
			free_everything(Node->items[i]);
		free(Node->items);
		break;
	case json_object:
		for (int i = 0; i < Node->size; ++i)
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
		if (returnCode != FATAL_ERROR && returnCode != EOF)
		{
			printf("SyntaxError: Unexpected token '%c'\n", returnCode);
		}
		else if (returnCode == FATAL_ERROR)
		{
			goto END_THAT;
		}
		else
		{
			print_all(head, 2, 0);
			printf("\n");
		}
	END_THAT:
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
		printf("Error: there is no file to parse.\n");
		return 1;
	}
	printf("File -- %s\n\n", argv[argc - 1]);

	if (parse_json(argv[1]))
	{
		printf("Error: file cannot be accessed.\n");
		return 1;
	}
	return 0;
}
