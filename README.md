# PrintSpy Cam

Minimal ESP32 camera firmware. Boots, connects to WiFi (with a softAP setup
flow if no credentials are stored), and serves JPEG snapshots and an MJPEG
stream over HTTP. [PrintSpy](https://github.com/ccmpbll/printspy) points at
it directly as a standalone webcam URL — same as any mjpg-streamer or
camera-streamer source, no backend changes needed.

> Status: early scaffolding, not yet functional.

## Board support

| Board | Status |
|---|---|
| Freenove ESP32-S3 CAM | Primary target — pin mapping verified |
| [nulllaborg ESP32-S3-CAM](https://github.com/nulllaborg/esp32s3-cam) | Pin mapping verified against schematic — no hardware tested yet |

Both boards build in CI and are selectable via `idf.py menuconfig` under
"PrintSpy Cam Configuration".

## Building

Requires [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/get-started/index.html) >= 5.3.

```bash
idf.py set-target esp32s3
idf.py menuconfig           # select board under "PrintSpy Cam Configuration"
idf.py build
idf.py -p PORT flash monitor
```

Or via the Makefile shortcuts: `make build`, `make flash`, `make monitor`, `make menuconfig`.

## HTTP endpoints

| Method | Path | Description |
|---|---|---|
| GET | `/snapshot` | Single JPEG frame |
| GET | `/stream` | MJPEG stream |
| GET | `/` | Web admin UI |
| GET | `/api/status` | Firmware version, WiFi info, camera settings, uptime |
| POST | `/api/settings` | Update camera settings |
| POST | `/api/wifi` | Update WiFi credentials, reboot |
| POST | `/api/ota` | Firmware update upload, reboot |

## License

MIT — see [LICENSE](LICENSE).
