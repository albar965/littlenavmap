/*
 * brings iframe to foreground
 */
function showIframe(srcLink) {
  var dstIframe = document.querySelector('iframe[name="' + srcLink.target + '"]');
  if(dstIframe) {
    if(dstIframe.src !== srcLink.href) {
      dstIframe.src = srcLink.href;
    }
    document.querySelector('iframe.shown')?.classList.remove("shown");
    dstIframe.classList.add("shown");
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
 * toggles display of menubars options
 */
function toggleMenubarsOptions(e) {
  document.querySelector("#toggleOptionsToggle").classList.toggle("shown");
  (e || window.event).stopPropagation();
}

/*
 * closes display of menubars options
 */
function closeMenubarsOptions() {
  document.querySelector("#toggleOptionsToggle").classList.remove("shown");
}

/*
 * sets value of [data-menubarsplacement] according to origin's value
 */
function setMenubarPosition(e) {
  document.querySelector("[data-menubarsplacement]").setAttribute("data-menubarsplacement", e.target.value);
  storeState("menubarsplacement", e.target.value);
}
var gotten = retrieveState("menubarsplacement", false);
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
  storeState("themeCSS", origin.value);
}
gotten = retrieveState("themeCSS", false);
if(gotten) {
  var destination = document.querySelector("select[name=theme]");
  destination.value = gotten;
  destination.dispatchEvent(new Event("change"));
}