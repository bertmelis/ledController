// globals
var ws;
var colourPicker;
var showSystem = false;
var currentState = {
  status: false,
  effect: "singleColour",
  colour: [255, 255, 255]
};
var sendUpdate = true;

function send() {
  if (sendUpdate) {
    ws.send(JSON.stringify(currentState));
  }
}

function validJSON(jsonString) {
  try {
    var o = JSON.parse(jsonString);
    if (o && typeof o === "object") {
      return true;
    }
  }
  catch (e) {
    return false;
  }
}

function updateUI() {
  document.getElementById("onoff").checked = currentState.status;
  colourPicker.color.red = currentState.colour[0];
  colourPicker.color.green = currentState.colour[1];
  colourPicker.color.blue = currentState.colour[2];
  el = document.getElementById(currentState.effect);
  if (el) {
    el.click();
    closeAllSelect();
  } else {
    console.log("could not find effect select option");
  }
}

function onMessage(message){
  message = message.data;
  try {
    if (validJSON(message)) {
      message = JSON.parse(message);
      if (message.hasOwnProperty("status")) {
        currentState.status = message.status;
      }
      if (message.hasOwnProperty("effect")) {
        currentState.effect = message.effect;
      }
      if (message.hasOwnProperty("colour")) {
        currentState.colour[0] = message.colour[0];
        currentState.colour[1] = message.colour[1];
        currentState.colour[2] = message.colour[2];
      }
      sendUpdate = false;  // avoid update-send-receive loop
      updateUI();
      sendUpdate = true;
    } else {
      // message is raw printed message
      var el = document.getElementById("system").getElementsByTagName("pre");
      el[0].textContent += message;
      el[0].scrollTop = el[0].scrollHeight;
    }
  }
  catch(e) {
    console.log(e);
  }
}

function initWebsocket() {
  if ("WebSocket" in window) {
    ws = new WebSocket(((window.location.protocol === "https:") ? "wss://" : "ws://") + window.location.host + "/ws");
    ws.onopen = function() {
      document.getElementById("overlay").classList.add("hidden");
      document.getElementById("control").classList.remove("hidden");
    };
    ws.onmessage = onMessage;
    ws.onclose = function() {
      document.getElementById("overlay").classList.remove("hidden");
      document.getElementById("control").classList.add("hidden");
      setTimeout(function() { initWebsocket(); }, 1000);
    };
    window.onbeforeunload = function(event) {
      ws.close();
    };
  } else {
    document.getElementById("body").innerHTML = "<h1>Your browser does not support WebSocket.</h1>";
  }
}

function sendXHRequest(formData, uri) {
  var xhr = new XMLHttpRequest();
  xhr.upload.addEventListener("loadstart", function(evt) {
    document.getElementById("updateMessage").innerHTML = "<br />Update started.";
  }, false);
  xhr.upload.addEventListener("progress", function(evt) {
    // progress bar is not implemented in html
    // var percent = evt.loaded/evt.total*100;
    // document.getElementById("updateProgress").setAttribute("value", percent);
  }, false);
  xhr.upload.addEventListener("load", function(evt) {
    document.getElementById("updateMessage").innerHTML += "<br />Firmware uploaded. Waiting for response.";
  }, false);
  /* The following doesn't seem to work on Edge and Chrome?
  xhr.addEventListener("readystatechange", function(evt) {
    // ...
  }
  }, false);
  */
  xhr.onreadystatechange = function() {
    if (xhr.readyState == 4 && xhr.status == 200) {
      document.getElementById("updateMessage").innerHTML += "<br />Server message: " + xhr.responseText;
      document.getElementById("updateMessage").innerHTML += "<br />Reloading in 5 seconds...";
      setTimeout(function() {
        //location.reload(true);  deprecated
        location.reload();
      }, 5000);
      return xhr.responseText;
    }
  };
  xhr.open("POST", uri, true);
  xhr.send(formData);
}

function initFirmwareUpload() {
  var form = document.getElementById("updateForm");
  form.onsubmit = function() {
    var formData = new FormData(form);
    var action = form.getAttribute("action");
    var fileInput = document.getElementById("firmware");
    var file = fileInput.files[0];
    formData.append("firmware", file);
    sendXHRequest(formData, action);
    return false;  // Avoid normal form submission
  };
}

function initColourpicker() {
  colourPicker = new iro.ColorPicker("#colourPicker", {
    width: 320,
    display: "inline-block",
    color: "#fff"
  });
  //colourPicker.on("input:change", function(colour) {
  colourPicker.on("input:end", function(c) {
    currentState.colour[0] = c.rgb.r;
    currentState.colour[1] = c.rgb.g;
    currentState.colour[2] = c.rgb.b;
    send();
  });
}

function initSelect() {
  var x, i, j, l, ll, selElmnt, a, b, c;
  // Look for any elements with the class "effect-select"
  x = document.getElementsByClassName("effect-select");
  l = x.length;
  for (i = 0; i < l; i++) {
    selElmnt = x[i].getElementsByTagName("select")[0];
    ll = selElmnt.length;
    // For each element, create a new DIV that will act as the selected item
    a = document.createElement("DIV");
    a.setAttribute("class", "select-selected");
    a.innerHTML = selElmnt.options[selElmnt.selectedIndex].innerHTML;
    a.setAttribute("id", selElmnt.options[selElmnt.selectedIndex].value);
    x[i].appendChild(a);
    // For each element, create a new DIV that will contain the option list
    b = document.createElement("DIV");
    b.setAttribute("class", "select-items select-hide");
    for (j = 1; j < ll; j++) {
      // For each option in the original select element, create a new DIV that will act as an option item
      c = document.createElement("DIV");
      c.setAttribute("id", selElmnt.options[j].value);
      c.innerHTML = selElmnt.options[j].innerHTML;
      c.addEventListener("click", function(e) {
          // When an item is clicked, update the original select box, and the selected item:
          var y, i, k, s, h, sl, yl;
          s = this.parentNode.parentNode.getElementsByTagName("select")[0];
          sl = s.length;
          h = this.parentNode.previousSibling;
          for (i = 0; i < sl; i++) {
            if (s.options[i].innerHTML == this.innerHTML) {
              s.selectedIndex = i;
              h.innerHTML = this.innerHTML;
              y = this.parentNode.getElementsByClassName("same-as-selected");
              yl = y.length;
              for (k = 0; k < yl; k++) {
                y[k].removeAttribute("class");
              }
              this.setAttribute("class", "same-as-selected");
              break;
            }
          }
          // here the selected effect is passed to other code
          currentState.effect = this.id;
          send();
          h.click();
      });
      b.appendChild(c);
    }
    x[i].appendChild(b);
    a.addEventListener("click", function(e) {
      // When the select box is clicked, close any other select boxes, and open/close the current select box
      e.stopPropagation();
      closeAllSelect(this);
      this.nextSibling.classList.toggle("select-hide");
      this.classList.toggle("select-arrow-active");
    });
  }
}

function closeAllSelect(elmnt) {
  // A function that will close all select boxes in the document, except the current select box:
  var x, y, i, xl, yl, arrNo = [];
  x = document.getElementsByClassName("select-items");
  y = document.getElementsByClassName("select-selected");
  xl = x.length;
  yl = y.length;
  for (i = 0; i < yl; i++) {
    if (elmnt == y[i]) {
      arrNo.push(i);
    } else {
      y[i].classList.remove("select-arrow-active");
    }
  }
  for (i = 0; i < xl; i++) {
    if (arrNo.indexOf(i)) {
      x[i].classList.add("select-hide");
    }
  }
  document.addEventListener("click", closeAllSelect);
}

function initButtons() {
  document.getElementById("systemButton").addEventListener("click", function (event) {
    if (showSystem) {
      document.getElementById("control").classList.remove("hidden");
      document.getElementById("system").classList.add("hidden");
      document.getElementById("systemButton").style.opacity = ""; /* set in CSS */
      showSystem = false;
    } else {
      document.getElementById("control").classList.add("hidden");
      document.getElementById("system").classList.remove("hidden");
      document.getElementById("systemButton").style.opacity = "1";
      showSystem = true;
    }
  }, true);
  document.getElementById("onoffslider").addEventListener("click", function (event) {
    currentState.status = !currentState.status;
    send();
  }, true);
}

document.addEventListener("DOMContentLoaded", function() {
  initWebsocket();
  initFirmwareUpload();
  initColourpicker();
  initButtons();
  initSelect();
}, false);
