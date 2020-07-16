LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

GIT_VERSION ?= " $(shell git rev-parse --short HEAD || echo unknown)"
ifneq ($(GIT_VERSION)," unknown")
	LOCAL_CFLAGS += -DGIT_VERSION=\"$(GIT_VERSION)\"
endif

LOCAL_MODULE := retro

R := ../
FR := $(LOCAL_PATH)/$(R)

use_cyclone = 0
use_fame = 0
use_musashi = 0
use_drz80 = 0
use_cz80 = 0
use_sh2drc = 0
use_svpdrc = 0

asm_memory = 0
asm_render = 0
asm_ym2612 = 0
asm_misc = 0
asm_cdmemory = 0
asm_mix = 0
asm_32xdraw = 0
asm_32xmemory = 0

ifeq ($(TARGET_ARCH),arm)
  LOCAL_ARM_MODE := arm
  ifeq ($(NEON_BUILD),1)
    LOCAL_ARM_NEON := true
  endif

#  use_cyclone = 1
#  use_drz80 = 1
  use_sh2drc = 1
#  use_svpdrc = 1

#  asm_memory = 1
#  asm_render = 1
#  asm_ym2612 = 1
#  asm_misc = 1
#  asm_cdmemory = 1
#  asm_mix = 1
#  asm_32xdraw = 1
#  asm_32xmemory = 1

# for armeabi to build...
CYCLONE_CONFIG = cyclone_config_armv4.h

$(cleantarget)::
	$(MAKE) -C $(FR)cpu/cyclone/ clean

else
  use_fame = 1
  use_cz80 = 1
endif

# PD is currently not strict aliasing safe
LOCAL_CFLAGS += -fno-strict-aliasing

# sources
SRCS_COMMON :=
DEFINES :=
ARCH := $(TARGET_ARCH)
include $(R)platform/common/common.mak

LOCAL_SRC_FILES += $(SRCS_COMMON)
LOCAL_SRC_FILES += $(R)platform/libretro/libretro.c
LOCAL_SRC_FILES += $(R)platform/common/mp3_sync.c
LOCAL_SRC_FILES += $(R)platform/common/mp3.c
LOCAL_SRC_FILES += $(R)platform/common/mp3_dummy.c

# zlib/unzip
LOCAL_SRC_FILES += $(R)zlib/gzio.c $(R)zlib/inffast.c $(R)zlib/inflate.c \
	$(R)zlib/inftrees.c $(R)zlib/trees.c $(R)zlib/deflate.c \
	$(R)zlib/crc32.c $(R)zlib/adler32.c $(R)zlib/zutil.c \
	$(R)zlib/compress.c $(R)zlib/uncompr.c

LOCAL_SRC_FILES += $(R)unzip/unzip.c

LOCAL_C_INCLUDES += $(R)

# note: don't use -O3, causes some NDKs run out of memory while compiling FAME
LOCAL_CFLAGS += -Wall -O2 -ffast-math -DNDEBUG
LOCAL_CFLAGS += $(addprefix -D,$(DEFINES))
LOCAL_LDLIBS := -llog

ifneq ($(filter armeabi%, $(TARGET_ARCH_ABI)),)
$(CORE_DIR)/pico/pico_int_offs.h:
	cp $(CORE_DIR)/tools/offsets/generic-ilp32-offsets.h $@
.PHONY: $(CORE_DIR)/pico/pico_int_offs.h

$(filter %.S,$(SRCS_COMMON)): $(CORE_DIR)/pico/pico_int_offs.h
endif

include $(BUILD_SHARED_LIBRARY)
