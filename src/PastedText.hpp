#pragma once

#include <QDateTime>
#include <QString>

namespace PastedText {

enum class Slot { Left, Middle, Right };
enum class LineEnding { Lf, Crlf };

// Folder for pasted-text files: %TEMP%/diffpick on Windows, ~/.cache/diffpick on
// Linux. Not /tmp, because Flatpak Meld cannot see the host's /tmp.
QString defaultDir();

// Writes text (UTF-8, no BOM, bytes exactly as given) to
// <dir>/<YYYYMMDD-HHMMSS-mmm>/Text (left|middle|right).txt and returns that path
// with '/' separators, or an empty string on failure. Meld shows the file name
// in its pane and tab titles.
QString save(const QString &dir, Slot slot, const QString &text, const QDateTime &now);

// Line ending a text should be saved with after editing: the one the original
// uses (CRLF wins when both occur), the OS convention when it has no line breaks.
LineEnding lineEndingFor(const QString &original);

// Converts every "\r\n" and "\n" in text to the given line ending.
QString withLineEnding(const QString &text, LineEnding lineEnding);

// Deletes the time-stamped subfolders of dir that are older than cutoff and
// returns how many were removed. Other files and folders are left alone. Pass a
// date in the future to delete all pasted texts.
int removeOlderThan(const QString &dir, const QDateTime &cutoff);

}  // namespace PastedText
