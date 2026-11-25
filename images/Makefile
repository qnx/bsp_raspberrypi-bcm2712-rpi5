ROOT_DIR := $(notdir $(CURDIR))
ifndef QCONFIG
QCONFIG=qconfig.mk
endif
include $(QCONFIG)

HOST_MKIFS := mkifs
HOST_SED := sed
SUFFIXES := .build .bin .raw

# NOTE: This value must match the '[image=<start_addr>]' value in the build file.
IMAGE_LOAD_ADDR = 0x80000

PROCESSOR = aarch64
BUILD_TEMPLATE = $(CURDIR)/../../Templates_build_sdp800
BOARD=rpi5
INSTALL=../install

.PHONY: all clean

all: ifs-$(BOARD).bin

# to boot QNX ifs-rpi5.bin IFS image, add "kernel=ifs-rpi5.bin" to bootable microSD card config.txt file.
# The image runs at IMAGE_LOAD_ADDR. If config.txt also sets kernel_address, it must be the same
# value (kernel_address=0x80000). Since Pi 5 bootloader 2026-06-17 the firmware no longer relocates
# images that lack an ARM64 header: it runs them at kernel_address, so an image loaded at another
# address (for example kernel_address=0x8000000) hangs before any output.
ifs-$(BOARD).bin: $(BOARD).build
	$(HOST_MKIFS) -v -r$(INSTALL) $(MKIFSFLAGS) $^ $@

# To boot from U-boot "go" command:
#  - load ifs-rpi5.raw IFS image to ${IMAGE_LOAD_ADDR}
#  - run "go ${IMAGE_LOAD_ADDR} ${fdtcontroladdr}"
ifs-$(BOARD).raw: $(BOARD).build
	$(CP_HOST) $(BOARD).build $(BOARD)-go.build
	$(HOST_SED) -i 's/u reg/u arg/' $(BOARD)-go.build
	$(HOST_MKIFS) -v -r$(INSTALL) $(MKIFSFLAGS)  $(BOARD)-go.build $@
	$(RM_HOST) $(BOARD)-go.build

clean:
	$(RM_HOST) ifs-$(BOARD).* *.sym

-include $(BUILD_TEMPLATE)/template.mk
