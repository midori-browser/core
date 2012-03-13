/**
 * An autosuggest textbox control.
 * from Nicholas C. Zakas (Author) example: http://www.nczonline.net/
 * Adopted for Midori by Alexander V. Butenko <a.butenka@gmail.com>
 */

function AutoSuggestControl(oTextbox /*:HTMLInputElement*/,
                            oProvider /*:SuggestionProvider*/) {
    /**
     * The currently selected suggestions.
     * @scope private
     */
    this.cur /*:int*/ = -1;
    /**
     * The dropdown list layer.
     * @scope private
     */
    this.layer = null;
    /**
     * Suggestion provider for the autosuggest feature.
     * @scope private.
     */
    this.provider /*:SuggestionProvider*/ = oProvider;
    /**
     * The textbox to capture.
     * @scope private
     */
    this.textbox /*:HTMLInputElement*/ = oTextbox;
    //initialize the control
    this.init();
}

/**
 * Autosuggests one or more suggestions for what the user has typed.
 * If no suggestions are passed in, then no autosuggest occurs.
 * @scope private
 * @param aSuggestions An array of suggestion strings.
 * @param bTypeAhead If the control should provide a type ahead suggestion.
 */
AutoSuggestControl.prototype.autosuggest = function (aSuggestions /*:Array*/) {
    if (aSuggestions.length > 0) {
        this.showSuggestions(aSuggestions);
    } else {
        this.hideSuggestions();
    }
};

/**
 * Creates the dropdown layer to display multiple suggestions.
 * @scope private
 */
AutoSuggestControl.prototype.createDropDown = function () {
    var sDiv = document.getElementById("suggestions_box");
    if (sDiv) {
        this.layer = sDiv;
        return;
    }
    this.layer = document.createElement("div");
    this.layer.className = "suggestions";
    this.layer.id = "suggestions_box";
    this.layer.style.visibility = "hidden";
    this.layer.style.width = this.textbox.offsetWidth;
    document.body.appendChild(this.layer);
};

/**
 * Gets the left coordinate of the textbox.
 * @scope private
 * @return The left coordinate of the textbox in pixels.
 */
AutoSuggestControl.prototype.getLeft = function () /*:int*/ {
    var oNode = this.textbox;
    var iLeft = 0;
    while (oNode.tagName != "BODY") {
        iLeft += oNode.offsetLeft;
        oNode = oNode.offsetParent;
    }
    return iLeft;
};

/**
 * Gets the top coordinate of the textbox.
 * @scope private
 * @return The top coordinate of the textbox in pixels.
 */
AutoSuggestControl.prototype.getTop = function () /*:int*/ {
    var oNode = this.textbox;
    var iTop = 0;
    while(oNode.tagName != "BODY") {
        iTop += oNode.offsetTop;
        oNode = oNode.offsetParent;
    }
    return iTop;
};

/**
 * Handles three keydown events.
 * @scope private
 * @param oEvent The event object for the keydown event.
 */
AutoSuggestControl.prototype.handleKeyDown = function (oEvent /*:Event*/) {
    switch(oEvent.keyCode) {
        case 38: //up arrow
            this.previousSuggestion();
            break;
        case 40: //down arrow
            this.nextSuggestion();
            break;
        case 13: //enter
            this.hideSuggestions();
            break;
    }
};

/**
 * Handles keyup events.
 * @scope private
 * @param oEvent The event object for the keyup event.
 */
AutoSuggestControl.prototype.handleKeyUp = function (oEvent /*:Event*/) {
    var iKeyCode = oEvent.keyCode;
    if (iKeyCode == 8 || iKeyCode == 46) {
        this.provider.requestSuggestions(this);
    //make sure not to interfere with non-character keys
    } else if (iKeyCode < 32 || (iKeyCode >= 33 && iKeyCode < 46) || (iKeyCode >= 112 && iKeyCode <= 123) ) {
        //ignore
    } else if (oEvent.ctrlKey) {
        this.hideSuggestions();
    } else {
        //request suggestions from the suggestion provider
        this.provider.requestSuggestions(this);
    }
};

/**
 * Hides the suggestion dropdown.
 * @scope private
 */
AutoSuggestControl.prototype.hideSuggestions = function () {
    this.layer.style.visibility = "hidden";
};

/**
 * Highlights the given node in the suggestions dropdown.
 * @scope private
 * @param oSuggestionNode The node representing a suggestion in the dropdown.
 */
AutoSuggestControl.prototype.highlightSuggestion = function (oSuggestionNode) {
    for (var i=0; i < this.layer.childNodes.length; i++) {
        var oNode = this.layer.childNodes[i];
        if (oNode == oSuggestionNode) {
            oNode.className = "current"
        } else if (oNode.className == "current") {
            oNode.className = "";
        }
    }
};

/**
 * Initializes the textbox with event handlers for
 * auto suggest functionality.
 * @scope private
 */
AutoSuggestControl.prototype.init = function () {
    //save a reference to this object
    var oThis = this;

    //assign the onkeyup event handler
    this.textbox.onkeyup = function (oEvent) {
        //check for the proper location of the event object
        if (!oEvent) {
            oEvent = window.event;
        }
        //call the handleKeyUp() method with the event object
        oThis.handleKeyUp(oEvent);
    };

    //assign onkeydown event handler
    this.textbox.onkeydown = function (oEvent) {
        //check for the proper location of the event object
        if (!oEvent) {
            oEvent = window.event;
        }
        //call the handleKeyDown() method with the event object
        oThis.handleKeyDown(oEvent);
    };

    //assign onblur event handler (hides suggestions)
    if (!this.textbox.onblur)
      this.textbox.onblur = function () { oThis.hideSuggestions(); };
    if (!this.textbox.onclick)
      this.textbox.onclick = function () { oThis.hideSuggestions(); };

    //create the suggestions dropdown
    this.createDropDown();
};

/**
 * Highlights the next suggestion in the dropdown and
 * places the suggestion into the textbox.
 * @scope private
 */
AutoSuggestControl.prototype.nextSuggestion = function () {
    var cSuggestionNodes = this.layer.childNodes;

    if (!cSuggestionNodes.length)
        return;

    if (this.cur == cSuggestionNodes.length-1)
        this.cur = -1;

    var oNode = cSuggestionNodes[++this.cur];
    this.highlightSuggestion(oNode);
    this.textbox.value = oNode.firstChild.nodeValue;
};

/**
 * Highlights the previous suggestion in the dropdown and
 * places the suggestion into the textbox.
 * @scope private
 */
AutoSuggestControl.prototype.previousSuggestion = function () {
    var cSuggestionNodes = this.layer.childNodes;

    if (!cSuggestionNodes.length)
        return;

    if (this.cur == -1 || this.cur == 0)
        this.cur = cSuggestionNodes.length;

    var oNode = cSuggestionNodes[--this.cur];
    this.highlightSuggestion(oNode);
    this.textbox.value = oNode.firstChild.nodeValue;
};

/**
 * Builds the suggestion layer contents, moves it into position,
 * and displays the layer.
 * @scope private
 * @param aSuggestions An array of suggestions for the control.
 */
AutoSuggestControl.prototype.showSuggestions = function (aSuggestions /*:Array*/) {
    var oThis  = this;
    var oDiv = null;
    this.layer.innerHTML = "";  //clear contents of the layer

    for (var i=0; i < aSuggestions.length; i++) {
        oDiv = document.createElement("div");
        oDiv.appendChild(document.createTextNode(aSuggestions[i]));
        this.layer.appendChild(oDiv);
    }

    this.layer.style.left = this.getLeft() + "px";
    this.layer.style.top = (this.getTop()+this.textbox.offsetHeight) + "px";
    this.layer.style.visibility = "visible";
    this.layer.style.position = "absolute";
    this.layer.onmousedown =
    this.layer.onmouseup =
    this.layer.onmouseover = function (oEvent) {
        var oEvent = oEvent || window.event;
        var oTarget = oEvent.target || oEvent.srcElement;

        if (oEvent.type == "mousedown") {
            oThis.textbox.value = oTarget.firstChild.nodeValue;
            oThis.hideSuggestions();
        } else if (oEvent.type == "mouseover") {
            oThis.highlightSuggestion(oTarget);
        } else {
            oThis.textbox.focus();
        }
    };
};

/**
 * Request suggestions for the given autosuggest control.
 * @scope protected
 * @param oAutoSuggestControl The autosuggest control to provide suggestions for.
 */
FormSuggestions.prototype.requestSuggestions = function (oAutoSuggestControl /*:AutoSuggestControl*/) {
    var aSuggestions = [];
    var sTextboxValue = oAutoSuggestControl.textbox.value.toLowerCase();

    if (!this.suggestions)
        return;

    //search for matching suggestions
    if (sTextboxValue.length)
        for (var i=0; i < this.suggestions.length; i++) {
             if (this.suggestions[i].toLowerCase().indexOf(sTextboxValue) == 0) {
                 aSuggestions.push(this.suggestions[i]);
        }
    }
    //provide suggestions to the control
    oAutoSuggestControl.autosuggest(aSuggestions);
};

function initSuggestions () {
    var inputs = document.getElementsByTagName ("input");
    if (inputs.length == 0)
        return false;

    for (var i=0;i<inputs.length;i++)
    {
        var ename = inputs[i].getAttribute("name");
        var eid = inputs[i].getAttribute("id");
        var autocomplete = inputs[i].getAttribute ("autocomplete");

        if (!ename && eid)
            ename=eid;
        if (inputs[i].type == "text" && autocomplete != "off")
            var smth = new AutoSuggestControl (inputs[i], new FormSuggestions (ename));
    }
    return true;
};
