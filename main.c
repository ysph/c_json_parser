// Copyright Mikhail ysph Subbotin

#include <stdio.h>

#define single_quote '\''
#define double_quote '"'

int parse_string(FILE *fptr, const char start) {
	char ch;
	while ((ch = fgetc(fptr)) != EOF) {
		switch(ch) {
			case '\\':
				printf("[reverse solidus]");
				break;
			case single_quote:
			case double_quote:
				printf("%c", ch);
				if (start != ch) {
					continue;
				}
				return 0;
			default:
				printf("%c",ch);
				continue;
		}
	}
	return 1;
}
int parse_object(FILE *fptr) {
	char ch;
	while ((ch = fgetc(fptr)) != EOF) {
		switch (ch) {
			case single_quote:
			case double_quote:
				printf("\n\t%c", ch);
				if (parse_string(fptr, ch)) {
					return 11;
				}
				break;
			case '}':
				return 0;
			default:
				printf("%c", ch);
				continue;
		}
	}
	return 1;
}

int main(int argc, char *argv[]) {
	if (argc == 1) {
		printf("Error: there is no file to parse.\n");
		return 1;
	}
	printf("Operating with %s\n", argv[1]);


	FILE *fptr;
	char ch;

	fptr = fopen(argv[1],"r");
	if (fptr) {
		printf("File opened.\n\n");

		while ((ch = fgetc(fptr)) != EOF) {
			switch (ch) {
				case '{':
					printf("object {\n\t");
					int rc = parse_object(fptr);
					if (rc == 11) {
						printf("Error: wrong object syntax.");
						goto STOP_PARSING;
					} else if (rc == 1) {
						printf("Error: wrong string syntax in object.");
						goto STOP_PARSING;
					}
					printf("\n}");
					break;
				default:
					continue;
			}
		}
		STOP_PARSING:
		fclose(fptr);
		printf("\nFile closed.\n");
	} else {
		printf("Error: file cannot be accessed.\n");
	}

	return 0;
}
