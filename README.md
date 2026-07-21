# MeshCore Experimental Firmware

> [!WARNING]
> **Use the `experimental-build` branch. Do not use this repository's `main` or `dev` branches for firmware.** All output firmware files from this repository are produced from `experimental-build`.

## An experimental reliability fork

This repository is becoming an experimental fork of the [original MeshCore firmware repository](https://github.com/meshcore-dev/MeshCore). It is not intended to compete with or replace the original project. Finished changes developed here are submitted, or are being prepared for submission, to the original repository. If they are accepted upstream, the same improvements should eventually become available in the main MeshCore codebase.

The purpose of this fork is to improve **mesh and device reliability**. It can move more quickly, fail fast, and test and adjust based on real-world results. Some testing is performed here, but this code has fundamentally received less testing than firmware from the original repository. Expect a higher risk of regressions and be prepared to report problems or return to upstream firmware.

The scope of `experimental-build` is deliberately narrow:

- Improve message delivery, routing, congestion behaviour, radio diagnostics, recovery, and device stability.
- Make reliability problems easier to observe, reproduce, and correct.
- Add no general-purpose features unless they directly make the mesh easier to use or operate reliably.
- Produce firmware exclusively from `experimental-build`; `main` and `dev` exist for upstream synchronization and development history, not for users to flash.

## Included reliability improvements

The following finished improvement branches are collected in `experimental-build`. Branches marked `UNFINISHED` (including the existing misspelled `UNFINSIHED` prefix) are intentionally excluded. `main`, `dev`, and `experimental-build` are baseline or integration branches, so they are not listed as individual improvements.

| Improvement title | Branch name | Description of the problem | Description of the fix |
| --- | --- | --- | --- |
| Radio receive diagnostics | `add-radio-rx-diagnostics` | Packet reception failures were grouped too broadly, making radio and link problems difficult to distinguish in the field. | Adds separate RadioLib receive-error counters and exposes them through compact diagnostic output. |
| Authenticated repeater time synchronization | `add-repeater-auth-time-sync` | Repeater time updates lacked strong source authentication, channel binding, replay protection, multi-source validation, and deployable defaults. | Adds signed, channel-bound time updates; replay and clock-step checks; multiple configured sources; and compile-time repeater defaults. |
| Configurable repeater build defaults | `add-repeater-build-defaults` | Operators building many repeaters had to patch source or configure each device manually to apply deployment-specific defaults. | Adds validated compile-time settings for repeater identity, radio, location, and related preferences while preserving upstream defaults when no overrides are supplied. |
| Contention-aware ACK scheduling | `fix-ack-contention-scheduling` | ACKs from several nodes could be transmitted at nearly the same time, collide on air, and create avoidable duplicate traffic. | Schedules ACKs with airtime-, path-, and queue-aware jitter and treats multipart/final ACK forms as one contended relay transmission. |
| Congestion-aware ACK retry control | `fix-ack-retry-backoff` | A retry could overlap its still-queued original transmission, begin its timeout too early, or add load while the mesh was already congested. | Tracks the logical send operation, starts the ACK window after local transmission, coalesces premature retries, and applies bounded pressure-aware backoff and timeout estimates. |
| Direct ACK amplification prevention | `fix-direct-ack-amplification` | Relays could multiply redundant direct ACK copies, consuming airtime without improving logical delivery. | Deduplicates equivalent ACK forms, reuses the received packet while forwarding, and prevents relay copy settings from amplifying direct ACK traffic. |
| Learned return paths for flood replies | `fix-flood-reply-return-paths` | Replies to flooded requests could ignore an already learned route and generate unnecessary flood traffic. | Returns eligible replies over the learned direct path, reducing airtime and improving reply reliability. |
| Bounded forced-transmit CAD timeout | `fix-forced-tx-cad-timeout` | Continuous channel activity could defer a queued transmission indefinitely, while oversized timeout values could overflow internal timing. | Adds a configurable CAD deferral deadline, permits transmission after expiry, and rejects timeout values that cannot be represented safely. |
| Reliable nRF52 OTA reset | `fix-nrf52-ota-reset` | OTA/DFU handoff state could be lost during reset, or an abandoned OTA session could leave a device stuck; oversized timeout settings could also overflow. | Preserves the BLE DFU handoff marker, resets into the nRF52 bootloader reliably, adds an OTA idle timeout, and validates the configured timeout range. |
| nRF52 radio initialization recovery | `fix-nrf52-radio-init-recovery` | A transient SX126x initialization failure could leave an nRF52 device without a working radio and with insufficient evidence of the preceding boot failure. | Retries recovery from transient radio initialization failures and retains current and previous boot diagnostics. |
| Brownout and power-failure recovery | `improve-brownout-recovery` | Ambiguous USB/battery readings and power-fail handling could cause false brownout decisions, boot loops, or interfere with OTA on nRF52 devices. | Validates power-source readings, hardens POF lifecycle handling, preserves diagnostic labels, and restores board-specific boot protection. |
| RSSI and noise-floor stability | `rssi_and_noise_floor_improvements` | Startup outliers, clamped samples, calibration feedback, and stale radio status could corrupt noise-floor estimates and carrier-sense decisions. | Caches per-packet metrics, filters implausible samples, rate-limits and bounds calibration, preserves trusted values during refresh, and exposes calibration diagnostics. |
| Mesh and MAC statistics instrumentation | `stats-instrumentation` | Queue pressure, duplicate traffic, airtime use, and MAC behaviour were difficult to inspect, which made reliability faults harder to diagnose. | Adds repeater MAC counters and concise CLI reports, and corrects/tests packet-queue peeking used by the instrumentation. |

## About MeshCore

MeshCore is a lightweight, portable C++ library that enables multi-hop packet routing for embedded projects using LoRa and other packet radios. It is designed for developers who want to create resilient, decentralized communication networks that work without the internet.

## 🔍 What is MeshCore?

MeshCore now supports a range of LoRa devices, allowing for easy flashing without the need to compile firmware manually. Users can flash a pre-built binary using tools like Adafruit ESPTool and interact with the network through a serial console.
MeshCore provides the ability to create wireless mesh networks, similar to Meshtastic and Reticulum but with a focus on lightweight multi-hop packet routing for embedded projects. Unlike Meshtastic, which is tailored for casual LoRa communication, or Reticulum, which offers advanced networking, MeshCore balances simplicity with scalability, making it ideal for custom embedded solutions, where devices (nodes) can communicate over long distances by relaying messages through intermediate nodes. This is especially useful in off-grid, emergency, or tactical situations where traditional communication infrastructure is unavailable.

## ⚡ Key Features

* Multi-Hop Packet Routing
  * Devices can forward messages across multiple nodes, extending range beyond a single radio's reach.
  * Supports up to a configurable number of hops to balance network efficiency and prevent excessive traffic.
  * Nodes use fixed roles where "Companion" nodes are not repeating messages at all to prevent adverse routing paths from being used.
* Supports LoRa Radios – Works with Heltec, RAK Wireless, and other LoRa-based hardware.
* Decentralized & Resilient – No central server or internet required; the network is self-healing.
* Low Power Consumption – Ideal for battery-powered or solar-powered devices.
* Simple to Deploy – Pre-built example applications make it easy to get started.

## 🎯 What Can You Use MeshCore For?

* Off-Grid Communication: Stay connected even in remote areas.
* Emergency Response & Disaster Recovery: Set up instant networks where infrastructure is down.
* Outdoor Activities: Hiking, camping, and adventure racing communication.
* Tactical & Security Applications: Military, law enforcement, and private security use cases.
* IoT & Sensor Networks: Collect data from remote sensors and relay it back to a central location.

## 🚀 How to Get Started

- Watch the [MeshCore QuickStart Playlist](https://www.youtube.com/watch?v=iaFltojJrAc&list=PLshzThxhw4O4WU_iZo3NmNZOv6KMrUuF9) by The Comms Channel
- Watch the [MeshCore Technical Presentation](https://www.youtube.com/watch?v=OwmkVkZQTf4) by Liam Cottle.
- Read through our [Frequently Asked Questions](./docs/faq.md) and [Documentation](https://docs.meshcore.io).
- Flash the MeshCore firmware on a supported device.
- Connect with a supported client.

For developers:

- Install [PlatformIO](https://docs.platformio.org) in [Visual Studio Code](https://code.visualstudio.com).
- Clone and open the MeshCore repository in Visual Studio Code.
- See the example applications you can modify and run:
  - [Companion Radio](./examples/companion_radio) - For use with an external chat app, over BLE, USB or Wi-Fi.
  - [KISS Modem](./examples/kiss_modem) - Serial KISS protocol bridge for host applications. ([protocol docs](./docs/kiss_modem_protocol.md))
  - [Simple Repeater](./examples/simple_repeater) - Extends network coverage by relaying messages.
  - [Simple Room Server](./examples/simple_room_server) - A simple BBS server for shared Posts.
  - [Simple Secure Chat](./examples/simple_secure_chat) - Secure terminal based text communication between devices.
  - [Simple Sensor](./examples/simple_sensor) - Remote sensor node with telemetry and alerting.

The Simple Secure Chat example can be interacted with through the Serial Monitor in Visual Studio Code, or with a Serial USB Terminal on Android.

## ⚡️ MeshCore Flasher

We have prebuilt firmware ready to flash on supported devices.

- Launch https://meshcore.io/flasher
- Select a supported device
- Flash one of the firmware types:
  - Companion, Repeater or Room Server
- Once flashing is complete, you can connect with one of the MeshCore clients below.

## 📱 MeshCore Clients

**Companion Firmware**

The companion firmware can be connected to via BLE, USB or Wi-Fi depending on the firmware type you flashed.

- Web: https://app.meshcore.nz
- Android: https://play.google.com/store/apps/details?id=com.liamcottle.meshcore.android
- iOS: https://apps.apple.com/us/app/meshcore/id6742354151?platform=iphone
- NodeJS: https://github.com/liamcottle/meshcore.js
- Python: https://github.com/fdlamotte/meshcore-cli

**Repeater and Room Server Firmware**

The repeater and room server firmware can be set up via USB in the web config tool.

- https://config.meshcore.io

They can also be managed via LoRa in the mobile app by using the Remote Management feature.

## 🛠 Hardware Compatibility

MeshCore is designed for devices listed in the [MeshCore Flasher](https://meshcore.io/flasher)

## 📜 License

MeshCore is open-source software released under the MIT License. You are free to use, modify, and distribute it for personal and commercial projects.

## Contributing

Please submit PR's using 'dev' as the base branch!
For minor changes just submit your PR and we'll try to review it, but for anything more 'impactful' please open an Issue first and start a discussion. It is better to sound out what it is you want to achieve first, and try to come to a consensus on what the best approach is, especially when it impacts the structure or architecture of this codebase.

Here are some general principles you should try to adhere to:
* Keep it simple. Please, don't think like a high-level lang programmer. Think embedded, and keep code concise, without any unnecessary layers.
* No dynamic memory allocation, except during setup/begin functions.
* Use the same brace and indenting style that's in the core source modules. (A .clang-format is probably going to be added soon, but please do NOT retroactively re-format existing code. This just creates unnecessary diffs that make finding problems harder)

Help us prioritize! Please react with thumbs-up to issues/PRs you care about most. We look at reaction counts when planning work.

### Running unit tests

To run unit tests, run the following command:

```bash
pio test --environment native --verbose
```

## Road-Map / To-Do

There are a number of fairly major features in the pipeline, with no particular time-frames attached yet. In very rough chronological order:
- [X] Companion radio: UI redesign
- [X] Repeater + Room Server: add ACL's (like Sensor Node has)
- [X] Standardise Bridge mode for repeaters
- [ ] Repeater/Bridge: Standardise the Transport Codes for zoning/filtering
- [X] Core + Repeater: enhanced zero-hop neighbour discovery
- [ ] Core: round-trip manual path support
- [ ] Companion + Apps: support for multiple sub-meshes (and 'off-grid' client repeat mode)
- [ ] Core + Apps: support for LZW message compression
- [ ] Core: dynamic CR (Coding Rate) for weak vs strong hops
- [ ] Core: new framework for hosting multiple virtual nodes on one physical device
- [ ] V2 protocol spec: discussion and consensus around V2 packet protocol, including path hashes, new encryption specs, etc

## 📞 Get Support

- Report bugs and request features on the [GitHub Issues](https://github.com/ripplebiz/MeshCore/issues) page.
- Find additional guides and components on [my site](https://buymeacoffee.com/ripplebiz).
- Join [MeshCore Discord](https://meshcore.gg) to chat with the developers and get help from the community.
