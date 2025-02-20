﻿<%@ Control Language="C#" Inherits="System.Web.Mvc.ViewUserControl<BuggsViewModel>" %>
<%@ Import Namespace="CrashReport.Models" %>





     <table id ='CrashesTable' style="table-layout:auto; width:auto; min-width:1300px"  cellspacing="0" cellpadding="0">
     <tr id="SelectBar" style='background-color: #e8eef4; border: none; !important' border=0> 
        <td colspan='15' style='background-color: #e8eef4; border: none !important; border-right:none !important;' border=0>Select: <span class='SelectorLink' id='CheckAll' >All</span><span class='SelectorLink' id="CheckNone">None</span>
        <div class="PaginationBox">
        
        <%= Html.PageLinks(Model.PagingInfo, i => Url.Action("", new { page = i, SearchQuery = Model.Query, SortTerm = Model.Term, SortOrder = Model.Order, UserGroup = Model.UserGroup, DateFrom = Model.DateFrom, DateTo = Model.DateTo }))%>
    </div>
    <!-- form starts and ends in site.master-->
         <!-- <div id='SetForm'>
             
                    <%=Html.HiddenFor(u => u.UserGroup)%>
                    <%=Html.Hidden("SortTerm", Model.Term) %>
                      <%=Html.Hidden("SortOrder", Model.Order) %>
                      <%=Html.Hidden("Page", Model.PagingInfo.CurrentPage) %>
                      <%=Html.Hidden("PageSize", Model.PagingInfo.PageSize) %>
                      <%=Html.Hidden("SearchQuery", Model.Query)%>
                          <%=Html.HiddenFor(m => m.DateFrom) %>
                    <%=Html.HiddenFor(m => m.DateTo)%>
    <Div id="SetInput">
                     <span id="set-status" style="vertical-align: middle;">Set Status</span>
     
                    <select name="SetStatus" id="SetStatus" >
                        <option selected="selected" value=""></option>
	                    <option  value="Unset">Unset</option>
                        <option value="Reviewed">Reviewed</option>
	                    <option value="New">New</option>
	                    <option value="Coder">Coder</option>
	                    <option value="Tester">Tester</option>

                    </select>
                    <input type="submit" name="SetStatusSubmit" value="Set" class="SetButton"   />

                   <span id="set-ttp" style="">TTP</span>
                    <input name="SetTTP" type="text" id="ttp-text-box"   />
                    <input type="submit" name="SetTTPSubmit" value="Set" class="SetButton" />
    
                   <span id="set-fixed-in" style="">FixedIn</span>
                    <input name="SetFixedIn" type="text" id="fixed-in-text-box" />
                    <input type="submit" name="SetFixedInSubmit" value="Set" class="SetButton" />
                    
       </div>

        </div>-->
    
    </td>
     </tr>
     <tr>
        <!--<th>&nbsp;</th> -->
        <!-- if you add a new column be sure to update the switch statement in the repository SortBy function -->
        <th style='width: 6em;'><%=Url.TableHeader("Id", "Id", Model)%></th>
         <!--<th>Username</th>--> 
            <th style='width: 15em'><%=Url.TableHeader("Crashes In Time Frame", "CrashesInTimeFrame", Model)%></th>
            <th style='width: 12em'><%=Url.TableHeader("Latest Crash", "LatestCrash", Model)%></th>
            <th style='width: 12em;'><%=Url.TableHeader("First Crash", "FirstCrash", Model)%></th> 
             <th style='width: 11em;'><%=Url.TableHeader("# of Crashes", "NumberOfCrashes", Model)%></th> 
            <th style='width: 11em;'><%=Url.TableHeader("Users Affected", "NumberOfUsers", Model)%></th> 
             <th style='width: 14em;'><%=Url.TableHeader("Pattern", "Pattern", Model)%></th> 
            <th><%=Url.TableHeader("Status", "Status", Model)%></th>
            <th><%=Url.TableHeader("FixedCL#", "FixedChangeList", Model)%></th>
          </tr>
     <%try
       { %>
          <%
         int iter = 0;

         foreach (var result in Model.Results)
         {
             Bugg b = result.Bugg;
             b.CrashesInTimeFrame = result.CrashesInTimeFrame;
 
               %>
               <tr class='BuggRow'
               >
                 <!-- <td class="CrashTd"><INPUT TYPE="CHECKBOX" Value="<%=iter %>"NAME="<%=b.Id%>" id="CHECKBOX1" class='input CrashCheckBox' ></td> -->
                <td class="Id" ><%=Html.ActionLink(b.Id.ToString(), "Show", new { controller = "buggs", id = b.Id }, null)%></td> 
              
                     <td><%=b.CrashesInTimeFrame%></td>
                     <td><%=b.TimeOfLastCrash%></td>
                     <td><%=b.TimeOfFirstCrash%></td>
                        <td> <%=b.NumberOfCrashes%> </td>
                     <td><%=b.NumberOfUsers%></td>
                 
                     
                    <td class="CallStack"  >
                  <div style="clip : auto; ">
                        <div id='Div1' class='TrimmedCallStackBox'>

                        <% 
         var FunctionCalls = b.GetFunctionCalls(20);
         int i = 0;
         foreach (string FunctionCall in FunctionCalls)
         {
             var fc = FunctionCall;
             if (FunctionCall.Length > 48)
             {
                 fc = FunctionCall.Substring(0, 48);
             }
                            %>
                                <span class = "FunctionName">
                                    <%=Html.Encode(fc)%>
                                </span><br />
                        <%
         i++;
         if (i > 3) break;
         } %>
                        </div>
                            <a class='FullCallStackTrigger' ><span class='FullCallStackTriggerText'>Full Callstack</span>
                                <div id='<%=b.Id %>-FullCallStackBox' class='FullCallStackBox'>
                                    <!--<span id="<%=b.Id %>-Less" class='LessCallStackBox'>less</span>-->
                                    <%foreach (string FunctionCall in FunctionCalls)
                                      {%>
                                                <span class = "FunctionName">
                                                  <%=Html.Encode(FunctionCall)%>
                                                </span><br />

                                    <%} %>
                            
                                </div>
                            </a>
                </div>
                </td> 
                <td><%=b.Status%></td>
                <td><%=b.FixedChangeList%></td>
               </tr>

               
        <%} %>
        <%}
       catch (NullReferenceException)
       {%> 
        <tr><td colspan="9">No Results Found. Please try adjusting your search. Or contact support.</td></tr>
       <%} %>
       
             </Table>




    