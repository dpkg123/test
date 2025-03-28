﻿using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.IO;

namespace GameplayProfiler
{
	/**
	 * Parses the passed in stream into a frames. The assumption is that only Actors can be root level objects and 
	 * actors are only ticked from the root level and never with a scope. Functions and components on the other hand
	 * can be ticked/ called from any scope and we use a special "Ungrouped" actor to not break the ordering of
	 * actors as the only root level objects.
	 */
	class StreamParser
	{		
		/** Mapping from Actor.GetPathName to its token, used by matching up components with their actors. */
		private static Dictionary<string,TokenActor> ActorNameToTokenMap = new Dictionary<string,TokenActor>();

		/**
		 * Parses passed in data stream into a profiler stream container class
		 * 
		 * @param	ParserStream	Raw data stream, needs to support seeking
		 * @return	ProfilerStream data was parsed into
		 */
		public static ProfilerStream Parse( Stream ParserStream )
		{
            var StartTime = DateTime.UtcNow;

			// Network stream the file is parsed into.
			var ProfilerStream = new ProfilerStream();
			
			// Serialize the header. This will also return an endian-appropriate binary reader to
			// be used for reading the data. 
			BinaryReader BinaryStream = null; 
			var Header = StreamHeader.ReadHeader( ParserStream, out BinaryStream );

			// Keep track of token stream offset as name table is at end of file.
			long TokenStreamOffset = ParserStream.Position;

			// Seek to name table and serialize it.
			ParserStream.Seek(Header.NameTableOffset,SeekOrigin.Begin);
			for( int NameIndex=0; NameIndex<Header.NameTableEntries; NameIndex++ )
			{
				UInt32 Length = BinaryStream.ReadUInt32();
				ProfilerStream.NameTable.Add(new string(BinaryStream.ReadChars((int)Length)));
			}

			// Seek to class hierarchy and serialize it.
			ParserStream.Seek(Header.ClassHierarchyOffset,SeekOrigin.Begin);
			for(int SerializedClassIndex=0; SerializedClassIndex<Header.ClassCount; SerializedClassIndex++ )
			{
				// Serialize class hierarchy for this class. We're keeping track via a stack as the code
				// needs to create classes from base class inward.
				var ClassNameIndices = new Stack<Int32>();
				while( true )
				{
					Int32 ClassNameIndex = BinaryStream.ReadInt32();
					if( ClassNameIndex >= 0 )
					{
						ClassNameIndices.Push(ClassNameIndex);
					}
					else
					{
						break;
					}
				}
				// Convert class stack to hierarchy.
				Class SuperClass = null;
				while( ClassNameIndices.Count > 0 )
				{
					Int32 ClassNameIndex = ClassNameIndices.Pop();
					Class CurrentClass = null;
					// Try to find existing class.
					if( ProfilerStream.IndexToClassMap.ContainsKey( ClassNameIndex ) )
					{
						CurrentClass = ProfilerStream.IndexToClassMap[ClassNameIndex];
					}
					// Not found, add it.
					else
					{
						// Add new class.
						CurrentClass = new Class( ProfilerStream.GetName(ClassNameIndex), ClassNameIndex, SuperClass );
						ProfilerStream.IndexToClassMap.Add(ClassNameIndex, CurrentClass);
						ProfilerStream.Classes.Add(CurrentClass);
						// Try to cache actor/ component/ function class name indices.
						if (ProfilerStream.ActorClass == null && ProfilerStream.GetName(ClassNameIndex) == "Actor")
						{
							ProfilerStream.ActorClass = CurrentClass;
						}
						if (ProfilerStream.ComponentClass == null && ProfilerStream.GetName(ClassNameIndex) == "ActorComponent")
						{
							ProfilerStream.ComponentClass = CurrentClass;
						}
						if (ProfilerStream.FunctionClass == null && ProfilerStream.GetName(ClassNameIndex) == "Function")
						{
							ProfilerStream.FunctionClass = CurrentClass;
						}
					}
					SuperClass = CurrentClass;
				}
			}

			// Seek to beginning of token stream.
			ParserStream.Seek(TokenStreamOffset,SeekOrigin.Begin);

			// Scratch variables used for building stream. Required as we emit certain information 
			// in reverse order.
			var ActiveObjects = new Stack<TokenObject>();
			int CurrentFrameStartIndex = 0;
			float CurrentFrameTrackedTime = 0;
			int CurrentIndex = 0;
			var CurrentFrameActors = new List<TokenActor>();
			var CurrentFrameNameToFunctionInfoMap = new Dictionary<string, FunctionInfo>();

			// Create special object token for ungrouped components.
			var UngroupedToken = new TokenActor(ProfilerStream);
			CurrentFrameActors.Add(UngroupedToken);

			// Parse stream till we reach the end, marked by special token.
			bool bHasReachedEndOfStream = false;
			while( bHasReachedEndOfStream == false )
			{
				TokenBase Token = TokenBase.ReadNextToken( BinaryStream, ProfilerStream );
				ProfilerStream.Tokens.Add(Token);

				switch (Token.TokenType)
				{
				case ETokenTypes.Function:
					// Skip first frame.
					if( CurrentFrameStartIndex != 0 )
					{
						var Function = (TokenFunction) Token;
						ActiveObjects.Push(Function);
					}
					break;
				case ETokenTypes.Actor:
					// Skip first frame.
					if( CurrentFrameStartIndex != 0 )
					{
						// Actors are always ticked top scope. This is being relied on by code
						// treating all actors as root level objects.
						if (ActiveObjects.Count > 0)
						{
							throw new InvalidDataException();
						}
						var Actor = (TokenActor) Token;
                        if (!ActorNameToTokenMap.ContainsKey(Actor.GetActorPathName()))
                        {
                            ActorNameToTokenMap.Add(Actor.GetActorPathName(), Actor);
                        }
						ActiveObjects.Push(Actor);
					}
					break;
				case ETokenTypes.Component:
					// Skip first frame.
					if( CurrentFrameStartIndex != 0 )
					{
						ActiveObjects.Push((TokenComponent)Token);
					}
					break;
				case ETokenTypes.EndOfScope:
					// Skip first frame.
					if( CurrentFrameStartIndex != 0 )
					{
						var EndOfScopeToken = (TokenEndOfScope)Token;
						var CurrentScope = ActiveObjects.Pop();
						CurrentScope.bShouldSkipInDetailedView = EndOfScopeToken.bShouldSkipInDetailedView;
						CurrentScope.InclusiveTime = (float)(EndOfScopeToken.DeltaCycles * Header.SecondsPerCycle * 1000);
						// Sum up root level objects to track tracked time. Non-root objects are accounted for in root objects.
						if (ActiveObjects.Count == 0)
						{
							CurrentFrameTrackedTime += CurrentScope.InclusiveTime;
						}
						var Component = CurrentScope as TokenComponent;
						var Object = CurrentScope as TokenObject;
						// Add component time to owning actor and also handle scoping. We handle scoping at the
						// end of a scope due to the special handling required for ungrouped components.
						if (Component != null)
						{
							TokenActor OwningActor = null;
							if (ActorNameToTokenMap.ContainsKey(Component.GetActorPathName()))
							{
								OwningActor = ActorNameToTokenMap[Component.GetActorPathName()];
							}
							else
							{
								OwningActor = UngroupedToken;
							}
							OwningActor.InclusiveTime += Component.InclusiveTime;
							OwningActor.ChildrenTime += Component.InclusiveTime;
							OwningActor.Children.Add(Component);
							// If a component is not ticked as root object subtract out the time from owned
							// objects to avoid double counting it as we're associating it with the owning 
							// actor above.
							foreach( TokenObject ActiveObject in ActiveObjects )
							{
								ActiveObject.InclusiveTime -= Component.InclusiveTime;
							}
						}
						// Not a component. Either actor or function.
						else if (Object != null)
						{
							var Actor = Object as TokenActor;
							if( ActiveObjects.Count > 0 )
							{
								// Can't be an actor at this point as actors are only ticked root level.
								// Can't be a component as that is handled at the top, leaving function.
								var Function = (TokenFunction) Object;
								ActiveObjects.Peek().Children.Add(Function);
								ActiveObjects.Peek().ChildrenTime += Function.InclusiveTime;
								// Using Count instead of Count-1 as frames are added at the end so  it is -1 + 1.
								FunctionInfo.AddFunction(ProfilerStream.Frames.Count,Function,CurrentFrameNameToFunctionInfoMap);
								FunctionInfo.AddFunction(ProfilerStream.Frames.Count,Function,ProfilerStream.AggregateNameToFunctionInfoMap);
							}
							// Actor
							else if (Actor != null)
							{
								CurrentFrameActors.Add(Actor);
							}
							// Only actors can be root level objects. This must be function as component
							// case is handled at the very top of the if statement.
							else
							{
								UngroupedToken.Children.Add(Object);
								UngroupedToken.ChildrenTime += Object.InclusiveTime;
							}
						}
					}
					break;
				case ETokenTypes.Frame:
					{
						// We skip the first frame as it's usually a spike.
						if( CurrentFrameStartIndex != 0 )
						{
							// Update max exclusive information. (Count instead of Count - 1 as current frame hasn't been added yet.)
							UpdateMaxTimeFunctionInfo( ProfilerStream.Frames.Count, ProfilerStream, CurrentFrameNameToFunctionInfoMap );

							// Create frame.
							var FrameMarker = (TokenFrameMarker)Token;
							var Frame = new Frame(CurrentFrameActors, CurrentFrameNameToFunctionInfoMap, CurrentFrameStartIndex, FrameMarker.ElapsedTime, CurrentFrameTrackedTime);
							ProfilerStream.Frames.Add(Frame);
						}
						// Reset frame specific helpers.
						ActorNameToTokenMap.Clear();
						CurrentFrameActors.Clear();
						CurrentFrameNameToFunctionInfoMap.Clear();
						CurrentFrameStartIndex = CurrentIndex + 1;
						CurrentFrameTrackedTime = 0;
					}
					break;
				case ETokenTypes.EndOfStream:
					{
						// We ignore partial frames so end of stream does not add a new frame without a token
						bHasReachedEndOfStream = true;
					}
					break;
				default:
					throw new InvalidDataException();
				}

				CurrentIndex++;
			}

			// Stats for profiling.
            double ParseTime = (DateTime.UtcNow - StartTime).TotalSeconds;
			Console.WriteLine( "Parsing {0} MBytes in stream took {1} seconds", ParserStream.Length / 1024 / 1024, ParseTime );

			return ProfilerStream;
		}


		/**
		 * Update max exclusive information.
		 */
		private static void UpdateMaxTimeFunctionInfo( int FrameIndex, ProfilerStream ProfilerStream, Dictionary<string, FunctionInfo> NameToFunctionInfoMap )
		{
			foreach( var FunctionNameAndInfo in NameToFunctionInfoMap )
			{
				var FunctionName = FunctionNameAndInfo.Key;
				var FunctionInfo = FunctionNameAndInfo.Value;
				float ExclusiveTime = FunctionInfo.InclusiveTime - FunctionInfo.ChildrenTime;
				var AggregateFunctionInfo = ProfilerStream.AggregateNameToFunctionInfoMap[FunctionName];
				if( ExclusiveTime > AggregateFunctionInfo.MaxExclusiveTime )
				{
					AggregateFunctionInfo.MaxExclusiveTime = ExclusiveTime;
					AggregateFunctionInfo.FrameMaxExclusiveTimeOccured = FrameIndex;
				}
				if( FunctionInfo.InclusiveTime > AggregateFunctionInfo.MaxInclusiveTime )
				{
					AggregateFunctionInfo.MaxInclusiveTime = FunctionInfo.InclusiveTime;
					AggregateFunctionInfo.FrameMaxInclusiveTimeOccured = FrameIndex;
				}
			}
		}
	}
}
