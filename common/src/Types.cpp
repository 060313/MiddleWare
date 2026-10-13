#include "Types.h"

namespace common {

const char* toString(SchedMode mode) {
    switch (mode) {
        case SchedMode::Auto:   return "auto";
        case SchedMode::Static: return "static";
        case SchedMode::Off:    return "off";
    }
    return "unknown";
}

}  // namespace common
