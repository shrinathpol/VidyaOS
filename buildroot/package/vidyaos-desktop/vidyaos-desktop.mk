VIDYAOS_DESKTOP_VERSION = 1.0.0
VIDYAOS_DESKTOP_SITE = /home/shrinathpol/EmbeddedSystem
VIDYAOS_DESKTOP_SITE_METHOD = local
VIDYAOS_DESKTOP_DEPENDENCIES = sdl2

define VIDYAOS_DESKTOP_BUILD_CMDS
	$(TARGET_MAKE_ENV) $(TARGET_CONFIGURE_OPTS) $(MAKE) -C $(@D) -f Makefile.standalone VIDYAOS=1
endef

define VIDYAOS_DESKTOP_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/vidyaos-desktop $(TARGET_DIR)/usr/bin/vidyaos-desktop
endef

$(eval $(generic-package))
