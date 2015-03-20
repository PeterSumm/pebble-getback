// Disable console for production or comment this line out to enable it for debugging.
console.log = function() {};

var initialized = false;
var divider = 1000000;
var interval = 10;
var runtime = 0;
var lat1 = 0;
var lon1 = 0;
var lat2 = 0;
var lon2 = 0;
var speed;
var bearing = -1;
var heading = -1;
var distance = "";
var accuracy = "";
var imperial = false;
var yardLength = 0.9144;
var yardsInMile = 1760;
var R = 6371000; // m
var locationWatcher;
var locationOptions = {timeout: 15000, maximumAge: 1000, enableHighAccuracy: true };
// var locationOptions = {timeout: 9000, maximumAge: 0, enableHighAccuracy: true };
var setPebbleToken = "YR5R";

Pebble.addEventListener("ready", function(e) {
  if (typeof(Number.prototype.toRad) === "undefined") {
    Number.prototype.toRad = function() {
      return this * Math.PI / 180;
    }
  }
  if (typeof(Number.prototype.toDeg) === "undefined") {
     Number.prototype.toDeg = function() {
      return this * 180 / Math.PI;
     }
  }
  lat2 = parseFloat(localStorage.getItem("lat2")) || null;
  lon2 = parseFloat(localStorage.getItem("lon2")) || null;
  interval = parseInt(localStorage.getItem("interval")) || 10;
  runtime = parseInt(localStorage.getItem("runtime")) || 0;
  imperial = (parseInt(localStorage.getItem("imperial")) == 1);
  if ((lat2 === null) || (lon2 === null)) {
    if (runtime === 0) 
      navigator.geolocation.getCurrentPosition(storeLocation, locationError, locationOptions);
    else {
      locationWatcher = navigator.geolocation.watchPosition( storeLocation, locationError, locationOptions );
      setTimeout( function() { navigator.geolocation.clearWatch( locationWatcher ); }, 1000 * runtime );
    }
  }
  else {
    console.log("Target location known:" + lon2 + ',' + lat2);
    if (runtime === 0) 
      navigator.geolocation.getCurrentPosition(locationSuccess, locationError, locationOptions);
    else {
      locationWatcher = navigator.geolocation.watchPosition( locationSuccess, locationError, locationOptions );
      setTimeout( function() { navigator.geolocation.clearWatch( locationWatcher ); }, 1000 * runtime );
    }
  }
  // console.log(e.type);
  initialized = true;
  console.log("JavaScript app ready and running! " + e.ready);
});

Pebble.addEventListener("appmessage",
  function(e) {
    if (e && e.payload && e.payload.cmd) {
      console.log("Got command: " + e.payload.cmd);
      switch (e.payload.cmd) {
        case 'set':
          console.log("Attempting to store current position.");
          if (runtime === 0) 
            navigator.geolocation.getCurrentPosition(storeLocation, locationError, locationOptions);
          else {
            locationWatcher = navigator.geolocation.watchPosition( storeLocation, locationError, locationOptions );
            setTimeout( function() { navigator.geolocation.clearWatch( locationWatcher ); }, 5000 );
          }
          break;
        case 'get':
          if (locationWatcher) { navigator.geolocation.clearWatch(locationWatcher); }
          if (runtime === 0) 
            navigator.geolocation.getCurrentPosition(locationSuccess, locationError, locationOptions);
          else {
            locationWatcher = navigator.geolocation.watchPosition( locationSuccess, locationError, locationOptions );
            setTimeout( function() { navigator.geolocation.clearWatch( locationWatcher ); }, 1000 * runtime );
          }
          break;
        case 'clear':
          lat2 = null;
          lon2 = null;
          break;
        case 'quit':
          if (locationWatcher) navigator.geolocation.clearWatch(locationWatcher);
          break;
        default:
          console.log("Unknown command!");
      }
    }
  }
);

Pebble.addEventListener("showConfiguration",
  function() {
    var uri = "http://x.setpebble.com/" + setPebbleToken + "/" + Pebble.getAccountToken();
    console.log("Configuration url: " + uri);
    Pebble.openURL(uri);
  }
);

Pebble.addEventListener("webviewclosed",
  function(e) {
    var options = JSON.parse(decodeURIComponent(e.response));
    console.log("Webview window returned: " + JSON.stringify(options));
    
/*    var target = options["1"];
    console.log("Target set to: " + target);
    var location1 = options["2"];
    console.log("Location 1 set to: " + location1);
    var location2 = options["3"];
    console.log("Location 2 set to: " + location2);
    var location3 = options["4"];
    console.log("Location 3 set to: " + location3);
    var location;
    if (target>0) {
      location = (target == 1) ? location1 : (target == 2 ? location2 : location3);
      var spaceIndex = location.indexOf(" ");
      if (spaceIndex > 1) {
        lat2 = 1 * location.slice(0,spaceIndex);
        lon2 = 1 * location.slice(spaceIndex+1,location.length);
        console.log("Location " + location + " set to: " + lat2 + " " + lon2);
        localStorage.setItem("lat2", lat2);
        localStorage.setItem("lon2", lon2);
      }
    }  */
    
    imperial = (options["1"] === 1);
    console.log("Units set to: " + (imperial ? "imperial" : "metric"));
    localStorage.setItem("imperial", (imperial ? 1 : 0));
    interval = parseInt(options["2"] || 10);
    console.log("Interval set to: " + interval);
    localStorage.setItem("interval", interval);
    runtime = parseInt(options["3"] || 5);
    console.log("RunTime set to: " + runtime);
    localStorage.setItem("runtime", runtime);
    calculate();
  }
);

function sendMessage(dict) {
  Pebble.sendAppMessage(dict, appMessageAck, appMessageNack);
  console.log("Sent message to Pebble! " + JSON.stringify(dict));
}

function appMessageAck(e) {
  console.log("Message accepted by Pebble!");
}

function appMessageNack(e) {
  console.log("Message rejected by Pebble! " + e.error);
}

function locationSuccess(position) {
  console.log("Got location " + position.coords.latitude + ',' + position.coords.longitude + ', altitude = ' + position.coords.altitude + ', accuracy = ' + position.coords.accuracy + ', speed = ' + position.coords.speed + ', heading at ' + position.coords.heading + ' at ' + position.timestamp);
  lat1 = position.coords.latitude;
  lon1 = position.coords.longitude;
  speed = position.coords.speed;
  if (speed > 0) {
    if (imperial) {
      speed *= 2.237;
    } else {
      speed *= 3.6;
    }
    speed = speed.toFixed((speed < 100) ? 1 : 0);
  } else 
    speed = "";
  accuracy = position.coords.accuracy;
  if (imperial) accuracy /= yardLength;
  accuracy = accuracy.toFixed(0);
  if ((position.coords.heading === null) || isNaN(position.coords.heading)) 
    heading = -1; 
  else {
    heading = Math.round(position.coords.heading);
    if (heading < 0) heading += 360;
  }
  calculate();
}

function storeLocation(position) {
  lat2 = position.coords.latitude;
  lon2 = position.coords.longitude;
  localStorage.setItem("lat2", lat2);
  localStorage.setItem("lon2", lon2);
  console.log("Stored location " + position.coords.latitude + ',' + position.coords.longitude);
  calculate();
}

function calculate() {
  var msg, unit;
  if (lat2 || lon2) {
    if ((Math.round(lat2*divider) == Math.round(lat1*divider)) && 
        (Math.round(lon2*divider) == Math.round(lon1*divider))) {
      console.log("Not moved yet, still at  " + lat1 + ',' + lon1);
      distance = "0";
      bearing = 0;
      msg = {"distance": distance,
             "bearing": bearing };
      sendMessage(msg);
      return;
    }
    console.log("Moved to  " + lat1 + ',' + lon1);
    if (!lat2) {
      lat2 = 0;
    }
    if (!lon2) {
      lon2 = 0;
    }
    console.log("Found stored point " + lat2 + ',' + lon2);
    var dLat = (lat2-lat1).toRad();
    console.log("Latitude difference (radians): " + dLat);
    var dLon = (lon2-lon1).toRad();
    console.log("Longitude difference (radians): " + dLon);
    var l1 = lat1.toRad();
    var l2 = lat2.toRad();
    console.log("current and stored latitudes in radians: " + l1 + ',' + l2);

    var a = Math.sin(dLat/2) * Math.sin(dLat/2) +
            Math.sin(dLon/2) * Math.sin(dLon/2) * Math.cos(l1) * Math.cos(l2); 
    var c = 2 * Math.atan2(Math.sqrt(a), Math.sqrt(1-a)); 
    distance = Math.round(R * c);
    if (imperial) {
      distance = distance / yardLength;
      if (distance < yardsInMile) {
        distance = distance.toFixed(0);
        unit = "yd";
      } else {
        distance = distance / yardsInMile;
        distance = distance.toFixed(distance < 10 ? 2 : (distance < 100 ? 1 : 0));
        unit = "mi";
      } 
    } else {
      if (distance < 1000) {
        distance = distance.toFixed(0);
        unit = "m";
      } else {
        distance = distance / 1000;
        distance = distance.toFixed(distance < 10 ? 2 : (distance < 100 ? 1 : 0));
        unit = "km";
      } 
    }
      
    console.log("Calculated distance " + distance + " " + unit);
    
    var y = Math.sin(dLon) * Math.cos(l2);
    var x = Math.cos(l1)*Math.sin(l2) -
            Math.sin(l1)*Math.cos(l2)*Math.cos(dLon);
    bearing = Math.round(Math.atan2(y, x).toDeg());
    if (bearing < 0) bearing += 360;
    
    console.log("Calculated bearing " + bearing);
    console.log("Calculated speed " + speed);
    console.log("Calculated accuracy " + accuracy);
    console.log("Calculated heading " + heading);

    msg = {"distance": distance,
           "bearing": bearing,
           "unit": unit,
           "interval": interval,
           "speed": speed,
           "accuracy": accuracy,
           "heading": heading };
    sendMessage(msg);
  }
  else {
    console.log("Will not calculate: no lat2 and lon2: " + lat2 + ',' + lon2);
  }
}

function locationError(error) {
  console.warn('location error (' + error.code + '): ' + error.message);
}
