#include "SPIFFSUtils.h"
#include <Arduino.h>

bool spiffsBegin(bool formatIfFailed) {
  if (!SPIFFS.begin(formatIfFailed)) {
    Serial.println("SPIFFS.begin failed");
    return false;
  }
  return true;
}

void spiffsListRoot() {
  Serial.println("Listing SPIFFS files:");
  File root = SPIFFS.open("/");
  if (!root) {
    Serial.println("Failed to open SPIFFS root");
    return;
  }
  if (!root.isDirectory()) {
    Serial.println("SPIFFS root is not a directory");
    root.close();
    return;
  }
  File file = root.openNextFile();
  while (file) {
    Serial.print("  "); Serial.print(file.name()); Serial.print(" (" ); Serial.print(file.size()); Serial.println(")");
    file = root.openNextFile();
  }
  root.close();
}

bool spiffsExists(const char *path) {
  return SPIFFS.exists(path);
}

String spiffsReadFile(const char *path) {
  String out = "";
  if (!SPIFFS.begin(true)) return out;
  File f = SPIFFS.open(path, FILE_READ);
  if (!f) return out;
  while (f.available()) out += (char)f.read();
  f.close();
  return out;
}

bool spiffsWriteFileIfMissing(const char *path, const String &content) {
  if (!SPIFFS.begin(true)) return false;
  if (SPIFFS.exists(path)) return true;
  File f = SPIFFS.open(path, FILE_WRITE);
  if (!f) return false;
  f.print(content);
  f.close();
  return true;
}

void spiffsDumpDataDir() {
  Serial.println("\n--- Dumping /data directory contents ---");
  if (!SPIFFS.exists("/data")) {
    Serial.println("/data directory not present in SPIFFS");
    return;
  }
  File d = SPIFFS.open("/data");
  if (!d) { Serial.println("Failed to open /data directory"); return; }
  if (!d.isDirectory()) { Serial.println("/data exists but is not a directory"); d.close(); return; }
  File f = d.openNextFile();
  while (f) {
    Serial.print("File: "); Serial.print(f.name()); Serial.print(" size="); Serial.println(f.size());
    Serial.println("--- begin file content ---");
    while (f.available()) {
      const size_t BUF_SZ = 128;
      char buf[BUF_SZ+1];
      size_t r = f.readBytes(buf, BUF_SZ);
      buf[r] = '\0';
      Serial.print(buf);
    }
    Serial.println();
    Serial.println("--- end file content ---\n");
    f = d.openNextFile();
  }
  d.close();
}

String spiffsDumpDataDirToString() {
  String out = "";
  // Dump root entries
  out += "--- SPIFFS root files ---\n";
  File root = SPIFFS.open("/");
  if (!root) {
    out += "Failed to open SPIFFS root\n";
  } else {
    File f = root.openNextFile();
    while (f) {
      out += "File: "; out += f.name(); out += " size="; out += String(f.size()); out += "\n";
      out += "--- begin file content ---\n";
      while (f.available()) {
        const size_t BUF_SZ = 128;
        char buf[BUF_SZ+1];
        size_t r = f.readBytes(buf, BUF_SZ);
        buf[r] = '\0';
        out += String(buf);
      }
      out += "\n--- end file content ---\n\n";
      f = root.openNextFile();
    }
    root.close();
  }

  // Also dump /data directory if present (some uploaders create files under /data)
  out += "--- /data directory ---\n";
  if (!SPIFFS.exists("/data")) {
    out += "/data directory not present in SPIFFS\n";
    return out;
  }
  File d = SPIFFS.open("/data");
  if (!d) { out += "Failed to open /data directory\n"; return out; }
  if (!d.isDirectory()) { out += "/data exists but is not a directory\n"; d.close(); return out; }
  File g = d.openNextFile();
  while (g) {
    out += "File: "; out += g.name(); out += " size="; out += String(g.size()); out += "\n";
    out += "--- begin file content ---\n";
    while (g.available()) {
      const size_t BUF_SZ = 128;
      char buf[BUF_SZ+1];
      size_t r = g.readBytes(buf, BUF_SZ);
      buf[r] = '\0';
      out += String(buf);
    }
    out += "\n--- end file content ---\n\n";
    g = d.openNextFile();
  }
  d.close();
  return out;
}

String spiffsReadFirstLines(const char *path, int maxLines) {
  String out = "";
  if (!SPIFFS.exists(path)) return out;
  File f = SPIFFS.open(path, FILE_READ);
  if (!f) return out;
  int lines = 0;
  String line = "";
  while (f.available() && lines < maxLines) {
    char c = f.read();
    if (c == '\n') {
      out += line + "\n";
      line = "";
      lines++;
    } else if (c != '\r') {
      line += c;
    }
  }
  // if file ended without newline and we have content
  if (line.length() && lines < maxLines) out += line + "\n";
  f.close();
  return out;
}

String spiffsInfo() {
  String out = "";
  // Use totalBytes/usedBytes which are available on ESP32 SPIFFS
  size_t total = 0;
  size_t used = 0;
#if defined(SPIFFS) && defined(ESP32)
  total = SPIFFS.totalBytes();
  used = SPIFFS.usedBytes();
#else
  // Fallback: leave zeros
#endif
  out += "Total bytes: "; out += String(total); out += "\n";
  out += "Used bytes: "; out += String(used); out += "\n";
  return out;
}

String spiffsFindFile(const char *candidates[], int count) {
  for (int i = 0; i < count; ++i) {
    if (SPIFFS.exists(candidates[i])) return String(candidates[i]);
  }
  return String("");
}

String spiffsDumpRootFirstLines(int maxLines) {
  String out = "";
  File root = SPIFFS.open("/");
  if (!root) return out;
  File f = root.openNextFile();
  while (f) {
    out += "File: "; out += f.name(); out += "\n";
    // read first maxLines
    int lines = 0;
    String line = "";
    while (f.available() && lines < maxLines) {
      char c = f.read();
      if (c == '\n') {
        out += line + "\n";
        line = "";
        lines++;
      } else if (c != '\r') {
        line += c;
      }
    }
    if (line.length() && lines < maxLines) out += line + "\n";
    out += "---\n";
    f = root.openNextFile();
  }
  root.close();
  return out;
}

String spiffsReadRaw(const char *path) {
  String out = "";
  if (!SPIFFS.exists(path)) return out;
  File f = SPIFFS.open(path, FILE_READ);
  if (!f) return out;
  const size_t CHUNK = 128;
  char buf[CHUNK+1];
  while (f.available()) {
    size_t r = f.readBytes(buf, CHUNK);
    buf[r] = '\0';
    out += String(buf);
    // yield occasionally to avoid watchdog
    delay(1);
  }
  f.close();
  return out;
}
