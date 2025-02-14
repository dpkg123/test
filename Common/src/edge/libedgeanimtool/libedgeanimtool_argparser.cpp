/* SCE CONFIDENTIAL
 * PlayStation(R)Edge 1.2.0
 * Copyright (C) 2007 Sony Computer Entertainment Inc.
 * All Rights Reserved.
 */

#include    "edge/libedgeanimtool/libedgeanimtool_argparser.h"

//--------------------------------------------------------------------------------------------------

namespace Edge
{
namespace Tools
{

void ArgParser::AddToken(std::string argName, int subArgCount)
{
    m_tokens.push_back(argName);
    m_subArgCounts.push_back(subArgCount);
}

bool ArgParser::ArgumentExists(std::string argName)
{
    return (m_args.find(argName.c_str()) != m_args.end());
}

std::string ArgParser::GetSubArgument(std::string argName, int subArgIndex)
{
    if(!ArgumentExists(argName))
        return "";

    Arg& arg = m_args[argName];

    if(subArgIndex >= arg.m_subArgCount)
        return "";

    return arg.m_subArgs[subArgIndex];
}

bool ArgParser::ParseArguments(int argc, char *argv[])
{
    int curArg = 1;
    while(curArg < argc) {
        const char* argName = argv[curArg++];

        if(argName[0] != '-')
            return false;

        std::string argStr(argName+1);

        unsigned int tokId = 0;
        for(; tokId < m_tokens.size(); tokId++) {
            if(m_tokens[tokId] == argStr)
                break;
        }

        if(tokId == m_tokens.size())
            return false;

        Arg arg;
        arg.m_token = m_tokens[tokId];
        arg.m_subArgCount = m_subArgCounts[tokId];

        for(int i=0; i<arg.m_subArgCount; i++) {
            if(curArg >= argc)
                return false;

            arg.m_subArgs.push_back(argv[curArg++]);
        }

        m_args[argStr] = arg;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------

} // namespace Tools
} // namespace Edge

