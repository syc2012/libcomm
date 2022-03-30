include ../build.include

SRC_DIR = $(BUILD_DIR)/source
INC_DIR = $(BUILD_DIR)/include
LIB_DIR = $(BUILD_DIR)/lib


############
#  Source
############

SRC += $(SRC_DIR)/comm_log.c
SRC += $(SRC_DIR)/comm_udp.c
SRC += $(SRC_DIR)/comm_tcp_client.c
SRC += $(SRC_DIR)/comm_tcp_server.c
SRC += $(SRC_DIR)/comm_raw.c
SRC += $(SRC_DIR)/comm_fifo.c
SRC += $(SRC_DIR)/comm_ipc_dgram.c
SRC += $(SRC_DIR)/comm_ipc_stream.c
SRC += $(SRC_DIR)/comm_netlink.c
SRC += $(SRC_DIR)/comm_uart.c

INC += -I$(INC_DIR)

OBJ = $(SRC:.c=.o)


############
#  CFLAGS
############

CFLAGS += -fPIC
CFLAGS += -Wall
CFLAGS += -O2
CFLAGS += $(INC)


############
#  LDFLAGS
############

LDFLAGS += -shared
LDFLAGS += -Wl,-soname,libcomm.so.1


############
#  Build
############

all: $(LIB_DIR)/libcomm.so.1.0.2 $(LIB_DIR)/libcomm.a
	@$(STRIP) $^

$(LIB_DIR)/libcomm.so.1.0.2: $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $(OBJ)
	@cd $(LIB_DIR) && ln -sf libcomm.so.1.0.2 libcomm.so.1
	@cd $(LIB_DIR) && ln -sf libcomm.so.1     libcomm.so

$(LIB_DIR)/libcomm.a: $(OBJ)
	$(AR) rcs $@ $^

%.o: %.c $(INC_DIR)/comm_if.h $(SRC_DIR)/comm_log.h
	$(CC) $(CFLAGS) -o $@ -c $<

clean:
	@rm -f $(LIB_DIR)/libcomm.so $(LIB_DIR)/libcomm.so.1 $(LIB_DIR)/libcomm.so.1.0.2
	@rm -f $(LIB_DIR)/libcomm.a
	@rm -f $(OBJ)

.PHONY: all clean
