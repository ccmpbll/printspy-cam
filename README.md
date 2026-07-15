# PrintSpy Cam

Minimal ESP32 camera firmware. Boots, connects to WiFi (with a softAP setup
flow if no credentials are stored), and serves JPEG snapshots and an MJPEG
stream over HTTP. [PrintSpy](https://github.com/ccmpbll/printspy) points at
it directly as a standalone webcam URL — same as any mjpg-streamer or
camera-streamer source, no backend changes needed.

## Board support

| Board | Status |
|---|---|
| Freenove ESP32-S3 CAM | Verified on hardware, built + released in CI |
| [nulllaborg ESP32-S3-CAM](https://github.com/nulllaborg/esp32s3-cam) | Verified on hardware, built + released in CI |
| Espressif ESP32-S3-EYE | Pin mapping unverified, no hardware tested — not built in CI |

Each supported board has its own config at `board-configs/<board>.sdkconfig`
(pin selection, flash size, any board-specific Kconfig). Adding a new board
needs a pin header in `main/boards/` plus a matching `board-configs/*.sdkconfig`.

## Building

Requires [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/get-started/index.html) >= 5.3.

```bash
make build BOARD=nulllab_esp32s3_cam   # or freenove_s3
make flash BOARD=nulllab_esp32s3_cam
make monitor
```

Switching `BOARD` on an already-configured build directory needs `make clean` first.

## HTTP endpoints

| Method | Path | Description |
|---|---|---|
| GET | `/snapshot` | Single JPEG frame |
| GET | `/stream` | MJPEG stream |
| GET | `/` | Web admin UI |
| GET | `/api/status` | Firmware version, WiFi info, chip temp, uptime |
| GET | `/api/settings` | Current camera settings |
| POST | `/api/settings` | Update camera settings |
| GET | `/api/logs` | Recent log lines |
| POST | `/api/wifi` | Update WiFi credentials, reboot |
| POST | `/api/ota` | Firmware update upload, reboot |

## License

MIT — see [LICENSE](LICENSE).
