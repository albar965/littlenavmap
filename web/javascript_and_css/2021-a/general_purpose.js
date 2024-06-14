function enableScrollIndicators(forElement) {
  var scrollableContainer = forElement.firstElementChild;
  function scrollIndicators() {
    if(scrollableContainer.scrollWidth > scrollableContainer.clientWidth) {
      if(scrollableContainer.scrollLeft > 0) {
        forElement.classList.add("indicator-scrollable-toleft");
        if(scrollableContainer.clientWidth + scrollableContainer.scrollLeft >= scrollableContainer.scrollWidth - 1) {
          forElement.classList.remove("indicator-scrollable-toright");
        } else {
          forElement.classList.add("indicator-scrollable-toright");
        }
      } else {
        forElement.classList.remove("indicator-scrollable-toleft");
        forElement.classList.add("indicator-scrollable-toright");
      }
    } else {
      forElement.classList.remove("indicator-scrollable-toleft");
      forElement.classList.remove("indicator-scrollable-toright");
    }
  }
  var examinedElement = forElement;
  while(examinedElement.tagName.toLowerCase() !== "html") {
    examinedElement = examinedElement.parentElement;
  }
  examinedElement.parentNode.defaultView.addEventListener("resize", scrollIndicators);
  scrollableContainer.addEventListener("scroll", scrollIndicators);
  scrollIndicators();
}