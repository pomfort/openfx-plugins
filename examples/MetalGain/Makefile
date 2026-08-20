PLUGIN    := MetalGainExample

BUILD_DIR := build
OBJ_DIR   := $(BUILD_DIR)/obj
BUNDLE    := $(BUILD_DIR)/$(PLUGIN).ofx.bundle
BINARY    := $(BUNDLE)/Contents/MacOS/$(PLUGIN).ofx
PLIST     := $(BUNDLE)/Contents/Info.plist

# Override to install elsewhere, e.g. make install OFX_PLUGIN_PATH=~/Library/OFX/Plugins
OFX_PLUGIN_PATH ?= /Library/OFX/Plugins

# Livegrade is Apple silicon only; override for a universal build, e.g. ARCHS="arm64 x86_64"
ARCHS      ?= arm64
ARCH_FLAGS := $(addprefix -arch ,$(ARCHS))

SRC_DIR    := src
OFX_DIR    := third_party/openfx

CPPFLAGS   := -I$(OFX_DIR)/include -I$(OFX_DIR)/Support/include -Ithird_party/extensions/include
CXXFLAGS   := -std=c++20 -fvisibility=hidden -MMD -MP $(ARCH_FLAGS)
LDFLAGS    := -bundle -fvisibility=hidden -framework Metal -framework Foundation -framework AppKit $(ARCH_FLAGS)

SUPPORT_SRCS := $(wildcard $(OFX_DIR)/Support/src/*.cpp)

OBJS := $(OBJ_DIR)/$(SRC_DIR)/$(PLUGIN).o \
        $(OBJ_DIR)/$(SRC_DIR)/MetalKernel.o \
        $(OBJ_DIR)/$(SRC_DIR)/SupportWebpage.o \
        $(SUPPORT_SRCS:%.cpp=$(OBJ_DIR)/%.o)
DEPS := $(OBJS:.o=.d)

all: $(BINARY) $(PLIST)

$(BINARY): $(OBJS)
	@mkdir -p $(dir $@)
	$(CXX) $^ -o $@ $(LDFLAGS)

$(OBJ_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

$(OBJ_DIR)/%.o: %.mm
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

$(PLIST): Info.plist
	@mkdir -p $(dir $@)
	cp $< $@

# Header edits trigger a rebuild of the objects that include them.
-include $(DEPS)

install: $(BINARY) $(PLIST)
	mkdir -p "$(OFX_PLUGIN_PATH)"
	rm -rf "$(OFX_PLUGIN_PATH)/$(PLUGIN).ofx.bundle"
	cp -R "$(BUNDLE)" "$(OFX_PLUGIN_PATH)"

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all install clean
