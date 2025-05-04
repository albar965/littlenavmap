function toolbarAPI(pluginsHost) {
  var germaneValue = {
    "text": "value",
    "checkbox": "checked"
  };

  function assignEventHandlers(element, handle, id, source) {
    for(let eventtype in handle) {
      element["on" + eventtype] = function(eventObject) {
        pluginsHost.pluginToolbarItemDelegate(source, id, eventtype, element[germaneValue[element.type]], handle[eventtype].map(property => eventObject[property]));
      	eventObject.stopPropagation();
      };
    }
  }

  function createXFrame(config, source, toolbar, XCode) {
    var x = XCode();
    assignEventHandlers(x, config.handle, config.id, source);
    config.style ? x.style = config.style : !1;
    toolbar.appendChild(x);
    return x;
  }


  return {
    createToolbar: function(parentElement, title, type, source) {
      var toolbar = document.createElement("form");
      toolbar.className = "toolbar";
      toolbar.title = title;
      toolbar.setAttribute("data-type", type);
      toolbar.setAttribute("data-source", source);
      toolbar.onsubmit = eventObject => eventObject.preventDefault();
      parentElement.appendChild(toolbar);
      return toolbar;
    },
    button: function(config, source, toolbar) {
      createXFrame(config, source, toolbar, function() {
        var button = document.createElement("button");
        button.type = "button";
        button.innerText = config.text;
        return button;
      });
    },
    text: function(config, source, toolbar) {
      createXFrame(config, source, toolbar, function() {
        var input = document.createElement("input");
        input.type = "text";
        input.placeholder = config.text;
        return input;
      });
    },
    checkbox: function(config, source, toolbar) {
      var checkbox = createXFrame(config, source, toolbar, function() {
        var input = document.createElement("input");
        input.type = "checkbox";
        return input;
      });
      do {
        var id = "c" + ("" + Math.random()).substring(2);
      } while(document.getElementById(id) || toolbar.querySelector("#" + id));
      checkbox.id = id;
      var label = document.createElement("label");
      label.setAttribute("for", id);
      label.innerText = config.text;
      toolbar.appendChild(label);
    },
    gap: function(config, source, toolbar) {
      var span = document.createElement("span");
      span.className = "gap";
      toolbar.appendChild(span);
    }
  }
}