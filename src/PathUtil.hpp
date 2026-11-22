#pragma once

#include <QString>

namespace PathUtil {

enum class Kind { File, Directory, Missing };

// Turns a path as typed, pasted, dropped or passed on the command line into
// an absolute path with '/' separators. Pure string transformation: reads only
// environment variables, the home folder and baseDir, never the file system.
// A relative path is resolved against baseDir; with an empty baseDir it stays
// relative. Returns an empty string for empty input and non-file URIs.
QString normalizePath(const QString &input, const QString &baseDir = QString());

// File system check. Relative and empty paths are Missing.
Kind classify(const QString &path);

// True when both paths point to the same item: equal canonical paths, or equal
// cleaned paths when an item does not exist. Case-insensitive on Windows.
bool sameItem(const QString &a, const QString &b);

}  // namespace PathUtil
