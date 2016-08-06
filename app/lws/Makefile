
#############################################################
# Required variables for each makefile
# Discard this section from all parent makefiles
# Expected variables (with automatic defaults):
#   CSRCS (all "C" files in the dir)
#   SUBDIRS (all subdirs with a Makefile)
#   GEN_LIBS - list of libs to be generated ()
#   GEN_IMAGES - list of images to be generated ()
#   COMPONENTS_xxx - a list of libs/objs in the form
#     subdir/lib to be extracted and rolled up into
#     a generated lib/image xxx.a ()
#
ifndef PDIR
GEN_LIBS = lws.a
endif

#############################################################
# Configuration i.e. compile options etc.
# Target specific stuff (defines etc.) goes in here!
# Generally values applying to a tree are captured in the
#   makefile at its root level - these are then overridden
#   for a subtree within the makefile rooted therein
#
#DEFINES += 

CROSS_PATH1:=$(shell which xtensa-lx106-elf-gcc )
CROSS_PATH:= $(shell dirname $(CROSS_PATH1) )/..

all: myall

myall:
	mkdir -p build ;\
	cd build ;\
	cmake ../libwebsockets -DLWS_C_FLAGS="$(CFLAGS)" \
		-DCROSS_PATH=$(CROSS_PATH) \
		-DCMAKE_TOOLCHAIN_FILE=../cross-xtensa.cmake \
		-DCMAKE_BUILD_TYPE=RELEASE \
		-DLWS_WITH_NO_LOGS=1 \
		-DLWS_WITH_ESP8266=1 ;\
	make VERBOSE=1 && pwd && \
	mkdir -p lib/.output/eagle/debug/lib && \
	cp -p lib/libwebsockets.a lib/.output/eagle/debug/lib/

clean: myclean

myclean:
	rm -rf ./build

INCLUDES := $(INCLUDES) -I build/ 

#############################################################
# Recursion Magic - Don't touch this!!
#
# Each subtree potentially has an include directory
#   corresponding to the common APIs applicable to modules
#   rooted at that subtree. Accordingly, the INCLUDE PATH
#   of a module can only contain the include directories up
#   its parent path, and not its siblings
#
# Required for each makefile to inherit from the parent
#

INCLUDES := $(INCLUDES) -I $(PDIR)include
INCLUDES += -I ./
INCLUDES += -I ../libc
PDIR := ../$(PDIR)
sinclude $(PDIR)Makefile

