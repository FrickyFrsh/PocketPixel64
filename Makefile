TARGET = EBOOT
OBJS = main.o
CFLAGS = -O2 -G0 -Wall -Wextra -std=gnu99
CXXFLAGS = $(CFLAGS)
ASFLAGS = $(CFLAGS)
LIBS = -lpspgu -lpspdisplay -lpspctrl
EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = PSP Pixel Studio V2.2
PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
