function toolbarAPI() {
  // config object documentation:
  /*
    config = {
      text: relevant text of to be created element if applicable, such as innerText, value or placeholder,
      for: toolbar element for which the label you are about to let get created is, if applicable,
      handlerNames: string of comma-separated event handler names, each name should be present as a property of config and be the function which you want to handle the event given by the event handler name
    }
  */

  var doc = document;

  function validifyToolbar(toolbar, source) {
    if(typeof toolbar === "undefined") {
      if(source.plugintype === "exclusive") {
        throw new Error("no toolbar given but toolbar required");
      }
      return doc.querySelector('iframe[data-id="theMap"]').contentDocument.querySelector("#pluginform");
    } else {
      if(source.plugintype !== "exclusive") {
        throw new Error("toolbar given but must be not given");
      }
    }
    return toolbar;
  }

  function assignEventHandlers(element, config) {
    if(typeof config.handlerNames === "string") {
      config.handlerNames.split(",").forEach(function(handlerName) {
        element["on" + handlerName] = config[handlerName];
      });
    }
  }

  function createXFrame(config, toolbar, source, XCode) {
    toolbar = validifyToolbar(toolbar, source);
    var x = XCode();
    assignEventHandlers(x, config);
    toolbar.appendChild(x);
    x.id = "e" + (Math.random() + "").substr(2);
    x.setAttribute("data-source", source.pluginname);
    return x;
  }

  // every creation function except createToolbar can be given a toolbar as second parameter
  // if you did not pass a toolbar from an exclusive plugin or if you did pass a toolbar form an unobtrusive plugin an Error is thrown (thus don't pass anything as second parameter from an unobtrusive plugin)
  // the to be created element is already added to the toolbar or the default map toolbar depending on plugin type
  return {
    // returns the created toolbars innermost container, requires the element the toolbar is to be put in and it is put into it
    createToolbar: function(parentElement) {
      var toolbar = document.createElement("div");
      toolbar.className = "toolbar";
      var div = document.createElement("div");
      var form = document.createElement("form");
      var existingFormNumber = 0;
      while(document.querySelector("pluginform" + existingFormNumber) !== null) {
        existingFormNumber++;
      }
      form.id = "pluginform" + existingFormNumber;
      form.name = form.id;
      div.appendChild(form);
      toolbar.appendChild(div);
      parentElement.appendChild(toolbar);
      var closestWindow = parentElement.parentElement;
      while(parentElement.tagName.toLowerCase() !== "html") {
        parentElement = parentElement.parentElement;
      }
      enableToolbarIndicators(toolbar, parentElement.parentNode.defaultView);
      return form;
    },
    // returns a button
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