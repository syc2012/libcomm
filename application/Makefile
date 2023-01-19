include ../build.include

SRC_DIR = $(BUILD_DIR)/application
INC_DIR = $(BUILD_DIR)/include
LIB_DIR = $(BUILD_DIR)/lib

DEBUG = 0


############
#  Source
############

SRC += $(SRC_DIR)/nptp_ctrl.c
SRC += $(SRC_DIR)/nptp_serv.c
SRC += $(SRC_DIR)/nptp_event.c
SRC += $(SRC_DIR)/nptp_xml.c
SRC += $(SRC_DIR)/nptp_proxy.c

INC += -I$(INC_DIR)


############
#  CFLAGS
############

CFLAGS += -Wall
CFLAGS += -I$(INC_DIR)
CFLAGS += -I/usr/include/libxml2
ifeq ($(DEBUG), 1)
CFLAGS += -rdynamic
CFLAGS += -g
endif


############
#  LDFLAGS
############

LDFLAGS += -L$(LIB_DIR)
LDFLAGS += -lcomm
LDFLAGS += -lpthread
LDFLAGS += -lxml2


############
#  Build
############

DEP_CFLAGS = $(filter-out -g, $(CFLAGS))
DEPEND_CMD = $(CC) $(DEP_CFLAGS) -M $<

OBJS = $(SRC:.c=.o)
DEPS = $(SRC:.c=.c.d)


NPTP_CTRL = nptp_ctrl
NPTP_SERV = nptp_serv

all: $(NPTP_CTRL) $(NPTP_SERV)
ifeq ($(DEBUG), 0)
	@$(STRIP) $^
endif

-include $(DEPS)

$(NPTP_CTRL): nptp_ctrl.o
	$(CC) $^ $(LDFLAGS) -o $@

$(NPTP_SERV): nptp_serv.o nptp_event.o nptp_xml.o nptp_proxy.o
	$(CC) $^ $(LDFLAGS) -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $<

ifeq ($(findstring $(MAKECMDGOALS), clean), )
%.c.d: %.c
	@echo "$@ \\" > $@
	@echo -n "$(dir $<)" >> $@
	@$(DEPEND_CMD) >> $@ || rm -f $@
endif

debug:
ifeq ($(DEBUG), 1)
	@$(OBJDUMP) -t $(NPTP_SERV) | grep "\.text" | sort > $(NPTP_SERV).sym
	@$(OBJDUMP) -S $(NPTP_SERV) > $(NPTP_SERV).asm
endif

clean:
ifeq ($(DEBUG), 1)
	@rm -f $(NPTP_SERV).sym $(NPTP_SERV).asm
endif
	@rm -f $(NPTP_CTRL) $(NPTP_SERV)
	@rm -f $(OBJS) $(DEPS)

.PHONY: all clean
