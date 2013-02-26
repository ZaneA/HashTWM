CFLAGS=-c -Wall
EXECUTABLE=hashtwm.exe
SOURCE_DIR=src
SOURCES=$(SOURCE_DIR)/main.c
OBJECTS=$(SOURCES:.c=.o)

all: $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CC) -s -Wl,--subsystem,windows $(OBJECTS) -o $@

.c.o:
	$(CC) $(CFLAGS) $< -o $@

clean:
	-rm $(OBJECTS) $(EXECUTABLE)

.PHONY: all clean
