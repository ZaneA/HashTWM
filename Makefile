CFLAGS=-c -Wall
#LDFLAGS=-s -Wl,--subsystem,windows
LDFLAGS=-s
EXECUTABLE=hashtwm
SOURCE_DIR=src
SOURCES=$(SOURCE_DIR)/main.c $(SOURCE_DIR)/dummy.c
OBJECTS=$(SOURCES:.c=.o)

windows: $(EXECUTABLE)
linux: $(EXECUTABLE)
osx: $(EXECUTABLE)
dummy: $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@

.c.o:
	$(CC) $(CFLAGS) $< -o $@

clean:
	-rm $(OBJECTS) $(EXECUTABLE)

.PHONY: windows linux osx dummy clean
