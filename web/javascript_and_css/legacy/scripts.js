/*****************************************************************************
* Javascript for Little Navmap internal web page
*****************************************************************************/

/*
 * Focus the input element in the airport search page on loading
 */
function focusAirportText() {
  document.getElementById("airporttext").focus();
}

/*
 * Reload a part of the page "pageToReload" and replace the content of id="doc".
 */
function reloadPage() {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      document.getElementById("doc").innerHTML = this.responseText;
    }
  };
  xhttp.open("GET", pageToReload, true);
  xhttp.send();
}

/*
 * Refresh a page periodically and udpate the interval in the server session by sending a request
 * with "refreshkey=refreshvalue".
 */
function refreshPage() {
  reloadPage();

  var refreshvalue = document.getElementById('refreshselect').value;
  if (refreshvalue != currentInterval) {
    // Value has changed - stop udpates
    clearInterval(timeoutHandle);

    if (currentInterval != -1) {
      // Not the first load - update session on server
      var xhttp = new XMLHttpRequest();
      xhttp.open("GET", "/refresh?session&" + refreshkey + "=" + refreshvalue, true);
      xhttp.send();
    }

    currentInterval = refreshvalue;

    if (currentInterval > 0) {
      // Set interval timer if no manual refresh is desired
      timeoutHandle = setInterval(reloadPage, currentInterval * 1000);
    }
  }
}

/* ====================================================================================
 * Map functions for index.html
 */

/*
 * Calculate map width and height.
 */
function mapWidth() {
  return Math.max(document.getElementById('mapcontainer').offsetWidth, 128);
}

function mapHeight() {
  return Math.max(window.innerHeight - document.getElementById('header').offsetHeight - 50, 128);
}

/*
 * Reload the map by updating the image src. Needs to add a random parameter to bypass cache.
 */
function reloadMap() {
  document.getElementById("map").src =
    "/mapimage?format=jpg&quality=80&width=" + mapWidth() + "&height=" + mapHeight() +
    "&session&reload=" + Math.random();
}

/*
 * Submit a command for zooming or scrolling and reload the map by updating the image src.
 * Needs to add a random parameter to bypass cache.
 */
function submitMapCmd(cmd) {
  document.getElementById("map").src =
    "/mapimage?format=jpg&quality=80&width=" + mapWidth() + "&height=" + mapHeight() +
    "&session&mapcmd=" + cmd + "&cmd=" + Math.random();
}

/*
 * Submit a command for showing an airport and reload the map by updating the image src.
 * Needs to add a random parameter to bypass cache.
 */
function submitMapAirportCmd() {
  var icao = document.getElementById('airporttext').value;
  document.getElementById("map").src =
    "/mapimage?format=jpg&quality=80&width=" + mapWidth() + "&height=" + mapHeight() +
    "&session&mapcmd=airport&airport=" + icao + "&distance=10&cmd=" + Math.random();
}

/*
 * Submit a command for showing the user aircraft and reload the map by updating the image src.
 * Needs to add a random parameter to bypass cache.
 */
function submitMapUserCmd() {
  document.getElementById("map").src =
    "/mapimage?format=jpg&quality=80&width=" + mapWidth() + "&height=" + mapHeight() +
    "&session&mapcmd=user&distance=5&cmd=" + Math.random();
}

/*
 * Submit a command for showing an flight plan and reload the map by updating the image src.
 * Needs to add a random parameter to bypass cache.
 */
function submitMapRouteCmd() {
  document.getElementById("map").src =
    "/mapimage?format=jpg&quality=80&width=" + mapWidth() + "&height=" + mapHeight() +
    "&session&mapcmd=route&cmd=" + Math.random();
}

/*
 * Toggle class on #map to fit it to parent.
 */
function toggleFitMap() {
  document.querySelector("#map").classList.toggle("fit-media-to-parent");
}

/*
 * Refresh the map image periodically and udpate the interval in the server session by sending a request
 * with "maprefresh=refreshvalue".
 */
function refreshMapPage() {
  reloadMap();

  var refreshvalue = document.getElementById('refreshselect').value;
  if (refreshvalue != currentInterval) {
    // Value has changed - stop udpates
    clearInterval(timeoutHandle);

    if (currentInterval != -1) {
      // Not the first load - update session on server
      var xhttp = new XMLHttpRequest();
      xhttp.open("GET", "/refresh?session&maprefresh=" + refreshvalue, true);
      xhttp.send();
    }

    currentInterval = refreshvalue;

    if (currentInterval > 0) {
      // Set interval timer for periodical reload
      timeoutHandle = setInterval(reloadMap, currentInterval * 1000);
    }
  }
}
