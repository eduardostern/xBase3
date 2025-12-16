# xBase3 Makefile

CC = clang
CFLAGS = -std=c11 -Wall -Wextra -pedantic -g -O0
LDFLAGS = -lm

# Check for readline
READLINE_CHECK := $(shell echo '\#include <readline/readline.h>' | $(CC) -E - 2>/dev/null && echo yes)
ifeq ($(READLINE_CHECK),yes)
    CFLAGS += -DHAVE_READLINE
    LDFLAGS += -lreadline
endif

# Directories
SRCDIR = src
BUILDDIR = build
TESTDIR = tests

# Source files
SOURCES = $(SRCDIR)/util.c \
          $(SRCDIR)/dbf.c \
          $(SRCDIR)/lexer.c \
          $(SRCDIR)/ast.c \
          $(SRCDIR)/parser.c \
          $(SRCDIR)/expr.c \
          $(SRCDIR)/functions.c \
          $(SRCDIR)/variables.c \
          $(SRCDIR)/commands.c

MAIN_SRC = $(SRCDIR)/main.c

# Object files
OBJECTS = $(SOURCES:$(SRCDIR)/%.c=$(BUILDDIR)/%.o)
MAIN_OBJ = $(BUILDDIR)/main.o

# Target executable
TARGET = $(BUILDDIR)/xbase3

# Test executables
TEST_DBF = $(BUILDDIR)/test_dbf
TEST_LEXER = $(BUILDDIR)/test_lexer
TEST_PARSER = $(BUILDDIR)/test_parser
TEST_EXPR = $(BUILDDIR)/test_expr

.PHONY: all clean test

all: $(BUILDDIR) $(TARGET)

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

$(BUILDDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) -I$(SRCDIR) -c $< -o $@

$(TARGET): $(OBJECTS) $(MAIN_OBJ)
	$(CC) $(OBJECTS) $(MAIN_OBJ) -o $@ $(LDFLAGS)

# Tests
test: $(TEST_DBF) $(TEST_LEXER) $(TEST_PARSER) $(TEST_EXPR)
	@echo "Running tests..."
	@$(TEST_DBF) && $(TEST_LEXER) && $(TEST_PARSER) && $(TEST_EXPR)
	@echo "All tests passed!"

$(TEST_DBF): $(TESTDIR)/test_dbf.c $(OBJECTS)
	$(CC) $(CFLAGS) -I$(SRCDIR) $(TESTDIR)/test_dbf.c $(OBJECTS) -o $@ $(LDFLAGS)

$(TEST_LEXER): $(TESTDIR)/test_lexer.c $(OBJECTS)
	$(CC) $(CFLAGS) -I$(SRCDIR) $(TESTDIR)/test_lexer.c $(OBJECTS) -o $@ $(LDFLAGS)

$(TEST_PARSER): $(TESTDIR)/test_parser.c $(OBJECTS)
	$(CC) $(CFLAGS) -I$(SRCDIR) $(TESTDIR)/test_parser.c $(OBJECTS) -o $@ $(LDFLAGS)

$(TEST_EXPR): $(TESTDIR)/test_expr.c $(OBJECTS)
	$(CC) $(CFLAGS) -I$(SRCDIR) $(TESTDIR)/test_expr.c $(OBJECTS) -o $@ $(LDFLAGS)

clean:
	rm -rf $(BUILDDIR)

# Dependencies
$(BUILDDIR)/util.o: $(SRCDIR)/util.h
$(BUILDDIR)/dbf.o: $(SRCDIR)/dbf.h $(SRCDIR)/util.h
$(BUILDDIR)/lexer.o: $(SRCDIR)/lexer.h $(SRCDIR)/util.h
$(BUILDDIR)/ast.o: $(SRCDIR)/ast.h $(SRCDIR)/lexer.h $(SRCDIR)/util.h
$(BUILDDIR)/parser.o: $(SRCDIR)/parser.h $(SRCDIR)/lexer.h $(SRCDIR)/ast.h
$(BUILDDIR)/expr.o: $(SRCDIR)/expr.h $(SRCDIR)/ast.h $(SRCDIR)/dbf.h
$(BUILDDIR)/functions.o: $(SRCDIR)/functions.h $(SRCDIR)/expr.h
$(BUILDDIR)/variables.o: $(SRCDIR)/variables.h $(SRCDIR)/expr.h
$(BUILDDIR)/commands.o: $(SRCDIR)/commands.h $(SRCDIR)/ast.h $(SRCDIR)/expr.h $(SRCDIR)/dbf.h
$(BUILDDIR)/main.o: $(SRCDIR)/util.h $(SRCDIR)/lexer.h $(SRCDIR)/parser.h $(SRCDIR)/ast.h $(SRCDIR)/expr.h $(SRCDIR)/commands.h $(SRCDIR)/dbf.h
