// Copyright Mikhail ysph Subbotin
#include <stdio.h>
#include <stdlib.h>

static const int CC_SIZE = 10;
static const int SC_SIZE = 9;
static const int CONTROL_CHARS[] = {0, 7, 8, 9, 10, 11, 12, 13, 26, 27}; // search in ascii table
static const int SOLIDUS_CHARS[] = {'"', '\\', '/', 'b', 'f', 'n', 'r', 't', 'u'};

#define FATAL_ERROR 1

int parse_number(FILE *fptr, int *position, int *line, char number[]) {
	char ch;
	char returnCode;
	int peek_how_many = 0;
	int check_once;
	int is_first_digit = 0;
	fseek(fptr, -1, SEEK_CUR);
	if ((ch = fgetc(fptr)) == EOF) return EOF;
	if (ch == '-') {
		peek_how_many += 1;
		ch = fgetc(fptr);
	}
	if (ch == '0') {
		peek_how_many += 1;
		if ((ch = fgetc(fptr)) == EOF) return EOF;
		else goto PARSE_CHOICE;
	} else if (ch >= '1' && ch <= '9') {
		peek_how_many += 1;
		while ((ch = fgetc(fptr)) != EOF) {
			if (ch >= '0' && ch <= '9') {
				peek_how_many += 1;
				continue;
			}
			PARSE_CHOICE:
			if (ch == '.') {
				peek_how_many += 1;
				goto PARSE_FRACTION;
			}
			if (ch == 'e' || ch == 'E') {
				peek_how_many += 1;
				goto PARSE_EXPONENT;
			}
			goto STOP_PARSING;
		}
		return EOF;
	} else if (ch == EOF) {
		return EOF;
	} else {
		printf("SyntaxError: Unexpected token %c\n",ch);
		return FATAL_ERROR;
	}

	PARSE_FRACTION:
	check_once = 0;
	while ((ch = fgetc(fptr)) != EOF) {
		if ((check_once == 0) && !(ch >= '0' && ch <= '9')) {
			check_once = 1;
			printf("SyntaxError: Unexpected token %c\n",ch);
			return FATAL_ERROR;
		}
		if (ch >= '0' && ch <= '9') {
			peek_how_many += 1;
			check_once = 1;
			continue;
		}
		if (ch == 'e' || ch == 'E') {
			peek_how_many += 1;
			goto PARSE_EXPONENT;
		}
		goto STOP_PARSING;
	}
	return EOF;

	PARSE_EXPONENT:
	if ((ch = fgetc(fptr)) == EOF) return EOF;
	if (ch == '-' || ch == '+') {
		peek_how_many += 1;
	} else if (ch >= '0' && ch <= '9') {
		fseek(fptr, -1, SEEK_CUR);
	} else {
		printf("SyntaxError: Unexpected token %c\n", ch);
		return FATAL_ERROR;
	}
	while (ch = fgetc(fptr)) {
		if (ch == EOF) return EOF;
		if (ch >= '0' && ch <= '9') {
			is_first_digit = 1;
			peek_how_many += 1;
			continue;
		} else if (!is_first_digit) {
			printf("SyntaxError: Unexpected token %c\n", ch);
			return FATAL_ERROR;
		}
		goto STOP_PARSING;
	}

	STOP_PARSING:
	number = malloc(sizeof(char) * peek_how_many);
	fseek(fptr, -(peek_how_many + 1), SEEK_CUR);
	for (int i = 0; i < peek_how_many; ++i) {
		ch = fgetc(fptr);
		number[i] = ch;
	}
	free(number);
	return ch;
}

int parse_string(FILE *fptr, int *position, int *line) {
	char ch;
	PARSE_NEXT:
	while ((ch = fgetc(fptr)) != EOF) {
		if (ch == '"') {
			return 0;
		}
		for (int i = 0; i < CC_SIZE; i++) {
			if (ch == CONTROL_CHARS[i]) return 'c';
		}
		if (ch == '\\') {
			if ((ch = fgetc(fptr)) == EOF) return EOF;
			if (ch == SOLIDUS_CHARS[8]) {
				for (int i = 0; i < 4; i++) {
					if ((ch = fgetc(fptr)) == EOF) return EOF;
					if ((ch >= 48 && ch <= 57) || (ch >= 65 && ch <= 70) || (ch >= 97 && ch <= 102)) continue;
					else {
						printf("SyntaxError: Unexpected string in JSON\n");
						return FATAL_ERROR;
					}
				}
				goto PARSE_NEXT;
			}
			for (int i = 0; i < SC_SIZE - 1; ++i) {
				if (ch == SOLIDUS_CHARS[i])
					goto PARSE_NEXT;
			}
			printf("SyntaxError: Unexpected token %c in JSON\n", ch);
			return FATAL_ERROR;
		}
	}
	if (ch == EOF) {
		puts("SyntaxError: Unexpected end of JSON input");
		return FATAL_ERROR;
	}
	printf("SyntaxError: Unexpected token %c in JSON\n", ch);
	return FATAL_ERROR;
}
int parse_whitespace(FILE *fptr, int *position, int *line) {
	char ch;
	while ((ch = fgetc(fptr)) != EOF) {
		switch (ch) {
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
int parse_value(FILE *fptr, int *position, int *line) {
	char returnCode;
	char number[] = "";
	static const char true[] = "true";
	static const char null[] = "null";
	static const char false[] = "false";
	char ch = parse_whitespace(fptr, position, line);
	switch (ch) {
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
			returnCode = parse_number(fptr, position, line, number);
			if (returnCode == FATAL_ERROR) {return FATAL_ERROR;}
			if (returnCode == EOF) return EOF;
			return parse_whitespace(fptr, position, line);
		case '"':
			returnCode = parse_string(fptr, position, line);
			if (returnCode == FATAL_ERROR) return FATAL_ERROR;
			//if (returnCode) return returnCode;
			return parse_whitespace(fptr, position, line);
		case '{':
			int comma_before = 0;
			PARSE_OBJECT:
			returnCode = parse_whitespace(fptr, position, line);
			if (returnCode == EOF) {
				printf("SyntaxError: Unexpected end of JSON input\n");
				return FATAL_ERROR;
			}
			if (comma_before && returnCode != '"') {
				printf("SyntaxError: Unexpected token %c\n", returnCode);
				return FATAL_ERROR;
			}
			if (returnCode == '}') return parse_whitespace(fptr, position, line);
			if (returnCode != '"') {
				printf("SyntaxError: Unexpected token %c\n", returnCode);
				return FATAL_ERROR;
			}
			returnCode = parse_string(fptr, position, line);	
			if (returnCode == FATAL_ERROR) return FATAL_ERROR;		
			returnCode = parse_whitespace(fptr, position, line);
			if (returnCode == FATAL_ERROR) return FATAL_ERROR;
			if (returnCode == EOF) {
				printf("SyntaxError: Unexpected end of JSON input\n");
				return EOF;
			}
			if (returnCode != ':') {
				return returnCode;
			}
			returnCode = parse_value(fptr, position, line);
			if (returnCode == FATAL_ERROR) return FATAL_ERROR;
			if (returnCode == ',') {
				comma_before = 1;
				goto PARSE_OBJECT;
			}
			if (returnCode == EOF) {
				puts("SyntaxError: Unexpected end of JSON input");
				return FATAL_ERROR;
			}
			if (returnCode != '}') {
				printf("SyntaxError: Unexpected token %c\n", returnCode);
				return FATAL_ERROR;
			}
			return parse_whitespace(fptr, position, line);
		case '[':
			returnCode = parse_whitespace(fptr, position, line);
			if (returnCode == EOF) {
				puts("SyntaxError: Unexpected end of JSON input\n");
				return FATAL_ERROR;
			}
			if (returnCode == ']') return parse_whitespace(fptr, position, line);
			fseek(fptr, -1, SEEK_CUR);

			returnCode = parse_value(fptr, position, line);
			if (returnCode == FATAL_ERROR) return FATAL_ERROR;
			if (returnCode == ']') {
				return parse_whitespace(fptr, position, line);
			} else if (returnCode != ',') {
				printf("SyntaxError: Unexpected token %c\n", returnCode);
				return FATAL_ERROR;
			}
			while (returnCode != ']' && returnCode == ',') {
				returnCode = parse_value(fptr, position, line);
			}
			if (returnCode == FATAL_ERROR) return FATAL_ERROR;
			if (returnCode != EOF) return parse_whitespace(fptr, position, line);
			printf("SyntaxError: Unexpected end of JSON input\n");
			return FATAL_ERROR;
		case 't':
			for (int i = 1; i < 4; ++i) {
				ch = fgetc(fptr);
				if (ch == true[i]) continue;
				else if (ch == EOF) {
					puts("SyntaxError: Unexpected end of JSON input");
					return FATAL_ERROR;
				} else {
					printf("Error: Expected '%c' instead of '%c'\n", true[i], ch);
					return FATAL_ERROR;
				}
			}
			return parse_whitespace(fptr, position, line);
		case 'f':
			for (int i = 1; i < 5; ++i) {
				ch = fgetc(fptr);
				if (ch == false[i]) continue;
				else if (ch == EOF) {
					puts("SyntaxError: Unexpected end of JSON input");
					return FATAL_ERROR;
				} else {
					printf("Error: Expected '%c' instead of '%c'\n", false[i], ch);
					return FATAL_ERROR;
				}
			}
			return parse_whitespace(fptr, position, line);
		case 'n':
			for (int i = 1; i < 4; ++i) {
				ch = fgetc(fptr);
				if (ch == null[i]) continue;
				else if (ch == EOF) {
					puts("SyntaxError: Unexpected end of JSON input");
					return FATAL_ERROR;
				} else {
					printf("Error: Expected '%c' instead of '%c'\n", null[i], ch);
					return FATAL_ERROR;
				}
			}
			return parse_whitespace(fptr, position, line);
		default:
			printf("SyntaxError: Unexpected token '%c'\n", ch);
			return FATAL_ERROR;
	}
}
int parse_json(char *str) {
	FILE *fptr;
	char ch;

	fptr = fopen(str,"r");
	if (fptr) {
		printf("File opened.\n");
		int position = 0, line = 1, returnCode;
		returnCode = parse_value(fptr, &position, &line);
		if (returnCode != FATAL_ERROR && returnCode != EOF) {
			printf("SyntaxError: Unexpected token '%c'\n", returnCode);
		}
		fclose(fptr);
		printf("\nFile closed.\n");
		return 0;
	}
	return 1;
}
int main(int argc, char *argv[]) {
	if (argc == 1) {
		printf("Error: there is no file to parse.\n");
		return 1;
	}
	printf("\n\nFile -- %s\n", argv[1]);

	if (parse_json(argv[1])) {
		printf("Error: file cannot be accessed.\n");
		return 1;
	}
	return 0;
}
