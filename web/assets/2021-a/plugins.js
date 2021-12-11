function findPlugins() {
  var currentlyStartedPlugin;
  var stopMethods = {};

  var top, parent;

  function type_background() {
    return {
      disable_AccessPrevention: (function(currentlyStartedPlugin) {
        return function() {
          var iframe = document.querySelector('iframe[data-id="' + currentlyStartedPlugin + '"]');
          iframe.contentWindow.parent = parent;
          iframe.contentWindow.top = top;
        };
      })(currentlyStartedPlugin)
    };
  }
  function type_unobtrusive() {
    var theToolbarAPI = toolbarAPI();
    delete theToolbarAPI.createToolbar;
    var descriptor = Object.create(null);
    descriptor.value = currentlyStartedPlugin;
    for(var funcname in theToolbarAPI) {
      Object.defineProperty(theToolbarAPI[funcname], "pluginname", descriptor);
    }
    return {
      run: (function(currentlyStartedPlugin) {
        return function() {
          var iframe = document.querySelector('iframe[data-id="' + currentlyStartedPlugin + '"]');
          iframe.className = "overlaying-content";
          iframe.setAttribute("allowtransparency", "true");   // appears to be unnecessary
          iframe.style.width = "100%";
          iframe.style.height = "calc(100% - 52px)";    // 52px is height of map HTML #header
          iframe.style.top = "52px";
          iframe.style.zIndex = "9999";
          iframe.style.visibility = "visible";
        };
      })(currentlyStartedPlugin),
      toolbarAPI: theToolbarAPI
    };
  }
  function type_exclusive() {
    var theToolbarAPI = toolbarAPI();
    var descriptor = Object.create(null);
    descriptor.value = "exclusive";
    for(var funcname in theToolbarAPI) {
      Object.defineProperty(theToolbarAPI[funcname], "plugintype", descriptor);
    }
    return {
      run: (function(currentlyStartedPlugin) {
        return function() {
          var iframe = document.querySelector('iframe[data-id="' + currentlyStartedPlugin + '"]');
          iframe.className = "displayable-content";
          iframe.setAttribute("allowtransparency", "false");   // appears to have no effect
          iframe.style.background = "#fff";
          iframe.style.pointerEvents = "all";
          iframe.style.width = "100%";
          iframe.style.height = "100%";
          iframe.style.zIndex = "99999";
          iframe.style.visibility = "visible";
          if(document.querySelector("#exclusivePlugins") === null) {
            var select = document.createElement("select");
            select.id = "exclusivePlugins";
            var option = document.createElement("option");
            option.innerText = "no plugin";
            option.value = "theMap";
            select.add(option);
            option = document.createElement("option");
            option.innerText = currentlyStartedPlugin;
            option.value = currentlyStartedPlugin;
            select.add(option);
            document.querySelector("#toolbarOuter").appendChild(select);
            select.onchange = function() {
              Array.prototype.forEach.call(document.querySelectorAll(".displayable-content"), function(dc) {
                dc.style.display = "none";
              });
              document.querySelector('iframe[data-id="' + this.value + '"]').style.display = "block";
            };
          } else {
            var option = document.createElement("option");
            option.innerText = currentlyStartedPlugin;
            option.value = currentlyStartedPlugin;
            var select = document.querySelector("#exclusivePlugins");
            select.add(option);
          }
          select.selectedIndex = select.childElementCount - 1;
          select.dispatchEvent(new Event("change"));
        };
      })(currentlyStartedPlugin),
      toolbarAPI: theToolbarAPI
    };
  }
  function plugin(type, stop_Method) {
    if(type === type_background || type === type_unobtrusive || type === type_exclusive) {
      if(typeof stop_Method !== "function") {
        return "stop_Method invalid";
      }
      stopMethods[currentlyStartedPlugin] = stop_Method;
      var rv = type();
      currentlyStartedPlugin = "";
      return rv;
    }
    return "type invalid";
  }
  // use functions to prevent plugin change a value or rely on a current specific implemented value
  // also this allows having the code needed for setting up each types particularities be executable directly.
  plugin.TYPE_BACKGROUND = type_background;
  plugin.TYPE_UNOBTRUSIVE = type_unobtrusive;
  plugin.TYPE_EXCLUSIVE = type_exclusive;

  function initPlugin() {
    currentlyStartedPlugin = this.dataset.id;
    try {
      top = this.contentWindow.top;
      delete this.contentWindow.top;            // this is currently in vain: no web browser support
      this.contentWindow.top = function () {
        return {};
      };
      parent = this.contentWindow.parent;
      delete this.contentWindow.parent;
      this.contentWindow.parent = this.contentWindow.top;
      this.contentWindow.init(plugin, MAP_VERSION);
    } catch(e) {
      switch(e) {
        case "aborted by user":
        case "cancelled by user":
                                  break;
        default: alert("Plugin " + this.dataset.id + " failed to initialise.");
      }
      document.querySelector('input[value="' + this.dataset.id + '"]').click();
    }
  }

  function loadPlugin() {
    if(this.checked) {
      var iframe = document.createElement("iframe");
      iframe.setAttribute("data-id", this.value);
      iframe.onload = initPlugin;
      iframe.style = "position:absolute;visibility:hidden;pointer-events:none;top:0;left:0;z-index:-1;width:1px;height:1px";
      iframe.src = "/plugins/" + this.value + "/index.html";
      document.querySelector("#contentContainer").appendChild(iframe);
    } else {
      try {
        stopMethods[this.value]();
      } catch(e) {}
      Array.prototype.forEach.call(document.querySelector('iframe[data-id="theMap"]').contentDocument.querySelectorAll('#pluginform [data-source="' + this.value + '"]'), function(toolbarEntry) {
        toolbarEntry.parentElement.removeChild(toolbarEntry);
      });
      var iframe = document.querySelector('iframe[data-id="' + this.value + '"]');
      if(iframe.style.display === "block") {
        document.querySelector('iframe[data-id="theMap"]').style.display = "block";
      }
      iframe.parentElement.removeChild(iframe);
      var select = document.querySelector("#exclusivePlugins");
      if(select !== null) {
        var option = select.querySelector('option[value="' + this.value + '"]');
        if(option !== null) {
          select.remove(Array.prototype.indexOf.call(select, option));
          if(select.childElementCount < 2) {
            select.parentElement.removeChild(select);
          }
        }
      }
    }
  }

  var pluginbutton = document.querySelector("#buttonPlugins");
  if(pluginbutton === null) {
    fetch("/plugins").then(function(response) {
      return response.text();
    }).then(function(text) {
      var pluginlist = document.createElement("div");
      text.split("/").forEach(function(dir) {
        if(dir.substr(0, "example-".length) !== "example-" || location.search.substr(1) === "debug") {
          var plugin = document.createElement("div");
          plugin.className = "selectable-plugin";
          var checkbox = document.createElement("input");
          var id = "a" + (Math.random() + "").substr(2);
          checkbox.type = "checkbox";
          checkbox.id = id;
          checkbox.value = dir;
          var label = document.createElement("label");
          label.setAttribute("for", id);
          label.innerText = dir;
          plugin.appendChild(checkbox);
          plugin.appendChild(label);
          pluginlist.appendChild(plugin);
        }
      });
      if(pluginlist.childElementCount) {
        pluginlist.id = "selectablePlugins";
        var menuentry = document.createElement("button");
        menuentry.id = "buttonPlugins";
        menuentry.setAttribute("data-activateable", "false");
        var menuname = document.createElement("span");
        menuname.innerText = "Plugins";
        menuentry.appendChild(menuname);
        menuentry.appendChild(pluginlist);
        document.querySelector("#toolbarOuter").appendChild(menuentry);
        Array.prototype.forEach.call(document.querySelectorAll(".selectable-plugin input"), function(checkbox) {
          checkbox.addEventListener("click", loadPlugin);
        });
      }
    });
  }
}