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
    var defaultMapQuality = 30;


    var realPixelsPerCSSPixel = devicePixelRatio || 1;


    var refresher = ocd.querySelector("#refreshselect");
    var refreshTypeWAC = ocd.querySelector("#refreshWithAircraft");
    var centerDistance = ocd.querySelector("#centerDistance");

    var airportText = ocd.querySelector("#airporttext");

    var iAParent = ocd.querySelector("#interactionParent");

    var mapElement = ocd.querySelector("#map");


    var mapImageLoaded = true;
    mapElement.onload = function() {
      mapImageLoaded = true;
    };

    /**
     * explicit map source update, performance version requires every parameter!
     */
    function updateMapImage(command, quality) {
      mapImageLoaded = false;
      mapElement.src = "/mapimage?format=jpg&quality=" + quality + "&width=" + ~~(mapElement.parentElement.clientWidth * realPixelsPerCSSPixel) + "&height=" + ~~(mapElement.parentElement.clientHeight * realPixelsPerCSSPixel) + "&session&" + command + "=" + Math.random();
    }

    function getZoomDistance() {
      return ~~Math.pow(2, centerDistance.value);
    }

    var imageRequestTimeout = null;
    // override default function to stay within our new ui look
    ocw.reloadMap = function(force) {
      clearTimeout(imageRequestTimeout);                            // only let the last size from resizing trigger a map update
      imageRequestTimeout = setTimeout(function() {
        mapElement.style.display = "none";                          // to get dimensions of parent unaffected by prior existing larger image enlarging parent when resizing
        updateMapImage(refreshTypeWAC.checked ? "mapcmd=user&distance=" + getZoomDistance() + "&cmd" : "reload", defaultMapQuality);
        mapElement.style.display = "block";
      }, force ? 0 : 50);
    };
    ocw.reloadMap();                                                // equals former body[onload]

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

    ocw.handleInteraction = function(e) {
      var shift = e.currentTarget.getAttribute("data-shift");
      shift !== null ? updateMapImage("mapcmd=" + shift + "&cmd", defaultMapQuality) : 0;         // on touch devices, without initial HTML attribute, shift === null when pinching for zoom in
    };

    var mapWheelZoomTimeout = null;
    var mapZoomCore = function(condition) {
      if(mapImageLoaded) {
        ocw.clearTimeout(mapWheelZoomTimeout);
        updateMapImage("mapcmd=" + (condition ? "in" : "out") + "&cmd", 3);
        mapWheelZoomTimeout = ocw.setTimeout(function() {
          updateMapImage("reload", defaultMapQuality);
        }, 1000);
      }
    };
    mapElement.onwheel = function(e) {
      mapZoomCore(e.deltaY < 0);
    };
    mapElement.ongesturechange = function(e) {
      mapZoomCore(e.scale > 1);
    };

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

    ocw.checkRefresh = function() {
      if(refreshTypeWAC.checked && refresher.value > 0) {
        iAParent.setAttribute("disabled", "");
      } else {
        iAParent.removeAttribute("disabled");
      }
    };

    // only called for auto refresh, page initialisation now reloadMap
    // was copy from script.js refreshMapPage() follows adjusted for document and window, as well as interval function being contained, plus new functionality
    ocw.refreshPage = function() {
      ocw.checkRefresh();

      var refreshvalue = refresher.value;
      if (refreshvalue != ocw.currentInterval) {
        ocw.currentInterval = refreshvalue;

        // Value has changed - stop udpates
        ocw.clearInterval(ocw.timeoutHandle);

        var xhttp = new XMLHttpRequest();
        xhttp.open("GET", "/refresh?session&maprefresh=" + refreshvalue, true);
        xhttp.send();

        if (ocw.currentInterval > 0) {
          // Set interval timer for periodical reload
          ocw.timeoutHandle = ocw.setInterval(function() {
            if(mapImageLoaded) {                                  // on short intervals with high-res images on slower server machines, the previous image might not have been created yet by the server and a new request results in cancellation of the old processing possibly resulting in an image never getting delivered
              updateMapImage(refreshTypeWAC.checked ? "mapcmd=user&distance=" + getZoomDistance() + "&cmd" : "reload", defaultMapQuality);
            }
          }, ocw.currentInterval * 1000);
        }
      }
    };
    centerDistance.addEventListener("change", function() {
      storeState("centerdistance", this.value);
    });
    var retrievedState = retrieveState("centerdistance", null);
    if(retrievedState !== null) {
      centerDistance.value = retrievedState;
      ocd.querySelector('#refreshvalue2').textContent = retrievedState;
    }
    refreshTypeWAC.addEventListener("click", function() {
      storeState("refreshwithaircraft", this.checked);
    });
    refreshTypeWAC.checked = retrieveState("refreshwithaircraft", false);
    refresher.addEventListener("change", function() {
      storeState("refreshinterval", this.value);
    });
    retrievedState = retrieveState("refreshinterval", null);
    if(retrievedState !== null) {
      refresher.value = retrievedState;
      refresher.dispatchEvent(new Event("change"));
      refresher.dispatchEvent(new Event("input"));
    }

    ocw.centerMapOnAircraft = function() {
      updateMapImage("mapcmd=user&distance=" + getZoomDistance() + "&cmd", defaultMapQuality);
    };

    function handleAutomap(withFunction) {
      if(refreshTypeWAC.checked && refresher.value > 0) {
        if(!refreshTypeWAC.classList.contains("enlarge")) {
          setTimeout(function() {
            setTimeout(function() {
              refreshTypeWAC.checked = false;
              withFunction();
              setTimeout(function() {
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
      handleAutomap(function(){updateMapImage("mapcmd=route&cmd", defaultMapQuality)});
    };

    // override default function to stay within our new ui look
    ocw.submitMapAirportCmd = function() {
      handleAutomap(function(){updateMapImage("mapcmd=airport&airport=" + airportText.value + "&distance=" + getZoomDistance() + "&cmd", defaultMapQuality)});
    };

    var standbyPreventionVideo = ocd.querySelector("#preventstandbyVideo");    // iOS need video with audio track to have it work as standby preventer
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

    ocw.toggleRetinaMap = function(innerorigin) {
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
    }

  }


  var toDo = {
    "#airportPage": updateAirportPage,
    "#progressPage": updateProgressPage,
    "#aircraftPage": updateGenericPage,
    "#flightplanPage": updateGenericPage,
    "#mapPage": updateMapPage
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
    themeCSS.href = "/assets/styles-2021-a-look-userdefined-theme-" + origin.value + ".css";
    themeCSS.rel = "stylesheet";
    themeCSS.id = "themeCSS";
    document.head.appendChild(themeCSS);
  }
}

/*
 * compilation of functions to run when the body receives a click event (possibly having bubbled)
 */
function bodyFunctions() {
  closeToolbarsOptions();
}