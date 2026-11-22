#pragma once

#include <QString>
#include <QStringList>

namespace Cli {

enum class Mode { Open, Compare, Mark, CompareWithMarked, Install, Uninstall };

enum class Problem {
    None,
    ConflictingModes,
    UnexpectedItems,
    TooManyItems,
    WrongCount,
    Missing,
    MarkedMissing,
    Duplicate,
};

struct Options
{
    bool open = false;
    bool compare = false;
    bool sort = false;
    bool mark = false;
    bool compareWithMarked = false;
    bool install = false;
    bool uninstall = false;
};

struct Request
{
    Mode mode = Mode::Open;
    QStringList items;
    Problem problem = Problem::None;
    int index = -1;

    bool isValid() const { return problem == Problem::None; }
    bool shouldCompareImmediately() const
    {
        return (mode == Mode::Compare && items.size() >= 2) || mode == Mode::CompareWithMarked;
    }
};

// Normalizes positional items, selects one unambiguous mode, applies natural
// sorting, and validates non-interactive operations before the GUI is opened.
Request prepare(const Options &options, const QStringList &rawItems, const QString &baseDir,
                const QString &markedItem = QString());

}  // namespace Cli
