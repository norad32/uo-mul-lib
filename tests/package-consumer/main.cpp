#include <app/cli.h>
#include <core/logger.h>
#include <sstream>
#include <string_view>
#include <vector>

int main()
{
    const std::vector<std::string_view> arguments{"check"};
    std::ostringstream output;
    std::ostringstream error;
    im_cpp::core::Logger logger{"package-consumer"};
    const int result = im_cpp::app::runCli(arguments, output, error, logger);
    return result == 0 && output.str() == "I'm C++ OK\n" && error.str().empty() ? 0 : 1;
}
