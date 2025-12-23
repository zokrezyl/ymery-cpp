# Ymery Build System
# Usage: make <target>

# Use system tools (avoid nix)
export PATH := /usr/bin:/bin:/usr/local/bin:$(PATH)

# Android SDK (adjust paths as needed)
export JAVA_HOME ?= /usr/lib/jvm/java-17-openjdk-amd64
export ANDROID_HOME ?= $(HOME)/android-sdk
export ANDROID_SDK_ROOT ?= $(ANDROID_HOME)

# Directories
BUILD_DIR_OPENGL := build-opengl
BUILD_DIR_WEBGPU := build-webgpu
BUILD_DIR_DEBUG := build-debug
BUILD_DIR_WEB := build-web
ANDROID_DIR := android

# CMake options
CMAKE := cmake
CMAKE_GENERATOR := -G Ninja
CMAKE_COMMON := -DCMAKE_POLICY_VERSION_MINIMUM=3.5
CMAKE_RELEASE := -DCMAKE_BUILD_TYPE=Release
CMAKE_DEBUG := -DCMAKE_BUILD_TYPE=Debug

# Default target
.PHONY: all
all: opengl

# === Desktop Builds ===

.PHONY: opengl
opengl: ## Build with OpenGL backend (Release)
	$(CMAKE) -B $(BUILD_DIR_OPENGL) $(CMAKE_GENERATOR) $(CMAKE_COMMON) $(CMAKE_RELEASE) -DYMERY_USE_WEBGPU=OFF
	$(CMAKE) --build $(BUILD_DIR_OPENGL)

.PHONY: webgpu
webgpu: ## Build with WebGPU backend (Release)
	$(CMAKE) -B $(BUILD_DIR_WEBGPU) $(CMAKE_GENERATOR) $(CMAKE_COMMON) $(CMAKE_RELEASE) -DYMERY_USE_WEBGPU=ON
	$(CMAKE) --build $(BUILD_DIR_WEBGPU)

.PHONY: debug
debug: ## Build with OpenGL backend (Debug)
	$(CMAKE) -B $(BUILD_DIR_DEBUG) $(CMAKE_GENERATOR) $(CMAKE_COMMON) $(CMAKE_DEBUG) -DYMERY_USE_WEBGPU=OFF
	$(CMAKE) --build $(BUILD_DIR_DEBUG)

.PHONY: debug-webgpu
debug-webgpu: ## Build with WebGPU backend (Debug)
	$(CMAKE) -B $(BUILD_DIR_WEBGPU)-debug $(CMAKE_GENERATOR) $(CMAKE_COMMON) $(CMAKE_DEBUG) -DYMERY_USE_WEBGPU=ON
	$(CMAKE) --build $(BUILD_DIR_WEBGPU)-debug

# === Android Build ===

.PHONY: android
android: ## Build Android APK (Debug)
	cd $(ANDROID_DIR) && ./gradlew assembleDebug

.PHONY: android-release
android-release: ## Build Android APK (Release)
	cd $(ANDROID_DIR) && ./gradlew assembleRelease

.PHONY: android-clean
android-clean: ## Clean Android build
	cd $(ANDROID_DIR) && ./gradlew clean
	rm -rf $(ANDROID_DIR)/app/.cxx

# === Web/Emscripten Build ===

.PHONY: web
web: ## Build for Web with Emscripten
	emcmake $(CMAKE) -B $(BUILD_DIR_WEB) $(CMAKE_COMMON) $(CMAKE_RELEASE)
	$(CMAKE) --build $(BUILD_DIR_WEB)

# === Individual Targets ===

.PHONY: cli
cli: opengl ## Build ymery-cli (OpenGL)
	$(CMAKE) --build $(BUILD_DIR_OPENGL) --target ymery-cli

.PHONY: editor
editor: opengl ## Build ymery-editor (OpenGL)
	$(CMAKE) --build $(BUILD_DIR_OPENGL) --target ymery-editor

.PHONY: lib
lib: opengl ## Build libymery only (OpenGL)
	$(CMAKE) --build $(BUILD_DIR_OPENGL) --target ymery

.PHONY: plugins
plugins: opengl ## Build plugins (OpenGL)
	$(CMAKE) --build $(BUILD_DIR_OPENGL) --target plugins

# === Run Targets ===

.PHONY: run
run: cli ## Run ymery-cli
	./$(BUILD_DIR_OPENGL)/ymery-cli

.PHONY: run-editor
run-editor: editor ## Run ymery-editor
	./$(BUILD_DIR_OPENGL)/ymery-editor

.PHONY: run-webgpu
run-webgpu: webgpu ## Run ymery-cli with WebGPU
	./$(BUILD_DIR_WEBGPU)/ymery-cli

# === Clean Targets ===

.PHONY: clean
clean: ## Clean all build directories
	rm -rf $(BUILD_DIR_OPENGL) $(BUILD_DIR_WEBGPU) $(BUILD_DIR_DEBUG) $(BUILD_DIR_WEB)
	rm -rf $(BUILD_DIR_WEBGPU)-debug

.PHONY: clean-opengl
clean-opengl: ## Clean OpenGL build
	rm -rf $(BUILD_DIR_OPENGL)

.PHONY: clean-webgpu
clean-webgpu: ## Clean WebGPU build
	rm -rf $(BUILD_DIR_WEBGPU) $(BUILD_DIR_WEBGPU)-debug

.PHONY: clean-all
clean-all: clean android-clean ## Clean everything including Android

# === Rebuild Targets ===

.PHONY: rebuild
rebuild: clean opengl ## Clean and rebuild (OpenGL)

.PHONY: rebuild-webgpu
rebuild-webgpu: clean-webgpu webgpu ## Clean and rebuild (WebGPU)

# === Install ===

.PHONY: install
install: opengl ## Install to system (requires sudo)
	$(CMAKE) --install $(BUILD_DIR_OPENGL)

# === Help ===

.PHONY: help
help: ## Show this help
	@echo "Ymery Build System"
	@echo ""
	@echo "Usage: make <target>"
	@echo ""
	@echo "Targets:"
	@grep -E '^[a-zA-Z_-]+:.*?## .*$$' $(MAKEFILE_LIST) | sort | awk 'BEGIN {FS = ":.*?## "}; {printf "  \033[36m%-16s\033[0m %s\n", $$1, $$2}'
	@echo ""
	@echo "Build directories:"
	@echo "  $(BUILD_DIR_OPENGL)    - OpenGL Release build"
	@echo "  $(BUILD_DIR_WEBGPU)    - WebGPU Release build"
	@echo "  $(BUILD_DIR_DEBUG)     - OpenGL Debug build"
	@echo "  $(BUILD_DIR_WEB)       - Emscripten/Web build"
	@echo "  $(ANDROID_DIR)         - Android build"
