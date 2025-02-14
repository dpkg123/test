/* SCE CONFIDENTIAL
 * PlayStation(R)Edge 1.2.0
 * Copyright (C) 2010 Sony Computer Entertainment Inc.
 * All Rights Reserved.
 */

#include "edge/libedgeanimtool/libedgeanimtool_argparser.h"
#include "edge/libedgeanimtool/libedgeanimtool_animation.h"
#include "animation.h"
#include "skeleton.h"

//--------------------------------------------------------------------------------------------------
/**
    Main application entry point.
**/
//--------------------------------------------------------------------------------------------------

static void PrintUsage(const char* arg0)
{
    std::cout << arg0 << " (PlayStation(R)Edge 1.2.0)" << std::endl;

    std::cout << "Usage: " << std::endl;
	std::cout << "\t" << arg0 << " -additiveanim <inputbaseanimfile> <inputanimfile> <inputbinaryskelfile> <outputanimfile>" << std::endl;
	std::cout << "\t" << arg0 << " -selfadditiveanim <inputanimfile> <inputbinaryskelfile> <outputanimfile>" << std::endl;
	std::cout << "\t" << arg0 << " -anim <inputanimfile> <inputbinaryskelfile> <outputanimfile>" << std::endl;
	std::cout << "\t" << arg0 << " -skel <inputskelfile> <outputskelfile>" << std::endl;
    std::cout << "Optional flags: " << std::endl;
	std::cout << "\t-pc                          Write binary in PC (little-endian) form" << std::endl;
	std::cout << "\t-stats                       Output anim stats to <outputanimfile>.txt" << std::endl;
	std::cout << "\t-userchannels <file>         Specify file which lists all user channels (only applicable with -skel)" << std::endl;
	std::cout << "\t-bitpacked                   Enable bitpacking" << std::endl;
	std::cout << "\t-tolerance <float>           Specify tolerance used by bitpacking" << std::endl;
	std::cout << "\t-optimizertolerance <float>  Specify tolerance used by optimizer" << std::endl;
	std::cout << "\t-disableoptimization         Disable optimization" << std::endl;
	std::cout << "\t-locojoint <jointname>       Specify locomotion joint" << std::endl;
	std::cout << "\t-jointnames                  Outputs joint names as custom data table entry 'JointNames'" << std::endl;
	std::cout << "\t-verbose                     Outputs extra information on compile process" << std::endl;
}

int main(int argc, char** argv)
{
    try {
		// check arguments
		std::string argSelfAdditive("selfadditiveanim");
		std::string argAdditive("additiveanim");
		std::string argAnim("anim");
		std::string argSkel("skel");
		std::string argPc("pc");
		std::string argStats("stats");
		std::string argUserChannels("userchannels");
		std::string argBitPacked("bitpacked");
		std::string argTolerance("tolerance");
		std::string argOptimizerTolerance("optimizertolerance");
		std::string argDisableOptimization("disableoptimization");
		std::string argLocoJoint("locojoint");
		std::string argJointNames("jointnames");
		std::string argVerbose("verbose");

		Edge::Tools::ArgParser argParser;
		argParser.AddToken(argSelfAdditive, 3);	// 3 sub args
		argParser.AddToken(argAdditive, 4);		// 4 sub args
		argParser.AddToken(argAnim, 3);			// 3 sub args
		argParser.AddToken(argSkel, 2);			// 2 sub args
		argParser.AddToken(argPc, 0);
		argParser.AddToken(argStats, 0);
		argParser.AddToken(argUserChannels, 1);
		argParser.AddToken(argBitPacked, 0);
		argParser.AddToken(argTolerance, 1);
		argParser.AddToken(argOptimizerTolerance, 1);
		argParser.AddToken(argDisableOptimization, 0);
		argParser.AddToken(argLocoJoint, 1);
		argParser.AddToken(argJointNames, 0);
		argParser.AddToken(argVerbose, 0);

		if(!argParser.ParseArguments(argc, argv)) {
			std::cout << "Failed: invalid arguments" << std::endl;
			PrintUsage(argv[0]);
			return 1;
		}

        bool commandLineError = true;
 
		// get optional args
		bool bigEndian = !argParser.ArgumentExists(argPc);
		bool stats = argParser.ArgumentExists(argStats);
		bool bitPacked = argParser.ArgumentExists(argBitPacked);
		bool optimize = !argParser.ArgumentExists(argDisableOptimization);
		bool verbose = argParser.ArgumentExists(argVerbose);

		// bitpacking tolerance
		float tolerance = Edge::Tools::kDefaultTolerance;
		if(argParser.ArgumentExists(argTolerance))
			tolerance = (float)atof(argParser.GetSubArgument(argTolerance, 0).c_str());

		// optimizer tolerance
		float optimizerTolerance = Edge::Tools::kDefaultTolerance;
		if(argParser.ArgumentExists(argOptimizerTolerance))
			optimizerTolerance = (float)atof(argParser.GetSubArgument(argOptimizerTolerance, 0).c_str());

		std::string userChannelFilename;
		if(argParser.ArgumentExists(argUserChannels))
			userChannelFilename = argParser.GetSubArgument(argUserChannels, 0);

		// locomotion joint
		std::string locoJointName;
		if(argParser.ArgumentExists(argLocoJoint))
			locoJointName = argParser.GetSubArgument(argLocoJoint, 0);

		// set the compression modes and error tolerances
		// note: edgeanimcompiler doesn't currently support the fine grain control that is possible with libedgeanimtool,
		// it simply enables/disables bitpacking globally
		Edge::Tools::CompressionInfo compressionInfo;
		if(bitPacked)
		{
			compressionInfo.m_compressionTypeRotation = Edge::Tools::COMPRESSION_TYPE_BITPACKED;
			compressionInfo.m_compressionTypeTranslation = Edge::Tools::COMPRESSION_TYPE_BITPACKED;
			compressionInfo.m_compressionTypeScale = Edge::Tools::COMPRESSION_TYPE_BITPACKED;
			compressionInfo.m_compressionTypeUser = Edge::Tools::COMPRESSION_TYPE_BITPACKED;
			compressionInfo.m_defaultToleranceRotation = tolerance;
			compressionInfo.m_defaultToleranceTranslation = tolerance;
			compressionInfo.m_defaultToleranceScale = tolerance;
			compressionInfo.m_defaultToleranceUser = tolerance;
		}
		if(argParser.ArgumentExists(argSelfAdditive)) {
            commandLineError = false;						
            const std::string inputAnimDaeFilename = argParser.GetSubArgument(argSelfAdditive, 0);
            const std::string inputSkelBinFilename = argParser.GetSubArgument(argSelfAdditive, 1);
            const std::string outputAnimBinFilename = argParser.GetSubArgument(argSelfAdditive, 2);
            
            Edge::Tools::ProcessAdditiveAnimation(inputAnimDaeFilename, inputAnimDaeFilename, inputSkelBinFilename, outputAnimBinFilename, true, locoJointName, bigEndian, stats, verbose, compressionInfo, optimize, optimizerTolerance);
        }
		if(argParser.ArgumentExists(argAdditive)) {
            commandLineError = false;						
			const std::string inputBaseAnimDaeFilename = argParser.GetSubArgument(argAdditive, 0);
            const std::string inputAnimDaeFilename = argParser.GetSubArgument(argAdditive, 1);
            const std::string inputSkelBinFilename = argParser.GetSubArgument(argAdditive, 2);
            const std::string outputAnimBinFilename = argParser.GetSubArgument(argAdditive, 3);
            
            Edge::Tools::ProcessAdditiveAnimation(inputBaseAnimDaeFilename, inputAnimDaeFilename, inputSkelBinFilename, outputAnimBinFilename, false, locoJointName, bigEndian, stats, verbose, compressionInfo, optimize, optimizerTolerance);
        }
		if(argParser.ArgumentExists(argAnim)) {
            commandLineError = false;
            const std::string inputAnimDaeFilename = argParser.GetSubArgument(argAnim, 0);
            const std::string inputSkelBinFilename = argParser.GetSubArgument(argAnim, 1);
            const std::string outputAnimBinFilename = argParser.GetSubArgument(argAnim, 2);
            
            Edge::Tools::ProcessAnimation(inputAnimDaeFilename, inputSkelBinFilename, outputAnimBinFilename, locoJointName, bigEndian, stats, verbose, compressionInfo, optimize, optimizerTolerance);
        }
		if(argParser.ArgumentExists(argSkel)) {
            commandLineError = false;
            const std::string inputSkelDaeFilename = argParser.GetSubArgument(argSkel, 0);
            const std::string outputSkelBinFilename = argParser.GetSubArgument(argSkel, 1);
			bool addJointNames = argParser.ArgumentExists(argJointNames);
            
            Edge::Tools::ProcessSkeleton(inputSkelDaeFilename, outputSkelBinFilename, userChannelFilename, locoJointName, addJointNames, bigEndian, verbose);
        }

		if(commandLineError) {
			PrintUsage(argv[0]);
			return 1;
		}
    }
    catch(std::exception& excuse) {
        std::cout << "Failed: " << excuse.what() << std::endl;
        return 1;
    }

	return 0;
}
