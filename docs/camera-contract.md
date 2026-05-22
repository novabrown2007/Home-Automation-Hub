# Legacy Camera Contract

The new Hub communication boundary is documented in `docs/hub-protocol.md`.
Camera stream discovery should move through `stream.available` Hub Protocol
metadata. The older HTTP registration notes below describe the disabled legacy
camera path and must not be extended with new hardware or transport logic.

## Ownership

- Bridge owns hardware communication.
- Hub owns analysis, mode logic, and trigger generation.
- UI consumes Bridge state and notifications.

## Accepted camera registration payload

- `POST /camera/feed/<camera_id>`

```json
{
  "source_feed_url": "http://camera-host/frame.jpg",
  "raw_feed_url": "http://127.0.0.1:8080/camera/stream/bedroomcamera",
  "stream_url": "http://127.0.0.1:8080/camera/stream/bedroomcamera",
  "frame_url": "http://127.0.0.1:8080/camera/frame/bedroomcamera",
  "audio_feed_url": "",
  "normalized_frame_format": "jpeg",
  "resolution": "1080p"
}
```

## Current ingest rules

- `frame_url` is required for Hub-side processing and must return JPEG image bytes.
- `audio_feed_url` is reserved for strange-sound detection and may be empty.
- Current motion detection operates on proxied frame bytes pulled from `frame_url`.
- Current Bridge-side normalization supports camera sources in `mjpg`/`mjpeg` and `yuy2`.
- `https://`, `rtsp://`, and long-lived MJPEG streaming are not implemented yet.

## Modes

- `home`: feed visible, motion enabled
- `away`: feed visible, motion + face + sound enabled
- `night`: feed visible, motion + face + sound enabled
- `privacy`: feed retained, analyzers disabled

## Trigger path

- Hub queues local notifications.
- Hub relays notifications to Bridge `POST /notifications`.
- Bridge stores and logs notifications for later UI retrieval.

## Notification payload

```json
{
  "source": "hub",
  "severity": "warning",
  "category": "camera",
  "title": "Motion detected",
  "message": "Motion detected for bedroomcamera.",
  "device_id": "bedroomcamera"
}
```
