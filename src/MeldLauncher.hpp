#pragma once

#include <QString>
#include <QStringList>

namespace MeldLauncher {

// How to start Meld: a program and the arguments that go before diffpick's own
// (e.g. "flatpak" with {"run", "org.gnome.meld"}).
struct Command
{
    QString program;
    QStringList args;
    bool isValid() const { return !program.isEmpty(); }
};

// Finds Meld in the order from the spec: customCommand (from Settings, empty =
// auto-detect), PATH, known install folders on Windows, Flatpak on Linux.
// Returns an invalid Command when Meld is not found.
Command findMeld(const QString &customCommand);

enum class Problem { None, WrongCount, Missing };

struct Validation
{
    Problem problem = Problem::None;
    int index = -1;  // item that is missing
    bool hasDuplicate = false;
};

// Requires 2-3 existing items and flags repeated paths for confirmation.
Validation validate(const QStringList &items);

// "meld A B" or "meld LEFT MIDDLE RIGHT".
QStringList compareArgs(const QStringList &items);

// "meld --diff F X --diff F Y ...", one tab per picked file. The fixed file
// stays on its side.
QStringList multiTabArgs(const QString &fixed, const QStringList &picked, bool fixedOnLeft);

// Starts Meld detached, without a shell.
bool launch(const Command &meld, const QStringList &args);

}  // namespace MeldLauncher
