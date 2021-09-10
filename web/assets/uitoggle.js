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