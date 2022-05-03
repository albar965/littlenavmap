/*
 * Toggles to and from the legacy UI
 */
function toggleLegacyUI() {
  var activeUI = sessionStorage.getItem("activeUI");
  if(activeUI !== null) {
    switch(activeUI) {
      case "2021-a":
        location.href = "legacy.html";
        break;
      case "legacy":
        location.href = "index.html";
        break;
      default:
        location.href = "index.html";

    }
  } else {
    location.href = "legacy.html";
  }
}

// switch to legacy if the new ui doesn't show as intended
var newUIToolbarsContainer = document.getElementById("toolbarsContainer");
if(newUIToolbarsContainer) {      // on new ui
  if(newUIToolbarsContainer.clientHeight < 1) {               // new ui not rendering correctly
    if(sessionStorage.getItem("activeUI") === "legacy") {     // if the user just was on the legacy ui, inform him (when back there) why he is back there
      sessionStorage.setItem("noNewUIInformUser", "1");
    }
    sessionStorage.setItem("noNewUI", "1");
    location.href = "/legacy.html";
  }
} else {                          // on legacy ui
  if(sessionStorage.getItem("noNewUI") === "1") {             // new ui not supported
    if(sessionStorage.getItem("noNewUIInformUser") === "1") {
      alert("\"new ui\" not supported in this browser.");
      sessionStorage.removeItem("noNewUIInformUser");         // only show once
    }
    setTimeout(function() {
      document.getElementById("extrasform").style.display = "none";       // don't show the "new ui" toggle
    }, 0);
  }
}