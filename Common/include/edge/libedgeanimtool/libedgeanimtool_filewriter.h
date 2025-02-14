/* SCE CONFIDENTIAL
 * PlayStation(R)Edge 1.2.0
 * Copyright (C) 2010 Sony Computer Entertainment Inc.
 * All Rights Reserved.
 */

#ifndef LIBEDGEANIMTOOL_FILEWRITER_H
#define LIBEDGEANIMTOOL_FILEWRITER_H

#include <iostream>
#include <list>
#include <map>

#include "edge/libedgeanimtool/libedgeanimtool_common.h"

//--------------------------------------------------------------------------------------------------

namespace Edge
{
namespace Tools
{

//--------------------------------------------------------------------------------------------------
/**
    Simple file writer with offset patching
**/
//--------------------------------------------------------------------------------------------------

class FileWriter
{
public:
    FileWriter(const std::string& filename, bool bigEndian);
	FileWriter(std::ostream* pStream, bool bigEndian);
    ~FileWriter(void);

    void    Close(void);
    void    Align(unsigned int alignment);
    void    Label(const std::string& label, unsigned int alignment = 1);    
    void    WriteOffset16(const std::string& label, const std::string& labelRelativeTo = std::string(""));
    void    WriteOffset32(const std::string& label, const std::string& labelRelativeTo = std::string(""));
    void    PatchOffsets(void);
    long    GetPos(void);
    long    GetLabelOffset(const std::string& label);

    template<typename T>    
    void Write(T val);

private:
    struct OffsetDef
    {
        enum Type
        {
            kOfs16,
            kOfs32
        }               m_type;
        std::string     m_label;
        std::string     m_labelRelativeTo;
        long            m_posOffset;
        long            m_posRelativeTo;
    };
    void    AddOffset(OffsetDef::Type type, const std::string& label, const std::string& labelRelativeTo);

	std::ostream*					m_pStream;
    std::list<OffsetDef>            m_offsets;
    std::map<std::string, long>     m_labels;
    bool                            m_bigEndian;
	bool							m_streamOwner;
};

template<typename T>
inline void FileWriter::Write(T val)
{
    T tmp = m_bigEndian ? Reverse(val) : val;
	m_pStream->write(reinterpret_cast<const char*>(&tmp), sizeof(T));
}

//--------------------------------------------------------------------------------------------------

}   // namespace Tools
}   // namespace Edge

#endif // LIBEDGEANIMTOOL_FILEWRITER_H
