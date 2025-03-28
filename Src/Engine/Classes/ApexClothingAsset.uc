/*=============================================================================
	ApexClothingAsset.h: PhysX APEX integration. Clothing Asset
	Copyright 2008-2009 NVIDIA Corporation.
=============================================================================*/

class ApexClothingAsset extends ApexAsset
	hidecategories(Object)
	native(Mesh);

var   native pointer                                          MApexAsset{class FIApexAsset};
var() const editfixedsize editoronly array<MaterialInterface>	Materials;

/** ApexClothingLibrary is only for legacy APEX 0.9 assets. */
var const deprecated ApexGenericAsset ApexClothingLibrary;

var() const bool bUseHardwareCloth;						// if true use hardware clothing for simulation
var() const bool bFallbackSkinning;						// if true, falls back to skinning clothing in software instead of using GPU skinning
var() const bool bSlowStart;							// Designates the 'slowStart' flag; see APEX clothing documentation
var() const bool bRecomputeNormals;						// Designates the 'recomputeNormals' flag; see APEX clothing documentation
var() const int UVChannelForTangentUpdate;				// Which UV channel is used for updating tangent space.
var() const float MaxDistanceBlendTime<ClampMin=0.0>;	// The maximimum distance blend time (see APEX clothing documentation)
var() const float ContinuousRotationThreshold;			// The angle in degrees to consider the clothing simulation continuous.
var() const float ContinuousDistanceThreshold;			// The distance to consider the clothing simulation continuous.
var() const bool  bResetAfterTeleport;					// If true, it resets the simulation after a teleport.
var() const float LodWeightsMaxDistance;				// LodWeightMaxDistance (see APEX clothing documentation)
var() const float LodWeightsDistanceWeight;				// LodWeightDistanceWeight (see APEX clothing documentation)
var() const float LodWeightsBias;						// LodWeightBias (see APEX clothing documentation)
var() const float LodWeightsBenefitsBias;				// LodWeightMaxBenefitsBias (see APEX clothing documentation)

cpptext
{
	public:
		/** Notification of the post load event. */
		virtual void                 PostLoad();

		/**** Serializes the asset
		* @param : Ar is a reference to the FArchive to either serialize from or to.
		*/
		virtual void                 Serialize(FArchive& Ar);

		/*** Returns the array of strings to display in the browser window */
		virtual TArray<FString>      GetGenericBrowserInfo();

		/*** This method is called when a generic asset is imported from an external file on disk.
		**
		** @param Buffer : A pointer to the raw data.
		** @param BufferSize : The length of the raw input data.
		** @param Name : The name of the asset which is being imported.
		**
		** @return : Returns true if the import was successful.
		**/
		UBOOL                        Import( const BYTE* Buffer, INT BufferSize, const FString& Name,UBOOL convertToUE3Coordinates );

		/*** This method is called after a property has changed. */
		virtual void                 PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);

		/** This method is called prior to the object being destroyed */
		virtual void                 BeginDestroy(void);

		/*** This method is called when the asset is renamed
		**
		** @param : InName : The new name of the object
		** @param : NewOuter : The new outer object (package) for this object.
		** @param : Flags : The ERenameFlags to honor.
		**
		** @return : Returns TRUE if the rename was successful
		**/
		virtual UBOOL Rename( const TCHAR* InName, UObject* NewOuter, ERenameFlags Flags );

	   	/** virtual method to return the number of materials used by this asset */
		virtual UINT                GetNumMaterials(void) const { return Materials.Num();  }
		/** Returns the default ::NxParameterized::Interface for this asset. */
		virtual UMaterialInterface *GetMaterial(UINT Index) const { return Materials(Index); }

		/** Returns the default ::NxParameterized::Interface for this object */
		virtual void * GetNxParameterized(void);



		/** Interface to ApexGenericAsset */
		class FIApexAsset * GetApexGenericAsset() const { return MApexAsset; }

		/** Re-assigns the APEX material resources by name with the current array of UE3 materials */
		void UpdateMaterials(void);
		virtual void NotifyApexEditMode(class ApexEditInterface *iface);
		
	private:
}

defaultproperties
{
  bUseHardwareCloth=true                 // if true use hardware clothing for simulation
  bFallbackSkinning=false                // if true, falls back to skinning clothing in software instead of using GPU skinning
  bSlowStart=true                        // Designates the 'slowStart' flag; see APEX clothing documentation
  bRecomputeNormals=false;
  UVChannelForTangentUpdate=0            // Which UV channel is used for updating tangent space.
  MaxDistanceBlendTime=1                 // The maximimum distance blend time (see APEX clothing documentation)
  ContinuousRotationThreshold=84         // The angle in degrees to consider the clothing simulation continuous.
  ContinuousDistanceThreshold=50.0f      // The distance to consider the clothing simulation continuous.
  bResetAfterTeleport = true		 	 // If true, it resets the simulation after a teleport
  LodWeightsMaxDistance=10000            // LodWeightMaxDistance (see APEX clothing documentation)
  LodWeightsDistanceWeight=1             // LodWeightDistanceWeight (see APEX clothing documentation)
  LodWeightsBias=0                       // LodWeightBias (see APEX clothing documentation)
  LodWeightsBenefitsBias=0               // LodWeightMaxBenefitsBias (see APEX clothing documentation)
}
