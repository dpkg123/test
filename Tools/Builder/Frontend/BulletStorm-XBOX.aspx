<%@ Page Language="C#" AutoEventWireup="true" CodeFile="Bulletstorm-XBOX.aspx.cs" Inherits="Bulletstorm" Debug="true" %>

<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">

<html xmlns="http://www.w3.org/1999/xhtml" >
<head runat="server">
    <title>Trigger a Bulletstorm Build from the BS branch</title>
</head>
<body>

<center>
    <form id="form1" runat="server">
    <div>
        <asp:Label ID="Label1" runat="server" Font-Bold="True" Font-Names="Arial" Font-Size="XX-Large"
            Height="56px" Text="Epic Build System - Trigger Build from BS branch" Width="640px" ForeColor="Blue"></asp:Label><br /><br />
        <asp:Label ID="Label_Welcome" runat="server" Height="32px" Width="384px" Font-Bold="True" Font-Names="Arial" Font-Size="Small" ForeColor="Blue"></asp:Label><br />
        <br />
            <asp:SqlDataSource ID="BuilderDBSource_Trigger" runat="server" ConnectionString="<%$ ConnectionStrings:BuilderConnectionString %>"
            SelectCommandType="StoredProcedure" SelectCommand="GetTriggerable">
            <SelectParameters>
            <asp:QueryStringParameter Name="MinAccess" Type="Int32" DefaultValue="212000" />
            <asp:QueryStringParameter Name="MaxAccess" Type="Int32" DefaultValue="212999" />
            </SelectParameters>
            </asp:SqlDataSource>
        <asp:Repeater ID="BuilderDBRepeater_Trigger" runat="server" DataSourceID="BuilderDBSource_Trigger" OnItemCommand="BuilderDBRepeater_ItemCommand">
        <ItemTemplate>
        <asp:Button runat="server" Font-Bold="True" Height="26px" Text=<%# DataBinder.Eval(Container.DataItem, "[\"Description\"]") %> CommandName=<%# DataBinder.Eval(Container.DataItem, "[\"ID\"]") %> CommandArgument=<%# DataBinder.Eval(Container.DataItem, "[\"Machine\"]") %> OnPreRender="BuilderDBRepeater_OnPreRender" Width="384px" />
        <br /><br />
        </ItemTemplate>
        </asp:Repeater>
        </div>
        
 <asp:SqlDataSource ID="BuilderDBSource_Commands" runat="server" ConnectionString="<%$ ConnectionStrings:BuilderConnectionString %>"
SelectCommandType="StoredProcedure" SelectCommand="SelectBuilds">
<SelectParameters>
<asp:QueryStringParameter Name="DisplayID" Type="Int32" DefaultValue="0" />
<asp:QueryStringParameter Name="DisplayDetailID" Type="Int32" DefaultValue="210000" />
</SelectParameters>
 </asp:SqlDataSource>
 
<asp:Repeater ID="Repeater_Commands" runat="server" DataSourceID="BuilderDBSource_Commands">
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
    </form>

</center>    
</body>
</html>

