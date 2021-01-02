// Copyright Mikhail ysph Subbotin
#include <stdio.h>

int parse_whitespace(FILE *fptr, int *position, int *line) {
	char ch;
	while ((ch = fgetc(fptr)) != EOF) {
		switch (ch) {
			case '\n':
				*line += 1;
				*position = 0;
			case ' ':
			case '\r':
			case '\t':
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
			puts("parsing string");
			return parse_whitespace(fptr, position, line);
		case '{':
			puts("parsing object");
			return parse_whitespace(fptr, position, line);
		case '[':
			returnCode = parse_whitespace(fptr, position, line);
			if (returnCode == ']')
				return parse_whitespace(fptr, position, line);
			fseek(fptr, -1, SEEK_CUR);

			returnCode = parse_value(fptr, position, line);
			if (returnCode == ']') return returnCode;
			while (returnCode != ']' && returnCode == ',') {
				returnCode = parse_value(fptr, position, line);
			}
			if (returnCode != EOF)
				return parse_whitespace(fptr, position, line);

			printf("Unexpected EOF\n");
			return returnCode;
		case 't':
			for (int i = 1; i < 4; ++i) {
				ch = fgetc(fptr);
				if (ch == true[i]) continue;
				else {
					printf("Inspected '%c' instead of '%c'\n", true[i], ch);
					return ch;
				}
			}
			puts("value = true");
			return parse_whitespace(fptr, position, line);
		case 'f':
			for (int i = 1; i < 5; ++i) {
				ch = fgetc(fptr);
				if (ch == false[i]) continue;
				else {
					printf("Inspected '%c' instead of '%c'\n", false[i], ch);
					return ch;
				}
			}
			puts("value = false");
			return parse_whitespace(fptr, position, line);
		case 'n':
			for (int i = 1; i < 4; ++i) {
				ch = fgetc(fptr);
				if (ch == null[i]) continue;
				else {
					printf("Inspected '%c' instead of '%c'\n", null[i], ch);
					return ch;
				}
			}
			puts("value = null");
			return parse_whitespace(fptr, position, line);
		default:
			printf("Unexpected '%c'\n", ch);
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
		if (returnCode != EOF) printf("Error: Expecting EOF, got '%c'\n", returnCode);

		//parse_object(fptr, &position, &line);
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
	printf("Operating with %s\n", argv[1]);

	if (parse_json(argv[1])) {
		printf("Error: file cannot be accessed.\n");
		return 1;
	}
	return 0;
}
