#pragma once

// LittleFS initialization state, exposed in /status to distinguish a valid
// mount from a filesystem that's only apparently available.
enum class FilesystemHealth {
  UNKNOWN,
  READY,
  READY_AFTER_FORMAT,
  MOUNT_FAILED,
  ZERO_CAPACITY,
  PROBE_OPEN_FAILED,
  PROBE_WRITE_FAILED,
  PROBE_REMOVE_FAILED,
};

void setFilesystemHealth(FilesystemHealth health);
FilesystemHealth filesystemHealth();
const char* filesystemHealthText();
