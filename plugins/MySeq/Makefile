#!/usr/bin/make -f
# Makefile for DISTRHO Plugins
# SPDX-License-Identifier: ISC

# --------------------------------------------------------------
# Project name, used for binaries

NAME = MySeq

# --------------------------------------------------------------
# Files to build

FILES_DSP = \
	PluginDSP.cpp \
	Patterns.cpp \
	Stats.cpp

FILES_UI = \
	PluginUI.cpp \
	Patterns.cpp \
	GenArray.cpp \
	Stats.cpp \
	../../dpf-widgets/opengl/DearImGui.cpp

# --------------------------------------------------------------
# Do some magic

include ../../dpf/Makefile.plugins.mk

BUILD_CXX_FLAGS += -I../../dpf-widgets/opengl -I/opt/homebrew/include -std=gnu++17 -DDPF_CAPTURE_CONSOLE_OUTPUT

# --------------------------------------------------------------
# Enable all possible plugin types

# all: clap dssi jack lv2_sep vst2 vst3
all: jack clap vst2 vst3

# --------------------------------------------------------------
