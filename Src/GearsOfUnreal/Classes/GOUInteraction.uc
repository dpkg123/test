class GOUInteraction extends Interaction config(GearsOfUnreal);

var PlayerController PC;
var name ToggleFOVKey;

function InitInteraction( PlayerController Player )
{	
	PC = Player;
}

function bool InputKey( int ControllerId, name Key, EInputEvent EventType, optional float AmountDepressed = 1.f, optional bool bGamepad )
{
    local GOUPawn MP;

    MP = GOUPawn( PC.Pawn );
	
    if ( MP != None )
    {
		
        if ( Key == ToggleFOVKey && EventType == IE_Pressed )
        {
            MP.SetFov(55.0f);         
        }
		if ( Key == ToggleFOVKey && EventType == IE_Released )
        {
            MP.SetFov(0.0f);         
        }
    }

	return false;
}

DefaultProperties
{
	   __OnReceivedNativeInputKey__Delegate=Default__GOUInteraction.InputKey
   Name="Default__GOUInteraction"
   ObjectArchetype=Interaction'Engine.Default__Interaction'
	ToggleFOVKey=RightMouseButton
}
