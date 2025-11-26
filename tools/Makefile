.SUFFIXES: .c .o

COMPILE = $(CC) $(CFLAGS)
INC_PATH = -I../common -I../client -I/usr/include/fastcommon
LIB_PATH = -L/usr/lib64 -L/usr/local/lib $(LIBS)
TARGET_LIB = $(LIB_PATH) -lfastcommon -lserverframe -lfdfsclient -lpthread -lm

FAST_SHARED_OBJS = ../common/fdfs_global.o \
                   ../tracker/tracker_proto.o \
                   ../tracker/fdfs_shared_func.o \
                   ../storage/trunk_mgr/trunk_shared.o

ALL_OBJS = $(FAST_SHARED_OBJS)

ALL_PRGS = fdfs_file_verify fdfs_file_migrate fdfs_batch_delete \
           fdfs_storage_stat fdfs_health_check fdfs_backup fdfs_restore \
           fdfs_dedup fdfs_analyze fdfs_repair fdfs_recover fdfs_benchmark \
           fdfs_sync_check fdfs_quota fdfs_cleanup fdfs_metadata_bulk \
           fdfs_replication_status fdfs_search fdfs_cluster_mgr fdfs_replication \
           fdfs_load_balancer fdfs_capacity_planner

all: $(ALL_OBJS) $(ALL_PRGS)

.o:
	$(COMPILE) -o $@ $<  $(TARGET_LIB)
.c:
	$(COMPILE) -o $@ $<  $(ALL_OBJS) $(TARGET_LIB)
.c.o:
	$(COMPILE) -c -o $@ $<  $(INC_PATH)

fdfs_file_verify: fdfs_file_verify.c $(ALL_OBJS)
	$(COMPILE) -o $@ $<  $(ALL_OBJS) $(TARGET_LIB)

fdfs_file_migrate: fdfs_file_migrate.c $(ALL_OBJS)
	$(COMPILE) -o $@ $<  $(ALL_OBJS) $(TARGET_LIB)

fdfs_batch_delete: fdfs_batch_delete.c $(ALL_OBJS)
	$(COMPILE) -o $@ $<  $(ALL_OBJS) $(TARGET_LIB)

fdfs_storage_stat: fdfs_storage_stat.c $(ALL_OBJS)
	$(COMPILE) -o $@ $<  $(ALL_OBJS) $(TARGET_LIB)

fdfs_health_check: fdfs_health_check.c $(ALL_OBJS)
	$(COMPILE) -o $@ $<  $(ALL_OBJS) $(TARGET_LIB)

fdfs_backup: fdfs_backup.c $(ALL_OBJS)
	$(COMPILE) -o $@ $<  $(ALL_OBJS) $(TARGET_LIB)

fdfs_restore: fdfs_restore.c $(ALL_OBJS)
	$(COMPILE) -o $@ $<  $(ALL_OBJS) $(TARGET_LIB)

fdfs_dedup: fdfs_dedup.c $(ALL_OBJS)
	$(COMPILE) -o $@ $<  $(ALL_OBJS) $(TARGET_LIB)

fdfs_analyze: fdfs_analyze.c $(ALL_OBJS)
	$(COMPILE) -o $@ $<  $(ALL_OBJS) $(TARGET_LIB)

fdfs_repair: fdfs_repair.c $(ALL_OBJS)
	$(COMPILE) -o $@ $<  $(ALL_OBJS) $(TARGET_LIB)

fdfs_recover: fdfs_recover.c $(ALL_OBJS)
	$(COMPILE) -o $@ $<  $(ALL_OBJS) $(TARGET_LIB)

fdfs_benchmark: fdfs_benchmark.c $(ALL_OBJS)
	$(COMPILE) -o $@ $<  $(ALL_OBJS) $(TARGET_LIB)

fdfs_sync_check: fdfs_sync_check.c $(ALL_OBJS)
	$(COMPILE) -o $@ $<  $(ALL_OBJS) $(TARGET_LIB)
  
fdfs_quota: fdfs_quota.c $(ALL_OBJS)
	$(COMPILE) -o $@ $<  $(ALL_OBJS) $(TARGET_LIB)

fdfs_cleanup: fdfs_cleanup.c $(ALL_OBJS)
	$(COMPILE) -o $@ $<  $(ALL_OBJS) $(TARGET_LIB)

fdfs_replication: fdfs_replication.c $(ALL_OBJS)
	$(COMPILE) -o $@ $<  $(ALL_OBJS) $(TARGET_LIB)
  
fdfs_metadata_bulk: fdfs_metadata_bulk.c $(ALL_OBJS)
	$(COMPILE) -o $@ $<  $(ALL_OBJS) $(TARGET_LIB)

fdfs_replication_status: fdfs_replication_status.c $(ALL_OBJS)
	$(COMPILE) -o $@ $<  $(ALL_OBJS) $(TARGET_LIB)

fdfs_search: fdfs_search.c $(ALL_OBJS)
	$(COMPILE) -o $@ $<  $(ALL_OBJS) $(TARGET_LIB)

fdfs_cluster_mgr: fdfs_cluster_mgr.c $(ALL_OBJS)
	$(COMPILE) -o $@ $<  $(ALL_OBJS) $(TARGET_LIB)

fdfs_load_balancer: fdfs_load_balancer.c $(ALL_OBJS)
	$(COMPILE) -o $@ $<  $(ALL_OBJS) $(TARGET_LIB)

fdfs_capacity_planner: fdfs_capacity_planner.c
	$(CC) $(CFLAGS) -o $@ $< -lm

install:
	mkdir -p $(DESTDIR)/usr/bin
	cp -f $(ALL_PRGS) $(DESTDIR)/usr/bin

clean:
	rm -f $(ALL_OBJS) $(ALL_PRGS)
