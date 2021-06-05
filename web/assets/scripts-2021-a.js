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
    var defaultMapQuality = 17;
    var zoomingMapQuality = 3;
    var resizingMapQuality = 3;

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

    mapElement.onload = function() {
      mapImageLoaded = true;
      forceLock = false;
      while(mapUpdateNotifiables.length) {
        var notifiable = mapUpdateNotifiables.shift();
        if(notifiable) {
          notifiable(mapUpdateCounter);
        }
      }
    };

    /**
     * API: map source update, notifiable = function to take notification
     */
    function updateMapImage(command, quality, force, notifiable) {
      mapUpdateNotifiables.push(notifiable);
      if(mapImageLoaded || force && !forceLock) {
        mapImageLoaded = false;
        forceLock = force;
        mapElement.src = "/mapimage?format=jpg&quality=" + quality + "&width=" + ~~(mapElement.parentElement.clientWidth * realPixelsPerCSSPixel) + "&height=" + ~~(mapElement.parentElement.clientHeight * realPixelsPerCSSPixel) + "&session&" + command + "=" + Math.random();
      }
      return ++mapUpdateCounter;
    }

    /*
     * API: get transformed user setting for map zoom in map command format
     */
    function getZoomDistance() {
      return ~~Math.pow(2, centerDistance.value);
    }


    /*
     * Event handling: resize window
     */
    var imageRequestTimeout = null;
    ocw.sizeMapToContainer = function() {
      ocw.clearTimeout(imageRequestTimeout);
      updateMapImage(refreshTypeWAC.checked ? ("mapcmd=user&distance=" + getZoomDistance() + "&cmd") : ("distance=" + getZoomDistance() + "&reload"), resizingMapQuality);
      imageRequestTimeout = ocw.setTimeout(function() {       // update after the resizing stopped to have an image for the final size "for certain"
        updateMapImage(refreshTypeWAC.checked ? ("mapcmd=user&distance=" + getZoomDistance() + "&cmd") : ("distance=" + getZoomDistance() + "&reload"), defaultMapQuality, true);
      }, 200);
    };

    /*
     * Event handling: mousemove over map
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
      updateMapImage("mapcmd=" + (condition ? "in" : "out") + "&cmd", zoomingMapQuality);
      mapWheelZoomTimeout = ocw.setTimeout(function() {
        updateMapImage("reload", defaultMapQuality, true);
      }, 750);
    };
    mapElement.onwheel = function(e) {
      mapZoomCore(e.deltaY < 0);
    };
    mapElement.ongesturechange = function(e) {
      mapZoomCore(e.scale > 1);
    };

    /*
     * Event handling: header options bar scrolling
     */
    var header = ocd.querySelector("#header");
    function headerIndicators() {
      if(header.firstElementChild.scrollWidth > header.firstElementChild.clientWidth) {
        if(header.firstElementChild.scrollLeft > 0) {
          header.classList.add("indicator-scrollable-toleft");
          if(header.firstElementChild.clientWidth + header.firstElementChild.scrollLeft >= header.firstElementChild.scrollWidth - 1) {
            header.classList.remove("indicator-scrollable-toright");
          } else {
            header.classList.add("indicator-scrollable-toright");
          }
        } else {
          header.classList.remove("indicator-scrollable-toleft");
          header.classList.add("indicator-scrollable-toright");
        }
      } else {
        header.classList.remove("indicator-scrollable-toleft");
        header.classList.remove("indicator-scrollable-toright");
      }
    }
    ocw.addEventListener("resize", headerIndicators);
    header.firstElementChild.addEventListener("scroll", headerIndicators);
    headerIndicators();


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

      function notifiable(id) {     // ignore if some(one/thing) else interrupted own request and thus time data is not accurate
        var durationTilImageHere = performance.now() - timeStartLastRequest;
        refreshIn = ~~(refresher.value * 1000 - durationTilImageHere);
        if(looping) {
          looper();
        }
      }

      function requester() {
        timeStartLastRequest = performance.now();
        updateMapImage(refreshTypeWAC.checked ? ("mapcmd=user&distance=" + getZoomDistance() + "&cmd") : ("distance=" + getZoomDistance() + "&reload"), defaultMapQuality, false, notifiable);
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
      updateMapImage(refreshTypeWAC.checked ? ("mapcmd=user&distance=" + getZoomDistance() + "&cmd") : ("distance=" + getZoomDistance() + "&reload"), defaultMapQuality, true);
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
      updateMapImage("mapcmd=user&distance=" + getZoomDistance() + "&cmd", defaultMapQuality, true);
    };

    // caring and handling state changes and restoration after content switch
    refreshTypeWAC.addEventListener("click", function() {
      storeState("refreshwithaircraft", this.checked);
    });
    refreshTypeWAC.checked = retrieveState("refreshwithaircraft", false);
    centerDistance.addEventListener("change", function() {
      storeState("centerdistance", this.value);
      ocw.checkRefresh();
    });
    retrievedState = retrieveState("centerdistance", null);
    if(retrievedState !== null) {
      centerDistance.value = retrievedState;
      ocd.querySelector('#refreshvalue2').textContent = retrievedState;
    }


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
      handleAutomap(function(){updateMapImage("mapcmd=route&distance=" + getZoomDistance() + "&cmd", defaultMapQuality, true)});
    };

    // override default function to stay within our new ui look
    ocw.submitMapAirportCmd = function() {
      handleAutomap(function(){updateMapImage("mapcmd=airport&airport=" + airportText.value + "&distance=" + getZoomDistance() + "&cmd", defaultMapQuality, true)});
    };


    /*
     * Option Commands Handling: prevent standby
     */
    var standbyPreventionVideo = ocd.querySelector("#preventstandbyVideoContainer").contentDocument.querySelector("video");    // iOS need video with audio track to have it work as standby preventer
    standbyPreventionVideo.addEventListener("play", function() {
      standbyPreventionVideo.classList.add("running");
    });
    standbyPreventionVideo.addEventListener("timeupdate", function() {         // iOS iPad showed a "unprecision" of 2s, thus rewinding after 2s before the end lead to the video ending and rewinding not applied anymore; loop HTML attribute appeared to get ignored
      standbyPreventionVideo.currentTime > 6 ? standbyPreventionVideo.currentTime = 4 : 0;
    });
    standbyPreventionVideo.addEventListener("pause", function() {
      standbyPreventionVideo.classList.remove("running");
    });
    var testTimeout;
    ocw.preventStandby = function(innerorigin) {
      if(innerorigin.checked) {
        standbyPreventionVideo.play();
        storeState("preventingstandby", true);
        testTimeout = ocw.setTimeout(function() {
          if(standbyPreventionVideo.paused) {                                 // being paused despite instructing play can occur on "restoring state" after content switch which is a page reload on iOS because it is denied by iOS as it is like an autoplay on page load
            ocd.querySelector("#preventstandby").click();
          }
        }, 1000);
      } else {
        ocw.clearTimeout(testTimeout);
        standbyPreventionVideo.pause();
        storeState("preventingstandby", false);
      }
    };
    var preventStandby = ocd.querySelector("#preventstandby");
    if(retrieveState("preventingstandby", preventStandby.checked) !== preventStandby.checked) {
      preventStandby.click();
    }

    /*ocw.toggleRetinaMap = function(innerorigin) {
      if(innerorigin.checked) {
        realPixelsPerCSSPixel = devicePixelRatio || 1;
        storeState("retinaon", true);
      } else {
        realPixelsPerCSSPixel = 1;
        storeState("retinaon", false);
      }
      ocw.reloadMap(true);
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


  var toDo = {
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

/*
 * set class of closest parent button of clicked element within children to active
 */
function setActive(e) {
  e = e || window.event;
  var i = e.target;
  while(i.tagName.toLowerCase() !== "button" && i.tagName.toLowerCase() !== "body") {
    i = i.parentElement;
  }
  if(i.tagName.toLowerCase() === "button") {
    e.currentTarget.querySelector(".active").classList.remove("active");
    i.classList.add("active");
  }
}

/*
 * toggles display of toolbars options
 */
function toggleToolbarsOptions(e) {
  document.querySelector("#toggleOptionsToggle").classList.toggle("shown");
  (e || window.event).stopPropagation();
}

/*
 * closes display of toolbars options
 */
function closeToolbarsOptions() {
  document.querySelector("#toggleOptionsToggle").classList.remove("shown");
}

/*
 * sets value of [data-toolbarsplacement] according to origin's value
 */
function setToolbarPosition(e) {
  document.querySelector("[data-toolbarsplacement]").setAttribute("data-toolbarsplacement", e.target.value);
  localStorage.setItem("toolbarsplacement", e.target.value);
}
var gotten = localStorage.getItem("toolbarsplacement");
if(gotten) {
  var destination = document.querySelector("input[type=radio][name=position][value=" + gotten + "]");
  destination.checked = true;
  destination.dispatchEvent(new Event("change", {bubbles: true}));
}

/*
 * sets theme CSS
 */
function switchTheme(origin) {
  var themeCSS = document.querySelector("#themeCSS");
  if(themeCSS !== null) {
    themeCSS.parentElement.removeChild(themeCSS);
  }
  if(origin.value !== "") {
    themeCSS = document.createElement("link");
    themeCSS.href = "/themes/styles-2021-a-look-userdefined-theme-" + origin.value + ".css";
    themeCSS.rel = "stylesheet";
    themeCSS.id = "themeCSS";
    document.head.appendChild(themeCSS);
  }
  localStorage.setItem("themeCSS", origin.value);
}
gotten = localStorage.getItem("themeCSS");
if(gotten) {
  var destination = document.querySelector("select[name=theme]");
  destination.value = gotten;
  destination.dispatchEvent(new Event("change", {bubbles: true}));
}

/*
 * compilation of functions to run when the body receives a click event (possibly having bubbled)
 */
function bodyFunctions() {
  closeToolbarsOptions();
}