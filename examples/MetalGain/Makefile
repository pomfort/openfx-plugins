PLUGIN    := MetalGainExample

BUILD_DIR := build
OBJ_DIR   := $(BUILD_DIR)/obj
BUNDLE    := $(BUILD_DIR)/$(PLUGIN).ofx.bundle
BINARY    := $(BUNDLE)/Contents/MacOS/$(PLUGIN).ofx
PLIST     := $(BUNDLE)/Contents/Info.plist
SIGNED    := $(BUILD_DIR)/.signed

# Override to install elsewhere, e.g. make install OFX_PLUGIN_PATH=~/Library/OFX/Plugins
OFX_PLUGIN_PATH ?= /Library/OFX/Plugins

# Livegrade is Apple silicon only; override for a universal build, e.g. ARCHS="arm64 x86_64"
ARCHS      ?= arm64
ARCH_FLAGS := $(addprefix -arch ,$(ARCHS))

SRC_DIR    := src
OFX_DIR    := third_party/openfx

CPPFLAGS   := -I$(OFX_DIR)/include -I$(OFX_DIR)/Support/include -Ithird_party/extensions/include
CXXFLAGS   := -std=c++20 -O2 -mmacosx-version-min=15.0 -fvisibility=hidden -fvisibility-inlines-hidden -MMD -MP $(ARCH_FLAGS)
LDFLAGS    := -bundle -mmacosx-version-min=15.0 -fvisibility=hidden -framework Metal -framework Foundation -framework AppKit $(ARCH_FLAGS)

SUPPORT_SRCS := $(wildcard $(OFX_DIR)/Support/src/*.cpp)

OBJS := $(OBJ_DIR)/$(SRC_DIR)/$(PLUGIN).o \
        $(OBJ_DIR)/$(SRC_DIR)/MetalKernel.o \
        $(OBJ_DIR)/$(SRC_DIR)/SupportWebpage.o \
        $(SUPPORT_SRCS:%.cpp=$(OBJ_DIR)/%.o)
DEPS := $(OBJS:.o=.d)

all: $(SIGNED)

$(BINARY): $(OBJS)
	@mkdir -p $(dir $@)
	$(CXX) $^ -o $@ $(LDFLAGS)

# The bundle is signed once its executable and Info.plist are in place. The stamp
# file keeps an unchanged build a no-op.
$(SIGNED): $(BINARY) $(PLIST)
	codesign --force --sign - "$(BUNDLE)"
	@touch $@

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

install: $(SIGNED)
	mkdir -p "$(OFX_PLUGIN_PATH)"
	rm -rf "$(OFX_PLUGIN_PATH)/$(PLUGIN).ofx.bundle"
	cp -R "$(BUNDLE)" "$(OFX_PLUGIN_PATH)"

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all install clean
