/*
 * Toggles to and from the legacy UI
 */
function toggleLegacyUI() {
  var activeUI = retrieveState("activeUI", null);
  if(activeUI !== null) {
    switch(activeUI) {
      case "2021-a":
        location.href = "/html/legacy/legacy.html";
        break;
      case "legacy":
        location.href = "/html/2021-a/index.html";
        break;
      default:
        location.href = "/html/2021-a/index.html";
    }
  } else {
    location.href = "/html/legacy/legacy.html";
  }
}

// switch to legacy if the new ui doesn't show as intended
var newUIToolbarsContainer = document.getElementById("toolbarsContainer");
if(newUIToolbarsContainer) {      // on new ui
  if(newUIToolbarsContainer.clientHeight < 1) {               // new ui not rendering correctly
    if(retrieveState("activeUI", 0, true) === "legacy") {     // if the user just was on the legacy ui, inform him (when back there) why he is back there
      storeState("noNewUIInformUser", "1", true);
    }
    storeState("noNewUI", "1", true);
    location.href = "/html/legacy/legacy.html";
  }
} else {                          // on legacy ui
  if(retrieveState("noNewUI", 0, true) === "1") {             // new ui not supported
    if(retrieveState("noNewUIInformUser", 0, true) === "1") {
      alert("\"new ui\" not supported in this browser.");
      deleteState("noNewUIInformUser", true);         				// only show once
    }
    setTimeout(function() {
      document.getElementById("extrasform").style.display = "none";       // don't show the "new ui" toggle
    }, 0);
  }
}