/*
 Copyright (C) 2014 Alexander V. Butenko <a.butenka@gmail.com>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
 */
function getElementsByAttribute (strTagName, strAttributeName, arrAttributeValue) {
    var arrElements = document.getElementsByTagName (strTagName);
    var arrReturnElements = new Array();
    for (var j=0; j<arrAttributeValue.length; j++) {
        var strAttributeValue = arrAttributeValue[j];
        for (var i=0; i<arrElements.length; i++) {
             var oCurrent = arrElements[i];
             var oAttribute = oCurrent.getAttribute && oCurrent.getAttribute (strAttributeName);
             if (oAttribute && oAttribute.length > 0 && strAttributeValue.indexOf (oAttribute) != -1)
                 arrReturnElements.push (oCurrent);
        }
    }
    return arrReturnElements;
};

function hideElementBySrc (uris) {
    var oElements = getElementsByAttribute('img', 'src', uris);
    if (oElements.length == 0)
        oElements = getElementsByAttribute ('iframe', 'src', uris);
    for (var i=0; i<oElements.length; i++) {
        oElements[i].style.visibility = 'hidden !important';
        oElements[i].style.width = '0';
        oElements[i].style.height = '0';
    }
};
