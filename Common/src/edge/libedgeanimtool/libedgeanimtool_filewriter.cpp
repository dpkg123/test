/* SCE CONFIDENTIAL
 * PlayStation(R)Edge 1.2.0
 * Copyright (C) 2010 Sony Computer Entertainment Inc.
 * All Rights Reserved.
 */


#ifdef _MSC_VER
#pragma warning( disable : 4702 )       // unreachable code
#endif//_MSC_VER

#include    <fstream>
#include    <string>
#include    <map>
#include    "edge/libedgeanimtool/libedgeanimtool_filewriter.h"

//--------------------------------------------------------------------------------------------------

namespace 
{

inline bool IsPow2(unsigned int value)
{
    return (((value & (value - 1)) == 0) && value);
}

inline unsigned int Align(unsigned int value, unsigned int alignment)
{
    return (value + (alignment - 1)) & ~(alignment - 1);
}

inline long Align(long value, unsigned int alignment)
{
    return (value + ((long) alignment - 1)) & ~((long) alignment - 1);
}

} // anonymous namespace

//--------------------------------------------------------------------------------------------------

namespace Edge
{
namespace Tools
{

FileWriter::FileWriter(const std::string& filename, bool bigEndian)  
{
	m_pStream = new std::ofstream(filename.c_str(), std::ios_base::out|std::ios_base::binary);
	if (!m_pStream) {
        std::cerr << "Error: can't open " << filename << "." << std::endl;
        EDGEERROR_F();
    }

    m_bigEndian = bigEndian;
	m_streamOwner = true;
}

FileWriter::FileWriter(std::ostream* pStream, bool bigEndian)
{
	m_pStream = pStream;
	m_bigEndian = bigEndian;
	m_streamOwner = false;
}

FileWriter::~FileWriter()
{
	if (m_pStream && m_streamOwner) {
		delete m_pStream;	// destructor will close the file too
	}
}

long FileWriter::GetPos(void)
{
    long pos = -1;
	if ( m_pStream ) {
		pos = (long) m_pStream->tellp();
		pos = (pos == -1) ? 0 : pos;	// unwritten streams return -1 so we alter it to 0 here
	} else {
        std::cerr << "Error: file error." << std::endl;
        EDGEERROR_F();
    }
    return pos;
}

void FileWriter::Close()
{
    PatchOffsets();
	if (m_pStream && m_streamOwner) {
		delete m_pStream;	// destructor will close the file too
		m_pStream = 0;
	}
}

void FileWriter::Align(unsigned int alignment)
{   
    long posCurrent = GetPos();
    long posNext = ::Align(posCurrent, alignment);
    for(long pos = posCurrent; pos < posNext; pos++) {
        static const unsigned char zero = 0;
		m_pStream->write(reinterpret_cast<const char*>(&zero), 1);
    }
}

void FileWriter::Label(const std::string& label, unsigned int alignment)
{
    long posCurrent = GetPos();
    m_labels[ label ] = ::Align(posCurrent, alignment);
}

void FileWriter::AddOffset(OffsetDef::Type type, const std::string& label, const std::string& labelRelativeTo)
{
    OffsetDef ofsDef;
    ofsDef.m_type = type;
    ofsDef.m_label = label;
    ofsDef.m_labelRelativeTo = labelRelativeTo;

    // Current position
    long posCurrent = GetPos();
    ofsDef.m_posOffset = posCurrent;

    // If position is not specified, it's relative to current
    if (labelRelativeTo == "") {
        ofsDef.m_posRelativeTo = posCurrent;
    }
    else {
        ofsDef.m_posRelativeTo  = -1;
    }

    // Add offset to the list
    m_offsets.push_back(ofsDef);
}

void FileWriter::WriteOffset16(const std::string& label, const std::string& labelRelativeTo)
{
    // Write placeholder 0 - will stay 0 if label is unresolved
    AddOffset(OffsetDef::kOfs16, label, labelRelativeTo);
    Write((unsigned short) 0);
}


void FileWriter::WriteOffset32(const std::string& label, const std::string& labelRelativeTo)
{
    // Write placeholder 0 - will stay 0 if label is unresolved
    AddOffset(OffsetDef::kOfs32, label, labelRelativeTo);
    Write((unsigned int) 0);
}

void FileWriter::PatchOffsets(void)
{
    long posSaved = GetPos();

    // Patch all offsets
    for(std::list<OffsetDef>::const_iterator iteOfs = m_offsets.begin();
         iteOfs != m_offsets.end(); 
         iteOfs++) {
        // Resolve label
        std::map<std::string, long>::const_iterator foundLabel = m_labels.find(iteOfs->m_label);
        if (foundLabel != m_labels.end()) {     
            long posRelativeTo = iteOfs->m_posRelativeTo;
            
            // Resolve base label, if specified
            if (iteOfs->m_posRelativeTo) {
                std::map<std::string, long>::const_iterator foundLabelRelative = m_labels.find(iteOfs->m_labelRelativeTo);
                if (foundLabelRelative != m_labels.end())
                    posRelativeTo = foundLabelRelative->second;
            }
            if (posRelativeTo >= 0)  {
                long ofs = foundLabel->second - posRelativeTo;
				m_pStream->seekp(iteOfs->m_posOffset);
                switch(iteOfs->m_type) {
                case OffsetDef::kOfs16:
                    Write((unsigned short) ofs);
                    break;
                case OffsetDef::kOfs32:
                    Write((unsigned int) ofs);
                    break;
                default:
                    break;
                }
            }
        }
    }

    // Remove offsets / restore file pos
    m_offsets.clear();
	m_pStream->seekp(posSaved);
}

long FileWriter::GetLabelOffset(const std::string& label)
{
    std::map<std::string, long>::const_iterator foundLabel = m_labels.find(label);

    if (foundLabel != m_labels.end())
        return foundLabel->second;
    else
        return -1;
}

//--------------------------------------------------------------------------------------------------

} // namespace Tools
} // namespace Edge

