#pragma once

#include <QString>

namespace nsl {

bool shouldShowMainWindow(bool startMinimizedOption, bool autoMinimizeEnabled);
QString singleInstanceServiceName();
QString singleInstanceObjectPath();
QString singleInstanceInterfaceName();

} // namespace nsl
