<%@ Page Language="C#" AutoEventWireup="true" CodeFile="Default.aspx.cs" Inherits="Default" %>

<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">

<html xmlns="http://www.w3.org/1999/xhtml" >
<head runat="server">
<title>Epic Build System - Main Page</title>
</head>
<body>
<center>

<form id="Form1" runat="server">
<ajaxToolkit:ToolkitScriptManager ID="BuilderScriptManager" runat="server" />

<asp:Label ID="Label_Title" runat="server" Font-Bold="True" Font-Names="Arial" Font-Size="XX-Large" ForeColor="Blue" Height="48px" Text="Epic Build System" Width="320px" />
<br />
<asp:Label ID="Label_Welcome" runat="server" Height="32px" Width="800px" Font-Bold="True" Font-Names="Arial" Font-Size="Small" ForeColor="Blue" />
<br />
<asp:Label ID="Label_UserLevel" runat="server" Height="32px" Width="150px" Font-Bold="True" Font-Names="Arial" Font-Size="Small" ForeColor="Blue" Text="Select User Level:" />
<ajaxToolkit:ComboBox ID="ComboBox_UserDetail" runat="server" AutoPostback="true"
	AutoCompleteMode="None" DropDownStyle="DropDownList" CaseSensitive="False" 
	CssClass="Windows" OnSelectedIndexChanged="SelectedIndexChanged" 
	ToolTip="Select your desired usability level" >
	<asp:ListItem Text="Simple" />
	<asp:ListItem Text="Normal" />
	<asp:ListItem Text="Advanced" />
</ajaxToolkit:ComboBox>
<br />
<hr />
<br />

<% 
	if( ComboBox_UserDetail.SelectedIndex == 0 )
	{   
%>
<asp:Button ID="Button_Engine0" runat="server" Font-Bold="True" Font-Names="Arial" Font-Size="Large" Text="Trigger Engine Build" OnClick="Button_TriggerBuild_Click" />
<br />
<br />
<asp:Button ID="Button_Nano0" runat="server" Font-Bold="True" Font-Names="Arial" Font-Size="Large" Text="Trigger Nano Build" OnClick="Button_TriggerBuild_Click" />
<br />
<br />
<asp:Button ID="Button_Rift0" runat="server" Font-Bold="True" Font-Names="Arial" Font-Size="Large" Text="Trigger Rift Build" OnClick="Button_TriggerBuild_Click" />
<br />
<br />
<% 
	}
	else if( ComboBox_UserDetail.SelectedIndex == 1 )
	{
%>
<asp:Button ID="Button_Engine1" runat="server" Font-Bold="True" Font-Names="Arial" Font-Size="Large" Text="Trigger Engine Build" OnClick="Button_TriggerBuild_Click" />
<br />
<br />
<asp:Button ID="Button_Promote1" runat="server" Font-Bold="True" Font-Names="Arial" Font-Size="Large" Text="Promote Build" OnClick="Button_TriggerBuild_Click" />
<br />
<br />
<asp:Button ID="Button_UDK1" runat="server" Font-Bold="True" Font-Names="Arial" Font-Size="Large" Text="Trigger UDK Build" OnClick="Button_TriggerBuild_Click" />
<br />
<br />
<asp:Button ID="Button_Nano1" runat="server" Font-Bold="True" Font-Names="Arial" Font-Size="Large" Text="Trigger Nano Build" OnClick="Button_TriggerBuild_Click" />
<br />
<br />
<asp:Button ID="Button_Rift1" runat="server" Font-Bold="True" Font-Names="Arial" Font-Size="Large" Text="Trigger Rift Build" OnClick="Button_TriggerBuild_Click" />
<br />
<br />
<asp:Button ID="Button_Lemming1" runat="server" Font-Bold="True" Font-Names="Arial" Font-Size="Large" Text="Trigger Lemming Build" OnClick="Button_TriggerBuild_Click" />
<br />
<br />
<asp:Button ID="Button_RiftBeta1" runat="server" Font-Bold="True" Font-Names="Arial" Font-Size="Large" Text="Trigger Rift Beta Build" OnClick="Button_TriggerBuild_Click" />
<br />
<br />
<asp:Button ID="Button_Mobile1" runat="server" Font-Bold="True" Font-Names="Arial" Font-Size="Large" Text="Trigger Mobile Build" OnClick="Button_TriggerBuild_Click" />
<br />
<br />
<asp:Button ID="Button_CIS1" runat="server" Font-Bold="True" Font-Names="Arial" Font-Size="Large" Text="Trigger CIS Build" OnClick="Button_TriggerBuild_Click" />
<br />
<br />
<asp:Button ID="Button_Verification1" runat="server" Font-Bold="True" Font-Names="Arial" Font-Size="Large" Text="Trigger Verification Build" OnClick="Button_TriggerBuild_Click" />
<br />
<br />
<br /><asp:Button ID="Button_BuildCharts1" runat="server" Font-Bold="True" Font-Names="Arial" Font-Size="Large" Text="Build Performance Charts" OnClick="Button_TriggerBuild_Click" />
<br />
<br />
<% 
	}
	else if( ComboBox_UserDetail.SelectedIndex == 2 )
	{
%>
<asp:Button ID="Button_Engine2" runat="server" Font-Bold="True" Font-Names="Arial" Font-Size="Large" Text="Trigger Engine Build" OnClick="Button_TriggerBuild_Click" />
<br />
<br />
<asp:Button ID="Button_Promote2" runat="server" Font-Bold="True" Font-Names="Arial" Font-Size="Large" Text="Promote Build" OnClick="Button_TriggerBuild_Click" />
<br />
<br />
<asp:Button ID="Button_Example2" runat="server" Font-Bold="True" Font-Names="Arial" Font-Size="Large" Text="Trigger Example Build" OnClick="Button_TriggerBuild_Click" />
<br />
<br />
<asp:Button ID="Button_UDK2" runat="server" Font-Bold="True" Font-Names="Arial" Font-Size="Large" Text="Trigger UDK Build" OnClick="Button_TriggerBuild_Click" />
<br />
<br />
<asp:Button ID="Button_Nano2" runat="server" Font-Bold="True" Font-Names="Arial" Font-Size="Large" Text="Trigger Nano Build" OnClick="Button_TriggerBuild_Click" />
<br />
<br />
<asp:Button ID="Button_Rift2" runat="server" Font-Bold="True" Font-Names="Arial" Font-Size="Large" Text="Trigger Rift Build" OnClick="Button_TriggerBuild_Click" />
<br />
<br />
<asp:Button ID="Button_Lemming2" runat="server" Font-Bold="True" Font-Names="Arial" Font-Size="Large" Text="Trigger Lemming Build" OnClick="Button_TriggerBuild_Click" />
<br />
<br />
<asp:Button ID="Button_RiftBeta2" runat="server" Font-Bold="True" Font-Names="Arial" Font-Size="Large" Text="Trigger Rift Beta Build" OnClick="Button_TriggerBuild_Click" />
<br />
<br />
<asp:Button ID="Button_Mobile2" runat="server" Font-Bold="True" Font-Names="Arial" Font-Size="Large" Text="Trigger Mobile Build" OnClick="Button_TriggerBuild_Click" />
<br />
<br />
<asp:Button ID="Button_Sword2" runat="server" Font-Bold="True" Font-Names="Arial" Font-Size="Large" Text="Trigger Sword Build" OnClick="Button_TriggerBuild_Click" />
<br />
<br />
<asp:Button ID="Button_InfinityBlade12" runat="server" Font-Bold="True" Font-Names="Arial" Font-Size="Large" Text="Trigger Infinity Blade Build" OnClick="Button_TriggerBuild_Click" />
<br />
<br />
<asp:Button ID="Button_Maintenance2" runat="server" Font-Bold="True" Font-Names="Arial" Font-Size="Large" Text="Builder Maintenance" OnClick="Button_TriggerBuild_Click" />
<br />
<br />
<asp:Button ID="Button_Tools2" runat="server" Font-Bold="True" Font-Names="Arial" Font-Size="Large" Text="Trigger Tool Build" OnClick="Button_TriggerBuild_Click" />
<br />
<br />
<asp:Button ID="Button_CIS2" runat="server" Font-Bold="True" Font-Names="Arial" Font-Size="Large" Text="Trigger CIS Build" OnClick="Button_TriggerBuild_Click" />
<br />
<br />
<asp:Button ID="Button_Verification2" runat="server" Font-Bold="True" Font-Names="Arial" Font-Size="Large" Text="Trigger Verification Build" OnClick="Button_TriggerBuild_Click" />
<br />
<br />
<br /><asp:Button ID="Button_BuildCharts2" runat="server" Font-Bold="True" Font-Names="Arial" Font-Size="Large" Text="Build Performance Charts" OnClick="Button_TriggerBuild_Click" />
<br />
<br />
<% 
	}
%>
<hr /><br />
<asp:Button ID="Button_BuildStatus" runat="server" Font-Bold="True" Font-Names="Arial" Font-Size="X-Large" Text="Build Status" OnClick="Button_TriggerBuild_Click" />
<br />
<br />

</center>
<center>
<asp:UpdatePanel ID="ScriptUpdatePanel" runat="Server" UpdateMode="Conditional">
<ContentTemplate>
<fieldset>
<Legend style="color:BlueViolet"><b>Builds in Progress</b></Legend>
<asp:Repeater ID="Repeater_BuildLog" runat="server" OnItemCommand="Repeater_BuildLog_ItemCommand">
<ItemTemplate>

<table width="80%">
<tr><td align="center">
<asp:Label ID="Label_BuildLog1" runat="server" Font-Bold="True" ForeColor=<%# CheckConnected( DataBinder.Eval(Container.DataItem, "[\"CurrentTime\"]") ) %> Text=<%# DataBinder.Eval(Container.DataItem, "[\"Machine\"]") %> />
is building from ChangeList :
<asp:Label ID="Label_BuilderLog2" runat="server" Font-Bold="True" ForeColor=<%# CheckConnected( DataBinder.Eval(Container.DataItem, "[\"CurrentTime\"]") ) %> Text=<%# DataBinder.Eval(Container.DataItem, "[\"ChangeList\"]") %> />
<br />
<asp:Label ID="Label_BuilderLog3" runat="server" Font-Bold="True" ForeColor=<%# CheckConnected( DataBinder.Eval(Container.DataItem, "[\"CurrentTime\"]") ) %> Text=<%# DataBinder.Eval(Container.DataItem, "[\"CurrentStatus\"]") %> />
</td><td>
<asp:Label ID="Label_BuilderLog4" runat="server" Font-Bold="True" ForeColor="DarkGreen" Text=<%# DateDiff( DataBinder.Eval(Container.DataItem, "[\"BuildStarted\"]") ) %> />
</td><td>
Triggered by : 
<asp:Label ID="Label_BuilderLog5" runat="server" Font-Bold="True" ForeColor="DarkGreen" Text=<%# DataBinder.Eval(Container.DataItem, "[\"Operator\"]") %> />
</td><td align="center" Width="40%">
<asp:LinkButton ID="Button_StopBuild" runat="server" Font-Bold="True" Width="384" ForeColor="Red" Font-Size="Large" Text=<%# "Stop " + DataBinder.Eval(Container.DataItem, "[\"Description\"]") %> />
<ajaxToolkit:ConfirmButtonExtender ID="ConfirmButtonExtender1" runat="server" TargetControlID="Button_StopBuild" ConfirmText="Are you sure you want to stop the build?" /> 
</td></tr>
</table>

</ItemTemplate>
</asp:Repeater>

<asp:Timer ID="ScriptTimer" runat="server" Interval="2000" OnTick="ScriptTimer_Tick" />  
</fieldset>
</ContentTemplate>
</asp:UpdatePanel>

        &nbsp; &nbsp;

<asp:UpdatePanel ID="JobUpdatePanel" runat="Server" UpdateMode="Always">
<ContentTemplate>
<fieldset>
<Legend style="color:BlueViolet"><b>Jobs in Progress</b></Legend>
<asp:Label ID="Label_PendingCISTasks" runat="server" Font-Bold="True" ForeColor="DarkBlue" /><br /><br />
<asp:Repeater ID="Repeater_JobLog" runat="server">
<ItemTemplate>

<table width="80%">
<tr><td align="center">
<asp:Label ID="Label_JobLog1" runat="server" Font-Bold="True" ForeColor="DarkBlue" Text=<%# DataBinder.Eval(Container.DataItem, "[\"Machine\"]") %> />
<asp:Label ID="Label_JobLog2" runat="server" Font-Bold="True" ForeColor="DarkBlue" Text=<%# DateDiff2( DataBinder.Eval(Container.DataItem, "[\"BuildStarted\"]") ) %> />
:
<asp:Label ID="Label_JobLog3" runat="server" Font-Bold="True" ForeColor="DarkBlue" Text=<%# DataBinder.Eval(Container.DataItem, "[\"CurrentStatus\"]") %> />
</td>
</tr>
</table>

</ItemTemplate>
</asp:Repeater>
<asp:Timer ID="JobTimer" runat="server" Interval="2000" OnTick="JobTimer_Tick" />  
</fieldset>
</ContentTemplate>
</asp:UpdatePanel>

        &nbsp; &nbsp;
        
<asp:UpdatePanel ID="VerifyUpdatePanel" runat="Server" UpdateMode="Conditional">
<ContentTemplate>
<fieldset>
<Legend style="color:BlueViolet"><b>Verification Builds in Progress</b></Legend>
<asp:Repeater ID="Repeater_Verify" runat="server" OnItemCommand="Repeater_BuildLog_ItemCommand">
<ItemTemplate>

<table width="80%">
<tr><td align="center">
<asp:Label ID="Label_BuildLog1" runat="server" Font-Bold="True" ForeColor="DarkGreen" Text=<%# DataBinder.Eval(Container.DataItem, "[\"Machine\"]") %> />
is building from ChangeList :
<asp:Label ID="Label_BuilderLog2" runat="server" Font-Bold="True" ForeColor="DarkGreen" Text=<%# DataBinder.Eval(Container.DataItem, "[\"ChangeList\"]") %> />
<br />
<asp:Label ID="Label_BuilderLog3" runat="server" Font-Bold="True" ForeColor="DarkGreen" Text=<%# DataBinder.Eval(Container.DataItem, "[\"CurrentStatus\"]") %> />
</td><td>
<asp:Label ID="Label_BuilderLog4" runat="server" Font-Bold="True" ForeColor="DarkGreen" Text=<%# DateDiff( DataBinder.Eval(Container.DataItem, "[\"BuildStarted\"]") ) %> />
</td><td>
Triggered by : 
<asp:Label ID="Label_BuilderLog5" runat="server" Font-Bold="True" ForeColor="DarkGreen" Text=<%# DataBinder.Eval(Container.DataItem, "[\"Operator\"]") %> />
</td><td align="center" Width="40%">
<asp:LinkButton ID="Button_StopBuild" runat="server" Font-Bold="True" Width="384" ForeColor="Red" Font-Size="Large" Text=<%# "Stop " + DataBinder.Eval(Container.DataItem, "[\"Description\"]") %> />
<ajaxToolkit:ConfirmButtonExtender ID="ConfirmButtonExtender1" runat="server" TargetControlID="Button_StopBuild" ConfirmText="Are you sure you want to stop the build?" /> 
</td></tr>
</table>

</ItemTemplate>
</asp:Repeater>

<asp:Timer ID="VerifyTimer" runat="server" Interval="2000" OnTick="VerifyTimer_Tick" />  
</fieldset>
</ContentTemplate>
</asp:UpdatePanel>

        &nbsp; &nbsp;

<br />
    </center>
    <center>
   <asp:UpdatePanel ID="MainBranchUpdatePanel" runat="Server" UpdateMode="Always">
<ContentTemplate>
 
                <asp:Label ID="Label_Main" runat="server" Font-Bold="True" Font-Names="Arial" Font-Size="X-Large"
            Height="36px" Text="Main UnrealEngine3 Branch" Width="640px" ForeColor="BlueViolet"></asp:Label><br />
            
<asp:Repeater ID="Repeater_MainBranch" runat="server">
<ItemTemplate>
Last good build of
<asp:Label ID="Label_Status1" runat="server" Font-Bold="True" ForeColor="DarkBlue" Text=<%# DataBinder.Eval(Container.DataItem, "[\"Description\"]") %> />
was from ChangeList 
<asp:Label ID="Label_Status2" runat="server" Font-Bold="True" ForeColor="DarkBlue" Text=<%# DataBinder.Eval(Container.DataItem, "[\"LastGoodChangeList\"]") %> />
on 
<asp:Label ID="Label_Status3" runat="server" Font-Bold="True" ForeColor="DarkBlue" Text=<%# DataBinder.Eval(Container.DataItem, "[\"LastGoodDateTime\"]") %> />
<asp:Label ID="Label_Status5" runat="server" Font-Bold="True" ForeColor="Blue" Text=<%# DataBinder.Eval(Container.DataItem, "[\"DisplayLabel\"]") %> />
<asp:Label ID="Label_Status4" runat="server" Font-Bold="True" ForeColor="Green" Text=<%# DataBinder.Eval(Container.DataItem, "[\"Status\"]") %> />
<br />
</ItemTemplate>
</asp:Repeater>
</ContentTemplate>
</asp:UpdatePanel>

        &nbsp; &nbsp;

<asp:UpdatePanel ID="BuildersUpdatePanel" runat="Server" UpdateMode="Always">
<ContentTemplate>
<fieldset>
<Legend style="color:BlueViolet"><b>Available Builders</b></Legend>

<asp:Repeater ID="Repeater_Builders" runat="server">
<ItemTemplate>
<asp:Label ID="Label_Builder1" runat="server" Font-Bold="True" ForeColor=<%# CheckConnected( DataBinder.Eval(Container.DataItem, "[\"CurrentTime\"]") ) %> Text=<%# GetAvailability( DataBinder.Eval(Container.DataItem, "[\"Machine\"]"), DataBinder.Eval(Container.DataItem, "[\"CurrentTime\"]") ) %> />
<br />
</ItemTemplate>
</asp:Repeater>
</fieldset>
</ContentTemplate>
</asp:UpdatePanel>
</form>
</center>
   
</body>
</html>
