#include "app/cli_arguments.h"
#include <span>
#include <string_view>
#include <vector>

namespace im_cpp::app
{

std::vector<std::string_view> makeCliArguments(std::span<char *> arguments)
{
    std::vector<std::string_view> cliArguments;
    cliArguments.reserve(arguments.empty() ? 0 : arguments.size() - 1);
    if (!arguments.empty())
    {
        for (const char * argument : arguments.subspan(1))
        {
            cliArguments.emplace_back(argument);
        }
    }
    return cliArguments;
}

}
