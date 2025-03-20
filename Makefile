# Compiler settings
CC = gcc
CFLAGS = -I./intr

# Directories
SRCDIR = src
INCDIR = intr
OBJDIR = obj
EXECDIR = execs

# Source files and object files
SRCS = $(wildcard $(SRCDIR)/*.c)
OBJS = $(SRCS:$(SRCDIR)/%.c=$(OBJDIR)/%.o)

SERVER = $(EXECDIR)/server
TEST = $(EXECDIR)/test





$(TEST): test.c $(OBJS) 
	@mkdir -p $(EXECDIR)
	$(CC) $(OBJS) $(notdir $(TEST)).c -o $(TEST) $(CFLAGS) 

$(SERVER): main.c $(OBJS)
	@mkdir -p $(EXECDIR)
	$(CC) $(OBJS) $(notdir $(SERVER)).c -o $(SERVER) $(CFLAGS)

$(OBJDIR)/%.o: $(SRCDIR)/%.c
	@mkdir -p $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJDIR) $(SERVER) $(TEST)

test: $(TEST)
	@echo "Running $(TEST)"
	@$(TEST)

# Phony targets to avoid conflicts with filenames
.PHONY: all clean test
