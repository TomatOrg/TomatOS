########################################################################################################################
# uACPI
########################################################################################################################

#-----------------------------------------------------------------------------------------------------------------------
# Build Configuration
#-----------------------------------------------------------------------------------------------------------------------

# Nuke built-in rules and variables.
MAKEFLAGS += -rR
.SUFFIXES:

# Use clang by default
CC				:= clang
AR				:= llvm-ar
LD				:= ld.lld

# The cflags you want to compile with
CFLAGS			?=

#-----------------------------------------------------------------------------------------------------------------------
# Build constants
#-----------------------------------------------------------------------------------------------------------------------

# The output directories
OUT_DIR			:= build
BIN_DIR 		:= $(OUT_DIR)/bin
BUILD_DIR		:= $(OUT_DIR)/obj

# Add some flags that we require to work
UACPI_CFLAGS	:= $(CFLAGS)
UACPI_CFLAGS	+= -Ilib/uACPI/include

# Get the sources along side all of the objects and dependencies
SRCS 		:= lib/uACPI/source/tables.c
SRCS 		+= lib/uACPI/source/types.c
SRCS 		+= lib/uACPI/source/uacpi.c
SRCS 		+= lib/uACPI/source/utilities.c
SRCS 		+= lib/uACPI/source/interpreter.c
SRCS 		+= lib/uACPI/source/opcodes.c
SRCS 		+= lib/uACPI/source/namespace.c
SRCS 		+= lib/uACPI/source/stdlib.c
SRCS 		+= lib/uACPI/source/shareable.c
SRCS 		+= lib/uACPI/source/opregion.c
SRCS 		+= lib/uACPI/source/default_handlers.c
SRCS 		+= lib/uACPI/source/io.c
SRCS 		+= lib/uACPI/source/notify.c
SRCS 		+= lib/uACPI/source/sleep.c
SRCS 		+= lib/uACPI/source/registers.c
SRCS 		+= lib/uACPI/source/resources.c
SRCS 		+= lib/uACPI/source/event.c
SRCS 		+= lib/uACPI/source/mutex.c
SRCS 		+= lib/uACPI/source/osi.c
OBJS 		:= $(SRCS:%=$(BUILD_DIR)/%.o)
DEPS 		:= $(OBJS:%.o=%.d)

# The default rule
.PHONY: default
default: all

# All the rules
.PHONY: all
all: $(BIN_DIR)/libuacpi.a $(DLLS)

#-----------------------------------------------------------------------------------------------------------------------
# Rules
#-----------------------------------------------------------------------------------------------------------------------

-include $(DEPS)

$(BUILD_DIR)/%.c.o: %.c
	@echo CC $@
	@mkdir -p $(@D)
	@$(CC) $(UACPI_CFLAGS) -MMD -MP -c $< -o $@

$(BIN_DIR)/libuacpi.a: $(OBJS)
	@echo AR $@
	@mkdir -p $(@D)
	@$(AR) rc $@ $^

clean:
	rm -rf $(OUT_DIR)/lib/uACPI
