/*
 * set class of closest parent button of clicked element within children to active
 */
function setActive(e) {
  e = e || window.event;
  var i = e.target;
  while(i.tagName.toLowerCase() !== "button" && i.tagName.toLowerCase() !== "body") {
    i = i.parentElement;
  }
  if(i.tagName.toLowerCase() === "button" && i.dataset.activateable !== "false") {
    e.currentTarget.querySelector(".active").classList.remove("active");
    if(i.id === "buttonMap") {
      Array.prototype.forEach.call(document.querySelectorAll(".overlaying-content"), function(oc) {
        oc.style.display = "block";
      });
    } else {
      Array.prototype.forEach.call(document.querySelectorAll(".overlaying-content"), function(oc) {
        oc.style.display = "none";
      });
    }
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
    themeCSS.href = "/themes/2021-a/look-userdefined-theme-" + origin.value + ".css";
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
  destination.dispatchEvent(new Event("change"));
}