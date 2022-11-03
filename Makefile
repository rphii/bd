#### Start of system configuration section. ####
VERSION := 1.00
CC      := gcc
CFLAGS  := -Wall -c
LDFLAGS := 
CSUFFIX	:= .c
HSUFFIX	:= .h
#### End of system configuration section. ####

APPNAME = a

#### Start of OS detection ####
UNAME := $(shell uname 2>nul || echo Windows)
ifeq ($(UNAME),Windows)
    detected_OS := Windows
else
# $(shell rm nul)
    detected_OS := Unix
endif
#### End of OS detection ####

# Path for important files 
# .c and .h files
SRC_DIR = src
# .o files
OBJ_DIR = obj

.PHONY: all clean


# Files to compile
TARGET  := $(APPNAME)
C_FILES := $(wildcard $(SRC_DIR)/*$(CSUFFIX))
O_FILES := $(addprefix $(OBJ_DIR)/,$(notdir $(C_FILES:$(CSUFFIX)=.o)))
H_FILES := $(wildcard $(SRC_DIR)/*$(HSUFFIX))
D_FILES := $(addprefix $(OBJ_DIR)/,$(notdir $(H_FILES:$(HSUFFIX)=.d)))

all: $(TARGET)

# link all .o files
$(TARGET): $(O_FILES)
	$(CC) $(LDFLAGS) -o $@ $^

# depend include files
-include $(D_FILES)

# compile all .c Files 
$(OBJ_DIR)/%.o: $(SRC_DIR)/%$(CSUFFIX) Makefile | $(OBJ_DIR)
	$(CC) $(CFLAGS) -MMD -MP -o $@ $<

# create directories if they don't exist
# .o dir
$(OBJ_DIR):
	@mkdir $@

#### Start of cleaning ####
ifeq ($(detected_OS),Windows)
# Cleaning rules for Windows OS
clean:
	@del /q $(OBJ_DIR) $(TARGET).exe
	@rmdir $(OBJ_DIR)
else
# Cleaning rules for Unix-based OS
clean:
	@rm -rf $(OBJ_DIR) $(TARGET)
endif
#### End of cleaning ####
