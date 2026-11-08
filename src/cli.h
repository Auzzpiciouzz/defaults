#pragma once

#include <QStringList>

// The commands that need no window, for scripts and agents:
//   defaults list | types | get <what> | apps <what> | set <what> <app>
// where <what> is a category id (images, pdf, ...) or a MIME type. Each takes
// --json, exits 0 on success and 1 on failure with the reason on stderr.
namespace Cli {
bool handles(const QStringList &arguments);
int run(const QStringList &arguments);
QString usage();
}
