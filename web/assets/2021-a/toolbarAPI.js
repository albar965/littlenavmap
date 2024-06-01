/*
  returns :

    functions to let create toolbar(s) or entries therein

      - every entry creation function can be given a toolbar as second parameter
      - the to be created entry is added to the given toolbar or the default map
        toolbar otherwise
      - (if you did not pass a toolbar from an exclusive plugin or if you did pass
        a toolbar from an unobtrusive plugin an Error is thrown (see example plugin
        documentation))
      - every entry created has a random id beginning with "e"
      - every entry created on the default map toolbar has an attribute "data-source"
        being the plugin's name
      - every entry creation function's first parameter is a config object for that
        specific entry's type


  details :

    createToolbar
      parameter parentElement
        required, can be document.body, toolbar will be added to it as a child
      returns the created toolbars innermost container, that is a form, its
      outermost container has class "toolbar", form has id "pluginform" + number,
      number being the count of toolbars already existing in your plugin
      
    config object
      text
        relevant text of to be created entry if applicable, such as innerText,
        value or placeholder
      for
        toolbar element for which the label you are about to let get created is,
        if applicable
      handlerNames
        string of comma-separated event type to be handled's names, each name
        must be present as a key of the config object and its value be the
        function which you want to handle the event given by the event type's name
      attr
        every key's value in an object being the value of "attr" is assigned to
        an attribute being the name of the key the value is of of the entry,
        overriding every previously set attributes, thus you can give your own id,
        attribute "data-source" is not overridable
        
        
    createButton
      returns a button
      
    createTextInput
      returns an input of type text
      
    createCheckbox
      returns an input of type checkbox
      
    createLabel
      returns a label
      
    createGap
      returns a span having class "gap"
*/

function toolbarAPI() {
  var doc = document;

  function validifyToolbar(toolbar, source) {
    if(typeof toolbar === "undefined") {
      if(source.plugintype === "exclusive") {
        throw new Error("no toolbar given but toolbar required");
      }
      return doc.querySelector('iframe[name="contentiframe"]').contentDocument.querySelector("#pluginform");
    } else {
      if(["unobtrusive"].includes(source.plugintype)) {
        throw new Error("toolbar given but must be not given");
      }
    }
    return toolbar;
  }

  function assignEventHandlers(element, config) {
    if(typeof config.handlerNames === "string") {
      config.handlerNames.split(",").forEach(function(handlerName) {
        element["on" + handlerName.replace(/\s/g, "")] = config[handlerName];
      });
    }
  }

  function createXFrame(config, toolbar, source, XCode) {
    toolbar = validifyToolbar(toolbar, source);
    var x = XCode();
    assignEventHandlers(x, config);
    toolbar.appendChild(x);
    x.id = "e" + (Math.random() + "").substring(2);
    source.pluginname ? x.setAttribute("data-source", source.pluginname) : !1;
    if(config.attr) {
      Object.keys(config.attr).forEach(key => {key !== "data-source" ? x.setAttribute(key, config.attr[key]) : !1});
    }
    return x;
  }


  return {
    createToolbar: function(parentElement) {
      var toolbar = document.createElement("div");
      toolbar.className = "toolbar";
      var div = document.createElement("div");
      var form = document.createElement("form");
      var existingFormNumber = 0;
      while(document.querySelector("pluginform" + existingFormNumber) !== null) {
        ++existingFormNumber;
      }
      form.id = "pluginform" + existingFormNumber;
      form.name = form.id;
      div.appendChild(form);
      toolbar.appendChild(div);
      parentElement.appendChild(toolbar);
      while(parentElement.tagName.toLowerCase() !== "html") {
        parentElement = parentElement.parentElement;
      }
      enableToolbarIndicators(toolbar, parentElement.parentNode.defaultView);
      return form;
    },
    createButton: function(config, toolbar) {
      return createXFrame(config, toolbar, this["createButton"], function() {
        var button = document.createElement("button");
        button.type = "button";
        button.innerText = config.text;
        return button;
      });
    },
    createTextInput: function(config, toolbar) {
      return createXFrame(config, toolbar, this["createTextInput"], function() {
        var input = document.createElement("input");
        input.type = "text";
        input.placeholder = config.text;
        return input;
      });
    },
    createLabel: function(config, toolbar) {
      return createXFrame(config, toolbar, this["createLabel"], function() {
        var label = document.createElement("label");
        config.for ? label.setAttribute("for", config.for.id) : 0;
        label.innerText = config.text;
        return label;
      });
    },
    createCheckbox: function(config, toolbar) {
      return createXFrame(config, toolbar, this["createCheckbox"], function() {
        var input = document.createElement("input");
        input.type = "checkbox";
        input.value = config.text;
        return input;
      });
    },
    createGap: function(config, toolbar) {
      return createXFrame(config, toolbar, this["createGap"], function() {
        var span = document.createElement("span");
        span.className = "gap";
        return span;
      });
    }
  }
}

function enableToolbarIndicators(toolbar, closestWindow) {
  function toolbarIndicators(toolbar) {
    if(toolbar.firstElementChild.scrollWidth > toolbar.firstElementChild.clientWidth) {
      if(toolbar.firstElementChild.scrollLeft > 0) {
        toolbar.classList.add("indicator-scrollable-toleft");
        if(toolbar.firstElementChild.clientWidth + toolbar.firstElementChild.scrollLeft >= toolbar.firstElementChild.scrollWidth - 1) {
          toolbar.classList.remove("indicator-scrollable-toright");
        } else {
          toolbar.classList.add("indicator-scrollable-toright");
        }
      } else {
        toolbar.classList.remove("indicator-scrollable-toleft");
        toolbar.classList.add("indicator-scrollable-toright");
      }
    } else {
      toolbar.classList.remove("indicator-scrollable-toleft");
      toolbar.classList.remove("indicator-scrollable-toright");
    }
  }
  var eventHandler = (function(toolbar) {
    return function() {
      toolbarIndicators(toolbar);
    };
  })(toolbar);
  closestWindow.addEventListener("resize", eventHandler);
  toolbar.firstElementChild.addEventListener("scroll", eventHandler);
  toolbarIndicators(toolbar);
}