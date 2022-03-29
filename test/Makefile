include ../build.include

SRC_DIR = $(BUILD_DIR)/source
INC_DIR = $(BUILD_DIR)/include
LIB_DIR = $(BUILD_DIR)/lib


############
#  CFLAGS
############

CFLAGS += -Wall
CFLAGS += -I$(INC_DIR)
ifeq ($(QUIET), 1)
CFLAGS += -DQUIET
endif


############
#  LDFLAGS
############

LDFLAGS += -L$(LIB_DIR)
LDFLAGS += -lpthread
LDFLAGS += -lcomm


############
#  Build
############

APPS += ipc_recv ipc_send
APPS += netlink_recv netlink_send
APPS += udp_recv udp_send
APPS += tcp_recv tcp_send
APPS += fifo_recv fifo_send
APPS += uart_recv uart_send

all: $(APPS)
	@$(STRIP) $^

ipc_recv: ipc_recv.o
	$(CC) $(LDFLAGS) -o $@ $<

ipc_send: ipc_send.o
	$(CC) $(LDFLAGS) -o $@ $<

netlink_recv: netlink_recv.o
	$(CC) $(LDFLAGS) -o $@ $<

netlink_send: netlink_send.o
	$(CC) $(LDFLAGS) -o $@ $<

udp_recv: udp_recv.o
	$(CC) $(LDFLAGS) -o $@ $<

udp_send: udp_send.o
	$(CC) $(LDFLAGS) -o $@ $<

tcp_recv: tcp_recv.o
	$(CC) $(LDFLAGS) -o $@ $<

tcp_send: tcp_send.o
	$(CC) $(LDFLAGS) -o $@ $<

fifo_recv: fifo_recv.o
	$(CC) $(LDFLAGS) -o $@ $<

fifo_send: fifo_send.o
	$(CC) $(LDFLAGS) -o $@ $<

uart_recv: uart_recv.o
	$(CC) $(LDFLAGS) -o $@ $<

uart_send: uart_send.o
	$(CC) $(LDFLAGS) -o $@ $<

%.o: %.c $(INC_DIR)/comm_if.h
	$(CC) $(CFLAGS) -c $<

clean:
	@rm -f $(APPS)
	@rm -f *.o

.PHONY: all clean
