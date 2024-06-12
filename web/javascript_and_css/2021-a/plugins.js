function findPlugins() {
  new (function() {
    var pluginsHost = this;
    
    var pluginsConfig = {};
    
    var activePlugins = [];
    var wasActivePlugins;
    var loadingOnInit;
    var activeExclusivePlugin;
    
    var pluginlist;
    
    var contentContainer;
    var contentIframe;
    var exclusivePlugins;
    
    var tAPI;
    var toolbarsLayer;
    var mapToolbar;
    
    
    this.pluginToolbarItemDelegate = function (plugin, id, germaneValue, values) {
      pluginsConfig[plugin].iframe.contentWindow.postMessage({
        pluginParent: {
          message: {
            cause: "callback",
            cargo: {
              id: id,
              value: germaneValue,
              data: values
            }
          }
        }
      });
    };
    
    function continueLoadingPlugins(firstCall) {
      while(wasActivePlugins.length) {
        var input = pluginlist.querySelector('input[value="' + wasActivePlugins.shift() + '"]');
        if(input) {
          input.click();
          return;
        }
      }
      if(firstCall) {
        storeState("activePlugins", activePlugins);
      }
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

    function loadPlugin(e) {
      var plugin = e.target;
      if(plugin?.tagName?.toLowerCase() === "input") {
        if(plugin.checked) {
          var iframe = document.createElement("iframe");
          iframe.style = "position:absolute;visibility:hidden;pointer-events:none;top:0;left:0;z-index:-1;width:1px;height:1px";
          switch(pluginsConfig[plugin.value].type) {
            case "background":
              break;
            case "unobtrusive":
              iframe.className = "overlaying-content";
              iframe.setAttribute("allowtransparency", "true");     // appears to be unnecessary
              iframe.style.width = "100%";
              iframe.style.height = "calc(100% - 50px)";            // 50px is height of map HTML #header
              iframe.style.top = "50px";
              iframe.style.visibility = "visible";
              iframe.style.zIndex = "";
              break;
            case "exclusive":
              iframe.className = "displayable-content";
              iframe.setAttribute("allowtransparency", "false");    // appears to have no effect
              iframe.style.background = "#fff";
              iframe.style.pointerEvents = "all";
              iframe.style.width = "100%";
              iframe.style.height = "100%";
              iframe.style.visibility = "visible";
              iframe.style.zIndex = "";
              if(!exclusivePlugins) {
                exclusivePlugins = document.createElement("select");
                exclusivePlugins.id = "exclusivePlugins";
                var option = document.createElement("option");
                option.innerText = "hide all whole-map-covering plugins";
                option.value = "/";
                exclusivePlugins.add(option);
                option = document.createElement("option");
                option.innerText = pluginsConfig[plugin.value].title;
                option.value = plugin.value;
                exclusivePlugins.add(option);
                document.getElementById("menubarOuter").appendChild(exclusivePlugins);
                exclusivePlugins.onchange = function() {
                  document.querySelectorAll(".displayable-content").forEach(dc => dc.style.display = "none");
                  if(this.value !== "/") {
                    document.querySelector('iframe[data-id="' + this.value + '"]').style.display = "block";
                  }
                  storeState("activeExclusivePlugin", this.value);
                };
              } else {
                var option = document.createElement("option");
                option.innerText = pluginsConfig[plugin.value].title;
                option.value = plugin.value;
                exclusivePlugins.add(option);
              }
              exclusivePlugins.selectedIndex = exclusivePlugins.childElementCount - 1;
              exclusivePlugins.dispatchEvent(new Event("change"));
              break;
            case "widget":
              iframe.className = "movable-content resizable-content";
              iframe.setAttribute("allowtransparency", "false");    // appears to be unnecessary
              iframe.style.background = "#fff";
              iframe.style.pointerEvents = "all";
              var settings = retrieveState("plugin_" + plugin.value);
              iframe.style.width = settings?.width ?? "205px";
              iframe.style.height = settings?.height ?? "225px";
              iframe.style.top = settings?.top ?? "100px";          // 50px is height of map HTML #header
              iframe.style.left = settings?.left ?? "50px";
              iframe.style.bottom = settings?.bottom ?? "auto";
              iframe.style.right = settings?.right ?? "auto";
              // 20 = iframe border-top
              if(iframe.offsetTop + 20 >= contentContainer.clientHeight - iframe.clientHeight) {
                iframe.style.top = "auto";
                iframe.style.bottom = "0";
                storeState("plugin_" + plugin.value, settings);
              }
              if(iframe.offsetLeft >= contentContainer.clientWidth - iframe.clientWidth) {
                iframe.style.left = "auto";
                iframe.style.right = "0";
                storeState("plugin_" + plugin.value, settings);
              }
              if(iframe.clientHeight > contentContainer.clientHeight - 50) {    // 50 = map menubar height (without toolbar scrollbar)
                iframe.style.height = (contentContainer.clientHeight - 50) / 2 + "px";
                storeState("plugin_" + plugin.value, settings);
              }
              if(iframe.clientWidth > contentContainer.clientWidth) {
                iframe.style.width = contentContainer.clientWidth / 2 + "px";
                storeState("plugin_" + plugin.value, settings);
              }
              iframe.style.visibility = "visible";
              iframe.style.zIndex = "";
              break;
            default:
              return;
          }
          iframe.setAttribute("data-type", pluginsConfig[plugin.value].type);
          iframe.setAttribute("data-id", plugin.value);
          iframe.setAttribute("allow", "accelerometer *; ambient-light-sensor *; attribution-reporting *; autoplay *; battery *; bluetooth *; browsing-topics *; camera *; compute-pressure *; display-capture *; document-domain *; encrypted-media *; execution-while-not-rendered *; execution-while-out-of-viewport *; fullscreen *; gamepad *; geolocation *; gyroscope *; hid *; identity-credentials-get *; idle-detection *; local-fonts *; magnetometer *; microphone *; midi *; otp-credentials *; payment *; picture-in-picture *; publickey-credentials-create *; publickey-credentials-get *; screen-wake-lock *; serial *; speaker-selection *; storage-access *; usb *; web-share *; window-management *; xr-spatial-tracking *");
          var allowMapAccess = "";
          if(pluginsConfig[plugin.value].requestMapAccess.length) {
            if(retrieveState("mapAccessGranted_plugin_" + plugin.value, 0, true) === "true" || confirm("The plugin\n\n\"" + pluginsConfig[plugin.value].title + "\"\n\nis requesting access to the map.\n\nReason given:\n\n\"" + pluginsConfig[plugin.value].requestMapAccess + "\"\n\nGrant it?\n\n\nNote:\n\nThe plugin then will be able to access anything else inside the Little Navmap web frontend.")) {
              storeState("mapAccessGranted_plugin_" + plugin.value, "true", true);
              allowMapAccess = " allow-same-origin";
              iframe.addEventListener("load", function() {
                this.contentWindow.setMapWindow(contentIframe.contentDocument);
              });
            }
          }
          iframe.setAttribute("sandbox", "allow-forms allow-modals allow-orientation-lock allow-presentation allow-scripts" + allowMapAccess);  
          iframe.addEventListener("load", function() {
            if(!pluginsConfig[plugin.value]?.aborted) {
              activePlugins.push(plugin.value);
              storeState("activePlugins", activePlugins);
              pluginsConfig[plugin.value].foreach(toolbarConfig => {
                var toolbar = tAPI.createToolbar(toolbarConfig.title.length ? toolbarsLayer : mapToolbar, toolbarConfig.title, toolbarConfig.type, plugin.value);
                toolbarConfig.items.forEach(item => {
                  tAPI[item.type]?.(item, plugin.value, toolbar);
                });
              });
              this.contentWindow.postMessage({pluginParent: {message: {version: MAP_VERSION}}});
            }
            loadingOnInit ? continueLoadingPlugins() : !1;
          });
          pluginsConfig[plugin.value].iframe = iframe;
          contentContainer.insertBefore(iframe, toolbarsLayer);
          iframe.src = "/plugins/" + plugin.value + "/index.html";
        } else {
          toolbarsLayer.querySelectorAll('[data-source="' + plugin.value + '"]').forEach(toolbar => toolbar.remove());
          pluginsConfig[plugin.value].iframe.contentWindow.postMessage({pluginParent: {message: {cause: "cleanup"}}});
          setTimeout(() => {
            pluginsConfig[plugin.value].iframe.remove();
            if(exclusivePlugins !== null) {
              var option = exclusivePlugins.querySelector('option[value="' + plugin.value + '"]');
              if(option !== null) {
                exclusivePlugins.remove(Array.prototype.indexOf.call(exclusivePlugins, option));
                if(exclusivePlugins.childElementCount < 2) {
                  exclusivePlugins.parentElement.removeChild(exclusivePlugins);
                }
              }
            }
            var spliceIndex = activePlugins.indexOf(plugin.value);
            if(spliceIndex > -1) {
              activePlugins.splice(spliceIndex, 1);
              storeState("activePlugins", activePlugins);
            }
            delete pluginsConfig[plugin.value];
          }, 500);
        }
      }
    }

    var pluginbutton = document.getElementById("buttonPlugins");
    if(pluginbutton === null) {
      fetch("/plugins").then(response => response.text()).then(text => {
        pluginlist = document.createElement("div");
        var configs = [];
        text.split("/").forEach(dir => {
          if(dir.substring(0, "example-".length) !== "example-" || location.search.substring(1) === "debug") {
            configs.push(fetch("/plugins/" + dir + "/config.json").then(response => {
            	return response.json();
            }).then(json => {
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
              pluginsConfig[dir].checkbox = checkbox;
              plugin.appendChild(checkbox);
              plugin.appendChild(label);
              pluginlist.appendChild(plugin);
            }, () => {
            	console?.error("Plugin folder \"" + dir + "\" present but no configuration file found within or that is incomplete or not well formed.");
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
            document.getElementById("menubarOuter").appendChild(menuentry);
            pluginlist.addEventListener("click", loadPlugin);
            onmessage = event => {
              Object.values(pluginsConfig).some(plugin => {
                if(plugin.iframe.contentWindow === event.origin) {
                  plugin.aborted = true;
                  plugin.checkbox.click();
                  alert("Plugin \"" + plugin.title + "\" not initialised.\n\n\nReason given:\n\n\"" + event.data + "\"");
                  return true;
                }
              });
            };
            contentContainer = document.getElementById("contentContainer");
            contentIframe = document.querySelector("iframe[name=contentiframe]")
            toolbarsLayer = document.getElementById("toolbarsLayer");
            mapToolbar = contentIframe.contentDocument.getElementById("header").firstElementChild;
            tAPI = toolbarAPI(pluginsHost);
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