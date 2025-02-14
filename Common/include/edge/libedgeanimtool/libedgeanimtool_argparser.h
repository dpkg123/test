/* SCE CONFIDENTIAL
 * PlayStation(R)Edge 1.2.0
 * Copyright (C) 2007 Sony Computer Entertainment Inc.
 * All Rights Reserved.
 */

#ifndef LIBEDGEANIMTOOL_ARGPARSER_H
#define LIBEDGEANIMTOOL_ARGPARSER_H

#ifdef _MSC_VER
#pragma warning( push )
#pragma warning( disable : 4702 )       // unreachable code
#endif//_MSC_VER

#include <string>
#include <map>
#include <vector>
#include <list>

#ifdef _MSC_VER
#pragma warning( pop )
#endif//_MSC_VER

//--------------------------------------------------------------------------------------------------

namespace Edge
{
namespace Tools
{

//--------------------------------------------------------------------------------------------------
/**
    Simple command line argument parser
**/
//--------------------------------------------------------------------------------------------------

class ArgParser
{
public:
    ArgParser() {};
    ~ArgParser() {};

    void AddToken(std::string argName, int subArgCount);
    bool ParseArguments(int argc, char *argv[]);
    std::string GetSubArgument(std::string argName, int subArgIndex);
    bool ArgumentExists(std::string argName);

private:

    struct Arg
    {
        std::string             m_token;
        int                     m_subArgCount;
        std::vector<std::string>m_subArgs;
    };

    std::vector<std::string>    m_tokens;
    std::vector<int>            m_subArgCounts;
    std::map<std::string, Arg>  m_args;
};

//--------------------------------------------------------------------------------------------------

}   // namespace Tools
}   // namespace Edge

#endif // LIBEDGEANIMTOOL_ARGPARSER_H
