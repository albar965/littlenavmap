function findPlugins() {
  new (function() {
    var we = this;
    
    var currentlyStartedPlugin;
    var stopMethods = {};

    var types = {
      type_background:
        function () {
          if(this !== we) {
            console?.error("no");
            return;
          }
          return {
            request_MapAccess:
              (function(currentlyStartedPlugin) {
                return function(reason) {
                  var iframe = document.querySelector('iframe[data-id="' + currentlyStartedPlugin + '"]');
                  iframe.setAttribute("data-type", "background");
                  if(sessionStorage.getItem("mapAccessGranted_" + currentlyStartedPlugin) === "true" || confirm("The plugin\n\n\"" + currentlyStartedPlugin + "\"\n\nis requesting access to the map.\n\nReason given:\n\n\"" + reason + "\"\n\nGrant it?\n\n\nNote:\n\nThis is a courtesy question only.\nBrowsers don't allow dynamically switching the\nsecurity settings of the technology employed\nto enable easy development of plugins.\nA plugin can change the map nevertheless if\nits author so instructed it.\n\nThis might change in the future.")) {
                    sessionStorage.setItem("mapAccessGranted_" + currentlyStartedPlugin, true);
                    return document.querySelector("iframe[name=contentiframe]").contentDocument;
                  } else {
                    return false;
                  }
                };
              })(currentlyStartedPlugin)
          };
        },
      type_unobtrusive:
        function () {
          if(this !== we) {
            console?.error("no");
            return;
          }
          var theToolbarAPI = toolbarAPI();
          delete theToolbarAPI.createToolbar;
          var descriptor = Object.create(null);
          descriptor.value = currentlyStartedPlugin;
          for(var funcname in theToolbarAPI) {
            Object.defineProperty(theToolbarAPI[funcname], "pluginname", descriptor);
          }
          return {
            run:
              (function(currentlyStartedPlugin) {
                return function() {
                  var iframe = document.querySelector('iframe[data-id="' + currentlyStartedPlugin + '"]');
                  iframe.className = "overlaying-content";
                  iframe.setAttribute("data-type", "unobtrusive");
                  iframe.setAttribute("allowtransparency", "true");   // appears to be unnecessary
                  iframe.style.display = "block";
                  iframe.style.width = "100%";
                  iframe.style.height = "calc(100% - 50px)";    // 50px is height of map HTML #header
                  iframe.style.top = "50px";
                  iframe.style.visibility = "visible";
                  iframe.style.zIndex = "";
                };
              })(currentlyStartedPlugin),
            toolbarAPI:
              theToolbarAPI
          };
        },
      type_exclusive:
        function () {
          if(this !== we) {
            console?.error("no");
            return;
          }
          var theToolbarAPI = toolbarAPI();
          var descriptor = Object.create(null);
          descriptor.value = "exclusive";
          for(var funcname in theToolbarAPI) {
            Object.defineProperty(theToolbarAPI[funcname], "plugintype", descriptor);
          }
          return {
            run:
              (function(currentlyStartedPlugin) {
                return function() {
                  var iframe = document.querySelector('iframe[data-id="' + currentlyStartedPlugin + '"]');
                  iframe.className = "displayable-content";
                  iframe.setAttribute("data-type", "exclusive");
                  iframe.setAttribute("allowtransparency", "false");   // appears to have no effect
                  iframe.style.background = "#fff";
                  iframe.style.pointerEvents = "all";
                  iframe.style.width = "100%";
                  iframe.style.height = "100%";
                  iframe.style.visibility = "visible";
                  iframe.style.zIndex = "";
                  if(document.querySelector("#exclusivePlugins") === null) {
                    var select = document.createElement("select");
                    select.id = "exclusivePlugins";
                    var option = document.createElement("option");
                    option.innerText = "no exclusive plugin";
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
            toolbarAPI:
              theToolbarAPI
          };
        }
    };
    
    
    function plugin(type, stop_Method) {
      if([types.type_background, types.type_unobtrusive, types.type_exclusive].includes(type)) {
        if(typeof stop_Method !== "function") {
          throw "stop_Method invalid";
        }
        stopMethods[currentlyStartedPlugin] = stop_Method;
        var rv = type.call(this);
        currentlyStartedPlugin = "";
        return rv;
      }
      throw "callback type invalid";
    }

    function initPlugin() {
      try {
        var volatileDelegate = (function(currentlyInitedPlugin) {
          return function () {
            currentlyStartedPlugin = currentlyInitedPlugin;
            return plugin.apply(we, arguments);
          }
        })(this.dataset.id);
        // use functions to prevent plugin change a value or rely on a current specific implemented value
        // also this allows having the code needed for setting up each types particularities be executable directly.
        volatileDelegate.TYPE_BACKGROUND = types.type_background;
        volatileDelegate.TYPE_UNOBTRUSIVE = types.type_unobtrusive;
        volatileDelegate.TYPE_EXCLUSIVE = types.type_exclusive;
        this.setAttribute("sandbox", "allow-forms allow-modals allow-orientation-lock allow-presentation allow-scripts allow-same-origin");
        new this.contentWindow.init({
          pluginParent: {
            callback: volatileDelegate,
            version: MAP_VERSION
          }
        });
        this.setAttribute("sandbox", "allow-forms allow-modals allow-orientation-lock allow-presentation allow-scripts");
      } catch(e) {
        alert("Plugin \"" + this.dataset.id + "\" not initialised.\n\n\nReason given:\n\n\"" + e + "\"");
        document.querySelector('input[value="' + this.dataset.id + '"]').click();
      }
    }

    function loadPlugin(e) {
      var plugin = e.target;
      if(plugin?.tagName?.toLowerCase() === "input") {
        if(plugin.checked) {
          var iframe = document.createElement("iframe");
          iframe.setAttribute("data-id", plugin.value);
          iframe.setAttribute("allow", "accelerometer *; ambient-light-sensor *; attribution-reporting *; autoplay *; battery *; bluetooth *; browsing-topics *; camera *; compute-pressure *; display-capture *; document-domain *; encrypted-media *; execution-while-not-rendered *; execution-while-out-of-viewport *; fullscreen *; gamepad *; geolocation *; gyroscope *; hid *; identity-credentials-get *; idle-detection *; local-fonts *; magnetometer *; microphone *; midi *; otp-credentials *; payment *; picture-in-picture *; publickey-credentials-create *; publickey-credentials-get *; screen-wake-lock *; serial *; speaker-selection *; storage-access *; usb *; web-share *; window-management *; xr-spatial-tracking *");
          iframe.setAttribute("sandbox", "allow-forms allow-modals allow-orientation-lock allow-presentation allow-scripts allow-same-origin");   // should be without allow-same-origin but dynamic switching not supported
          iframe.onload = initPlugin;
          iframe.style = "position:absolute;visibility:hidden;pointer-events:none;top:0;left:0;z-index:-1;width:1px;height:1px";
          iframe.src = "/plugins/" + plugin.value + "/index.html";
          document.querySelector("#contentContainer").insertBefore(iframe, document.querySelector('iframe[name="addoniframe"]'));
        } else {
          try {
            stopMethods[plugin.value]();
          } catch(e) {}
          Array.prototype.forEach.call(document.querySelector('iframe[data-id="theMap"]').contentDocument.querySelectorAll('#pluginform [data-source="' + plugin.value + '"]'), function(toolbarEntry) {
            toolbarEntry.parentElement.removeChild(toolbarEntry);
          });
          var iframe = document.querySelector('iframe[data-id="' + plugin.value + '"]');
          iframe.parentElement.removeChild(iframe);
          var select = document.querySelector("#exclusivePlugins");
          if(select !== null) {
            var option = select.querySelector('option[value="' + plugin.value + '"]');
            if(option !== null) {
              select.remove(Array.prototype.indexOf.call(select, option));
              if(select.childElementCount < 2) {
                select.parentElement.removeChild(select);
              }
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
          if(dir.substring(0, "example-".length) !== "example-" || location.search.substring(1) === "debug") {
            var plugin = document.createElement("div");
            plugin.className = "selectable-plugin";
            var checkbox = document.createElement("input");
            var id = "a" + (Math.random() + "").substring(2);
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
          pluginlist.addEventListener("click", loadPlugin);
        }
      });
    }
  })();
}