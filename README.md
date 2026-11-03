# claude-desktop-buddy for M5StickC Plus2

> **This is a fork of
> [anthropics/claude-desktop-buddy](https://github.com/anthropics/claude-desktop-buddy)
> ported to the M5StickC Plus2** by [@Martin78b](https://github.com/Martin78b)
> ([repo](https://github.com/Martin78b/claude-desktop-buddy-plus2)). The upstream firmware targets the original
> M5StickC Plus and the `M5StickCPlus` library (AXP192 power chip). The Plus2
> has no AXP192, so this fork moves everything to
> [`M5Unified`](https://github.com/m5stack/M5Unified) behind a small
> hardware layer in `src/hal.h`.
>
> **What's different from upstream**
>
> - Runs on the **M5StickC Plus2** (ESP32-PICO-V3-02, 8MB flash). It does not
>   target the original Plus anymore; use upstream for that board.
> - 8MB partition table (`partitions_plus2_8MB.csv`): 3MB app + ~4.9MB
>   LittleFS for character packs.
> - A 19th pet, **Clawd**: the orange critter from Claude Code, drawn as
>   pixel art with all seven animations.
> - The settings menu shows the current pet's name while you cycle pets.
>
> **Plus2 limitations:** no battery-current or VBUS measurement (USB power
> is inferred from battery voltage ≥ 4.15V), and "turn off" from the menu
> only works on battery, because USB keeps the board powered.


Claude for macOS and Windows can connect Claude Cowork and Claude Code to
maker devices over BLE, so developers and makers can build hardware that
displays permission prompts, recent messages, and other interactions. We've
been impressed by the creativity of the maker community around Claude -
providing a lightweight, opt-in API is our way of making it easier to build
fun little hardware devices that integrate with Claude.

> **Building your own device?** You don't need any of the code here. See
> **[REFERENCE.md](REFERENCE.md)** for the wire protocol: Nordic UART
> Service UUIDs, JSON schemas, and the folder push transport.

As an example, we built a desk pet on ESP32 that lives off permission
approvals and interaction with Claude. It sleeps when nothing's happening,
wakes when sessions start, gets visibly impatient when an approval prompt is
waiting, and lets you approve or deny right from the device.

<p align="center">
  <img src="docs/device.jpg" alt="M5StickC Plus running the buddy firmware" width="500">
</p>

<p align="center">
  <img src="docs/device-plus2.jpg" alt="M5StickC Plus2 running this port, with the Clawd pixel-art pet" width="400">
  <br><em>This port running on an M5StickC Plus2, with the Clawd pet.</em>
</p>

## Hardware

This fork targets the **M5StickC Plus2** with the Arduino framework and
[`M5Unified`](https://github.com/m5stack/M5Unified) for the display, IMU,
buttons, RTC, buzzer and power management. Board-specific code lives in
`src/hal.h`:

| | M5StickC Plus (upstream) | M5StickC Plus2 (this fork) |
| --- | --- | --- |
| Library | `M5StickCPlus` | `M5Unified` |
| Power chip | AXP192 | none (GPIO4 power hold) |
| Backlight | AXP LDO2 | PWM via M5GFX |
| Red LED | GPIO10, active-low | GPIO19, active-high |
| Flash | 4MB | 8MB |
| Battery current / VBUS | measured | not available |

For the original M5StickC Plus, use
[upstream](https://github.com/anthropics/claude-desktop-buddy).

## Flashing

Install
[PlatformIO Core](https://docs.platformio.org/en/latest/core/installation/),
connect the Plus2 over USB-C, then:

```bash
pio run -e m5stickc-plus2 -t upload
```

If more than one serial port is present, pass it explicitly, for example
`--upload-port /dev/cu.usbserial-XXXX` (`ls /dev/cu.*` on macOS).

If you're starting from a previously-flashed device, wipe it first:

```bash
pio run -t erase && pio run -t upload
```

Once running, you can also wipe everything from the device itself: **hold A
→ settings → reset → factory reset → tap twice**.

## Pairing

To pair your device with Claude, first enable developer mode (**Help →
Troubleshooting → Enable Developer Mode**). Then, open the Hardware Buddy
window in **Developer → Open Hardware Buddy…**, click **Connect**, and pick
your device from the list. macOS will prompt for Bluetooth permission on
first connect; grant it.

<p align="center">
  <img src="docs/menu.png" alt="Developer → Open Hardware Buddy… menu item" width="420">
  <img src="docs/hardware-buddy-window.png" alt="Hardware Buddy window with Connect button and folder drop target" width="420">
</p>

Once paired, the bridge auto-reconnects whenever both sides are awake.

If discovery isn't finding the stick:

- Make sure it's awake (any button press)
- Check the stick's settings menu → bluetooth is on

## Controls

|                         | Normal               | Pet         | Info        | Approval    |
| ----------------------- | -------------------- | ----------- | ----------- | ----------- |
| **A** (front)           | next screen          | next screen | next screen | **approve** |
| **B** (right)           | scroll transcript    | next page   | next page   | **deny**    |
| **Hold A**              | menu                 | menu        | menu        | menu        |
| **Power** (left, short) | toggle screen off    |             |             |             |
| **Power** (left, ~6s)   | hard power off       |             |             |             |
| **Shake**               | dizzy                |             |             | —           |
| **Face-down**           | nap (energy refills) |             |             |             |

The screen auto-powers-off after 30s of no interaction (kept on while an
approval prompt is up). Any button press wakes it.

## ASCII pets

Nineteen pets, each with seven animations (sleep, idle, busy, attention,
celebrate, dizzy, heart): capybara, duck, goose, blob, cat, dragon, octopus,
owl, penguin, turtle, snail, ghost, axolotl, cactus, robot, rabbit, mushroom,
chonk and **clawd**. **Hold A → settings → pet row** cycles them; the row
shows the pet's name and a counter. Choice persists to NVS.

Clawd (`src/buddies/clawd.cpp`) is pixel art rather than ASCII. It is drawn
with `buddyFillPx()`, which scales and positions like the ASCII helpers, so
it's a template if you want to add other pixel-art pets.

Each preview below is the first idle frame. **ID** is the index stored in
NVS (`src/buddy.cpp` `SPECIES_TABLE`); **menu** is the counter shown in the
settings pet row. When a GIF character is installed it is added as one
extra entry after the last pet.

<table>
  <tr>
    <td align="center"><pre> n______n
( o    o )
(   oo   )
 `------&#x27;</pre><b>capybara</b><br>ID 0 · menu 1/19</td>
    <td align="center"><pre>  __
&lt;(o )___
 (  ._&gt;
  `--´</pre><b>duck</b><br>ID 1 · menu 2/19</td>
    <td align="center"><pre>  (&gt;
  ||
_(__)_
 ^^^^</pre><b>goose</b><br>ID 2 · menu 3/19</td>
    <td align="center"><pre> .--.
(o  o)
(    )
 `--`</pre><b>blob</b><br>ID 3 · menu 4/19</td>
  </tr>
  <tr>
    <td align="center"><pre> /\_/\
( o   o )
(  w   )
(&quot;)_(&quot;)</pre><b>cat</b><br>ID 4 · menu 5/19</td>
    <td align="center"><pre> /^\  /^\
&lt;  o    o &gt;
(   ww   )
 `-vvvv-&#x27;</pre><b>dragon</b><br>ID 5 · menu 6/19</td>
    <td align="center"><pre> .----.
( o  o )
(______)
/\/\/\/\</pre><b>octopus</b><br>ID 6 · menu 7/19</td>
    <td align="center"><pre> /\  /\
((O)(O))
(  &gt;&lt;  )
 `----&#x27;</pre><b>owl</b><br>ID 7 · menu 8/19</td>
  </tr>
  <tr>
    <td align="center"><pre>  .---.
 ( o&gt;o )
/(     )\
 `-----`
  J   L</pre><b>penguin</b><br>ID 8 · menu 9/19</td>
    <td align="center"><pre>  _,--._
 ( o    o)
/[______]\
 ``    ``</pre><b>turtle</b><br>ID 9 · menu 10/19</td>
    <td align="center"><pre> \\  /
   .--.
 _( oo )_
(___@@___)
 ~~~~~~~~</pre><b>snail</b><br>ID 10 · menu 11/19</td>
    <td align="center"><pre> .----.
( o    o )
|   __   |
~`~``~`~</pre><b>ghost</b><br>ID 11 · menu 12/19</td>
  </tr>
  <tr>
    <td align="center"><pre>}~(______)~{
}~( o  o )~{
  ( .--. )
  (_/  \_)</pre><b>axolotl</b><br>ID 12 · menu 13/19</td>
    <td align="center"><pre>n  ____  n
| |o  o| |
|_|    |_|
  |    |</pre><b>cactus</b><br>ID 13 · menu 14/19</td>
    <td align="center"><pre> .[||].
[ o    o ]
[ ==== ]
`------&#x27;</pre><b>robot</b><br>ID 14 · menu 15/19</td>
    <td align="center"><pre>  (\_/)
 ( o o )
=(  v  )=
 (&quot;)_(&quot;)</pre><b>rabbit</b><br>ID 15 · menu 16/19</td>
  </tr>
  <tr>
    <td align="center"><pre> .-o-OO-o-.
(__________)
   |o   o|
   |____|</pre><b>mushroom</b><br>ID 16 · menu 17/19</td>
    <td align="center"><pre> /\____/\
( o    o )
(   ..   )
 `------&#x27;</pre><b>chonk</b><br>ID 17 · menu 18/19</td>
    <td align="center"><pre> ▐▛███▜▌ 
▝▜█████▛▘
  ▘▘ ▝▝  </pre><b>clawd</b><br>ID 18 · menu 19/19</td>
  </tr>
</table>

Clawd is shown here with the block glyphs from Claude Code's banner; on the
device it is drawn as orange (`#D97757`) pixel art.

## GIF pets

If you want a custom GIF character instead of an ASCII buddy, drag a
character pack folder onto the drop target in the Hardware Buddy window. The
app streams it over BLE and the stick switches to GIF mode live. **Settings
→ reset → delete char** reverts to ASCII mode.

A character pack is a folder with `manifest.json` and 96px-wide GIFs:

```json
{
  "name": "bufo",
  "colors": {
    "body": "#6B8E23",
    "bg": "#000000",
    "text": "#FFFFFF",
    "textDim": "#808080",
    "ink": "#000000"
  },
  "states": {
    "sleep": "sleep.gif",
    "idle": ["idle_0.gif", "idle_1.gif", "idle_2.gif"],
    "busy": "busy.gif",
    "attention": "attention.gif",
    "celebrate": "celebrate.gif",
    "dizzy": "dizzy.gif",
    "heart": "heart.gif"
  }
}
```

State values can be a single filename or an array. Arrays rotate: each
loop-end advances to the next GIF, useful for an idle activity carousel so
the home screen doesn't loop one clip forever.

GIFs are 96px wide; height up to ~140px stays on a 135×240 portrait screen.
Crop tight to the character — transparent margins waste screen and shrink
the sprite. `tools/prep_character.py` handles the resize: feed it source
GIFs at any sizes and it produces a 96px-wide set where the character is the
same scale in every state.

The whole folder must fit under 1.8MB —
`gifsicle --lossy=80 -O3 --colors 64` typically cuts 40–60%.

See `characters/bufo/` for a working example.

If you're iterating on a character and would rather skip the BLE round-trip,
`tools/flash_character.py characters/bufo` stages it into `data/` and runs
`pio run -t uploadfs` directly over USB.

> **Apple Silicon note:** PlatformIO's bundled `mklittlefs` is an x86_64
> binary. Without Rosetta, `uploadfs` fails with `Bad CPU type in
> executable`. Install Rosetta (`softwareupdate --install-rosetta`) or use
> the BLE drop target instead.

## The seven states

| State       | Trigger                     | Feel                        |
| ----------- | --------------------------- | --------------------------- |
| `sleep`     | bridge not connected        | eyes closed, slow breathing |
| `idle`      | connected, nothing urgent   | blinking, looking around    |
| `busy`      | sessions actively running   | sweating, working           |
| `attention` | approval pending            | alert, **LED blinks**       |
| `celebrate` | level up (every 50K tokens) | confetti, bouncing          |
| `dizzy`     | you shook the stick         | spiral eyes, wobbling       |
| `heart`     | approved in under 5s        | floating hearts             |

## Project layout

```
src/
  hal.h          — Plus2 hardware layer over M5Unified (power, LED, IMU…)
  main.cpp       — loop, state machine, UI screens
  buddy.cpp      — ASCII species dispatch + render helpers
  buddies/       — one file per species, seven anim functions each
                   (clawd.cpp is pixel art via buddyFillPx)
  ble_bridge.cpp — Nordic UART service, line-buffered TX/RX
  character.cpp  — GIF decode + render
  data.h         — wire protocol, JSON parse
  xfer.h         — folder push receiver
  stats.h        — NVS-backed stats, settings, owner, species choice
characters/      — example GIF character packs
tools/           — generators and converters
partitions_plus2_8MB.csv — 3MB app + ~4.9MB LittleFS layout
```

## Availability

The BLE API is only available when the desktop apps are in developer mode
(**Help → Troubleshooting → Enable Developer Mode**). It's intended for
makers and developers and isn't an officially supported product feature.

## Credits

Original firmware, protocol and character art by Anthropic in
[anthropics/claude-desktop-buddy](https://github.com/anthropics/claude-desktop-buddy).
The M5StickC Plus2 port and the Clawd pet are by
[Martin (@Martin78b)](https://github.com/Martin78b) in
[Martin78b/claude-desktop-buddy-plus2](https://github.com/Martin78b/claude-desktop-buddy-plus2).
See `LICENSE`.
