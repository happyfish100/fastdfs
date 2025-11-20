.SUFFIXES: .c .o .lo

COMPILE = $(CC) -g -Wall -O -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64 -DDEBUG
INC_PATH = -I/usr/local/include
LIB_PATH = -L/usr/local/lib -lfdfsclient -lfastcommon -lserverframe
TARGET_PATH = $(TARGET_PREFIX)/bin

SHARED_OBJS = common_func.o dfs_func.o
#SHARED_OBJS = common_func.o dfs_func_pc.o

ALL_OBJS = $(SHARED_OBJS)

#ALL_PRGS = gen_files test_upload test_download test_delete test_append test_modify \
#           test_truncate test_slave test_fileinfo test_metadata combine_result

ALL_PRGS = gen_files test_upload test_download test_delete test_append \
           test_metadata test_concurrent test_range_download combine_result

all: $(ALL_OBJS) $(ALL_PRGS)
.o:
	$(COMPILE) -o $@ $<  $(SHARED_OBJS) $(LIB_PATH) $(INC_PATH)
.c:
	$(COMPILE) -o $@ $<  $(SHARED_OBJS) $(LIB_PATH) $(INC_PATH)
.c.o:
	$(COMPILE) -c -o $@ $<  $(INC_PATH)
.c.lo:
	$(COMPILE) -c -fPIC -o $@ $<  $(INC_PATH)
install:
	mkdir -p $(TARGET_PATH)
	cp -f $(ALL_PRGS) $(TARGET_PATH)
clean:
	rm -f $(ALL_OBJS) $(ALL_PRGS)
