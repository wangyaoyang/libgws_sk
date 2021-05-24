#
# Top level makefile for gws_nl library
#
#
# Copyright (C) 2010-2015 6Harmonics Inc.
#
# 
#
include $(TOPDIR)/rules.mk
PKG_NAME:=libgws_sk
SRC_NAME:=gws_sk
PKG_VERSION:=1.0
PKG_RELEASE:=0

include $(INCLUDE_DIR)/package.mk

define Package/$(PKG_NAME)
  SECTION:=libs
  CATEGORY:=Libraries
  TITLE:=6Harmonics GWS Socket library
  DEPENDS:=+libpthread +libgws
  MAINTAINER:=6Harmonics
endef

define Package/$(PKG_NAME)/description
  6Harmonics GWS Socket library
endef

define Build/Prepare
	$(CP) -r ./$(SRC_NAME)/* $(PKG_BUILD_DIR)/
endef

define Build/Configure
endef

define Build/InstallDev
	$(INSTALL_DIR) $(1)/usr/lib/pkgconfig $(1)/usr/include/libgws_sk
	$(CP) $(PKG_BUILD_DIR)/*.h $(1)/usr/include/libgws_sk
	$(CP) ./files/libgws_sk.pc $(1)/usr/lib/pkgconfig
endef

define Package/$(PKG_NAME)/install
	$(INSTALL_DIR) $(1)/usr/include
	$(INSTALL_DIR) $(1)/usr/lib
	$(INSTALL_DIR) $(PKG_BUILD_DIR)/../libgws
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/wsocket.h $(PKG_BUILD_DIR)/../libgws/wsocket.h
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/wsocket.h $(1)/usr/include/wsocket.h
#	$(INSTALL_BIN) $(PKG_BUILD_DIR)/dist/Release/MIPS_4_4_14-Linux/libgws_sk.so $(1)/usr/lib/
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/dist/Release/GNU_GWS5000_4_4_14-Linux/libgws_sk.a $(PKG_BUILD_DIR)/../libgws/libgws_sk.a
endef

$(eval $(call BuildPackage,$(PKG_NAME)))

