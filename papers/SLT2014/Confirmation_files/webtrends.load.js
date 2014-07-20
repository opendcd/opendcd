// WebTrends SmartSource Data Collector Tag v10.4.12
// Copyright (c) 2013 Webtrends Inc.  All rights reserved.
// Tag Builder Version: 4.1.2.7
// Created: 2013.10.18
window.webtrendsAsyncInit=function(){
    var dcs=new Webtrends.dcs().init({
        dcsid:"dcss5bd0j6bv0hoa1vk1j6fzj_8z9x",
        domain:"statse.webtrendslive.com",
        timezone:1,
        i18n:true,
        adimpressions:true,
        adsparam:"WT.ac",
        offsite:true,
        download:true,
        downloadtypes:"xls,doc,pdf,txt,csv,zip,docx,xlsx,rar,gzip",
        onsitedoms:new RegExp("www.swiss.com"),
        plugins:{
            //hm:{src:"//s.webtrends.com/js/webtrends.hm.js"}
        }
        }).track();
};
(function(){
    var s=document.createElement("script"); s.async=true; s.src="/CMSContent/web/js/webtrends.min.js";
    var s2=document.getElementsByTagName("script")[0]; s2.parentNode.insertBefore(s,s2);
}());