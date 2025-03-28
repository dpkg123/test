//------------------------------------------------------------------------------
// This command flushes the undo buffer.
//
// Owner: Jamie Redmond
//
// Copyright (c) 2002-2010 OC3 Entertainment, Inc.
//------------------------------------------------------------------------------

#ifndef FxFlushUndoCommand_H__
#define FxFlushUndoCommand_H__

#include "FxCommand.h"

namespace OC3Ent
{

namespace Face
{

// This command flushes the undo buffer.
class FxFlushUndoCommand : public FxCommand
{
	// Declare the class.
	FX_DECLARE_CLASS(FxFlushUndoCommand, FxCommand);
public:
	// Constructor.
	FxFlushUndoCommand();
	// Destructor.
	virtual ~FxFlushUndoCommand();

	// Execute the command.
	virtual FxCommandError Execute( const FxCommandArgumentList& argList );
};

} // namespace Face

} // namespace OC3Ent

#endif
