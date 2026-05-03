# ==============================================================================
# CONSENSUS HYPERVISOR - BARE METAL BUILD SYSTEM
# ==============================================================================
ARCH ?= x86_64
TARGET = emergence_os.bin
BUILD_DIR = build/$(ARCH)
SRC_DIR = src

ifeq ($(ARCH), aarch64)
PREFIX = aarch64-linux-gnu-
else
PREFIX = x86_64-linux-gnu-
endif

CXX = $(PREFIX)g++
AS = $(PREFIX)as
LD = $(PREFIX)ld

CXXFLAGS = -ffreestanding -fno-pie -fno-stack-protector -fno-exceptions -fno-rtti -fno-threadsafe-statics -O3 -Wall -Wextra \
           -nostdlib -nostdinc++ -I$(SRC_DIR) -I$(SRC_DIR)/boot -I$(SRC_DIR)/core -I$(SRC_DIR)/crypto -I$(SRC_DIR)/hal -I$(SRC_DIR)/mm -I$(SRC_DIR)/vdev

ASFLAGS = --64
LDFLAGS = -m elf_x86_64 -T linker.ld -nostdlib -z noexecstack

ifeq ($(ARCH), aarch64)
CXXFLAGS += -mgeneral-regs-only
else
CXXFLAGS += -mno-red-zone -mno-mmx -mno-sse -mno-sse2 -m64
endif

CPP_SOURCES = $(wildcard $(SRC_DIR)/**/*.cpp $(SRC_DIR)/*.cpp)
S_SOURCES = $(wildcard $(SRC_DIR)/**/*.S $(SRC_DIR)/*.S)

OBJECTS = $(patsubst $(SRC_DIR)/%.cpp, $(BUILD_DIR)/%.o, $(CPP_SOURCES)) \
          $(patsubst $(SRC_DIR)/%.S, $(BUILD_DIR)/%.o, $(S_SOURCES))

all: prep $(TARGET) emergence_os.iso

prep:
	@mkdir -p $(BUILD_DIR)/boot $(BUILD_DIR)/core $(BUILD_DIR)/crypto $(BUILD_DIR)/hal $(BUILD_DIR)/mm $(BUILD_DIR)/vdev
	@mkdir -p isodir/boot/grub

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	@$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.S
	@$(AS) $(ASFLAGS) $< -o $@

$(TARGET): $(OBJECTS)
	@$(LD) $(LDFLAGS) -o $@ $(OBJECTS)

release: LDFLAGS += -s
release: CXXFLAGS += -DNDEBUG -fvisibility=hidden
release: all
	@echo "[OK] Enterprise Binary Hardened and Stripped."

emergence_os.iso: $(TARGET) isodir/boot/grub/grub.cfg
	@cp $(TARGET) isodir/boot/
	@grub-mkrescue -o emergence_os.iso isodir/

clean:
	rm -rf build
	rm -f $(TARGET) emergence_os.iso
