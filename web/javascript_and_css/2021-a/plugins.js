function findPlugins() {
  new (function() {
    var we = this;
    
    var pluginsConfig = {};
    
    var currentlyStartedPlugin;
    var stopMethods = {};

    var types = {
      type_background:
        function () {
          return {
            request_MapAccess:
              (function(currentlyStartedPlugin) {
                return function(reason) {
                  var iframe = document.querySelector('iframe[data-id="' + currentlyStartedPlugin + '"]');
                  iframe.setAttribute("data-type", "background");
                  if(retrieveState("mapAccessGranted_plugin_" + currentlyStartedPlugin, 0, true) === "true" || confirm("The plugin\n\n\"" + currentlyStartedPlugin + "\"\n\nis requesting access to the map.\n\nReason given:\n\n\"" + reason + "\"\n\nGrant it?\n\n\nNote:\n\nThis is a courtesy question only.\nBrowsers don't allow dynamically switching the\nsecurity settings of the technology employed\nto enable easy development of plugins.\nA plugin can change the map nevertheless if\nits author so instructed it.\n\nThis might change in the future.")) {
                    storeState("mapAccessGranted_plugin_" + currentlyStartedPlugin, "true", true);
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
          var theToolbarAPI = toolbarAPI();
          delete theToolbarAPI.createToolbar;
          var descriptor1 = Object.create(null);
          descriptor1.value = "unobtrusive";
          var descriptor2 = Object.create(null);
          descriptor2.value = currentlyStartedPlugin;
          for(var funcname in theToolbarAPI) {
            Object.defineProperty(theToolbarAPI[funcname], "plugintype", descriptor1);
            Object.defineProperty(theToolbarAPI[funcname], "pluginname", descriptor2);
          }
          return {
            run:
              (function(currentlyStartedPlugin) {
                return function() {
                  var iframe = document.querySelector('iframe[data-id="' + currentlyStartedPlugin + '"]');
                  iframe.className = "overlaying-content";
                  iframe.setAttribute("data-type", "unobtrusive");
                  iframe.setAttribute("allowtransparency", "true");   // appears to be unnecessary
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
                    option.value = "/";
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
                      if(this.value !== "/") {
                        document.querySelector('iframe[data-id="' + this.value + '"]').style.display = "block";
                      }
                      storeState("activeExclusivePlugin", this.value);
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
        },
      type_widget:
        function () {
          var theToolbarAPI = toolbarAPI();
          var descriptor1 = Object.create(null);
          descriptor1.value = "widget";
          var descriptor2 = Object.create(null);
          descriptor2.value = currentlyStartedPlugin;
          for(var funcname in theToolbarAPI) {
            Object.defineProperty(theToolbarAPI[funcname], "plugintype", descriptor1);
            Object.defineProperty(theToolbarAPI[funcname], "pluginname", descriptor2);
          }
          return {
            run:
              (function(currentlyStartedPlugin) {
                return function() {
                  var iframe = document.querySelector('iframe[data-id="' + currentlyStartedPlugin + '"]');
                  iframe.className = "movable-content resizable-content";
                  iframe.setAttribute("data-type", "widget");
                  iframe.setAttribute("allowtransparency", "false");   // appears to be unnecessary
                  iframe.style.background = "#fff";
                  iframe.style.pointerEvents = "all";
                  var contentContainer = document.getElementById("contentContainer");
                  var settings = retrieveState("plugin_" + currentlyStartedPlugin);
                  iframe.style.width = settings?.width ?? "205px";
                  iframe.style.height = settings?.height ?? "225px";
                  iframe.style.top = settings?.top ?? "100px";    // 50px is height of map HTML #header
                  iframe.style.left = settings?.left ?? "50px";
                  iframe.style.bottom = settings?.bottom ?? "auto";
                  iframe.style.right = settings?.right ?? "auto";
                  // 20 = iframe border-top
                  if(iframe.offsetTop + 20 >= contentContainer.clientHeight - iframe.clientHeight) {
                    iframe.style.top = "auto";
                    iframe.style.bottom = "0";
                    storeState("plugin_" + currentlyStartedPlugin, settings);
                  }
                  if(iframe.offsetLeft >= contentContainer.clientWidth - iframe.clientWidth) {
                    iframe.style.left = "auto";
                    iframe.style.right = "0";
                    storeState("plugin_" + currentlyStartedPlugin, settings);
                  }
                  // 50 = map toolbar height (without toolbar scrollbar)
                  if(iframe.clientHeight > contentContainer.clientHeight - 50) {
                    iframe.style.height = (contentContainer.clientHeight - 50) / 2 + "px";
                    storeState("plugin_" + currentlyStartedPlugin, settings);
                  }
                  if(iframe.clientWidth > contentContainer.clientWidth) {
                    iframe.style.width = contentContainer.clientWidth / 2 + "px";
                    storeState("plugin_" + currentlyStartedPlugin, settings);
                  }
                  iframe.style.visibility = "visible";
                  iframe.style.zIndex = "";
                };
              })(currentlyStartedPlugin),
            toolbarAPI:
              theToolbarAPI
          };
        },
    };
    
    
    var activePlugins = [];
    var wasActivePlugins;
    var loadingOnInit;
    var activeExclusivePlugin;
    
    function continueLoadingPlugins(firstCall) {
      while(wasActivePlugins.length) {
        var input = document.querySelector('#selectablePlugins input[value="' + wasActivePlugins.shift() + '"]');
        if(input) {
          input.click();
          return;
        }
      }
      if(firstCall) {
        storeState("activePlugins", activePlugins);
      }
      var exclusivePlugins = document.querySelector("#exclusivePlugins");
      if(exclusivePlugins !== null) {
        if(activePlugins.includes(activeExclusivePlugin)) {
          exclusivePlugins.value = activeExclusivePlugin;
        } else {
          exclusivePlugins.value = "/";
        }
        exclusivePlugins.dispatchEvent(new Event("change"));
      }
      loadingOnInit = false;
    }
    
    var allowedTypes = [];
    function plugin(type, stop_Method) {
      if(allowedTypes.includes(type)) {
        if(typeof stop_Method === "function") {
          stopMethods[currentlyStartedPlugin] = stop_Method;
          return type.call(this);
        }
        throw "stop_Method invalid";
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
        allowedTypes.forEach(typeFunc => {volatileDelegate[typeFunc.id] = typeFunc});
        this.setAttribute("sandbox", "allow-forms allow-modals allow-orientation-lock allow-presentation allow-scripts allow-same-origin");
        new this.contentWindow.init({
          pluginParent: {
            callback: volatileDelegate,
            version: MAP_VERSION
          }
        });
        this.setAttribute("sandbox", "allow-forms allow-modals allow-orientation-lock allow-presentation allow-scripts");
        activePlugins.push(currentlyStartedPlugin);
        storeState("activePlugins", activePlugins);
        loadingOnInit ? continueLoadingPlugins() : !1;
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
          iframe.style = "position:absolute;visibility:hidden;pointer-events:none;top:0;left:0;z-index:-1;width:1px;height:1px";
          switch(pluginsConfig[plugin.value].type) {
          	case "background":
          		iframe.setAttribute("data-type", "background");
          		break;
          	case "unobtrusive":
          		iframe.className = "overlaying-content";
              iframe.setAttribute("data-type", "unobtrusive");
              iframe.setAttribute("allowtransparency", "true");   // appears to be unnecessary
              iframe.style.width = "100%";
              iframe.style.height = "calc(100% - 50px)";    // 50px is height of map HTML #header
              iframe.style.top = "50px";
              iframe.style.visibility = "visible";
              iframe.style.zIndex = "";
              break;
            case "exclusive":
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
                option.value = "/";
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
                  if(this.value !== "/") {
                    document.querySelector('iframe[data-id="' + this.value + '"]').style.display = "block";
                  }
                  storeState("activeExclusivePlugin", this.value);
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
              break;
            case "widget":
            	iframe.className = "movable-content resizable-content";
              iframe.setAttribute("data-type", "widget");
              iframe.setAttribute("allowtransparency", "false");   // appears to be unnecessary
              iframe.style.background = "#fff";
              iframe.style.pointerEvents = "all";
              var contentContainer = document.getElementById("contentContainer");
              var settings = retrieveState("plugin_" + currentlyStartedPlugin);
              iframe.style.width = settings?.width ?? "205px";
              iframe.style.height = settings?.height ?? "225px";
              iframe.style.top = settings?.top ?? "100px";    // 50px is height of map HTML #header
              iframe.style.left = settings?.left ?? "50px";
              iframe.style.bottom = settings?.bottom ?? "auto";
              iframe.style.right = settings?.right ?? "auto";
              // 20 = iframe border-top
              if(iframe.offsetTop + 20 >= contentContainer.clientHeight - iframe.clientHeight) {
                iframe.style.top = "auto";
                iframe.style.bottom = "0";
                storeState("plugin_" + currentlyStartedPlugin, settings);
              }
              if(iframe.offsetLeft >= contentContainer.clientWidth - iframe.clientWidth) {
                iframe.style.left = "auto";
                iframe.style.right = "0";
                storeState("plugin_" + currentlyStartedPlugin, settings);
              }
              // 50 = map toolbar height (without toolbar scrollbar)
              if(iframe.clientHeight > contentContainer.clientHeight - 50) {
                iframe.style.height = (contentContainer.clientHeight - 50) / 2 + "px";
                storeState("plugin_" + currentlyStartedPlugin, settings);
              }
              if(iframe.clientWidth > contentContainer.clientWidth) {
                iframe.style.width = contentContainer.clientWidth / 2 + "px";
                storeState("plugin_" + currentlyStartedPlugin, settings);
              }
              iframe.style.visibility = "visible";
              iframe.style.zIndex = "";
              break;
            default:
            	return;
          }
          iframe.setAttribute("data-id", plugin.value);
          iframe.setAttribute("allow", "accelerometer *; ambient-light-sensor *; attribution-reporting *; autoplay *; battery *; bluetooth *; browsing-topics *; camera *; compute-pressure *; display-capture *; document-domain *; encrypted-media *; execution-while-not-rendered *; execution-while-out-of-viewport *; fullscreen *; gamepad *; geolocation *; gyroscope *; hid *; identity-credentials-get *; idle-detection *; local-fonts *; magnetometer *; microphone *; midi *; otp-credentials *; payment *; picture-in-picture *; publickey-credentials-create *; publickey-credentials-get *; screen-wake-lock *; serial *; speaker-selection *; storage-access *; usb *; web-share *; window-management *; xr-spatial-tracking *");
          if(pluginsConfig[plugin.value].requestMapAccess.length) {
	          if(retrieveState("mapAccessGranted_plugin_" + plugin.value, 0, true) === "true" || confirm("The plugin\n\n\"" + pluginsConfig[plugin.value].title + "\"\n\nis requesting access to the map.\n\nReason given:\n\n\"" + pluginsConfig[plugin.value].requestMapAccess + "\"\n\nGrant it?\n\n\nNote:\n\nThe plugin then will be able to access anything else inside the Little Navmap web frontend.")) {
	            storeState("mapAccessGranted_plugin_" + plugin.value, "true", true);
		          iframe.setAttribute("sandbox", "allow-forms allow-modals allow-orientation-lock allow-presentation allow-scripts allow-same-origin");
		          iframe.onload = function() {
		          	this.contentWindow.setMapWindow(document.querySelector("iframe[name=contentiframe]").contentDocument);
		          };
	          } else {
		          iframe.setAttribute("sandbox", "allow-forms allow-modals allow-orientation-lock allow-presentation allow-scripts");
	          }
	        }
          document.querySelector("#contentContainer").insertBefore(iframe, document.querySelector('iframe[name="addoniframe"]'));
          iframe.src = "/plugins/" + plugin.value + "/index.html";
          activePlugins.push(currentlyStartedPlugin);
	        storeState("activePlugins", activePlugins);
	        loadingOnInit ? continueLoadingPlugins() : !1;
        } else {
          Array.prototype.forEach.call(document.querySelector('iframe[name="contentiframe"]').contentDocument.querySelectorAll('#pluginform [data-source="' + plugin.value + '"]'), function(toolbarEntry) {
            toolbarEntry.parentElement.removeChild(toolbarEntry);
          });
          var iframe = document.querySelector('iframe[data-id="' + plugin.value + '"]');
          iframe.postMessage({pluginParent: {message: {cause: "cleanup"}}});
          setTimeout(() => {
	          iframe.remove();
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
	          var spliceIndex = activePlugins.indexOf(plugin.value);
	          if(spliceIndex > -1) {
	            activePlugins.splice(spliceIndex, 1);
	            storeState("activePlugins", activePlugins);
	          }
          }, 500;
        }
      }
    }

    var pluginbutton = document.querySelector("#buttonPlugins");
    if(pluginbutton === null) {
      fetch("/plugins").then(response => response.text()).then(text => {
        var pluginlist = document.createElement("div");
        var configs = [];
        text.split("/").forEach(function(dir) {
          if(dir.substring(0, "example-".length) !== "example-" || location.search.substring(1) === "debug") {
          	configs.push(fetch("/plugins/" + dir + "/config.json").then(response => response.json()).then(json => {
	            pluginsConfig[dir] = json;
	            var plugin = document.createElement("div");
	            plugin.className = "selectable-plugin plugin-" + json.type;
	            var checkbox = document.createElement("input");
	            var id = "a" + (Math.random() + "").substring(2);
	            checkbox.type = "checkbox";
	            checkbox.id = id;
	            checkbox.value = dir;
	            var label = document.createElement("label");
	            label.setAttribute("for", id);
	            label.innerText = json.title;
	            plugin.appendChild(checkbox);
	            plugin.appendChild(label);
	            pluginlist.appendChild(plugin);
			      }));
          }
        });
        Promise.all(configs).then(() => {
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
	          wasActivePlugins = retrieveState("activePlugins", []);
	          activeExclusivePlugin = retrieveState("activeExclusivePlugin", "/")
	          loadingOnInit = true;
	          continueLoadingPlugins(true);
	        }
        });
      });
    }
  })();
}