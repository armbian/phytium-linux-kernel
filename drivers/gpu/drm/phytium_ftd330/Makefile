# SPDX-License-Identifier: GPL-2.0
ccflags-y := -I $(srctree)/$(src)
ccflags-y += -I $(srctree)/$(src)/FTD330
#ccflags-y += -Wall  -Wextra

ftd330-drm-dc-y := ftd330_crtc.o \
		ftd330_drv.o \
		ftd330_fb.o \
		ftd330_gem.o \
		ftd330_plane.o \
		ftd330_dc_lut.o \
		ftd330_dc_scale.o \
		ftd330_dc_property.o \
		ftd330_dc_drm_property.o \
		ftd330_simple_enc.o

ifeq ($(CONFIG_PHYTIUM_DCDP_EMULATOR), y)
ftd330-drm-dc-y += phytium_dp_emulator.o
else
ftd330-drm-dc-y += phytium_dp.o
ftd330-drm-dc-y += ftd330_dp.o
ftd330-drm-dc-y += phytium_panel.o
ftd330-drm-dc-y += phytium_edp_pwm.o
endif

ftd330-drm-dc-$(CONFIG_PHYTIUM_PSR) += phytium_psr.o

ftd330-drm-dc-y += phytium_vrr.o
ftd330-drm-dc-y += phytium_dp_debugfs.o
ftd330-drm-dc-y += phytium_parse_bios.o
ftd330-drm-dc-y += phytium_se_communicate.o

# FTD330 specific info
ftd330-drm-dc-y += FTD330/info/phytium_dc_info.o

# FTD330 other objs
ftd330-drm-dc-y += FTD330/ftd330_dc_hw.o
ftd330-drm-dc-y += FTD330/ftd330_dc.o
ftd330-drm-dc-y += FTD330/preprocess/ftd330_dc_plane_blender.o
ftd330-drm-dc-y += FTD330/preprocess/ftd330_dc_preprocess.o
ftd330-drm-dc-y += FTD330/postprocess/ftd330_dc_postprocess.o

ftd330-drm-dc-$(CONFIG_PHYTIUM_WRITEBACK) += ftd330_writeback.o
ftd330-drm-dc-$(CONFIG_PHYTIUM_WRITEBACK) += FTD330/writeback/ftd330_dc_writeback.o
ftd330-drm-dc-$(CONFIG_PHYTIUM_DEBUG) += ftd330_debug.o
ftd330-drm-dc-$(CONFIG_PHYTIUM_MMU) += ftd330_dc_mmu.o
ftd330-drm-dc-$(CONFIG_PHYTIUM_DEC) += FTD330/ftd330_dc_dec.o
ftd330-drm-dc-$(CONFIG_PHYTIUM_FBDEV_ON) += phytium_fbdev.o
ftd330-drm-dc-$(CONFIG_PHYTIUM_VIRTUAL_DISPLAY) += ftd330_virtual.o

obj-$(CONFIG_DRM_PHYTIUM_FTD330) += ftd330-drm-dc.o

CFLAGS_REMOVE_ftd330_dc_scale.o += -mgeneral-regs-only
