.SUFFIXES: .c .o

COMPILE = $(CC) -Wall -O2 -D_FILE_OFFSET_BITS=64 -DOS_LINUX
#COMPILE = $(CC) -Wall -g -D_FILE_OFFSET_BITS=64 -DOS_LINUX -D__DEBUG__
INC_PATH = -I/usr/local/include
LIB_PATH = -L/usr/local/lib
TARGET_PATH = /usr/local/bin

COMMON_LIB =
SHARED_OBJS = hash.o chain.o shared_func.o ini_file_reader.o \
              logger.o sockopt.o fdfs_global.o base64.o sched_thread.o \
              mime_file_parser.o fdfs_http_shared.o

ALL_OBJS = $(SHARED_OBJS)

ALL_PRGS = 

all: $(ALL_OBJS) $(ALL_PRGS)
.o:
	$(COMPILE) -o $@ $<  $(SHARED_OBJS) $(COMMON_LIB) $(LIB_PATH) $(INC_PATH)
.c:
	$(COMPILE) -o $@ $<  $(ALL_OBJS) $(COMMON_LIB) $(LIB_PATH) $(INC_PATH)
.c.o:
	$(COMPILE) -c -o $@ $<  $(INC_PATH)
install:
	cp -f $(ALL_PRGS) $(TARGET_PATH)
clean:
	rm -f $(ALL_OBJS) $(ALL_PRGS)
