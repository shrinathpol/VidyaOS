VIDYAOS_NATIVE_VERSION = 1.0.0
VIDYAOS_NATIVE_SITE = /home/shrinathpol/EmbeddedSystem
VIDYAOS_NATIVE_SITE_METHOD = local
VIDYAOS_NATIVE_DEPENDENCIES = sdl2 sdl2_ttf libvterm

define VIDYAOS_NATIVE_BUILD_CMDS
	$(TARGET_MAKE_ENV) $(TARGET_CONFIGURE_OPTS) $(MAKE) -C $(@D) -f Makefile.standalone NATIVE=1
endef

define VIDYAOS_NATIVE_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/vidyaos-desktop $(TARGET_DIR)/usr/bin/vidyaos-native
endef

$(eval $(generic-package))
