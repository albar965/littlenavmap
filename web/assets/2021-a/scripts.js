sessionStorage.setItem("activeUI", "2021-a");

/*
 * adds look and functionality updates for the original HTML to the document loaded in the iframe origin
 */
function injectUpdates(origin) {
  var ocw = origin.contentWindow;
  var ocd = origin.contentDocument;

  if(!ocd) {   // likely no CORS
    return;
  }

  function updateAirportPage(page) {
    page.querySelector("#airportform").style.fontSize = "0";
    page.querySelector("#icaoSelector").style.fontSize = "14px";
  }

  function updateProgressPage(page) {
    page.querySelector("#defaultCommands").style.display = "none";

    var refresher = ocd.querySelector("#refreshselect");
    refresher.addEventListener("change", function() {
      storeState("refreshprogressinterval", this.selectedIndex);
    });
    var refreshInterval = retrieveState("refreshprogressinterval", null);
    if(refreshInterval !== null) {
      refresher.selectedIndex = refreshInterval;
      refresher.dispatchEvent(new Event("change"));
    }
  }

  function updateGenericPage(page) {
    page.querySelector("#refreshform").style.display = "none";
  }

  function updateMapPage(page) {
    /*
     * settings (developer modifiable)
     */
    var defaultMapQuality = 70;                                                       // no observed difference across 30 to 100
    var fastRefreshMapQuality = 50;                                                   // below 17 JPEG artifacts on vertical lines are significantly visible: doubled lines. 18 to 21 (something around these) are significantly more artifacted too including an off blue ocean color. space saved compared to default: around 150 KB or 40 % at 1080p; works in conjunction with fastRefreshMapThreshold
    var zoomingMapQuality = 3;
    var resizingMapQuality = 3;

    var fastRefreshMapThreshold = 12;                                                 // time in seconds below which an auto-refreshing map's refresh interval is considered "fast"; works in conjunction with fastRefreshMapQuality
    var mapUpdateTimeoutWaitDuration = 10000;                                         // time in milliseconds to wait for the updated map image to have arrived, after this time a "map image updated" (= notifications are run and image update locks are released) is forced, this is server response time + download time! The main purpose is unlocking the locks on an image update when no image received for a reason like a server outage which if not handled would prevent new images to get requested which the server could handle again if the outage was only temporary.

    /*
     * settings (code modifiable)
     */
    var realPixelsPerCSSPixel = /*devicePixelRatio ||*/ 1;


    /*
     * elements
     */
    var refresher = ocd.querySelector("#refreshDelay");
    var refreshToggle = ocd.querySelector("#refreshMapToggle");
    var refreshTypeWAC = ocd.querySelector("#refreshWithAircraft");
    var centerDistance = ocd.querySelector("#centerDistance");

    var airportText = ocd.querySelector("#airporttext");

    var iAParent = ocd.querySelector("#interactionParent");
    var mapElement = ocd.querySelector("#map");


    /*
     * map image handling
     */
    var mapUpdateNotifiables = [];
    var mapUpdateCounter = 0;
    var mapImageLoaded = true;
    var forceLock = false;
    var mapUpdateTimeout = null;
    var valueMapUpdateCounterMustBeAbove = mapUpdateCounter;            // be able to not run code which should only run once per returned image and which was run for the "forced" update on timeout wait duration end if the image does still return after the timeout wait duration, see mapUpdateTimeoutWaitDuration

    mapElement.onload = function() {
      clearTimeout(mapUpdateTimeout);
      mapImageLoaded = true;
      forceLock = false;
      while(mapUpdateNotifiables.length) {                              // this loop empties the notifiables thus they cannot run a second time and thus do not need to be inside below "if(mapUpdateCounter > valueMapUpdateCounterMustBeAbove)"
        var notifiable = mapUpdateNotifiables.shift();
        if(typeof notifiable === "function") {
          notifiable(mapUpdateCounter);
        }
      }
      if(mapUpdateCounter > valueMapUpdateCounterMustBeAbove) {}
    };

    /**
     * API: map source update
     * locks and does not transfer new commands while waiting for map image to have been received.
     * lock can be overridden by force = true which locks again. That lock cannot be overridden unless nolock = true.
     * Note: upon assigning a new image source, Chrome based browsers "cancel" the old image request. As http (1.x) apparently has no means of doing so, this is likely done on a lower network connection level. The LNM web server, by Chrome developer tools, does not send an image to cancelled requests anymore. The logic here has been built around minimising cancellation of requests and thus minimising time in which no updated image would be shown.
     * notifiable = function to take notification of this function call returned a new image, will be passed id of current update "cycle"
     * returns integer id of current update "cycle", will have the negative value of the current update "cycle" if this function call did not request a new image due to locking
     */
    // has a copy updateMapImage_cssPixels (see there) which needs parallel treatment
    var updateMapImage_realPixels = function(command, quality, force, notifiable, nolock) {
      mapUpdateNotifiables.push(notifiable);
      if(mapImageLoaded || force && !forceLock) {
        mapImageLoaded = false;
        forceLock = force && !nolock;
        mapElement.src = "/mapimage?format=jpg&quality=" + quality + "&width=" + ~~(mapElement.parentElement.clientWidth * realPixelsPerCSSPixel) + "&height=" + ~~(mapElement.parentElement.clientHeight * realPixelsPerCSSPixel) + "&session&" + command + "=" + Math.random();
        clearTimeout(mapUpdateTimeout);
        mapUpdateTimeout = setTimeout((function(mapUpdateCounter) {
          return function() {
            mapElement.onload();
            valueMapUpdateCounterMustBeAbove = mapUpdateCounter + 1;
          };
        })(mapUpdateCounter), mapUpdateTimeoutWaitDuration);
        return ++mapUpdateCounter;
      }
      return -mapUpdateCounter;
    };
    // same as updateMapImage_realPixels except width and height are as delivered by JS (= in CSS pixels (which are real when devicePixelRatio == 1))
    var updateMapImage_cssPixels = function(command, quality, force, notifiable, nolock) {
      mapUpdateNotifiables.push(notifiable);
      if(mapImageLoaded || force && !forceLock) {
        mapImageLoaded = false;
        forceLock = force && !nolock;
        mapElement.src = "/mapimage?format=jpg&quality=" + quality + "&width=" + ~~mapElement.parentElement.clientWidth + "&height=" + ~~mapElement.parentElement.clientHeight + "&session&" + command + "=" + Math.random();
        clearTimeout(mapUpdateTimeout);
        mapUpdateTimeout = setTimeout((function(mapUpdateCounter) {
          return function() {
            mapElement.onload();
            valueMapUpdateCounterMustBeAbove = mapUpdateCounter + 1;
          };
        })(mapUpdateCounter), mapUpdateTimeoutWaitDuration);
        return ++mapUpdateCounter;
      }
      return -mapUpdateCounter;
    };

    function setMapImageUpdateFunction() {
      window.updateMapImage = realPixelsPerCSSPixel === 1 ? updateMapImage_cssPixels : updateMapImage_realPixels;
    }
    setMapImageUpdateFunction();

    /*
     * API: returns user setting for map zoom transformed in map command format, used to be used for explicitly setting zoom level
     */
    function getZoomDistance() {
      return ~~Math.pow(2, centerDistance.value);
    }

    /*
     * API: returns the map command equalling a reload taking into account relevant settings
     */
    function mapCommand() {
      return refreshTypeWAC.checked ? "mapcmd=user&cmd" : "reload";
    }


    /*
     * Event handling: resize window
     */
    var imageRequestTimeout = null;
    ocw.sizeMapToContainer = function() {
      ocw.clearTimeout(imageRequestTimeout);
      updateMapImage(mapCommand(), resizingMapQuality);
      imageRequestTimeout = ocw.setTimeout(function() {       // update after the resizing stopped to have an image for the final quality "for certain"
        updateMapImage(mapCommand(), defaultMapQuality, true, 0, true);     // do not lock for event handling like a new zoom request to be able to take priority over waiting for the potentially longer loading higher-quality final quality
      }, 200);
    };

    /*
     * Event handling: mousemove over map (parent)
     */
    mapElement.parentElement.onmousemove = function(e) {
      var s = e.currentTarget.clientHeight / e.currentTarget.clientWidth;
      var x = e.offsetX - e.currentTarget.clientWidth / 2;
      var y = -(e.offsetY - e.currentTarget.clientHeight / 2);
      if(y > s * x) {
        if(y > -s * x) {
          e.currentTarget.setAttribute("data-shift", "up");         // north
        } else {
          e.currentTarget.setAttribute("data-shift", "left");       // west
        }
      } else {
        if(y > -s * x) {
          e.currentTarget.setAttribute("data-shift", "right");      // east
        } else {
          e.currentTarget.setAttribute("data-shift", "down");       // south
        }
      }
    };

    /*
     * Event handling: click map
     */
    ocw.handleInteraction = function(e) {
      var shift = e.currentTarget.getAttribute("data-shift");
      shift !== null ? updateMapImage("mapcmd=" + shift + "&cmd", defaultMapQuality, true) : 0;         // on touch devices, without initial HTML attribute, shift === null when pinching for zoom in
    };

    /*
     * Event handling: mousewheel / finger pinch map
     */
    var mapWheelZoomTimeout = null;
    var mapZoomCore = function(condition) {
      ocw.clearTimeout(mapWheelZoomTimeout);
      updateMapImage("mapcmd=" + (condition ? "in" : "out") + "&cmd", zoomingMapQuality, true);
      mapWheelZoomTimeout = ocw.setTimeout(function() {
        updateMapImage("reload", defaultMapQuality, true, 0, true);     // do not lock for event handling like a new zoom request to be able to take priority over waiting for the potentially longer loading higher-quality final quality
      }, 750);
    };
    mapElement.onwheel = function(e) {
      mapZoomCore(e.deltaY < 0);
    };
    var pointers = {};
    mapElement.onpointerdown = function(e) {
      pointers[e.pointerId] = [e, e];
    };
    mapElement.onpointermove = function(e) {
      if(pointers.hasOwnProperty(e.pointerId)) {
        pointers[e.pointerId][1] = e;
        var keys = Object.keys(pointers);
        if(keys.length > 1) {
          var key1 = keys[0];
          var key2 = keys[1];
          var distStart = Math.hypot(pointers[key1][0].clientX - pointers[key2][0].clientX, pointers[key1][0].clientY - pointers[key2][0].clientY);
          var distNow = Math.hypot(pointers[key1][1].clientX - pointers[key2][1].clientX, pointers[key1][1].clientY - pointers[key2][1].clientY);
          if(distNow < .85 * distStart) {
            mapZoomCore(false);
            pointers[key1][0] = pointers[key1][1];
            pointers[key2][0] = pointers[key2][1];
          } else if(distNow > 1.15 * distStart) {
            mapZoomCore(true);
            pointers[key1][0] = pointers[key1][1];
            pointers[key2][0] = pointers[key2][1];
          }
        }
      }
    };
    function pointerup(e) {
      delete pointers[e.pointerId];
    }
    mapElement.onpointerup = pointerup;
    mapElement.onpointercancel = pointerup;
    mapElement.onpointerout = pointerup;
    mapElement.onpointerleave = pointerup;


    /*
     * scrollable indicators of header options bar which is a toolbar
     */
    enableToolbarIndicators(ocd.querySelector("#header"), ocw);


    /*
     * Option Commands Handling: refresh
     */
    // prevent users from "mapipulation" by adjusting for all features whose setting's values create an "auto" map
    function adjustForAutoMapSettings() {
      if(refreshTypeWAC.checked && refreshToggle.checked) {
        iAParent.setAttribute("disabled", "");
      } else {
        iAParent.removeAttribute("disabled");
      }
    }

    var mapRefresher = new (function() {
      var refreshMapTimeout = null;
      var looping = false;
      var timeStartLastRequest = 0;
      var refreshIn = 0;

      function notifiable(id) {     // ignore id thus don't determine if some(one/thing) else interrupted own request and thus using time data does not produce expected result (resulting inprecision is not sufficiently detrimental to warrant effort)
        var durationTilImageHere = performance.now() - timeStartLastRequest;
        refreshIn = ~~(refresher.value * 1000 - durationTilImageHere);
        if(looping) {
          looper();
        }
      }

      function requester() {
        timeStartLastRequest = performance.now();
        updateMapImage(mapCommand(), refresher.value < fastRefreshMapThreshold ? fastRefreshMapQuality : defaultMapQuality, false, notifiable);     // not storing return value because using it to determine if some(one/thing) else interrupted own request is not done
      }

      function looper() {
        refreshMapTimeout = ocw.setTimeout(requester, Math.max(0, refreshIn));     // be >= 0
      }

      // does stop before
      this.start = function() {
        this.stop();
        looping = true;
        refreshIn = 0;
        looper();
      };

      this.stop = function() {
        ocw.clearTimeout(refreshMapTimeout);
        looping = false;
      };
    })();

    function adjustDelay() {
        var xhttp = new XMLHttpRequest();
        xhttp.open("GET", "/refresh?session&maprefresh=" + (refreshToggle.checked ? refresher.value : 0), true);
        xhttp.send();
    };

    ocw.checkRefresh = function() {
        adjustDelay();
        refreshToggle.checked ? mapRefresher.start() : mapRefresher.stop();
        refresher.disabled = !refreshToggle.checked;
        adjustForAutoMapSettings();
        return refreshToggle.checked;
    };

    ocw.delayRefresh = function() {
      ocw.checkRefresh();
    };

    ocw.refreshMap = function() {
      updateMapImage(mapCommand(), defaultMapQuality, true);
    };

    // caring and handling state changes and restoration after content switch (after outer ui other button presses)
    refreshToggle.addEventListener("click", function() {
      storeState("autorefresh", this.checked);
    });
    refreshToggle.checked = retrieveState("autorefresh", false);
    refresher.addEventListener("input", function() {
      storeState("refreshdelay", this.value);
    });
    var retrievedState = retrieveState("refreshdelay", null);
    if(retrievedState !== null) {
      refresher.value = retrievedState;
    }

    /*
     * Option Commands Handling: aircraft
     */
    ocw.toggleCenterAircraft = function () {
      ocw.checkRefresh();
    };

    ocw.centerMapOnAircraft = function() {
      updateMapImage("mapcmd=user&cmd", defaultMapQuality, true);
    };

    // caring and handling state changes and restoration after content switch
    refreshTypeWAC.addEventListener("click", function() {
      storeState("refreshwithaircraft", this.checked);
    });
    refreshTypeWAC.checked = retrieveState("refreshwithaircraft", false);


    /*
     * Option Commands Handling: waypoints and airports
     */
    function handleAutomap(withFunction) {
      if(refreshTypeWAC.checked && refreshToggle.checked) {
        if(!refreshTypeWAC.classList.contains("enlarge")) {
          ocw.setTimeout(function() {
            ocw.setTimeout(function() {
              refreshTypeWAC.checked = false;
              withFunction();
              ocw.setTimeout(function() {
                refreshTypeWAC.classList.remove("enlarge");
              }, 250);
            }, 250);
          }, 1500);
          refreshTypeWAC.classList.add("enlarge");
        }
        return;
      }
      withFunction();
    }

    // override default function to stay within our new ui look
    ocw.submitMapRouteCmd = function() {
      handleAutomap(function(){updateMapImage("mapcmd=route&cmd", defaultMapQuality, true)});
    };

    // override default function to stay within our new ui look
    ocw.submitMapAirportCmd = function() {
      handleAutomap(function(){updateMapImage("mapcmd=airport&airport=" + airportText.value + "&cmd", defaultMapQuality, true)});
    };


    /*
     * Option Commands Handling: prevent standby
     */
    var standbyPreventionVideoContainer = ocd.querySelector("#preventstandbyVideoContainer");
    var standbyPreventionVideo = null;
    function handleVideo() {
      standbyPreventionVideo = standbyPreventionVideoContainer.contentDocument.querySelector("video");    // iOS needs video with audio track to have it work as standby preventer
      standbyPreventionVideo.addEventListener("play", function() {
        standbyPreventionVideo.classList.add("running");
      });
      standbyPreventionVideo.addEventListener("timeupdate", function() {                                  // iOS iPad showed a "unprecision" of 2s, thus rewinding after 2s before the end lead to the video ending and rewinding not applied anymore; loop HTML attribute appeared to get ignored
        standbyPreventionVideo.currentTime > 6 ? standbyPreventionVideo.currentTime = 4 : 0;
      });
      standbyPreventionVideo.addEventListener("pause", function() {
        standbyPreventionVideo.classList.remove("running");
      });
    }
    var preventStandbyToggle = ocd.querySelector("#preventstandby");
    var testTimeout;
    function enableStandbyPrevention() {
      standbyPreventionVideo.play();
      storeState("preventingstandby", true);
      testTimeout = ocw.setTimeout(function() {
        if(standbyPreventionVideo.paused) {                                                               // being paused despite instructing play can occur on "restoring state" after content switch because playing is denied by iOS on page load, content switch is a page load and playing would be autoplay
          preventStandbyToggle.click();
        }
      }, 1000);
    }
    function disableStandbyPrevention() {
      ocw.clearTimeout(testTimeout);
      standbyPreventionVideo.pause();
      storeState("preventingstandby", false);
    }
    ocw.preventStandby = function(innerorigin) {
      if(innerorigin.checked) {
        if(standbyPreventionVideo !== null) {
          enableStandbyPrevention();
        } else {
          standbyPreventionVideoContainer.onload = function() {
            handleVideo();
            enableStandbyPrevention();
          };
          standbyPreventionVideoContainer.src = "preventstandbyvideo.html";
        }
      } else {
        if(standbyPreventionVideo !== null) {
          disableStandbyPrevention();
        } else {
          standbyPreventionVideoContainer.onload = function() {
            handleVideo();
            disableStandbyPrevention();
          };
          standbyPreventionVideoContainer.src = "preventstandbyvideo.html";
        }
      }
    };
    if(retrieveState("preventingstandby", preventStandbyToggle.checked) !== preventStandbyToggle.checked) {
      preventStandbyToggle.click();
    }

    /*ocw.toggleRetinaMap = function(innerorigin) {
      if(innerorigin.checked) {
        realPixelsPerCSSPixel = devicePixelRatio || 1;
        storeState("retinaon", true);
      } else {
        realPixelsPerCSSPixel = 1;
        storeState("retinaon", false);
      }
      setMapImageUpdateFunction();
      ocw.refreshMap();
    }
    var retinaToggle = ocd.querySelector("#retinaToggle");
    if(retrieveState("retinaon", retinaToggle.checked) !== retinaToggle.checked) {
      retinaToggle.click();
    }*/


    /*
     * Initial Page initialisations: map
     */
    var transitionElement = mapElement.parentElement;
    function initiallyShowMap() {
      function show() {
        mapElement.removeEventListener("load", show);
        transitionElement.classList.remove("initially");
        transitionElement.classList.remove("transition");
        transitionElement.classList.remove("toshow");
      }
      mapElement.addEventListener("load", show);
      ocw.checkRefresh() || ocw.refreshMap();                                       // takes over for former body[onload]
      storeState("mapshown", true);
    }
    if(!retrieveState("mapshown", false)) {
      transitionElement.classList.add("toshow");
      transitionElement.classList.remove("initially");
      setTimeout(function() {
        transitionElement.classList.add("transition");
        setTimeout(function() {
          transitionElement.classList.remove("toshow");
          setTimeout(initiallyShowMap, 3000);
        }, 0);
      }, 0);
    } else {
      initiallyShowMap();
    }

  }


  var toDo = {                                                                      // ordered according to assumed likelihood of access: first assumedly accessed page is checked first: code is done. currently equals the outer ui menu button order.
    "#mapPage": updateMapPage,
    "#flightplanPage": updateGenericPage,
    "#progressPage": updateProgressPage,
    "#aircraftPage": updateGenericPage,
    "#airportPage": updateAirportPage
  };

  for(var i in toDo) {
    var find = ocd.querySelector(i);
    if(find) {
      toDo[i](find);
      break;
    }
  }


  function storeState(key, state) {
    sessionStorage.setItem(key, typeof state === "boolean" ? state ? "1" : "" : "" + state);
    sessionStorage.setItem(key + "__type", typeof state === "boolean" ? "1" : typeof state === "number" ? "2" : "");
  }

  function retrieveState(key, defaultValue) {
    var type = sessionStorage.getItem(key + "__type");
    var value = sessionStorage.getItem(key);
    return value === null ? defaultValue : type === "1" ? value === "1" : type === "2" ? parseFloat(value) : value;
  }

}