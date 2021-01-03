// Copyright Mikhail ysph Subbotin
#include <stdio.h>

static const int CC_SIZE = 10;
static const int SC_SIZE = 9;
static const int CONTROL_CHARS[] = {0, 7, 8, 9, 10, 11, 12, 13, 26, 27}; // check out the ascii table
static const int SOLIDUS_CHARS[] = {34, 92, 47, 98, 102, 110, 114, 116, 117};

int parse_string(FILE *fptr, int *position, int *line) {
	char ch;
	PARSE_NEXT:
	while ((ch = fgetc(fptr)) != EOF) {
		printf("%c", ch);
		if (ch == '"') {
			//printf("\n");
			return 0;
		}
		for (int i = 0; i < CC_SIZE; i++) {
			if (ch == CONTROL_CHARS[i]) return 'c';
		}
		if (ch == '\\') {
			if ((ch = fgetc(fptr)) == EOF) return EOF;
			printf("%c", ch);
			if (ch == SOLIDUS_CHARS[8]) {
				for (int i = 0; i < 4; i++) {
					if ((ch = fgetc(fptr)) == EOF) return EOF;
					printf("%c", ch);
					if ((ch >= 48 && ch <= 57) || (ch >= 65 && ch <= 70) || (ch >= 97 && ch <= 102)) continue;
					else {
						printf("Error: bad string '%c'\n", ch);
						return ch;
					}
				}
			}
			for (int i = 0; i < SC_SIZE - 1; ++i) {
				if (ch == SOLIDUS_CHARS[i])
					goto PARSE_NEXT;
			}
		}
	}
	return EOF;
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
	static const char true[] = "true";
	static const char null[] = "null";
	static const char false[] = "false";
	char ch = parse_whitespace(fptr, position, line);
	switch (ch) {
		case '"':
			printf("string = \"");
			returnCode = parse_string(fptr, position, line);
			if (returnCode) return returnCode;
			return parse_whitespace(fptr, position, line);
		case '{':
			printf("object={");
			PARSE_OBJECT:
			returnCode = parse_whitespace(fptr, position, line);
			if (returnCode == '}') return parse_whitespace(fptr, position, line);
			if (returnCode != '"') return returnCode;
			printf("\"");
			returnCode = parse_string(fptr, position, line);
			if (returnCode) return returnCode;
			returnCode = parse_whitespace(fptr, position, line);
			if (returnCode != ':') {
				printf("Error: expected ':'\n");
				return returnCode;
			}
			printf(":");
			returnCode = parse_value(fptr, position, line);
			if (returnCode == ',') {
				printf(",");
				goto PARSE_OBJECT;
			}
			if (returnCode != '}') return returnCode;
			printf("}");
			return parse_whitespace(fptr, position, line);
		case '[':
			printf("[");
			returnCode = parse_whitespace(fptr, position, line);
			if (returnCode == ']') return parse_whitespace(fptr, position, line);
			fseek(fptr, -1, SEEK_CUR);

			returnCode = parse_value(fptr, position, line);
			if (returnCode == ']') return returnCode;
			while (returnCode != ']' && returnCode == ',') {
				printf(",");
				returnCode = parse_value(fptr, position, line);
			}
			printf("%c",returnCode); // the ']' character
			if (returnCode != EOF)
				return parse_whitespace(fptr, position, line);

			printf("Error: Unexpected EOF\n");
			return returnCode;
		case 't':
			for (int i = 1; i < 4; ++i) {
				ch = fgetc(fptr);
				if (ch == true[i]) continue;
				else {
					printf("Error: Inspected '%c' instead of '%c'\n", true[i], ch);
					return ch;
				}
			}
			printf("true");
			return parse_whitespace(fptr, position, line);
		case 'f':
			for (int i = 1; i < 5; ++i) {
				ch = fgetc(fptr);
				if (ch == false[i]) continue;
				else {
					printf("Error: Inspected '%c' instead of '%c'\n", false[i], ch);
					return ch;
				}
			}
			printf("false");
			return parse_whitespace(fptr, position, line);
		case 'n':
			for (int i = 1; i < 4; ++i) {
				ch = fgetc(fptr);
				if (ch == null[i]) continue;
				else {
					printf("Error: Inspected '%c' instead of '%c'\n", null[i], ch);
					return ch;
				}
			}
			printf("null");
			return parse_whitespace(fptr, position, line);
		default:
			printf("Error: Unexpected '%c'\n", ch);
			return ch;
	}
}
int parse_json(char *str) {
	FILE *fptr;
	char ch;

	fptr = fopen(str,"r");
	if (fptr) {
		printf("File opened.\n\n");

		int position = 0, line = 1, returnCode;
		returnCode = parse_value(fptr, &position, &line);
		if (returnCode != EOF) {
			//printf("Error: Expecting EOF, got '%c'\n", returnCode);
		}

		//parse_object(fptr, &position, &line);
		fclose(fptr);
		printf("\n\nFile closed.\n");
		return 0;
	}
	return 1;
}
int main(int argc, char *argv[]) {
	if (argc == 1) {
		printf("Error: there is no file to parse.\n");
		return 1;
	}
	printf("Operating with %s\n", argv[1]);

	if (parse_json(argv[1])) {
		printf("Error: file cannot be accessed.\n");
		return 1;
	}
	return 0;
}
