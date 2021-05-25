/*
 * adds the stylesheet containing styles for the original HTML in alternative look to the document loaded in the iframe
 */
function injectAlternativeStyling() {

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
 * compilation of functions to run when the body receives a click event (possibly having bubbled)
 */
function bodyFunctions() {
  closeToolbarsOptions();
}