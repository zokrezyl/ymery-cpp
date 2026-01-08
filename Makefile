# Ymery Build System
# Usage: make <target>
#
# Targets:
#   config-<platform>-<backend>-<mode>  - Configure build
#   build-<platform>-<backend>-<mode>   - Build (auto-runs config if needed)
#   test-<platform>-<backend>-<mode>    - Run tests (auto-runs build if needed)
#
# Modes: release, debug

SYSTEM_PATH := /usr/bin:/bin:/usr/local/bin:$(PATH)

export JAVA_HOME ?= /usr/lib/jvm/java-17-openjdk-amd64
export ANDROID_HOME ?= $(HOME)/android-sdk
export ANDROID_SDK_ROOT ?= $(ANDROID_HOME)

BUILD_TOOLS_DIR := build-tools

CMAKE := cmake
CMAKE_GENERATOR := -G Ninja
CMAKE_RELEASE := -DCMAKE_BUILD_TYPE=Release
CMAKE_DEBUG := -DCMAKE_BUILD_TYPE=Debug

# =============================================================================
# Help (default target)
# =============================================================================

.PHONY: help
help:
	@echo "Ymery Build System"
	@echo ""
	@echo "Usage: make <target>"
	@echo ""
	@echo "Desktop OpenGL:"
	@echo "  config-desktop-opengl-release   Configure desktop OpenGL (release)"
	@echo "  config-desktop-opengl-debug     Configure desktop OpenGL (debug)"
	@echo "  build-desktop-opengl-release    Build desktop OpenGL (release)"
	@echo "  build-desktop-opengl-debug      Build desktop OpenGL (debug)"
	@echo "  test-desktop-opengl-release     Test desktop OpenGL (release)"
	@echo "  test-desktop-opengl-debug       Test desktop OpenGL (debug)"
	@echo ""
	@echo "Desktop WebGPU:"
	@echo "  config-desktop-webgpu-release   Configure desktop WebGPU (release)"
	@echo "  config-desktop-webgpu-debug     Configure desktop WebGPU (debug)"
	@echo "  build-desktop-webgpu-release    Build desktop WebGPU (release)"
	@echo "  build-desktop-webgpu-debug      Build desktop WebGPU (debug)"
	@echo "  test-desktop-webgpu-release     Test desktop WebGPU (release)"
	@echo "  test-desktop-webgpu-debug       Test desktop WebGPU (debug)"
	@echo ""
	@echo "Android OpenGL:"
	@echo "  config-android-opengl-release   Configure Android OpenGL (release)"
	@echo "  config-android-opengl-debug     Configure Android OpenGL (debug)"
	@echo "  build-android-opengl-release    Build Android OpenGL APK (release)"
	@echo "  build-android-opengl-debug      Build Android OpenGL APK (debug)"
	@echo "  test-android-opengl-release     Test Android OpenGL (release)"
	@echo "  test-android-opengl-debug       Test Android OpenGL (debug)"
	@echo ""
	@echo "Android WebGPU:"
	@echo "  config-android-webgpu-release   Configure Android WebGPU (release)"
	@echo "  config-android-webgpu-debug     Configure Android WebGPU (debug)"
	@echo "  build-android-webgpu-release    Build Android WebGPU APK (release)"
	@echo "  build-android-webgpu-debug      Build Android WebGPU APK (debug)"
	@echo "  test-android-webgpu-release     Test Android WebGPU (release)"
	@echo "  test-android-webgpu-debug       Test Android WebGPU (debug)"
	@echo ""
	@echo "WebAssembly WebGPU:"
	@echo "  config-webasm-webgpu-release    Configure WebAssembly WebGPU (release)"
	@echo "  config-webasm-webgpu-debug      Configure WebAssembly WebGPU (debug)"
	@echo "  build-webasm-webgpu-release     Build WebAssembly WebGPU (release)"
	@echo "  build-webasm-webgpu-debug       Build WebAssembly WebGPU (debug)"
	@echo "  test-webasm-webgpu-release      Test WebAssembly WebGPU (release)"
	@echo "  test-webasm-webgpu-debug        Test WebAssembly WebGPU (debug)"
	@echo ""
	@echo "Other:"
	@echo "  clean                           Clean all build directories"

# =============================================================================
# Desktop OpenGL Release
# =============================================================================

.PHONY: config-desktop-opengl-release
config-desktop-opengl-release:
	PATH="$(SYSTEM_PATH)" $(CMAKE) -B build-desktop-opengl-release $(CMAKE_GENERATOR) $(CMAKE_RELEASE) -DYMERY_USE_WEBGPU=OFF -DYMERY_BUILD_GUI_TESTS=OFF

.PHONY: build-desktop-opengl-release
build-desktop-opengl-release:
	@if [ ! -f build-desktop-opengl-release/build.ninja ]; then $(MAKE) config-desktop-opengl-release; fi
	PATH="$(SYSTEM_PATH)" $(CMAKE) --build build-desktop-opengl-release --parallel

.PHONY: test-desktop-opengl-release
test-desktop-opengl-release: build-desktop-opengl-release
	cd build-desktop-opengl-release && ctest --output-on-failure

# =============================================================================
# Desktop OpenGL Debug
# =============================================================================

.PHONY: config-desktop-opengl-debug
config-desktop-opengl-debug:
	PATH="$(SYSTEM_PATH)" $(CMAKE) -B build-desktop-opengl-debug $(CMAKE_GENERATOR) $(CMAKE_DEBUG) -DYMERY_USE_WEBGPU=OFF -DYMERY_BUILD_GUI_TESTS=OFF

.PHONY: build-desktop-opengl-debug
build-desktop-opengl-debug:
	@if [ ! -f build-desktop-opengl-debug/build.ninja ]; then $(MAKE) config-desktop-opengl-debug; fi
	PATH="$(SYSTEM_PATH)" $(CMAKE) --build build-desktop-opengl-debug --parallel

.PHONY: test-desktop-opengl-debug
test-desktop-opengl-debug: build-desktop-opengl-debug
	cd build-desktop-opengl-debug && ctest --output-on-failure

# =============================================================================
# Desktop WebGPU Release
# =============================================================================

.PHONY: config-desktop-webgpu-release
config-desktop-webgpu-release:
	bash $(BUILD_TOOLS_DIR)/shared/build-wgpu-desktop.sh build-desktop-webgpu-release
	PATH="$(SYSTEM_PATH)" $(CMAKE) -B build-desktop-webgpu-release $(CMAKE_GENERATOR) $(CMAKE_RELEASE) -DYMERY_USE_WEBGPU=ON -DYMERY_BUILD_GUI_TESTS=OFF

.PHONY: build-desktop-webgpu-release
build-desktop-webgpu-release:
	@if [ ! -f build-desktop-webgpu-release/build.ninja ]; then $(MAKE) config-desktop-webgpu-release; fi
	PATH="$(SYSTEM_PATH)" $(CMAKE) --build build-desktop-webgpu-release --parallel

.PHONY: test-desktop-webgpu-release
test-desktop-webgpu-release: build-desktop-webgpu-release
	cd build-desktop-webgpu-release && ctest --output-on-failure

# =============================================================================
# Desktop WebGPU Debug
# =============================================================================

.PHONY: config-desktop-webgpu-debug
config-desktop-webgpu-debug:
	bash $(BUILD_TOOLS_DIR)/shared/build-wgpu-desktop.sh build-desktop-webgpu-debug
	PATH="$(SYSTEM_PATH)" $(CMAKE) -B build-desktop-webgpu-debug $(CMAKE_GENERATOR) $(CMAKE_DEBUG) -DYMERY_USE_WEBGPU=ON -DYMERY_BUILD_GUI_TESTS=OFF

.PHONY: build-desktop-webgpu-debug
build-desktop-webgpu-debug:
	@if [ ! -f build-desktop-webgpu-debug/build.ninja ]; then $(MAKE) config-desktop-webgpu-debug; fi
	PATH="$(SYSTEM_PATH)" $(CMAKE) --build build-desktop-webgpu-debug --parallel

.PHONY: test-desktop-webgpu-debug
test-desktop-webgpu-debug: build-desktop-webgpu-debug
	cd build-desktop-webgpu-debug && ctest --output-on-failure

# =============================================================================
# Android OpenGL Release
# =============================================================================

.PHONY: config-android-opengl-release
config-android-opengl-release:
	@echo "Android OpenGL: No separate config step (gradle handles it)"

.PHONY: build-android-opengl-release
build-android-opengl-release:
	cd $(BUILD_TOOLS_DIR)/android/ymery && PATH="$(SYSTEM_PATH)" ./gradlew assembleOpenglRelease

.PHONY: test-android-opengl-release
test-android-opengl-release: build-android-opengl-release
	cd $(BUILD_TOOLS_DIR)/android/ymery && PATH="$(SYSTEM_PATH)" ./gradlew testOpenglReleaseUnitTest

# =============================================================================
# Android OpenGL Debug
# =============================================================================

.PHONY: config-android-opengl-debug
config-android-opengl-debug:
	@echo "Android OpenGL: No separate config step (gradle handles it)"

.PHONY: build-android-opengl-debug
build-android-opengl-debug:
	cd $(BUILD_TOOLS_DIR)/android/ymery && PATH="$(SYSTEM_PATH)" ./gradlew assembleOpenglDebug

.PHONY: test-android-opengl-debug
test-android-opengl-debug: build-android-opengl-debug
	cd $(BUILD_TOOLS_DIR)/android/ymery && PATH="$(SYSTEM_PATH)" ./gradlew testOpenglDebugUnitTest

# =============================================================================
# Android WebGPU Release
# =============================================================================

.PHONY: config-android-webgpu-release
config-android-webgpu-release:
	cd $(BUILD_TOOLS_DIR)/android/ymery && bash ./build-wgpu.sh

.PHONY: build-android-webgpu-release
build-android-webgpu-release:
	@if [ ! -f $(BUILD_TOOLS_DIR)/android/ymery/app/libs/arm64-v8a/libwgpu_native.so ]; then $(MAKE) config-android-webgpu-release; fi
	cd $(BUILD_TOOLS_DIR)/android/ymery && PATH="$(SYSTEM_PATH)" ./gradlew assembleWebgpuRelease

.PHONY: test-android-webgpu-release
test-android-webgpu-release: build-android-webgpu-release
	cd $(BUILD_TOOLS_DIR)/android/ymery && PATH="$(SYSTEM_PATH)" ./gradlew testWebgpuReleaseUnitTest

# =============================================================================
# Android WebGPU Debug
# =============================================================================

.PHONY: config-android-webgpu-debug
config-android-webgpu-debug:
	cd $(BUILD_TOOLS_DIR)/android/ymery && bash ./build-wgpu.sh

.PHONY: build-android-webgpu-debug
build-android-webgpu-debug:
	@if [ ! -f $(BUILD_TOOLS_DIR)/android/ymery/app/libs/arm64-v8a/libwgpu_native.so ]; then $(MAKE) config-android-webgpu-debug; fi
	cd $(BUILD_TOOLS_DIR)/android/ymery && PATH="$(SYSTEM_PATH)" ./gradlew assembleWebgpuDebug

.PHONY: test-android-webgpu-debug
test-android-webgpu-debug: build-android-webgpu-debug
	cd $(BUILD_TOOLS_DIR)/android/ymery && PATH="$(SYSTEM_PATH)" ./gradlew testWebgpuDebugUnitTest

# =============================================================================
# WebAssembly WebGPU Release
# =============================================================================

.PHONY: config-webasm-webgpu-release
config-webasm-webgpu-release:
	nix develop .#web --command bash -c "$(CMAKE) -B build-webasm-webgpu-release $(CMAKE_GENERATOR) $(CMAKE_RELEASE) -DCMAKE_TOOLCHAIN_FILE=$(CURDIR)/$(BUILD_TOOLS_DIR)/cmake/EmscriptenNix.cmake"

.PHONY: build-webasm-webgpu-release
build-webasm-webgpu-release:
	@if [ ! -f build-webasm-webgpu-release/build.ninja ]; then $(MAKE) config-webasm-webgpu-release; fi
	nix develop .#web --command bash -c "$(CMAKE) --build build-webasm-webgpu-release --parallel"

.PHONY: test-webasm-webgpu-release
test-webasm-webgpu-release: build-webasm-webgpu-release
	cd build-webasm-webgpu-release && ctest --output-on-failure

.PHONY: serve-webasm-webgpu-release
serve-webasm-webgpu-release: build-webasm-webgpu-release
	@echo "Serving at http://localhost:8080/ymery.html"
	cd build-webasm-webgpu-release && python3 -m http.server 8080

.PHONY: browser-test-webasm-webgpu-release
browser-test-webasm-webgpu-release: build-webasm-webgpu-release
	@echo "Running headless browser test..."
	nix develop .#web --command bash -c "node test/webasm-test.mjs http://localhost:8080/ymery.html"

# =============================================================================
# WebAssembly WebGPU Debug
# =============================================================================

.PHONY: config-webasm-webgpu-debug
config-webasm-webgpu-debug:
	nix develop .#web --command bash -c "$(CMAKE) -B build-webasm-webgpu-debug $(CMAKE_GENERATOR) $(CMAKE_DEBUG) -DCMAKE_TOOLCHAIN_FILE=$(CURDIR)/$(BUILD_TOOLS_DIR)/cmake/EmscriptenNix.cmake"

.PHONY: build-webasm-webgpu-debug
build-webasm-webgpu-debug:
	@if [ ! -f build-webasm-webgpu-debug/build.ninja ]; then $(MAKE) config-webasm-webgpu-debug; fi
	nix develop .#web --command bash -c "$(CMAKE) --build build-webasm-webgpu-debug --parallel"

.PHONY: test-webasm-webgpu-debug
test-webasm-webgpu-debug: build-webasm-webgpu-debug
	cd build-webasm-webgpu-debug && ctest --output-on-failure

# =============================================================================
# Release Packaging
# =============================================================================

VERSION ?= dev

.PHONY: release-desktop-webgpu-release
release-desktop-webgpu-release: build-desktop-webgpu-release
	bash $(BUILD_TOOLS_DIR)/release/package-linux.sh build-desktop-webgpu-release $(VERSION)

.PHONY: release-android-opengl-release
release-android-opengl-release: build-android-opengl-release
	bash $(BUILD_TOOLS_DIR)/release/package-android.sh $(VERSION)

.PHONY: release-android-webgpu-release
release-android-webgpu-release: build-android-webgpu-release
	bash $(BUILD_TOOLS_DIR)/release/package-android.sh $(VERSION)

.PHONY: release-webasm-webgpu-release
release-webasm-webgpu-release: build-webasm-webgpu-release
	bash $(BUILD_TOOLS_DIR)/release/package-web.sh build-webasm-webgpu-release $(VERSION)

# =============================================================================
# Clean
# =============================================================================

.PHONY: clean
clean:
	rm -rf build-desktop-opengl-release build-desktop-opengl-debug
	rm -rf build-desktop-webgpu-release build-desktop-webgpu-debug
	rm -rf build-webasm-webgpu-release build-webasm-webgpu-debug
	cd $(BUILD_TOOLS_DIR)/android/ymery && PATH="$(SYSTEM_PATH)" ./gradlew clean 2>/dev/null || true
