window.storeState = function (key, state, temporary) {
  var value, type;
  switch(typeof state) {
    case "boolean":
      value = state ? "1" : "";
      type = "1";
      break;
    case "number":
      value = "" + state;
      type = "2";
      break;
    case "string":
      value = state;
      type = "3";
      break;
    case "object":
      value = JSON.stringify(state);
      type = "4";
      break;
    default:
      value = "" + state;
      type = typeof state;
  }
  (temporary ? sessionStorage : localStorage).setItem("LNM_" + key, value);
  (temporary ? sessionStorage : localStorage).setItem("LNM_" + key + "__type", type);
};

window.retrieveState = function (key, defaultValue, temporary) {
  var value = (temporary ? sessionStorage : localStorage).getItem("LNM_" + key);
  var type = (temporary ? sessionStorage : localStorage).getItem("LNM_" + key + "__type");
  if(value !== null && type !== null) {
    switch(type) {
      case "1":
        return value === "1";
      case "2":
        return parseFloat(value);
      case "3":
        return value;
      case "4":
        return JSON.parse(value);
      default:
        return value;
    }
  }
  return defaultValue;
};

window.deleteState = function (key, temporary) {
  (temporary ? sessionStorage : localStorage).removeItem("LNM_" + key);
  (temporary ? sessionStorage : localStorage).removeItem("LNM_" + key + "__type");
}