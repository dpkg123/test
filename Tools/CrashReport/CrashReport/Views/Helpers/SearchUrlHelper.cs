﻿using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;
using System.Web.Mvc;
using CrashReport.Models;
using System.Text;

namespace CrashReport.Views.Helpers
{

    public static class SearchUrlHelper
    {
                    
        public static MvcHtmlString SearchLink(this HtmlHelper html, PagingInfo pagingInfo, Func<int, string> pageUrl)
        {
        StringBuilder result = new StringBuilder();

            // go to first page
            TagBuilder FirstTag = new TagBuilder("a"); // Construct an <a> Tag
            FirstTag.MergeAttribute("href", pageUrl(pagingInfo.FirstPage));
            FirstTag.InnerHtml = "<<";

            result.AppendLine(FirstTag.ToString());


            return MvcHtmlString.Create(result.ToString());
        }
    }
}