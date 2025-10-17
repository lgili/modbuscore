PREFIX ?= /usr/local
PKG_CONFIG ?= pkg-config
PKG_CONFIG_PATH := $(PREFIX)/lib/pkgconfig:$(PKG_CONFIG_PATH)

all: pkgconfig_consume

pkgconfig_consume: main.c
	PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) $(CC) main.c \
		$(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) $(PKG_CONFIG) --cflags modbuscore) \
		$(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) $(PKG_CONFIG) --libs modbuscore) \
		-o $@

clean:
	$(RM) pkgconfig_consume

.PHONY: all clean
