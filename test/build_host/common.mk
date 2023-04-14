

QUIET =
MAPFILE  = $(ROOTDIR)/build/map_$(CODEC_NAME).txt
LDSCRIPT = $(ROOTDIR)/build/ldscript_$(CODEC_NAME).txt
SYMFILE  = $(ROOTDIR)/build/symbols_$(CODEC_NAME).txt

ifeq ($(XA_RTOS), linux)
CPU = gcc
endif

ifeq ($(CPU), gcc)
    S = /
    AR = ar
    OBJCOPY = objcopy
    CC = gcc
    CFLAGS += -DCSTUB=1
    CFLAGS += -D_GNU_SOURCE
    CFLAGS += -ffloat-store 
    CFLAGS += -D__CSTUB_HIFI2__ -DHIFI2_CSTUB
	RM = rm -f
    RM_R = rm -rf
    MKPATH = mkdir -p
    CP = cp -f
    INCLUDES += \
    -I$(ROOTDIR)/test/include

    CFLAGS += -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast
    CFLAGS += -Wno-stringop-overflow

    #for hosted
    #CFLAGS += -fno-pic
    CFLAGS += -no-pie
    LDFLAGS+= -no-pie
else
    AR = xt-ar 
    OBJCOPY = xt-objcopy
ifeq ($(XAF_HOSTED_DSP),1)
    SUBSYSTEM ?= $(ROOTDIR)/xtsc
    CC = xt-clang --xtensa-core=$(XTCORE) --xtensa-system=$(XTENSA_SYSTEM)
    INCLUDES += -I$(SUBSYSTEM)/sysbuilder/include
    INCLUDES += -I$(SUBSYSTEM)/mbuild/package/xtensa-elf/include
else
ifneq ($(XF_CFG_CORES_NUM),1)
    SUBSYSTEM ?= $(ROOTDIR)/xtsc
    CC = xt-clang --xtensa-core=$(XTCORE) --xtensa-system=$(XTENSA_SYSTEM)
    INCLUDES += -I$(SUBSYSTEM)/sysbuilder/include
    INCLUDES += -I$(SUBSYSTEM)/mbuild/package/xtensa-elf/include
else
    CC = xt-clang
endif
endif
    ISS = xt-run $(XTCORE)
    CONFIGDIR := $(shell $(ISS) --show-config=config)
    include $(CONFIGDIR)/misc/hostenv.mk
    CFLAGS += -Wall 
    CFLAGS += -Werror 
    CFLAGS += -mno-mul16 -mno-mul32 -mno-div32 -fsigned-char -mlongcalls -INLINE:requested -ffunction-sections 
    ISR_SAFE_CFLAGS = -mcoproc
endif

#CFLAGS += -Wno-unused -DXF_TRACE=1
CFLAGS += -DHIFI_ONLY_XAF

OBJDIR = objs$(S)$(CODEC_NAME)
LIBDIR = $(ROOTDIR)$(S)lib

OBJ_LIBO2OBJS  = $(addprefix $(OBJDIR)/,$(LIBO2OBJS))
OBJ_LIBOSOBJS  = $(addprefix $(OBJDIR)/,$(LIBOSOBJS))
OBJ_LIBISROBJS = $(addprefix $(OBJDIR)/,$(LIBISROBJS))
OBJS_HOSTOBJS = $(addprefix $(OBJDIR)/,$(HOSTOBJS))

OBJS_LIST = $(OBJS_HOSTOBJS)

TEMPOBJ = temp.o    

ifeq ($(CPU), gcc)
    LIBOBJ   = $(OBJDIR)/xgcc_$(CODEC_NAME).o
    LIB      = xgcc_$(CODEC_NAME).a
else
    LIBOBJ   = $(OBJDIR)/xa_$(CODEC_NAME)_$(XTCORE).o
    LIB      = xa_$(CODEC_NAME)_$(XTCORE).a
endif

CFLAGS += $(EXTRA_CFLAGS)

ifeq ($(DEBUG),1)
  CFLAGS += -DXF_DEBUG=1
  NOSTRIP = 1
  OPT_O2 = -O0 -g 
  OPT_OS = -O0 -g
else
  OPT_O2 = -O2 
ifeq ($(CPU), gcc)
  OPT_OS = -O2 
else
  OPT_OS = -Os 
endif
endif


all: $(OBJDIR) $(LIB) 
$(CODEC_NAME): $(OBJDIR) $(LIB) 

install: $(OBJDIR) $(LIB)
	@echo "Installing $(LIB)"
	$(QUIET) -$(MKPATH) "$(LIBDIR)"
	$(QUIET) $(CP) $(LIB) "$(LIBDIR)"

$(OBJDIR):
	$(QUIET) -$(MKPATH) $@

ifeq ($(NOSTRIP), 1)
$(LIBOBJ): $(OBJ_LIBO2OBJS) $(OBJ_LIBOSOBJS) $(OBJ_LIBISROBJS) $(OBJS_LIST)
	@echo "Linking Objects"
	$(QUIET) $(CC) -o $@ $^ \
	-Wl,-r,-Map,$(MAPFILE) --no-standard-libraries \
	-Wl,--script,$(LDSCRIPT) \
	$(LDFLAGS)
else
$(LIBOBJ): $(OBJ_LIBO2OBJS) $(OBJ_LIBOSOBJS) $(OBJ_LIBISROBJS) $(OBJS_LIST)
	@echo "Linking Objects"
	$(QUIET) $(CC) -o $@ $^ \
	-Wl,-r,-Map,$(MAPFILE) --no-standard-libraries \
	-Wl,--retain-symbols-file,$(SYMFILE) \
	-Wl,--script,$(LDSCRIPT) \
	$(LDFLAGS)
	$(QUIET) $(OBJCOPY) --keep-global-symbols=$(SYMFILE) $@ $(TEMPOBJ)
	$(QUIET) $(OBJCOPY) --strip-unneeded $(TEMPOBJ) $@
	$(QUIET) -$(RM) $(TEMPOBJ)
endif 


$(OBJ_PLUGINOBJS) $(OBJ_LIBO2OBJS): $(OBJDIR)/%.o: %.c
	@echo "Compiling $<"
	$(QUIET) $(CC) -o $@ $(OPT_O2) $(CFLAGS) $(ISR_SAFE_CFLAGS) $(INCLUDES) -c $<

$(OBJ_LIBISROBJS): $(OBJDIR)/%.o: %.c
	@echo "Compiling $<"
	$(QUIET) $(CC) -o $@ $(OPT_O2) $(CFLAGS) $(INCLUDES) -c $<
	
$(OBJ_LIBOSOBJS): $(OBJDIR)/%.o: %.c
	@echo "Compiling $<"
	$(QUIET) $(CC) -o $@ $(OPT_OS) $(CFLAGS) $(ISR_SAFE_CFLAGS) $(INCLUDES) -c $<
	
$(OBJS_HOSTOBJS): $(OBJDIR)/%.o: %.c
	@echo "Compiling $<"
	$(QUIET) $(CC) -o $@ $(OPT_O2) $(CFLAGS) $(ISR_SAFE_CFLAGS) $(INCLUDES) -c $<

$(LIB): %.a: $(OBJDIR)/%.o
	@echo "Creating Library $@"
	$(QUIET) $(AR) rc $@ $^

clean:
	-$(RM) $(LIBDIR)$(S)xa_$(CODEC_NAME)_$(XTCORE).a
	-$(RM) $(LIBDIR)$(S)xgcc_$(CODEC_NAME).a
	-$(RM_R) $(OBJDIR) $(MAPFILE)
