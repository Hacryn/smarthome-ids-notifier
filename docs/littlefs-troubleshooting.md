# LittleFS Unavailable: Diagnosis and Resolution

## Symptom

The `/status` command showed:

```text
LittleFS: 0/0 bytes, write errors: N
```

GPIO inputs were working correctly: level changes on `D4` and `D5` were detected.
However, events were not being saved to `log.jsonl` and, consequently, Telegram
notifications were not being sent.

## Configuration Used

- Board: Arduino Nano ESP32
- Core: Arduino ESP32 Boards `2.0.18-arduino.5`
- `Pin Numbering`: `By Arduino pin (default)`
- `Partition Scheme`: `With SPIFFS partition (advanced)`
- `USB Mode`: `Normal mode (TinyUSB)`

The firmware uses the `LittleFS` library, which requires a data partition of type
`spiffs`. The SPIFFS partition table on the Nano ESP32 contains a partition called
`spiffs` at address `0x610000`.

## Root Cause

The Nano ESP32 normally uses `dfu-util` for sketch uploads. In this configuration,
the upload transfers only the application firmware to address `0x10000`; it does
not update the partition table at address `0x8000`.

Consequently, changing in the IDE:

```text
Partition Scheme: With FAT partition (default)
```

to:

```text
Partition Scheme: With SPIFFS partition (advanced)
```

was not sufficient. The updated firmware continued to find the old `ffat` partition
on flash, while `LittleFS` was looking for a partition of type `spiffs`. The mount
failed, and even the format attempt could not resolve the issue because the wrong
partition table was being used.

## Added Diagnostics

The firmware verifies LittleFS initialization in three steps:

1. Mount the partition;
2. Check that `totalBytes()` is greater than zero;
3. Open, write, and remove a temporary file.

The status is visible in `/status` via the `LittleFS Diagnostics` line. The
resolved state is:

```text
LittleFS Diagnostics: reformatted, mounted and writable
```

or, if the filesystem was already valid:

```text
LittleFS Diagnostics: mounted and writable
```

## Resolution

The partition table generated with the SPIFFS schema was exported and then manually
written to address `0x8000` using `esptool`:

```powershell
& "$env:LOCALAPPDATA\Arduino15\packages\esp32\tools\esptool_py\4.5.1\esptool.exe" `
  --chip esp32s3 --port COM4 --baud 460800 `
  write_flash 0x8000 ".\Notifier.ino.partitions.bin"
```

Replace the COM port number with the one actually assigned by the board in bootloader
mode.

To enter bootloader mode:

1. Temporarily connect `B1` to `GND`;
2. Press `RST`;
3. Remove the `B1-GND` connection;
4. Run the `esptool` command.

After rewriting the partition table, the firmware found the `spiffs` partition,
formatted it as LittleFS, and was able to correctly write the event log and notification
files.

## Important Notes

- Rewriting the partition table is a flash operation: the content of the old FAT
  partition cannot be reused as LittleFS and may be lost during formatting.
- If you change the partition scheme again, the normal DFU upload may only update
  the application. You must verify that the partition table at address `0x8000`
  was actually updated.
- The `With SPIFFS partition (advanced)` selection in the IDE must remain active
  when generating the `.partitions.bin` file.
