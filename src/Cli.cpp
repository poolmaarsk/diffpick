#include "Cli.hpp"

#include "MeldLauncher.hpp"
#include "PathUtil.hpp"

#include <QCollator>
#include <QFileInfo>
#include <QLocale>

#include <algorithm>

namespace Cli {

Request prepare(const Options &options, const QStringList &rawItems, const QString &baseDir,
                const QString &markedItem)
{
    Request request;
    const int modeCount = int(options.open) + int(options.compare) + int(options.mark) +
                          int(options.compareWithMarked) + int(options.install) +
                          int(options.uninstall);
    if (modeCount > 1)
    {
        request.problem = Problem::ConflictingModes;
        return request;
    }

    if (options.compare)
    {
        request.mode = Mode::Compare;
    }
    else if (options.mark)
    {
        request.mode = Mode::Mark;
    }
    else if (options.compareWithMarked)
    {
        request.mode = Mode::CompareWithMarked;
    }
    else if (options.install)
    {
        request.mode = Mode::Install;
    }
    else if (options.uninstall)
    {
        request.mode = Mode::Uninstall;
    }

    for (const QString &item : rawItems)
    {
        request.items << PathUtil::normalizePath(item, baseDir);
    }

    if ((request.mode == Mode::Install || request.mode == Mode::Uninstall) &&
        !request.items.isEmpty())
    {
        request.problem = Problem::UnexpectedItems;
        return request;
    }

    if (request.items.size() > 3)
    {
        request.problem = Problem::TooManyItems;
        return request;
    }

    if (options.sort)
    {
        // The C locale used by headless Linux ignores numericMode. An explicit
        // Unicode locale keeps natural file-name ordering deterministic.
        QCollator collator{QLocale{QLocale::English}};
        collator.setCaseSensitivity(Qt::CaseInsensitive);
        collator.setNumericMode(true);
        std::sort(request.items.begin(), request.items.end(),
                  [&collator](const QString &a, const QString &b) {
                      return collator.compare(QFileInfo(a).fileName(), QFileInfo(b).fileName()) < 0;
                  });
    }

    if (request.mode == Mode::Mark)
    {
        if (request.items.size() != 1)
        {
            request.problem = Problem::WrongCount;
            return request;
        }

        if (PathUtil::classify(request.items.first()) == PathUtil::Kind::Missing)
        {
            request.problem = Problem::Missing;
            request.index = 0;
        }

        return request;
    }

    if (request.mode == Mode::CompareWithMarked)
    {
        if (request.items.size() != 1)
        {
            request.problem = Problem::WrongCount;
            return request;
        }

        const QString marked = PathUtil::normalizePath(markedItem, baseDir);
        if (PathUtil::classify(marked) == PathUtil::Kind::Missing)
        {
            request.problem = Problem::MarkedMissing;
            return request;
        }

        request.items.prepend(marked);
    }

    if (request.mode == Mode::Compare && request.items.isEmpty())
    {
        request.problem = Problem::WrongCount;
        return request;
    }

    if (!request.shouldCompareImmediately())
    {
        return request;
    }

    const MeldLauncher::Validation validation = MeldLauncher::validate(request.items);
    switch (validation.problem)
    {
    case MeldLauncher::Problem::Missing:
        request.problem = Problem::Missing;
        request.index = validation.index;
        break;
    case MeldLauncher::Problem::WrongCount:
        request.problem = Problem::WrongCount;
        break;
    case MeldLauncher::Problem::None:
        break;
    }

    return request;
}

}  // namespace Cli
