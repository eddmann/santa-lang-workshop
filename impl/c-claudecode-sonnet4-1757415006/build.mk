CC = gcc
CFLAGS = -std=c23 -Wall -Wextra -O2
TARGET = elf-lang
SOURCES = main.c lexer.c parser.c evaluator.c

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCES) -lm

clean:
	rm -f $(TARGET)

.PHONY: clean