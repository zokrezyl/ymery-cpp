App is crashing when using webgpu target and resize the main window

```
->build-webgpu/ymery-cli -p demo/layouts/editor -m app                                                                                               misi@oldy:/home/misi/work/my/ymery-cpp: main
thread '<unnamed>' panicked at src/lib.rs:429:5:
wgpu uncaptured error:
Validation Error

Caused by:
    In wgpuRenderPassEncoderEnd
      note: encoder = `<CommandBuffer-(0, 466, Vulkan)>`
    In a set_viewport command
    Viewport has invalid rect Rect { x: 0.0, y: 0.0, w: 1288.0, h: 743.0 }; origin and/or size is less than or equal to 0, and/or is not contained in the render target (1281, 725, 1)


note: run with `RUST_BACKTRACE=1` environment variable to display a backtrace
fatal runtime error: failed to initiate panic, error 5
[1]    182923 IOT instruction (core dumped)  build-webgpu/ymery-cli -p demo/layouts/editor -m app
```
