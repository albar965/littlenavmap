function findPlugins() {
  new (function() {
    var pluginsConfig = {};
    
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
              if(document.querySelector("#exclusivePlugins") === null) {
                var select = document.createElement("select");
                select.id = "exclusivePlugins";
                var option = document.createElement("option");
                option.innerText = "hide all whole-map-covering plugins";
                option.value = "/";
                select.add(option);
                option = document.createElement("option");
                option.innerText = pluginsConfig[plugin.value].title;
                option.value = plugin.value;
                select.add(option);
                document.querySelector("#toolbarOuter").appendChild(select);
                select.onchange = function() {
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
                var select = document.querySelector("#exclusivePlugins");
                select.add(option);
              }
              select.selectedIndex = select.childElementCount - 1;
              select.dispatchEvent(new Event("change"));
              break;
            case "widget":
              iframe.className = "movable-content resizable-content";
              iframe.setAttribute("allowtransparency", "false");    // appears to be unnecessary
              iframe.style.background = "#fff";
              iframe.style.pointerEvents = "all";
              var contentContainer = document.getElementById("contentContainer");
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
              if(iframe.clientHeight > contentContainer.clientHeight - 50) {    // 50 = map toolbar height (without toolbar scrollbar)
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
                this.contentWindow.setMapWindow(document.querySelector("iframe[name=contentiframe]").contentDocument);
              });
            }
          }
          iframe.setAttribute("sandbox", "allow-forms allow-modals allow-orientation-lock allow-presentation allow-scripts" + allowMapAccess);  
          iframe.addEventListener("load", function() {
            if(!pluginsConfig[plugin.value]?.aborted) {
              activePlugins.push(plugin.value);
              storeState("activePlugins", activePlugins);
              
              this.contentWindow.postMessage({pluginParent: {message: {version: MAP_VERSION}}});
              loadingOnInit ? continueLoadingPlugins() : !1;
            }
          });
          pluginsConfig[plugin.value].window = iframe.contentWindow;
          document.querySelector("#contentContainer").insertBefore(iframe, document.querySelector('iframe[name="addoniframe"]'));
          iframe.src = "/plugins/" + plugin.value + "/index.html";
        } else {
          document.querySelectorAll('#toolbarsContainer [data-source="' + plugin.value + '"]').forEach(toolbar => toolbar.remove());
          var iframe = document.querySelector('iframe[data-id="' + plugin.value + '"]');
          iframe.contentWindow.postMessage({pluginParent: {message: {cause: "cleanup"}}});
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
            delete pluginsConfig[plugin.value];
          }, 500);
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
              pluginsConfig[dir].checkbox = checkbox;
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
            onmessage = event => {
              Object.values(pluginsConfig).some(plugin => {
                if(plugin.window === event.origin) {
                  plugin.aborted = true;
                  plugin.checkbox.click();
                  alert("Plugin \"" + plugin.title + "\" not initialised.\n\n\nReason given:\n\n\"" + event.data + "\"");
                  return true;
                }
              });
            };
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