# Vulkan Port Tasklist & Session State Tracker

**Branch:** `feature/android-vulkan-port`  
**Target:** OpenXRay Android ARM64 (`arm64-v8a`) — Qualcomm Adreno 619 (`SM6375`)  
**Persistent Status File:** `doc/vulkan_port_tasklist.md` (Update after every milestone)

---

## 1. Quick Session Resume (For Any Agent / Local Restart)

- **Current Active Phase:** **Phase 3.2: Buffers, Textures & 2D UI Pipeline (ACTIVE)**
- **Default Renderer:** Vulkan (`renderer_vk`)
- **Connected Test Device:** `HQ63CA17FC` (Sony Xperia XQ-DC54, Android 13/API 33, Adreno 619)
- **Primary Toolchain:**
  - Build script: `.\scripts\build_android.ps1 -Config Release`
  - APK build: `.\gradlew.bat assembleDebug` (inside `android/`)
  - Deploy & Launch: `adb install -r android/app/build/outputs/apk/debug/app-debug.apk` + `adb shell am start -n org.openxray/.OpenXRayActivity`

---

## 2. Granular Task Checklist

### Phase 3.1: Scaffolding & Swapchain Baseline (COMPLETED)
- [x] **Engine & Registration Layer**
  - [x] Add `XRRENDER_VK_API` and `render_vk::GetRendererModule()` declaration in `src/Include/xrRender/xrRender.h`.
  - [x] Update `CEngineAPI::CreateRendererList` in `src/xrEngine/EngineAPI.h` and `EngineAPI.cpp` to support dynamic/3 modules and prefer `renderer_vk` on Android.
  - [x] Register `render_vk::GetRendererModule()` in `src/xr_3da/entry_point.cpp`.
  - [x] Link `xrRender_VK` in `src/xr_3da/CMakeLists.txt`.
  - [x] Configure `OpenXRayActivity.java` to load `xrRender_VK` and pass `-renderer renderer_vk`.
- [x] **Vulkan Abstraction Layer (`src/Layers/xrRenderVK/`)**
  - [x] Create `CommonTypes.h` (Vulkan error handling, helper macros, type aliases).
  - [x] Create `vkHW.h` and `vkHW.cpp`:
    - [x] `SetPrimaryAttributes()` with `SDL_WINDOW_VULKAN`.
    - [x] Instance creation with SDL2 instance extensions.
    - [x] Surface creation via `SDL_Vulkan_CreateSurface`.
    - [x] Physical device enumeration and queue family discovery (Graphics & Present).
    - [x] Logical device creation (`VkDevice`) with `VK_KHR_swapchain`.
    - [x] Swapchain creation (`VkSwapchainKHR`, format `B8G8R8A8` / `R8G8B8A8`, double/triple buffering).
    - [x] Swapchain image views and command pool.
    - [x] Frame synchronization primitives (`VkSemaphore`, `VkFence`).
    - [x] `BeginFrame()`, `ClearTarget()`, `EndFrame()` / `Present()`.
  - [x] Create `vkHWCaps.h` and `vkHWCaps.cpp`.
- [x] **Renderer PC Target (`src/Layers/xrRenderPC_VK/`)**
  - [x] Create `CMakeLists.txt` for `xrRender_VK` linking against NDK `libvulkan.so`, `SDL2`, `xrCore`, `xrEngine`, `xrAPI`, and `log`.
  - [x] Create `stdafx.h` and `stdafx.cpp`.
  - [x] Create `xrRender_VK.cpp` implementing `RVKRendererModule` (`renderer_vk`).
  - [x] Add `add_subdirectory(xrRenderPC_VK)` to `src/Layers/CMakeLists.txt`.
- [x] **Device Deployment & Verification**
  - [x] Compile release shared object `bin/aarch64/Release/xrRender_VK.so`.
  - [x] Package APK with `gradlew.bat assembleDebug`.
  - [x] Deploy to device `HQ63CA17FC` and verify clean boot, surface creation, and swapchain clear presentation.

---

### Phase 3.2: Buffers, Textures & 2D UI Pipeline (ACTIVE)
- [x] Implement `vkBufferUtils.cpp` (vertex and index stream/staging buffers, `CreateGeom` support).
- [x] Resolved `ImFontAtlasUpdateNewFrame` SIGSEGV by setting `ImGuiBackendFlags_RendererHasTextures` in `dxImGuiRender.cpp`.
- [x] Resolved `dxFontRender.cpp` `R_ASSERT(T)` crash by adding dynamic sampler registration in `CBlender_Compile::i_Sampler` (`Blender_Recorder_R2.cpp`) and setting default texture dimensions in `vkSH_Texture.cpp` (`desc_update`).
- [x] Resolved `CRender::RenderMenu()` null pointer crash by creating `g_menu`, `s_menu`, and implementing `get_base_rt()` in `CRenderTarget` (`vk_rendertarget.cpp`).
- [x] **Milestone Achieved**: Full game engine boot and continuous Vulkan frame presentation loop running live on device (`Adreno 619`, ~42% CPU, 0 errors/crashes).
- [ ] Implement `vkTexture.cpp` and `vkTextureUtils.cpp` (image allocation, layout transitions, samplers).
- [ ] Implement `dxImGuiRender` Vulkan backend (`imgui_impl_vulkan`).
- [ ] Render 2D UI, console, and Main Menu in pure Vulkan.

---

### Phase 3.3: SPIR-V Pipeline & Static Geometry (Upcoming)
- [ ] Configure `shaderc` / runtime SPIR-V module compilation.
- [ ] Implement Pipeline State Object (PSO) and Descriptor Set layout manager.
- [ ] Forward rendering pass for static meshes, props, and terrain.

---

### Phase 3.4: Mobile TBDR Subpasses & Deferred Shading (Upcoming)
- [ ] Design mobile subpass RenderPass:
  - Subpass 0: G-Buffer generation (`VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT`).
  - Subpass 1: Light accumulation (`input attachments` read directly from Adreno GMEM).
  - Subpass 2: Combine pass to swapchain.
- [ ] Benchmark frametime and thermal stability against GLES 3.2 baseline.

---

## 3. Key Architectural Notes & Gotchas
- **NDK Headers:** Vulkan 1.2 headers are located in the NDK sysroot (`usr/include/vulkan/vulkan.h`).
- **Android Vulkan Loader:** Dynamically links with `vulkan` in CMake (`-lvulkan`), provided by the Android OS runtime.
- **SDL2 Integration:** Must include `SDL_vulkan.h` from sysroot (`android/sysroot/arm64-v8a/include/SDL2`).
- **Adreno GMEM:** G-buffer attachments MUST NOT be flushed to DRAM. Use subpass input attachments with `LOAD_OP_DONT_CARE` and `STORE_OP_DONT_CARE`.
