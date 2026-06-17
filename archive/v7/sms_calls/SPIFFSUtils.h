#ifndef SPIFFS_UTILS_H
#define SPIFFS_UTILS_H

#include <FS.h>
#include <SPIFFS.h>

// Initialize SPIFFS, optionally format if mount fails
bool spiffsBegin(bool formatIfFailed = true);

// Print root listing to Serial
void spiffsListRoot();

// Check exist
bool spiffsExists(const char *path);

// Read file into String (empty string on error)
String spiffsReadFile(const char *path);

// Write file if missing; returns true if file exists or was written
bool spiffsWriteFileIfMissing(const char *path, const String &content);

// Dump files under /data and print their contents
void spiffsDumpDataDir();
// Return a textual dump of /data directory (files + contents)
String spiffsDumpDataDirToString();
// Read first N lines of a file and return as String (empty if file missing/error)
String spiffsReadFirstLines(const char *path, int maxLines = 5);
// Read entire file into String (assumes SPIFFS already mounted). Does not create file.
String spiffsReadRaw(const char *path);
// Print first N lines of every file in SPIFFS root and return as a single string
String spiffsDumpRootFirstLines(int maxLines = 5);
// Return basic SPIFFS usage info (total/used)
String spiffsInfo();
// Find first existing path from a list of candidate paths. Returns empty if none found.
String spiffsFindFile(const char *candidates[], int count);

#endif
