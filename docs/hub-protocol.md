# Hub Protocol Boundary

The Hub consumes normalized orchestration messages from the Bridge. Hardware
transports, device protocol translation, authoritative device state, and stream
routing remain Bridge responsibilities.

## Hub ingress

The Hub accepts Bridge protocol envelopes at:

- `POST /hub/messages`

Supported inbound categories:

- `device.state`
- `device.event`
- `device.telemetry`
- `stream.available`
- `stream.closed`
- `analysis.result`
- `bridge.error`

Malformed envelopes are rejected with `400` and are logged without stopping the
Hub. The Hub caches normalized state for orchestration only; the Bridge remains
authoritative.

## Hub outbound

`HubClient` posts Hub Protocol envelopes to the Bridge protocol endpoint:

- `POST http://127.0.0.1:8080/hub/messages`

Startup sends a `subscription.request` with the Hub category subscriptions.
Analysis modules return intelligence with `analysis.result`. Automation modules
send device intent with `bridge.command` using `CommandBuilder`.

## Streams

Hub Protocol messages carry stream metadata only. `StreamRegistry` tracks stream
references advertised by `stream.available` and removes them on
`stream.closed`. Video payloads stay on standard streaming systems such as RTSP,
with WebRTC or HLS transport added outside the Hub Protocol later.
