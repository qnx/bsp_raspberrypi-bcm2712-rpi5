define PINFO
PINFO DESCRIPTION=sd/mmc disk driver
endef
# Generate a new variant use file if board specific use file is presented.
VARIANT_NAME=$(basename $(VARIANT1))
ifneq ($(wildcard $(PWD)/$(VARIANT_NAME).use),)
GENERIC_USEFILE=$(PROJECT_ROOT)/$(SECTION)/$(NAME).use
SOC_USEFILE=$(PROJECT_ROOT)/$(SECTION)/$(CPU)/$(VARIANT1)/$(VARIANT_NAME).use
USEFILE=$(PROJECT_ROOT)/$(SECTION)/$(CPU)/$(VARIANT1)/$(NAME)-$(VARIANT_NAME).use

$(shell cat $(GENERIC_USEFILE) > $(USEFILE))
$(shell cat $(SOC_USEFILE) >> $(USEFILE))
$(shell sed -i 's/<variant>/$(VARIANT_NAME)/' $(USEFILE))

EXTRA_CLEAN += $(NAME)-$(VARIANT_NAME).use
endif
PUBLIC_INCVPATH += $(wildcard $(PROJECT_ROOT)/$(SECTION)/public )

# Uncomment when powman is enabled on all variants
#LIBS += powman slog2

NDAS := $(wildcard *.o-nda)

ifeq ($(filter $(basename $(NDAS)), $(basename $(notdir $(SRCS)))),)
# we don't have the NDA source, have to use pre-compiled binaries
SRCS += $(NDAS)
else
# We don't need the following lines if we're willing to manually update
# the *.?-nda files.
#UPDATE_NDA_OBJECTS = 1
#POST_TARGET = $(NDAS)
endif
