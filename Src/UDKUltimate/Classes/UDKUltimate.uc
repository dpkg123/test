class UDKUltimate extends UTTeamGame;

defaultproperties
{
   bScoreTeamKills=true
   bMustJoinBeforeStart=false
   bTeamGame=True
   TeamAIType(0)=class'UTGame.UTTeamAI'
   TeamAIType(1)=class'UTGame.UTTeamAI'
   EndMessageWait=1
   TeammateBoost=+0.3
   FriendlyFireScale=+0.0
   bMustHaveMultiplePlayers=true
   FlagKillMessageName=TAUNT
   
   DefaultPawnClass=class'UDKUltimate.UDKUltimatePawn'
   PlayerControllerClass=class'UDKUltimate.UDKUltimatePlayerController'
   Acronym="UDK"
   MapPrefixes[0]="UDK"
   
   OnlineGameSettingsClass=class'UTGame.UTGameSettingsDM'
   MidgameScorePanelTag=TDMPanel
}