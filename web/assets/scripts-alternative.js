/*
 * adds look and functionality updates for the original HTML to the document loaded in the iframe origin
 */
function injectUpdates(origin) {

  if(!origin.contentDocument) {   // likely no CORS
    return;
  }

  function updateAirportPage(page) {
    page.querySelector("#airportform").style.fontSize = "0";
    page.querySelector("#icaoSelector").style.fontSize = "14px";
  }

  function updateProgressPage(page) {
    page.querySelector("#defaultCommands").style.display = "none";

    var refresher = origin.contentDocument.querySelector("#refreshselect");
    refresher.addEventListener("change", function() {
      storeState("refreshprogressinterval", this.selectedIndex);
    });
    var refreshInterval = retrieveState("refreshprogressinterval");
    if(refreshInterval !== null) {
      refresher.selectedIndex = refreshInterval;
      refresher.dispatchEvent(new Event("change"));
    }
  }

  function updateGeneric(page) {
    page.querySelector("#refreshform").style.display = "none";
  }

  function updateMapPage(page) {

    var imageRequestTimeout = null;
    origin.contentWindow.reloadMap = function(cmd) {
      clearTimeout(imageRequestTimeout);
      imageRequestTimeout = setTimeout(function() {
        var map = origin.contentDocument.querySelector("#map");
        if(map) {
          typeof cmd === "undefined" ? map.style.display = "none" : 0;   // to get dimensions of parent unaffected by prior existing larger image enlarging parent when resizing; inline resize event handler case
          map.src =
            "/mapimage?format=jpg&quality=30&width=" + map.parentElement.clientWidth + "&height=" + map.parentElement.clientHeight +
            "&session" + (typeof cmd !== "undefined" && cmd !== "refreshonly" ? "&mapcmd=" + cmd + "&cmd=" : "&reload=") + Math.random();
          typeof cmd === "undefined" ? map.style.display = "block" : 0;
        }
      }, 50);    // wait 50ms before requesting image to avoid server overload
    };
    origin.contentWindow.reloadMap();

    origin.contentWindow.setMapInteraction = function(event) {
      origin.contentDocument.querySelector("#mapcontainer").setAttribute("data-interaction", event.target.value);
        storeState("mapinteraction", event.target.id);
    };
    var mapInteraction = retrieveState("mapinteraction");
    if(mapInteraction !== null) {
      origin.contentDocument.querySelector("#" + mapInteraction).click();
    }

    origin.contentWindow.evalCursor = function(e) {
      if(e.currentTarget.getAttribute("data-interaction") === "shift") {
        var s = e.currentTarget.clientHeight / e.currentTarget.clientWidth;
        var x = e.offsetX - e.currentTarget.clientWidth / 2;
        var y = -(e.offsetY - e.currentTarget.clientHeight / 2);
        if(y > s * x) {
          if(y > -s * x) {
            // north
            e.currentTarget.setAttribute("data-shift", "n");
          } else {
            // west
            e.currentTarget.setAttribute("data-shift", "w");
          }
        } else {
          if(y > -s * x) {
            // east
            e.currentTarget.setAttribute("data-shift", "e");
          } else {
            // south
            e.currentTarget.setAttribute("data-shift", "s");
          }
        }
      }
    };

    origin.contentWindow.submitMapCmd2 = function(e) {
      /*
      var x = e.offsetX - e.currentTarget.clientWidth / 2;
      var y = -(e.offsetY - e.currentTarget.clientHeight / 2);
      // nice to have: set center of map as LatLng in command
      */
      var interaction = e.currentTarget.getAttribute("data-interaction");
      var shiftType = e.currentTarget.getAttribute("data-shift");
      switch(interaction) {
        case "shift":
          switch(shiftType) {
            case "n":
              origin.contentWindow.reloadMap("up");
              break;
            case "s":
              origin.contentWindow.reloadMap("down");
              break;
            case "e":
              origin.contentWindow.reloadMap("right");
              break;
            case "w":
              origin.contentWindow.reloadMap("left");
              break;
          }
          break;
        case "zoom":
          origin.contentWindow.reloadMap("in");
          break;
        case "widen":
          origin.contentWindow.reloadMap("out");
          break;
      }
    };

    var standbyPreventionVideo = origin.contentDocument.querySelector("#preventstandbyVideo");    // iOS need video with audio track to have it work as standby preventer
    standbyPreventionVideo.addEventListener("play", function() {
      standbyPreventionVideo.classList.add("running");
    });
    standbyPreventionVideo.addEventListener("timeupdate", function() {
      standbyPreventionVideo.currentTime > 6 ? standbyPreventionVideo.currentTime = 4 : 0;
    });
    standbyPreventionVideo.addEventListener("pause", function() {
      standbyPreventionVideo.classList.remove("running");
    });
    var testTimeout;
    origin.contentWindow.preventStandby = function(innerorigin) {
      if(innerorigin.checked) {
        standbyPreventionVideo.play();
        storeState("preventingstandby", true);
        testTimeout = origin.contentWindow.setTimeout(function() {
          if(standbyPreventionVideo.paused) {     // this can occur on "restoring state" after content switch on iOS because it is denied by iOS as it is like an autoplay
            origin.contentDocument.querySelector("#preventstandby").click();
          }
        }, 1000);
      } else {
        origin.contentWindow.clearTimeout(testTimeout);
        standbyPreventionVideo.pause();
        storeState("preventingstandby", false);
      }
    };
    if(retrieveState("preventingstandby")) {
      origin.contentDocument.querySelector("#preventstandby").click();
    }

    origin.contentWindow.refreshPage = function() {
      origin.contentWindow.reloadMap("refreshonly");

      // copy from script.js refreshMapPage() follows adjusted for document and window, as well as interval function being contained
      var refreshvalue = origin.contentDocument.getElementById('refreshselect').value;
      if (refreshvalue != origin.contentWindow.currentInterval) {
        // Value has changed - stop udpates
        origin.contentWindow.clearInterval(origin.contentWindow.timeoutHandle);

        if (origin.contentWindow.currentInterval != -1) {
          // Not the first load - update session on server
          var xhttp = new XMLHttpRequest();
          xhttp.open("GET", "/refresh?session&maprefresh=" + refreshvalue, true);
          xhttp.send();
        }

        origin.contentWindow.currentInterval = refreshvalue;

        if (origin.contentWindow.currentInterval > 0) {
          // Set interval timer for periodical reload
          origin.contentWindow.timeoutHandle = origin.contentWindow.setInterval(function(){origin.contentWindow.reloadMap("refreshonly")}, origin.contentWindow.currentInterval * 1000);
        }
      }
    };
    var refresher = origin.contentDocument.querySelector("#refreshselect");
    refresher.addEventListener("change", function() {
      storeState("refreshinterval", this.selectedIndex);
    });
    var refreshInterval = retrieveState("refreshinterval");
    if(refreshInterval !== null) {
      refresher.selectedIndex = refreshInterval;
      refresher.dispatchEvent(new Event("change"));
    }

  }

  var toDo = {
    "#airportPage": updateAirportPage,
    "#progressPage": updateProgressPage,
    "#aircraftPage": updateGeneric,
    "#flightplanPage": updateGeneric,
    "#mapPage": updateMapPage
  };

  for(var i in toDo) {
    var find = origin.contentDocument.querySelector(i);
    if(find) {
      toDo[i](find);
      break;
    }
  }

  function storeState(key, state) {
    sessionStorage.setItem(key, typeof state === "boolean" ? state ? "1" : "" : "" + state);
    sessionStorage.setItem(key + "__type", typeof state === "boolean" ? "1" : typeof state === "number" ? "2" : "");
  }

  function retrieveState(key) {
    var type = sessionStorage.getItem(key + "__type");
    var value = sessionStorage.getItem(key);
    return type === "1" ? value === "1" : type === "2" ? parseFloat(value) : value;
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
function setToolbarPosition(origin) {
  document.querySelector("[data-toolbarsplacement]").setAttribute("data-toolbarsplacement", origin.value);
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
    themeCSS.href = "/assets/styles-alternative-look-userdefined-theme-" + origin.value + ".css";
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