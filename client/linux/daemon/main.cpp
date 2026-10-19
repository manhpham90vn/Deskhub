#include <cstring>

#include "Daemon.h"
#include "UnattendedHost.h"
#include "secure/UnattendedNames.h"

int main(int argc, char** argv) {
    if (argc >= 2 && std::strcmp(argv[1], kUnattendedHostArg) == 0) return RunUnattendedHost();
    return RunDaemon();
}
