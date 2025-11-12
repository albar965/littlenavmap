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
    var exclusivePlugins = null;
    
    var tAPI;
    var toolbarsLayer;
    var mapToolbar;
    
    
    this.pluginToolbarItemDelegate = function (plugin, id, eventtype, germaneValue, values) {
      pluginsConfig[plugin].iframe.contentWindow.postMessage({
        pluginParent: {
          cause: "callback",
          cargo: {
            id: id,
            event: eventtype,
            value: germaneValue,
            data: values
          }
        }
      }, "*");
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
          if(!pluginsConfig[plugin.value].deloading) {
            var iframe = document.createElement("iframe");
            contentContainer.insertBefore(iframe, toolbarsLayer.parentElement);   // make iframe available for dimension queries before all else
            iframe.style = "position:absolute;visibility:hidden;pointer-events:none;top:0;left:0;z-index:-1;width:1px;height:1px";
            iframe.setAttribute("data-type", pluginsConfig[plugin.value].type);
            iframe.setAttribute("data-id", plugin.value);
            iframe.setAttribute("allow", "accelerometer *; autoplay *; bluetooth *; camera *; compute-pressure *; display-capture *; encrypted-media *; fullscreen *; gamepad *; geolocation *; gyroscope *; hid *; identity-credentials-get *; idle-detection *; local-fonts *; magnetometer *; microphone *; midi *; payment *; picture-in-picture *; publickey-credentials-create *; publickey-credentials-get *; screen-wake-lock *; serial *; storage-access *; usb *; web-share *; window-management *; xr-spatial-tracking *");
            pluginsConfig[plugin.value].aborted = false;
            var allowMapAccess = " allow-same-origin";      // this should be empty string. parent access is wished to be prevented. without "allow-same-origin" that is achieved. it also blocks iframes within iframes to communicate with each other directly which is not wished. also localStorage is not accessible then.
            if(pluginsConfig[plugin.value].requestMapAccess.length) {
              if(true || retrieveState("mapAccessGranted_plugin_" + plugin.value, 0, true) === "true" || confirm("The plugin\n\n\"" + pluginsConfig[plugin.value].title + "\"\n\nis requesting access to the map.\n\nReason given:\n\n\"" + pluginsConfig[plugin.value].requestMapAccess + "\"\n\nGrant it?\n\n\nNote:\n\nThe plugin then will be able to access anything else inside the Little Navmap web frontend.\n\nWhen you deny access, a plugin still can find access due to web\nbrowsers ungranular security model which needed to be configured\nfor plugins to be able to do non-trivial tasks.")) {    // no nagging for now
                storeState("mapAccessGranted_plugin_" + plugin.value, "true", true);
                allowMapAccess = " allow-same-origin";
                iframe.addEventListener("load", function() {
                  this.contentWindow.setMapDocument(contentIframe.contentDocument, MAP_VERSION);
                });
              }
            }
            iframe.setAttribute("sandbox", "allow-forms allow-modals allow-orientation-lock allow-presentation allow-scripts" + allowMapAccess);
            iframe.addEventListener("load", function() {
              if(!pluginsConfig[plugin.value].aborted) {
                activePlugins.push(plugin.value);
                storeState("activePlugins", activePlugins);
                pluginsConfig[plugin.value].toolbars.forEach(toolbarConfig => {
                  var toolbar = tAPI.createToolbar(toolbarConfig.title.length ? toolbarsLayer : mapToolbar, toolbarConfig.title, pluginsConfig[plugin.value].type, plugin.value);
                  if(activeExclusivePlugin !== "/" && activeExclusivePlugin !== plugin.value && toolbarConfig.title.length) {
                    toolbar.style.display = "none";
                  }
                  toolbarConfig.items.forEach(item => {
                    tAPI[item.type]?.(item, plugin.value, toolbar);
                  });
                });
                this.contentWindow.postMessage({pluginParent: {cause: "commence"}}, "*");
              }
              loadingOnInit ? continueLoadingPlugins() : !1;
            });
            pluginsConfig[plugin.value].iframe = iframe;
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
                toolbarsLayer.parentElement.classList.add("exclusive-active");
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
                      (document.querySelector('iframe[data-id="' + this.value + '"]')??{style:{display:""}}).style.display = "block";
                      toolbarsLayer.querySelectorAll('.toolbar').forEach(tb => tb.style.display = "none");
                      toolbarsLayer.querySelectorAll('.toolbar[data-source="' + this.value + '"]').forEach(tb => tb.style.display = "");
                      toolbarsLayer.parentElement.classList.add("exclusive-active");
                    } else {
                      toolbarsLayer.querySelectorAll('.toolbar[data-type="exclusive"]').forEach(tb => tb.style.display = "none");
                      toolbarsLayer.querySelectorAll('.toolbar:not([data-type="exclusive"])').forEach(tb => tb.style.display = "");
                      toolbarsLayer.parentElement.classList.remove("exclusive-active");
                    }
                    activeExclusivePlugin = loadingOnInit ? activeExclusivePlugin : this.value;
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
                var settings = retrieveState("plugin_" + plugin.value, {});
                var doUpdate = !settings?.width || !settings?.height || !settings?.top || !settings?.left || !settings?.bottom || !settings?.right || !settings?.offsetX || !settings?.offsetY;
                iframe.style.width = settings?.width ?? "205px";
                iframe.style.height = settings?.height ?? "225px";
                iframe.style.top = settings?.top ?? "";
                iframe.style.left = settings?.left ?? "";
                iframe.style.bottom = settings?.bottom ?? "auto";
                iframe.style.right = settings?.right ?? "auto";
                iframe.style.setProperty("--offsetX", settings?.offsetX ?? "50px");
                iframe.style.setProperty("--offsetY", settings?.offsetY ?? "100px");          // extra 50px is height of map HTML #header toolbar
                if(iframe.clientHeight + parseFloat(getComputedStyle(iframe).borderBottomWidth) > contentContainer.clientHeight - 50) {    // 50 = map menubar height (without toolbar scrollbar)
                  iframe.style.height = (contentContainer.clientHeight - 50) / 2 + "px";
                  doUpdate = true;
                }
                if(iframe.clientWidth + parseFloat(getComputedStyle(iframe).borderRightWidth) > contentContainer.clientWidth) {
                  iframe.style.width = contentContainer.clientWidth / 2 + "px";
                  doUpdate = true;
                }
                if(iframe.offsetTop + parseFloat(getComputedStyle(iframe).borderTopWidth) < 50) {
                  iframe.style.setProperty("--offsetY", 50 - parseFloat(getComputedStyle(iframe).borderTopWidth) + "px");
                  iframe.style.top = "";
                  iframe.style.bottom = "auto";
                  doUpdate = true;
                }
                if(iframe.offsetTop + parseFloat(getComputedStyle(iframe).borderTopWidth) + iframe.clientHeight > contentContainer.clientHeight) {
                  iframe.style.setProperty("--offsetY", "0");
                  iframe.style.top = "auto";
                  iframe.style.bottom = "";
                  doUpdate = true;
                }
                if(iframe.offsetLeft < 0) {
                  iframe.style.setProperty("--offsetX", "0");
                  iframe.style.left = "";
                  iframe.style.right = "auto";
                  doUpdate = true;
                }
                if(iframe.offsetLeft + iframe.clientWidth >= contentContainer.clientWidth) {
                  iframe.style.setProperty("--offsetX", "0");
                  iframe.style.left = "auto";
                  iframe.style.right = "";
                  doUpdate = true;
                }
                if(doUpdate) {
                  settings.height = iframe.style.height;
                  settings.width = iframe.style.width;
                  settings.top = iframe.style.top;
                  settings.bottom = iframe.style.bottom;
                  settings.left = iframe.style.left;
                  settings.right = iframe.style.right;
                  settings.offsetX = settings?.offsetX ?? "50px";
                  settings.offsetY = settings?.offsetY ?? "100px";
                  storeState("plugin_" + plugin.value, settings);
                }
                iframe.style.visibility = "visible";
                iframe.style.zIndex = "";
                break;
              default:
                console?.error("configured type of plugin wrong");
                plugin.checked = false;
                return;
            }
            iframe.src = "/plugins/" + plugin.value + "/index.html";
          } else {
            pluginsConfig[plugin.value].reload = true;
          }
        } else {
          if(!pluginsConfig[plugin.value].deloading) {
            pluginsConfig[plugin.value].deloading = true;
            toolbarsLayer.querySelectorAll('[data-source="' + plugin.value + '"]').forEach(toolbar => toolbar.remove());
            mapToolbar.querySelectorAll('[data-source="' + plugin.value + '"]').forEach(toolbar => toolbar.remove());
            pluginsConfig[plugin.value].iframe.contentWindow.postMessage({pluginParent: {cause: "cleanup"}}, "*");
            setTimeout(() => {
              pluginsConfig[plugin.value].iframe.remove();
              if(exclusivePlugins !== null) {
                var option = exclusivePlugins.querySelector('option[value="' + plugin.value + '"]');
                if(option !== null) {
                  exclusivePlugins.remove(Array.prototype.indexOf.call(exclusivePlugins, option));
                  exclusivePlugins.value = "/";
                  exclusivePlugins.dispatchEvent(new Event("change"));
                  if(exclusivePlugins.childElementCount < 2) {    // only necessary if < 2
                    setTimeout(() => {                            // wait for change event processed
                      if(exclusivePlugins.childElementCount < 2) {    // can have >= 2 in the meantime
                        exclusivePlugins.parentElement.removeChild(exclusivePlugins);
                        exclusivePlugins = null;
                      }
                    }, 500);
                  }
                }
              }
              var spliceIndex = activePlugins.indexOf(plugin.value);
              if(spliceIndex > -1) {
                activePlugins.splice(spliceIndex, 1);
                storeState("activePlugins", activePlugins);
              }
              pluginsConfig[plugin.value].deloading = false;
              if(pluginsConfig[plugin.value].reload) {
                pluginsConfig[plugin.value].reload = false;
                loadPlugin(e);
              }
            }, 500);
          } else {
            pluginsConfig[plugin.value].reload = false;
          }
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
            configs.push(fetch("/plugins/" + dir + "/config.json").then(response => response.json()).then(json => {
              pluginsConfig[dir] = json;
            }, () => console?.error("Plugin folder \"" + dir + "\" present but no configuration file found within or that is incomplete or not well formed.")));
          }
        });
        Promise.all(configs).then(() => {
          Object.keys(pluginsConfig).sort((a, b) => pluginsConfig[a].title.localeCompare(pluginsConfig[b].title)).forEach(key => {
            var plugin = document.createElement("div");
            plugin.className = "selectable-plugin plugin-" + pluginsConfig[key].type;
            var checkbox = document.createElement("input");
            var id = "a" + (Math.random() + "").substring(2);
            checkbox.type = "checkbox";
            checkbox.id = id;
            checkbox.value = key;
            var label = document.createElement("label");
            label.setAttribute("for", id);
            label.innerText = pluginsConfig[key].title;
            pluginsConfig[key].checkbox = checkbox;
            plugin.appendChild(checkbox);
            plugin.appendChild(label);
            pluginlist.appendChild(plugin);
          });
        }).then(() => {
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
              if(event.data.throw) {
                Object.values(pluginsConfig).some(plugin => {
                  if(plugin.iframe?.contentWindow === event.source && !plugin.aborted) {
                    plugin.aborted = true;
                    plugin.checkbox.click();
                    alert("Plugin \"" + plugin.title + "\" not initialised.\n\n\nReason given:\n\n\"" + event.data.throw + "\"");
                    return true;
                  }
                });
              }
            };
            contentContainer = document.getElementById("contentContainer");
            contentIframe = document.querySelector("iframe[name=contentiframe]")
            mapToolbar = contentIframe.contentDocument.getElementById("header").firstElementChild;
            toolbarsLayer = document.getElementById("toolbarsLayer").firstElementChild;
            enableScrollIndicators(toolbarsLayer.parentElement);
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