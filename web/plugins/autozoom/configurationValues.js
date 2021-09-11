var zoomKnots = [0, 23, 55, 80, 110, 155, 200, 300, 400, 500, 750, 1500];
/*
  reasoning:
  rest, taxi (0 - 22), takeoff (ga Cessna 172) (55), intermediate, max (ga Icon A5) (110),
  intermediate, max (ga DA62), intermediate, cruise (airliner), max (airliner),
  for Mach >= 1 jets, for Mach >= 1 jets

  Values changed here need to be reflected in zoomPowerAddForAltsPerKnot keys further below !
*/

var zoomPowerForKnots = [-4, -3, -2, -1, 0, 1, 2, 3, 4, 5, 6, 7];
/*
  index i corresponds to range of values in zoomKnots
  from value at same index to excluding value at next index
  or unlimited, eg. -4 corresponds to 0 to 22.999 knots.

  It is the power to 2 and sets the distance of the viewer of the map above the ground in km,
  ie. -4 corresponds to 2^-4 = 1/16 = 0.0625 km = 62.5 m above ground.
*/


var zoomAltsAbvGrd = [0, 500, 4000, 9000, 15000, 32000];

var zoomPowerAddForAltsPerKnot = {
  "0": [0, 0, 0, 0, 0, 0],
  "23": [0, 0, 1, 1, 1, 1],
  "55": [0, 0, 1, 2, 2, 2],
  "80": [0, 1, 1, 2, 3, 3],
  "110": [0, 1, 1, 2, 3, 4],
  "155": [0, 0, 1, 2, 3, 3],
  "200": [0, -1, 1, 1, 2, 3],
  "300": [0, -2, 1, 1, 2, 3],
  "400": [0, -3, 1, 1, 2, 3],
  "500": [0, -4, 1, 1, 1, 2],
  "750": [0, -4, 1, 1, 1, 1],
  "1500": [0, -4, 0, 0, 0, 0]
};

/*
  adjustment of zoomPowerForKnots by adding the value according to the knot and altitude range as given above
*/


var zoomWaitForKnots = [5, 3, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1];
/*
  checking for a change in knot speed in seconds of value
  corresponding to the current knot speed (like zoomPowerForKnots does)
*/