#include "FilesystemHealth.h"

namespace {
FilesystemHealth g_health = FilesystemHealth::UNKNOWN;
}

void setFilesystemHealth(FilesystemHealth health) { g_health = health; }

FilesystemHealth filesystemHealth() { return g_health; }

const char* filesystemHealthText() {
  switch (g_health) {
    case FilesystemHealth::UNKNOWN:
      return "non verificato";
    case FilesystemHealth::READY:
      return "montato e scrivibile";
    case FilesystemHealth::READY_AFTER_FORMAT:
      return "riformattato, montato e scrivibile";
    case FilesystemHealth::MOUNT_FAILED:
      return "mount fallito anche dopo il format";
    case FilesystemHealth::ZERO_CAPACITY:
      return "mount riuscito ma capacita' zero";
    case FilesystemHealth::PROBE_OPEN_FAILED:
      return "apertura file di prova fallita";
    case FilesystemHealth::PROBE_WRITE_FAILED:
      return "scrittura file di prova fallita";
    case FilesystemHealth::PROBE_REMOVE_FAILED:
      return "rimozione file di prova fallita";
  }
  return "stato sconosciuto";
}
