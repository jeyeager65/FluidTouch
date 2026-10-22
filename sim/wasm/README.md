# FluidTouch in a web page (Emscripten)

The same firmware sources as the desktop simulator (`src/` + `sim/src`), built
with Emscripten for a browser page. Only the platform edges differ (`sim/wasm`):

| Desktop sim                          | Browser build                                                    |
|--------------------------------------|------------------------------------------------------------------|
| SDL window (`display_driver_sim`)     | LVGL draws into an 800x480 RGB565 framebuffer the page copies    |
| mouse = touch (`touch_driver_sim`)    | `ft_touch(x, y, down)` from the page                             |
| sockets (`net.cpp`)                   | `net_wasm.cpp`: the WebSocket's TCP bytes go to the page          |
| `sim_main.cpp`                        | `wasm_main.cpp`: `setup()` / `loop()` on a pthread                |
| `ESP.restart()` relaunches the exe    | calls the page's `onRestart`                                      |

The WebSocket client is still ArduinoWebsockets: it writes a raw byte stream
and the page joins that to a controller in the same page - FluidNC's own wasm
build, through its virtual connections (`fluidnc_vconn_open/send/close` into
FluidNC's web server). The handshake and frames are the real ones.

On the first boot (settings live in MEMFS until the page reloads) it adds one
machine, **LowRider (browser)** (WiFi, `192.168.0.1:80` = FluidNC wasm's
virtual address), selected and loaded without the machine list.

## Build

Needs emsdk (`emsdk install latest`, `emsdk activate latest`) and MSYS2's
cmake + ninja (same packages as the desktop sim):

```powershell
.\sim\wasm\build.ps1 -Emsdk D:\emsdk     # -> sim/wasm/build/fluidtouch.js + fluidtouch.wasm
```

## Page API

`fluidtouch.js` defines `createFluidTouch(options) -> Promise<Module>`
(MODULARIZE; pthreads, so the page must be cross-origin isolated).

Options the page supplies:

- `ftNetOpen(port) -> id` (0 = refused), `ftNetSend(id, Uint8Array)`, `ftNetClose(id)`
- `onRestart()`, `onPowerOff()`, `print` / `printErr` (the serial log)

Exports: `_ft_framebuffer()` (pointer to 800x480 RGB565), `_ft_frame()`
(counter, changes after each redraw), `_ft_width()` / `_ft_height()`,
`_ft_touch(x, y, down)`, `_ft_net_data(id, ptr, len)` / `_ft_net_closed(id)`
(the page hands received bytes in: `_malloc`, `HEAPU8.set`, call, `_free`).

Not bridged yet: HTTP uploads (`HTTPClient` / `WiFiClient` fail to connect),
the serial console input, ESP-NOW / UART connection types.

Used by the LowRider 4 Sandbox page (`web/js/fluidtouch_sim.js` there).
