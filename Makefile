# Contributed by Fabio to help me not get into bad habits from the get-go.
CC=gcc
CFLAGS=-Wall -Werror -fPIC -Wformat=2 -Wshadow -Wmissing-prototypes -Wstrict-prototypes -Wdeclaration-after-statement -Wpointer-arith -Wwrite-strings -Wcast-align -Wbad-function-cast -Wmissing-format-attribute -Wformat-security -Wformat-nonliteral -Wno-long-long -Wno-strict-aliasing -Wmissing-declarations
LDFLAGS=

TARGET=diximal

TARGET_DEBUG=$(TARGET)_debug

all: $(TARGET) $(TARGET_DEBUG)

$(TARGET): $(TARGET).o
	$(CC) -o $@ $^ $(LDFLAGS)

$(TARGET_DEBUG): $(TARGET_DEBUG).o
	$(CC) -o $@ $^ $(LDFLAGS)

$(TARGET).o: $(TARGET).c
	$(CC) $(CFLAGS) -O2 -c -o $@ $<

$(TARGET_DEBUG).o: $(TARGET).c
	$(CC) $(CFLAGS) -O0 -ggdb3 -c -o $@ $<

clean:
	rm -f *.o $(TARGET) $(TARGET_DEBUG)
