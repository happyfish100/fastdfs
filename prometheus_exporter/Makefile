
COMPILE = $(CC) -g -Wall -O2 -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64
INC_PATH = -I/usr/local/include -I../client -I../common -I../tracker
LIB_PATH = -L/usr/local/lib -lfdfsclient -lfastcommon -lserverframe
TARGET_PATH = $(TARGET_PREFIX)/bin

ALL_PRGS = fdfs_exporter

all: $(ALL_PRGS)

fdfs_exporter: fdfs_exporter.c
	$(COMPILE) -o $@ $< $(LIB_PATH) $(INC_PATH)

install:
	mkdir -p $(TARGET_PATH)
	cp -f $(ALL_PRGS) $(TARGET_PATH)

clean:
	rm -f $(ALL_PRGS)
